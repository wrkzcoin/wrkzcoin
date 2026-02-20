# We need 3.8 for set(CMAKE_CXX_STANDARD 20)
cmake_minimum_required(VERSION 3.8)

find_program(CCACHE_PROGRAM ccache)
if (CCACHE_PROGRAM)
    message(STATUS "Found ccache package... Activating...")
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
endif ()

if (DEFINED CMAKE_BUILD_TYPE)
    set (CMAKE_BUILD_TYPE ${CMAKE_BUILD_TYPE} CACHE STRING "Choose the type of build, options are: Debug, Release, RelWithDebInfo")
else ()
    set (CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build, options are: Debug, Release, RelWithDebInfo")
endif ()

set(CMAKE_CONFIGURATION_TYPES Debug RelWithDebInfo Release CACHE TYPE INTERNAL)
set(ARCH default CACHE STRING "CPU to build for: -march value or native")
set(CONSENSUS_SAFE_BUILD ON CACHE BOOL "Use reproducible, consensus-safe compiler optimizations")
set(PORTABLE_BINARY ON CACHE BOOL "Prefer portable CPU flags for binaries distributed across different machines")
set(ENABLE_X86_AESNI OFF CACHE BOOL "Enable x86 AES-NI compile flag (-maes); may reduce CPU portability")
set(FULLY_STATIC OFF CACHE BOOL "Attempt fully static executable linkage on Linux (requires static system libs)")
set(ENABLE_ZMQ ON CACHE BOOL "Enable daemon ZMQ publisher support")

project(WrkzCoin)

# Required for finding Threads on ARM
enable_language(C)
enable_language(CXX)

# Assert our compiler is good
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # GCC 7.0 or higher
    if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 7)
        message(FATAL_ERROR "GCC/G++ 7.0 or greater is required to compile. Please check the README.md for detailed compilation instructions.")
    endif ()
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    # Clang 6.0 or higher
    if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 6)
        message(FATAL_ERROR "Clang 6.0 or greater is required to compile. Please check the README.md for detailed compilation instructions.")
    endif ()
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    # Visual Studio 15 2017 or higher
    if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 19.10)
        message(FATAL_ERROR "MSVC 19.10 or greater is required to compile (Latest Visual Studio 15 2017 should suffice). Please check the README.md for detailed compilation instructions.")
    endif ()
else ()
    message(WARNING "You are using an unsupported compiler. The compilation is very likely to fail. God speed.")
endif ()

find_package(Threads)

set(VERSION "0.1")

## This section describes our general CMake setup options
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(CMAKE_SKIP_INSTALL_RULES OFF FORCE)
set(CMAKE_SKIP_PACKAGE_ALL_DEPENDENCY ON FORCE)
set(CMAKE_SUPPRESS_REGENERATION ON)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# Enable c++20 (required by RocksDB 10 headers)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

## This section is for settings found in the slow-hash routine(s) that may benefit some systems (mostly ARM)
set(FORCE_USE_HEAP ON CACHE BOOL "Force the use of heap memory allocation")
set(NO_AES OFF CACHE BOOL "Turn off Hardware AES instructions?")
set(NO_OPTIMIZED_MULTIPLY_ON_ARM OFF CACHE BOOL "Turn off Optimized Multiplication on ARM?")

message(STATUS "Building for target architecture: ${ARCH}")
message(STATUS "CONSENSUS_SAFE_BUILD: ${CONSENSUS_SAFE_BUILD}")
message(STATUS "PORTABLE_BINARY: ${PORTABLE_BINARY}")
message(STATUS "ENABLE_X86_AESNI: ${ENABLE_X86_AESNI}")
message(STATUS "FULLY_STATIC: ${FULLY_STATIC}")
message(STATUS "ENABLE_ZMQ: ${ENABLE_ZMQ}")

if (FORCE_USE_HEAP)
    add_definitions(-DFORCE_USE_HEAP)
    message(STATUS "FORCE_USE_HEAP: ENABLED")
else ()
    message(STATUS "FORCE_USE_HEAP: DISABLED")
endif ()

if (NO_AES)
    add_definitions(-DNO_AES)
    message(STATUS "HW AES: DISABLED")
else ()
    message(STATUS "HW AES: ENABLED")
endif ()

