find_path(TCMALLOC_ROOT_DIR
        NAMES include/gperftools/tcmalloc.h
        PATHS /usr/local/opt/gperftools/ /opt/local/ /usr/local/ /usr/
        NO_DEFAULT_PATH
        )

find_path(TCMALLOC_INCLUDE_DIR
        NAMES gperftools/tcmalloc.h
        PATHS ${TCMALLOC_ROOT_DIR}/include
        NO_DEFAULT_PATH
        )

find_library(TCMALLOC_LIBRARY
        NAMES tcmalloc
        PATHS ${TCMALLOC_ROOT_DIR}/lib /usr/lib /usr/lib/x86_64-linux-gnu
        NO_DEFAULT_PATH
        )

find_library(PROFILER_LIBRARY
        NAMES profiler
        PATHS ${TCMALLOC_ROOT_DIR}/lib /usr/lib /usr/lib/x86_64-linux-gnu
        NO_DEFAULT_PATH
        )

if(TCMALLOC_INCLUDE_DIR AND TCMALLOC_LIBRARY AND PROFILER_LIBRARY)
    set(TCMALLOC_FOUND TRUE)
else()
    set(TCMALLOC_FOUND FALSE)
endif()

mark_as_advanced(
        TCMALLOC_ROOT_DIR
        TCMALLOC_INCLUDE_DIR
        TCMALLOC_LIBRARY
)

set(CMAKE_REQUIRED_INCLUDES ${TCMALLOC_INCLUDE_DIR})
set(CMAKE_REQUIRED_LIBRARIES ${TCMALLOC_LIBRARY})
set(CMAKE_REQUIRED_LIBRARIES ${PROFILER_LIBRARY})

