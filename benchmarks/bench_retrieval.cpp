#include "adapters/vector/brute_force_index.hpp"
#include "adapters/vector/hnsw_index.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <random>
#include <unordered_set>
#include <vector>

namespace {

constexpr std::size_t dimension = 64;
constexpr std::size_t records = 10'000;
constexpr std::size_t queries = 200;
constexpr std::size_t top_k = 10;

struct QuerySample {
  llcl::Embedding embedding;
  std::vector<llcl::SearchHit> baseline_hits;
};

[[nodiscard]] llcl::Embedding random_embedding(std::mt19937& random) {
  std::normal_distribution<float> distribution;
  llcl::Embedding result(dimension);
  float squared_norm{};
  for (auto& value : result) {
    value = distribution(random);
    squared_norm += value * value;
  }
  const auto inverse_norm = 1.0F / std::sqrt(squared_norm);
  for (auto& value : result) {
    value *= inverse_norm;
  }
  return result;
}

[[nodiscard]] double recall(const std::vector<llcl::SearchHit>& baseline,
                            const std::vector<llcl::SearchHit>& candidate) {
  std::unordered_set<llcl::ChunkId> ids;
  for (const auto& hit : baseline) {
    ids.insert(hit.chunk_id);
  }
  std::size_t matches{};
  for (const auto& hit : candidate) {
    matches += ids.contains(hit.chunk_id) ? 1U : 0U;
  }
  return static_cast<double>(matches) / static_cast<double>(baseline.size());
}

[[nodiscard]] long long percentile_us(std::vector<long long> values, const double percentile) {
  std::sort(values.begin(), values.end());
  const auto position = static_cast<std::size_t>(percentile * static_cast<double>(values.size() - 1U));
  return values[position];
}

} // namespace

int main() {
  std::mt19937 random(42U);
  llcl::vector_adapter::BruteForceIndex baseline{dimension};
  llcl::vector_adapter::HnswIndex hnsw{
      dimension, {.max_elements = records, .m = 16, .ef_construction = 200, .ef_search = 256}};
  const auto build_started = std::chrono::steady_clock::now();
  for (llcl::ChunkId id = 0; id < records; ++id) {
    const auto vector = random_embedding(random);
    baseline.upsert(id, vector);
    hnsw.upsert(id, vector);
  }
  const auto build_elapsed = std::chrono::steady_clock::now() - build_started;
  std::vector<long long> brute_latencies;
  brute_latencies.reserve(queries);
  std::vector<QuerySample> samples;
  samples.reserve(queries);
  for (std::size_t index = 0; index < queries; ++index) {
    const auto query = random_embedding(random);
    const auto brute_started = std::chrono::steady_clock::now();
    const auto brute_hits = baseline.search(query, top_k);
    brute_latencies.push_back(std::chrono::duration_cast<std::chrono::microseconds>(
                                  std::chrono::steady_clock::now() - brute_started)
                                  .count());
    samples.push_back({.embedding = query, .baseline_hits = brute_hits});
  }
  std::cout << "records=" << records << " dimensions=" << dimension << " top_k=" << top_k
            << " build_ms="
            << std::chrono::duration_cast<std::chrono::milliseconds>(build_elapsed).count()
            << " brute_p50_us=" << percentile_us(brute_latencies, 0.50)
            << " brute_p95_us=" << percentile_us(brute_latencies, 0.95) << '\n';
  for (const auto ef_search : {64U, 128U, 256U}) {
    hnsw.set_ef_search(ef_search);
    std::vector<long long> hnsw_latencies;
    hnsw_latencies.reserve(queries);
    double total_recall{};
    for (const auto& sample : samples) {
      const auto hnsw_started = std::chrono::steady_clock::now();
      const auto hnsw_hits = hnsw.search(sample.embedding, top_k);
      hnsw_latencies.push_back(std::chrono::duration_cast<std::chrono::microseconds>(
                                   std::chrono::steady_clock::now() - hnsw_started)
                                   .count());
      total_recall += recall(sample.baseline_hits, hnsw_hits);
    }
    std::cout << "ef_search=" << ef_search
              << " hnsw_p50_us=" << percentile_us(hnsw_latencies, 0.50)
              << " hnsw_p95_us=" << percentile_us(hnsw_latencies, 0.95)
              << " recall_at_10=" << total_recall / static_cast<double>(queries) << '\n';
  }
}
