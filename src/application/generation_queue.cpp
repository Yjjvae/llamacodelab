#include "llamacodelab/application/generation_queue.hpp"

#include <stdexcept>
#include <utility>

namespace llcl {

GenerationQueue::GenerationQueue(const std::size_t capacity) : capacity_(capacity) {
  if (capacity_ == 0) {
    throw std::invalid_argument("generation queue capacity must be positive");
  }
  worker_ = std::jthread([this](const std::stop_token stop_token) { run(stop_token); });
}

GenerationQueue::~GenerationQueue() {
  stop();
}

bool GenerationQueue::try_submit(Task task) {
  if (!task) {
    throw std::invalid_argument("generation task must not be empty");
  }
  std::scoped_lock lock(mutex_);
  if (!accepting_ || tasks_.size() >= capacity_) {
    return false;
  }
  tasks_.push(std::move(task));
  available_.notify_one();
  return true;
}

std::size_t GenerationQueue::depth() const {
  std::scoped_lock lock(mutex_);
  return tasks_.size();
}

void GenerationQueue::stop() {
  {
    std::scoped_lock lock(mutex_);
    accepting_ = false;
  }
  worker_.request_stop();
  available_.notify_all();
}

void GenerationQueue::run(const std::stop_token stop_token) {
  while (!stop_token.stop_requested()) {
    Task task;
    {
      std::unique_lock lock(mutex_);
      available_.wait(lock, stop_token, [this] { return !tasks_.empty() || !accepting_; });
      if (stop_token.stop_requested() || tasks_.empty()) {
        continue;
      }
      task = std::move(tasks_.front());
      tasks_.pop();
    }
    task(stop_token);
  }
}

} // namespace llcl
