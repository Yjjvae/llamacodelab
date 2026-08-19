#include "adapters/vector/brute_force_index.hpp"

#include <gtest/gtest.h>
#include <limits>
#include <stdexcept>
#include <vector>

namespace llcl::test {

TEST(BruteForceIndexTest, ReturnsStableTopKAndHonorsBounds) {
  vector_adapter::BruteForceIndex index(2);
  const Embedding horizontal{1.0F, 0.0F};
  const Embedding vertical{0.0F, 1.0F};
  index.upsert(9, horizontal);
  index.upsert(3, horizontal);
  index.upsert(5, vertical);
  EXPECT_TRUE(index.search(horizontal, 0).empty());
  const auto hits = index.search(horizontal, 99);
  ASSERT_EQ(hits.size(), 3U);
  EXPECT_EQ(hits[0].chunk_id, 3U);
  EXPECT_EQ(hits[1].chunk_id, 9U);
  EXPECT_EQ(hits[2].chunk_id, 5U);
}

TEST(BruteForceIndexTest, UpsertsAndErasesWithoutDuplicates) {
  vector_adapter::BruteForceIndex index(2);
  const Embedding horizontal{1.0F, 0.0F};
  const Embedding vertical{0.0F, 1.0F};
  index.upsert(7, horizontal);
  index.upsert(7, vertical);
  EXPECT_EQ(index.size(), 1U);
  EXPECT_EQ(index.search(vertical, 1).front().chunk_id, 7U);
  index.erase(7);
  EXPECT_TRUE(index.search(vertical, 1).empty());
}

TEST(BruteForceIndexTest, RejectsInvalidDimensionsAndNonFiniteValues) {
  vector_adapter::BruteForceIndex index(2);
  const Embedding one_value{1.0F};
  const Embedding non_finite{1.0F, std::numeric_limits<float>::quiet_NaN()};
  EXPECT_THROW(index.upsert(1, one_value), std::invalid_argument);
  EXPECT_THROW(index.upsert(1, non_finite), std::invalid_argument);
  EXPECT_THROW(static_cast<void>(index.search(one_value, 1)), std::invalid_argument);
}

} // namespace llcl::test
