#include "llamacodelab/support/config.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
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

  ~TemporaryConfig() { std::filesystem::remove(path_); }

  TemporaryConfig(const TemporaryConfig&) = delete;
  TemporaryConfig& operator=(const TemporaryConfig&) = delete;

  [[nodiscard]] const std::filesystem::path& path() const { return path_; }

 private:
  std::filesystem::path path_;
};

TEST(ConfigTest, LoadsCompleteJson) {
  TemporaryConfig file(
      "llcl-complete.json",
      R"({
        "generation_model": {
          "path": "models/generation.gguf",
          "context_size": 8192,
          "batch_size": 256,
          "gpu_layers": 32,
          "flash_attention": false
        },
        "embedding_model": {"path": "models/embedding.gguf"},
        "index": {
          "data_dir": "var/test-index",
          "chunk_lines": 100,
          "overlap_lines": 20,
          "top_k": 12
        },
        "log_level": "debug"
      })");

  const auto config = load_config(file.path());

  EXPECT_EQ(config.generation_model.path, "models/generation.gguf");
  EXPECT_EQ(config.generation_model.context_size, 8192U);
  EXPECT_EQ(config.generation_model.batch_size, 256U);
  EXPECT_EQ(config.generation_model.gpu_layers, 32);
  EXPECT_FALSE(config.generation_model.flash_attention);
  EXPECT_EQ(config.embedding_model.path, "models/embedding.gguf");
  EXPECT_EQ(config.index.top_k, 12U);
  EXPECT_EQ(config.log_level, "debug");
}

TEST(ConfigTest, UsesDefaultsForOptionalFields) {
  TemporaryConfig file(
      "llcl-defaults.json", R"({"generation_model":{"path":"model.gguf"}})");

  const auto config = load_config(file.path());

  EXPECT_EQ(config.generation_model.context_size, 4096U);
  EXPECT_EQ(config.generation_model.batch_size, 512U);
  EXPECT_EQ(config.embedding_model.path, "model.gguf");
  EXPECT_EQ(config.index.chunk_lines, 80U);
  EXPECT_EQ(config.log_level, "info");
}

TEST(ConfigTest, RejectsMissingModelPath) {
  TemporaryConfig file("llcl-no-model.json", R"({"generation_model":{}})");
  EXPECT_THROW(static_cast<void>(load_config(file.path())), std::invalid_argument);
}

TEST(ConfigTest, RejectsOverlapNotSmallerThanChunkSize) {
  TemporaryConfig file(
      "llcl-overlap.json",
      R"({
        "generation_model":{"path":"model.gguf"},
        "index":{"chunk_lines":32,"overlap_lines":32}
      })");
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

}  // namespace
}  // namespace llcl
