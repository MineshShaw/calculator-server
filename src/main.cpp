#include "server.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

int main(int argc, char** argv) {
  int port = 8080;
  std::size_t threads = 0;

  if (argc >= 2) {
    port = std::atoi(argv[1]);
  }
  if (argc >= 3) {
    threads = static_cast<std::size_t>(std::atoi(argv[2]));
  }

  try {
    Server server(port, threads);
    std::cerr << "calculator server listening on 0.0.0.0:" << server.port()
              << std::endl;
    server.run();
  } catch (const std::exception& ex) {
    std::cerr << "fatal: " << ex.what() << std::endl;
    return 1;
  }
  return 0;
}
