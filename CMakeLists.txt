# 3.16 is needed for target_precompile_headers(); CONFIGURE_DEPENDS globs,
# $CACHE{} and CMAKE_CXX_STANDARD 20 already required >= 3.12.
cmake_minimum_required(VERSION 3.16)

# MSVC: use the static CRT (/MT, /MTd) for every target, bundled externals
# included. CMP0091 (CMake >= 3.15) replaces the old "/MD" -> "/MT" flag
# substitution with this abstraction; the policy default makes externals that
# declare an older cmake_minimum_required honour it as well.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(CMAKE_POLICY_DEFAULT_CMP0091 NEW)

# Boost discovery policies (CMake >= 3.27 / 3.30):
#  - CMP0144 NEW: honour the upper-case BOOST_ROOT variable the cross-build
#    toolchain files and scripts set.
#  - CMP0167 OLD: keep CMake's FindBoost module. This project deliberately uses
#    it in header-only mode (Boost_NO_BOOST_CMAKE below); switching to NEW
#    would require a BoostConfig.cmake in every build prefix.
if (POLICY CMP0144)
    cmake_policy(SET CMP0144 NEW)
endif ()
if (POLICY CMP0167)
    cmake_policy(SET CMP0167 OLD)
endif ()

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

set(CMAKE_CONFIGURATION_TYPES Debug RelWithDebInfo Release CACHE STRING "Available build configurations")
set(ARCH default CACHE STRING "CPU to build for: -march value or native")
set(CONSENSUS_SAFE_BUILD ON CACHE BOOL "Use reproducible, consensus-safe compiler optimizations")
set(PORTABLE_BINARY ON CACHE BOOL "Prefer portable CPU flags for binaries distributed across different machines")
set(ENABLE_X86_AESNI OFF CACHE BOOL "Enable x86 AES-NI compile flag (-maes); may reduce CPU portability")
set(FULLY_STATIC OFF CACHE BOOL "Attempt fully static executable linkage on Linux (requires static system libs)")
set(ENABLE_ZMQ ON CACHE BOOL "Enable daemon ZMQ publisher support")
set(ROCKSDB_BUILD_PARALLEL "" CACHE STRING "Parallel jobs for RocksDB sub-build (empty=auto)")
set(ROCKSDB_BUILD_PARALLEL_CAP "8" CACHE STRING "Max auto-detected parallel jobs for RocksDB sub-build when no explicit parallel value is provided (MAKEFLAGS -jN / CMAKE_BUILD_PARALLEL_LEVEL / ROCKSDB_BUILD_PARALLEL take precedence)")
option(WRKZ_BUILD_EXECUTABLES "Build CLI/daemon executables" ON)
option(WRKZ_BUILD_WALLET_CAPI "Build generic wallet C API shared library" OFF)
option(WRKZ_BUILD_WALLET_WASM "Build wallet WASM module (Emscripten)" OFF)
option(WRKZ_WASM_PTHREADS "Build wallet WASM with pthread/atomics support" OFF)
option(WRKZ_ANDROID_PROFILE "Enable Android-focused build defaults" OFF)
option(WRKZ_ANDROID_HEADER_ONLY_BOOST "Use Boost headers only on Android builds" ON)
option(WRKZ_ANDROID_DISABLE_OPENSSL "Disable OpenSSL probing/linking on Android builds" ON)
option(WRKZ_FORCE_STATIC_INTERNALS "Link internal libraries statically" ON)
set(LIBUCONTEXT_ROOT "" CACHE PATH "Root of prebuilt libucontext for Android (expects include/ and lib/libucontext.a)")

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
    if (CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 15)
        message(WARNING
            "Clang < 15 on Linux may fail with C++20 <chrono> depending on libstdc++ headers "
            "(known issue: clang-14 with GCC 13 libstdc++). Prefer clang >= 15, GCC, clang+libc++, "
            "or clang with an older GCC toolchain."
        )
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
set(CMAKE_SKIP_INSTALL_RULES OFF)
set(CMAKE_SKIP_PACKAGE_ALL_DEPENDENCY ON)
set(CMAKE_SUPPRESS_REGENERATION ON)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# Enable c++20 (required by RocksDB 10 headers)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Validate the active compiler/standard-library tuple can compile C++20 <chrono>.
# This catches known combinations such as clang-14 with GCC 13 libstdc++ headers.
include(CheckCXXSourceCompiles)
set(CMAKE_REQUIRED_QUIET TRUE)
check_cxx_source_compiles(
    "
    #include <chrono>
    int main() {
      std::chrono::nanoseconds n{1};
      return static_cast<int>(n.count());
    }
    "
    WRKZ_CXX20_CHRONO_WORKS
)
if (NOT WRKZ_CXX20_CHRONO_WORKS)
    message(FATAL_ERROR
        "This compiler/stdlib combination cannot compile C++20 <chrono>.\n"
        "Known bad combo: clang-14 with GCC 13 libstdc++ headers.\n"
        "Use GCC, clang >= 15, clang + libc++, or clang with an older GCC toolchain (e.g. g++-11 headers)."
    )