if (NO_OPTIMIZED_MULTIPLY_ON_ARM)
    add_definitions(-DNO_OPTIMIZED_MULTIPLY_ON_ARM)
    message(STATUS "OPTIMIZED_ARM_MULTIPLICATION: DISABLED")
else ()
    message(STATUS "OPTIMIZED_ARM_MULTIPLICATION: ENABLED")
endif ()

# We need to set the label and import it into CMake if it exists
set(LABEL "")
if (DEFINED ENV{LABEL})
    set(LABEL $ENV{LABEL})
    message(STATUS "Found LABEL: ${LABEL}")
endif ()

## We only build static binaries -- this is left here for our dependencies
set(STATIC ON CACHE BOOL FORCE "Link libraries statically? Forced to ON")

## This section helps us tag our builds with the git commit information
set(COMMIT_ID_IN_VERSION ON CACHE BOOL "Include commit ID in version")

# So we can use std::string with rapidjson
add_definitions(-DRAPIDJSON_HAS_STDSTRING)

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/version")

if (NOT COMMIT_ID_IN_VERSION)
    set(VERSION "${VERSION}-unknown")
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/src/config/version.h.in" "${CMAKE_CURRENT_BINARY_DIR}/version/version.h")
    add_custom_target(version ALL)
elseif (DEFINED COMMIT)
    string(REPLACE "." "\\." VERSION_RE "${VERSION}")
    if (NOT REFS MATCHES "(\\(|, )tag: v${VERSION_RE}(\\)|, )")
        set(VERSION "${VERSION}-g${COMMIT}")
    endif ()
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/src/config/version.h.in" "${CMAKE_CURRENT_BINARY_DIR}/version/version.h")
    add_custom_target(version ALL)
else ()
    find_package(Git QUIET)
    if (Git_FOUND OR GIT_FOUND)
        message(STATUS "Found Git: ${GIT_EXECUTABLE}")
        add_custom_target(version ALL "${CMAKE_COMMAND}" "-D" "VERSION=${VERSION}" "-D" "GIT=${GIT_EXECUTABLE}" "-D" "TO=${CMAKE_CURRENT_BINARY_DIR}/version/version.h" "-P" "${CMAKE_CURRENT_SOURCE_DIR}/src/version.cmake" WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
    else ()
        message(STATUS "WARNING: Git was not found!")
        set(VERSION "${VERSION}-unknown")
        configure_file("${CMAKE_CURRENT_SOURCE_DIR}/src/config/version.h.in" "${CMAKE_CURRENT_BINARY_DIR}/version/version.h")
        add_custom_target(version ALL)
    endif ()
endif ()
include_directories(include "${CMAKE_CURRENT_BINARY_DIR}/version" src external)

## Platform specific code base information is applied here
if (WIN32)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/platform/windows)
    if (MSVC)
        add_definitions("/bigobj /MP /W3 /GS- /D_CRT_SECURE_NO_WARNINGS /wd4996 /wd4345 /D_WIN32_WINNT=0x0600 /DWIN32_LEAN_AND_MEAN /DGTEST_HAS_TR1_TUPLE=0 /D_VARIADIC_MAX=8 /D__SSE4_1__")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /STACK:10485760")
        foreach (VAR CMAKE_C_FLAGS_DEBUG CMAKE_CXX_FLAGS_DEBUG CMAKE_C_FLAGS_RELWITHDEBINFO CMAKE_CXX_FLAGS_RELWITHDEBINFO CMAKE_C_FLAGS_RELEASE CMAKE_CXX_FLAGS_RELEASE)
            string(REPLACE "/MD" "/MT" ${VAR} "${${VAR}}")
        endforeach ()
        include_directories(SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/msc)
    else ()
        include_directories(SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/mingw)
        add_definitions(-DWIN32 -D_WIN32_WINNT=0x0600 -DWIN32_LEAN_AND_MEAN)
    endif ()
elseif (APPLE)
    include_directories(SYSTEM /usr/include/malloc)
    enable_language(ASM)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/platform/osx)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/platform/posix)

else ()
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/platform/linux)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/platform/posix)
endif ()

