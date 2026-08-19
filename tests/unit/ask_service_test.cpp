#include "llamacodelab/application/ask_service.hpp"
#include "test_doubles/fake_generator.hpp"

#include <gtest/gtest.h>
#include <unordered_map>

namespace llcl::test {
namespace {

class FakeEmbedder final : public IEmbedder {
public:
  std::string last_text;
  EmbeddingKind last_kind{};

  [[nodiscard]] Embedding embed(const std::string_view text, const EmbeddingKind kind) override {
    last_text = text;
    last_kind = kind;
    return {1.0F, 0.0F};
  }
  [[nodiscard]] std::vector<Embedding> embed_batch(std::span<const std::string_view>,
                                                   EmbeddingKind) override {
    return {};
  }
  [[nodiscard]] std::size_t dimension() const noexcept override {
    return 2;
  }
};

class FakeIndex final : public IVectorIndex {
public:
  std::size_t last_top_k{};
  void upsert(ChunkId, std::span<const float>) override {}
  void erase(ChunkId) override {}
  [[nodiscard]] std::vector<SearchHit> search(std::span<const float>,
                                              const std::size_t top_k) const override {
    const_cast<FakeIndex*>(this)->last_top_k = top_k;
    return {{.chunk_id = 4, .score = 0.9F}, {.chunk_id = 7, .score = 0.8F}};
  }
  [[nodiscard]] std::size_t size() const noexcept override {
    return 2;
  }
  [[nodiscard]] std::size_t dimension() const noexcept override {
    return 2;
  }
};

class FakeRepository final : public IChunkRepository {
public:
  [[nodiscard]] std::vector<Chunk> get_many(std::span<const ChunkId> ids) const override {
    std::vector<Chunk> result;
    for (const auto id : ids) {
      result.push_back({.id = id,
                        .source = {.path = id == 4 ? "src/pool.cpp" : "include/pool.hpp",
                                   .start_line = 10,
                                   .end_line = 20},
                        .language = "cpp",
                        .content = id == 4 ? "ConnectionPool pool;" : "class ConnectionPool;",
                        .content_hash = "test",
                        .chunker_version = "test"});
    }
    return result;
  }
};

} // namespace

TEST(AskServiceTest, ConnectsQuestionRetrievalContextGenerationAndCitations) {
  FakeEmbedder embedder;
  FakeIndex index;
  FakeRepository repository;
  ContextBudget context_budget;
  FakeGenerator generator;
  generator.response = "The pool is implemented in [S1] and declared in [S2].";
  AskService service(
      embedder, index, repository, context_budget, generator, {.max_tokens = 32},
      {.model_context = 4096, .reserved_output_tokens = 32, .safety_margin_tokens = 16});
  std::string streamed;
  const auto result = service.ask(
      "Where is the pool?", 2, [&streamed](std::string_view piece) { streamed.append(piece); }, {});
  EXPECT_EQ(embedder.last_text, "Where is the pool?");
  EXPECT_EQ(embedder.last_kind, EmbeddingKind::query);
  EXPECT_EQ(index.last_top_k, 2U);
  EXPECT_EQ(streamed, result.answer);
  EXPECT_NE(generator.last_prompt.find("src/pool.cpp"), std::string::npos);
  ASSERT_EQ(result.citations.size(), 2U);
  EXPECT_EQ(result.citations[0].source_id, "S1");
  EXPECT_TRUE(result.citations_valid);
}

} // namespace llcl::test
