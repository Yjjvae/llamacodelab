#include "adapters/llama/llama_generator.hpp"
#include "adapters/llama/llama_runtime.hpp"
#include "llamacodelab/domain/generation.hpp"
#include "llamacodelab/support/config.hpp"

#include <gtest/gtest.h>

#include <cstdlib>
#include <string>

namespace llcl::llama_adapter {
namespace {

TEST(LlamaSmokeTest, GeneratesWithConfiguredModel) {
  const char* model_path = std::getenv("LLCL_TEST_MODEL");
  if (model_path == nullptr || std::string(model_path).empty()) {
    GTEST_SKIP() << "set LLCL_TEST_MODEL to run the real GGUF smoke test";
  }

  LlamaRuntime runtime;
  ModelConfig config;
  config.path = model_path;
  config.context_size = 512;
  config.batch_size = 128;
  config.gpu_layers = 0;
  config.flash_attention = false;
  LlamaGenerator generator(runtime, config);

  std::string output;
  GenerationOptions options;
  options.max_tokens = 8;
  options.temperature = 0.0F;
  const auto stats = generator.generate(
      "C++ RAII means", options, [&output](std::string_view piece) { output.append(piece); }, {});

  EXPECT_GT(stats.prompt_tokens, 0U);
  EXPECT_LE(stats.generated_tokens, 8U);
  EXPECT_FALSE(generator.model_description().empty());
}

}  // namespace
}  // namespace llcl::llama_adapter
