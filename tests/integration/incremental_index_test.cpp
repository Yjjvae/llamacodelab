#include "adapters/vector/brute_force_index.hpp"
#include "adapters/vector/hnsw_index.hpp"
#include "llamacodelab/application/index_service.hpp"

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>

namespace llcl::test {
namespace {

class CountingEmbedder final : public IEmbedder {
public:
  std::size_t embedded{};
  [[nodiscard]] Embedding embed(std::string_view, EmbeddingKind) override {
    return {1.0F, 0.0F};
  }
  [[nodiscard]] std::vector<Embedding> embed_batch(std::span<const std::string_view> texts,
                                                   EmbeddingKind) override {
    embedded += texts.size();
    return std::vector<Embedding>(texts.size(), {1.0F, 0.0F});
  }
  [[nodiscard]] std::size_t dimension() const noexcept override {
    return 2;
  }
};

class TempDirectory {
public:
  TempDirectory() : path(std::filesystem::temp_directory_path() / "llcl-incremental-index-test") {
    std::filesystem::remove_all(path);
    std::filesystem::create_directories(path);
  }
  ~TempDirectory() {
    std::filesystem::remove_all(path);
  }
  std::filesystem::path path;
};

void write_file(const std::filesystem::path& path, const std::string_view content) {
  std::ofstream output(path);
  output << content;
}

} // namespace

TEST(IncrementalIndexTest, ReusesUnchangedEmbeddingsAndRemovesDeletedDocuments) {
  TempDirectory temporary;
  const auto repository = temporary.path / "repo";
  std::filesystem::create_directories(repository);
  write_file(repository / "one.cpp", "int one() { return 1; }\n");
  write_file(repository / "two.cpp", "int two() { return 2; }\n");
  CountingEmbedder embedder;
  SearchIndexHandle handle;
  IndexConfig config;
  config.data_dir = temporary.path / "index";
  config.chunk_lines = 20;
  config.overlap_lines = 1;
  IndexService service(embedder, handle, config, "test-embedding-v1");

  const auto initial = service.update(repository);
  EXPECT_EQ(initial.files_added, 2U);
  EXPECT_EQ(embedder.embedded, 2U);
  ASSERT_NE(handle.load(), nullptr);
  EXPECT_EQ(handle.load()->size(), 2U);

  embedder.embedded = 0;
  const auto unchanged = service.update(repository);
  EXPECT_EQ(unchanged.files_unchanged, 2U);
  EXPECT_EQ(embedder.embedded, 0U);

  write_file(repository / "one.cpp", "int one() { return 42; }\n");
  const auto changed = service.update(repository);
  EXPECT_EQ(changed.files_changed, 1U);
  EXPECT_EQ(embedder.embedded, 1U);

  std::filesystem::remove(repository / "two.cpp");
  const auto removed = service.update(repository);
  EXPECT_EQ(removed.files_removed, 1U);
  EXPECT_EQ(handle.load()->size(), 1U);
}

TEST(IncrementalIndexTest, RejectsAnIndexBuiltForAnotherEmbeddingModel) {
  TempDirectory temporary;
  const auto repository = temporary.path / "repo";
  std::filesystem::create_directories(repository);
  write_file(repository / "one.cpp", "int one() { return 1; }\n");
  CountingEmbedder embedder;
  SearchIndexHandle handle;
  IndexConfig config;
  config.data_dir = temporary.path / "index";
  IndexService first(embedder, handle, config, "embedding-v1");
  (void)first.update(repository);
  IndexService incompatible(embedder, handle, config, "embedding-v2");
  EXPECT_THROW((void)incompatible.update(repository), std::runtime_error);
}

TEST(IncrementalIndexTest, SelectsConfiguredHnswOrBruteForceSnapshot) {
  TempDirectory temporary;
  const auto repository = temporary.path / "repo";
  std::filesystem::create_directories(repository);
  write_file(repository / "one.cpp", "int one() { return 1; }\n");
  CountingEmbedder embedder;
  SearchIndexHandle handle;
  IndexConfig config;
  config.data_dir = temporary.path / "index";
  config.hnsw_enabled = true;
  IndexService hnsw_service(embedder, handle, config, "test-embedding-v1");
  (void)hnsw_service.update(repository);
  EXPECT_NE(dynamic_cast<const vector_adapter::HnswIndex*>(handle.load().get()), nullptr);

  config.hnsw_enabled = false;
  IndexService brute_service(embedder, handle, config, "test-embedding-v1");
  (void)brute_service.update(repository);
  EXPECT_NE(dynamic_cast<const vector_adapter::BruteForceIndex*>(handle.load().get()), nullptr);
}

} // namespace llcl::test
