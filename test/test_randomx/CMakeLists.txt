set(test_randomx_sources
        test_randomx.cpp)

set(test_randomx_headers)

add_executable(test_randomx
        ${test_randomx_sources}
        ${test_randomx_headers})

target_link_libraries(test_randomx
        PRIVATE
        ${GTEST_LIBRARY})

include_directories(test_core ${GTEST_INCLUDE_DIR})

set_property(TARGET test_randomx
        PROPERTY
        FOLDER "test")

add_test(NAME   test_randomx
        COMMAND test_randomx)