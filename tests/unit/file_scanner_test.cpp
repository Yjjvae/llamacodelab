#include "adapters/filesystem/file_scanner.hpp"

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace llcl::filesystem_adapter {
namespace {

class TemporaryRepository {
public:
  TemporaryRepository()
      : root_(std::filesystem::path(testing::TempDir()) / "llcl-file-scanner-test") {
    std::filesystem::remove_all(root_);
    std::filesystem::create_directories(root_);
  }

  ~TemporaryRepository() {
    std::filesystem::remove_all(root_);
  }

  void write(const std::filesystem::path& relative_path, const std::string_view content) const {
    const auto path = root_ / relative_path;
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    output << content;
  }

  [[nodiscard]] const std::filesystem::path& root() const {
    return root_;
  }

private:
  std::filesystem::path root_;
};

TEST(FileScannerTest, FindsSupportedFilesAndHonorsIgnoreRules) {
  TemporaryRepository repository;
  repository.write("src/main.cpp", "int main() {}\n");
  repository.write("include/example.hpp", "#pragma once\n");
  repository.write("CMakeLists.txt", "project(example)\n");
  repository.write("ignored.cpp", "int ignored;\n");
  repository.write("nested/.gitignore", "secret.cpp\n");
  repository.write("nested/secret.cpp", "int secret;\n");
  repository.write("build/generated.cpp", "int generated;\n");
  repository.write("third_party/vendor.cpp", "int vendor;\n");
  repository.write("README.md", "not indexable\n");
  repository.write(".gitignore", "ignored.cpp\n");

  const auto result = FileScanner{}.scan(repository.root());

  EXPECT_EQ(result.stats.files_indexable, 3U);
  EXPECT_EQ(result.files.size(), 3U);
  const std::vector<std::filesystem::path> paths{
      result.files[0].relative_path,
      result.files[1].relative_path,
      result.files[2].relative_path,
  };
  EXPECT_EQ(paths, (std::vector<std::filesystem::path>{"CMakeLists.txt", "include/example.hpp",
                                                       "src/main.cpp"}));
  EXPECT_GE(result.stats.files_skipped, 3U);
}

TEST(FileScannerTest, ReturnsNoFilesForAnEmptyDirectory) {
  TemporaryRepository repository;

  const auto result = FileScanner{}.scan(repository.root());

  EXPECT_TRUE(result.files.empty());
  EXPECT_EQ(result.stats.files_seen, 0U);
  EXPECT_EQ(result.stats.files_indexable, 0U);
}

TEST(FileScannerTest, SupportsAdditionalIncludeAndExcludeGlobs) {
  TemporaryRepository repository;
  repository.write("src/main.cpp", "int main() {}\n");
  repository.write("notes/design.txt", "design\n");

  const auto result = FileScanner{}.scan(
      repository.root(), {.include_globs = {"notes/*.txt"}, .exclude_globs = {"src/*"}});

  ASSERT_EQ(result.files.size(), 1U);
  EXPECT_EQ(result.files.front().relative_path, "notes/design.txt");
  EXPECT_EQ(result.files.front().language, "text");
}

TEST(FileScannerTest, SkipsFilesLargerThanConfiguredLimit) {
  TemporaryRepository repository;
  repository.write("small.cpp", "ok");
  repository.write("large.cpp", "this file is too large");

  const auto result = FileScanner{}.scan(
      repository.root(), {.max_file_bytes = 2, .include_globs = {}, .exclude_globs = {}});

  ASSERT_EQ(result.files.size(), 1U);
  EXPECT_EQ(result.files.front().relative_path, "small.cpp");
  EXPECT_EQ(result.stats.files_skipped, 1U);
}

TEST(FileScannerTest, SkipsSymlinkThatEscapesRepositoryRoot) {
  TemporaryRepository repository;
  const auto outside = std::filesystem::path(testing::TempDir()) / "llcl-outside.cpp";
  {
    std::ofstream output(outside);
    output << "int secret;\n";
  }
  std::error_code error;
  std::filesystem::create_symlink(outside, repository.root() / "escape.cpp", error);
  if (error) {
    std::filesystem::remove(outside);
    GTEST_SKIP() << "symlinks are unavailable: " << error.message();
  }

  const auto result = FileScanner{}.scan(repository.root());
  EXPECT_TRUE(result.files.empty());
  EXPECT_EQ(result.stats.files_seen, 1U);
  EXPECT_EQ(result.stats.files_skipped, 1U);
  std::filesystem::remove(outside);
}

} // namespace
} // namespace llcl::filesystem_adapter
