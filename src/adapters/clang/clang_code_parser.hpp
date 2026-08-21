#pragma once

#include "llamacodelab/domain/chunk.hpp"
#include "llamacodelab/domain/symbol.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace llcl::clang_adapter {

struct ParsedTranslationUnit {
  std::vector<Symbol> symbols;
  std::vector<SymbolEdge> edges;
  std::vector<Chunk> semantic_chunks;
  std::vector<std::string> diagnostics;
  bool success{};
};

class ClangCodeParser {
public:
  explicit ClangCodeParser(std::filesystem::path compilation_database_directory);

  [[nodiscard]] ParsedTranslationUnit parse(const std::filesystem::path& source_file,
                                            const std::filesystem::path& repository_root) const;

private:
  std::filesystem::path compilation_database_directory_;
};

} // namespace llcl::clang_adapter
