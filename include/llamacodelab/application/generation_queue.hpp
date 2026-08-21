#pragma once

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <queue>
#include <stop_token>
#include <thread>

namespace llcl {

class GenerationQueue {
public:
  using Task = std::function<void(std::stop_token)>;

  explicit GenerationQueue(std::size_t capacity = 4);
  ~GenerationQueue();
  GenerationQueue(const GenerationQueue&) = delete;
  GenerationQueue& operator=(const GenerationQueue&) = delete;

  [[nodiscard]] bool try_submit(Task task);
  [[nodiscard]] std::size_t depth() const;
  void stop();

private:
  void run(std::stop_token stop_token);

  const std::size_t capacity_;
  mutable std::mutex mutex_;
  std::condition_variable_any available_;
  std::queue<Task> tasks_;
  bool accepting_{true};
  std::jthread worker_;
};

} // namespace llcl
