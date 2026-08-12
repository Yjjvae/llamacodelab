#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace llcl {

[[nodiscard]] std::uint64_t stable_hash64(std::string_view value) noexcept;
[[nodiscard]] std::string stable_hash_hex(std::string_view value);

} // namespace llcl
