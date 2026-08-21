#include "adapters/vector/hnsw_index.hpp"

#include "llamacodelab/domain/similarity.hpp"

#include <algorithm>
#include <fstream>
#include <hnswlib/hnswlib.h>
#include <limits>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <unordered_set>
#include <utility>
#include <vector>

namespace llcl::vector_adapter {
namespace {

using Json = nlohmann::json;

[[nodiscard]] bool better(const SearchHit& lhs, const SearchHit& rhs) noexcept {
  return lhs.score > rhs.score || (lhs.score == rhs.score && lhs.chunk_id < rhs.chunk_id);
}

[[nodiscard]] std::filesystem::path metadata_path(const std::filesystem::path& index_path) {
  return index_path.string() + ".meta.json";
}

void validate_options(const HnswOptions& options) {
  if (options.max_elements == 0 || options.m == 0 || options.ef_construction == 0 ||
      options.ef_search == 0) {
    throw std::invalid_argument("all HNSW options must be positive");
  }
}

void validate_metadata(const HnswFileMetadata& metadata) {
  if (metadata.version != HnswFileMetadata::format_version || metadata.dimension == 0 ||
      metadata.max_elements == 0 || metadata.embedding_model_hash.empty()) {
    throw std::invalid_argument("invalid HNSW file metadata");
  }
}

void rename_replace(const std::filesystem::path& source, const std::filesystem::path& destination) {
  std::error_code error;
  std::filesystem::remove(destination, error);
  std::filesystem::rename(source, destination, error);
  if (error) {
    throw std::runtime_error("failed to publish HNSW index: " + error.message());
  }
}

} // namespace

class HnswIndex::Impl {
public:
  Impl(const std::size_t vector_dimension, HnswOptions index_options)
      : dimension(vector_dimension), options(index_options),
        space(std::make_unique<hnswlib::InnerProductSpace>(vector_dimension)),
        index(std::make_unique<hnswlib::HierarchicalNSW<float>>(
            space.get(), this->options.max_elements, this->options.m, this->options.ef_construction,
            100U, true)) {
    index->setEf(this->options.ef_search);
  }

