#pragma once

#include "llamacodelab/domain/retrieval.hpp"

#include <cstddef>
#include <span>
#include <unordered_map>

namespace llcl {

struct RetrievalMetrics {
  double recall_at_k{};
  double reciprocal_rank{};
  double ndcg_at_k{};
};

// Relevance grades are non-negative. Missing ids have relevance zero.
[[nodiscard]] RetrievalMetrics
evaluate_ranking(std::span<const SearchHit> ranked_hits,
                 const std::unordered_map<ChunkId, unsigned int>& relevance, std::size_t k);

[[nodiscard]] double percentile(std::span<const double> values, double quantile);

} // namespace llcl
