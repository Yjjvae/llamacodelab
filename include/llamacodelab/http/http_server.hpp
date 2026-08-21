#pragma once

#include "llamacodelab/application/ask_service.hpp"
#include "llamacodelab/application/generation_queue.hpp"
#include "llamacodelab/application/index_service.hpp"
#include "llamacodelab/domain/retrieval.hpp"

#include <functional>
#include <memory>
#include <stop_token>
#include <string>
#include <vector>

namespace httplib {
class Server;
}

namespace llcl::http_adapter {

struct SearchResult {
  std::vector<SearchHit> hits;
  std::vector<Chunk> chunks;
};

struct HttpCallbacks {
  std::function<bool()> ready;
  std::function<std::string()> model_name;
  std::function<IndexUpdateResult()> update_index;
  std::function<SearchResult(std::string_view, std::size_t)> search;
  std::function<AskResult(std::string_view, std::size_t, const TokenCallback&, std::stop_token)>
      ask;
};

class HttpServer {
public:
  HttpServer(HttpCallbacks callbacks, GenerationQueue& generation_queue);
  ~HttpServer();
  HttpServer(const HttpServer&) = delete;
  HttpServer& operator=(const HttpServer&) = delete;

  [[nodiscard]] bool listen(const std::string& host, int port);
  void stop();

private:
  HttpCallbacks callbacks_;
  GenerationQueue& generation_queue_;
  std::unique_ptr<httplib::Server> server_;
};

} // namespace llcl::http_adapter
