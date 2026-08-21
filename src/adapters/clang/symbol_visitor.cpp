#include "adapters/clang/symbol_visitor.hpp"

#include "llamacodelab/support/hash.hpp"

#include <algorithm>
#include <clang/AST/ASTContext.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/Expr.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Basic/SourceManager.h>
#include <optional>
#include <unordered_map>
#include <utility>

namespace llcl::clang_adapter {

[[nodiscard]] SourceRange extract_source_range(const clang::SourceManager& source_manager,
                                               clang::SourceRange range,
                                               const std::filesystem::path& repository_root);
[[nodiscard]] std::string extract_source_text(const clang::SourceManager& source_manager,
                                              const clang::LangOptions& language_options,
                                              clang::SourceRange range);

namespace {

[[nodiscard]] SymbolKind kind_of(const clang::FunctionDecl& declaration) {
  if (llvm::isa<clang::CXXConstructorDecl>(declaration)) {
    return SymbolKind::constructor;
  }
  if (llvm::isa<clang::CXXDestructorDecl>(declaration)) {
    return SymbolKind::destructor;
  }
  if (llvm::isa<clang::CXXMethodDecl>(declaration)) {
    return SymbolKind::method;
  }
  return SymbolKind::function;
}

} // namespace

class SymbolVisitor final : public clang::RecursiveASTVisitor<SymbolVisitor> {
public:
  explicit SymbolVisitor(std::filesystem::path repository_root)
      : repository_root_(std::move(repository_root)) {}

  void collect(clang::ASTContext& context) {
    context_ = &context;
    TraverseDecl(context.getTranslationUnitDecl());
    resolve_edges();
  }

  bool TraverseFunctionDecl(clang::FunctionDecl* declaration) {
    FunctionScope scope(*this, declaration);
    const auto traversed =
        clang::RecursiveASTVisitor<SymbolVisitor>::TraverseFunctionDecl(declaration);
    return traversed;
  }

  bool TraverseCXXMethodDecl(clang::CXXMethodDecl* declaration) {
    FunctionScope scope(*this, declaration);
    const auto traversed =
        clang::RecursiveASTVisitor<SymbolVisitor>::TraverseCXXMethodDecl(declaration);
    return traversed;
  }

  bool TraverseCXXConstructorDecl(clang::CXXConstructorDecl* declaration) {
    FunctionScope scope(*this, declaration);
    const auto traversed =
        clang::RecursiveASTVisitor<SymbolVisitor>::TraverseCXXConstructorDecl(declaration);
    return traversed;
  }

  bool TraverseCXXDestructorDecl(clang::CXXDestructorDecl* declaration) {
    FunctionScope scope(*this, declaration);
    const auto traversed =
        clang::RecursiveASTVisitor<SymbolVisitor>::TraverseCXXDestructorDecl(declaration);
    return traversed;
  }

  bool VisitFunctionDecl(clang::FunctionDecl* declaration) {
    if (declaration == nullptr || declaration->isImplicit() ||
        !is_from_main_file(declaration->getLocation())) {
      return true;
    }
    const auto index =
        add_symbol(*declaration, kind_of(*declaration), declaration->getType().getAsString());
    if (declaration->isThisDeclarationADefinition()) {
      add_semantic_chunk(*declaration, index);
    }
    if (const auto* method = llvm::dyn_cast<clang::CXXMethodDecl>(declaration); method != nullptr) {
      for (const auto* overridden : method->overridden_methods()) {
        pending_edges_.push_back({.from = declaration->getCanonicalDecl(),
                                  .to = overridden->getCanonicalDecl(),
                                  .relation = "overrides"});
      }
    }
    return true;
  }

  bool VisitCXXRecordDecl(clang::CXXRecordDecl* declaration) {
    if (declaration == nullptr || declaration->isImplicit() ||
        !declaration->isThisDeclarationADefinition() ||
        !is_from_main_file(declaration->getLocation())) {
      return true;
    }
    const auto kind = declaration->isClass() ? SymbolKind::class_ : SymbolKind::struct_;
    const auto index = add_symbol(*declaration, kind, declaration->getNameAsString());
    add_semantic_chunk(*declaration, index);
    for (const auto& base : declaration->bases()) {
      if (const auto* base_declaration = base.getType()->getAsCXXRecordDecl();
          base_declaration != nullptr) {
        pending_edges_.push_back({.from = declaration->getCanonicalDecl(),
                                  .to = base_declaration->getCanonicalDecl(),
                                  .relation = "inherits"});
      }
    }
    return true;
  }

  bool VisitEnumDecl(clang::EnumDecl* declaration) {
    if (declaration == nullptr || declaration->isImplicit() ||
        !is_from_main_file(declaration->getLocation())) {
      return true;
    }
    add_symbol(*declaration, SymbolKind::enum_, declaration->getNameAsString());
    return true;
  }

  bool VisitCallExpr(clang::CallExpr* expression) {
    if (expression == nullptr || current_functions_.empty()) {
      return true;
    }
    if (const auto* callee = expression->getDirectCallee(); callee != nullptr) {
      pending_edges_.push_back({.from = current_functions_.back(),
                                .to = callee->getCanonicalDecl(),
                                .relation = "calls"});
    }
    return true;
  }

