CMAKE_MINIMUM_REQUIRED(VERSION 3.0.0)

PROJECT(zstd)

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    message(STATUS "Found ccache package... Activating...")
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
endif()

SET(CMAKE_INCLUDE_CURRENT_DIR TRUE)

# Define library directory, where sources and header files are located
SET(LIBRARY_DIR ${CMAKE_CURRENT_LIST_DIR}/lib)
INCLUDE_DIRECTORIES(${LIBRARY_DIR} ${LIBRARY_DIR}/common)

# Parse version
function(GetZstdLibraryVersion _header _major _minor _release)
    # Read file content
    FILE(READ ${_header} CONTENT)

    string(REGEX MATCH ".*define ZSTD_VERSION_MAJOR *([0-9]+).*define ZSTD_VERSION_MINOR *([0-9]+).*define ZSTD_VERSION_RELEASE *([0-9]+)" VERSION_REGEX "${CONTENT}")
    SET(${_major} ${CMAKE_MATCH_1} PARENT_SCOPE)
    SET(${_minor} ${CMAKE_MATCH_2} PARENT_SCOPE)
    SET(${_release} ${CMAKE_MATCH_3} PARENT_SCOPE)
endfunction()

GetZstdLibraryVersion(${LIBRARY_DIR}/zstd.h LIBVER_MAJOR LIBVER_MINOR LIBVER_RELEASE)
MESSAGE("ZSTD VERSION ${LIBVER_MAJOR}.${LIBVER_MINOR}.${LIBVER_RELEASE}")

SET(Sources
        ${LIBRARY_DIR}/common/entropy_common.c
        ${LIBRARY_DIR}/common/fse_decompress.c
        ${LIBRARY_DIR}/common/threading.c
        ${LIBRARY_DIR}/common/pool.c
        ${LIBRARY_DIR}/common/zstd_common.c
        ${LIBRARY_DIR}/common/error_private.c
        ${LIBRARY_DIR}/common/xxhash.c
        ${LIBRARY_DIR}/compress/fse_compress.c
        ${LIBRARY_DIR}/compress/huf_compress.c
        ${LIBRARY_DIR}/compress/zstd_compress.c
        ${LIBRARY_DIR}/compress/zstdmt_compress.c
        ${LIBRARY_DIR}/compress/zstd_fast.c
        ${LIBRARY_DIR}/compress/zstd_double_fast.c
        ${LIBRARY_DIR}/compress/zstd_lazy.c
        ${LIBRARY_DIR}/compress/zstd_opt.c
        ${LIBRARY_DIR}/compress/zstd_ldm.c
        ${LIBRARY_DIR}/decompress/huf_decompress.c
        ${LIBRARY_DIR}/decompress/zstd_decompress.c
        ${LIBRARY_DIR}/dictBuilder/cover.c
        ${LIBRARY_DIR}/dictBuilder/divsufsort.c
        ${LIBRARY_DIR}/dictBuilder/zdict.c
        ${LIBRARY_DIR}/deprecated/zbuff_common.c
        ${LIBRARY_DIR}/deprecated/zbuff_compress.c
        ${LIBRARY_DIR}/deprecated/zbuff_decompress.c)

SET(Headers
        ${LIBRARY_DIR}/zstd.h
        ${LIBRARY_DIR}/common/pool.h
        ${LIBRARY_DIR}/common/threading.h
        ${LIBRARY_DIR}/common/bitstream.h
        ${LIBRARY_DIR}/common/error_private.h
        ${LIBRARY_DIR}/common/zstd_errors.h
        ${LIBRARY_DIR}/common/fse.h
        ${LIBRARY_DIR}/common/huf.h
        ${LIBRARY_DIR}/common/mem.h
        ${LIBRARY_DIR}/common/zstd_internal.h
        ${LIBRARY_DIR}/compress/zstd_compress_internal.h
        ${LIBRARY_DIR}/compress/zstd_fast.h
        ${LIBRARY_DIR}/compress/zstd_double_fast.h
        ${LIBRARY_DIR}/compress/zstd_lazy.h
        ${LIBRARY_DIR}/compress/zstd_opt.h
        ${LIBRARY_DIR}/compress/zstd_ldm.h
        ${LIBRARY_DIR}/compress/zstdmt_compress.h
        ${LIBRARY_DIR}/dictBuilder/zdict.h
        ${LIBRARY_DIR}/deprecated/zbuff.h)

IF (MSVC)
    SET(MSVC_RESOURCE_DIR ${Sources}/build/VS2010/libzstd-dll)
    SET(PlatformDependResources ${MSVC_RESOURCE_DIR}/libzstd-dll.rc)
ENDIF (MSVC)

# Split project to static and shared libraries build
SET(ZSTD_STATIC_LIB zstd)
add_library(${ZSTD_STATIC_LIB} STATIC ${Headers} ${Sources})
