#pragma once

#include "adapters/llama/llama_runtime.hpp"
#include "llamacodelab/domain/chat.hpp"
#include "llamacodelab/domain/generation.hpp"
#include "llamacodelab/support/config.hpp"

#include <chrono>
#include <memory>
#include <span>
#include <string>
#include <string_view>

namespace llcl::llama_adapter {

class LlamaGenerator final : public ITextGenerator, public IChatFormatter {
public:
  LlamaGenerator(LlamaRuntime& runtime, ModelConfig config);
  ~LlamaGenerator() override;

  LlamaGenerator(const LlamaGenerator&) = delete;
  LlamaGenerator& operator=(const LlamaGenerator&) = delete;
  LlamaGenerator(LlamaGenerator&&) = delete;
  LlamaGenerator& operator=(LlamaGenerator&&) = delete;

  [[nodiscard]] std::size_t count_tokens(std::string_view text) const override;

  GenerationStats generate(std::string_view prompt, const GenerationOptions& options,
                           const TokenCallback& on_token, std::stop_token stop_token) override;

  [[nodiscard]] std::string format_chat(std::span<const ChatMessage> messages,
                                        bool add_assistant_prefix = true) const;
  [[nodiscard]] std::string format(std::span<const ChatMessage> messages,
                                   bool add_assistant_prefix) const override;
  GenerationStats generate_chat(std::span<const ChatMessage> messages,
                                const GenerationOptions& options, const TokenCallback& on_token,
                                std::stop_token stop_token);

  [[nodiscard]] const std::string& model_description() const noexcept;
  [[nodiscard]] std::chrono::milliseconds model_load_time() const noexcept;

private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace llcl::llama_adapter
