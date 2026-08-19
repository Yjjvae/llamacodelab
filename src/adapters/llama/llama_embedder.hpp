#pragma once

#include "adapters/llama/llama_runtime.hpp"
#include "llamacodelab/domain/retrieval.hpp"
#include "llamacodelab/support/config.hpp"

#include <memory>
#include <string>

namespace llcl::llama_adapter {

struct EmbedderOptions {
  std::string query_prefix{"search_query: "};
  std::string document_prefix{"search_document: "};
};

class LlamaEmbedder final : public IEmbedder {
public:
  LlamaEmbedder(LlamaRuntime& runtime, ModelConfig config, EmbedderOptions options = {});
  ~LlamaEmbedder() override;

  LlamaEmbedder(const LlamaEmbedder&) = delete;
  LlamaEmbedder& operator=(const LlamaEmbedder&) = delete;

  [[nodiscard]] Embedding embed(std::string_view text, EmbeddingKind kind) override;
  [[nodiscard]] std::vector<Embedding> embed_batch(std::span<const std::string_view> texts,
                                                   EmbeddingKind kind) override;
  [[nodiscard]] std::size_t dimension() const noexcept override;

private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace llcl::llama_adapter
