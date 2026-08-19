#include "adapters/vector/brute_force_index.hpp"

#include "llamacodelab/domain/similarity.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace llcl::vector_adapter {
namespace {

[[nodiscard]] bool better(const SearchHit& lhs, const SearchHit& rhs) noexcept {
  return lhs.score > rhs.score || (lhs.score == rhs.score && lhs.chunk_id < rhs.chunk_id);
}

} // namespace

BruteForceIndex::BruteForceIndex(const std::size_t dimension) : dimension_(dimension) {
  if (dimension_ == 0) {
    throw std::invalid_argument("vector dimension must be positive");
  }
}

void BruteForceIndex::upsert(const ChunkId id, const std::span<const float> values) {
  if (values.size() != dimension_) {
    throw std::invalid_argument("embedding dimensions do not match index");
  }
  if (!is_finite_embedding(values)) {
    throw std::invalid_argument("embedding must be finite");
  }
  const auto position = positions_.find(id);
  if (position != positions_.end()) {
    records_[position->second].values.assign(values.begin(), values.end());
    return;
  }
  positions_.emplace(id, records_.size());
  records_.push_back({.id = id, .values = {values.begin(), values.end()}});
}

void BruteForceIndex::erase(const ChunkId id) {
  const auto position = positions_.find(id);
  if (position == positions_.end()) {
    return;
  }
  const auto index = position->second;
  const auto last = records_.size() - 1;
  if (index != last) {
    records_[index] = std::move(records_[last]);
    positions_[records_[index].id] = index;
  }
  records_.pop_back();
  positions_.erase(position);
}

std::vector<SearchHit> BruteForceIndex::search(const std::span<const float> query,
                                               const std::size_t top_k) const {
  if (top_k == 0) {
    return {};
  }
  if (query.size() != dimension_) {
    throw std::invalid_argument("query dimensions do not match index");
  }
  if (!is_finite_embedding(query)) {
    throw std::invalid_argument("query must be finite");
  }
  std::vector<SearchHit> hits;
  hits.reserve(records_.size());
  for (const auto& record : records_) {
    hits.push_back({.chunk_id = record.id, .score = dot_product(query, record.values)});
  }
  std::sort(hits.begin(), hits.end(), better);
  if (hits.size() > top_k) {
    hits.resize(top_k);
  }
  return hits;
}

std::size_t BruteForceIndex::size() const noexcept {
  return records_.size();
}
std::size_t BruteForceIndex::dimension() const noexcept {
  return dimension_;
}

} // namespace llcl::vector_adapter
