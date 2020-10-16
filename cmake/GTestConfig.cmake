#compile gtest with flag -DBUILD_SHARED_LIBS=ON -DCMAKE_CXX_STANDARD=11

find_path(GTEST_ROOT_DIR
        NAMES include/gtest/gtest.h
        PATHS /usr/local/ /usr/local/opt/gtest/ /opt/local/ /usr/
        NO_DEFAULT_PATH
        )
find_path(GTEST_DIR
        NAMES include/gtest/gtest.h
        PATHS /usr/local/ /usr/local/opt/gtest/ /opt/local/ /usr/
        NO_DEFAULT_PATH
        )

find_path(GTEST_INCLUDE_DIR
        NAMES GTEST/gtest.h
        PATHS ${GTEST_ROOT_DIR}/include
        NO_DEFAULT_PATH
        )

find_library(GTEST_LIBRARY
        NAMES gtest
        PATHS ${GTEST_ROOT_DIR}/lib /usr/lib /usr/lib/x86_64-linux-gnu
        NO_DEFAULT_PATH
        )

if(GTEST_INCLUDE_DIR AND GTEST_LIBRARY)
    set(GTEST_FOUND TRUE)
else()
    set(GTEST_FOUND FALSE)
endif()

mark_as_advanced(
        GTEST_ROOT_DIR
        GTEST_INCLUDE_DIR
        GTEST_LIBRARY
)

set(CMAKE_REQUIRED_INCLUDES ${GTEST_INCLUDE_DIR})
set(CMAKE_REQUIRED_LIBRARIES ${GTEST_LIBRARY})