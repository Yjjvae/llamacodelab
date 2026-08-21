#pragma once

#include "llamacodelab/domain/chunk.hpp"

#include <cstddef>
#include <span>
#include <string_view>
#include <vector>

namespace llcl {

using Embedding = std::vector<float>;

enum class EmbeddingKind {
  query,
  document,
};

struct SearchHit {
  ChunkId chunk_id{};
  float score{};
};

class IEmbedder {
public:
  virtual ~IEmbedder() = default;

  [[nodiscard]] virtual Embedding embed(std::string_view text, EmbeddingKind kind) = 0;
  [[nodiscard]] virtual std::vector<Embedding> embed_batch(std::span<const std::string_view> texts,
                                                           EmbeddingKind kind) = 0;
  [[nodiscard]] virtual std::size_t dimension() const noexcept = 0;
};

class IVectorIndex {
public:
  virtual ~IVectorIndex() = default;

  virtual void upsert(ChunkId id, std::span<const float> values) = 0;
  virtual void erase(ChunkId id) = 0;
  [[nodiscard]] virtual std::vector<SearchHit> search(std::span<const float> query,
                                                      std::size_t top_k) const = 0;
  [[nodiscard]] virtual std::size_t size() const noexcept = 0;
  [[nodiscard]] virtual std::size_t dimension() const noexcept = 0;
};

class IKeywordSearcher {
public:
  virtual ~IKeywordSearcher() = default;

  [[nodiscard]] virtual std::vector<SearchHit> search(std::string_view query,
                                                      std::size_t top_k) const = 0;
};

class IRetriever {
public:
  virtual ~IRetriever() = default;

  [[nodiscard]] virtual std::vector<SearchHit> retrieve(std::string_view query,
                                                        std::size_t top_k) const = 0;
};

class IChunkRepository {
public:
  virtual ~IChunkRepository() = default;
  [[nodiscard]] virtual std::vector<Chunk> get_many(std::span<const ChunkId> ids) const = 0;
};

} // namespace llcl
