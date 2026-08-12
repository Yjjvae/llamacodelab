#include "llamacodelab/application/prompt_builder.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace llcl {

BuiltPrompt PromptBuilder::build(std::span<const ChatMessage> messages,
                                 const IChatFormatter& formatter,
                                 const ITokenCounter& token_counter,
                                 const std::size_t max_prompt_tokens) const {
  if (messages.empty()) {
    throw std::invalid_argument("chat messages must not be empty");
  }
  if (max_prompt_tokens == 0) {
    throw std::invalid_argument("max_prompt_tokens must be positive");
  }

  std::vector<ChatMessage> retained(messages.begin(), messages.end());
  std::size_t discarded = 0;
  while (true) {
    auto text = formatter.format(retained, true);
    const auto token_count = token_counter.count_tokens(text);
    if (token_count <= max_prompt_tokens) {
      return {
          .text = std::move(text),
          .retained_messages = std::move(retained),
          .token_count = token_count,
          .discarded_messages = discarded,
      };
    }

    const auto candidate =
        std::find_if(retained.begin(), retained.end(),
                     [](const ChatMessage& message) { return message.role != Role::system; });
    if (candidate == retained.end()) {
      throw std::invalid_argument("system prompt exceeds the configured chat prompt token budget");
    }
    if (retained.size() == 1) {
      throw std::invalid_argument("no chat messages fit within the configured prompt token budget");
    }
    retained.erase(candidate);
    ++discarded;
  }
}

} // namespace llcl
