#pragma once

#include "llamacodelab/domain/chat.hpp"
#include "llamacodelab/domain/generation.hpp"

#include <cstddef>
#include <span>
#include <string>
#include <vector>

namespace llcl {

struct BuiltPrompt {
  std::string text;
  std::vector<ChatMessage> retained_messages;
  std::size_t token_count{};
  std::size_t discarded_messages{};
};

class PromptBuilder {
public:
  [[nodiscard]] BuiltPrompt build(std::span<const ChatMessage> messages,
                                  const IChatFormatter& formatter,
                                  const ITokenCounter& token_counter,
                                  std::size_t max_prompt_tokens) const;
};

} // namespace llcl
