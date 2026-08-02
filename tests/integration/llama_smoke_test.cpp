#include "adapters/llama/llama_generator.hpp"
#include "adapters/llama/llama_runtime.hpp"
#include "llamacodelab/domain/generation.hpp"
#include "llamacodelab/support/config.hpp"

#include <cstdlib>
#include <gtest/gtest.h>
#include <stdexcept>
#include <string>

namespace llcl::llama_adapter {
namespace {

int env_int(const char* name, const int fallback) {
  const char* value = std::getenv(name);
  if (value == nullptr || std::string(value).empty()) {
    return fallback;
  }

  try {
    return std::stoi(value);
  } catch (const std::exception&) {
    throw std::runtime_error(std::string(name) + " must be an integer");
  }
}

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
  config.gpu_layers = env_int("LLCL_TEST_GPU_LAYERS", 0);
  config.flash_attention = false;
  LlamaGenerator generator(runtime, config);

  GenerationOptions options;
  options.max_tokens = 8;
  options.temperature = 0.0F;
  const int repeats = env_int("LLCL_TEST_REPEAT", 1);
  ASSERT_GT(repeats, 0);

  for (int run = 0; run < repeats; ++run) {
    std::string output;
    const auto stats = generator.generate(
        "C++ RAII means", options, [&output](std::string_view piece) { output.append(piece); }, {});

    EXPECT_GT(stats.prompt_tokens, 0U) << "run " << run;
    EXPECT_LE(stats.generated_tokens, 8U) << "run " << run;
  }
  EXPECT_FALSE(generator.model_description().empty());
}

} // namespace
} // namespace llcl::llama_adapter