if (NOT MSVC)
    string(TOLOWER "${CMAKE_C_COMPILER}" WRKZ_C_COMPILER_LC)
    string(TOLOWER "${CMAKE_CXX_COMPILER}" WRKZ_CXX_COMPILER_LC)
    set(WRKZ_COMPILER_TARGETS_ARM FALSE)
    set(WRKZ_COMPILER_TARGETS_AARCH64 FALSE)
    set(WRKZ_COMPILER_TARGETS_ARM32 FALSE)
    set(WRKZ_COMPILER_TARGETS_ARMHF FALSE)
    set(WRKZ_COMPILER_TARGETS_ARMEL FALSE)
    if (
        WRKZ_C_COMPILER_LC MATCHES "aarch64|arm64|arm-linux-gnueabihf|arm-linux-gnueabi"
        OR WRKZ_CXX_COMPILER_LC MATCHES "aarch64|arm64|arm-linux-gnueabihf|arm-linux-gnueabi"
    )
        set(WRKZ_COMPILER_TARGETS_ARM TRUE)
    endif ()
    if (
        WRKZ_C_COMPILER_LC MATCHES "aarch64|arm64"
        OR WRKZ_CXX_COMPILER_LC MATCHES "aarch64|arm64"
    )
        set(WRKZ_COMPILER_TARGETS_AARCH64 TRUE)
    endif ()
    if (
        WRKZ_C_COMPILER_LC MATCHES "arm-linux-gnueabihf|arm-linux-gnueabi"
        OR WRKZ_CXX_COMPILER_LC MATCHES "arm-linux-gnueabihf|arm-linux-gnueabi"
    )
        set(WRKZ_COMPILER_TARGETS_ARM32 TRUE)
    endif ()
    if (
        WRKZ_C_COMPILER_LC MATCHES "arm-linux-gnueabihf"
        OR WRKZ_CXX_COMPILER_LC MATCHES "arm-linux-gnueabihf"
    )
        set(WRKZ_COMPILER_TARGETS_ARMHF TRUE)
    endif ()
    if (
        WRKZ_C_COMPILER_LC MATCHES "arm-linux-gnueabi"
        OR WRKZ_CXX_COMPILER_LC MATCHES "arm-linux-gnueabi"
    )
        set(WRKZ_COMPILER_TARGETS_ARMEL TRUE)
    endif ()

    if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
        # This option has no effect in glibc version less than 2.20.
        # Since glibc 2.20 _BSD_SOURCE is deprecated, this macro is recomended instead
        add_definitions("-D_DEFAULT_SOURCE -D_GNU_SOURCE")
    endif ()

    ## This is here to support building for multiple architecture types... but we all know how well that usually goes...
    if ("${ARCH}" STREQUAL "default")
        if (
            PORTABLE_BINARY
            AND NOT WRKZ_COMPILER_TARGETS_ARM
            AND CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|AMD64"
        )
            set(ARCH_FLAG "-march=x86-64 -mtune=generic")
        else ()
            set(ARCH_FLAG "")
        endif ()
    else ()
        if (
            WRKZ_COMPILER_TARGETS_ARM
            AND "${ARCH}" MATCHES "x86|X86|amd64|AMD64"
        )
            message(WARNING "Ignoring ARCH=${ARCH} because selected compiler targets ARM/aarch64.")
            set(ARCH_FLAG "")
        else ()
            set(ARCH_FLAG "-march=${ARCH}")
        endif ()
        if (PORTABLE_BINARY AND "${ARCH}" STREQUAL "native")
            message(WARNING "ARCH=native requested with PORTABLE_BINARY=ON. Result may not run on different CPUs.")
        endif ()
    endif ()

    ## These options generate all those nice warnings we see while building
    set(WARNINGS "-Wall -Wextra -Wpointer-arith -Wvla -Wwrite-strings  -Wno-error=extra -Wno-error=unused-function -Wno-error=sign-compare -Wno-error=strict-aliasing -Wno-error=type-limits -Wno-unused-parameter -Wno-error=unused-variable -Wno-error=undef -Wno-error=uninitialized -Wno-error=unused-result")
    if (CMAKE_C_COMPILER_ID STREQUAL "Clang")
        set(WARNINGS "${WARNINGS} -Wno-error=mismatched-tags -Wno-error=null-conversion -Wno-overloaded-shift-op-parentheses -Wno-error=shift-count-overflow -Wno-error=tautological-constant-out-of-range-compare -Wno-error=unused-private-field -Wno-error=unneeded-internal-declaration -Wno-error=unused-function -Wno-error=missing-braces -Wno-error=unused-command-line-argument")
    else ()
        set(WARNINGS "${WARNINGS} -Wlogical-op -Wno-error=maybe-uninitialized -Wno-error=clobbered -Wno-error=unused-but-set-variable")
    endif ()

    if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
        set(WARNINGS "${WARNINGS} -Wno-error=odr")
    endif ()
    set(C_WARNINGS "-Waggregate-return -Wnested-externs -Wold-style-definition -Wstrict-prototypes -Wno-implicit-fallthrough")
    set(CXX_WARNINGS "-Wno-reorder -Wno-missing-field-initializers")

    if (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "aarch64" OR "${LABEL}" STREQUAL "aarch64" OR WRKZ_COMPILER_TARGETS_AARCH64)
        set(MAES_FLAG "")
        set(CRYPTOPP_AARCH64 ON CACHE BOOL FORCE "Tell CryptoPP that we are building for aarch64")
    elseif (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "arm" OR "${LABEL}" STREQUAL "aarch32-linux-gnueabihf" OR WRKZ_COMPILER_TARGETS_ARMHF)
        set(MAES_FLAG "")
        set(CRYPTOPP_ARMHF ON CACHE BOOL FORCE "Tell CryptoPP that we are building for aarch32-linux-gnueabihf")
    elseif (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "arm" OR "${LABEL}" STREQUAL "aarch32-linux-gnueabi" OR WRKZ_COMPILER_TARGETS_ARMEL)
        set(MAES_FLAG "")
        set(CRYPTOPP_ARM ON CACHE BOOL FORCE "Tell CryptoPP that we are building for aarch32-linux-gnueabi")
    elseif (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "x86_64" AND "${LABEL}" STREQUAL "Ubuntu GCC 8 x86")
        set(MAES_FLAG "")
        set(CRYPTOPP_I386 ON CACHE BOOL FORCE "Tell CryptoPP that we are building for i386")
    elseif (${CMAKE_SYSTEM_PROCESSOR} MATCHES "x86_64" AND NOT WRKZ_COMPILER_TARGETS_ARM AND NOT "${LABEL}" STREQUAL "aarch64" AND NOT "${LABEL}" STREQUAL "aarch32-linux-gnueabihf" AND NOT "${LABEL}" STREQUAL "aarch32-linux-gnueabi")
        if (ENABLE_X86_AESNI AND NOT NO_AES)
            set(MAES_FLAG "-maes")
        else ()
            set(MAES_FLAG "")
        endif ()
    else ()
        set(MAES_FLAG "")
    endif ()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c11 ${STATICASSERTC_FLAG} ${WARNINGS} ${C_WARNINGS} ${ARCH_FLAG} ${MAES_FLAG}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++20 ${STATICASSERTCPP_FLAG} ${WARNINGS} ${CXX_WARNINGS} ${ARCH_FLAG} ${MAES_FLAG}")

    if (APPLE)
        if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
            # Need to build against libc++ instead of libstc++ on apple
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -nostdinc++ -I/usr/local/opt/llvm/include/c++/v1 -nodefaultlibs -lc++ -lc++abi -lm -lc -lgcc -lgcc_eh")

            # Need these flags so gcc OSX works on the cryptopp ASM - https://groups.google.com/forum/#!topic/cryptopp-users/po8yO-vYaac
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DCRYPTOPP_CLANG_INTEGRATED_ASSEMBLER=1")

        elseif (CMAKE_C_COMPILER_ID STREQUAL "Clang")
            # Need to link against the llvm libc++ library, default is too old for std::filesystem
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++ -I/usr/local/opt/llvm/include/c++/v1")
        endif ()
    endif ()

    if (NOT APPLE AND NOT WIN32)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
    endif ()

    ## Setting up DEBUG flags
    if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
        set(DEBUG_FLAGS "-g3 -Og -gdwarf-4 -fvar-tracking -fvar-tracking-assignments -fno-inline -fno-omit-frame-pointer")
    else ()
        set(DEBUG_FLAGS "-g3 -O0 -fno-omit-frame-pointer")
    endif ()

    ## Setting up RELEASE flags
    if (CONSENSUS_SAFE_BUILD)
        set(RELEASE_FLAGS "-O2 -DNDEBUG -Wno-unused-variable -fno-strict-aliasing")
    else ()
        set(RELEASE_FLAGS "-Ofast -DNDEBUG -Wno-unused-variable")
    endif ()

    if (NOT APPLE)
        # There is a clang bug that does not allow to compile code that uses AES-NI intrinsics if -flto is enabled
        if (CMAKE_C_COMPILER_ID STREQUAL "GNU" AND CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_BUILD_TYPE STREQUAL "Release")
            # On linux, to build in lto mode, check that ld.gold linker is used: 'update-alternatives --install /usr/bin/ld ld /usr/bin/ld.gold HIGHEST_PRIORITY'
            set(CMAKE_AR gcc-ar)
            set(CMAKE_RANLIB gcc-ranlib)
        endif ()
    endif ()

    ## Set up the normal CMake flags as we've built them
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} ${DEBUG_FLAGS}")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${DEBUG_FLAGS}")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} ${RELEASE_FLAGS}")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} ${RELEASE_FLAGS}")

    ## Statically link our binaries
    if (NOT APPLE)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++")
        if (FULLY_STATIC AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
            set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")
            message(STATUS "FULLY_STATIC enabled: linking with -static")
        endif ()
    endif ()
endif ()

## Go get us some static BOOST libraries
set(Boost_NO_BOOST_CMAKE ON)
set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_STATIC_RUNTIME ON)
find_package(Boost REQUIRED COMPONENTS date_time)
message(STATUS "Boost Found: ${Boost_INCLUDE_DIRS}")
include_directories(SYSTEM ${Boost_INCLUDE_DIRS})

