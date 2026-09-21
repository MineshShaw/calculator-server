#include "thread_pool.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <mutex>
#include <set>
#include <stdexcept>
#include <thread>
#include <vector>

TEST(ThreadPool, ConstructsRequestedWorkers) {
  ThreadPool pool(4);
  EXPECT_EQ(pool.thread_count(), 4u);
  EXPECT_FALSE(pool.is_shutdown());
}

TEST(ThreadPool, ExecutesAllTasks) {
  constexpr int kTasks = 1000;
  std::atomic<int> counter{0};
  {
    ThreadPool pool(8);
    for (int i = 0; i < kTasks; ++i) {
      pool.enqueue([&counter] { counter.fetch_add(1, std::memory_order_relaxed); });
    }
  }
  EXPECT_EQ(counter.load(), kTasks);
}

TEST(ThreadPool, TasksRunConcurrentlyOnMultipleThreads) {
  ThreadPool pool(4);
  std::mutex mu;
  std::set<std::thread::id> ids;
  std::atomic<int> started{0};
  std::atomic<int> released{0};

  for (int i = 0; i < 4; ++i) {
    pool.enqueue([&] {
      started.fetch_add(1);
      while (started.load() < 4) {
        std::this_thread::yield();
      }
      {
        std::lock_guard<std::mutex> lock(mu);
        ids.insert(std::this_thread::get_id());
      }
      released.fetch_add(1);
    });
  }

  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (released.load() < 4 && std::chrono::steady_clock::now() < deadline) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  EXPECT_EQ(released.load(), 4);
  EXPECT_GE(ids.size(), 2u);
}

TEST(ThreadPool, ShutdownIsIdempotentAndClean) {
  ThreadPool pool(2);
  pool.enqueue([] {});
  pool.shutdown();
  EXPECT_TRUE(pool.is_shutdown());
  pool.shutdown();
  EXPECT_THROW(pool.enqueue([] {}), std::runtime_error);
}

TEST(ThreadPool, DestructorDrainsQueue) {
  std::atomic<int> n{0};
  {
    ThreadPool pool(2);
    for (int i = 0; i < 50; ++i) {
      pool.enqueue([&n] {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        n.fetch_add(1);
      });
    }
  }
  EXPECT_EQ(n.load(), 50);
}
