#include "adapters/sqlite/sqlite_chunk_repository.hpp"

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
  sqlite3_stmt* get() const noexcept {
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

} // namespace

SqliteChunkRepository::SqliteChunkRepository(const std::filesystem::path& database_path) {
  std::filesystem::create_directories(database_path.parent_path());
  check(sqlite3_open_v2(database_path.string().c_str(), &database_,
                        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
                        nullptr),
        database_, "open SQLite database");
  sqlite3_busy_timeout(database_, 5'000);
  execute(database_, "PRAGMA journal_mode=WAL; PRAGMA foreign_keys=ON;");
  execute(database_, R"(CREATE TABLE IF NOT EXISTS documents (
    id INTEGER PRIMARY KEY, relative_path TEXT NOT NULL UNIQUE, content_hash TEXT NOT NULL,
    size_bytes INTEGER NOT NULL, modified_ns INTEGER NOT NULL, parser_version TEXT NOT NULL);
    CREATE TABLE IF NOT EXISTS chunks (
    id INTEGER PRIMARY KEY, document_id INTEGER NOT NULL, start_line INTEGER NOT NULL,
    end_line INTEGER NOT NULL, language TEXT NOT NULL, symbol TEXT NOT NULL DEFAULT '',
    content TEXT NOT NULL, content_hash TEXT NOT NULL, embedding_offset INTEGER,
    FOREIGN KEY(document_id) REFERENCES documents(id) ON DELETE CASCADE);
    CREATE TABLE IF NOT EXISTS index_metadata (key TEXT PRIMARY KEY, value TEXT NOT NULL);
    CREATE INDEX IF NOT EXISTS idx_chunks_document_id ON chunks(document_id);
    CREATE VIRTUAL TABLE IF NOT EXISTS chunk_fts USING fts5(path, symbol, content);
    CREATE TRIGGER IF NOT EXISTS chunks_fts_insert AFTER INSERT ON chunks BEGIN
      INSERT INTO chunk_fts(rowid,path,symbol,content)
      SELECT new.id,documents.relative_path,new.symbol,new.content
      FROM documents WHERE documents.id=new.document_id;
    END;
    CREATE TRIGGER IF NOT EXISTS chunks_fts_delete AFTER DELETE ON chunks BEGIN
      DELETE FROM chunk_fts WHERE rowid=old.id;
    END;
    CREATE TRIGGER IF NOT EXISTS chunks_fts_update AFTER UPDATE OF document_id,symbol,content ON chunks BEGIN
      DELETE FROM chunk_fts WHERE rowid=old.id;
      INSERT INTO chunk_fts(rowid,path,symbol,content)
      SELECT new.id,documents.relative_path,new.symbol,new.content
      FROM documents WHERE documents.id=new.document_id;
    END;)");
  execute(database_, R"(INSERT OR REPLACE INTO chunk_fts(rowid,path,symbol,content)
    SELECT chunks.id,documents.relative_path,chunks.symbol,chunks.content
    FROM chunks JOIN documents ON documents.id=chunks.document_id;)");
  set_metadata("schema_version", "2");
}

SqliteChunkRepository::~SqliteChunkRepository() {
  sqlite3_close(database_);
}
void SqliteChunkRepository::begin() {
  execute(database_, "BEGIN IMMEDIATE;");
}
void SqliteChunkRepository::commit() {
  execute(database_, "COMMIT;");
}
void SqliteChunkRepository::rollback() noexcept {
  try {
    execute(database_, "ROLLBACK;");
  } catch (...) {
  }
}

void SqliteChunkRepository::replace_document(const StoredDocument& document,
                                             const std::uint64_t vector_generation) {
  erase_document(document.relative_path);
  Statement insert_document(
      database_,
      "INSERT INTO documents(relative_path,content_hash,size_bytes,modified_ns,parser_version) "
      "VALUES(?,?,?,?,?);");
  bind_text(insert_document.get(), 1, document.relative_path.generic_string());
  bind_text(insert_document.get(), 2, document.content_hash);
  check(
      sqlite3_bind_int64(insert_document.get(), 3, static_cast<sqlite3_int64>(document.size_bytes)),
      database_, "bind document size");
  check(sqlite3_bind_int64(insert_document.get(), 4, document.modified_ns), database_,
        "bind document mtime");
  bind_text(insert_document.get(), 5, document.parser_version);
  check(sqlite3_step(insert_document.get()), database_, "insert document");
  const auto document_id = sqlite3_last_insert_rowid(database_);
  Statement insert_chunk(database_, "INSERT INTO "
                                    "chunks(id,document_id,start_line,end_line,language,content,"
                                    "content_hash,embedding_offset) VALUES(?,?,?,?,?,?,?,?);");
  std::uint64_t offset{};
  for (const auto& chunk : document.chunks) {
    sqlite3_reset(insert_chunk.get());
    sqlite3_clear_bindings(insert_chunk.get());
    check(sqlite3_bind_int64(insert_chunk.get(), 1, static_cast<sqlite3_int64>(chunk.id)),
          database_, "bind chunk id");
    check(sqlite3_bind_int64(insert_chunk.get(), 2, document_id), database_, "bind document id");
    check(sqlite3_bind_int(insert_chunk.get(), 3, static_cast<int>(chunk.source.start_line)),
          database_, "bind start line");
    check(sqlite3_bind_int(insert_chunk.get(), 4, static_cast<int>(chunk.source.end_line)),
          database_, "bind end line");
    bind_text(insert_chunk.get(), 5, chunk.language);
    bind_text(insert_chunk.get(), 6, chunk.content);
    bind_text(insert_chunk.get(), 7, chunk.content_hash);
    check(sqlite3_bind_int64(insert_chunk.get(), 8, static_cast<sqlite3_int64>(offset++)),
          database_, "bind vector offset");
    check(sqlite3_step(insert_chunk.get()), database_, "insert chunk");
  }
  set_metadata("active_vector_generation", std::to_string(vector_generation));
}

