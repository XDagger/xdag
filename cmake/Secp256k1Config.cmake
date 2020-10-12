find_path(SECP256K1_ROOT_DIR
        NAMES include/secp256k1.h
        PATHS /usr/local/ /usr/local/opt/ /opt/local/ /usr/
        NO_DEFAULT_PATH
        )

find_path(SECP256K1_INCLUDE_DIR
        NAMES secp256k1.h
        PATHS ${SECP256K1_ROOT_DIR}/include
        NO_DEFAULT_PATH
        )

find_library(SECP256K1_LIBRARY
        NAMES secp256k1
        PATHS ${SECP256K1_ROOT_DIR}/lib /usr/lib /usr/lib/x86_64-linux-gnu
        NO_DEFAULT_PATH
        )

if(SECP256K1_INCLUDE_DIR AND SECP256K1_LIBRARY)
    set(SECP256K1_FOUND TRUE)
else()
    set(SECP256K1_FOUND FALSE)
endif()

mark_as_advanced(
        SECP256K1_ROOT_DIR
        SECP256K1_INCLUDE_DIR
        SECP256K1_LIBRARY
)

set(CMAKE_REQUIRED_INCLUDES ${SECP256K1_INCLUDE_DIR})
set(CMAKE_REQUIRED_LIBRARIES ${SECP256K1_LIBRARY})

