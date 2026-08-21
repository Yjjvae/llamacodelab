#include "adapters/sqlite/fts_search.hpp"
#include "adapters/sqlite/sqlite_chunk_repository.hpp"
#include "llamacodelab/application/hybrid_retriever.hpp"

#include <filesystem>
#include <gtest/gtest.h>
#include <stdexcept>

namespace llcl::test {
namespace {

class TempDirectory {
public:
  TempDirectory() : path(std::filesystem::temp_directory_path() / "llcl-hybrid-retrieval-test") {
    std::filesystem::remove_all(path);
    std::filesystem::create_directories(path);
  }
  ~TempDirectory() {
    std::filesystem::remove_all(path);
  }
  std::filesystem::path path;
};

class FakeEmbedder final : public IEmbedder {
public:
  [[nodiscard]] Embedding embed(std::string_view, EmbeddingKind) override {
    return {1.0F, 0.0F};
  }
  [[nodiscard]] std::vector<Embedding> embed_batch(std::span<const std::string_view>,
                                                   EmbeddingKind) override {
    return {};
  }
  [[nodiscard]] std::size_t dimension() const noexcept override {
    return 2;
  }
};

class FakeVectorIndex final : public IVectorIndex {
public:
  void upsert(ChunkId, std::span<const float>) override {}
  void erase(ChunkId) override {}
  [[nodiscard]] std::vector<SearchHit> search(std::span<const float>, std::size_t) const override {
    return {{.chunk_id = 11, .score = 0.9F}, {.chunk_id = 12, .score = 0.8F}};
  }
  [[nodiscard]] std::size_t size() const noexcept override {
    return 2;
  }
  [[nodiscard]] std::size_t dimension() const noexcept override {
    return 2;
  }
};

class FakeKeywordSearch final : public IKeywordSearcher {
public:
  [[nodiscard]] std::vector<SearchHit> search(std::string_view, std::size_t) const override {
    return {{.chunk_id = 12, .score = 10.0F}, {.chunk_id = 13, .score = 9.0F}};
  }
};

} // namespace

TEST(HybridRetrievalTest, FindsExactSymbolsWithFts5) {
  TempDirectory temporary;
  const auto database_path = temporary.path / "index.sqlite3";
  sqlite_adapter::SqliteChunkRepository repository(database_path);
  repository.begin();
  repository.replace_document(
      {.relative_path = "src/pool.cpp",
       .content_hash = "pool",
       .size_bytes = 32,
       .modified_ns = 1,
       .parser_version = "test",
       .chunks = {{.id = 12,
                   .source = {.path = "src/pool.cpp", .start_line = 1, .end_line = 3},
                   .language = "cpp",
                   .content = "class ConnectionPool { };",
                   .content_hash = "chunk-pool",
                   .chunker_version = "test"}}},
      1);
  repository.replace_document(
      {.relative_path = "src/other.cpp",
       .content_hash = "other",
       .size_bytes = 16,
       .modified_ns = 1,
       .parser_version = "test",
       .chunks = {{.id = 11,
                   .source = {.path = "src/other.cpp", .start_line = 1, .end_line = 2},
                   .language = "cpp",
                   .content = "void make_pool();",
                   .content_hash = "chunk-other",
                   .chunker_version = "test"}}},
      1);
  repository.commit();

  sqlite_adapter::FtsSearch search(database_path);
  const auto hits = search.search("ConnectionPool", 5);

  ASSERT_FALSE(hits.empty());
  EXPECT_EQ(hits.front().chunk_id, 12U);

  repository.erase_document("src/pool.cpp");
  EXPECT_TRUE(search.search("ConnectionPool", 5).empty());
}

TEST(HybridRetrievalTest, PromotesExactKeywordCandidateWithRrf) {
  FakeEmbedder embedder;
  FakeVectorIndex vector_index;
  FakeKeywordSearch keyword_search;
  HybridRetriever retriever(embedder, vector_index, keyword_search);

  const auto hits = retriever.retrieve("ConnectionPool", 3);

  ASSERT_EQ(hits.size(), 3U);
  EXPECT_EQ(hits.front().chunk_id, 12U);
}

} // namespace llcl::test
