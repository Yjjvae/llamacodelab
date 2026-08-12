#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace llcl::filesystem_adapter {

struct FileScannerOptions {
  std::size_t max_file_bytes{1024U * 1024U};
  std::vector<std::string> include_globs;
  std::vector<std::string> exclude_globs;
};

struct ScannedFile {
  std::filesystem::path absolute_path;
  std::filesystem::path relative_path;
  std::string language;
  std::size_t size_bytes{};
};

struct FileScanStats {
  std::size_t files_seen{};
  std::size_t files_indexable{};
  std::size_t files_skipped{};
};

struct FileScanResult {
  std::vector<ScannedFile> files;
  FileScanStats stats;
};

class FileScanner {
public:
  [[nodiscard]] FileScanResult scan(const std::filesystem::path& repository_root,
                                    const FileScannerOptions& options = {}) const;
};

} // namespace llcl::filesystem_adapter
