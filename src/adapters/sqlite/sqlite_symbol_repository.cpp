#include "adapters/sqlite/sqlite_symbol_repository.hpp"

#include <sqlite3.h>
#include <stdexcept>
#include <string>

namespace llcl::sqlite_adapter {
namespace {

void check(const int result, sqlite3* database, const char* operation) {
  if (result != SQLITE_OK && result != SQLITE_DONE && result != SQLITE_ROW) {
    throw std::runtime_error(std::string(operation) + ": " + sqlite3_errmsg(database));
  }
}

void execute(sqlite3* database, const char* sql) {
  char* error{};
  const auto result = sqlite3_exec(database, sql, nullptr, nullptr, &error);
  if (result != SQLITE_OK) {
    const std::string message = error == nullptr ? sqlite3_errmsg(database) : error;
    sqlite3_free(error);
    throw std::runtime_error(message);
  }
}

class Statement {
public:
  Statement(sqlite3* database, const char* sql) : database_(database) {
    check(sqlite3_prepare_v2(database, sql, -1, &statement_, nullptr), database, "prepare SQL");
  }
  ~Statement() {
    sqlite3_finalize(statement_);
  }
  [[nodiscard]] sqlite3_stmt* get() const noexcept {
    return statement_;
  }

private:
  sqlite3* database_{};
  sqlite3_stmt* statement_{};
};

void bind_text(sqlite3_stmt* statement, const int index, const std::string_view value) {
  if (sqlite3_bind_text(statement, index, value.data(), static_cast<int>(value.size()),
                        SQLITE_TRANSIENT) != SQLITE_OK) {
    throw std::runtime_error("bind text failed");
  }
}

[[nodiscard]] SourceRange read_range(sqlite3_stmt* statement, const int offset) {
  return {.path = reinterpret_cast<const char*>(sqlite3_column_text(statement, offset)),
          .start_line = static_cast<std::uint32_t>(sqlite3_column_int(statement, offset + 1)),
          .end_line = static_cast<std::uint32_t>(sqlite3_column_int(statement, offset + 2))};
}

} // namespace

SqliteSymbolRepository::SqliteSymbolRepository(const std::filesystem::path& database_path) {
  std::filesystem::create_directories(database_path.parent_path());
  check(sqlite3_open_v2(database_path.string().c_str(), &database_,
                        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
                        nullptr),
        database_, "open SQLite database");
  sqlite3_busy_timeout(database_, 5'000);
  execute(database_, "PRAGMA journal_mode=WAL; PRAGMA foreign_keys=ON;");
  execute(database_, R"(CREATE TABLE IF NOT EXISTS symbols (
    id INTEGER PRIMARY KEY, kind INTEGER NOT NULL, qualified_name TEXT NOT NULL,
    signature TEXT NOT NULL, declaration_path TEXT NOT NULL, declaration_start_line INTEGER NOT NULL,
    declaration_end_line INTEGER NOT NULL, definition_path TEXT, definition_start_line INTEGER,
    definition_end_line INTEGER);
    CREATE UNIQUE INDEX IF NOT EXISTS idx_symbols_identity
      ON symbols(qualified_name,declaration_path,declaration_start_line);
    CREATE INDEX IF NOT EXISTS idx_symbols_qualified_name ON symbols(qualified_name);
    CREATE TABLE IF NOT EXISTS symbol_edges (
      from_id INTEGER NOT NULL, to_id INTEGER NOT NULL, relation TEXT NOT NULL,
      PRIMARY KEY(from_id,to_id,relation),
      FOREIGN KEY(from_id) REFERENCES symbols(id) ON DELETE CASCADE,
      FOREIGN KEY(to_id) REFERENCES symbols(id) ON DELETE CASCADE);)");
}

SqliteSymbolRepository::~SqliteSymbolRepository() {
  sqlite3_close(database_);
}

void SqliteSymbolRepository::replace_file(const std::filesystem::path relative_path,
                                          const std::span<const Symbol> symbols,
                                          const std::span<const SymbolEdge> edges) {
  execute(database_, "BEGIN IMMEDIATE;");
  try {
    Statement remove(database_, "DELETE FROM symbols WHERE declaration_path=?;");
    bind_text(remove.get(), 1, relative_path.generic_string());
    check(sqlite3_step(remove.get()), database_, "delete file symbols");

    Statement insert(database_, R"(INSERT INTO symbols(
      id,kind,qualified_name,signature,declaration_path,declaration_start_line,declaration_end_line,
      definition_path,definition_start_line,definition_end_line) VALUES(?,?,?,?,?,?,?,?,?,?);)");
    for (const auto& symbol : symbols) {
      sqlite3_reset(insert.get());
      sqlite3_clear_bindings(insert.get());
      check(sqlite3_bind_int64(insert.get(), 1, static_cast<sqlite3_int64>(symbol.id)), database_,
            "bind symbol id");
      check(sqlite3_bind_int(insert.get(), 2, static_cast<int>(symbol.kind)), database_,
            "bind symbol kind");
      bind_text(insert.get(), 3, symbol.qualified_name);
      bind_text(insert.get(), 4, symbol.signature);
      bind_text(insert.get(), 5, symbol.declaration.path.generic_string());
      check(sqlite3_bind_int(insert.get(), 6, static_cast<int>(symbol.declaration.start_line)),
            database_, "bind declaration start");
      check(sqlite3_bind_int(insert.get(), 7, static_cast<int>(symbol.declaration.end_line)),
            database_, "bind declaration end");
      if (symbol.definition.has_value()) {
        bind_text(insert.get(), 8, symbol.definition->path.generic_string());
        check(sqlite3_bind_int(insert.get(), 9, static_cast<int>(symbol.definition->start_line)),
              database_, "bind definition start");
        check(sqlite3_bind_int(insert.get(), 10, static_cast<int>(symbol.definition->end_line)),
              database_, "bind definition end");
      } else {
        check(sqlite3_bind_null(insert.get(), 8), database_, "bind null definition path");
        check(sqlite3_bind_null(insert.get(), 9), database_, "bind null definition start");
        check(sqlite3_bind_null(insert.get(), 10), database_, "bind null definition end");
      }
      check(sqlite3_step(insert.get()), database_, "insert symbol");
    }
    Statement insert_edge(
        database_, "INSERT OR IGNORE INTO symbol_edges(from_id,to_id,relation) VALUES(?,?,?);");
    for (const auto& edge : edges) {
      sqlite3_reset(insert_edge.get());
      sqlite3_clear_bindings(insert_edge.get());
      check(sqlite3_bind_int64(insert_edge.get(), 1, static_cast<sqlite3_int64>(edge.from)),
            database_, "bind edge source");
      check(sqlite3_bind_int64(insert_edge.get(), 2, static_cast<sqlite3_int64>(edge.to)),
            database_, "bind edge target");
      bind_text(insert_edge.get(), 3, edge.relation);
      check(sqlite3_step(insert_edge.get()), database_, "insert symbol edge");
    }
    execute(database_, "COMMIT;");
  } catch (...) {
    try {
      execute(database_, "ROLLBACK;");
    } catch (...) {
    }
    throw;
  }
}

