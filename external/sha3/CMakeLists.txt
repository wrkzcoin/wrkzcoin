cmake_minimum_required(VERSION 2.8.12)

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    message(STATUS "SHA3: Found ccache package... Activating...")
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
endif()

set(LIB_MAJOR_VERSION "1")
set(LIB_MINOR_VERSION "0")
set(LIB_PATCH_VERSION "0")
set(LIB_VERSION_STRING "${LIB_MAJOR_VERSION}.${LIB_MINOR_VERSION}.${LIB_PATCH_VERSION}")

if(CMAKE_VERSION VERSION_LESS 3.0)
    project(sha3 C)
    enable_language(C)
    enable_language(CXX)
else()
    cmake_policy(SET CMP0048 NEW)
    project(sha3 VERSION "${LIB_VERSION_STRING}" LANGUAGES C CXX)
endif()

## This section describes our general CMake setup options
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(CMAKE_SKIP_INSTALL_RULES OFF FORCE)
set(CMAKE_SKIP_PACKAGE_ALL_DEPENDENCY ON FORCE)
set(CMAKE_SUPPRESS_REGENERATION ON)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# Enable c++11
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

## We only build static binaries -- this is left here for our dependencies
set(STATIC ON CACHE BOOL FORCE "Link libraries statically? Forced to ON")
add_definitions(-DSTATICLIB)

option(BUILD_BENCHMARK "Build benchmark testing tool" OFF)
if(DEFINED ENV${BUILD_BENCHMARK})
    set(BUILD_BENCHMARK $ENV{BUILD_BENCHMARK})
    message(STATUS "SHA3: Activating benchmark testing tool")
endif()

if(MSVC)
    add_definitions("/EHsc -std:c++17 /DWIN32_LEAN_AND_MEAN /O2 /wd4267")
endif()

if(NOT MSVC)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c11 -O3")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -O3")
endif()

include_directories(include)

set(SHA3_SRC
    src/sha3.cpp
)

add_library(sha3 STATIC ${SHA3_SRC})

set_property(TARGET sha3 PROPERTY C_STANDARD 90)
set_property(TARGET sha3 PROPERTY SOVERSION 1)

if(BUILD_BENCHMARK)
    set(BENCHMARK_SOURCES src/benchmark.cpp)

    add_executable(benchmark ${BENCHMARK_SOURCES})
    target_link_libraries(benchmark sha3)
    set_property(TARGET benchmark PROPERTY OUTPUT_NAME "benchmark")
endif()

foreach(flag_var
        CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE
        CMAKE_C_FLAGS_MINSIZEREL CMAKE_C_FLAGS_RELWITHDEBINFO
        CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
        CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO)
    string(REGEX REPLACE "/RTC[^ ]*" "" ${flag_var} "${${flag_var}}")
    string(REGEX REPLACE "/Od" "" ${flag_var} "${${flag_var}}")
endforeach(flag_var)
