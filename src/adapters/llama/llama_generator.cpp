#include "adapters/llama/llama_generator.hpp"

#include "adapters/llama/llama_chat_template.hpp"
#include "llamacodelab/support/logging.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <llama.h>
#include <mutex>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace llcl::llama_adapter {
namespace {

using Clock = std::chrono::steady_clock;

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

struct SamplerDeleter {
  void operator()(llama_sampler* value) const noexcept {
    if (value != nullptr) {
      llama_sampler_free(value);
    }
  }
};

using ModelPtr = std::unique_ptr<llama_model, ModelDeleter>;
using ContextPtr = std::unique_ptr<llama_context, ContextDeleter>;
using SamplerPtr = std::unique_ptr<llama_sampler, SamplerDeleter>;

[[nodiscard]] std::uint32_t checked_u32(std::size_t value, std::string_view name) {
  if (value > std::numeric_limits<std::uint32_t>::max()) {
    throw std::invalid_argument(std::string(name) + " is too large");
  }
  return static_cast<std::uint32_t>(value);
}

[[nodiscard]] std::int32_t checked_i32(std::size_t value, std::string_view name) {
  if (value > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())) {
    throw std::invalid_argument(std::string(name) + " is too large");
  }
  return static_cast<std::int32_t>(value);
}

[[nodiscard]] std::vector<llama_token> tokenize(const llama_vocab* vocabulary,
                                                std::string_view text) {
  const auto text_size = checked_i32(text.size(), "text size");
  const auto required = llama_tokenize(vocabulary, text.data(), text_size, nullptr, 0, true, true);
  if (required == 0) {
    return {};
  }
  if (required > 0 || required == std::numeric_limits<std::int32_t>::min()) {
    throw std::runtime_error("llama_tokenize did not report the required buffer size");
  }

  std::vector<llama_token> result(static_cast<std::size_t>(-required));
  const auto count = llama_tokenize(vocabulary, text.data(), text_size, result.data(),
                                    static_cast<std::int32_t>(result.size()), true, true);
  if (count < 0) {
    throw std::runtime_error("failed to tokenize input text");
  }
  result.resize(static_cast<std::size_t>(count));
  return result;
}

[[nodiscard]] std::string token_piece(const llama_vocab* vocabulary, llama_token token) {
  std::vector<char> buffer(256);
  auto size = llama_token_to_piece(vocabulary, token, buffer.data(),
                                   static_cast<std::int32_t>(buffer.size()), 0, true);
  if (size < 0) {
    buffer.resize(static_cast<std::size_t>(-size));
    size = llama_token_to_piece(vocabulary, token, buffer.data(),
                                static_cast<std::int32_t>(buffer.size()), 0, true);
  }
  if (size < 0) {
    throw std::runtime_error("failed to convert generated token to text");
  }
  return {buffer.data(), static_cast<std::size_t>(size)};
}

[[nodiscard]] bool is_continuation(unsigned char byte) {
  return (byte & 0xC0U) == 0x80U;
}

void emit_complete_utf8(std::string& pending, const TokenCallback& callback, bool final) {
  std::string output;
  std::size_t consumed = 0;

  while (consumed < pending.size()) {
    const auto lead = static_cast<unsigned char>(pending[consumed]);
    std::size_t width = 0;
    if (lead <= 0x7FU) {
      width = 1;
    } else if (lead >= 0xC2U && lead <= 0xDFU) {
      width = 2;
    } else if (lead >= 0xE0U && lead <= 0xEFU) {
      width = 3;
    } else if (lead >= 0xF0U && lead <= 0xF4U) {
      width = 4;
    } else {
      output.append("\xEF\xBF\xBD");
      ++consumed;
      continue;
    }

    if (pending.size() - consumed < width) {
      if (final) {
        output.append("\xEF\xBF\xBD");
        ++consumed;
        continue;
      }
      break;
    }

    bool valid = true;
    for (std::size_t offset = 1; offset < width; ++offset) {
      valid = valid && is_continuation(static_cast<unsigned char>(pending[consumed + offset]));
    }
    if (valid && width == 3) {
      const auto second = static_cast<unsigned char>(pending[consumed + 1]);
      valid = !((lead == 0xE0U && second < 0xA0U) || (lead == 0xEDU && second >= 0xA0U));
    }
    if (valid && width == 4) {
      const auto second = static_cast<unsigned char>(pending[consumed + 1]);
      valid = !((lead == 0xF0U && second < 0x90U) || (lead == 0xF4U && second > 0x8FU));
    }

    if (!valid) {
      output.append("\xEF\xBF\xBD");
      ++consumed;
      continue;
    }

    output.append(pending, consumed, width);
    consumed += width;
  }

  pending.erase(0, consumed);
  if (!output.empty()) {
    callback(output);
  }
}

[[nodiscard]] double tokens_per_second(std::uint64_t tokens, Clock::duration elapsed) {
  const auto seconds = std::chrono::duration<double>(elapsed).count();
  return seconds > 0.0 ? static_cast<double>(tokens) / seconds : 0.0;
}

} // namespace

