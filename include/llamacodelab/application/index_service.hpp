#pragma once

#include "llamacodelab/domain/retrieval.hpp"
#include "llamacodelab/support/config.hpp"

#include <atomic>
#include <filesystem>
#include <memory>
#include <string>

namespace llcl {

using IndexSnapshot = std::shared_ptr<const IVectorIndex>;

class SearchIndexHandle {
public:
  [[nodiscard]] IndexSnapshot load() const noexcept;
  void publish(IndexSnapshot next) noexcept;

private:
  std::atomic<IndexSnapshot> current_{};
};

struct IndexUpdateResult {
  std::size_t files_added{};
  std::size_t files_changed{};
  std::size_t files_removed{};
  std::size_t files_unchanged{};
  std::size_t embedded_chunks{};
  std::uint64_t generation{};
};

class IndexService {
public:
  IndexService(IEmbedder& embedder, SearchIndexHandle& index_handle, IndexConfig config,
               std::string embedding_model_id);
  [[nodiscard]] IndexUpdateResult update(const std::filesystem::path& repository_root);

private:
  IEmbedder& embedder_;
  SearchIndexHandle& index_handle_;
  IndexConfig config_;
  std::string embedding_model_id_;
};

} // namespace llcl
