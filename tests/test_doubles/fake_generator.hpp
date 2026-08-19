#pragma once

#include "llamacodelab/domain/generation.hpp"

#include <cstddef>
#include <stop_token>
#include <string>
#include <string_view>

namespace llcl::test {

class FakeGenerator final : public ITextGenerator {
public:
  std::string response{"fake answer"};
  std::string last_prompt;

  [[nodiscard]] std::size_t count_tokens(std::string_view text) const override {
    return text.empty() ? 0U : 1U;
  }

  GenerationStats generate(std::string_view prompt, const GenerationOptions& /* options */,
                           const TokenCallback& on_token, std::stop_token stop_token) override {
    last_prompt = prompt;
    if (stop_token.stop_requested()) {
      return {};
    }
    on_token(response);
    return {.generated_tokens = 2};
  }
};

} // namespace llcl::test
