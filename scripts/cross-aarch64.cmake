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

# Help FindBoost/FindOpenSSL resolve cross-built deps from CROSS_PREFIX.
set(BOOST_ROOT "${CROSS_PREFIX}" CACHE PATH "Boost root for cross build")
set(_BOOST_INCLUDE_CANDIDATE "${CROSS_PREFIX}/include")
if (NOT EXISTS "${_BOOST_INCLUDE_CANDIDATE}/boost/version.hpp")
  file(GLOB _BOOST_VERSIONED_INCLUDE_DIRS "${CROSS_PREFIX}/include/boost-*")
  foreach(_dir ${_BOOST_VERSIONED_INCLUDE_DIRS})
    if (EXISTS "${_dir}/boost/version.hpp")
      set(_BOOST_INCLUDE_CANDIDATE "${_dir}")
      break()
    endif ()
  endforeach()
endif ()
set(BOOST_INCLUDEDIR "${_BOOST_INCLUDE_CANDIDATE}" CACHE PATH "Boost include dir")
set(Boost_INCLUDE_DIR "${_BOOST_INCLUDE_CANDIDATE}" CACHE PATH "Boost include dir (FindBoost)" FORCE)

set(_BOOST_LIB_CANDIDATE "${CROSS_PREFIX}/lib")
if (EXISTS "${CROSS_PREFIX}/lib64/libboost_date_time.a")
  set(_BOOST_LIB_CANDIDATE "${CROSS_PREFIX}/lib64")
endif ()
set(BOOST_LIBRARYDIR "${_BOOST_LIB_CANDIDATE}" CACHE PATH "Boost library dir")
set(Boost_LIBRARY_DIR "${_BOOST_LIB_CANDIDATE}" CACHE PATH "Boost library dir (FindBoost)" FORCE)
set(Boost_NO_SYSTEM_PATHS ON CACHE BOOL "Disable host boost paths")
set(Boost_USE_STATIC_LIBS ON CACHE BOOL "Use static boost libs")
set(Boost_ADDITIONAL_VERSIONS "1.84" "1.84.0" "1.83" "1.82" CACHE STRING "Boost versions")

set(OPENSSL_ROOT_DIR "${CROSS_PREFIX}" CACHE PATH "OpenSSL root for cross build")