  std::size_t dimension;
  HnswOptions options;
  std::unique_ptr<hnswlib::InnerProductSpace> space;
  std::unique_ptr<hnswlib::HierarchicalNSW<float>> index;
  std::unordered_set<ChunkId> active_ids;
};

HnswIndex::HnswIndex(const std::size_t dimension, HnswOptions options) {
  if (dimension == 0) {
    throw std::invalid_argument("HNSW dimension must be positive");
  }
  validate_options(options);
  impl_ = std::make_unique<Impl>(dimension, options);
}

HnswIndex::~HnswIndex() = default;
HnswIndex::HnswIndex(HnswIndex&&) noexcept = default;
HnswIndex& HnswIndex::operator=(HnswIndex&&) noexcept = default;

void HnswIndex::upsert(const ChunkId id, const std::span<const float> values) {
  if (values.size() != impl_->dimension) {
    throw std::invalid_argument("embedding dimensions do not match HNSW index");
  }
  if (!is_finite_embedding(values)) {
    throw std::invalid_argument("embedding must be finite");
  }
  if (id > static_cast<ChunkId>(std::numeric_limits<hnswlib::labeltype>::max())) {
    throw std::invalid_argument("chunk id cannot be represented by HNSW");
  }
  const auto is_new_or_reactivated = !impl_->active_ids.contains(id);
  impl_->index->addPoint(values.data(), static_cast<hnswlib::labeltype>(id), is_new_or_reactivated);
  impl_->active_ids.insert(id);
}

void HnswIndex::erase(const ChunkId id) {
  const auto found = impl_->active_ids.find(id);
  if (found == impl_->active_ids.end()) {
    return;
  }
  impl_->index->markDelete(static_cast<hnswlib::labeltype>(id));
  impl_->active_ids.erase(found);
}

std::vector<SearchHit> HnswIndex::search(const std::span<const float> query,
                                         const std::size_t top_k) const {
  if (top_k == 0 || impl_->active_ids.empty()) {
    return {};
  }
  if (query.size() != impl_->dimension) {
    throw std::invalid_argument("query dimensions do not match HNSW index");
  }
  if (!is_finite_embedding(query)) {
    throw std::invalid_argument("query must be finite");
  }
  const auto count = std::min(top_k, impl_->active_ids.size());
  auto nearest = impl_->index->searchKnn(query.data(), count);
  std::vector<SearchHit> hits;
  hits.reserve(nearest.size());
  while (!nearest.empty()) {
    const auto [distance, label] = nearest.top();
    nearest.pop();
    hits.push_back({.chunk_id = static_cast<ChunkId>(label), .score = 1.0F - distance});
  }
  std::sort(hits.begin(), hits.end(), better);
  return hits;
}

std::size_t HnswIndex::size() const noexcept {
  return impl_->active_ids.size();
}

std::size_t HnswIndex::dimension() const noexcept {
  return impl_->dimension;
}

void HnswIndex::set_ef_search(const std::size_t ef_search) {
  if (ef_search == 0) {
    throw std::invalid_argument("HNSW ef_search must be positive");
  }
  impl_->options.ef_search = ef_search;
  impl_->index->setEf(ef_search);
}

void HnswIndex::save(const std::filesystem::path& index_path,
                     const HnswFileMetadata& metadata) const {
  validate_metadata(metadata);
  if (metadata.dimension != impl_->dimension ||
      metadata.max_elements != impl_->options.max_elements) {
    throw std::invalid_argument("HNSW metadata does not match the in-memory index");
  }
  std::filesystem::create_directories(index_path.parent_path());
  const auto index_temporary = index_path.string() + ".tmp";
  const auto metadata_file = metadata_path(index_path);
  const auto metadata_temporary = metadata_file.string() + ".tmp";
  impl_->index->saveIndex(index_temporary);
  std::vector<ChunkId> active_ids(impl_->active_ids.begin(), impl_->active_ids.end());
  std::sort(active_ids.begin(), active_ids.end());
  {
    std::ofstream output(metadata_temporary, std::ios::binary | std::ios::trunc);
    if (!output) {
      throw std::runtime_error("failed to create HNSW metadata file");
    }
    output << Json{{"format_version", metadata.version},
                   {"dimension", metadata.dimension},
                   {"max_elements", metadata.max_elements},
                   {"embedding_model_hash", metadata.embedding_model_hash},
                   {"active_ids", active_ids}}
                  .dump();
    if (!output) {
      throw std::runtime_error("failed to write HNSW metadata file");
    }
  }
  rename_replace(index_temporary, index_path);
  rename_replace(metadata_temporary, metadata_file);
}

HnswIndex HnswIndex::load(const std::filesystem::path& index_path,
                          const HnswFileMetadata& expected_metadata, HnswOptions options) {
  validate_metadata(expected_metadata);
  validate_options(options);
  if (expected_metadata.max_elements != options.max_elements) {
    throw std::invalid_argument("HNSW options max_elements does not match expected metadata");
  }
  std::ifstream input(metadata_path(index_path), std::ios::binary);
  if (!input) {
    throw std::runtime_error("HNSW metadata file does not exist");
  }
  const auto stored = Json::parse(input);
  const HnswFileMetadata actual{.version = stored.at("format_version").get<std::uint32_t>(),
                                .dimension = stored.at("dimension").get<std::size_t>(),
                                .max_elements = stored.at("max_elements").get<std::size_t>(),
                                .embedding_model_hash =
                                    stored.at("embedding_model_hash").get<std::string>()};
  validate_metadata(actual);
  if (actual.version != expected_metadata.version ||
      actual.dimension != expected_metadata.dimension ||
      actual.max_elements != expected_metadata.max_elements ||
      actual.embedding_model_hash != expected_metadata.embedding_model_hash) {
    throw std::runtime_error("HNSW index metadata does not match the expected vector space");
  }
  HnswIndex loaded(actual.dimension, options);
  loaded.impl_->index = std::make_unique<hnswlib::HierarchicalNSW<float>>(
      loaded.impl_->space.get(), index_path.string(), false, actual.max_elements, true);
  loaded.impl_->index->setEf(loaded.impl_->options.ef_search);
  for (const auto& id : stored.at("active_ids")) {
    loaded.impl_->active_ids.insert(id.get<ChunkId>());
  }
  return loaded;
}

} // namespace llcl::vector_adapter
