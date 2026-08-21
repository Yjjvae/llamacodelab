#include "adapters/vector/brute_force_index.hpp"
#include "adapters/vector/hnsw_index.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <gtest/gtest.h>
#include <random>
#include <unordered_set>

namespace llcl::test {
namespace {

constexpr std::size_t dimensions = 16;
constexpr std::size_t vector_count = 1'000;
constexpr std::size_t query_count = 40;
constexpr std::size_t top_k = 10;

[[nodiscard]] Embedding random_embedding(std::mt19937& random) {
  std::uniform_real_distribution<float> distribution(-1.0F, 1.0F);
  Embedding embedding(dimensions);
  float squared_norm{};
  for (auto& value : embedding) {
    value = distribution(random);
    squared_norm += value * value;
  }
  const auto inverse_norm = 1.0F / std::sqrt(squared_norm);
  for (auto& value : embedding) {
    value *= inverse_norm;
  }
  return embedding;
}

[[nodiscard]] double recall_at_k(const std::vector<SearchHit>& expected,
                                 const std::vector<SearchHit>& actual) {
  std::unordered_set<ChunkId> expected_ids;
  for (const auto& hit : expected) {
    expected_ids.insert(hit.chunk_id);
  }
  std::size_t matches{};
  for (const auto& hit : actual) {
    matches += expected_ids.contains(hit.chunk_id) ? 1U : 0U;
  }
  return static_cast<double>(matches) / static_cast<double>(expected.size());
}

} // namespace

TEST(HnswRecallTest, MatchesBruteForceRecallAtTen) {
  std::mt19937 random(42U);
  vector_adapter::BruteForceIndex baseline(dimensions);
  vector_adapter::HnswIndex hnsw(dimensions, {.max_elements = vector_count, .ef_search = 256});
  for (ChunkId id = 0; id < vector_count; ++id) {
    const auto embedding = random_embedding(random);
    baseline.upsert(id, embedding);
    hnsw.upsert(id, embedding);
  }
  double total_recall{};
  for (std::size_t index = 0; index < query_count; ++index) {
    const auto query = random_embedding(random);
    total_recall += recall_at_k(baseline.search(query, top_k), hnsw.search(query, top_k));
  }
  EXPECT_GE(total_recall / static_cast<double>(query_count), 0.95);
}

} // namespace llcl::test
