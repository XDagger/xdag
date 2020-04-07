set(test_core_sources
        double_spending.cpp
        gen_blockchain.cpp
        invalid_tx.cpp
        main.cpp)

set(test_core_headers)

add_executable(test_core
        ${test_core_sources}
        ${test_core_headers})

target_link_libraries(test_core
        PRIVATE
        ${GTEST_LIBRARY})

include_directories(test_core ${GTEST_INCLUDE_DIR})

#file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test_core")

#add_custom_target(test_core_all
#        COMMAND           test_core
#        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test_core")

add_test(NAME   test_core
        COMMAND test_core)