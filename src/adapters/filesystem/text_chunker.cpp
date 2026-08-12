#include "adapters/filesystem/text_chunker.hpp"

#include "llamacodelab/support/hash.hpp"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <utility>

namespace llcl::filesystem_adapter {
namespace {

[[nodiscard]] std::vector<std::string> split_lines(const std::string_view text) {
  std::vector<std::string> lines;
  std::size_t start = 0;
  while (start < text.size()) {
    const auto newline = text.find('\n', start);
    const auto end = newline == std::string_view::npos ? text.size() : newline;
    auto line = std::string(text.substr(start, end - start));
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    lines.push_back(std::move(line));
    if (newline == std::string_view::npos) {
      break;
    }
    start = newline + 1U;
  }
  return lines;
}

[[nodiscard]] std::size_t content_size(const std::vector<std::string>& lines,
                                       const std::size_t start, const std::size_t end) {
  std::size_t result = 0;
  for (auto index = start; index < end; ++index) {
    result += lines[index].size();
    if (index + 1U < end) {
      ++result;
    }
  }
  return result;
}

[[nodiscard]] std::string join_lines(const std::vector<std::string>& lines, const std::size_t start,
                                     const std::size_t end) {
  std::string result;
  result.reserve(content_size(lines, start, end));
  for (auto index = start; index < end; ++index) {
    if (index != start) {
      result.push_back('\n');
    }
    result.append(lines[index]);
  }
  return result;
}

void validate_options(const ChunkingOptions& options) {
  if (options.max_lines == 0) {
    throw std::invalid_argument("chunk max_lines must be positive");
  }
  if (options.overlap_lines >= options.max_lines) {
    throw std::invalid_argument("chunk overlap_lines must be smaller than max_lines");
  }
  if (options.max_bytes == 0) {
    throw std::invalid_argument("chunk max_bytes must be positive");
  }
  if (options.version.empty()) {
    throw std::invalid_argument("chunk version must not be empty");
  }
}

[[nodiscard]] std::uint32_t line_number(const std::size_t zero_based) {
  if (zero_based >= std::numeric_limits<std::uint32_t>::max()) {
    throw std::invalid_argument("source file has too many lines");
  }
  return static_cast<std::uint32_t>(zero_based + 1U);
}

} // namespace

std::vector<Chunk> TextChunker::chunk_file(const ScannedFile& file,
                                           const ChunkingOptions& options) const {
  std::ifstream input(file.absolute_path, std::ios::binary);
  if (!input) {
    throw std::runtime_error("cannot read scanned file: " + file.absolute_path.string());
  }
  const std::string text{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
  return chunk_text(file.relative_path, file.language, text, options);
}

std::vector<Chunk> TextChunker::chunk_text(const std::filesystem::path& relative_path,
                                           const std::string_view language,
                                           const std::string_view text,
                                           const ChunkingOptions& options) const {
  validate_options(options);
  const auto lines = split_lines(text);
  if (lines.empty()) {
    return {};
  }

  const auto normalized_path = relative_path.lexically_normal().generic_string();
  if (normalized_path.empty() || relative_path.is_absolute() ||
      normalized_path.starts_with("../")) {
    throw std::invalid_argument("chunk path must be a relative path inside the repository");
  }

  std::vector<Chunk> chunks;
  std::size_t start = 0;
  while (start < lines.size()) {
    auto end = std::min(start + options.max_lines, lines.size());
    while (end > start + 1U && content_size(lines, start, end) > options.max_bytes) {
      --end;
    }
    const auto content = join_lines(lines, start, end);
    const auto content_hash = stable_hash_hex(content);
    const auto start_line = line_number(start);
    const auto end_line = line_number(end - 1U);
    const std::string separator(1, '\0');
    const auto key = normalized_path + separator + std::to_string(start_line) + separator +
                     std::to_string(end_line) + separator + content_hash + separator +
                     options.version;
    chunks.push_back({
        .id = stable_hash64(key),
        .source = {.path = std::filesystem::path(normalized_path),
                   .start_line = start_line,
                   .end_line = end_line},
        .language = std::string(language),
        .content = content,
        .content_hash = content_hash,
        .chunker_version = options.version,
    });

    if (end == lines.size()) {
      break;
    }
    const auto next_start = end > options.overlap_lines ? end - options.overlap_lines : end;
    start = std::max(start + 1U, next_start);
  }
  return chunks;
}

} // namespace llcl::filesystem_adapter
