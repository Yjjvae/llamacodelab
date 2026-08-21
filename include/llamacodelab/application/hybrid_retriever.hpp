#pragma once

#include "llamacodelab/domain/retrieval.hpp"

#include <cstddef>
#include <string_view>
#include <vector>

namespace llcl {

struct HybridRetrievalOptions {
  std::size_t vector_candidates{30};
  std::size_t keyword_candidates{30};
  std::size_t rrf_rank_constant{60};
};

[[nodiscard]] std::vector<SearchHit>
reciprocal_rank_fuse(const std::vector<SearchHit>& vector_hits,
                     const std::vector<SearchHit>& keyword_hits, std::size_t top_k,
                     std::size_t rank_constant = 60);

class VectorRetriever final : public IRetriever {
public:
  VectorRetriever(IEmbedder& embedder, const IVectorIndex& index);
  [[nodiscard]] std::vector<SearchHit> retrieve(std::string_view query,
                                                std::size_t top_k) const override;

private:
  IEmbedder& embedder_;
  const IVectorIndex& index_;
};

class HybridRetriever final : public IRetriever {
public:
  HybridRetriever(IEmbedder& embedder, const IVectorIndex& vector_index,
                  const IKeywordSearcher& keyword_searcher, HybridRetrievalOptions options = {});
  [[nodiscard]] std::vector<SearchHit> retrieve(std::string_view query,
                                                std::size_t top_k) const override;

private:
  IEmbedder& embedder_;
  const IVectorIndex& vector_index_;
  const IKeywordSearcher& keyword_searcher_;
  HybridRetrievalOptions options_;
};

} // namespace llcl
