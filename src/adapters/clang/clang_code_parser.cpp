#include "adapters/clang/clang_code_parser.hpp"

#include "adapters/clang/symbol_visitor.hpp"

#include <clang/Frontend/FrontendAction.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Tooling/CompilationDatabase.h>
#include <clang/Tooling/Tooling.h>
#include <memory>
#include <string>
#include <utility>

namespace llcl::clang_adapter {
namespace {

class CollectingConsumer final : public clang::ASTConsumer {
public:
  explicit CollectingConsumer(SymbolCollector& collector) : collector_(collector) {}
  void HandleTranslationUnit(clang::ASTContext& context) override {
    collector_.collect(context);
  }

private:
  SymbolCollector& collector_;
};

class CollectingAction final : public clang::ASTFrontendAction {
public:
  explicit CollectingAction(SymbolCollector& collector) : collector_(collector) {}
  [[nodiscard]] std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance&,
                                                                      llvm::StringRef) override {
    return std::make_unique<CollectingConsumer>(collector_);
  }

private:
  SymbolCollector& collector_;
};

class CollectingActionFactory final : public clang::tooling::FrontendActionFactory {
public:
  explicit CollectingActionFactory(SymbolCollector& collector) : collector_(collector) {}
  [[nodiscard]] std::unique_ptr<clang::FrontendAction> create() override {
    return std::make_unique<CollectingAction>(collector_);
  }

private:
  SymbolCollector& collector_;
};

} // namespace

ClangCodeParser::ClangCodeParser(std::filesystem::path compilation_database_directory)
    : compilation_database_directory_(std::move(compilation_database_directory)) {}

ParsedTranslationUnit ClangCodeParser::parse(const std::filesystem::path& source_file,
                                             const std::filesystem::path& repository_root) const {
  ParsedTranslationUnit result;
  std::string database_error;
  auto database = clang::tooling::CompilationDatabase::loadFromDirectory(
      compilation_database_directory_.string(), database_error);
  if (database == nullptr) {
    result.diagnostics.push_back("cannot load compilation database: " + database_error);
    return result;
  }
  SymbolCollector collector(repository_root);
  clang::tooling::ClangTool tool(*database, {source_file.string()});
  CollectingActionFactory factory(collector);
  const auto status = tool.run(&factory);
  if (status != 0) {
    result.diagnostics.push_back("Clang failed to parse " + source_file.string());
    return result;
  }
  result.symbols = collector.symbols();
  result.edges = collector.edges();
  result.semantic_chunks = collector.semantic_chunks();
  result.success = true;
  return result;
}

} // namespace llcl::clang_adapter
