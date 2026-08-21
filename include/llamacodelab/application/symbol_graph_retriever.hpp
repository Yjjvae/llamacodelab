#pragma once

#include "llamacodelab/domain/retrieval.hpp"
#include "llamacodelab/domain/symbol.hpp"

namespace llcl {

class SymbolGraphRetriever final : public IRetriever {
public:
  explicit SymbolGraphRetriever(const ISymbolRepository& symbols);
  [[nodiscard]] std::vector<SearchHit> retrieve(std::string_view query,
                                                std::size_t top_k) const override;

private:
  const ISymbolRepository& symbols_;
};

} // namespace llcl
