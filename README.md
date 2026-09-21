# calculator-server

Multi-threaded HTTP/1.1 calculator built from raw TCP sockets in C++. There is no web framework and no HTTP library: request bytes are parsed until `\r\n\r\n`, routed to arithmetic helpers, and written back as HTTP/1.1 responses. Concurrent clients are served by a custom thread pool. Persistent connections (Keep-Alive) stay open so a client can send many sequential requests on one TCP session.

## Architecture

```
client TCP connection
        |
        v
  Server (listen/accept)
        |
        v
  ThreadPool task queue  --->  worker thread
                                    |
                                    v
                         persistent connection loop
                         1. read more socket bytes
                         2. HttpParser (one request)
                         3. Router -> Calculator
                         4. send response
                         5. repeat until client closes
```

| Component | Role |
| --- | --- |
| `Calculator` | `add` / `sub` / `mul` / `div`, strict number parsing, divide-by-zero handling |
| `HttpParser` | Incremental parser for method, path, query, headers, optional body |
| `Router` | Maps a parsed request to a status line and body |
| `ThreadPool` | Fixed workers, mutex + condition variable, FIFO `std::function` queue |
| `Server` | `socket` / `bind` / `listen` / `accept`; each client fd is enqueued |

### Persistent connections

HTTP/1.1 defaults to Keep-Alive. After `accept`, a worker does **not** close the socket after the first response. It loops:

1. Append `recv` bytes to a per-connection buffer.
2. When `\r\n\r\n` is present (and `Content-Length` bytes if any), extract **exactly one** request.
3. Any leftover bytes stay in the buffer so pipelined or coalesced requests stay aligned.
4. Route, `send` the full response, then wait for the next request.
5. Exit when `recv` returns 0 (client closed) or the client sent `Connection: close`.

That byte-exact consume-and-leave-remainder step is what keeps the next request on the same socket parseable.

### Thread pool

`ThreadPool` starts N worker threads (hardware concurrency by default). `enqueue` pushes a task under a mutex and `notify_one`. Workers `wait` on a condition variable until the queue is non-empty or shutdown is requested. `shutdown` (also called from the destructor) sets a stop flag, wakes everyone, and joins. Queued work is drained before workers exit so in-flight connections can finish.

## Endpoints

Listen address: `0.0.0.0:8080` (reachable as `localhost:8080`).

| Request | Result |
| --- | --- |
| `GET /add?a=2&b=3` | `200` body `5` |
| `GET /sub?a=10&b=4` | `200` body `6` |
| `GET /mul?a=6&b=7` | `200` body `42` |
| `GET /div?a=9&b=3` | `200` body `3` |
| `GET /div?a=1&b=0` | `400 Bad Request` |
| `GET /add?a=x&b=3` | `400 Bad Request` |
| `GET /pow?a=2&b=8` | `404 Not Found` |
| `POST /add` | `405 Method Not Allowed` |
| `GET /add` without `Host` | `400 Bad Request` |

A `Host` header is required on every request (HTTP/1.1). Operands `a` and `b` must parse as finite numbers.

## Build

Requires CMake 3.16+, a C++17 compiler, pthread, and network access on the first configure so GoogleTest can be fetched.

```bash
cmake -S . -B build
cmake --build build -j
```

Binaries:

- `build/calculator_server`
- `build/unit_tests`

## Run the server

```bash
./build/calculator_server           # port 8080, hardware_concurrency workers
./build/calculator_server 8080 16   # port, worker count
```

Quick manual check:

```bash
curl -s 'http://127.0.0.1:8080/add?a=2&b=3'
```

## Unit tests

GoogleTest covers calculator arithmetic, parser extraction (including incremental and pipelined bytes), router status codes, and thread-pool concurrency plus clean shutdown.

```bash
cmake --build build -j
ctest --test-dir build --output-on-failure
# or
./build/unit_tests
```

## Persistent-connection test

With the server running:

```bash
python3 scripts/persistent_test.py
```

This opens **one** TCP socket and runs the assignment queries back-to-back without reconnecting.

## Stress test

With the server running:

```bash
python3 scripts/stress_test.py
python3 scripts/stress_test.py --connections 300 --requests 40
```

Defaults: 200 concurrent Keep-Alive connections, 50 requests each (10,000 requests). The process reports successes, error samples, elapsed time, and requests/sec. A non-zero exit means at least one request failed or a connection dropped unexpectedly.

## Layout

```
calculator_server/
├── CMakeLists.txt
├── README.md
├── include/
│   ├── calculator.hpp
│   ├── http_parser.hpp
│   ├── router.hpp
│   ├── thread_pool.hpp
│   └── server.hpp
├── src/
│   ├── calculator.cpp
│   ├── http_parser.cpp
│   ├── router.cpp
│   ├── thread_pool.cpp
│   ├── server.cpp
│   └── main.cpp
├── tests/
│   ├── test_calculator.cpp
│   ├── test_http_parser.cpp
│   ├── test_router.cpp
│   └── test_thread_pool.cpp
└── scripts/
    ├── persistent_test.py
    └── stress_test.py
```
