#pragma once

#include "llamacodelab/domain/retrieval.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace llcl::vector_adapter {

struct VectorFileMetadata {
  std::uint32_t dimension{};
  std::string model_hash;
};

struct StoredVector {
  ChunkId chunk_id{};
  Embedding values;
};

class VectorFile {
public:
  static void write_atomic(const std::filesystem::path& path, const VectorFileMetadata& metadata,
                           const std::vector<StoredVector>& records);
  [[nodiscard]] static std::vector<StoredVector> read(const std::filesystem::path& path,
                                                      VectorFileMetadata* metadata = nullptr);
};

} // namespace llcl::vector_adapter
