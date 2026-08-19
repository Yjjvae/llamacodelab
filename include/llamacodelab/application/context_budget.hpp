#pragma once

#include "llamacodelab/domain/chunk.hpp"
#include "llamacodelab/domain/generation.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace llcl {

struct RagSource {
  std::string id;
  Chunk chunk;
  float score{};
};

struct RagPromptBudget {
  std::size_t model_context{4096};
  std::size_t reserved_output_tokens{512};
  std::size_t safety_margin_tokens{128};
};

struct BuiltRagPrompt {
  std::string text;
  std::vector<RagSource> retained_sources;
  std::size_t token_count{};
  std::size_t skipped_sources{};
};

class ContextBudget {
public:
  [[nodiscard]] BuiltRagPrompt build(std::string_view question, std::span<const RagSource> sources,
                                     const ITokenCounter& token_counter,
                                     RagPromptBudget budget) const;
};

} // namespace llcl
