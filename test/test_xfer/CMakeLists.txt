set(xfer_sources
        test_xfer.cpp)

set(xfer_headers)

add_executable(test_xfer
  ${xfer_sources}
  ${xfer_headers})

target_link_libraries(test_xfer
  PRIVATE
  ${GTEST_LIBRARY})

include_directories(test_xfer ${GTEST_INCLUDE_DIR})

set_property(TARGET test_xfer
        PROPERTY
        FOLDER "test")

add_test(NAME test_xfer
        COMMAND test_xfer)

if(RUN_TEST)
    add_custom_target(test_xfer_all
            ALL DEPENDS test_xfer)

    add_custom_command(TARGET test_xfer_all
            POST_BUILD
            COMMAND           test_xfer)
endif()