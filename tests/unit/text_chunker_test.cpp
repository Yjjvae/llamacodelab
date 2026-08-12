#include "adapters/filesystem/text_chunker.hpp"

#include <gtest/gtest.h>
#include <stdexcept>
#include <string>

namespace llcl::filesystem_adapter {
namespace {

TEST(TextChunkerTest, NormalizesCrlfAndTracksOverlappingLineRanges) {
  const auto chunks = TextChunker{}.chunk_text(
      "src/example.cpp", "cpp", "one\r\ntwo\r\nthree",
      {.max_lines = 2, .overlap_lines = 1, .max_bytes = 1024, .version = "test-v1"});

  ASSERT_EQ(chunks.size(), 2U);
  EXPECT_EQ(chunks[0].source.start_line, 1U);
  EXPECT_EQ(chunks[0].source.end_line, 2U);
  EXPECT_EQ(chunks[0].content, "one\ntwo");
  EXPECT_EQ(chunks[1].source.start_line, 2U);
  EXPECT_EQ(chunks[1].source.end_line, 3U);
  EXPECT_EQ(chunks[1].content, "two\nthree");
}

TEST(TextChunkerTest, DoesNotCreateChunksForAnEmptyFile) {
  EXPECT_TRUE(TextChunker{}.chunk_text("src/empty.cpp", "cpp", "").empty());
}

TEST(TextChunkerTest, EmitsAnOversizedSingleLineAndStillMakesProgress) {
  const auto chunks = TextChunker{}.chunk_text(
      "src/example.cpp", "cpp", "very-long-line\nok",
      {.max_lines = 2, .overlap_lines = 1, .max_bytes = 4, .version = "test-v1"});

  ASSERT_EQ(chunks.size(), 2U);
  EXPECT_EQ(chunks[0].source.start_line, 1U);
  EXPECT_EQ(chunks[0].source.end_line, 1U);
  EXPECT_EQ(chunks[0].content, "very-long-line");
  EXPECT_EQ(chunks[1].source.start_line, 2U);
  EXPECT_EQ(chunks[1].source.end_line, 2U);
  EXPECT_EQ(chunks[1].content, "ok");
}

TEST(TextChunkerTest, ProducesStableIdsThatIncludeTheRelativePath) {
  const auto options =
      ChunkingOptions{.max_lines = 8, .overlap_lines = 0, .max_bytes = 1024, .version = "test-v1"};
  const auto first = TextChunker{}.chunk_text("src/one.cpp", "cpp", "int value;", options);
  const auto repeated = TextChunker{}.chunk_text("src/one.cpp", "cpp", "int value;", options);
  const auto other_path = TextChunker{}.chunk_text("src/two.cpp", "cpp", "int value;", options);

  ASSERT_EQ(first.size(), 1U);
  ASSERT_EQ(repeated.size(), 1U);
  ASSERT_EQ(other_path.size(), 1U);
  EXPECT_EQ(first[0].id, repeated[0].id);
  EXPECT_EQ(first[0].content_hash, repeated[0].content_hash);
  EXPECT_NE(first[0].id, other_path[0].id);
}

TEST(TextChunkerTest, RejectsInvalidOverlap) {
  EXPECT_THROW(static_cast<void>(TextChunker{}.chunk_text("src/example.cpp", "cpp", "line",
                                                          {.max_lines = 2, .overlap_lines = 2})),
               std::invalid_argument);
}

} // namespace
} // namespace llcl::filesystem_adapter