class LlamaGenerator::Impl {
public:
  Impl(LlamaRuntime& runtime_value, ModelConfig config_value)
      : runtime(runtime_value), config(std::move(config_value)) {
    if (config.path.empty()) {
      throw std::invalid_argument("model path must not be empty");
    }
    if (!std::filesystem::is_regular_file(config.path)) {
      throw std::runtime_error("GGUF model does not exist or is not a file: " +
                               config.path.string());
    }

    const auto load_started = Clock::now();
    auto parameters = llama_model_default_params();
    parameters.n_gpu_layers = config.gpu_layers;
    model.reset(llama_model_load_from_file(config.path.string().c_str(), parameters));
    load_time = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - load_started);
    if (model == nullptr) {
      throw std::runtime_error("failed to load GGUF model: " + config.path.string());
    }

    vocabulary = llama_model_get_vocab(model.get());
    if (vocabulary == nullptr) {
      throw std::runtime_error("model does not expose a vocabulary: " + config.path.string());
    }
    if (llama_model_has_encoder(model.get())) {
      throw std::runtime_error("encoder-decoder models are not supported in M2");
    }
    chat_template = std::make_unique<LlamaChatTemplate>(model.get(), config.chat_template);

    const auto trained_context = llama_model_n_ctx_train(model.get());
    if (trained_context > 0 && config.context_size > static_cast<std::size_t>(trained_context)) {
      throw std::invalid_argument("configured context_size exceeds model training context: " +
                                  std::to_string(config.context_size) + " > " +
                                  std::to_string(trained_context));
    }

    std::vector<char> description_buffer(512);
    const auto description_size =
        llama_model_desc(model.get(), description_buffer.data(), description_buffer.size());
    if (description_size > 0) {
      description.assign(description_buffer.data());
    } else {
      description = config.path.filename().string();
    }
    logger()->info("loaded model '{}' in {} ms (gpu_layers={})", description, load_time.count(),
                   config.gpu_layers);
  }

  LlamaRuntime& runtime;
  ModelConfig config;
  ModelPtr model;
  const llama_vocab* vocabulary{};
  std::string description;
  std::chrono::milliseconds load_time{};
  std::unique_ptr<LlamaChatTemplate> chat_template;
  std::mutex generation_mutex;
  std::atomic<std::uint64_t> next_request_id{1};
};

LlamaGenerator::LlamaGenerator(LlamaRuntime& runtime, ModelConfig config)
    : impl_(std::make_unique<Impl>(runtime, std::move(config))) {}

LlamaGenerator::~LlamaGenerator() = default;

std::size_t LlamaGenerator::count_tokens(std::string_view text) const {
  std::scoped_lock lock(impl_->generation_mutex);
  return tokenize(impl_->vocabulary, text).size();
}

