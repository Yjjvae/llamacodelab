#pragma once

#include "llamacodelab/domain/chat.hpp"

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace llcl {

enum class ChatGenerationState {
  idle,
  prefill,
  decoding,
  completed,
  cancelled,
  failed,
};

class ChatSession {
public:
  void add(Role role, std::string content);
  void begin_prefill();
  void begin_decoding();
  void complete(std::string assistant_content);
  void cancel();
  void fail();

  [[nodiscard]] ChatGenerationState state() const noexcept;
  [[nodiscard]] std::span<const ChatMessage> messages() const noexcept;

private:
  std::vector<ChatMessage> messages_;
  ChatGenerationState state_{ChatGenerationState::idle};
};

} // namespace llcl
