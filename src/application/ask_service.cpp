#include "llamacodelab/application/ask_service.hpp"

#include <algorithm>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace llcl {
namespace {

[[nodiscard]] bool answer_cites(const std::string_view answer, const std::string_view source_id) {
  return answer.find("[" + std::string(source_id) + "]") != std::string_view::npos;
}

} // namespace

AskService::AskService(const IRetriever& retriever, IChunkRepository& chunks,
                       ContextBudget& context_budget, ITextGenerator& generator,
                       GenerationOptions options, RagPromptBudget budget,
                       AskRetrievalOptions retrieval_options)
    : retriever_(retriever), chunks_(chunks), context_budget_(context_budget),
      generator_(generator), options_(options), budget_(budget),
      retrieval_options_(retrieval_options) {
  if (retrieval_options_.rerank_candidates < 20 || retrieval_options_.rerank_candidates > 50) {
    throw std::invalid_argument("rerank candidate count must be in [20, 50]");
  }
}

AskResult AskService::ask(const std::string_view question, const std::size_t top_k,
                          const TokenCallback& on_token, const std::stop_token stop_token) {
  if (question.empty() || top_k == 0 || !on_token) {
    throw std::invalid_argument("question, top_k, and token callback must be provided");
  }
  auto hits =
      retriever_.retrieve(question, retrieval_options_.reranker == nullptr
                                        ? top_k
                                        : std::max(top_k, retrieval_options_.rerank_candidates));
  if (hits.empty()) {
    throw std::runtime_error("no repository context matched the question");
  }
  std::vector<ChunkId> ids;
  ids.reserve(hits.size());
  for (const auto& hit : hits) {
    ids.push_back(hit.chunk_id);
  }
  const auto chunks = chunks_.get_many(ids);
  if (retrieval_options_.reranker != nullptr) {
    hits = retrieval_options_.reranker->rerank(question, hits, chunks, top_k);
  }
  std::unordered_map<ChunkId, Chunk> by_id;
  by_id.reserve(chunks.size());
  for (const auto& chunk : chunks) {
    by_id.emplace(chunk.id, chunk);
  }
  std::vector<RagSource> sources;
  sources.reserve(hits.size());
  for (const auto& hit : hits) {
    const auto found = by_id.find(hit.chunk_id);
    if (found != by_id.end()) {
      sources.push_back({.id = "S" + std::to_string(sources.size() + 1),
                         .chunk = found->second,
                         .score = hit.score});
    }
  }
  if (sources.empty()) {
    throw std::runtime_error("all retrieved chunks were removed from the repository");
  }
  const auto prompt = context_budget_.build(question, sources, generator_, budget_);
  std::string answer;
  const auto tee = [&](const std::string_view piece) {
    answer.append(piece);
    on_token(piece);
  };
  const auto stats = generator_.generate(prompt.text, options_, tee, stop_token);
  std::vector<Citation> citations;
  citations.reserve(prompt.retained_sources.size());
  bool citations_valid = !prompt.retained_sources.empty();
  for (const auto& source : prompt.retained_sources) {
    citations.push_back(
        {.source_id = source.id, .source = source.chunk.source, .retrieval_score = source.score});
    citations_valid = citations_valid && answer_cites(answer, source.id);
  }
  return {.answer = std::move(answer),
          .citations = std::move(citations),
          .generation = stats,
          .prompt_tokens = prompt.token_count,
          .skipped_sources = prompt.skipped_sources,
          .citations_valid = citations_valid};
}

} // namespace llcl
