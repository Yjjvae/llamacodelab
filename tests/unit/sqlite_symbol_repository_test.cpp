#include "adapters/sqlite/sqlite_symbol_repository.hpp"

#include <filesystem>
#include <gtest/gtest.h>

namespace llcl::test {
namespace {

class TemporaryDatabase {
public:
  TemporaryDatabase() : path_(std::filesystem::path(testing::TempDir()) / "llcl-symbols.sqlite3") {
    std::filesystem::remove(path_);
  }
  ~TemporaryDatabase() {
    std::filesystem::remove(path_);
    std::filesystem::remove(path_.string() + "-shm");
    std::filesystem::remove(path_.string() + "-wal");
  }
  [[nodiscard]] const std::filesystem::path& path() const noexcept {
    return path_;
  }

private:
  std::filesystem::path path_;
};

[[nodiscard]] Symbol make_symbol(const std::uint64_t id, const SymbolKind kind,
                                 std::string qualified_name, const std::filesystem::path& path,
                                 const std::uint32_t line) {
  return {.id = id,
          .kind = kind,
          .qualified_name = std::move(qualified_name),
          .signature = "void()",
          .declaration = {.path = path, .start_line = line, .end_line = line},
          .definition = SourceRange{.path = path, .start_line = line, .end_line = line + 2}};
}

} // namespace

TEST(SqliteSymbolRepositoryTest, PersistsExactSymbolsAndOneHopEdges) {
  TemporaryDatabase temporary;
  sqlite_adapter::SqliteSymbolRepository repository(temporary.path());
  const std::filesystem::path path{"src/widget.cpp"};
  const std::vector<Symbol> symbols{
      make_symbol(101, SymbolKind::class_, "demo::Widget", path, 3),
      make_symbol(102, SymbolKind::method, "demo::Widget::run", path, 8),
      make_symbol(103, SymbolKind::function, "demo::helper", path, 16),
  };
  const std::vector<SymbolEdge> edges{{.from = 102, .to = 103, .relation = "calls"}};

  repository.replace_file(path, symbols, edges);

  const auto methods = repository.find_exact("demo::Widget::run");
  ASSERT_EQ(methods.size(), 1U);
  EXPECT_EQ(methods.front().id, 102U);
  EXPECT_EQ(methods.front().kind, SymbolKind::method);
  ASSERT_TRUE(methods.front().definition.has_value());
  EXPECT_EQ(methods.front().definition->start_line, 8U);

  const auto calls = repository.outgoing(102, "calls");
  ASSERT_EQ(calls.size(), 1U);
  EXPECT_EQ(calls.front().to, 103U);

  const auto callers = repository.incoming(103, "calls");
  ASSERT_EQ(callers.size(), 1U);
  EXPECT_EQ(callers.front().from, 102U);
}

TEST(SqliteSymbolRepositoryTest, ReplacesFileSymbolsAndCascadesTheirEdges) {
  TemporaryDatabase temporary;
  sqlite_adapter::SqliteSymbolRepository repository(temporary.path());
  const std::filesystem::path path{"src/widget.cpp"};
  const std::vector<Symbol> original{
      make_symbol(201, SymbolKind::function, "demo::old_function", path, 1),
      make_symbol(202, SymbolKind::function, "demo::target", path, 6),
  };
  const std::vector<SymbolEdge> edges{{.from = 201, .to = 202, .relation = "calls"}};
  repository.replace_file(path, original, edges);

  const std::vector<Symbol> replacement{
      make_symbol(203, SymbolKind::function, "demo::new_function", path, 2),
  };
  repository.replace_file(path, replacement, {});

  EXPECT_TRUE(repository.find_exact("demo::old_function").empty());
  EXPECT_TRUE(repository.outgoing(201).empty());
  EXPECT_EQ(repository.find_exact("demo::new_function").size(), 1U);
}

} // namespace llcl::test