## Let's see if we have any OpenSSL static libraries available
set(OPENSSL_USE_STATIC_LIBS ON)
## On MSVC, we need to link the RT/MT libraries
if (MSVC)
    set(OPENSSL_MSVC_STATIC_RT ON)
endif ()

# We have to look for Homebrew OpenSSL a bit differently
# Borrowed from https://github.com/tarantool/tarantool/commit/6eab201af1843f53a833c8928dc58fceffa08147
if (APPLE)
    find_program(HOMEBREW_EXECUTABLE brew)
    execute_process(COMMAND ${HOMEBREW_EXECUTABLE} --prefix openssl
            OUTPUT_VARIABLE HOMEBREW_OPENSSL
            OUTPUT_STRIP_TRAILING_WHITESPACE)
    if (DEFINED HOMEBREW_OPENSSL)
        if (NOT DEFINED OPENSSL_ROOT_DIR)
            message(STATUS "Setting OpenSSL root to ${HOMEBREW_OPENSSL}")
            set(OPENSSL_ROOT_DIR "${HOMEBREW_OPENSSL}")
        endif ()
    endif ()
endif ()

find_package(OpenSSL)

if (NOT OPENSSL_FOUND)
    find_package(PkgConfig)

    if (PKG_CONFIG_FOUND)
        pkg_search_module(OPENSSL openssl)
    endif()