std::vector<Symbol>
SqliteSymbolRepository::find_exact(const std::string_view qualified_name) const {
  Statement statement(database_, R"(SELECT id,kind,qualified_name,signature,declaration_path,
    declaration_start_line,declaration_end_line,definition_path,definition_start_line,definition_end_line
    FROM symbols WHERE qualified_name=? ORDER BY declaration_path,declaration_start_line;)");
  bind_text(statement.get(), 1, qualified_name);
  std::vector<Symbol> result;
  while (sqlite3_step(statement.get()) == SQLITE_ROW) {
    Symbol symbol{
        .id = static_cast<std::uint64_t>(sqlite3_column_int64(statement.get(), 0)),
        .kind = static_cast<SymbolKind>(sqlite3_column_int(statement.get(), 1)),
        .qualified_name = reinterpret_cast<const char*>(sqlite3_column_text(statement.get(), 2)),
        .signature = reinterpret_cast<const char*>(sqlite3_column_text(statement.get(), 3)),
        .declaration = read_range(statement.get(), 4),
        .definition = std::nullopt};
    if (sqlite3_column_type(statement.get(), 7) != SQLITE_NULL) {
      symbol.definition = read_range(statement.get(), 7);
    }
    result.push_back(std::move(symbol));
  }
  return result;
}

std::vector<SymbolEdge> SqliteSymbolRepository::outgoing(const std::uint64_t symbol_id,
                                                         const std::string_view relation) const {
  Statement statement(
      database_,
      relation.empty()
          ? "SELECT from_id,to_id,relation FROM symbol_edges WHERE from_id=?;"
          : "SELECT from_id,to_id,relation FROM symbol_edges WHERE from_id=? AND relation=?;");
  check(sqlite3_bind_int64(statement.get(), 1, static_cast<sqlite3_int64>(symbol_id)), database_,
        "bind outgoing symbol");
  if (!relation.empty()) {
    bind_text(statement.get(), 2, relation);
  }
  std::vector<SymbolEdge> result;
  while (sqlite3_step(statement.get()) == SQLITE_ROW) {
    result.push_back(
        {.from = static_cast<std::uint64_t>(sqlite3_column_int64(statement.get(), 0)),
         .to = static_cast<std::uint64_t>(sqlite3_column_int64(statement.get(), 1)),
         .relation = reinterpret_cast<const char*>(sqlite3_column_text(statement.get(), 2))});
  }
  return result;
}

std::vector<SymbolEdge> SqliteSymbolRepository::incoming(const std::uint64_t symbol_id,
                                                         const std::string_view relation) const {
  Statement statement(
      database_,
      relation.empty()
          ? "SELECT from_id,to_id,relation FROM symbol_edges WHERE to_id=?;"
          : "SELECT from_id,to_id,relation FROM symbol_edges WHERE to_id=? AND relation=?;");
  check(sqlite3_bind_int64(statement.get(), 1, static_cast<sqlite3_int64>(symbol_id)), database_,
        "bind incoming symbol");
  if (!relation.empty())
    bind_text(statement.get(), 2, relation);
  std::vector<SymbolEdge> result;
  while (sqlite3_step(statement.get()) == SQLITE_ROW) {
    result.push_back(
        {.from = static_cast<std::uint64_t>(sqlite3_column_int64(statement.get(), 0)),
         .to = static_cast<std::uint64_t>(sqlite3_column_int64(statement.get(), 1)),
         .relation = reinterpret_cast<const char*>(sqlite3_column_text(statement.get(), 2))});
  }
  return result;
}

} // namespace llcl::sqlite_adapter
