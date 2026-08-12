#include "llamacodelab/support/hash.hpp"

#include <cstddef>

namespace llcl {

std::uint64_t stable_hash64(const std::string_view value) noexcept {
  constexpr std::uint64_t offset_basis = 14695981039346656037ULL;
  constexpr std::uint64_t prime = 1099511628211ULL;

  auto result = offset_basis;
  for (const auto character : value) {
    result ^= static_cast<unsigned char>(character);
    result *= prime;
  }
  return result;
}

std::string stable_hash_hex(const std::string_view value) {
  constexpr std::string_view digits{"0123456789abcdef"};
  const auto hash = stable_hash64(value);
  std::string result(16, '0');
  for (std::size_t index = 0; index < result.size(); ++index) {
    const auto shift = static_cast<unsigned>((result.size() - 1U - index) * 4U);
    result[index] = digits[(hash >> shift) & 0xFU];
  }
  return result;
}

} // namespace llcl
