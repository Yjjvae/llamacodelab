#pragma once

#include "llamacodelab/domain/retrieval.hpp"

#include <cstddef>
#include <filesystem>
#include <memory>
#include <string>

namespace llcl::vector_adapter {

struct HnswOptions {
  std::size_t max_elements{100'000};
  std::size_t m{16};
  std::size_t ef_construction{200};
  std::size_t ef_search{64};
};

struct HnswFileMetadata {
  static constexpr std::uint32_t format_version = 1;

  std::uint32_t version{format_version};
  std::size_t dimension{};
  std::size_t max_elements{};
  std::string embedding_model_hash;
};

class HnswIndex final : public IVectorIndex {
public:
  HnswIndex(std::size_t dimension, HnswOptions options = {});
  ~HnswIndex();
  HnswIndex(HnswIndex&&) noexcept;
  HnswIndex& operator=(HnswIndex&&) noexcept;
  HnswIndex(const HnswIndex&) = delete;
  HnswIndex& operator=(const HnswIndex&) = delete;

  void upsert(ChunkId id, std::span<const float> values) override;
  void erase(ChunkId id) override;
  [[nodiscard]] std::vector<SearchHit> search(std::span<const float> query,
                                              std::size_t top_k) const override;
  [[nodiscard]] std::size_t size() const noexcept override;
  [[nodiscard]] std::size_t dimension() const noexcept override;

  void set_ef_search(std::size_t ef_search);

  void save(const std::filesystem::path& index_path, const HnswFileMetadata& metadata) const;
  [[nodiscard]] static HnswIndex load(const std::filesystem::path& index_path,
                                      const HnswFileMetadata& expected_metadata,
                                      HnswOptions options = {});

private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace llcl::vector_adapter
