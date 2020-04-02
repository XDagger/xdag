cmake_minimum_required(VERSION 3.2)
project(rsdbg_test)


set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_FLAGS "-std=gnu11 -O3 -g -Wall -Wmissing-prototypes -Wno-unused-result")

file(GLOB_RECURSE XDAG_TEST_PC_SRC
    queue.c
    producer_consumer_test.c
)

file(GLOB_RECURSE XDAG_TEST_LIBUV_SRC
    tcp_server.c
)

include_directories("/usr/local/include")

find_library(LIBUV libuv.a "/usr/local/lib")

add_executable(
    producer_consumer_test ${XDAG_TEST_PC_SRC}
)
add_executable(
    tcp_server ${XDAG_TEST_LIBUV_SRC}
)

target_link_libraries(
    producer_consumer_test ${LIBUV}
)

target_link_libraries(
    tcp_server ${LIBUV}
)
