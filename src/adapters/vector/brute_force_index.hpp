#pragma once

#include "llamacodelab/domain/retrieval.hpp"

#include <cstddef>
#include <unordered_map>
#include <vector>

namespace llcl::vector_adapter {

class BruteForceIndex final : public IVectorIndex {
public:
  explicit BruteForceIndex(std::size_t dimension);

  void upsert(ChunkId id, std::span<const float> values) override;
  void erase(ChunkId id) override;
  [[nodiscard]] std::vector<SearchHit> search(std::span<const float> query,
                                              std::size_t top_k) const override;
  [[nodiscard]] std::size_t size() const noexcept override;
  [[nodiscard]] std::size_t dimension() const noexcept override;

private:
  struct VectorRecord {
    ChunkId id{};
    Embedding values;
  };

  std::size_t dimension_;
  std::vector<VectorRecord> records_;
  std::unordered_map<ChunkId, std::size_t> positions_;
};

} // namespace llcl::vector_adapter