endif()

if (OPENSSL_FOUND)
    ## On non MSVC build systems, we need to link ldl with the static OpenSSL library
    if (NOT MSVC AND NOT WIN32)
        set(OPENSSL_LIBRARIES "${OPENSSL_LIBRARIES};dl")
    endif ()
    if (WIN32)
        # Static OpenSSL for MinGW needs these system libs after libcrypto/libssl.
        set(OPENSSL_LIBRARIES "${OPENSSL_LIBRARIES};crypt32;ws2_32;bcrypt;iphlpapi")
    endif ()

    include_directories(SYSTEM ${OPENSSL_INCLUDE_DIR})
    add_definitions(-DCPPHTTPLIB_OPENSSL_SUPPORT)
    message(STATUS "OpenSSL Found: ${OPENSSL_INCLUDE_DIR}")
    message(STATUS "OpenSSL Libraries: ${OPENSSL_LIBRARIES}")
else ()
    message(STATUS "OpenSSL Found: No... Skipping...")
endif ()

if (ENABLE_ZMQ)
    if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/libzmq/CMakeLists.txt")
        set(WRKZ_USE_BUNDLED_ZMQ ON CACHE BOOL "Use bundled libzmq from external/libzmq" FORCE)
        message(STATUS "ZeroMQ Source: bundled external/libzmq")

        set(WRKZ_ZMQ_LINK_MODE "auto" CACHE STRING "ZeroMQ link mode: auto, shared, static")
        set_property(CACHE WRKZ_ZMQ_LINK_MODE PROPERTY STRINGS auto shared static)

        if (WRKZ_ZMQ_LINK_MODE STREQUAL "auto")
            if (FULLY_STATIC)
                set(WRKZ_ZMQ_LINK_MODE_RESOLVED "static")
            else ()
                set(WRKZ_ZMQ_LINK_MODE_RESOLVED "shared")
            endif ()
        elseif (WRKZ_ZMQ_LINK_MODE STREQUAL "shared" OR WRKZ_ZMQ_LINK_MODE STREQUAL "static")
            set(WRKZ_ZMQ_LINK_MODE_RESOLVED "${WRKZ_ZMQ_LINK_MODE}")
        else ()
            message(WARNING "Unknown WRKZ_ZMQ_LINK_MODE='${WRKZ_ZMQ_LINK_MODE}', falling back to auto")
            if (FULLY_STATIC)
                set(WRKZ_ZMQ_LINK_MODE_RESOLVED "static")
            else ()
                set(WRKZ_ZMQ_LINK_MODE_RESOLVED "shared")
            endif ()
        endif ()

        if (WRKZ_ZMQ_LINK_MODE_RESOLVED STREQUAL "static")
            set(WRKZ_ZMQ_TARGET libzmq-static CACHE STRING "ZeroMQ target used by Wrkzd" FORCE)
        else ()
            set(WRKZ_ZMQ_TARGET libzmq CACHE STRING "ZeroMQ target used by Wrkzd" FORCE)
        endif ()
        message(STATUS "ZeroMQ Link Target: ${WRKZ_ZMQ_TARGET}")
        include_directories(SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}/external/libzmq/include)
    else ()
        message(WARNING "ENABLE_ZMQ=ON but external/libzmq is missing. Disabling ZMQ support for this build.")
        set(ENABLE_ZMQ OFF CACHE BOOL "Enable daemon ZMQ publisher support" FORCE)
    endif ()
