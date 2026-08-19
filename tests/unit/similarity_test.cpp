#include "llamacodelab/domain/similarity.hpp"

#include <cmath>
#include <gtest/gtest.h>
#include <limits>
#include <stdexcept>
#include <vector>

namespace llcl::test {

TEST(SimilarityTest, NormalizesAndComputesCosineAsDotProduct) {
  std::vector<float> values{3.0F, 4.0F};
  l2_normalize(values);
  EXPECT_NEAR(values[0], 0.6F, 1e-6F);
  EXPECT_NEAR(values[1], 0.8F, 1e-6F);
  EXPECT_NEAR(dot_product(values, values), 1.0F, 1e-6F);
}

TEST(SimilarityTest, RejectsZeroNonFiniteAndMismatchedVectors) {
  std::vector<float> zero{0.0F, 0.0F};
  EXPECT_THROW(l2_normalize(zero), std::runtime_error);
  std::vector<float> non_finite{1.0F, std::numeric_limits<float>::infinity()};
  EXPECT_THROW(l2_normalize(non_finite), std::invalid_argument);
  const std::vector<float> lhs{1.0F};
  const std::vector<float> rhs{1.0F, 2.0F};
  EXPECT_THROW(static_cast<void>(dot_product(lhs, rhs)), std::invalid_argument);
}

} // namespace llcl::test
