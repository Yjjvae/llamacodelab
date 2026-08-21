#include "adapters/clang/clang_code_parser.hpp"
#include "adapters/sqlite/sqlite_chunk_repository.hpp"
#include "adapters/sqlite/sqlite_symbol_repository.hpp"
#include "llamacodelab/application/index_service.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <string>

namespace llcl::test {
namespace {

class TemporaryProject {
public:
  TemporaryProject() : path_(std::filesystem::path(testing::TempDir()) / "llcl-clang-parser") {
    std::filesystem::remove_all(path_);
    std::filesystem::create_directories(path_ / "build");
  }
  ~TemporaryProject() {
    std::filesystem::remove_all(path_);
  }
  [[nodiscard]] const std::filesystem::path& path() const noexcept {
    return path_;
  }

private:
  std::filesystem::path path_;
};

void write_file(const std::filesystem::path& path, const std::string_view content) {
  std::ofstream output(path);
  output << content;
}

class FixedEmbedder final : public IEmbedder {
public:
  [[nodiscard]] Embedding embed(std::string_view, EmbeddingKind) override {
    return {1.0F, 0.0F};
  }
  [[nodiscard]] std::vector<Embedding> embed_batch(std::span<const std::string_view> texts,
                                                   EmbeddingKind) override {
    return std::vector<Embedding>(texts.size(), {1.0F, 0.0F});
  }
  [[nodiscard]] std::size_t dimension() const noexcept override {
    return 2;
  }
};

} // namespace

TEST(ClangCodeParserTest, ExtractsSemanticSymbolsChunksAndOneHopEdges) {
  TemporaryProject project;
  const auto source = project.path() / "fixture.cpp";
  write_file(source, R"(namespace demo {
int helper() { return 7; }
class Base {
public:
  virtual int run() { return helper(); }
};
class Derived : public Base {
public:
  int run() override { return helper(); }
};
})");
  write_file(project.path() / "build" / "compile_commands.json",
             "[{\"directory\":\"" + project.path().generic_string() +
                 "\",\"command\":\"clang++-21 -std=c++20 -c " + source.generic_string() +
                 "\",\"file\":\"" + source.generic_string() + "\"}]");

  clang_adapter::ClangCodeParser parser(project.path() / "build");
  const auto result = parser.parse(source, project.path());

  ASSERT_TRUE(result.success) << (result.diagnostics.empty() ? "" : result.diagnostics.front());
  const auto derived =
      std::find_if(result.symbols.begin(), result.symbols.end(),
                   [](const Symbol& symbol) { return symbol.qualified_name == "demo::Derived"; });
  ASSERT_NE(derived, result.symbols.end());
  const auto helper =
      std::find_if(result.symbols.begin(), result.symbols.end(),
                   [](const Symbol& symbol) { return symbol.qualified_name == "demo::helper"; });
  ASSERT_NE(helper, result.symbols.end());
  ASSERT_TRUE(helper->definition.has_value());
  EXPECT_EQ(helper->definition->path, "fixture.cpp");
  EXPECT_EQ(helper->definition->start_line, 2U);
  EXPECT_TRUE(std::any_of(result.edges.begin(), result.edges.end(), [&](const SymbolEdge& edge) {
    return edge.from == derived->id && edge.relation == "inherits";
  }));
  EXPECT_TRUE(std::any_of(result.edges.begin(), result.edges.end(), [&](const SymbolEdge& edge) {
    return edge.to == helper->id && edge.relation == "calls";
  }));
  EXPECT_TRUE(std::any_of(result.semantic_chunks.begin(), result.semantic_chunks.end(),
                          [](const Chunk& chunk) {
                            return chunk.content.find("Symbol: demo::helper") != std::string::npos;
                          }));
  EXPECT_TRUE(std::any_of(result.semantic_chunks.begin(), result.semantic_chunks.end(),
                          [&](const Chunk& chunk) {
                            return chunk.id == derived->id &&
                                   chunk.content.find("Symbol: demo::Derived") != std::string::npos;
                          }));
}

