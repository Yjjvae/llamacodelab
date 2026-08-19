#include "adapters/llama/llama_embedder.hpp"

#include "llamacodelab/domain/similarity.hpp"

#include <cstdint>
#include <filesystem>
#include <limits>
#include <llama.h>
#include <mutex>
#include <stdexcept>
#include <utility>
#include <vector>

namespace llcl::llama_adapter {
namespace {

struct ModelDeleter {
  void operator()(llama_model* value) const noexcept {
    if (value != nullptr) {
      llama_model_free(value);
    }
  }
};
struct ContextDeleter {
  void operator()(llama_context* value) const noexcept {
    if (value != nullptr) {
      llama_free(value);
    }
  }
};
struct BatchDeleter {
  void operator()(llama_batch* value) const noexcept {
    if (value != nullptr) {
      llama_batch_free(*value);
      delete value;
    }
  }
};
using ModelPtr = std::unique_ptr<llama_model, ModelDeleter>;
using ContextPtr = std::unique_ptr<llama_context, ContextDeleter>;
using BatchPtr = std::unique_ptr<llama_batch, BatchDeleter>;

[[nodiscard]] std::vector<llama_token> tokenize(const llama_vocab* vocabulary,
                                                const std::string_view text) {
  if (text.size() > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())) {
    throw std::invalid_argument("embedding input is too large");
  }
  const auto needed = llama_tokenize(
      vocabulary, text.data(), static_cast<std::int32_t>(text.size()), nullptr, 0, true, true);
  if (needed >= 0) {
    throw std::runtime_error("llama_tokenize did not report required embedding token capacity");
  }
  std::vector<llama_token> tokens(static_cast<std::size_t>(-needed));
  const auto count =
      llama_tokenize(vocabulary, text.data(), static_cast<std::int32_t>(text.size()), tokens.data(),
                     static_cast<std::int32_t>(tokens.size()), true, true);
  if (count <= 0) {
    throw std::runtime_error("failed to tokenize embedding input");
  }
  tokens.resize(static_cast<std::size_t>(count));
  return tokens;
}

} // namespace

class LlamaEmbedder::Impl {
public:
  Impl(LlamaRuntime& runtime_value, ModelConfig config_value, EmbedderOptions options_value)
      : runtime(runtime_value), config(std::move(config_value)), options(std::move(options_value)) {
    if (!std::filesystem::is_regular_file(config.path)) {
      throw std::runtime_error("embedding GGUF model does not exist: " + config.path.string());
    }
    auto model_parameters = llama_model_default_params();
    model_parameters.n_gpu_layers = config.gpu_layers;
    model.reset(llama_model_load_from_file(config.path.string().c_str(), model_parameters));
    if (model == nullptr) {
      throw std::runtime_error("failed to load embedding GGUF: " + config.path.string());
    }
    vocabulary = llama_model_get_vocab(model.get());
    dimension = llama_model_n_embd_out(model.get());
    if (vocabulary == nullptr || dimension <= 0) {
      throw std::runtime_error("embedding model has no vocabulary or output dimension");
    }
  }

  [[nodiscard]] std::string with_prefix(const std::string_view text,
                                        const EmbeddingKind kind) const {
    if (text.empty()) {
      throw std::invalid_argument("embedding input must not be empty");
    }
    const auto& prefix =
        kind == EmbeddingKind::query ? options.query_prefix : options.document_prefix;
    return prefix + std::string(text);
  }

