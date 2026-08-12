#include "adapters/llama/llama_generator.hpp"
#include "adapters/llama/llama_runtime.hpp"
#include "llamacodelab/domain/chat.hpp"
#include "llamacodelab/domain/generation.hpp"
#include "llamacodelab/support/config.hpp"

#include <chrono>
#include <cstdlib>
#include <gtest/gtest.h>
#include <stop_token>
#include <string>
#include <vector>

namespace llcl::llama_adapter {
namespace {

TEST(StreamCancelTest, StopsAfterFirstStreamedToken) {
  const char* model_path = std::getenv("LLCL_TEST_MODEL");
  if (model_path == nullptr || std::string(model_path).empty()) {
    GTEST_SKIP() << "set LLCL_TEST_MODEL to run the real GGUF cancellation test";
  }

  LlamaRuntime runtime;
  ModelConfig config;
  config.path = model_path;
  config.context_size = 512;
  config.batch_size = 128;
  config.gpu_layers = -1;
  LlamaGenerator generator(runtime, config);
  const std::vector<ChatMessage> messages{{Role::user, "Explain C++ RAII."}};

  std::stop_source stop_source;
  std::chrono::steady_clock::time_point stop_requested_at{};
  const auto stats = generator.generate_chat(
      messages, {.max_tokens = 128, .temperature = 0.0F},
      [&stop_source, &stop_requested_at](const std::string_view) {
        stop_requested_at = std::chrono::steady_clock::now();
        stop_source.request_stop();
      },
      stop_source.get_token());
  const auto returned_at = std::chrono::steady_clock::now();

  ASSERT_NE(stop_requested_at, std::chrono::steady_clock::time_point{});
  EXPECT_LT(returned_at - stop_requested_at, std::chrono::milliseconds(500));
  EXPECT_LE(stats.generated_tokens, 1U);
}

} // namespace
} // namespace llcl::llama_adapter
