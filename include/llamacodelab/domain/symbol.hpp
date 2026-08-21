#pragma once

#include "llamacodelab/domain/chunk.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace llcl {

enum class SymbolKind {
  namespace_,
  class_,
  struct_,
  function,
  method,
  constructor,
  destructor,
  enum_,
  variable,
};

struct Symbol {
  std::uint64_t id{};
  SymbolKind kind{};
  std::string qualified_name;
  std::string signature;
  SourceRange declaration;
  std::optional<SourceRange> definition;
};

struct SymbolEdge {
  std::uint64_t from{};
  std::uint64_t to{};
  std::string relation;
};

class ISymbolRepository {
public:
  virtual ~ISymbolRepository() = default;

  virtual void replace_file(std::filesystem::path relative_path, std::span<const Symbol> symbols,
                            std::span<const SymbolEdge> edges) = 0;
  [[nodiscard]] virtual std::vector<Symbol> find_exact(std::string_view qualified_name) const = 0;
  [[nodiscard]] virtual std::vector<SymbolEdge> outgoing(std::uint64_t symbol_id,
                                                         std::string_view relation = {}) const = 0;
  [[nodiscard]] virtual std::vector<SymbolEdge> incoming(std::uint64_t symbol_id,
                                                         std::string_view relation = {}) const = 0;
};

} // namespace llcl
