#include "llamacodelab/application/hybrid_retriever.hpp"

#include <algorithm>
#include <stdexcept>

namespace llcl {

VectorRetriever::VectorRetriever(IEmbedder& embedder, const IVectorIndex& index)
    : embedder_(embedder), index_(index) {}

std::vector<SearchHit> VectorRetriever::retrieve(const std::string_view query,
                                                 const std::size_t top_k) const {
  if (query.empty() || top_k == 0) {
    throw std::invalid_argument("retrieval query and top_k must be provided");
  }
  return index_.search(embedder_.embed(query, EmbeddingKind::query), top_k);
}

HybridRetriever::HybridRetriever(IEmbedder& embedder, const IVectorIndex& vector_index,
                                 const IKeywordSearcher& keyword_searcher,
                                 HybridRetrievalOptions options)
    : embedder_(embedder), vector_index_(vector_index), keyword_searcher_(keyword_searcher),
      options_(options) {
  if (options_.vector_candidates == 0 || options_.keyword_candidates == 0 ||
      options_.rrf_rank_constant == 0) {
    throw std::invalid_argument("hybrid retrieval options must be positive");
  }
}

std::vector<SearchHit> HybridRetriever::retrieve(const std::string_view query,
                                                 const std::size_t top_k) const {
  if (query.empty() || top_k == 0) {
    throw std::invalid_argument("retrieval query and top_k must be provided");
  }
  const auto vector_limit = std::max(top_k, options_.vector_candidates);
  const auto keyword_limit = std::max(top_k, options_.keyword_candidates);
  const auto vector_hits =
      vector_index_.search(embedder_.embed(query, EmbeddingKind::query), vector_limit);
  const auto keyword_hits = keyword_searcher_.search(query, keyword_limit);
  return reciprocal_rank_fuse(vector_hits, keyword_hits, top_k, options_.rrf_rank_constant);
}

} // namespace llcl