endif ()

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

if (WRKZ_ANDROID_PROFILE)
    message(STATUS "Applying Android profile defaults")
    set(WRKZ_BUILD_EXECUTABLES OFF CACHE BOOL "Build CLI/daemon executables" FORCE)
    set(ENABLE_ZMQ OFF CACHE BOOL "Enable daemon ZMQ publisher support" FORCE)
endif ()

# Emscripten WASM builds only need the wallet library chain — disable
# executables, ZMQ, and RocksDB which are daemon-only.
if (EMSCRIPTEN AND WRKZ_BUILD_WALLET_WASM)
    message(STATUS "Emscripten WASM build: disabling executables, ZMQ, RocksDB")
    set(WRKZ_BUILD_EXECUTABLES OFF CACHE BOOL "Build CLI/daemon executables" FORCE)
    set(ENABLE_ZMQ OFF CACHE BOOL "Enable daemon ZMQ publisher support" FORCE)
endif ()
message(STATUS "WRKZ_BUILD_EXECUTABLES: ${WRKZ_BUILD_EXECUTABLES}")
message(STATUS "WRKZ_BUILD_WALLET_CAPI: ${WRKZ_BUILD_WALLET_CAPI}")
message(STATUS "WRKZ_BUILD_WALLET_WASM: ${WRKZ_BUILD_WALLET_WASM}")
message(STATUS "WRKZ_WASM_PTHREADS: ${WRKZ_WASM_PTHREADS}")
message(STATUS "WRKZ_ANDROID_PROFILE: ${WRKZ_ANDROID_PROFILE}")
message(STATUS "WRKZ_ANDROID_HEADER_ONLY_BOOST: ${WRKZ_ANDROID_HEADER_ONLY_BOOST}")
message(STATUS "WRKZ_ANDROID_DISABLE_OPENSSL: ${WRKZ_ANDROID_DISABLE_OPENSSL}")
message(STATUS "WRKZ_FORCE_STATIC_INTERNALS: ${WRKZ_FORCE_STATIC_INTERNALS}")

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

## Keep static internal libraries by default for compatibility.
set(STATIC ${WRKZ_FORCE_STATIC_INTERNALS} CACHE BOOL "Link libraries statically?" FORCE)

