#include "adapters/vector/vector_file.hpp"

#include <filesystem>
#include <gtest/gtest.h>

namespace llcl::vector_adapter::test {

TEST(VectorFileTest, RoundTripsMetadataAndRecordsAndRejectsTruncation) {
  const auto path = std::filesystem::temp_directory_path() / "llcl-vector-file-test.bin";
  std::filesystem::remove(path);
  VectorFile::write_atomic(path, {.dimension = 2, .model_hash = "model-hash"},
                           {{.chunk_id = 7, .values = {0.6F, 0.8F}}});
  VectorFileMetadata metadata;
  const auto records = VectorFile::read(path, &metadata);
  ASSERT_EQ(records.size(), 1U);
  EXPECT_EQ(metadata.dimension, 2U);
  EXPECT_EQ(metadata.model_hash, "model-hash");
  EXPECT_EQ(records.front().chunk_id, 7U);
  EXPECT_EQ(records.front().values, (Embedding{0.6F, 0.8F}));

  std::filesystem::resize_file(path, 4U);
  EXPECT_THROW((void)VectorFile::read(path), std::runtime_error);
  std::filesystem::remove(path);
}

} // namespace llcl::vector_adapter::test
