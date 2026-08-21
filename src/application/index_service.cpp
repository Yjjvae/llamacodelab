#include "llamacodelab/application/index_service.hpp"

#include "adapters/filesystem/file_scanner.hpp"
#include "adapters/filesystem/text_chunker.hpp"
#include "adapters/sqlite/sqlite_chunk_repository.hpp"
#include "adapters/sqlite/sqlite_symbol_repository.hpp"
#include "adapters/vector/brute_force_index.hpp"
#include "adapters/vector/hnsw_index.hpp"
#include "adapters/vector/vector_file.hpp"
#include "llamacodelab/support/hash.hpp"

#ifdef LLCL_HAS_CLANG
#include "adapters/clang/clang_code_parser.hpp"
#endif

#include <chrono>
#include <fstream>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace llcl {
namespace {

[[nodiscard]] std::string content_hash(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    throw std::runtime_error("cannot read indexed file: " + path.string());
  }
  std::string content{std::istreambuf_iterator<char>(input), {}};
  return stable_hash_hex(content);
}

[[nodiscard]] std::int64_t modified_ns(const std::filesystem::path& path) {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(
             std::filesystem::last_write_time(path).time_since_epoch())
      .count();
}

[[nodiscard]] std::uint64_t generation_of(const std::string& value) {
  if (value.empty()) {
    return 0;
  }
  try {
    return std::stoull(value);
  } catch (const std::exception&) {
    throw std::runtime_error("invalid active_vector_generation metadata");
  }
}

} // namespace

IndexSnapshot SearchIndexHandle::load() const noexcept {
  return current_.load(std::memory_order_acquire);
}
void SearchIndexHandle::publish(IndexSnapshot next) noexcept {
  current_.store(std::move(next), std::memory_order_release);
}

IndexService::IndexService(IEmbedder& embedder, SearchIndexHandle& index_handle, IndexConfig config,
                           std::string embedding_model_id)
    : embedder_(embedder), index_handle_(index_handle), config_(std::move(config)),
      embedding_model_id_(std::move(embedding_model_id)) {
  if (embedding_model_id_.empty()) {
    throw std::invalid_argument("embedding model id must not be empty");
  }
}

