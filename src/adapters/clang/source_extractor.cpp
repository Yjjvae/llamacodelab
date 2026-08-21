#include "llamacodelab/domain/chunk.hpp"

#include <clang/Basic/SourceManager.h>
#include <clang/Lex/Lexer.h>
#include <filesystem>
#include <string>

namespace llcl::clang_adapter {
namespace {

[[nodiscard]] std::filesystem::path relative_or_absolute(const std::filesystem::path& path,
                                                         const std::filesystem::path& root) {
  std::error_code error;
  const auto relative = std::filesystem::relative(path, root, error);
  return error || relative.empty() || relative.native().starts_with("..") ? path : relative;
}

} // namespace

[[nodiscard]] SourceRange extract_source_range(const clang::SourceManager& source_manager,
                                               const clang::SourceRange range,
                                               const std::filesystem::path& repository_root) {
  const auto begin = source_manager.getSpellingLoc(range.getBegin());
  const auto end = source_manager.getSpellingLoc(range.getEnd());
  const std::filesystem::path path{source_manager.getFilename(begin).str()};
  return {.path = relative_or_absolute(path, repository_root),
          .start_line = source_manager.getSpellingLineNumber(begin),
          .end_line = source_manager.getSpellingLineNumber(end)};
}

[[nodiscard]] std::string extract_source_text(const clang::SourceManager& source_manager,
                                              const clang::LangOptions& language_options,
                                              const clang::SourceRange range) {
  const auto text = clang::Lexer::getSourceText(clang::CharSourceRange::getTokenRange(range),
                                                source_manager, language_options);
  return text.str();
}

} // namespace llcl::clang_adapter
