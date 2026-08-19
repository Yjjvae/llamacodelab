#include "llamacodelab/application/context_budget.hpp"

#include <gtest/gtest.h>
#include <string>
#include <vector>

namespace llcl::test {
namespace {

class CharacterCounter final : public ITokenCounter {
public:
  [[nodiscard]] std::size_t count_tokens(const std::string_view text) const override {
    return text.size();
  }
};

[[nodiscard]] RagSource source(const std::string_view id, const std::string_view content) {
  return {.id = std::string(id),
          .chunk = {.id = static_cast<ChunkId>(id.back()),
                    .source = {.path = "src/pool.cpp", .start_line = 10, .end_line = 20},
                    .language = "cpp",
                    .content = std::string(content),
                    .content_hash = "test",
                    .chunker_version = "test"},
          .score = 1.0F};
}

} // namespace

TEST(ContextBudgetTest, RetainsHighPrioritySourcesThatFitAndRendersMetadata) {
  CharacterCounter counter;
  ContextBudget budget;
  const std::vector<RagSource> sources{source("S1", "first"), source("S2", "second")};
  const auto result = budget.build(
      "pool?", sources, counter,
      {.model_context = 800, .reserved_output_tokens = 100, .safety_margin_tokens = 50});
  EXPECT_EQ(result.retained_sources.size(), 2U);
  EXPECT_NE(result.text.find("path=\"src/pool.cpp\" lines=\"10-20\""), std::string::npos);
  EXPECT_NE(result.text.find("[QUESTION]\npool?"), std::string::npos);
}

TEST(ContextBudgetTest, SkipsSourcesInsteadOfConsumingOutputBudget) {
  CharacterCounter counter;
  ContextBudget context_budget;
  const std::vector<RagSource> sources{source("S1", std::string(900, 'a')), source("S2", "short")};
  const auto result = context_budget.build(
      "q", sources, counter,
      {.model_context = 800, .reserved_output_tokens = 100, .safety_margin_tokens = 50});
  EXPECT_EQ(result.retained_sources.size(), 1U);
  EXPECT_EQ(result.retained_sources.front().id, "S2");
  EXPECT_EQ(result.skipped_sources, 1U);
  EXPECT_LE(result.token_count, 650U);
}

} // namespace llcl::test
