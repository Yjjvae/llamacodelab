#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace llcl {

using ChunkId = std::uint64_t;

struct SourceRange {
  std::filesystem::path path;
  std::uint32_t start_line{};
  std::uint32_t end_line{};
};

struct Chunk {
  ChunkId id{};
  SourceRange source;
  std::string language;
  std::string content;
  std::string content_hash;
  std::string chunker_version;
};

} // namespace llcl
