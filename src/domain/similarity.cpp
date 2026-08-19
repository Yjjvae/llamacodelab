#include "llamacodelab/domain/similarity.hpp"

#include <cmath>
#include <numeric>
#include <stdexcept>

namespace llcl {

bool is_finite_embedding(const std::span<const float> values) noexcept {
  for (const float value : values) {
    if (!std::isfinite(value)) {
      return false;
    }
  }
  return true;
}

void l2_normalize(const std::span<float> values) {
  if (values.empty() || !is_finite_embedding(values)) {
    throw std::invalid_argument("embedding must be non-empty and finite");
  }
  double sum = 0.0;
  for (const float value : values) {
    sum += static_cast<double>(value) * static_cast<double>(value);
  }
  const double norm = std::sqrt(sum);
  if (norm <= 1e-12) {
    throw std::runtime_error("cannot normalize a zero embedding");
  }
  for (float& value : values) {
    value = static_cast<float>(static_cast<double>(value) / norm);
  }
}

float dot_product(const std::span<const float> lhs, const std::span<const float> rhs) {
  if (lhs.size() != rhs.size()) {
    throw std::invalid_argument("embedding dimensions do not match");
  }
  if (!is_finite_embedding(lhs) || !is_finite_embedding(rhs)) {
    throw std::invalid_argument("embedding must be finite");
  }
  return std::inner_product(lhs.begin(), lhs.end(), rhs.begin(), 0.0F);
}

} // namespace llcl
