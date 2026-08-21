#pragma once

#include "llamacodelab/domain/generation.hpp"
#include "llamacodelab/domain/retrieval.hpp"

namespace llcl::llama_adapter {

class LlamaReranker final : public IReranker {
public:
  explicit LlamaReranker(ITextGenerator& generator);

  [[nodiscard]] std::vector<SearchHit> rerank(std::string_view query,
                                              std::span<const SearchHit> candidates,
                                              std::span<const Chunk> chunks,
                                              std::size_t top_k) override;

private:
  ITextGenerator& generator_;
};

} // namespace llcl::llama_adapter
