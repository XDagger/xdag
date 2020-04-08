set(test_libuv_source_file
        queue.cpp
        tcp_server.cpp
        producer_consumer_test.cpp
        main.cpp
        )

add_executable(test_libuv
        ${test_libuv_source_file}
)
include_directories(test_libuv
        ${GTEST_INCLUDE_DIR}
        ${UV_INCLUDE_DIR}
)

target_link_libraries(test_libuv
        ${UV_LIBRARY}
        ${GTEST_LIBRARY}
)

add_test(NAME test_libuv
        COMMAND test_libuv)

if(RUN_TEST)
#add_custom_target(test_libuv_all ALL DEPENDS test_libuv)
#
#add_custom_command(TARGET test_libuv_all
#        POST_BUILD
#        COMMAND           test_libuv)
endif()