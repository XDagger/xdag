cmake_minimum_required(VERSION 3.2)
project(rsdbg_test)


set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_FLAGS "-std=gnu11 -O3 -g -Wall -Wmissing-prototypes -Wno-unused-result")

file(GLOB_RECURSE XDAG_TEST_RSDB_SRC
  ../rsdb.c
  rsdb_test.c
)

include_directories("/usr/local/include")

find_library(ROCKSDB librocksdb.a "/usr/local/lib")

add_executable(
  rsdb_test ${XDAG_TEST_RSDB_SRC}
)

target_link_libraries(
  rsdb_test ${ROCKSDB} 
)
