#pragma once

#include "adapters/filesystem/file_scanner.hpp"
#include "llamacodelab/domain/chunk.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace llcl::filesystem_adapter {

struct ChunkingOptions {
  std::size_t max_lines{80};
  std::size_t overlap_lines{16};
  std::size_t max_bytes{12U * 1024U};
  std::string version{"text-v1"};
};

class TextChunker {
public:
  [[nodiscard]] std::vector<Chunk> chunk_file(const ScannedFile& file,
                                              const ChunkingOptions& options = {}) const;
  [[nodiscard]] std::vector<Chunk> chunk_text(const std::filesystem::path& relative_path,
                                              std::string_view language, std::string_view text,
                                              const ChunkingOptions& options = {}) const;
};

} // namespace llcl::filesystem_adapter
