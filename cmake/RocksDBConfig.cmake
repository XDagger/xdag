find_path(RocksDB_ROOT_DIR
        NAMES include/rocksdb/c.h
        PATHS /usr/local/ /usr/local/opt/rocksdb/ /opt/local/ /usr/
        NO_DEFAULT_PATH
        )
find_path(RocksDB_DIR
        NAMES include/rocksdb/c.h
        PATHS /usr/local/ /usr/local/opt/rocksdb/ /opt/local/ /usr/
        NO_DEFAULT_PATH
        )

find_path(RocksDB_INCLUDE_DIR
        NAMES rocksdb/c.h
        PATHS ${RocksDB_ROOT_DIR}/include
        NO_DEFAULT_PATH
        )

find_library(RocksDB_LIBRARY
        NAMES rocksdb
        PATHS ${RocksDB_ROOT_DIR}/lib /usr/lib /usr/lib/x86_64-linux-gnu
        NO_DEFAULT_PATH
        )

if(RocksDB_INCLUDE_DIR AND RocksDB_LIBRARY)
    set(ROCKSDB_FOUND TRUE)
else()
    set(ROCKSDB_FOUND FALSE)
endif()

mark_as_advanced(
        RocksDB_ROOT_DIR
        RocksDB_INCLUDE_DIR
        RocksDB_LIBRARY
)

set(CMAKE_REQUIRED_INCLUDES ${RocksDB_INCLUDE_DIR})
set(CMAKE_REQUIRED_LIBRARIES ${RocksDB_LIBRARY})