GenerationStats LlamaGenerator::generate(std::string_view prompt, const GenerationOptions& options,
                                         const TokenCallback& on_token,
                                         std::stop_token stop_token) {
  std::scoped_lock lock(impl_->generation_mutex);
  if (prompt.empty()) {
    throw std::invalid_argument("prompt must not be empty");
  }
  if (!on_token) {
    throw std::invalid_argument("token callback must not be empty");
  }
  if (options.max_tokens < 0) {
    throw std::invalid_argument("max_tokens must not be negative");
  }
  if (!std::isfinite(options.temperature) || options.temperature < 0.0F) {
    throw std::invalid_argument("temperature must be finite and non-negative");
  }
  if (!std::isfinite(options.top_p) || options.top_p <= 0.0F || options.top_p > 1.0F) {
    throw std::invalid_argument("top_p must be in (0, 1]");
  }
  if (options.top_k < 0) {
    throw std::invalid_argument("top_k must not be negative");
  }
  if (!std::isfinite(options.repeat_penalty) || options.repeat_penalty <= 0.0F) {
    throw std::invalid_argument("repeat_penalty must be finite and positive");
  }

  const auto request_id = impl_->next_request_id.fetch_add(1, std::memory_order_relaxed);

  const auto started = Clock::now();
  const auto prompt_tokens = tokenize(impl_->vocabulary, prompt);
  if (prompt_tokens.empty()) {
    throw std::runtime_error("prompt tokenization produced no tokens");
  }
  if (prompt_tokens.size() >= impl_->config.context_size) {
    throw std::invalid_argument("prompt needs " + std::to_string(prompt_tokens.size()) +
                                " tokens but context_size is " +
                                std::to_string(impl_->config.context_size));
  }

  auto context_parameters = llama_context_default_params();
  context_parameters.n_ctx = checked_u32(impl_->config.context_size, "context_size");
  context_parameters.n_batch = checked_u32(impl_->config.batch_size, "batch_size");
  context_parameters.n_ubatch = context_parameters.n_batch;
  context_parameters.flash_attn_type = impl_->config.flash_attention
                                           ? LLAMA_FLASH_ATTN_TYPE_ENABLED
                                           : LLAMA_FLASH_ATTN_TYPE_DISABLED;
  context_parameters.no_perf = false;

  ContextPtr context(llama_init_from_model(impl_->model.get(), context_parameters));
  if (context == nullptr) {
    throw std::runtime_error("failed to create llama context");
  }

  auto sampler_parameters = llama_sampler_chain_default_params();
  sampler_parameters.no_perf = false;
  SamplerPtr sampler(llama_sampler_chain_init(sampler_parameters));
  if (sampler == nullptr) {
    throw std::runtime_error("failed to create sampler chain");
  }
  if (options.temperature == 0.0F) {
    llama_sampler_chain_add(sampler.get(), llama_sampler_init_greedy());
  } else {
    llama_sampler_chain_add(sampler.get(), llama_sampler_init_top_k(options.top_k));
    llama_sampler_chain_add(sampler.get(), llama_sampler_init_top_p(options.top_p, 1));
    llama_sampler_chain_add(sampler.get(),
                            llama_sampler_init_penalties(-1, options.repeat_penalty, 0.0F, 0.0F));
    llama_sampler_chain_add(sampler.get(), llama_sampler_init_temp(options.temperature));
    llama_sampler_chain_add(sampler.get(), llama_sampler_init_dist(options.seed));
  }

  const auto prefill_started = Clock::now();
  std::size_t offset = 0;
  while (offset < prompt_tokens.size()) {
    if (stop_token.stop_requested()) {
      return {};
    }
    const auto count = std::min(impl_->config.batch_size, prompt_tokens.size() - offset);
    auto batch = llama_batch_get_one(const_cast<llama_token*>(prompt_tokens.data() + offset),
                                     static_cast<std::int32_t>(count));
    const auto decode_result = llama_decode(context.get(), batch);
    if (decode_result != 0) {
      throw std::runtime_error("llama prompt decode failed with code " +
                               std::to_string(decode_result));
    }
    offset += count;
  }
  const auto prefill_finished = Clock::now();

  GenerationStats stats;
  stats.prompt_tokens = prompt_tokens.size();
  stats.prompt_tokens_per_second =
      tokens_per_second(stats.prompt_tokens, prefill_finished - prefill_started);

  const auto available = impl_->config.context_size - prompt_tokens.size();
  const auto requested = static_cast<std::size_t>(options.max_tokens);
  const auto token_limit = std::min(available, requested);
  std::string pending_utf8;
  Clock::time_point first_token_time{};

  std::string finish_reason = "max_tokens";
  for (std::size_t generated = 0; generated < token_limit; ++generated) {
    if (stop_token.stop_requested()) {
      finish_reason = "cancelled";
      break;
    }

    const auto token = llama_sampler_sample(sampler.get(), context.get(), -1);
    if (llama_vocab_is_eog(impl_->vocabulary, token)) {
      finish_reason = "end_of_generation";
      break;
    }

    if (stats.generated_tokens == 0) {
      first_token_time = Clock::now();
      stats.time_to_first_token =
          std::chrono::duration_cast<std::chrono::milliseconds>(first_token_time - started);
    }

    pending_utf8.append(token_piece(impl_->vocabulary, token));
    emit_complete_utf8(pending_utf8, on_token, false);
    ++stats.generated_tokens;

    if (generated + 1 < token_limit && !stop_token.stop_requested()) {
      auto mutable_token = token;
      auto batch = llama_batch_get_one(&mutable_token, 1);
      const auto decode_result = llama_decode(context.get(), batch);
      if (decode_result != 0) {
        throw std::runtime_error("llama token decode failed with code " +
                                 std::to_string(decode_result));
      }
    }
  }

  emit_complete_utf8(pending_utf8, on_token, true);
  if (stats.generated_tokens > 0) {
    stats.decode_tokens_per_second =
        tokens_per_second(stats.generated_tokens, Clock::now() - prefill_finished);
  }
  logger()->info(
      "request_id={} model='{}' prompt_tokens={} generated_tokens={} ttft_ms={} finish_reason={}",
      request_id, impl_->description, stats.prompt_tokens, stats.generated_tokens,
      stats.time_to_first_token.count(), finish_reason);
  return stats;
}

std::string LlamaGenerator::format_chat(const std::span<const ChatMessage> messages,
                                        const bool add_assistant_prefix) const {
  return format(messages, add_assistant_prefix);
}

std::string LlamaGenerator::format(const std::span<const ChatMessage> messages,
                                   const bool add_assistant_prefix) const {
  std::scoped_lock lock(impl_->generation_mutex);
  return impl_->chat_template->format(messages, add_assistant_prefix);
}

GenerationStats LlamaGenerator::generate_chat(const std::span<const ChatMessage> messages,
                                              const GenerationOptions& options,
                                              const TokenCallback& on_token,
                                              const std::stop_token stop_token) {
  const auto prompt = format_chat(messages, true);
  return generate(prompt, options, on_token, stop_token);
}

const std::string& LlamaGenerator::model_description() const noexcept {
  return impl_->description;
}

std::chrono::milliseconds LlamaGenerator::model_load_time() const noexcept {
  return impl_->load_time;
}

} // namespace llcl::llama_adapter
