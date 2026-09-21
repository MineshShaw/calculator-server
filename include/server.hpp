#pragma once

#include <atomic>
#include <cstddef>
#include <memory>

#include "thread_pool.hpp"

class Server {
 public:
  explicit Server(int port = 8080, std::size_t thread_count = 0);
  ~Server();

  Server(const Server&) = delete;
  Server& operator=(const Server&) = delete;

  void run();
  void stop();

  int port() const { return port_; }

 private:
  static void handle_client(int client_fd);
  static bool write_all(int fd, const char* data, std::size_t len);

  int port_;
  int listen_fd_ = -1;
  std::atomic<bool> running_{false};
  std::unique_ptr<ThreadPool> pool_;
};