  [[nodiscard]] std::vector<Embedding> embed_many(const std::span<const std::string_view> texts,
                                                  const EmbeddingKind kind) {
    std::vector<std::vector<llama_token>> token_lists;
    token_lists.reserve(texts.size());
    for (const auto text : texts) {
      auto tokens = tokenize(vocabulary, with_prefix(text, kind));
      if (tokens.size() > config.context_size || tokens.size() > config.batch_size) {
        throw std::invalid_argument("embedding input exceeds configured context or batch size");
      }
      token_lists.push_back(std::move(tokens));
    }

    std::vector<Embedding> result;
    result.reserve(texts.size());
    std::size_t first = 0;
    while (first < token_lists.size()) {
      std::size_t last = first;
      std::size_t token_count = 0;
      while (last < token_lists.size() &&
             token_count + token_lists[last].size() <= config.batch_size) {
        token_count += token_lists[last].size();
        ++last;
      }

      if (last == first) {
        throw std::invalid_argument("embedding batch cannot fit one input");
      }
      auto context_parameters = llama_context_default_params();
      context_parameters.n_ctx = static_cast<std::uint32_t>(config.context_size);
      context_parameters.n_batch = static_cast<std::uint32_t>(config.batch_size);
      context_parameters.n_ubatch = context_parameters.n_batch;
      context_parameters.n_seq_max = static_cast<std::uint32_t>(last - first);
      context_parameters.embeddings = true;
      context_parameters.flash_attn_type =
          config.flash_attention ? LLAMA_FLASH_ATTN_TYPE_ENABLED : LLAMA_FLASH_ATTN_TYPE_DISABLED;
      ContextPtr context(llama_init_from_model(model.get(), context_parameters));
      if (context == nullptr) {
        throw std::runtime_error("failed to create embedding context");
      }
      BatchPtr batch(new llama_batch(llama_batch_init(static_cast<std::int32_t>(token_count), 0,
                                                      static_cast<std::int32_t>(last - first))));
      std::size_t batch_index = 0;
      for (std::size_t sequence = first; sequence < last; ++sequence) {
        for (std::size_t position = 0; position < token_lists[sequence].size(); ++position) {
          batch->token[batch_index] = token_lists[sequence][position];
          batch->pos[batch_index] = static_cast<llama_pos>(position);
          batch->n_seq_id[batch_index] = 1;
          batch->seq_id[batch_index][0] = static_cast<llama_seq_id>(sequence - first);
          batch->logits[batch_index] = 1;
          ++batch_index;
        }
      }
      batch->n_tokens = static_cast<std::int32_t>(token_count);
      const auto evaluation = llama_model_has_encoder(model.get())
                                  ? llama_encode(context.get(), *batch)
                                  : llama_decode(context.get(), *batch);
      if (evaluation != 0) {
        throw std::runtime_error("embedding evaluation failed with code " +
                                 std::to_string(evaluation));
      }
      std::size_t token_offset = 0;
      for (std::size_t sequence = first; sequence < last; ++sequence) {
        float* source =
            llama_pooling_type(context.get()) == LLAMA_POOLING_TYPE_NONE
                ? llama_get_embeddings_ith(
                      context.get(),
                      static_cast<std::int32_t>(token_offset + token_lists[sequence].size() - 1))
                : llama_get_embeddings_seq(context.get(),
                                           static_cast<llama_seq_id>(sequence - first));
        if (source == nullptr) {
          throw std::runtime_error("embedding model did not return a pooled vector");
        }
        Embedding values(source, source + dimension);
        l2_normalize(values);
        result.push_back(std::move(values));
        token_offset += token_lists[sequence].size();
      }
      first = last;
    }
    return result;
  }

  LlamaRuntime& runtime;
  ModelConfig config;
  EmbedderOptions options;
  ModelPtr model;
  const llama_vocab* vocabulary{};
  std::int32_t dimension{};
  std::mutex mutex;
};

LlamaEmbedder::LlamaEmbedder(LlamaRuntime& runtime, ModelConfig config, EmbedderOptions options)
    : impl_(std::make_unique<Impl>(runtime, std::move(config), std::move(options))) {}
LlamaEmbedder::~LlamaEmbedder() = default;

Embedding LlamaEmbedder::embed(const std::string_view text, const EmbeddingKind kind) {
  std::scoped_lock lock(impl_->mutex);
  return impl_->embed_many(std::span{&text, 1}, kind).front();
}

std::vector<Embedding> LlamaEmbedder::embed_batch(const std::span<const std::string_view> texts,
                                                  const EmbeddingKind kind) {
  std::scoped_lock lock(impl_->mutex);
  return impl_->embed_many(texts, kind);
}

std::size_t LlamaEmbedder::dimension() const noexcept {
  return static_cast<std::size_t>(impl_->dimension);
}

} // namespace llcl::llama_adapter
