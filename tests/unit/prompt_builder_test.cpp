#include "llamacodelab/application/prompt_builder.hpp"

#include <cstddef>
#include <gtest/gtest.h>
#include <string>
#include <string_view>

namespace llcl {
namespace {

class PipeFormatter final : public IChatFormatter {
public:
  [[nodiscard]] std::string format(const std::span<const ChatMessage> messages,
                                   const bool add_assistant_prefix) const override {
    std::string result;
    for (const auto& message : messages) {
      result += std::string(role_name(message.role));
      result += ':';
      result += message.content;
      result += '|';
    }
    if (add_assistant_prefix) {
      result += "assistant:";
    }
    return result;
  }
};

class CharacterCounter final : public ITokenCounter {
public:
  [[nodiscard]] std::size_t count_tokens(const std::string_view text) const override {
    return text.size();
  }
};

TEST(PromptBuilderTest, PreservesSystemMessageAndTrimsOldestConversationMessages) {
  const std::vector<ChatMessage> messages{
      {Role::system, "s"},
      {Role::user, "first"},
      {Role::assistant, "first-answer"},
      {Role::user, "latest"},
  };
  PromptBuilder builder;
  PipeFormatter formatter;
  CharacterCounter counter;

  const auto built = builder.build(messages, formatter, counter, 32);

  ASSERT_EQ(built.retained_messages.size(), 2U);
  EXPECT_EQ(built.retained_messages[0].role, Role::system);
  EXPECT_EQ(built.retained_messages[1].content, "latest");
  EXPECT_EQ(built.discarded_messages, 2U);
  EXPECT_LE(built.token_count, 32U);
}

TEST(PromptBuilderTest, RejectsSystemPromptThatCannotFit) {
  const std::vector<ChatMessage> messages{{Role::system, "this system prompt is too long"}};
  PromptBuilder builder;
  PipeFormatter formatter;
  CharacterCounter counter;

  EXPECT_THROW(static_cast<void>(builder.build(messages, formatter, counter, 4)),
               std::invalid_argument);
}

TEST(PromptBuilderTest, RejectsConversationWhenNoMessageCanFit) {
  const std::vector<ChatMessage> messages{{Role::user, "too long"}};
  const PromptBuilder builder;

  EXPECT_THROW(
      {
        [[maybe_unused]] const auto built =
            builder.build(messages, PipeFormatter{}, CharacterCounter{}, 1);
      },
      std::invalid_argument);
}

} // namespace
} // namespace llcl
