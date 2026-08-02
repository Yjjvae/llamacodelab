#include "test_doubles/fake_generator.hpp"

#include <gtest/gtest.h>

#include <stop_token>
#include <string>

namespace llcl::test {
namespace {

TEST(FakeGeneratorTest, StreamsConfiguredResponseAndRecordsPrompt) {
  FakeGenerator generator;
  generator.response = "one token at a time";
  std::string output;

  const auto stats = generator.generate(
      "question", {}, [&output](std::string_view token) { output.append(token); }, {});

  EXPECT_EQ(generator.last_prompt, "question");
  EXPECT_EQ(output, generator.response);
  EXPECT_EQ(stats.generated_tokens, 2U);
  EXPECT_EQ(generator.count_tokens("anything"), 1U);
  EXPECT_EQ(generator.count_tokens(""), 0U);
}

TEST(FakeGeneratorTest, HonorsPreRequestedStop) {
  FakeGenerator generator;
  std::stop_source stop_source;
  stop_source.request_stop();
  std::string output;

  const auto stats = generator.generate(
      "question",
      {},
      [&output](std::string_view token) { output.append(token); },
      stop_source.get_token());

  EXPECT_TRUE(output.empty());
  EXPECT_EQ(stats.generated_tokens, 0U);
}

}  // namespace
}  // namespace llcl::test
