#include "llamacodelab/support/config.hpp"

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <stdexcept>
#include <string>
#include <string_view>

namespace llcl {
namespace {

class TemporaryConfig {
public:
  TemporaryConfig(std::string_view filename, std::string_view content)
      : path_(std::filesystem::path(testing::TempDir()) / filename) {
    std::ofstream output(path_);
    output << content;
  }

  ~TemporaryConfig() {
    std::filesystem::remove(path_);
  }

  TemporaryConfig(const TemporaryConfig&) = delete;
  TemporaryConfig& operator=(const TemporaryConfig&) = delete;

  [[nodiscard]] const std::filesystem::path& path() const {
    return path_;
  }

private:
  std::filesystem::path path_;
};

TEST(ConfigTest, LoadsCompleteJson) {
  TemporaryConfig file("llcl-complete.json",
                       R"({
        "generation_model": {
          "path": "models/generation.gguf",
          "context_size": 8192,
          "batch_size": 256,
          "gpu_layers": 32,
          "flash_attention": false,
          "chat_template": "chatml"
        },
        "embedding_model": {"path": "models/embedding.gguf"},
        "index": {
          "data_dir": "var/test-index",
          "chunk_lines": 100,
          "overlap_lines": 20,
          "max_file_bytes": 2048,
          "top_k": 12,
          "hnsw_enabled": true,
          "hnsw_ef_search": 128,
          "reranker_enabled": true,
          "rerank_candidates": 24,
          "semantic_index_enabled": true,
          "compilation_database_dir": "out/compile-db"
        },
        "log_level": "debug"
      })");

  const auto config = load_config(file.path());

  EXPECT_EQ(config.generation_model.path, "models/generation.gguf");
  EXPECT_EQ(config.generation_model.context_size, 8192U);
  EXPECT_EQ(config.generation_model.batch_size, 256U);
  EXPECT_EQ(config.generation_model.gpu_layers, 32);
  EXPECT_FALSE(config.generation_model.flash_attention);
  ASSERT_TRUE(config.generation_model.chat_template.has_value());
  EXPECT_EQ(*config.generation_model.chat_template, "chatml");
  EXPECT_EQ(config.embedding_model.path, "models/embedding.gguf");
  EXPECT_EQ(config.index.max_file_bytes, 2048U);
  EXPECT_EQ(config.index.top_k, 12U);
  EXPECT_TRUE(config.index.hnsw_enabled);
  EXPECT_EQ(config.index.hnsw_ef_search, 128U);
  EXPECT_TRUE(config.index.reranker_enabled);
  EXPECT_EQ(config.index.rerank_candidates, 24U);
  EXPECT_TRUE(config.index.semantic_index_enabled);
  EXPECT_EQ(config.index.compilation_database_dir, "out/compile-db");
  EXPECT_EQ(config.log_level, "debug");
}

TEST(ConfigTest, UsesDefaultsForOptionalFields) {
  TemporaryConfig file("llcl-defaults.json", R"({"generation_model":{"path":"model.gguf"}})");

  const auto config = load_config(file.path());

  EXPECT_EQ(config.generation_model.context_size, 4096U);
  EXPECT_EQ(config.generation_model.batch_size, 512U);
  EXPECT_EQ(config.embedding_model.path, "model.gguf");
  EXPECT_EQ(config.index.chunk_lines, 80U);
  EXPECT_FALSE(config.index.semantic_index_enabled);
  EXPECT_EQ(config.index.compilation_database_dir, "build");
  EXPECT_EQ(config.log_level, "info");
}

TEST(ConfigTest, RejectsMissingModelPath) {
  TemporaryConfig file("llcl-no-model.json", R"({"generation_model":{}})");
  EXPECT_THROW(static_cast<void>(load_config(file.path())), std::invalid_argument);
}

TEST(ConfigTest, RejectsOverlapNotSmallerThanChunkSize) {
  TemporaryConfig file("llcl-overlap.json",
                       R"({
        "generation_model":{"path":"model.gguf"},
        "index":{"chunk_lines":32,"overlap_lines":32}
      })");
  EXPECT_THROW(static_cast<void>(load_config(file.path())), std::invalid_argument);
}

TEST(ConfigTest, RejectsInvalidMaximumFileSize) {
  TemporaryConfig file(
      "llcl-file-size.json",
      R"({"generation_model":{"path":"model.gguf"},"index":{"max_file_bytes":0}})");
  EXPECT_THROW(static_cast<void>(load_config(file.path())), std::invalid_argument);
}

TEST(ConfigTest, RejectsRerankCandidatesOutsideConfiguredWindow) {
  TemporaryConfig file(
      "llcl-rerank-window.json",
      R"({"generation_model":{"path":"model.gguf"},"index":{"rerank_candidates":19}})");
  EXPECT_THROW(static_cast<void>(load_config(file.path())), std::invalid_argument);
}

TEST(ConfigTest, ParseErrorContainsPathAndByte) {
  TemporaryConfig file("llcl-invalid.json", R"({"generation_model": [})");

  try {
    static_cast<void>(load_config(file.path()));
    FAIL() << "expected a parse error";
  } catch (const std::runtime_error& error) {
    const std::string message = error.what();
    EXPECT_NE(message.find(file.path().string()), std::string::npos);
    EXPECT_NE(message.find("byte"), std::string::npos);
  }
}

} // namespace
} // namespace llcl
