cmake_minimum_required(VERSION 3.2)
project(xdag_tests C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_FLAGS "-std=gnu11 -O3 -g -Wall -Wmissing-prototypes -Wno-unused-result")

add_subdirectory(test_core)
add_subdirectory(test_libuv)
add_subdirectory(test_randomx)
add_subdirectory(test_xfer)

set(enabled_tests
        test_core
        test_randomx
        test_xfer)

add_custom_target(xdag_tests DEPENDS enabled_tests)
set_property(TARGET xdag_tests PROPERTY FOLDER "${folder}")
