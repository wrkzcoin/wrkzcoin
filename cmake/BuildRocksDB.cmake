# RocksDB nested build wrapper.
# Priority:
# 1) ROCKSDB_BUILD_PARALLEL (explicit cache/config value)
# 2) CMAKE_BUILD_PARALLEL_LEVEL from environment
# 3) MAKEFLAGS -jN from parent make invocation
# 4) Configure-time fallback (CPU count capped by ROCKSDB_BUILD_PARALLEL_CAP)

if (NOT DEFINED ROCKSDB_BUILD_DIR OR ROCKSDB_BUILD_DIR STREQUAL "")
    message(FATAL_ERROR "ROCKSDB_BUILD_DIR is required")
endif ()

if (NOT DEFINED ROCKSDB_TARGET OR ROCKSDB_TARGET STREQUAL "")
    set(ROCKSDB_TARGET rocksdb)
endif ()

set(_wrkz_jobs "")
set(_wrkz_source "")

if (DEFINED ROCKSDB_BUILD_PARALLEL AND NOT ROCKSDB_BUILD_PARALLEL STREQUAL "")
    set(_wrkz_jobs "${ROCKSDB_BUILD_PARALLEL}")
    set(_wrkz_source "ROCKSDB_BUILD_PARALLEL")
elseif (DEFINED ENV{CMAKE_BUILD_PARALLEL_LEVEL} AND NOT "$ENV{CMAKE_BUILD_PARALLEL_LEVEL}" STREQUAL "")
    set(_wrkz_jobs "$ENV{CMAKE_BUILD_PARALLEL_LEVEL}")
    set(_wrkz_source "CMAKE_BUILD_PARALLEL_LEVEL")
elseif (DEFINED ENV{MAKEFLAGS} AND NOT "$ENV{MAKEFLAGS}" STREQUAL "")
    if ("$ENV{MAKEFLAGS}" MATCHES "(^|[ ;])-j([0-9]+)($|[ ;])")
        set(_wrkz_jobs "${CMAKE_MATCH_2}")
        set(_wrkz_source "MAKEFLAGS")
    elseif ("$ENV{MAKEFLAGS}" MATCHES "(^|[ ;])-j[ ]+([0-9]+)($|[ ;])")
        set(_wrkz_jobs "${CMAKE_MATCH_2}")
        set(_wrkz_source "MAKEFLAGS")
    endif ()
endif ()

if (_wrkz_jobs STREQUAL "")
    if (DEFINED ROCKSDB_FALLBACK_PARALLEL AND NOT ROCKSDB_FALLBACK_PARALLEL STREQUAL "")
        set(_wrkz_jobs "${ROCKSDB_FALLBACK_PARALLEL}")
        set(_wrkz_source "fallback")
    else ()
        set(_wrkz_jobs "1")
        set(_wrkz_source "default")
    endif ()
endif ()

if (NOT _wrkz_jobs MATCHES "^[0-9]+$" OR _wrkz_jobs LESS 1)
    message(FATAL_ERROR "Invalid RocksDB parallel jobs value: '${_wrkz_jobs}'")
endif ()

message(STATUS "RocksDB build jobs resolved: ${_wrkz_jobs} (source=${_wrkz_source})")

execute_process(
    COMMAND ${CMAKE_COMMAND} --build ${ROCKSDB_BUILD_DIR} --target ${ROCKSDB_TARGET} --parallel ${_wrkz_jobs}
    RESULT_VARIABLE _wrkz_build_result
)

if (NOT _wrkz_build_result EQUAL 0)
    message(FATAL_ERROR "RocksDB build failed with code ${_wrkz_build_result}")
endif ()
