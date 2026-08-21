#pragma once

#include "llamacodelab/domain/chunk.hpp"
#include "llamacodelab/domain/symbol.hpp"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace clang {
class ASTContext;
class CXXRecordDecl;
class CallExpr;
class EnumDecl;
class FunctionDecl;
class RecursiveASTVisitorBase;
} // namespace clang

namespace llcl::clang_adapter {

class SymbolVisitor;

class SymbolCollector {
public:
  explicit SymbolCollector(std::filesystem::path repository_root);
  ~SymbolCollector();
  SymbolCollector(const SymbolCollector&) = delete;
  SymbolCollector& operator=(const SymbolCollector&) = delete;

  void collect(clang::ASTContext& context);
  [[nodiscard]] const std::vector<Symbol>& symbols() const noexcept;
  [[nodiscard]] const std::vector<SymbolEdge>& edges() const noexcept;
  [[nodiscard]] const std::vector<Chunk>& semantic_chunks() const noexcept;

private:
  std::unique_ptr<SymbolVisitor> visitor_;
};

} // namespace llcl::clang_adapter
