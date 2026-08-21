#include "llamacodelab/application/generation_queue.hpp"

#include <atomic>
#include <chrono>
#include <future>
#include <gtest/gtest.h>

namespace llcl::test {

TEST(GenerationQueueTest, RejectsWorkBeyondItsBoundedCapacity) {
  GenerationQueue queue(4);
  std::promise<void> started;
  std::atomic_bool release{false};
  ASSERT_TRUE(queue.try_submit([&](const std::stop_token stop_token) {
    started.set_value();
    while (!release.load() && !stop_token.stop_requested()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  }));
  ASSERT_EQ(started.get_future().wait_for(std::chrono::seconds(1)), std::future_status::ready);
  for (int index = 0; index < 4; ++index) {
    EXPECT_TRUE(queue.try_submit([](std::stop_token) {}));
  }
  EXPECT_FALSE(queue.try_submit([](std::stop_token) {}));
  EXPECT_EQ(queue.depth(), 4U);
  release.store(true);
}

} // namespace llcl::test
