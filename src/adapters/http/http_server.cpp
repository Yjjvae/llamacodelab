#include "llamacodelab/http/http_server.hpp"

#include <atomic>
#include <chrono>
#include <future>
#include <httplib.h>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace llcl::http_adapter {
namespace {

using Json = nlohmann::json;

[[nodiscard]] std::string request_id() {
  static std::atomic_uint64_t next{1};
  return "llcl-" + std::to_string(next.fetch_add(1, std::memory_order_relaxed));
}

void json_response(httplib::Response& response, const int status, const Json& body,
                   const std::string& id) {
  response.status = status;
  response.set_header("X-Request-Id", id);
  response.set_content(body.dump(), "application/json");
}

void error_response(httplib::Response& response, const int status, const std::string& code,
                    const std::string& message, const std::string& id) {
  json_response(response, status,
                {{"error", {{"code", code}, {"message", message}, {"request_id", id}}}}, id);
}

[[nodiscard]] std::string sse_event(const std::string_view name, const Json& payload) {
  return "event: " + std::string(name) + "\ndata: " + payload.dump() + "\n\n";
}

[[nodiscard]] std::size_t top_k_from(const Json& request) {
  const auto top_k = request.value("top_k", 5U);
  if (top_k == 0 || top_k > 1'000) {
    throw std::invalid_argument("top_k must be in [1, 1000]");
  }
  return top_k;
}

[[nodiscard]] std::string question_from(const Json& request) {
  if (request.contains("question") && request.at("question").is_string()) {
    return request.at("question").get<std::string>();
  }
  if (request.contains("messages") && request.at("messages").is_array()) {
    for (auto iterator = request.at("messages").rbegin(); iterator != request.at("messages").rend();
         ++iterator) {
      if (iterator->value("role", "") == "user" && iterator->contains("content") &&
          iterator->at("content").is_string()) {
        return iterator->at("content").get<std::string>();
      }
    }
  }
  throw std::invalid_argument("request must provide question or a user message");
}

[[nodiscard]] Json citations_json(const std::vector<Citation>& citations) {
  Json items = Json::array();
  for (const auto& citation : citations) {
    items.push_back({{"id", citation.source_id},
                     {"path", citation.source.path.generic_string()},
                     {"start_line", citation.source.start_line},
                     {"end_line", citation.source.end_line},
                     {"score", citation.retrieval_score}});
  }
  return items;
}

} // namespace

