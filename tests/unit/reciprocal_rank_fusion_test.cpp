#include "llamacodelab/application/hybrid_retriever.hpp"

#include <gtest/gtest.h>
#include <stdexcept>

namespace llcl::test {

TEST(ReciprocalRankFusionTest, RewardsDocumentsFoundByBothRankers) {
  const std::vector<SearchHit> vector_hits{{.chunk_id = 1, .score = 0.9F},
                                           {.chunk_id = 2, .score = 0.8F}};
  const std::vector<SearchHit> keyword_hits{{.chunk_id = 2, .score = 8.0F},
                                            {.chunk_id = 3, .score = 7.0F}};

  const auto fused = reciprocal_rank_fuse(vector_hits, keyword_hits, 3);

  ASSERT_EQ(fused.size(), 3U);
  EXPECT_EQ(fused[0].chunk_id, 2U);
  EXPECT_EQ(fused[1].chunk_id, 1U);
  EXPECT_EQ(fused[2].chunk_id, 3U);
}

TEST(ReciprocalRankFusionTest, EnforcesBoundsAndRejectsInvalidRankConstant) {
  const std::vector<SearchHit> hits{{.chunk_id = 7, .score = 1.0F}};
  EXPECT_TRUE(reciprocal_rank_fuse(hits, {}, 0).empty());
  EXPECT_THROW(static_cast<void>(reciprocal_rank_fuse(hits, {}, 1, 0)), std::invalid_argument);
}

} // namespace llcl::test
