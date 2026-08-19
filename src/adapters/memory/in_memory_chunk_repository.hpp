#pragma once

#include "llamacodelab/domain/retrieval.hpp"

#include <unordered_map>
#include <vector>

namespace llcl::memory_adapter {

class InMemoryChunkRepository final : public IChunkRepository {
public:
  void replace(std::vector<Chunk> chunks);
  [[nodiscard]] std::vector<Chunk> get_many(std::span<const ChunkId> ids) const override;
  [[nodiscard]] std::size_t size() const noexcept;

private:
  std::unordered_map<ChunkId, Chunk> chunks_;
};

} // namespace llcl::memory_adapter
