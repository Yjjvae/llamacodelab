#include "adapters/vector/brute_force_index.hpp"

#include <chrono>
#include <cstddef>
#include <iostream>
#include <vector>

int main() {
  constexpr std::size_t kDimension = 768;
  constexpr std::size_t kRecords = 10'000;
  llcl::vector_adapter::BruteForceIndex index{kDimension};
  std::vector<float> vector(kDimension, 1.0F / static_cast<float>(kDimension));
  for (std::size_t id = 0; id < kRecords; ++id) {
    vector[0] = static_cast<float>(id % 97U) / 97.0F;
    index.upsert(id, vector);
  }
  const auto start = std::chrono::steady_clock::now();
  const auto hits = index.search(vector, 5);
  const auto elapsed = std::chrono::steady_clock::now() - start;
  std::cout << "records=" << kRecords << " top_k=" << hits.size() << " elapsed_us="
            << std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count() << '\n';
}
