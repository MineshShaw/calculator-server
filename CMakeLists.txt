cmake_minimum_required(VERSION 3.16)
project(calculator_server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_compile_options(-Wall -Wextra -Wpedantic)

set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED)

add_library(calculator_core STATIC
  src/calculator.cpp
  src/http_parser.cpp
  src/router.cpp
  src/thread_pool.cpp
  src/server.cpp
)
target_include_directories(calculator_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(calculator_core PUBLIC Threads::Threads)

add_executable(calculator_server src/main.cpp)
target_link_libraries(calculator_server PRIVATE calculator_core)

include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.14.0
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

add_executable(unit_tests
  tests/test_calculator.cpp
  tests/test_http_parser.cpp
  tests/test_router.cpp
  tests/test_thread_pool.cpp
)
target_link_libraries(unit_tests PRIVATE
  calculator_core
  GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(unit_tests)
