set(UPNPC_BUILD_STATIC ON CACHE BOOL "Build static library")
set(UPNPC_BUILD_SHARED OFF CACHE BOOL "Build shared library")
set(UPNPC_BUILD_TESTS OFF CACHE BOOL "Build test executables")
set(WITH_MD_LIBRARY OFF CACHE BOOL "Code Generation for static library")

# Disable warnings
if (NOT MSVC)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -w -Wno-everything")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -w -Wno-everything")
endif()

add_subdirectory(miniupnpc)
add_subdirectory(cryptopp)
add_subdirectory(argon2)
add_subdirectory(zstd)
add_subdirectory(snappy)
add_subdirectory(turtlecoin-crypto)

if (MSVC)
    add_subdirectory(rocksDB EXCLUDE_FROM_ALL)
    add_subdirectory(levelDB EXCLUDE_FROM_ALL)
    set_property(TARGET upnpc-static cryptopp-static rocksdb leveldb snappy zstd argon2 turtlecoin-crypto PROPERTY FOLDER "external")
elseif (NOT MSVC)
    set_property(TARGET upnpc-static cryptopp-static snappy zstd argon2 turtlecoin-crypto PROPERTY FOLDER "external")
    add_custom_target(
            rocksdb
            COMMAND $(MAKE) rocksdb
            WORKING_DIRECTORY ${PROJECT_BINARY_DIR}/rocksdb
    )
    add_library(rocksdblib STATIC IMPORTED GLOBAL)
    set_target_properties(rocksdblib PROPERTIES IMPORTED_LOCATION ${PROJECT_BINARY_DIR}/rocksdb/librocksdb.a)
    add_dependencies(rocksdblib rocksdb)

    add_custom_target(
            leveldb
            COMMAND $(MAKE) leveldb
            WORKING_DIRECTORY ${PROJECT_BINARY_DIR}/leveldb
    )
    add_library(leveldblib STATIC IMPORTED GLOBAL)
    set_target_properties(leveldblib PROPERTIES IMPORTED_LOCATION ${PROJECT_BINARY_DIR}/leveldb/libleveldb.a)
    add_dependencies(leveldblib leveldb)
endif ()

if (MSVC)
    set_property(TARGET upnpc-static APPEND_STRING PROPERTY COMPILE_FLAGS " -wd4244 -wd4267")
elseif (NOT MSVC)
    set_property(TARGET upnpc-static APPEND_STRING PROPERTY COMPILE_FLAGS " -Wno-undef -Wno-unused-result -Wno-unused-value")
endif ()