void SqliteChunkRepository::erase_document(const std::filesystem::path& relative_path) {
  Statement statement(database_, "DELETE FROM documents WHERE relative_path=?;");
  bind_text(statement.get(), 1, relative_path.generic_string());
  check(sqlite3_step(statement.get()), database_, "delete document");
}

std::vector<Chunk> SqliteChunkRepository::get_many(const std::span<const ChunkId> ids) const {
  std::vector<Chunk> result;
  result.reserve(ids.size());
  Statement statement(
      database_,
      R"(SELECT d.relative_path,c.start_line,c.end_line,c.language,c.content,c.content_hash,d.parser_version
    FROM chunks c JOIN documents d ON d.id=c.document_id WHERE c.id=?;)");
  for (const auto id : ids) {
    sqlite3_reset(statement.get());
    sqlite3_clear_bindings(statement.get());
    check(sqlite3_bind_int64(statement.get(), 1, static_cast<sqlite3_int64>(id)), database_,
          "bind chunk lookup");
    if (sqlite3_step(statement.get()) == SQLITE_ROW) {
      result.push_back(
          {.id = id,
           .source =
               {.path = reinterpret_cast<const char*>(sqlite3_column_text(statement.get(), 0)),
                .start_line = static_cast<std::uint32_t>(sqlite3_column_int(statement.get(), 1)),
                .end_line = static_cast<std::uint32_t>(sqlite3_column_int(statement.get(), 2))},
           .language = reinterpret_cast<const char*>(sqlite3_column_text(statement.get(), 3)),
           .content = reinterpret_cast<const char*>(sqlite3_column_text(statement.get(), 4)),
           .content_hash = reinterpret_cast<const char*>(sqlite3_column_text(statement.get(), 5)),
           .chunker_version =
               reinterpret_cast<const char*>(sqlite3_column_text(statement.get(), 6))});
    }
  }
  return result;
}

std::vector<StoredDocument> SqliteChunkRepository::documents() const {
  std::vector<StoredDocument> result;
  Statement statement(
      database_,
      "SELECT relative_path,content_hash,size_bytes,modified_ns,parser_version FROM documents;");
  while (sqlite3_step(statement.get()) == SQLITE_ROW) {
    result.push_back(
        {.relative_path = reinterpret_cast<const char*>(sqlite3_column_text(statement.get(), 0)),
         .content_hash = reinterpret_cast<const char*>(sqlite3_column_text(statement.get(), 1)),
         .size_bytes = static_cast<std::uintmax_t>(sqlite3_column_int64(statement.get(), 2)),
         .modified_ns = sqlite3_column_int64(statement.get(), 3),
         .parser_version = reinterpret_cast<const char*>(sqlite3_column_text(statement.get(), 4)),
         .chunks = {}});
  }
  return result;
}

std::vector<Chunk>
SqliteChunkRepository::chunks_for_document(const std::filesystem::path& relative_path) const {
  std::vector<Chunk> result;
  Statement statement(
      database_,
      R"(SELECT c.id,c.start_line,c.end_line,c.language,c.content,c.content_hash,d.parser_version
    FROM chunks c JOIN documents d ON d.id=c.document_id WHERE d.relative_path=? ORDER BY c.id;)");
  bind_text(statement.get(), 1, relative_path.generic_string());
  while (sqlite3_step(statement.get()) == SQLITE_ROW) {
    result.push_back({
        .id = static_cast<ChunkId>(sqlite3_column_int64(statement.get(), 0)),
        .source = {.path = relative_path,
                   .start_line = static_cast<std::uint32_t>(sqlite3_column_int(statement.get(), 1)),
                   .end_line = static_cast<std::uint32_t>(sqlite3_column_int(statement.get(), 2))},
        .language = reinterpret_cast<const char*>(sqlite3_column_text(statement.get(), 3)),
        .content = reinterpret_cast<const char*>(sqlite3_column_text(statement.get(), 4)),
        .content_hash = reinterpret_cast<const char*>(sqlite3_column_text(statement.get(), 5)),
        .chunker_version = reinterpret_cast<const char*>(sqlite3_column_text(statement.get(), 6)),
    });
  }
  return result;
}

std::string SqliteChunkRepository::metadata(const std::string_view key) const {
  Statement statement(database_, "SELECT value FROM index_metadata WHERE key=?;");
  bind_text(statement.get(), 1, key);
  return sqlite3_step(statement.get()) == SQLITE_ROW
             ? reinterpret_cast<const char*>(sqlite3_column_text(statement.get(), 0))
             : "";
}
void SqliteChunkRepository::set_metadata(const std::string_view key, const std::string_view value) {
  Statement statement(database_, "INSERT INTO index_metadata(key,value) VALUES(?,?) ON "
                                 "CONFLICT(key) DO UPDATE SET value=excluded.value;");
  bind_text(statement.get(), 1, key);
  bind_text(statement.get(), 2, value);
  check(sqlite3_step(statement.get()), database_, "set metadata");
}
bool SqliteChunkRepository::healthy() const noexcept {
  return database_ != nullptr;
}

} // namespace llcl::sqlite_adapter