## This section helps us tag our builds with the git commit information
set(COMMIT_ID_IN_VERSION ON CACHE BOOL "Include commit ID in version")


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
        # Static CRT (/MT) is selected via CMAKE_MSVC_RUNTIME_LIBRARY at the top of this file.
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
elseif (ANDROID)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/platform/linux)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/platform/posix)
    add_definitions(-D_GNU_SOURCE -D_XOPEN_SOURCE=700)
    add_definitions(-DBOOST_NO_CXX98_FUNCTION_BASE -D_LIBCPP_ENABLE_CXX17_REMOVED_UNARY_BINARY_FUNCTION)
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

    ## These options generate all those nice warnings we see while building.
    ## -Werror is deliberately not enabled, so no -Wno-error=... overrides are needed.
    set(WARNINGS "-Wall -Wextra -Wpointer-arith -Wvla -Wwrite-strings -Wno-unused-parameter")
    if (CMAKE_C_COMPILER_ID STREQUAL "Clang")
        set(WARNINGS "${WARNINGS} -Wno-overloaded-shift-op-parentheses")
    else ()
        set(WARNINGS "${WARNINGS} -Wlogical-op")
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
    # The C++ standard flag comes from CMAKE_CXX_STANDARD / CMAKE_CXX_EXTENSIONS above.
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c11 ${WARNINGS} ${C_WARNINGS} ${ARCH_FLAG} ${MAES_FLAG}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${WARNINGS} ${CXX_WARNINGS} ${ARCH_FLAG} ${MAES_FLAG}")

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

    if (NOT APPLE AND NOT WIN32 AND NOT EMSCRIPTEN)
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

    ## Optional faster linker for GCC/Clang builds (mold, lld, gold). Opt-in:
    ## the toolchain default is kept unless the requested linker proves to work.
    set(WRKZ_LINKER "" CACHE STRING "Alternative linker (mold, lld, gold); empty = toolchain default")
    if (WRKZ_LINKER)
        set(CMAKE_REQUIRED_LINK_OPTIONS "-fuse-ld=${WRKZ_LINKER}")
        check_cxx_source_compiles("int main() { return 0; }" WRKZ_LINKER_WORKS_${WRKZ_LINKER})
        unset(CMAKE_REQUIRED_LINK_OPTIONS)
        if (WRKZ_LINKER_WORKS_${WRKZ_LINKER})
            add_link_options("-fuse-ld=${WRKZ_LINKER}")
            message(STATUS "Linker: ${WRKZ_LINKER} (-fuse-ld=${WRKZ_LINKER})")
        else ()
            message(WARNING "WRKZ_LINKER=${WRKZ_LINKER} requested, but -fuse-ld=${WRKZ_LINKER} does not link here; keeping the toolchain default.")
        endif ()
    endif ()

    ## Set up the normal CMake flags as we've built them
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} ${DEBUG_FLAGS}")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${DEBUG_FLAGS}")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} ${RELEASE_FLAGS}")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} ${RELEASE_FLAGS}")

    ## Statically link our binaries where it is supported.
    if (NOT APPLE AND NOT ANDROID)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++")
        if (FULLY_STATIC AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
            set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")
            message(STATUS "FULLY_STATIC enabled: linking with -static")
        endif ()
    endif ()

    ## Strip symbols from Release binaries on Linux to reduce binary size.
    ## A fully-static Release build without stripping can exceed 200 MB due to
    ## debug sections from bundled static libs; stripping brings it down to ~30-50 MB.
    if (CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_BUILD_TYPE STREQUAL "Release")
        set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -Wl,-s")
        message(STATUS "Release build: stripping symbols from executables (-Wl,-s)")
    endif ()
endif ()

## Go get us some static BOOST libraries
set(Boost_NO_BOOST_CMAKE ON)
if (WRKZ_ANDROID_PROFILE)
    set(Boost_USE_STATIC_LIBS OFF)
    set(Boost_USE_STATIC_RUNTIME OFF)
else ()
    set(Boost_USE_STATIC_LIBS ON)
    set(Boost_USE_STATIC_RUNTIME ON)
endif ()
if ((WRKZ_ANDROID_PROFILE AND WRKZ_BUILD_WALLET_CAPI AND NOT WRKZ_BUILD_EXECUTABLES)
    OR (ANDROID AND WRKZ_ANDROID_HEADER_ONLY_BOOST))
    # Android builds can run with Boost headers only.
    if (NOT DEFINED BOOST_ROOT AND DEFINED ENV{BOOST_ROOT})
        set(BOOST_ROOT "$ENV{BOOST_ROOT}")
    endif ()

    set(_WRKZ_BOOST_HEADER_CANDIDATES "")
    if (DEFINED BOOST_ROOT)
        list(APPEND _WRKZ_BOOST_HEADER_CANDIDATES "${BOOST_ROOT}/include" "${BOOST_ROOT}")
    endif ()

    set(Boost_INCLUDE_DIRS "")
    foreach(_WRKZ_BOOST_DIR IN LISTS _WRKZ_BOOST_HEADER_CANDIDATES)
        if (EXISTS "${_WRKZ_BOOST_DIR}/boost/version.hpp")
            set(Boost_INCLUDE_DIRS "${_WRKZ_BOOST_DIR}")
            break()
        endif ()
    endforeach ()

    if (Boost_INCLUDE_DIRS STREQUAL "")
        message(FATAL_ERROR
            "Android build requires Boost headers but none were found.\n"
            "Set BOOST_ROOT to an isolated Boost headers prefix containing include/boost/version.hpp.\n"
            "Do not pass /usr/include directly in cross builds.")
    endif ()

    # Normalize include root: if the probe resolved to ".../boost", use its parent.
    if (EXISTS "${Boost_INCLUDE_DIRS}/version.hpp" AND EXISTS "${Boost_INCLUDE_DIRS}/uuid/uuid.hpp")
        get_filename_component(Boost_INCLUDE_DIRS "${Boost_INCLUDE_DIRS}" DIRECTORY)
    endif ()

    if (
        NOT EXISTS "${Boost_INCLUDE_DIRS}/boost/version.hpp"
        OR NOT EXISTS "${Boost_INCLUDE_DIRS}/boost/uuid/uuid.hpp"
        OR NOT EXISTS "${Boost_INCLUDE_DIRS}/boost/variant.hpp"
    )
        message(FATAL_ERROR
            "Boost include root is invalid for Android build: ${Boost_INCLUDE_DIRS}\n"
            "Expected headers:\n"
            "  ${Boost_INCLUDE_DIRS}/boost/version.hpp\n"
            "  ${Boost_INCLUDE_DIRS}/boost/uuid/uuid.hpp\n"
            "  ${Boost_INCLUDE_DIRS}/boost/variant.hpp\n"
            "Set BOOST_ROOT to a valid isolated Boost prefix.")
    endif ()
    message(STATUS "Boost headers for Android build: ${Boost_INCLUDE_DIRS}")

    set(WRKZ_BOOST_LINK_LIBS "")
else ()
    if (EMSCRIPTEN)
        set(_WRKZ_BOOST_HEADER_CANDIDATES "")
        if (DEFINED BOOST_ROOT)
            list(APPEND _WRKZ_BOOST_HEADER_CANDIDATES "${BOOST_ROOT}/include" "${BOOST_ROOT}")
        endif ()
        list(APPEND _WRKZ_BOOST_HEADER_CANDIDATES
            "/usr/include"
            "/usr/local/include"
            "/opt/homebrew/include"
            "/opt/local/include")

        set(Boost_INCLUDE_DIRS "")
        foreach(_WRKZ_BOOST_DIR IN LISTS _WRKZ_BOOST_HEADER_CANDIDATES)
            if (EXISTS "${_WRKZ_BOOST_DIR}/boost/version.hpp")
                set(Boost_INCLUDE_DIRS "${_WRKZ_BOOST_DIR}")
                break()
            endif ()
        endforeach ()

        if (Boost_INCLUDE_DIRS STREQUAL "")
            message(FATAL_ERROR
                "Emscripten wallet_wasm build requires Boost headers but none were found.\n"
                "Set BOOST_ROOT to a prefix containing include/boost/version.hpp.")
        endif ()

        # Avoid injecting full host include roots (e.g. /usr/include) into the
        # Emscripten compile command, which can shadow sysroot libc headers.
        # Build a shim include dir that only exposes boost/*.
        set(_WRKZ_BOOST_SHIM_ROOT "${CMAKE_BINARY_DIR}/boost-headers")
        file(MAKE_DIRECTORY "${_WRKZ_BOOST_SHIM_ROOT}")
        if (NOT EXISTS "${_WRKZ_BOOST_SHIM_ROOT}/boost")
            # Use file(COPY) for portability; symlinks require elevated privileges on Windows
            file(COPY "${Boost_INCLUDE_DIRS}/boost" DESTINATION "${_WRKZ_BOOST_SHIM_ROOT}")
        endif ()
        set(Boost_INCLUDE_DIRS "${_WRKZ_BOOST_SHIM_ROOT}")

        message(STATUS "Boost headers for Emscripten wallet_wasm: ${Boost_INCLUDE_DIRS}")
        set(WRKZ_BOOST_LINK_LIBS "")
    else ()
        # Only Boost headers are needed — boost::posix_time::ptime and
        # microsec_clock::local_time() are fully header-only (they use
        # Windows/POSIX APIs directly).  The compiled date_time library is
        # only required for locale-based I/O which this codebase does not use.
        find_package(Boost REQUIRED)
        set(WRKZ_BOOST_LINK_LIBS "")
        # Suppress MSVC #pragma comment(lib, "boost_date_time-...") auto-link.
        add_compile_definitions(BOOST_DATE_TIME_NO_LIB BOOST_ALL_NO_LIB)
    endif ()
endif ()
if ((NOT Boost_INCLUDE_DIRS OR Boost_INCLUDE_DIRS STREQUAL "") AND DEFINED Boost_INCLUDE_DIR)
    if (NOT "${Boost_INCLUDE_DIR}" MATCHES "-NOTFOUND$")
        set(Boost_INCLUDE_DIRS "${Boost_INCLUDE_DIR}")
    endif ()
endif ()
if (Boost_INCLUDE_DIRS AND EXISTS "${Boost_INCLUDE_DIRS}/version.hpp" AND EXISTS "${Boost_INCLUDE_DIRS}/uuid/uuid.hpp")
    get_filename_component(Boost_INCLUDE_DIRS "${Boost_INCLUDE_DIRS}" DIRECTORY)
endif ()
message(STATUS "Boost Found: ${Boost_INCLUDE_DIRS}")
include_directories(SYSTEM ${Boost_INCLUDE_DIRS})

## Detect ZLIB before OpenSSL.
## Static OpenSSL on Linux/macOS commonly has a transitive zlib dependency.
## CMake's FindOpenSSL appends ZLIB_LIBRARY to OPENSSL_LIBRARIES; if zlib-dev
## is absent that becomes ZLIB_LIBRARY-NOTFOUND which causes a CMake generate
## failure for every target that links OpenSSL.  Pre-finding ZLIB here allows
## FindOpenSSL to resolve it correctly, and our cleanup block below removes
## stray NOTFOUND entries for systems where zlib-dev is not installed.
##
## Debian/Ubuntu: sudo apt install zlib1g-dev
## Fedora/RHEL:   sudo dnf install zlib-devel
## macOS:         zlib ships with Xcode CLT (no extra step needed)
if (NOT EMSCRIPTEN AND NOT (ANDROID AND WRKZ_ANDROID_DISABLE_OPENSSL))
    if (FULLY_STATIC AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
        ## Force static zlib when building a fully-static binary; otherwise
        ## find_package(ZLIB) returns libz.so which the static linker rejects.
        set(_wrkz_saved_suffixes ${CMAKE_FIND_LIBRARY_SUFFIXES})
        set(CMAKE_FIND_LIBRARY_SUFFIXES .a)
        find_package(ZLIB QUIET)
        set(CMAKE_FIND_LIBRARY_SUFFIXES ${_wrkz_saved_suffixes})
        unset(_wrkz_saved_suffixes)
    else ()
        find_package(ZLIB QUIET)
    endif ()
    if (ZLIB_FOUND)
        message(STATUS "Found ZLIB: ${ZLIB_LIBRARIES}")

        ## Let cpp-httplib negotiate gzip on both the server and the client.
        ## Wallet sync responses are hex encoded keys and hashes wrapped in
        ## JSON, which compresses several fold. It is opt in per request - the
        ## server only spends the CPU when a client sends Accept-Encoding: gzip,
        ## and older clients that do not are served uncompressed as before.
        add_definitions(-DCPPHTTPLIB_ZLIB_SUPPORT)
        include_directories(SYSTEM ${ZLIB_INCLUDE_DIRS})
        message(STATUS "  HTTP gzip compression enabled for RPC")
    else ()
        message(STATUS "ZLIB not found (zlib1g-dev / zlib-devel not installed).")
        message(STATUS "  If the build fails with ZLIB_LIBRARY-NOTFOUND, install the zlib development package.")
    endif ()
endif ()

## Let's see if we have any OpenSSL static libraries available
if (WRKZ_ANDROID_PROFILE)
    set(OPENSSL_USE_STATIC_LIBS OFF)
else ()
    set(OPENSSL_USE_STATIC_LIBS ON)
endif ()
## On MSVC, we need to link the RT/MT libraries
if (MSVC)
    set(OPENSSL_MSVC_STATIC_RT ON)
endif ()

# We have to look for Homebrew OpenSSL a bit differently
# Borrowed from https://github.com/tarantool/tarantool/commit/6eab201af1843f53a833c8928dc58fceffa08147
if (APPLE)
    find_program(HOMEBREW_EXECUTABLE brew)
    if (HOMEBREW_EXECUTABLE)
        execute_process(COMMAND ${HOMEBREW_EXECUTABLE} --prefix openssl
                OUTPUT_VARIABLE HOMEBREW_OPENSSL
                OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET)
        if (HOMEBREW_OPENSSL AND NOT DEFINED OPENSSL_ROOT_DIR)
            message(STATUS "Setting OpenSSL root to ${HOMEBREW_OPENSSL}")
            set(OPENSSL_ROOT_DIR "${HOMEBREW_OPENSSL}")
        endif ()
    endif ()
endif ()

if (EMSCRIPTEN OR (ANDROID AND WRKZ_ANDROID_DISABLE_OPENSSL))
    set(OPENSSL_FOUND FALSE)
else ()
    find_package(OpenSSL)

    if (NOT OPENSSL_FOUND)
        find_package(PkgConfig)

        if (PKG_CONFIG_FOUND)
            pkg_search_module(OPENSSL openssl)
        endif()
    endif()
endif ()

if (OPENSSL_FOUND)
    set(WRKZ_OPENSSL_INCLUDE_DIRS "")

    if (DEFINED OPENSSL_INCLUDE_DIRS)
        foreach(_WRKZ_SSL_DIR IN LISTS OPENSSL_INCLUDE_DIRS)
            if (NOT "${_WRKZ_SSL_DIR}" MATCHES "-NOTFOUND$")
                list(APPEND WRKZ_OPENSSL_INCLUDE_DIRS "${_WRKZ_SSL_DIR}")
            endif ()
        endforeach ()
    endif ()

    if (DEFINED OPENSSL_INCLUDE_DIR AND NOT "${OPENSSL_INCLUDE_DIR}" MATCHES "-NOTFOUND$")
        list(APPEND WRKZ_OPENSSL_INCLUDE_DIRS "${OPENSSL_INCLUDE_DIR}")
    endif ()

    list(REMOVE_DUPLICATES WRKZ_OPENSSL_INCLUDE_DIRS)

    if (WRKZ_OPENSSL_INCLUDE_DIRS)
        ## On non MSVC build systems, we need to link ldl with the static OpenSSL library
        if (NOT MSVC AND NOT WIN32)
            set(OPENSSL_LIBRARIES "${OPENSSL_LIBRARIES};dl")
        endif ()
        if (WIN32)
            # Static OpenSSL for MinGW needs these system libs after libcrypto/libssl.
            set(OPENSSL_LIBRARIES "${OPENSSL_LIBRARIES};crypt32;ws2_32;bcrypt;iphlpapi")
        endif ()

        ## CMake's FindOpenSSL may inject ZLIB_LIBRARY-NOTFOUND into
        ## OPENSSL_LIBRARIES when zlib-dev is absent.  Strip NOTFOUND entries
        ## and explicitly add zlib when available so static OpenSSL links.
        if (NOT WIN32 AND NOT EMSCRIPTEN)
            list(FILTER OPENSSL_LIBRARIES EXCLUDE REGEX "NOTFOUND")
            if (ZLIB_FOUND)
                list(APPEND OPENSSL_LIBRARIES ${ZLIB_LIBRARIES})
            endif ()
        endif ()

        include_directories(SYSTEM ${WRKZ_OPENSSL_INCLUDE_DIRS})
        add_definitions(-DCPPHTTPLIB_OPENSSL_SUPPORT)
        message(STATUS "OpenSSL Found: ${WRKZ_OPENSSL_INCLUDE_DIRS}")
        message(STATUS "OpenSSL Libraries: ${OPENSSL_LIBRARIES}")
    else ()
        message(WARNING
            "OpenSSL was detected but include directories are invalid. "
            "Disabling OpenSSL integration for this build.")
        set(OPENSSL_FOUND FALSE)
        set(OPENSSL_LIBRARIES "")
    endif ()
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
            set(WRKZ_ZMQ_LINK_MODE_RESOLVED "static")
        elseif (WRKZ_ZMQ_LINK_MODE STREQUAL "shared" OR WRKZ_ZMQ_LINK_MODE STREQUAL "static")
            set(WRKZ_ZMQ_LINK_MODE_RESOLVED "${WRKZ_ZMQ_LINK_MODE}")
        else ()
            message(WARNING "Unknown WRKZ_ZMQ_LINK_MODE='${WRKZ_ZMQ_LINK_MODE}', falling back to auto")
            set(WRKZ_ZMQ_LINK_MODE_RESOLVED "static")
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

# When building wallet_capi (shared lib), every static library linked into it
# must be compiled with -fPIC.  Set the default BEFORE external deps are created
# so their targets inherit it at creation time (more reliable than setting the
# target property after the fact from src/CMakeLists.txt).
if (WRKZ_BUILD_WALLET_CAPI AND NOT WIN32)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif ()

add_subdirectory(external)

# Optional Emscripten pthread builds require every linked object to be compiled
# with atomics/bulk-memory support, including external static libraries.
if (EMSCRIPTEN AND WRKZ_BUILD_WALLET_WASM AND WRKZ_WASM_PTHREADS)
    # Pthread WASM requires every linked object to be built with atomics and
    # bulk-memory features, not only final link target.
    add_compile_options(
        -pthread
        -matomics
        -mbulk-memory
    )
    add_link_options(
        -pthread
    )

    foreach(_wrkz_pthread_target IN ITEMS cryptopp-object cryptopp-static argon2 upnpc-static)
        if (TARGET ${_wrkz_pthread_target})
            target_compile_options(${_wrkz_pthread_target} PRIVATE
                -pthread
                -matomics
                -mbulk-memory
            )
        endif ()
    endforeach ()
endif ()

add_subdirectory(src)

## We need to setup the RocksDB build environment to match our system
## RocksDB is only linked by the daemon: skip it (and its configure step) for
## wallet-only builds (WRKZ_BUILD_EXECUTABLES=OFF) and for Emscripten.
if (WRKZ_BUILD_EXECUTABLES AND NOT MSVC AND NOT EMSCRIPTEN)
    set(ROCKSDB_ZSTD_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/zstd/lib")
    set(ROCKSDB_ZSTD_LIBRARY "${PROJECT_BINARY_DIR}/external/zstd/build/cmake/lib/libzstd.a")
    set(ROCKSDB_CMAKE_EXTRA_ARGS "")
    if (CMAKE_TOOLCHAIN_FILE)
        list(APPEND ROCKSDB_CMAKE_EXTRA_ARGS "-DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}")
    endif ()
    if (CMAKE_SYSROOT)
        list(APPEND ROCKSDB_CMAKE_EXTRA_ARGS "-DCMAKE_SYSROOT=${CMAKE_SYSROOT}")
    endif ()
    if (ANDROID)
        list(APPEND ROCKSDB_CMAKE_EXTRA_ARGS "-DANDROID=ON")
        if (DEFINED ANDROID_ABI AND NOT "${ANDROID_ABI}" STREQUAL "")
            list(APPEND ROCKSDB_CMAKE_EXTRA_ARGS "-DANDROID_ABI=${ANDROID_ABI}")
        endif ()
        if (DEFINED ANDROID_PLATFORM AND NOT "${ANDROID_PLATFORM}" STREQUAL "")
            list(APPEND ROCKSDB_CMAKE_EXTRA_ARGS "-DANDROID_PLATFORM=${ANDROID_PLATFORM}")
        endif ()
        if (DEFINED ANDROID_NDK AND NOT "${ANDROID_NDK}" STREQUAL "")
            list(APPEND ROCKSDB_CMAKE_EXTRA_ARGS "-DANDROID_NDK=${ANDROID_NDK}")
        endif ()
        if (DEFINED CMAKE_ANDROID_NDK AND NOT "${CMAKE_ANDROID_NDK}" STREQUAL "")
            list(APPEND ROCKSDB_CMAKE_EXTRA_ARGS "-DCMAKE_ANDROID_NDK=${CMAKE_ANDROID_NDK}")
        endif ()
        if (DEFINED CMAKE_ANDROID_ARCH_ABI AND NOT "${CMAKE_ANDROID_ARCH_ABI}" STREQUAL "")
            list(APPEND ROCKSDB_CMAKE_EXTRA_ARGS "-DCMAKE_ANDROID_ARCH_ABI=${CMAKE_ANDROID_ARCH_ABI}")
        endif ()
        if (DEFINED CMAKE_ANDROID_API AND NOT "${CMAKE_ANDROID_API}" STREQUAL "")
            list(APPEND ROCKSDB_CMAKE_EXTRA_ARGS "-DCMAKE_ANDROID_API=${CMAKE_ANDROID_API}")
        endif ()
        if (DEFINED CMAKE_ANDROID_STL_TYPE AND NOT "${CMAKE_ANDROID_STL_TYPE}" STREQUAL "")
            list(APPEND ROCKSDB_CMAKE_EXTRA_ARGS "-DCMAKE_ANDROID_STL_TYPE=${CMAKE_ANDROID_STL_TYPE}")
        endif ()
    endif ()
    # Pass generator so sub-cmake doesn't fall back to VS on Windows
    list(APPEND ROCKSDB_CMAKE_EXTRA_ARGS "-G${CMAKE_GENERATOR}")
    if (CMAKE_MAKE_PROGRAM)
        list(APPEND ROCKSDB_CMAKE_EXTRA_ARGS "-DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}")
    endif ()
    # Emscripten pthreads: provide thread primitives so RocksDB configure succeeds
    if (EMSCRIPTEN AND WRKZ_WASM_PTHREADS)
        list(APPEND ROCKSDB_CMAKE_EXTRA_ARGS
            "-DCMAKE_HAVE_LIBC_PTHREAD=ON"
            "-DCMAKE_THREAD_LIBS_INIT=-pthread"
        )
    endif ()

    set(ROCKSDB_CMAKE_ARGS
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
        # Build RocksDB with the same compilers as the rest of the tree.
        -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
        -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
        ${ROCKSDB_CMAKE_EXTRA_ARGS}
        -B${PROJECT_BINARY_DIR}/rocksdb
    )
    # RocksDB's own configure is slow (dozens of try_compiles). Only re-run it
    # when the sub-build has never been configured or our arguments changed;
    # RocksDB's generated build system re-checks its own inputs at build time.
    string(SHA1 _WRKZ_ROCKSDB_ARGS_HASH "${ROCKSDB_CMAKE_ARGS}")
    set(_WRKZ_ROCKSDB_STAMP "${PROJECT_BINARY_DIR}/rocksdb/wrkz-configure.stamp")
    set(_WRKZ_ROCKSDB_CONFIGURE TRUE)
    if (EXISTS "${PROJECT_BINARY_DIR}/rocksdb/CMakeCache.txt" AND EXISTS "${_WRKZ_ROCKSDB_STAMP}")
        file(READ "${_WRKZ_ROCKSDB_STAMP}" _WRKZ_ROCKSDB_PREV_HASH)
        if (_WRKZ_ROCKSDB_PREV_HASH STREQUAL _WRKZ_ROCKSDB_ARGS_HASH)
            set(_WRKZ_ROCKSDB_CONFIGURE FALSE)
        endif ()
    endif ()
    if (_WRKZ_ROCKSDB_CONFIGURE)
        execute_process(
            COMMAND ${CMAKE_COMMAND} ${ROCKSDB_CMAKE_ARGS}
            RESULT_VARIABLE ROCKSDB_CMAKE_CONFIGURE_RESULT
        )
        if (NOT ROCKSDB_CMAKE_CONFIGURE_RESULT EQUAL 0)
            message(FATAL_ERROR "RocksDB CMake configure failed with code ${ROCKSDB_CMAKE_CONFIGURE_RESULT}")
        endif ()
        file(WRITE "${_WRKZ_ROCKSDB_STAMP}" "${_WRKZ_ROCKSDB_ARGS_HASH}")
    else ()
        message(STATUS "RocksDB: sub-build already configured with identical arguments, skipping configure")
    endif ()
    unset(_WRKZ_ROCKSDB_CONFIGURE)
    unset(_WRKZ_ROCKSDB_PREV_HASH)
    set_directory_properties(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES "${PROJECT_BINARY_DIR}/rocksdb/librocksdb.a")
endif ()
