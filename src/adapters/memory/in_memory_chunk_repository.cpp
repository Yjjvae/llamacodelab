#include "adapters/memory/in_memory_chunk_repository.hpp"

namespace llcl::memory_adapter {

void InMemoryChunkRepository::replace(std::vector<Chunk> chunks) {
  chunks_.clear();
  chunks_.reserve(chunks.size());
  for (auto& chunk : chunks) {
    chunks_.insert_or_assign(chunk.id, std::move(chunk));
  }
}

std::vector<Chunk> InMemoryChunkRepository::get_many(const std::span<const ChunkId> ids) const {
  std::vector<Chunk> result;
  result.reserve(ids.size());
  for (const auto id : ids) {
    if (const auto found = chunks_.find(id); found != chunks_.end()) {
      result.push_back(found->second);
    }
  }
  return result;
}

std::size_t InMemoryChunkRepository::size() const noexcept {
  return chunks_.size();
}

} // namespace llcl::memory_adapter