IndexUpdateResult IndexService::update(const std::filesystem::path& repository_root) {
  std::filesystem::create_directories(config_.data_dir);
  for (const auto& entry : std::filesystem::directory_iterator(config_.data_dir)) {
    if (entry.path().extension() == ".tmp") {
      std::filesystem::remove(entry.path());
    }
  }
  sqlite_adapter::SqliteChunkRepository repository(config_.data_dir / "index.sqlite3");
#ifdef LLCL_HAS_CLANG
  std::optional<sqlite_adapter::SqliteSymbolRepository> symbol_repository;
  if (config_.semantic_index_enabled) {
    symbol_repository.emplace(config_.data_dir / "symbols.sqlite3");
  }
#else
  if (config_.semantic_index_enabled) {
    spdlog::warn(
        "semantic indexing requested but this build has no Clang adapter; using text chunks");
  }
#endif
  const auto model_hash = stable_hash_hex(embedding_model_id_);
  const auto existing_model_hash = repository.metadata("embedding_model_sha256");
  if (!existing_model_hash.empty() && existing_model_hash != model_hash) {
    throw std::runtime_error(
        "embedding model changed; rebuild the persistent index before loading it");
  }
  const auto existing_dimension = repository.metadata("embedding_dimension");
  if (!existing_dimension.empty() && existing_dimension != std::to_string(embedder_.dimension())) {
    throw std::runtime_error(
        "embedding dimension changed; rebuild the persistent index before loading it");
  }

  const auto old_generation = generation_of(repository.metadata("active_vector_generation"));
  std::unordered_map<ChunkId, Embedding> vectors;
  if (old_generation != 0) {
    vector_adapter::VectorFileMetadata metadata;
    for (auto& vector : vector_adapter::VectorFile::read(
             config_.data_dir / ("vectors." + std::to_string(old_generation) + ".bin"),
             &metadata)) {
      vectors.emplace(vector.chunk_id, std::move(vector.values));
    }
    if (metadata.dimension != embedder_.dimension() || metadata.model_hash != model_hash) {
      throw std::runtime_error("persistent vector metadata does not match embedding model");
    }
  }

  const auto scan = filesystem_adapter::FileScanner{}.scan(
      repository_root,
      {.max_file_bytes = config_.max_file_bytes, .include_globs = {}, .exclude_globs = {}});
  std::unordered_map<std::string, sqlite_adapter::StoredDocument> old_documents;
  for (auto& document : repository.documents()) {
    old_documents.emplace(document.relative_path.generic_string(), std::move(document));
  }
  std::unordered_set<std::string> present;
  IndexUpdateResult result;
  std::vector<sqlite_adapter::StoredDocument> replacements;
#ifdef LLCL_HAS_CLANG
  struct SymbolReplacement {
    std::filesystem::path relative_path;
    std::vector<Symbol> symbols;
    std::vector<SymbolEdge> edges;
  };
  std::vector<SymbolReplacement> symbol_replacements;
#endif
  filesystem_adapter::TextChunker chunker;
  const filesystem_adapter::ChunkingOptions chunking{.max_lines = config_.chunk_lines,
                                                     .overlap_lines = config_.overlap_lines};
  for (const auto& file : scan.files) {
    const auto path = file.relative_path.generic_string();
    present.insert(path);
    const auto hash = content_hash(file.absolute_path);
    const auto previous = old_documents.find(path);
    if (previous != old_documents.end() && previous->second.content_hash == hash) {
      ++result.files_unchanged;
      continue;
    }
    if (previous == old_documents.end()) {
      ++result.files_added;
    } else {
      ++result.files_changed;
      for (const auto& chunk : repository.chunks_for_document(file.relative_path)) {
        vectors.erase(chunk.id);
      }
    }
    auto chunks = chunker.chunk_file(file, chunking);
    std::string parser_version{"text-v1"};
#ifdef LLCL_HAS_CLANG
    if (symbol_repository.has_value() && file.language == "cpp") {
      const auto database_directory = config_.compilation_database_dir.is_absolute()
                                          ? config_.compilation_database_dir
                                          : repository_root / config_.compilation_database_dir;
      const clang_adapter::ClangCodeParser parser(database_directory);
      auto parsed = parser.parse(file.absolute_path, repository_root);
      if (parsed.success && !parsed.semantic_chunks.empty()) {
        chunks = std::move(parsed.semantic_chunks);
        parser_version = "clang-ast-v1";
        symbol_replacements.push_back({.relative_path = file.relative_path,
                                       .symbols = std::move(parsed.symbols),
                                       .edges = std::move(parsed.edges)});
      } else {
        const auto diagnostic =
            parsed.diagnostics.empty() ? "no semantic chunks produced" : parsed.diagnostics.front();
        spdlog::warn("AST parse fallback for {}: {}", file.relative_path.string(), diagnostic);
        symbol_replacements.push_back(
            {.relative_path = file.relative_path, .symbols = {}, .edges = {}});
      }
    }
#endif
    std::vector<std::string_view> texts;
    texts.reserve(chunks.size());
    for (const auto& chunk : chunks) {
      texts.push_back(chunk.content);
    }
    const auto embedded = embedder_.embed_batch(texts, EmbeddingKind::document);
    for (std::size_t index = 0; index < chunks.size(); ++index) {
      vectors.insert_or_assign(chunks[index].id, embedded[index]);
    }
    result.embedded_chunks += chunks.size();
    replacements.push_back({.relative_path = file.relative_path,
                            .content_hash = hash,
                            .size_bytes = file.size_bytes,
                            .modified_ns = modified_ns(file.absolute_path),
                            .parser_version = std::move(parser_version),
                            .chunks = std::move(chunks)});
  }
  std::vector<std::filesystem::path> removals;
  for (const auto& [path, _] : old_documents) {
    if (!present.contains(path)) {
      ++result.files_removed;
      const std::filesystem::path relative_path{path};
      for (const auto& chunk : repository.chunks_for_document(relative_path)) {
        vectors.erase(chunk.id);
      }
      removals.push_back(relative_path);
    }
  }
  if (replacements.empty() && removals.empty() && old_generation != 0) {
    result.generation = old_generation;
  } else {
    result.generation = old_generation + 1;
    std::vector<vector_adapter::StoredVector> stored;
    stored.reserve(vectors.size());
    for (const auto& [id, values] : vectors) {
      stored.push_back({.chunk_id = id, .values = values});
    }
    const auto vector_path =
        config_.data_dir / ("vectors." + std::to_string(result.generation) + ".bin");
    vector_adapter::VectorFile::write_atomic(
        vector_path,
        {.dimension = static_cast<std::uint32_t>(embedder_.dimension()), .model_hash = model_hash},
        stored);
    (void)vector_adapter::VectorFile::read(vector_path);
    repository.begin();
    try {
      for (const auto& relative_path : removals) {
        repository.erase_document(relative_path);
      }
      for (const auto& replacement : replacements) {
        repository.replace_document(replacement, result.generation);
      }
      repository.set_metadata("chunker_version", "text-v1");
      repository.set_metadata("embedding_model_id", embedding_model_id_);
      repository.set_metadata("embedding_model_sha256", model_hash);
      repository.set_metadata("embedding_dimension", std::to_string(embedder_.dimension()));
      repository.set_metadata("normalization", "l2");
      repository.set_metadata("active_vector_generation", std::to_string(result.generation));
      repository.commit();
    } catch (...) {
      repository.rollback();
      throw;
    }
#ifdef LLCL_HAS_CLANG
    if (symbol_repository.has_value()) {
      for (const auto& relative_path : removals) {
        symbol_repository->replace_file(relative_path, {}, {});
      }
      for (const auto& replacement : symbol_replacements) {
        symbol_repository->replace_file(replacement.relative_path, replacement.symbols,
                                        replacement.edges);
      }
    }
#endif
  }
  std::shared_ptr<IVectorIndex> snapshot;
  if (config_.hnsw_enabled) {
    snapshot = std::make_shared<vector_adapter::HnswIndex>(
        embedder_.dimension(),
        vector_adapter::HnswOptions{.max_elements = std::max<std::size_t>(1, vectors.size()),
                                    .m = 16,
                                    .ef_construction = 200,
                                    .ef_search = config_.hnsw_ef_search});
  } else {
    snapshot = std::make_shared<vector_adapter::BruteForceIndex>(embedder_.dimension());
  }
  for (const auto& [id, values] : vectors) {
    snapshot->upsert(id, values);
  }
  index_handle_.publish(std::move(snapshot));
  return result;
}

} // namespace llcl
