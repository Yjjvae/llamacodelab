#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <stop_token>
#include <string_view>

namespace llcl {

struct GenerationOptions {
  std::int32_t max_tokens{512};
  float temperature{0.2F};
  float top_p{0.9F};
  std::uint32_t seed{42};
};

struct GenerationStats {
  std::uint64_t prompt_tokens{};
  std::uint64_t generated_tokens{};
  std::chrono::milliseconds time_to_first_token{};
  double prompt_tokens_per_second{};
  double decode_tokens_per_second{};
};

using TokenCallback = std::function<void(std::string_view)>;

class ITokenCounter {
 public:
  virtual ~ITokenCounter() = default;
  [[nodiscard]] virtual std::size_t count_tokens(std::string_view text) const = 0;
};

class ITextGenerator : public ITokenCounter {
 public:
  ~ITextGenerator() override = default;

  virtual GenerationStats generate(
      std::string_view prompt,
      const GenerationOptions& options,
      const TokenCallback& on_token,
      std::stop_token stop_token) = 0;
};

}  // namespace llcl
