#pragma once

#include <span>
#include <string>
#include <string_view>

namespace llcl {

enum class Role {
  system,
  user,
  assistant,
};

struct ChatMessage {
  Role role;
  std::string content;
};

[[nodiscard]] constexpr std::string_view role_name(Role role) noexcept {
  switch (role) {
  case Role::system:
    return "system";
  case Role::user:
    return "user";
  case Role::assistant:
    return "assistant";
  }
  return "unknown";
}

class IChatFormatter {
public:
  virtual ~IChatFormatter() = default;

  [[nodiscard]] virtual std::string format(std::span<const ChatMessage> messages,
                                           bool add_assistant_prefix) const = 0;
};

} // namespace llcl
