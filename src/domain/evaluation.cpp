#include "llamacodelab/domain/evaluation.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

namespace llcl {

RetrievalMetrics evaluate_ranking(const std::span<const SearchHit> ranked_hits,
                                  const std::unordered_map<ChunkId, unsigned int>& relevance,
                                  const std::size_t k) {
  if (k == 0) {
    throw std::invalid_argument("evaluation k must be positive");
  }
  std::size_t relevant_total{};
  double ideal_dcg{};
  std::vector<unsigned int> grades;
  grades.reserve(relevance.size());
  for (const auto& [_, grade] : relevance) {
    if (grade > 0) {
      ++relevant_total;
      grades.push_back(grade);
    }
  }
  std::sort(grades.begin(), grades.end(), std::greater<>());
  for (std::size_t index = 0; index < std::min(k, grades.size()); ++index) {
    ideal_dcg += (std::exp2(static_cast<double>(grades[index])) - 1.0) /
                 std::log2(static_cast<double>(index) + 2.0);
  }

  RetrievalMetrics metrics;
  std::size_t found{};
  double dcg{};
  for (std::size_t index = 0; index < std::min(k, ranked_hits.size()); ++index) {
    const auto found_grade = relevance.find(ranked_hits[index].chunk_id);
    const auto grade = found_grade == relevance.end() ? 0U : found_grade->second;
    if (grade > 0) {
      ++found;
      if (metrics.reciprocal_rank == 0.0) {
        metrics.reciprocal_rank = 1.0 / static_cast<double>(index + 1U);
      }
      dcg += (std::exp2(static_cast<double>(grade)) - 1.0) /
             std::log2(static_cast<double>(index) + 2.0);
    }
  }
  metrics.recall_at_k =
      relevant_total == 0 ? 0.0 : static_cast<double>(found) / static_cast<double>(relevant_total);
  metrics.ndcg_at_k = ideal_dcg == 0.0 ? 0.0 : dcg / ideal_dcg;
  return metrics;
}

double percentile(const std::span<const double> values, const double quantile) {
  if (values.empty()) {
    throw std::invalid_argument("cannot calculate percentile of an empty sample");
  }
  if (!std::isfinite(quantile) || quantile < 0.0 || quantile > 1.0) {
    throw std::invalid_argument("percentile quantile must be in [0, 1]");
  }
  std::vector<double> sorted(values.begin(), values.end());
  std::sort(sorted.begin(), sorted.end());
  const auto position =
      static_cast<std::size_t>(std::ceil(quantile * static_cast<double>(sorted.size() - 1U)));
  return sorted[position];
}

} // namespace llcl
