#include "server.hpp"

#include "http_parser.hpp"
#include "router.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr int kBacklog = 512;
constexpr std::size_t kReadChunk = 4096;

int close_fd(int& fd) {
  if (fd >= 0) {
    const int rc = ::close(fd);
    fd = -1;
    return rc;
  }
  return 0;
}

}  // namespace

Server::Server(int port, std::size_t thread_count) : port_(port) {
  pool_ = std::make_unique<ThreadPool>(thread_count);
}

Server::~Server() { stop(); }

void Server::stop() {
  running_.store(false);
  if (listen_fd_ >= 0) {
    ::shutdown(listen_fd_, SHUT_RDWR);
    close_fd(listen_fd_);
  }
}

bool Server::write_all(int fd, const char* data, std::size_t len) {
  std::size_t sent = 0;
  while (sent < len) {
    const ssize_t n = ::send(fd, data + sent, len - sent, MSG_NOSIGNAL);
    if (n < 0) {
      if (errno == EINTR) {
        continue;
      }
      return false;
    }
    if (n == 0) {
      return false;
    }
    sent += static_cast<std::size_t>(n);
  }
  return true;
}

void Server::handle_client(int client_fd) {
  int flag = 1;
  ::setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));

  HttpParser parser;
  std::vector<char> chunk(kReadChunk);

  auto send_error_and_close = [&](const std::string& body) {
    HttpRequest dummy;
    dummy.headers["connection"] = "close";
    dummy.headers["host"] = "localhost";
    dummy.path = "/";
    dummy.method = "GET";
    (void)body;
    const std::string resp =
        "HTTP/1.1 400 Bad Request\r\n"
        "Content-Type: text/plain; charset=utf-8\r\n"
        "Content-Length: 0\r\n"
        "Connection: close\r\n"
        "\r\n";
    write_all(client_fd, resp.data(), resp.size());
  };

  while (true) {
    while (!parser.has_complete_request() && !parser.failed()) {
      const ssize_t n = ::recv(client_fd, chunk.data(), chunk.size(), 0);
      if (n < 0) {
        if (errno == EINTR) {
          continue;
        }
        close_fd(client_fd);
        return;
      }
      if (n == 0) {
        close_fd(client_fd);
        return;
      }
      parser.feed(chunk.data(), static_cast<std::size_t>(n));
    }

    if (parser.failed()) {
      send_error_and_close("");
      close_fd(client_fd);
      return;
    }

    HttpRequest req = parser.take_request();
    const std::string conn = HttpParser::header_value(req, "connection");
    const bool close_after = (conn == "close");
    const std::string resp = Router::handle(req);
    if (!write_all(client_fd, resp.data(), resp.size())) {
      close_fd(client_fd);
      return;
    }
    if (close_after) {
      close_fd(client_fd);
      return;
    }
  }
}

void Server::run() {
  listen_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
  if (listen_fd_ < 0) {
    throw std::runtime_error(std::string("socket: ") + std::strerror(errno));
  }

  int yes = 1;
  if (::setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
    close_fd(listen_fd_);
    throw std::runtime_error(std::string("setsockopt: ") + std::strerror(errno));
  }

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(static_cast<uint16_t>(port_));
  addr.sin_addr.s_addr = htonl(INADDR_ANY);

  if (::bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
    close_fd(listen_fd_);
    throw std::runtime_error(std::string("bind: ") + std::strerror(errno));
  }
  if (::listen(listen_fd_, kBacklog) < 0) {
    close_fd(listen_fd_);
    throw std::runtime_error(std::string("listen: ") + std::strerror(errno));
  }

  running_.store(true);
  while (running_.load()) {
    sockaddr_in client_addr{};
    socklen_t client_len = sizeof(client_addr);
    const int client_fd =
        ::accept(listen_fd_, reinterpret_cast<sockaddr*>(&client_addr), &client_len);
    if (client_fd < 0) {
      if (!running_.load() || errno == EBADF || errno == EINVAL) {
        break;
      }
      if (errno == EINTR) {
        continue;
      }
      continue;
    }

    try {
      pool_->enqueue([client_fd] { Server::handle_client(client_fd); });
    } catch (...) {
      ::close(client_fd);
    }
  }

  close_fd(listen_fd_);
  if (pool_) {
    pool_->shutdown();
  }
}
