#include "adapters/vector/hnsw_index.hpp"

#include <filesystem>
#include <gtest/gtest.h>
#include <limits>
#include <stdexcept>

namespace llcl::test {
namespace {

class TempDirectory {
public:
  TempDirectory() : path(std::filesystem::temp_directory_path() / "llcl-hnsw-index-test") {
    std::filesystem::remove_all(path);
    std::filesystem::create_directories(path);
  }
  ~TempDirectory() {
    std::filesystem::remove_all(path);
  }
  std::filesystem::path path;
};

} // namespace

TEST(HnswIndexTest, SupportsUpsertEraseAndStableSearch) {
  vector_adapter::HnswIndex index(2, {.max_elements = 16});
  const Embedding horizontal{1.0F, 0.0F};
  const Embedding vertical{0.0F, 1.0F};
  index.upsert(9, horizontal);
  index.upsert(3, horizontal);
  index.upsert(5, vertical);
  const auto hits = index.search(horizontal, 2);
  ASSERT_EQ(hits.size(), 2U);
  EXPECT_EQ(hits[0].chunk_id, 3U);
  EXPECT_EQ(hits[1].chunk_id, 9U);
  index.erase(3);
  EXPECT_EQ(index.size(), 2U);
  EXPECT_EQ(index.search(horizontal, 1).front().chunk_id, 9U);
  index.upsert(3, horizontal);
  EXPECT_EQ(index.size(), 3U);
  EXPECT_EQ(index.search(horizontal, 2).front().chunk_id, 3U);
  index.upsert(9, vertical);
  EXPECT_EQ(index.search(vertical, 1).front().chunk_id, 5U);
}

TEST(HnswIndexTest, PersistsAndValidatesVectorSpaceMetadata) {
  TempDirectory temporary;
  const auto index_path = temporary.path / "index.hnsw";
  const vector_adapter::HnswOptions options{.max_elements = 16};
  vector_adapter::HnswIndex index(2, options);
  const Embedding horizontal{1.0F, 0.0F};
  const Embedding vertical{0.0F, 1.0F};
  index.upsert(1, horizontal);
  index.upsert(2, vertical);
  const vector_adapter::HnswFileMetadata metadata{
      .dimension = 2, .max_elements = 16, .embedding_model_hash = "test-model"};
  index.save(index_path, metadata);
  const auto loaded = vector_adapter::HnswIndex::load(index_path, metadata, options);
  EXPECT_EQ(loaded.size(), 2U);
  EXPECT_EQ(loaded.search(horizontal, 1).front().chunk_id, 1U);
  auto incompatible = metadata;
  incompatible.embedding_model_hash = "other-model";
  EXPECT_THROW(
      static_cast<void>(vector_adapter::HnswIndex::load(index_path, incompatible, options)),
      std::runtime_error);
}

TEST(HnswIndexTest, RejectsInvalidInputs) {
  EXPECT_THROW((void)vector_adapter::HnswIndex(0), std::invalid_argument);
  EXPECT_THROW((void)vector_adapter::HnswIndex(2, {.max_elements = 0}), std::invalid_argument);
  vector_adapter::HnswIndex index(2, {.max_elements = 4});
  const Embedding one_value{1.0F};
  const Embedding non_finite{1.0F, std::numeric_limits<float>::quiet_NaN()};
  EXPECT_THROW(index.upsert(1, one_value), std::invalid_argument);
  EXPECT_THROW(index.upsert(1, non_finite), std::invalid_argument);
}

TEST(HnswIndexTest, UpdatesSearchBreadth) {
  vector_adapter::HnswIndex index(2, {.max_elements = 8});
  EXPECT_NO_THROW(index.set_ef_search(128));
  EXPECT_THROW(index.set_ef_search(0), std::invalid_argument);
}

} // namespace llcl::test
