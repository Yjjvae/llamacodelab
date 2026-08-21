#include "llamacodelab/domain/evaluation.hpp"

#include <gtest/gtest.h>
#include <stdexcept>
#include <unordered_map>

namespace llcl::test {

TEST(EvaluationTest, CalculatesRecallMrrAndNdcgAtK) {
  const std::vector<SearchHit> hits{{.chunk_id = 4, .score = 0.9F},
                                    {.chunk_id = 2, .score = 0.8F},
                                    {.chunk_id = 1, .score = 0.7F}};
  const std::unordered_map<ChunkId, unsigned int> relevance{{1, 1}, {2, 3}};

  const auto metrics = evaluate_ranking(hits, relevance, 3);

  EXPECT_DOUBLE_EQ(metrics.recall_at_k, 1.0);
  EXPECT_DOUBLE_EQ(metrics.reciprocal_rank, 0.5);
  EXPECT_GT(metrics.ndcg_at_k, 0.6);
  EXPECT_LT(metrics.ndcg_at_k, 1.0);
}

TEST(EvaluationTest, HandlesNoRelevantDocumentsAndValidatesInputs) {
  const std::vector<SearchHit> hits{{.chunk_id = 1, .score = 1.0F}};
  EXPECT_EQ(evaluate_ranking(hits, {}, 1).recall_at_k, 0.0);
  EXPECT_THROW(static_cast<void>(evaluate_ranking(hits, {}, 0)), std::invalid_argument);
  const std::vector<double> samples{3.0, 1.0, 2.0, 4.0};
  EXPECT_DOUBLE_EQ(percentile(samples, 0.5), 3.0);
  EXPECT_DOUBLE_EQ(percentile(samples, 0.95), 4.0);
  EXPECT_THROW(static_cast<void>(percentile({}, 0.5)), std::invalid_argument);
}

} // namespace llcl::test
