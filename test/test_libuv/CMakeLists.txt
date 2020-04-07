set(libuv_test_source_file
        queue.cpp
        tcp_server.cpp
        producer_consumer_test.cpp
        main.cpp
        )

add_executable(libuv_test
        ${libuv_test_source_file}
)
include_directories(libuv_test
        ${GTEST_INCLUDE_DIR}
        ${UV_INCLUDE_DIR}
)

target_link_libraries(libuv_test
        ${UV_LIBRARY}
        ${GTEST_LIBRARY}
)

add_test(NAME libuv_test
        COMMAND libuv_test)