HttpServer::HttpServer(HttpCallbacks callbacks, GenerationQueue& generation_queue)
    : callbacks_(std::move(callbacks)), generation_queue_(generation_queue),
      server_(std::make_unique<httplib::Server>()) {
  server_->Get("/healthz", [](const httplib::Request&, httplib::Response& response) {
    const auto id = request_id();
    json_response(response, 200, {{"status", "ok"}}, id);
  });
  server_->Get("/readyz", [this](const httplib::Request&, httplib::Response& response) {
    const auto id = request_id();
    if (!callbacks_.ready()) {
      error_response(response, 503, "not_ready", "model or index is not ready", id);
      return;
    }
    json_response(response, 200, {{"status", "ready"}}, id);
  });
  server_->Get("/v1/models", [this](const httplib::Request&, httplib::Response& response) {
    const auto id = request_id();
    json_response(response, 200,
                  {{"object", "list"},
                   {"data", Json::array({{{"id", callbacks_.model_name()}, {"object", "model"}}})}},
                  id);
  });
  server_->Get("/metrics", [this](const httplib::Request&, httplib::Response& response) {
    const auto id = request_id();
    std::ostringstream body;
    body << "# TYPE llcl_http_requests_total counter\n"
         << "llcl_http_requests_total 0\n"
         << "# TYPE llcl_http_request_duration_seconds gauge\n"
         << "llcl_http_request_duration_seconds 0\n"
         << "# TYPE llcl_generation_queue_depth gauge\n"
         << "llcl_generation_queue_depth " << generation_queue_.depth() << "\n"
         << "# TYPE llcl_generation_ttft_seconds gauge\n"
         << "llcl_generation_ttft_seconds 0\n"
         << "# TYPE llcl_generation_decode_tokens_total counter\n"
         << "llcl_generation_decode_tokens_total 0\n"
         << "# TYPE llcl_generation_decode_seconds gauge\n"
         << "llcl_generation_decode_seconds 0\n"
         << "# TYPE llcl_retrieval_duration_seconds gauge\n"
         << "llcl_retrieval_duration_seconds 0\n"
         << "# TYPE llcl_index_chunks gauge\n"
         << "llcl_index_chunks 0\n"
         << "# TYPE llcl_index_failures_total counter\n"
         << "llcl_index_failures_total 0\n";
    response.status = 200;
    response.set_header("X-Request-Id", id);
    response.set_content(body.str(), "text/plain; version=0.0.4");
  });
  server_->Post("/v1/index", [this](const httplib::Request&, httplib::Response& response) {
    const auto id = request_id();
    try {
      const auto result = callbacks_.update_index();
      json_response(response, 200,
                    {{"generation", result.generation},
                     {"files_added", result.files_added},
                     {"files_changed", result.files_changed},
                     {"files_removed", result.files_removed},
                     {"files_unchanged", result.files_unchanged},
                     {"chunks_embedded", result.embedded_chunks}},
                    id);
    } catch (const std::exception& exception) {
      error_response(response, 500, "index_failed", exception.what(), id);
    }
  });
  server_->Post("/v1/search", [this](const httplib::Request& request, httplib::Response& response) {
    const auto id = request_id();
    try {
      if (!callbacks_.ready()) {
        error_response(response, 503, "index_not_ready", "repository index has not been built", id);
        return;
      }
      const auto body = Json::parse(request.body);
      const auto result = callbacks_.search(question_from(body), top_k_from(body));
      Json items = Json::array();
      for (std::size_t index = 0; index < result.hits.size() && index < result.chunks.size();
           ++index) {
        const auto& chunk = result.chunks[index];
        items.push_back({{"score", result.hits[index].score},
                         {"path", chunk.source.path.generic_string()},
                         {"start_line", chunk.source.start_line},
                         {"end_line", chunk.source.end_line},
                         {"content", chunk.content}});
      }
      json_response(response, 200, {{"data", std::move(items)}}, id);
    } catch (const Json::exception& exception) {
      error_response(response, 400, "invalid_json", exception.what(), id);
    } catch (const std::invalid_argument& exception) {
      error_response(response, 400, "invalid_request", exception.what(), id);
    } catch (const std::exception& exception) {
      error_response(response, 500, "search_failed", exception.what(), id);
    }
  });
  server_->Post("/v1/chat/completions", [this](const httplib::Request& request,
                                               httplib::Response& response) {
    const auto id = request_id();
    if (!callbacks_.ready()) {
      error_response(response, 503, "index_not_ready", "repository index has not been built", id);
      return;
    }
    try {
      const auto body = Json::parse(request.body);
      const auto question = question_from(body);
      const auto top_k = top_k_from(body);
      const auto stream = body.value("stream", false);
      if (stream) {
        response.set_header("X-Request-Id", id);
        response.set_chunked_content_provider(
            "text/event-stream", [this, question, top_k](std::size_t, httplib::DataSink& sink) {
              auto completion = std::make_shared<std::promise<AskResult>>();
              auto result = completion->get_future();
              auto client_stop = std::make_shared<std::stop_source>();
              const auto accepted =
                  generation_queue_.try_submit([this, question, top_k, &sink, completion,
                                                client_stop](std::stop_token worker_stop) {
                    try {
                      std::stop_callback bridge(worker_stop,
                                                [client_stop] { client_stop->request_stop(); });
                      completion->set_value(callbacks_.ask(
                          question, top_k,
                          [&sink, client_stop](const std::string_view token) {
                            if (!sink.is_writable()) {
                              client_stop->request_stop();
                              return;
                            }
                            const auto event = sse_event("token", {{"text", token}});
                            sink.write(event.data(), event.size());
                          },
                          client_stop->get_token()));
                    } catch (...) {
                      completion->set_exception(std::current_exception());
                    }
                  });
              if (!accepted) {
                const auto error = sse_event("error", {{"code", "queue_full"}});
                sink.write(error.data(), error.size());
                sink.done();
                return true;
              }
              try {
                const auto ask_result = result.get();
                const auto citations =
                    sse_event("citations", {{"items", citations_json(ask_result.citations)}});
                sink.write(citations.data(), citations.size());
                const auto metrics = sse_event(
                    "metrics", {{"ttft_ms", ask_result.generation.time_to_first_token.count()},
                                {"decode_tokens", ask_result.generation.generated_tokens}});
                sink.write(metrics.data(), metrics.size());
                const auto done = sse_event("done", {{"finish_reason", "stop"}});
                sink.write(done.data(), done.size());
              } catch (const std::exception& exception) {
                const auto error = sse_event(
                    "error", {{"code", "generation_failed"}, {"message", exception.what()}});
                sink.write(error.data(), error.size());
              }
              sink.done();
              return true;
            });
        return;
      }
      auto completion = std::make_shared<std::promise<AskResult>>();
      auto result = completion->get_future();
      const auto accepted = generation_queue_.try_submit(
          [this, question, top_k, completion](std::stop_token stop_token) {
            try {
              completion->set_value(
                  callbacks_.ask(question, top_k, [](std::string_view) {}, stop_token));
            } catch (...) {
              completion->set_exception(std::current_exception());
            }
          });
      if (!accepted) {
        error_response(response, 429, "queue_full", "generation queue is full", id);
        return;
      }
      const auto ask_result = result.get();
      json_response(
          response, 200,
          {{"id", id},
           {"object", "chat.completion"},
           {"model", callbacks_.model_name()},
           {"choices",
            Json::array({{{"index", 0},
                          {"message", {{"role", "assistant"}, {"content", ask_result.answer}}},
                          {"finish_reason", "stop"}}})},
           {"citations", citations_json(ask_result.citations)}},
          id);
    } catch (const Json::exception& exception) {
      error_response(response, 400, "invalid_json", exception.what(), id);
    } catch (const std::invalid_argument& exception) {
      error_response(response, 400, "invalid_request", exception.what(), id);
    } catch (const std::exception& exception) {
      error_response(response, 500, "generation_failed", exception.what(), id);
    }
  });
}

HttpServer::~HttpServer() = default;

bool HttpServer::listen(const std::string& host, const int port) {
  return server_->listen(host, port);
}

void HttpServer::stop() {
  server_->stop();
}

} // namespace llcl::http_adapter
