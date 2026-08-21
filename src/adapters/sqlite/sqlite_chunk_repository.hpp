#pragma once

#include "llamacodelab/domain/retrieval.hpp"

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

struct sqlite3;

namespace llcl::sqlite_adapter {

struct StoredDocument {
  std::filesystem::path relative_path;
  std::string content_hash;
  std::uintmax_t size_bytes{};
  std::int64_t modified_ns{};
  std::string parser_version;
  std::vector<Chunk> chunks;
};

class SqliteChunkRepository final : public IChunkRepository {
public:
  explicit SqliteChunkRepository(const std::filesystem::path& database_path);
  ~SqliteChunkRepository() override;
  SqliteChunkRepository(const SqliteChunkRepository&) = delete;
  SqliteChunkRepository& operator=(const SqliteChunkRepository&) = delete;

  void begin();
  void commit();
  void rollback() noexcept;
  void replace_document(const StoredDocument& document, std::uint64_t vector_generation);
  void erase_document(const std::filesystem::path& relative_path);
  [[nodiscard]] std::vector<Chunk> get_many(std::span<const ChunkId> ids) const override;
  [[nodiscard]] std::vector<StoredDocument> documents() const;
  [[nodiscard]] std::vector<Chunk>
  chunks_for_document(const std::filesystem::path& relative_path) const;
  [[nodiscard]] std::string metadata(std::string_view key) const;
  void set_metadata(std::string_view key, std::string_view value);
  [[nodiscard]] bool healthy() const noexcept;

private:
  sqlite3* database_{};
};

} // namespace llcl::sqlite_adapter