  [[nodiscard]] const std::vector<Symbol>& symbols() const noexcept {
    return symbols_;
  }
  [[nodiscard]] const std::vector<SymbolEdge>& edges() const noexcept {
    return edges_;
  }
  [[nodiscard]] const std::vector<Chunk>& semantic_chunks() const noexcept {
    return semantic_chunks_;
  }

private:
  class FunctionScope {
  public:
    FunctionScope(SymbolVisitor& visitor, const clang::FunctionDecl* declaration)
        : visitor_(visitor) {
      if (declaration != nullptr && visitor_.is_from_main_file(declaration->getLocation())) {
        visitor_.current_functions_.push_back(declaration->getCanonicalDecl());
        pushed_ = true;
      }
    }
    ~FunctionScope() {
      if (pushed_) {
        visitor_.current_functions_.pop_back();
      }
    }

  private:
    SymbolVisitor& visitor_;
    bool pushed_{};
  };

  struct PendingEdge {
    const clang::Decl* from{};
    const clang::Decl* to{};
    std::string relation;
  };

  [[nodiscard]] bool is_from_main_file(const clang::SourceLocation location) const {
    const auto& source_manager = context_->getSourceManager();
    return source_manager.isWrittenInMainFile(source_manager.getExpansionLoc(location));
  }

  template <typename Decl>
  std::size_t add_symbol(const Decl& declaration, const SymbolKind kind, std::string signature) {
    const auto* canonical = declaration.getCanonicalDecl();
    if (const auto found = indices_.find(canonical); found != indices_.end()) {
      auto& symbol = symbols_[found->second];
      if (declaration.isThisDeclarationADefinition()) {
        symbol.definition = extract_source_range(context_->getSourceManager(),
                                                 declaration.getSourceRange(), repository_root_);
      }
      return found->second;
    }
    const auto declaration_range = extract_source_range(
        context_->getSourceManager(), declaration.getSourceRange(), repository_root_);
    const auto qualified_name = declaration.getQualifiedNameAsString();
    const auto id = stable_hash64(std::to_string(static_cast<int>(kind)) + "|" + qualified_name +
                                  "|" + declaration_range.path.generic_string() + "|" +
                                  std::to_string(declaration_range.start_line));
    Symbol symbol{.id = id,
                  .kind = kind,
                  .qualified_name = qualified_name,
                  .signature = std::move(signature),
                  .declaration = declaration_range,
                  .definition = std::nullopt};
    if (declaration.isThisDeclarationADefinition()) {
      symbol.definition = declaration_range;
    }
    indices_.emplace(canonical, symbols_.size());
    symbols_.push_back(std::move(symbol));
    return symbols_.size() - 1U;
  }

  template <typename Decl>
  void add_semantic_chunk(const Decl& declaration, const std::size_t index) {
    const auto& symbol = symbols_[index];
    const auto source = extract_source_text(context_->getSourceManager(), context_->getLangOpts(),
                                            declaration.getSourceRange());
    if (source.empty()) {
      return;
    }
    const auto content =
        "Symbol: " + symbol.qualified_name + "\nSignature: " + symbol.signature + "\n\n" + source;
    semantic_chunks_.push_back({.id = symbol.id,
                                .source = symbol.definition.value_or(symbol.declaration),
                                .language = "cpp",
                                .content = content,
                                .content_hash = stable_hash_hex(content),
                                .chunker_version = "clang-ast-v1"});
  }

  void resolve_edges() {
    for (const auto& pending : pending_edges_) {
      const auto source = indices_.find(pending.from);
      const auto target = indices_.find(pending.to);
      if (source != indices_.end() && target != indices_.end() &&
          source->second != target->second) {
        edges_.push_back({.from = symbols_[source->second].id,
                          .to = symbols_[target->second].id,
                          .relation = pending.relation});
      }
    }
    std::sort(edges_.begin(), edges_.end(), [](const SymbolEdge& lhs, const SymbolEdge& rhs) {
      return lhs.from != rhs.from
                 ? lhs.from < rhs.from
                 : (lhs.to != rhs.to ? lhs.to < rhs.to : lhs.relation < rhs.relation);
    });
    edges_.erase(std::unique(edges_.begin(), edges_.end(),
                             [](const SymbolEdge& lhs, const SymbolEdge& rhs) {
                               return lhs.from == rhs.from && lhs.to == rhs.to &&
                                      lhs.relation == rhs.relation;
                             }),
                 edges_.end());
  }

  clang::ASTContext* context_{};
  std::filesystem::path repository_root_;
  std::unordered_map<const clang::Decl*, std::size_t> indices_;
  std::vector<const clang::FunctionDecl*> current_functions_;
  std::vector<PendingEdge> pending_edges_;
  std::vector<Symbol> symbols_;
  std::vector<SymbolEdge> edges_;
  std::vector<Chunk> semantic_chunks_;
};

SymbolCollector::SymbolCollector(std::filesystem::path repository_root)
    : visitor_(std::make_unique<SymbolVisitor>(std::move(repository_root))) {}
SymbolCollector::~SymbolCollector() = default;
void SymbolCollector::collect(clang::ASTContext& context) {
  visitor_->collect(context);
}
const std::vector<Symbol>& SymbolCollector::symbols() const noexcept {
  return visitor_->symbols();
}
const std::vector<SymbolEdge>& SymbolCollector::edges() const noexcept {
  return visitor_->edges();
}
const std::vector<Chunk>& SymbolCollector::semantic_chunks() const noexcept {
  return visitor_->semantic_chunks();
}

} // namespace llcl::clang_adapter
