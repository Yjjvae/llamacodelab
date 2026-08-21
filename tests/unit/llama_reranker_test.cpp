#include "adapters/llama/llama_reranker.hpp"

#include <gtest/gtest.h>
#include <string>

namespace llcl::test {
namespace {

class ScriptedGenerator final : public ITextGenerator {
public:
  [[nodiscard]] std::size_t count_tokens(std::string_view text) const override {
    return text.empty() ? 0U : 1U;
  }

  GenerationStats generate(std::string_view prompt, const GenerationOptions& options,
                           const TokenCallback& on_token, std::stop_token) override {
    EXPECT_EQ(options.max_tokens, 1);
    last_prompt = prompt;
    on_token(prompt.find("direct answer implementation") == std::string_view::npos ? "0" : "3");
    return {.generated_tokens = 1};
  }

  std::string last_prompt;
};

[[nodiscard]] Chunk make_chunk(const ChunkId id, std::string content) {
  return {.id = id,
          .source = {.path = "fixture.cpp", .start_line = 1, .end_line = 1},
          .language = "cpp",
          .content = std::move(content),
          .content_hash = "fixture",
          .chunker_version = "test"};
}

} // namespace

TEST(LlamaRerankerTest, SortsCandidatesByDeterministicModelRating) {
  ScriptedGenerator generator;
  llama_adapter::LlamaReranker reranker(generator);
  const std::vector<SearchHit> candidates{{.chunk_id = 1, .score = 0.9F},
                                          {.chunk_id = 2, .score = 0.8F}};
  const std::vector<Chunk> chunks{make_chunk(1, "unrelated implementation"),
                                  make_chunk(2, "direct answer implementation")};

  const auto ranked = reranker.rerank("Where is the answer?", candidates, chunks, 1);

  ASSERT_EQ(ranked.size(), 1U);
  EXPECT_EQ(ranked.front().chunk_id, 2U);
  EXPECT_NE(generator.last_prompt.find("Untrusted document"), std::string::npos);
}

} // namespace llcl::test
