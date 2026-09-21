#include "thread_pool.hpp"

#include <stdexcept>
#include <utility>

ThreadPool::ThreadPool(std::size_t num_threads) {
  if (num_threads == 0) {
    num_threads = std::thread::hardware_concurrency();
  }
  if (num_threads == 0) {
    num_threads = 4;
  }

  workers_.reserve(num_threads);
  try {
    for (std::size_t i = 0; i < num_threads; ++i) {
      workers_.emplace_back([this] { worker_loop(); });
    }
  } catch (...) {
    shutdown();
    throw;
  }
}

ThreadPool::~ThreadPool() { shutdown(); }

void ThreadPool::worker_loop() {
  for (;;) {
    std::function<void()> task;
    {
      std::unique_lock<std::mutex> lock(mutex_);
      cv_.wait(lock, [this] { return stop_ || !tasks_.empty(); });
      if (stop_ && tasks_.empty()) {
        return;
      }
      task = std::move(tasks_.front());
      tasks_.pop();
    }
    task();
  }
}

void ThreadPool::enqueue(std::function<void()> task) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (stop_) {
      throw std::runtime_error("enqueue on stopped ThreadPool");
    }
    tasks_.push(std::move(task));
  }
  cv_.notify_one();
}

void ThreadPool::shutdown() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (stop_) {
      return;
    }
    stop_ = true;
  }
  cv_.notify_all();
  for (auto& worker : workers_) {
    if (worker.joinable()) {
      worker.join();
    }
  }
}

std::size_t ThreadPool::thread_count() const { return workers_.size(); }

bool ThreadPool::is_shutdown() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return stop_;
}
