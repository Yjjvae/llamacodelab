#include "llamacodelab/application/hybrid_retriever.hpp"

#include <algorithm>
#include <stdexcept>
#include <unordered_map>

namespace llcl {

std::vector<SearchHit> reciprocal_rank_fuse(const std::vector<SearchHit>& vector_hits,
                                            const std::vector<SearchHit>& keyword_hits,
                                            const std::size_t top_k,
                                            const std::size_t rank_constant) {
  if (top_k == 0) {
    return {};
  }
  if (rank_constant == 0) {
    throw std::invalid_argument("RRF rank constant must be positive");
  }
  std::unordered_map<ChunkId, float> scores;
  const auto accumulate = [&scores, rank_constant](const std::vector<SearchHit>& hits) {
    for (std::size_t index = 0; index < hits.size(); ++index) {
      scores[hits[index].chunk_id] += 1.0F / static_cast<float>(rank_constant + index + 1U);
    }
  };
  accumulate(vector_hits);
  accumulate(keyword_hits);
  std::vector<SearchHit> fused;
  fused.reserve(scores.size());
  for (const auto& [chunk_id, score] : scores) {
    fused.push_back({.chunk_id = chunk_id, .score = score});
  }
  std::sort(fused.begin(), fused.end(), [](const SearchHit& lhs, const SearchHit& rhs) {
    return lhs.score > rhs.score || (lhs.score == rhs.score && lhs.chunk_id < rhs.chunk_id);
  });
  fused.resize(std::min(top_k, fused.size()));
  return fused;
}

} // namespace llcl
