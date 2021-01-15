cmake_minimum_required(VERSION 2.8.12)

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    message(STATUS "TurtleCoin Crypto: Found ccache package... Activating...")
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
endif()

set(LIB_MAJOR_VERSION "1")
set(LIB_MINOR_VERSION "0")
set(LIB_PATCH_VERSION "0")
set(LIB_VERSION_STRING "${LIB_MAJOR_VERSION}.${LIB_MINOR_VERSION}.${LIB_PATCH_VERSION}")

if(CMAKE_VERSION VERSION_LESS 3.0)
    project(turtlecoin-crypto CXX)
    enable_language(CXX)
else()
    cmake_policy(SET CMP0048 NEW)
    project(turtlecoin-crypto VERSION "${LIB_VERSION_STRING}" LANGUAGES CXX)
endif()

## This section describes our general CMake setup options
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_SKIP_INSTALL_RULES OFF FORCE)
set(CMAKE_SKIP_PACKAGE_ALL_DEPENDENCY ON FORCE)
set(CMAKE_SUPPRESS_REGENERATION ON)
set(CMAKE_POLICY_WARNING_CMP0048 OFF)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# Enable c++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

## We only build static binaries -- this is left here for our dependencies
set(STATIC ON CACHE BOOL FORCE "Link libraries statically? Forced to ON")
add_definitions(-DSTATICLIB)
add_definitions(-DRAPIDJSON_HAS_STDSTRING)

if(MSVC)
    add_definitions("/EHsc -std:c++17 /DWIN32_LEAN_AND_MEAN /O2 /wd4267")
endif()

if(NOT MSVC)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c11 -O3")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -O3")
endif()

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/external)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/external)

set(SOURCES
    src/bulletproofs.cpp
    src/bulletproofsplus.cpp
    src/crypto_common.cpp
    src/hashing.cpp
    src/multisig.cpp
    src/ringct.cpp
    src/ring_signature_borromean.cpp
    src/ring_signature_clsag.cpp
    src/signature.cpp
    src/string_tools.cpp
)

add_library(turtlecoin-crypto STATIC ${SOURCES})
target_link_libraries(turtlecoin-crypto ed25519 sha3)

if(WIN32)
    target_link_libraries(turtlecoin-crypto ws2_32)
endif()

set(TEST_SOURCES
    src/cryptotest.cpp
)

add_executable(cryptotestbp ${TEST_SOURCES})
target_link_libraries(cryptotestbp turtlecoin-crypto)
set_property(TARGET cryptotestbp PROPERTY OUTPUT_NAME "cryptotestbp")

foreach(flag_var
        CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE
        CMAKE_C_FLAGS_MINSIZEREL CMAKE_C_FLAGS_RELWITHDEBINFO
        CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
        CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO)
    string(REGEX REPLACE "/RTC[^ ]*" "" ${flag_var} "${${flag_var}}")
    string(REGEX REPLACE "/Od" "" ${flag_var} "${${flag_var}}")
endforeach(flag_var)
