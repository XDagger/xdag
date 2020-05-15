find_path(UV_ROOT_DIR
        NAMES include/uv.h
        PATHS /usr/local/ /usr/local/opt/uv/ /opt/local/ /usr/ /usr/lib/x86_64-linux-gnu
        NO_DEFAULT_PATH
        )
message("uv root dir ${UV_ROOT_DIR}")

find_path(UV_DIR
        NAMES include/uv.h
        PATHS /usr/local/ /usr/local/opt/gtest/ /opt/local/ /usr/ /usr/lib/x86_64-linux-gnu
        NO_DEFAULT_PATH
        )
message("uv dir ${UV_DIR}")

find_path(UV_INCLUDE_DIR
        NAMES uv.h
        PATHS ${UV_ROOT_DIR}/include
        NO_DEFAULT_PATH
        )
message("uv include dir ${UV_INCLUDE_DIR}")

find_library(UV_LIBRARY
        NAMES uv
        PATHS ${UV_ROOT_DIR}/lib /usr/lib/x86_64-linux-gnu
        NO_DEFAULT_PATH
        )
message("uv library ${UV_LIBRARY}")

if(UV_INCLUDE_DIR AND UV_LIBRARY)
    set(UV_FOUND TRUE)
    message("UV_FOUND ${UV_FOUND}")
else()
    set(UV_FOUND FALSE)
    message("UV_FOUND ${UV_FOUND}")
endif()

mark_as_advanced(
        UV_ROOT_DIR
        UV_INCLUDE_DIR
        UV_LIBRARY
)

set(CMAKE_REQUIRED_INCLUDES ${UV_INCLUDE_DIR})
set(CMAKE_REQUIRED_LIBRARIES ${UV_LIBRARY})