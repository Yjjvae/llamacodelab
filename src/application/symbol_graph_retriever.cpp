#include "llamacodelab/application/symbol_graph_retriever.hpp"

#include <algorithm>
#include <cctype>
#include <unordered_map>

namespace llcl {
namespace {
[[nodiscard]] std::vector<std::string> extract_qualified_names(const std::string_view query) {
  std::vector<std::string> names;
  std::string token;
  const auto flush = [&] {
    if (token.find("::") != std::string::npos)
      names.push_back(token);
    token.clear();
  };
  for (const auto character : query) {
    if (std::isalnum(static_cast<unsigned char>(character)) != 0 || character == '_' ||
        character == ':')
      token += character;
    else
      flush();
  }
  flush();
  return names;
}
} // namespace

SymbolGraphRetriever::SymbolGraphRetriever(const ISymbolRepository& symbols) : symbols_(symbols) {}
std::vector<SearchHit> SymbolGraphRetriever::retrieve(const std::string_view query,
                                                      const std::size_t top_k) const {
  std::unordered_map<ChunkId, float> scores;
  const bool caller_query = query.find("caller") != std::string_view::npos ||
                            query.find("calls") != std::string_view::npos ||
                            query.find("调用") != std::string_view::npos;
  for (const auto& name : extract_qualified_names(query)) {
    for (const auto& symbol : symbols_.find_exact(name)) {
      scores[symbol.id] += 2.0F;
      const auto edges =
          caller_query ? symbols_.incoming(symbol.id, "calls") : symbols_.outgoing(symbol.id);
      for (const auto& edge : edges)
        scores[caller_query ? edge.from : edge.to] += 1.0F;
    }
  }
  std::vector<SearchHit> hits;
  for (const auto& [id, score] : scores)
    hits.push_back({.chunk_id = id, .score = score});
  std::sort(hits.begin(), hits.end(), [](const SearchHit& lhs, const SearchHit& rhs) {
    return lhs.score > rhs.score || (lhs.score == rhs.score && lhs.chunk_id < rhs.chunk_id);
  });
  hits.resize(std::min(top_k, hits.size()));
  return hits;
}
} // namespace llcl