endif ()

add_subdirectory(external)
add_subdirectory(src)

## We need to setup the RocksDB build environment to match our system
if (NOT MSVC)
    set(ROCKSDB_ZSTD_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/zstd/lib")
    set(ROCKSDB_ZSTD_LIBRARY "${PROJECT_BINARY_DIR}/external/zstd/build/cmake/lib/libzstd.a")
    set(ROCKSDB_CMAKE_EXTRA_ARGS "")
    if (CMAKE_TOOLCHAIN_FILE)
        list(APPEND ROCKSDB_CMAKE_EXTRA_ARGS "-DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}")
    endif ()
    if (CMAKE_SYSROOT)
        list(APPEND ROCKSDB_CMAKE_EXTRA_ARGS "-DCMAKE_SYSROOT=${CMAKE_SYSROOT}")
    endif ()

    execute_process(
        COMMAND ${CMAKE_COMMAND}
            ${CMAKE_CURRENT_SOURCE_DIR}/external/rocksdb
            -DWITH_ZSTD=ON
            -DWITH_SNAPPY=OFF
            -DWITH_LIBURING=OFF
            -DWITH_GFLAGS=0
            -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
            -DWITH_TESTS=OFF
            -DWITH_TOOLS=OFF
            -DPORTABLE=ON
            -DUSE_RTTI=ON
            -DZSTD_INCLUDE_DIRS=${ROCKSDB_ZSTD_INCLUDE_DIR}
            -DZSTD_LIBRARIES=${ROCKSDB_ZSTD_LIBRARY}
            ${ROCKSDB_CMAKE_EXTRA_ARGS}
            -B${PROJECT_BINARY_DIR}/rocksdb
        RESULT_VARIABLE ROCKSDB_CMAKE_CONFIGURE_RESULT
    )
    if (NOT ROCKSDB_CMAKE_CONFIGURE_RESULT EQUAL 0)
        message(FATAL_ERROR "RocksDB CMake configure failed with code ${ROCKSDB_CMAKE_CONFIGURE_RESULT}")
    endif()
    set_directory_properties(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES "${PROJECT_BINARY_DIR}/rocksdb/librocksdb.a")
endif ()
