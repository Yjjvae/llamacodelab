#include "adapters/vector/brute_force_index.hpp"
#include "adapters/vector/hnsw_index.hpp"
#include "llamacodelab/domain/evaluation.hpp"

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
constexpr std::size_t default_records = 10'000;
constexpr std::size_t default_queries = 200;
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

[[nodiscard]] std::size_t argument_or(const int argc, char** argv, const int position,
                                      const std::size_t fallback) {
  return argc <= position ? fallback : static_cast<std::size_t>(std::stoull(argv[position]));
}

} // namespace

int main(int argc, char** argv) {
  const auto records = argument_or(argc, argv, 1, default_records);
  const auto queries = argument_or(argc, argv, 2, default_queries);
  const auto seed = static_cast<std::uint32_t>(argument_or(argc, argv, 3, 42));
  std::mt19937 random(seed);
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
  std::vector<double> brute_latencies;
  brute_latencies.reserve(queries);
  std::vector<QuerySample> samples;
  samples.reserve(queries);
  for (std::size_t index = 0; index < queries; ++index) {
    const auto query = random_embedding(random);
    const auto brute_started = std::chrono::steady_clock::now();
    const auto brute_hits = baseline.search(query, top_k);
    brute_latencies.push_back(std::chrono::duration<double, std::milli>(
                                  std::chrono::steady_clock::now() - brute_started)
                                  .count());
    samples.push_back({.embedding = query, .baseline_hits = brute_hits});
  }
  for (const auto ef_search : {64U, 128U, 256U}) {
    hnsw.set_ef_search(ef_search);
    std::vector<double> hnsw_latencies;
    hnsw_latencies.reserve(queries);
    double total_recall{};
    double total_mrr{};
    double total_ndcg{};
    for (const auto& sample : samples) {
      const auto hnsw_started = std::chrono::steady_clock::now();
      const auto hnsw_hits = hnsw.search(sample.embedding, top_k);
      hnsw_latencies.push_back(std::chrono::duration<double, std::milli>(
                                   std::chrono::steady_clock::now() - hnsw_started)
                                   .count());
      total_recall += recall(sample.baseline_hits, hnsw_hits);
      std::unordered_map<llcl::ChunkId, unsigned int> relevance;
      for (const auto& hit : sample.baseline_hits) {
        relevance.emplace(hit.chunk_id, 1U);
      }
      const auto metrics = llcl::evaluate_ranking(hnsw_hits, relevance, top_k);
      total_mrr += metrics.reciprocal_rank;
      total_ndcg += metrics.ndcg_at_k;
    }
    std::cout << "{\"benchmark\":\"retrieval\",\"seed\":" << seed
              << ",\"records\":" << records << ",\"queries\":" << queries
              << ",\"dimensions\":" << dimension << ",\"top_k\":" << top_k
              << ",\"build_ms\":"
              << std::chrono::duration<double, std::milli>(build_elapsed).count()
              << ",\"index\":\"hnsw\",\"ef_search\":" << ef_search
              << ",\"recall_at_10\":" << total_recall / static_cast<double>(queries)
              << ",\"mrr_at_10\":" << total_mrr / static_cast<double>(queries)
              << ",\"ndcg_at_10\":" << total_ndcg / static_cast<double>(queries)
              << ",\"brute_p50_ms\":" << llcl::percentile(brute_latencies, 0.50)
              << ",\"brute_p95_ms\":" << llcl::percentile(brute_latencies, 0.95)
              << ",\"hnsw_p50_ms\":" << llcl::percentile(hnsw_latencies, 0.50)
              << ",\"hnsw_p95_ms\":" << llcl::percentile(hnsw_latencies, 0.95) << "}\n";
  }
}
