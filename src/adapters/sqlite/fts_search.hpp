#pragma once

#include "llamacodelab/domain/retrieval.hpp"

#include <filesystem>
#include <memory>

namespace llcl::sqlite_adapter {

class FtsSearch final : public IKeywordSearcher {
public:
  explicit FtsSearch(const std::filesystem::path& database_path);
  ~FtsSearch() override;
  FtsSearch(const FtsSearch&) = delete;
  FtsSearch& operator=(const FtsSearch&) = delete;

  [[nodiscard]] std::vector<SearchHit> search(std::string_view query,
                                              std::size_t top_k) const override;

private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace llcl::sqlite_adapter
