#include "adapters/llama/llama_embedder.hpp"
#include "adapters/llama/llama_generator.hpp"
#include "adapters/llama/llama_runtime.hpp"
#include "adapters/sqlite/fts_search.hpp"
#include "adapters/sqlite/sqlite_chunk_repository.hpp"
#include "llamacodelab/application/ask_service.hpp"
#include "llamacodelab/application/context_budget.hpp"
#include "llamacodelab/application/generation_queue.hpp"
#include "llamacodelab/application/hybrid_retriever.hpp"
#include "llamacodelab/application/index_service.hpp"
#include "llamacodelab/http/http_server.hpp"
#include "llamacodelab/support/config.hpp"
#include "llamacodelab/support/logging.hpp"

#include <CLI/CLI.hpp>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

int main(int argc, char** argv) {
  CLI::App app{"LlamaCodeLab HTTP/SSE server", "llcl-server"};
  std::string config_path = "configs/default.json";
  std::string repository;
  int port = 8080;
  app.add_option("--config", config_path, "JSON configuration file")->check(CLI::ExistingFile);
  app.add_option("--repo", repository, "Repository root to index and serve")
      ->required()
      ->check(CLI::ExistingDirectory);
  app.add_option("--port", port, "TCP port")->check(CLI::Range(1, 65535));
  CLI11_PARSE(app, argc, argv);

  try {
    const auto config = llcl::load_config(config_path);
    llcl::configure_logging(config.log_level);
    llcl::llama_adapter::LlamaRuntime runtime;
    llcl::llama_adapter::LlamaGenerator generator(runtime, config.generation_model);
    llcl::llama_adapter::LlamaEmbedder embedder(runtime, config.embedding_model);
    llcl::SearchIndexHandle index_handle;
    llcl::IndexService index_service(embedder, index_handle, config.index,
                                     config.embedding_model.path.filename().string());
    llcl::sqlite_adapter::SqliteChunkRepository chunk_repository(config.index.data_dir /
                                                                 "index.sqlite3");
    llcl::sqlite_adapter::FtsSearch keyword_search(config.index.data_dir / "index.sqlite3");
    llcl::GenerationQueue generation_queue(4);
    (void)index_service.update(repository);

    const auto ask = [&]() -> llcl::http_adapter::HttpCallbacks {
      return {
          .ready = [&] { return index_handle.load() != nullptr && chunk_repository.healthy(); },
          .model_name = [&] { return config.generation_model.path.filename().string(); },
          .update_index = [&] { return index_service.update(repository); },
          .search =
              [&](const std::string_view question, const std::size_t top_k) {
                const auto snapshot = index_handle.load();
                if (snapshot == nullptr) {
                  throw std::runtime_error("repository index has not been built");
                }
                llcl::HybridRetriever retriever(embedder, *snapshot, keyword_search);
                const auto hits = retriever.retrieve(question, top_k);
                std::vector<llcl::ChunkId> ids;
                ids.reserve(hits.size());
                for (const auto& hit : hits) {
                  ids.push_back(hit.chunk_id);
                }
                return llcl::http_adapter::SearchResult{.hits = hits,
                                                        .chunks = chunk_repository.get_many(ids)};
              },
          .ask =
              [&](const std::string_view question, const std::size_t top_k,
                  const llcl::TokenCallback& on_token, const std::stop_token stop_token) {
                const auto snapshot = index_handle.load();
                if (snapshot == nullptr) {
                  throw std::runtime_error("repository index has not been built");
                }
                llcl::ContextBudget context_budget;
                llcl::RagPromptBudget budget{.model_context = config.generation_model.context_size,
                                             .reserved_output_tokens = 512,
                                             .safety_margin_tokens = 64};
                llcl::GenerationOptions options{.max_tokens = 512};
                llcl::HybridRetriever retriever(embedder, *snapshot, keyword_search);
                llcl::AskService service(retriever, chunk_repository, context_budget, generator,
                                         options, budget);
                return service.ask(question, top_k, on_token, stop_token);
              },
      };
    }();
    llcl::http_adapter::HttpServer server(ask, generation_queue);
    std::cout << "llcl-server listening on http://127.0.0.1:" << port << '\n';
    return server.listen("127.0.0.1", port) ? 0 : 1;
  } catch (const std::exception& exception) {
    std::cerr << "error: " << exception.what() << '\n';
    return 1;
  }
}
