#pragma once

#include "llamacodelab/domain/symbol.hpp"

#include <filesystem>

struct sqlite3;

namespace llcl::sqlite_adapter {

class SqliteSymbolRepository final : public ISymbolRepository {
public:
  explicit SqliteSymbolRepository(const std::filesystem::path& database_path);
  ~SqliteSymbolRepository() override;
  SqliteSymbolRepository(const SqliteSymbolRepository&) = delete;
  SqliteSymbolRepository& operator=(const SqliteSymbolRepository&) = delete;

  void replace_file(std::filesystem::path relative_path, std::span<const Symbol> symbols,
                    std::span<const SymbolEdge> edges) override;
  [[nodiscard]] std::vector<Symbol> find_exact(std::string_view qualified_name) const override;
  [[nodiscard]] std::vector<SymbolEdge> outgoing(std::uint64_t symbol_id,
                                                 std::string_view relation = {}) const override;
  [[nodiscard]] std::vector<SymbolEdge> incoming(std::uint64_t symbol_id,
                                                 std::string_view relation = {}) const override;

private:
  sqlite3* database_{};
};

} // namespace llcl::sqlite_adapter