TEST(ClangCodeParserTest, ReportsCompilationDatabaseFailuresWithoutThrowing) {
  TemporaryProject project;
  clang_adapter::ClangCodeParser parser(project.path() / "missing-build");

  const auto result = parser.parse(project.path() / "missing.cpp", project.path());

  EXPECT_FALSE(result.success);
  EXPECT_FALSE(result.diagnostics.empty());
  EXPECT_TRUE(result.symbols.empty());
}

TEST(ClangCodeParserTest, IndexesSemanticChunksAndFallsBackForInvalidSource) {
  TemporaryProject project;
  const auto source = project.path() / "fixture.cpp";
  write_file(source,
             "namespace demo { int helper() { return 7; } int caller() { return helper(); } }\n");
  write_file(project.path() / "build" / "compile_commands.json",
             "[{\"directory\":\"" + project.path().generic_string() +
                 "\",\"command\":\"clang++-21 -std=c++20 -c " + source.generic_string() +
                 "\",\"file\":\"" + source.generic_string() + "\"}]");
  FixedEmbedder embedder;
  SearchIndexHandle handle;
  IndexConfig config;
  config.data_dir = project.path() / "index";
  config.semantic_index_enabled = true;
  config.compilation_database_dir = "build";
  IndexService service(embedder, handle, config, "test-embedding");

  const auto semantic = service.update(project.path());
  EXPECT_EQ(semantic.embedded_chunks, 2U);
  sqlite_adapter::SqliteChunkRepository chunks(config.data_dir / "index.sqlite3");
  const auto stored = chunks.chunks_for_document("fixture.cpp");
  ASSERT_EQ(stored.size(), 2U);
  EXPECT_NE(stored.front().content.find("Symbol: demo::"), std::string::npos);
  EXPECT_EQ(stored.front().chunker_version, "clang-ast-v1");
  sqlite_adapter::SqliteSymbolRepository symbols(config.data_dir / "symbols.sqlite3");
  const auto helpers = symbols.find_exact("demo::helper");
  ASSERT_EQ(helpers.size(), 1U);
  EXPECT_TRUE(std::any_of(stored.begin(), stored.end(),
                          [&](const Chunk& chunk) { return chunk.id == helpers.front().id; }));

  write_file(source, "namespace demo { int broken( { return 0; } }\n");
  const auto fallback = service.update(project.path());
  EXPECT_EQ(fallback.files_changed, 1U);
  const auto text_stored = chunks.chunks_for_document("fixture.cpp");
  ASSERT_EQ(text_stored.size(), 1U);
  EXPECT_EQ(text_stored.front().chunker_version, "text-v1");
  EXPECT_TRUE(symbols.find_exact("demo::helper").empty());
}

TEST(ClangCodeParserTest, HandlesTemplatesOverridesAndMacroExpandedBodies) {
  TemporaryProject project;
  const auto source = project.path() / "fixture.cpp";
  write_file(source, R"(#define ANSWER 42
namespace demo {
template <typename T> T identity(T value) { return value; }
struct Base { virtual int run() { return ANSWER; } };
struct Derived : Base { int run() override { return identity(ANSWER); } };
})");
  write_file(project.path() / "build" / "compile_commands.json",
             "[{\"directory\":\"" + project.path().generic_string() +
                 "\",\"command\":\"clang++-21 -std=c++20 -c " + source.generic_string() +
                 "\",\"file\":\"" + source.generic_string() + "\"}]");

  clang_adapter::ClangCodeParser parser(project.path() / "build");
  const auto result = parser.parse(source, project.path());

  ASSERT_TRUE(result.success);
  EXPECT_TRUE(std::any_of(result.symbols.begin(), result.symbols.end(), [](const Symbol& symbol) {
    return symbol.qualified_name == "demo::identity";
  }));
  EXPECT_TRUE(std::any_of(result.edges.begin(), result.edges.end(),
                          [](const SymbolEdge& edge) { return edge.relation == "overrides"; }));
  EXPECT_TRUE(std::any_of(
      result.semantic_chunks.begin(), result.semantic_chunks.end(), [](const Chunk& chunk) {
        return chunk.content.find("Symbol: demo::Derived::run") != std::string::npos;
      }));
}

} // namespace llcl::test
