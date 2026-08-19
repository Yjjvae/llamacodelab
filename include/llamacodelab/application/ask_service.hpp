#pragma once

#include "llamacodelab/application/context_budget.hpp"
#include "llamacodelab/domain/retrieval.hpp"

#include <cstddef>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

namespace llcl {

struct Citation {
  std::string source_id;
  SourceRange source;
  float retrieval_score{};
};

struct AskResult {
  std::string answer;
  std::vector<Citation> citations;
  GenerationStats generation;
  std::size_t prompt_tokens{};
  std::size_t skipped_sources{};
  bool citations_valid{};
};

class AskService {
public:
  AskService(IEmbedder& embedder, IVectorIndex& index, IChunkRepository& chunks,
             ContextBudget& context_budget, ITextGenerator& generator, GenerationOptions options,
             RagPromptBudget budget);

  [[nodiscard]] AskResult ask(std::string_view question, std::size_t top_k,
                              const TokenCallback& on_token, std::stop_token stop_token);

private:
  IEmbedder& embedder_;
  IVectorIndex& index_;
  IChunkRepository& chunks_;
  ContextBudget& context_budget_;
  ITextGenerator& generator_;
  GenerationOptions options_;
  RagPromptBudget budget_;
};

} // namespace llcl
