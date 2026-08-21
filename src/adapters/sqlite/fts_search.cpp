#include "adapters/sqlite/fts_search.hpp"

#include <algorithm>
#include <cctype>
#include <limits>
#include <sqlite3.h>
#include <stdexcept>
#include <string>
#include <vector>

namespace llcl::sqlite_adapter {
namespace {

void check(const int result, sqlite3* database, const char* operation) {
  if (result != SQLITE_OK && result != SQLITE_DONE && result != SQLITE_ROW) {
    throw std::runtime_error(std::string(operation) + ": " + sqlite3_errmsg(database));
  }
}

class Statement {
public:
  Statement(sqlite3* database, const char* sql) : database_(database) {
    check(sqlite3_prepare_v2(database, sql, -1, &statement_, nullptr), database,
          "prepare FTS query");
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

[[nodiscard]] std::string fts_query(const std::string_view query) {
  std::vector<std::string> terms;
  std::string term;
  for (const auto character : query) {
    const auto byte = static_cast<unsigned char>(character);
    if (std::isalnum(byte) != 0 || character == '_') {
      term.push_back(character);
    } else if (!term.empty()) {
      terms.push_back(std::move(term));
      term.clear();
    }
  }
  if (!term.empty()) {
    terms.push_back(std::move(term));
  }
  std::string result;
  for (const auto& item : terms) {
    if (!result.empty()) {
      result += " AND ";
    }
    result += '"';
    result += item;
    result += '"';
  }
  return result;
}

} // namespace

class FtsSearch::Impl {
public:
  explicit Impl(const std::filesystem::path& database_path) {
    const auto result = sqlite3_open_v2(database_path.string().c_str(), &database,
                                        SQLITE_OPEN_READONLY | SQLITE_OPEN_FULLMUTEX, nullptr);
    if (result != SQLITE_OK) {
      const std::string message =
          database == nullptr ? "open FTS database failed" : sqlite3_errmsg(database);
      sqlite3_close(database);
      database = nullptr;
      throw std::runtime_error(message);
    }
    sqlite3_busy_timeout(database, 5'000);
  }
  ~Impl() {
    sqlite3_close(database);
  }

  sqlite3* database{};
};

FtsSearch::FtsSearch(const std::filesystem::path& database_path)
    : impl_(std::make_unique<Impl>(database_path)) {}
FtsSearch::~FtsSearch() = default;

std::vector<SearchHit> FtsSearch::search(const std::string_view query,
                                         const std::size_t top_k) const {
  if (top_k == 0) {
    return {};
  }
  const auto expression = fts_query(query);
  if (expression.empty()) {
    return {};
  }
  if (top_k > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
    throw std::invalid_argument("FTS top_k is too large");
  }
  Statement statement(impl_->database,
                      "SELECT rowid, -bm25(chunk_fts) FROM chunk_fts WHERE chunk_fts MATCH ? "
                      "ORDER BY bm25(chunk_fts), rowid LIMIT ?;");
  check(sqlite3_bind_text(statement.get(), 1, expression.c_str(), -1, SQLITE_TRANSIENT),
        impl_->database, "bind FTS query");
  check(sqlite3_bind_int(statement.get(), 2, static_cast<int>(top_k)), impl_->database,
        "bind FTS limit");
  std::vector<SearchHit> hits;
  int result{};
  while ((result = sqlite3_step(statement.get())) == SQLITE_ROW) {
    hits.push_back({.chunk_id = static_cast<ChunkId>(sqlite3_column_int64(statement.get(), 0)),
                    .score = static_cast<float>(sqlite3_column_double(statement.get(), 1))});
  }
  check(result, impl_->database, "run FTS query");
  return hits;
}

} // namespace llcl::sqlite_adapter
