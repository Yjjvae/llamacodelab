#include "adapters/filesystem/file_scanner.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <ranges>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace llcl::filesystem_adapter {
namespace {

struct IgnoreRule {
  std::string pattern;
  bool negated{};
  bool directory_only{};
  bool contains_slash{};
};

[[nodiscard]] std::string normalized_path(const std::filesystem::path& path) {
  return path.lexically_normal().generic_string();
}

[[nodiscard]] bool is_within(const std::filesystem::path& root,
                             const std::filesystem::path& candidate) {
  const auto relative = candidate.lexically_relative(root);
  if (relative.empty()) {
    return candidate == root;
  }
  return *relative.begin() != "..";
}

[[nodiscard]] bool glob_matches(const std::string_view pattern, const std::string_view value) {
  std::vector<bool> previous(value.size() + 1U);
  std::vector<bool> current(value.size() + 1U);
  previous[0] = true;
  for (const auto token : pattern) {
    std::fill(current.begin(), current.end(), false);
    if (token == '*') {
      current[0] = previous[0];
      for (std::size_t index = 1; index <= value.size(); ++index) {
        current[index] = previous[index] || current[index - 1U];
      }
    } else if (token == '?') {
      for (std::size_t index = 1; index <= value.size(); ++index) {
        current[index] = previous[index - 1U];
      }
    } else {
      for (std::size_t index = 1; index <= value.size(); ++index) {
        current[index] = previous[index - 1U] && token == value[index - 1U];
      }
    }
    previous.swap(current);
  }
  return previous[value.size()];
}

[[nodiscard]] bool matches_rule(const IgnoreRule& rule, const std::filesystem::path& relative) {
  const auto path = normalized_path(relative);
  if (rule.directory_only) {
    return path == rule.pattern || path.starts_with(rule.pattern + "/");
  }
  if (rule.contains_slash) {
    return glob_matches(rule.pattern, path);
  }

  for (const auto& component : relative) {
    if (glob_matches(rule.pattern, component.generic_string())) {
      return true;
    }
  }
  return false;
}

[[nodiscard]] std::vector<IgnoreRule> read_ignore_file(const std::filesystem::path& path) {
  std::ifstream input(path);
  std::vector<IgnoreRule> rules;
  std::string line;
  while (std::getline(input, line)) {
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    if (line.empty() || line.front() == '#') {
      continue;
    }

    IgnoreRule rule;
    if (line.front() == '!') {
      rule.negated = true;
      line.erase(0, 1);
    }
    if (line.empty()) {
      continue;
    }
    if (line.front() == '/') {
      line.erase(0, 1);
    }
    rule.directory_only = line.back() == '/';
    if (rule.directory_only) {
      line.pop_back();
    }
    if (line.empty()) {
      continue;
    }
    rule.contains_slash = line.find('/') != std::string::npos;
    rule.pattern = std::move(line);
    rules.push_back(std::move(rule));
  }
  return rules;
}

class GitIgnoreMatcher {
public:
  explicit GitIgnoreMatcher(std::filesystem::path root) : root_(std::move(root)) {}

  [[nodiscard]] bool ignores(const std::filesystem::path& candidate) {
    auto directory = candidate.parent_path();
    std::vector<std::filesystem::path> directories;
    while (is_within(root_, directory)) {
      directories.push_back(directory);
      if (directory == root_) {
        break;
      }
      directory = directory.parent_path();
    }
    std::reverse(directories.begin(), directories.end());

    bool ignored = false;
    for (const auto& current : directories) {
      const auto relative = candidate.lexically_relative(current);
      for (const auto& rule : rules_for(current)) {
        if (matches_rule(rule, relative)) {
          ignored = !rule.negated;
        }
      }
    }
    return ignored;
  }

private:
  [[nodiscard]] const std::vector<IgnoreRule>& rules_for(const std::filesystem::path& directory) {
    const auto [iterator, inserted] = cache_.try_emplace(directory);
    if (inserted) {
      iterator->second = read_ignore_file(directory / ".gitignore");
    }
    return iterator->second;
  }

