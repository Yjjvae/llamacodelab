#pragma once

#include "llamacodelab/domain/chat.hpp"

#include <memory>
#include <optional>
#include <string>

struct llama_model;

namespace llcl::llama_adapter {

class LlamaChatTemplate final : public IChatFormatter {
public:
  LlamaChatTemplate(const llama_model* model, std::optional<std::string> template_override);
  ~LlamaChatTemplate() override;

  LlamaChatTemplate(const LlamaChatTemplate&) = delete;
  LlamaChatTemplate& operator=(const LlamaChatTemplate&) = delete;
  LlamaChatTemplate(LlamaChatTemplate&&) noexcept;
  LlamaChatTemplate& operator=(LlamaChatTemplate&&) noexcept;

  [[nodiscard]] std::string format(std::span<const ChatMessage> messages,
                                   bool add_assistant_prefix) const override;
  [[nodiscard]] const std::string& source() const noexcept;

private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace llcl::llama_adapter
