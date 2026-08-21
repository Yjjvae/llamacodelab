#include "llamacodelab/http/http_server.hpp"

#include <chrono>
#include <gtest/gtest.h>
#include <httplib.h>
#include <thread>

namespace llcl::test {
namespace {

class RunningServer {
public:
  explicit RunningServer(const bool ready = false)
      : server(callbacks(ready), queue),
        thread([this] { (void)server.listen("127.0.0.1", 18080); }) {
    httplib::Client client("127.0.0.1", 18080);
    for (int attempt = 0; attempt < 100; ++attempt) {
      if (client.Get("/healthz")) {
        return;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    throw std::runtime_error("test HTTP server did not start");
  }
  ~RunningServer() {
    server.stop();
    thread.join();
  }

  static http_adapter::HttpCallbacks callbacks(const bool ready) {
    return {
        .ready = [ready] { return ready; },
        .model_name = [] { return "fake-model"; },
        .update_index = [] { return IndexUpdateResult{.files_added = 2, .generation = 1}; },
        .search = [](std::string_view, std::size_t) { return http_adapter::SearchResult{}; },
        .ask =
            [](std::string_view, std::size_t, const TokenCallback& on_token, std::stop_token) {
              on_token("hello");
              return AskResult{
                  .answer = "hello", .citations = {}, .generation = {.generated_tokens = 1}};
            },
    };
  }

  GenerationQueue queue{4};
  http_adapter::HttpServer server;
  std::jthread thread;
};

} // namespace

TEST(HttpApiTest, HealthIsLiveWhileReadinessReportsUnavailableIndex) {
  RunningServer running;
  httplib::Client client("127.0.0.1", 18080);
  const auto health = client.Get("/healthz");
  ASSERT_TRUE(health);
  EXPECT_EQ(health->status, 200);
  EXPECT_TRUE(health->has_header("X-Request-Id"));
  const auto ready = client.Get("/readyz");
  ASSERT_TRUE(ready);
  EXPECT_EQ(ready->status, 503);
  EXPECT_NE(ready->body.find("not_ready"), std::string::npos);
  const auto models = client.Get("/v1/models");
  ASSERT_TRUE(models);
  EXPECT_EQ(models->status, 200);
  EXPECT_NE(models->body.find("fake-model"), std::string::npos);
}

TEST(HttpApiTest, ChatSupportsJsonAndServerSentEvents) {
  RunningServer running(true);
  httplib::Client client("127.0.0.1", 18080);
  const auto json =
      client.Post("/v1/chat/completions", R"({"question":"hello"})", "application/json");
  ASSERT_TRUE(json);
  EXPECT_EQ(json->status, 200);
  EXPECT_NE(json->body.find("hello"), std::string::npos);
  EXPECT_TRUE(json->has_header("X-Request-Id"));
  const auto stream = client.Post("/v1/chat/completions", R"({"question":"hello","stream":true})",
                                  "application/json");
  ASSERT_TRUE(stream);
  EXPECT_EQ(stream->status, 200);
  EXPECT_NE(stream->body.find("event: token"), std::string::npos);
  EXPECT_NE(stream->body.find("event: done"), std::string::npos);
}

} // namespace llcl::test