  std::filesystem::path root_;
  std::unordered_map<std::filesystem::path, std::vector<IgnoreRule>> cache_;
};

[[nodiscard]] bool is_default_excluded_directory(const std::filesystem::path& path) {
  static const std::unordered_set<std::string> names{
      ".git", "build", "out", "third_party", "vendor", "node_modules", ".cache"};
  const auto name = path.filename().string();
  return names.contains(name) || name.starts_with("build-");
}

[[nodiscard]] std::string source_language(const std::filesystem::path& path) {
  if (path.filename() == "CMakeLists.txt" || path.extension() == ".cmake") {
    return "cmake";
  }
  const auto extension = path.extension().string();
  if (extension == ".c") {
    return "c";
  }
  if (extension == ".cc" || extension == ".cpp" || extension == ".cxx" || extension == ".ipp" ||
      extension == ".tpp") {
    return "cpp";
  }
  if (extension == ".h" || extension == ".hh" || extension == ".hpp" || extension == ".hxx") {
    return "cpp-header";
  }
  return {};
}

[[nodiscard]] bool matches_any(const std::vector<std::string>& patterns,
                               const std::filesystem::path& path) {
  const auto value = normalized_path(path);
  return std::ranges::any_of(
      patterns, [&value](const std::string& pattern) { return glob_matches(pattern, value); });
}

} // namespace

FileScanResult FileScanner::scan(const std::filesystem::path& repository_root,
                                 const FileScannerOptions& options) const {
  if (options.max_file_bytes == 0) {
    throw std::invalid_argument("max_file_bytes must be positive");
  }
  std::error_code error;
  const auto root = std::filesystem::weakly_canonical(repository_root, error);
  if (error || !std::filesystem::is_directory(root)) {
    throw std::invalid_argument("repository root must be an accessible directory: " +
                                repository_root.string());
  }

  FileScanResult result;
  GitIgnoreMatcher gitignore(root);
  std::filesystem::recursive_directory_iterator iterator(
      root, std::filesystem::directory_options::skip_permission_denied, error);
  const std::filesystem::recursive_directory_iterator end;
  while (!error && iterator != end) {
    const auto entry = *iterator;
    const auto& entry_path = entry.path();
    std::error_code entry_error;
    const auto symlink = entry.is_symlink(entry_error);
    const auto directory = entry.is_directory(entry_error);
    if (directory) {
      if (symlink || is_default_excluded_directory(entry_path) || gitignore.ignores(entry_path) ||
          matches_any(options.exclude_globs, entry_path.lexically_relative(root))) {
        iterator.disable_recursion_pending();
      }
      iterator.increment(error);
      continue;
    }

    if (!entry.is_regular_file(entry_error)) {
      iterator.increment(error);
      continue;
    }
    ++result.stats.files_seen;
    const auto canonical = std::filesystem::weakly_canonical(entry_path, entry_error);
    const auto relative = entry_path.lexically_relative(root).lexically_normal();
    if (entry_error || !is_within(root, canonical) || gitignore.ignores(entry_path) ||
        matches_any(options.exclude_globs, relative)) {
      ++result.stats.files_skipped;
      iterator.increment(error);
      continue;
    }

    const auto language = source_language(relative);
    const auto included = !language.empty() || matches_any(options.include_globs, relative);
    const auto size = entry.file_size(entry_error);
    if (!included || entry_error || size > options.max_file_bytes) {
      ++result.stats.files_skipped;
      iterator.increment(error);
      continue;
    }
    result.files.push_back({canonical, relative, language.empty() ? "text" : language,
                            static_cast<std::size_t>(size)});
    ++result.stats.files_indexable;
    iterator.increment(error);
  }
  if (error) {
    throw std::runtime_error("failed while scanning repository: " + error.message());
  }

  std::ranges::sort(result.files, [](const ScannedFile& left, const ScannedFile& right) {
    return normalized_path(left.relative_path) < normalized_path(right.relative_path);
  });
  return result;
}

} // namespace llcl::filesystem_adapter
