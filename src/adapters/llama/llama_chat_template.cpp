#include "adapters/llama/llama_chat_template.hpp"

#include <limits>
#include <llama.h>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace llcl::llama_adapter {

class LlamaChatTemplate::Impl {
public:
  Impl(const llama_model* model, std::optional<std::string> template_override) {
    const char* value = llama_model_chat_template(model, nullptr);
    if (template_override.has_value()) {
      template_source = std::move(*template_override);
      return;
    }
    if (value == nullptr || *value == '\0') {
      throw std::runtime_error(
          "GGUF model has no supported chat template; configure an explicit template override");
    }
    template_source = value;
  }

  std::string template_source;
};

LlamaChatTemplate::LlamaChatTemplate(const llama_model* model,
                                     std::optional<std::string> template_override)
    : impl_(std::make_unique<Impl>(model, std::move(template_override))) {}

LlamaChatTemplate::~LlamaChatTemplate() = default;
LlamaChatTemplate::LlamaChatTemplate(LlamaChatTemplate&&) noexcept = default;
LlamaChatTemplate& LlamaChatTemplate::operator=(LlamaChatTemplate&&) noexcept = default;

std::string LlamaChatTemplate::format(const std::span<const ChatMessage> messages,
                                      const bool add_assistant_prefix) const {
  if (messages.empty()) {
    throw std::invalid_argument("chat messages must not be empty");
  }

  std::vector<llama_chat_message> native_messages;
  native_messages.reserve(messages.size());
  for (const auto& message : messages) {
    if (message.content.empty()) {
      throw std::invalid_argument("chat message content must not be empty");
    }
    native_messages.push_back({role_name(message.role).data(), message.content.c_str()});
  }

  std::vector<char> buffer(512);
  const auto apply = [&]() {
    return llama_chat_apply_template(impl_->template_source.c_str(), native_messages.data(),
                                     native_messages.size(), add_assistant_prefix, buffer.data(),
                                     static_cast<std::int32_t>(buffer.size()));
  };

  auto size = apply();
  if (size < 0) {
    throw std::runtime_error("llama.cpp failed to apply the GGUF chat template");
  }
  if (static_cast<std::size_t>(size) >= buffer.size()) {
    if (static_cast<std::uint64_t>(size) >= std::numeric_limits<std::int32_t>::max()) {
      throw std::runtime_error("formatted chat prompt is too large");
    }
    buffer.resize(static_cast<std::size_t>(size) + 1U);
    size = apply();
  }
  if (size < 0 || static_cast<std::size_t>(size) >= buffer.size()) {
    throw std::runtime_error("llama.cpp failed to format the complete chat prompt");
  }
  return {buffer.data(), static_cast<std::size_t>(size)};
}

const std::string& LlamaChatTemplate::source() const noexcept {
  return impl_->template_source;
}

} // namespace llcl::llama_adapter
