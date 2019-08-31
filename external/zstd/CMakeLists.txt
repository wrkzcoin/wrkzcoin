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

# Define library directory, where sources and header files are located
include_directories(${LIBRARY_DIR} ${LIBRARY_DIR}/common)

file(GLOB CommonSources ${LIBRARY_DIR}/common/*.c)
file(GLOB CompressSources ${LIBRARY_DIR}/compress/*.c)
file(GLOB DecompressSources ${LIBRARY_DIR}/decompress/*.c)
file(GLOB DictBuilderSources ${LIBRARY_DIR}/dictBuilder/*.c)
file(GLOB DeprecatedSources ${LIBRARY_DIR}/deprecated/*.c)

set(Sources
        ${CommonSources}
        ${CompressSources}
        ${DecompressSources}
        ${DictBuilderSources}
        ${DeprecatedSources})

file(GLOB CommonHeaders ${LIBRARY_DIR}/common/*.h)
file(GLOB CompressHeaders ${LIBRARY_DIR}/compress/*.h)
file(GLOB DecompressHeaders ${LIBRARY_DIR}/decompress/*.h)
file(GLOB DictBuilderHeaders ${LIBRARY_DIR}/dictBuilder/*.h)
file(GLOB DeprecatedHeaders ${LIBRARY_DIR}/deprecated/*.h)

set(Headers
        ${LIBRARY_DIR}/zstd.h
        ${CommonHeaders}
        ${CompressHeaders}
        ${DecompressHeaders}
        ${DictBuilderHeaders}
        ${DeprecatedHeaders})

IF (MSVC)
    SET(MSVC_RESOURCE_DIR ${Sources}/build/VS2010/libzstd-dll)
    SET(PlatformDependResources ${MSVC_RESOURCE_DIR}/libzstd-dll.rc)
ENDIF (MSVC)

# Split project to static and shared libraries build
SET(ZSTD_STATIC_LIB zstd)
add_library(${ZSTD_STATIC_LIB} STATIC ${Headers} ${Sources})
