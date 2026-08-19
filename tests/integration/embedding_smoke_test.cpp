#include "adapters/filesystem/file_scanner.hpp"
#include "adapters/filesystem/text_chunker.hpp"
#include "adapters/llama/llama_embedder.hpp"
#include "adapters/llama/llama_runtime.hpp"
#include "adapters/vector/brute_force_index.hpp"
#include "llamacodelab/domain/similarity.hpp"
#include "llamacodelab/support/config.hpp"

#include <cstdlib>
#include <filesystem>
#include <gtest/gtest.h>
#include <string>
#include <vector>

namespace llcl::test {

TEST(EmbeddingSmokeTest, ProducesNormalizedStableVectorsAndSearchesThem) {
  const auto* model_path = std::getenv("LLCL_TEST_EMBEDDING_MODEL");
  if (model_path == nullptr || !std::filesystem::is_regular_file(model_path)) {
    GTEST_SKIP() << "set LLCL_TEST_EMBEDDING_MODEL to a local embedding GGUF";
  }
  llama_adapter::LlamaRuntime runtime;
  ModelConfig config;
  config.path = model_path;
  config.context_size = 2048;
  config.batch_size = 512;
  config.gpu_layers = std::getenv("LLCL_TEST_GPU_LAYERS") == nullptr
                          ? 0
                          : std::stoi(std::getenv("LLCL_TEST_GPU_LAYERS"));
  llama_adapter::LlamaEmbedder embedder(runtime, config);
  const auto query =
      embedder.embed("where is the connection pool implemented?", EmbeddingKind::query);
  const auto repeated =
      embedder.embed("where is the connection pool implemented?", EmbeddingKind::query);
  ASSERT_EQ(query.size(), embedder.dimension());
  EXPECT_NEAR(dot_product(query, query), 1.0F, 1e-4F);
  EXPECT_EQ(query, repeated);

  const std::vector<std::string_view> documents{
      "class ConnectionPool manages reusable database connections.",
      "The command line parser reads JSON configuration files.",
  };
  const auto values = embedder.embed_batch(documents, EmbeddingKind::document);
  ASSERT_EQ(values.size(), documents.size());
  vector_adapter::BruteForceIndex index(embedder.dimension());
  index.upsert(1, values[0]);
  index.upsert(2, values[1]);
  const auto hits = index.search(query, 2);
  ASSERT_EQ(hits.size(), 2U);
  EXPECT_EQ(hits.front().chunk_id, 1U);
}

TEST(EmbeddingSmokeTest, RetrievesTheGroundTruthFixture) {
  const auto* model_path = std::getenv("LLCL_TEST_EMBEDDING_MODEL");
  if (model_path == nullptr || !std::filesystem::is_regular_file(model_path)) {
    GTEST_SKIP() << "set LLCL_TEST_EMBEDDING_MODEL to a local embedding GGUF";
  }
  const auto fixture =
      std::filesystem::path{PROJECT_SOURCE_DIR} / "tests/fixtures/retrieval_sample";
  const auto scanned = filesystem_adapter::FileScanner{}.scan(fixture);
  ASSERT_EQ(scanned.files.size(), 1U);
  const auto chunks = filesystem_adapter::TextChunker{}.chunk_file(scanned.files.front());
  ASSERT_FALSE(chunks.empty());

  llama_adapter::LlamaRuntime runtime;
  ModelConfig config;
  config.path = model_path;
  config.context_size = 2048;
  config.batch_size = 512;
  llama_adapter::LlamaEmbedder embedder(runtime, config);
  std::vector<std::string_view> documents;
  documents.reserve(chunks.size());
  for (const auto& chunk : chunks) {
    documents.push_back(chunk.content);
  }
  const auto embeddings = embedder.embed_batch(documents, EmbeddingKind::document);
  vector_adapter::BruteForceIndex index(embedder.dimension());
  for (std::size_t i = 0; i < chunks.size(); ++i) {
    index.upsert(chunks[i].id, embeddings[i]);
  }
  const auto query =
      embedder.embed("Where is the connection pool implemented?", EmbeddingKind::query);
  const auto hits = index.search(query, 1);
  ASSERT_EQ(hits.size(), 1U);
  EXPECT_EQ(hits.front().chunk_id, chunks.front().id);
}

} // namespace llcl::test
