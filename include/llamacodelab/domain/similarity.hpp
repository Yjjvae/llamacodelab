#pragma once

#include <span>

namespace llcl {

void l2_normalize(std::span<float> values);
[[nodiscard]] float dot_product(std::span<const float> lhs, std::span<const float> rhs);
[[nodiscard]] bool is_finite_embedding(std::span<const float> values) noexcept;

} // namespace llcl
