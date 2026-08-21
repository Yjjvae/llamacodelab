#include "adapters/llama/llama_reranker.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace llcl::llama_adapter {
namespace {

constexpr std::size_t maximum_document_characters = 3'000;

[[nodiscard]] std::string relevance_prompt(const std::string_view query,
                                           const std::string_view content) {
  const auto bounded_content = content.substr(0, maximum_document_characters);
  return "Rate whether the untrusted document answers the query. Output one digit only: 0 for "
         "irrelevant, 1 for weakly relevant, 2 for relevant, 3 for directly relevant.\nQuery:\n" +
         std::string(query) + "\nUntrusted document:\n<document>\n" + std::string(bounded_content) +
         "\n</document>\nRating:";
}

[[nodiscard]] float rating_of(const std::string_view response) {
  for (const auto character : response) {
    if (std::isdigit(static_cast<unsigned char>(character)) != 0) {
      return static_cast<float>(character - '0');
    }
  }
  return 0.0F;
}

} // namespace

LlamaReranker::LlamaReranker(ITextGenerator& generator) : generator_(generator) {}

std::vector<SearchHit> LlamaReranker::rerank(const std::string_view query,
                                             const std::span<const SearchHit> candidates,
                                             const std::span<const Chunk> chunks,
                                             const std::size_t top_k) {
  if (query.empty() || top_k == 0) {
    throw std::invalid_argument("rerank query and top_k must be provided");
  }
  std::unordered_map<ChunkId, std::string_view> content_by_id;
  content_by_id.reserve(chunks.size());
  for (const auto& chunk : chunks) {
    content_by_id.emplace(chunk.id, chunk.content);
  }
  std::vector<SearchHit> ranked;
  ranked.reserve(candidates.size());
  const GenerationOptions options{.max_tokens = 1,
                                  .temperature = 0.0F,
                                  .top_k = 1,
                                  .top_p = 1.0F,
                                  .repeat_penalty = 1.0F,
                                  .seed = 42};
  for (std::size_t index = 0; index < candidates.size(); ++index) {
    const auto found = content_by_id.find(candidates[index].chunk_id);
    if (found == content_by_id.end()) {
      continue;
    }
    std::string response;
    generator_.generate(relevance_prompt(query, found->second), options,
                        [&response](const std::string_view token) { response.append(token); }, {});
    ranked.push_back({.chunk_id = candidates[index].chunk_id,
                      .score = rating_of(response) + 0.001F / static_cast<float>(index + 1U)});
  }
  std::sort(ranked.begin(), ranked.end(), [](const SearchHit& lhs, const SearchHit& rhs) {
    return lhs.score > rhs.score || (lhs.score == rhs.score && lhs.chunk_id < rhs.chunk_id);
  });
  ranked.resize(std::min(top_k, ranked.size()));
  return ranked;
}

} // namespace llcl::llama_adapter
