message(STATUS "Activating Cross-Compiler: aarch64-linux-gnu")
message(STATUS "aarch64 toolchain file: ${CMAKE_CURRENT_LIST_FILE}")

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

if (DEFINED ENV{TARGET_TRIPLE})
    set(TARGET_TRIPLE "$ENV{TARGET_TRIPLE}")
else ()
    set(TARGET_TRIPLE "aarch64-linux-gnu")
endif ()

set(CMAKE_C_COMPILER "/usr/bin/${TARGET_TRIPLE}-gcc")
set(CMAKE_CXX_COMPILER "/usr/bin/${TARGET_TRIPLE}-g++")
set(CMAKE_AR "/usr/bin/${TARGET_TRIPLE}-ar")
set(CMAKE_RANLIB "/usr/bin/${TARGET_TRIPLE}-ranlib")
set(CMAKE_STRIP "/usr/bin/${TARGET_TRIPLE}-strip")

if (DEFINED ENV{CROSS_PREFIX})
    set(CROSS_PREFIX "$ENV{CROSS_PREFIX}")
else ()
    set(CROSS_PREFIX "$ENV{HOME}/toolchain/aarch64-linux-gnu/prefix")
endif ()
message(STATUS "aarch64 cross prefix: ${CROSS_PREFIX}")

set(CMAKE_FIND_ROOT_PATH
    "/usr/${TARGET_TRIPLE}"
    "${CROSS_PREFIX}"
)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Help FindOpenSSL resolve cross-built deps from CROSS_PREFIX.
set(OPENSSL_ROOT_DIR "${CROSS_PREFIX}" CACHE PATH "OpenSSL root for cross build")
