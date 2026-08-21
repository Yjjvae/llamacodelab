#include "llamacodelab/application/symbol_graph_retriever.hpp"

#include <gtest/gtest.h>

namespace llcl::test {
namespace {
class FakeSymbols final : public ISymbolRepository {
public:
  void replace_file(std::filesystem::path, std::span<const Symbol>,
                    std::span<const SymbolEdge>) override {}
  [[nodiscard]] std::vector<Symbol> find_exact(const std::string_view name) const override {
    if (name != "demo::helper")
      return {};
    Symbol symbol;
    symbol.id = 2;
    symbol.qualified_name = "demo::helper";
    return {std::move(symbol)};
  }
  [[nodiscard]] std::vector<SymbolEdge> outgoing(std::uint64_t id,
                                                 std::string_view) const override {
    return id == 2 ? std::vector<SymbolEdge>{{.from = 2, .to = 3, .relation = "calls"}}
                   : std::vector<SymbolEdge>{};
  }
  [[nodiscard]] std::vector<SymbolEdge> incoming(std::uint64_t id,
                                                 std::string_view) const override {
    return id == 2 ? std::vector<SymbolEdge>{{.from = 1, .to = 2, .relation = "calls"}}
                   : std::vector<SymbolEdge>{};
  }
};
} // namespace

TEST(SymbolGraphRetrieverTest, PrioritizesExactSymbolAndExpandsCallers) {
  FakeSymbols symbols;
  SymbolGraphRetriever retriever(symbols);
  const auto hits = retriever.retrieve("who calls demo::helper?", 3);
  ASSERT_EQ(hits.size(), 2U);
  EXPECT_EQ(hits[0].chunk_id, 2U);
  EXPECT_EQ(hits[1].chunk_id, 1U);
}

TEST(SymbolGraphRetrieverTest, ExpandsCalleesForNonCallerQueries) {
  FakeSymbols symbols;
  SymbolGraphRetriever retriever(symbols);
  const auto hits = retriever.retrieve("demo::helper implementation", 3);
  ASSERT_EQ(hits.size(), 2U);
  EXPECT_EQ(hits[0].chunk_id, 2U);
  EXPECT_EQ(hits[1].chunk_id, 3U);
}
} // namespace llcl::test
