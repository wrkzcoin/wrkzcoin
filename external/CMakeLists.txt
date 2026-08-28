set(UPNPC_BUILD_STATIC ON CACHE BOOL "Build static library")
set(UPNPC_BUILD_SHARED OFF CACHE BOOL "Build shared library")
set(UPNPC_BUILD_TESTS OFF CACHE BOOL "Build test executables")
# miniupnpc >= 2.2 builds upnpc/listdevices samples by default and names the
# sample executable "upnpc-static" - the same name its 2.1 static library used.
# We only want the library, so keep the samples off.
set(UPNPC_BUILD_SAMPLE OFF CACHE BOOL "Build sample executables")
set(UPNPC_NO_INSTALL ON CACHE BOOL "Disable miniupnpc installation rules")
set(WITH_MD_LIBRARY OFF CACHE BOOL "Code Generation for static library")

# CMake >= 4.0 refuses cmake_minimum_required() < 3.5, which the bundled
# argon2 (2.6) and libzmq (3.0.2...) still declare. Treat
# them as 3.5-era projects; older CMake ignores this variable.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

# Disable warnings
if (NOT MSVC)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -w -Wno-everything")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -w -Wno-everything")
endif()

add_subdirectory(miniupnpc)
add_subdirectory(argon2)

if (ENABLE_ZMQ AND WRKZ_USE_BUNDLED_ZMQ)
    if (DEFINED CACHE{BUILD_SHARED})
        set(_WRKZ_PREV_BUILD_SHARED "$CACHE{BUILD_SHARED}")
        set(_WRKZ_HAD_BUILD_SHARED TRUE)
    else ()
        set(_WRKZ_HAD_BUILD_SHARED FALSE)
    endif ()
    if (DEFINED CACHE{BUILD_STATIC})
        set(_WRKZ_PREV_BUILD_STATIC "$CACHE{BUILD_STATIC}")
        set(_WRKZ_HAD_BUILD_STATIC TRUE)
    else ()
        set(_WRKZ_HAD_BUILD_STATIC FALSE)
    endif ()
    if (DEFINED CACHE{BUILD_TESTS})
        set(_WRKZ_PREV_BUILD_TESTS "$CACHE{BUILD_TESTS}")
        set(_WRKZ_HAD_BUILD_TESTS TRUE)
    else ()
        set(_WRKZ_HAD_BUILD_TESTS FALSE)
    endif ()
    if (DEFINED CACHE{WITH_PERF_TOOL})
        set(_WRKZ_PREV_WITH_PERF_TOOL "$CACHE{WITH_PERF_TOOL}")
        set(_WRKZ_HAD_WITH_PERF_TOOL TRUE)
    else ()
        set(_WRKZ_HAD_WITH_PERF_TOOL FALSE)
    endif ()
    if (DEFINED CACHE{WITH_DOC})
        set(_WRKZ_PREV_WITH_DOC "$CACHE{WITH_DOC}")
        set(_WRKZ_HAD_WITH_DOC TRUE)
    else ()
        set(_WRKZ_HAD_WITH_DOC FALSE)
    endif ()
    if (DEFINED CACHE{WITH_DOCS})
        set(_WRKZ_PREV_WITH_DOCS "$CACHE{WITH_DOCS}")
        set(_WRKZ_HAD_WITH_DOCS TRUE)
    else ()
        set(_WRKZ_HAD_WITH_DOCS FALSE)
    endif ()
    if (DEFINED CACHE{ENABLE_CURVE})
        set(_WRKZ_PREV_ENABLE_CURVE "$CACHE{ENABLE_CURVE}")
        set(_WRKZ_HAD_ENABLE_CURVE TRUE)
    else ()
        set(_WRKZ_HAD_ENABLE_CURVE FALSE)
    endif ()
    if (DEFINED CACHE{WITH_LIBBSD})
        set(_WRKZ_PREV_WITH_LIBBSD "$CACHE{WITH_LIBBSD}")
        set(_WRKZ_HAD_WITH_LIBBSD TRUE)
    else ()
        set(_WRKZ_HAD_WITH_LIBBSD FALSE)
    endif ()

    set(BUILD_SHARED OFF CACHE BOOL "Disable shared libzmq" FORCE)
    set(BUILD_STATIC ON CACHE BOOL "Build static libzmq" FORCE)
    set(BUILD_TESTS OFF CACHE BOOL "Disable libzmq tests" FORCE)
    set(WITH_PERF_TOOL OFF CACHE BOOL "Disable libzmq perf tools" FORCE)
    set(WITH_DOC OFF CACHE BOOL "Disable libzmq docs" FORCE)
    set(WITH_DOCS OFF CACHE BOOL "Disable libzmq html docs" FORCE)
    set(ENABLE_CURVE OFF CACHE BOOL "Disable libzmq CURVE for bundled build" FORCE)
    # Avoid host/target mismatch in cross builds (e.g. pkg-config finding host libbsd).
    # libzmq has a builtin strlcpy fallback when libbsd is disabled.
    set(WITH_LIBBSD OFF CACHE BOOL "Disable libzmq libbsd for bundled builds" FORCE)
    add_subdirectory(libzmq EXCLUDE_FROM_ALL)

    if (_WRKZ_HAD_BUILD_SHARED)
        set(BUILD_SHARED "${_WRKZ_PREV_BUILD_SHARED}" CACHE BOOL "Build shared library" FORCE)
    else ()
        unset(BUILD_SHARED CACHE)
    endif ()
    if (_WRKZ_HAD_BUILD_STATIC)
        set(BUILD_STATIC "${_WRKZ_PREV_BUILD_STATIC}" CACHE BOOL "Build static library" FORCE)
    else ()
        unset(BUILD_STATIC CACHE)
    endif ()
    if (_WRKZ_HAD_BUILD_TESTS)
        set(BUILD_TESTS "${_WRKZ_PREV_BUILD_TESTS}" CACHE BOOL "Build tests" FORCE)
    else ()
        unset(BUILD_TESTS CACHE)
    endif ()
    if (_WRKZ_HAD_WITH_PERF_TOOL)
        set(WITH_PERF_TOOL "${_WRKZ_PREV_WITH_PERF_TOOL}" CACHE BOOL "Build with perf-tools" FORCE)
    else ()
        unset(WITH_PERF_TOOL CACHE)
    endif ()
    if (_WRKZ_HAD_WITH_DOC)
        set(WITH_DOC "${_WRKZ_PREV_WITH_DOC}" CACHE BOOL "Build Reference Guide documentation" FORCE)
    else ()
        unset(WITH_DOC CACHE)
    endif ()
    if (_WRKZ_HAD_WITH_DOCS)
        set(WITH_DOCS "${_WRKZ_PREV_WITH_DOCS}" CACHE BOOL "Build html docs" FORCE)
    else ()
        unset(WITH_DOCS CACHE)
    endif ()
    if (_WRKZ_HAD_ENABLE_CURVE)
        set(ENABLE_CURVE "${_WRKZ_PREV_ENABLE_CURVE}" CACHE BOOL "Enable CURVE security" FORCE)
    else ()
        unset(ENABLE_CURVE CACHE)
    endif ()
    if (_WRKZ_HAD_WITH_LIBBSD)
        set(WITH_LIBBSD "${_WRKZ_PREV_WITH_LIBBSD}" CACHE BOOL "Use libbsd instead of builtin strlcpy" FORCE)
    else ()
        unset(WITH_LIBBSD CACHE)
    endif ()

    unset(_WRKZ_HAD_BUILD_SHARED)
    unset(_WRKZ_HAD_BUILD_STATIC)
    unset(_WRKZ_HAD_BUILD_TESTS)
    unset(_WRKZ_HAD_WITH_PERF_TOOL)
    unset(_WRKZ_HAD_WITH_DOC)
    unset(_WRKZ_HAD_WITH_DOCS)
    unset(_WRKZ_HAD_ENABLE_CURVE)
    unset(_WRKZ_HAD_WITH_LIBBSD)
    unset(_WRKZ_PREV_BUILD_SHARED)
    unset(_WRKZ_PREV_BUILD_STATIC)
    unset(_WRKZ_PREV_BUILD_TESTS)
    unset(_WRKZ_PREV_WITH_PERF_TOOL)
    unset(_WRKZ_PREV_WITH_DOC)
    unset(_WRKZ_PREV_WITH_DOCS)
    unset(_WRKZ_PREV_ENABLE_CURVE)
    unset(_WRKZ_PREV_WITH_LIBBSD)
endif ()

# ── zstd + RocksDB (daemon-only: skipped for wallet-only and Emscripten builds) ──
if (WRKZ_BUILD_EXECUTABLES AND NOT EMSCRIPTEN AND WRKZ_SYSTEM_ROCKSDB)
    # Packagers and CI can supply RocksDB themselves and skip the bundled tree
    # (and the bundled zstd, which only exists to be RocksDB's compressor)
    # entirely. Prefer the upstream CMake package config, fall back to a plain
    # header/library search.
    find_package(RocksDB CONFIG QUIET)
    if (TARGET RocksDB::rocksdb)
        set(WRKZ_ROCKSDB_LIBS RocksDB::rocksdb PARENT_SCOPE)
        message(STATUS "RocksDB: using system package (RocksDB::rocksdb)")
    elseif (TARGET RocksDB::rocksdb-shared)
        set(WRKZ_ROCKSDB_LIBS RocksDB::rocksdb-shared PARENT_SCOPE)
        message(STATUS "RocksDB: using system package (RocksDB::rocksdb-shared)")
    else ()
        find_path(WRKZ_ROCKSDB_INCLUDE_DIR NAMES rocksdb/db.h)
        find_library(WRKZ_ROCKSDB_LIBRARY NAMES rocksdb)
        if (NOT WRKZ_ROCKSDB_INCLUDE_DIR OR NOT WRKZ_ROCKSDB_LIBRARY)
            message(FATAL_ERROR
                "WRKZ_SYSTEM_ROCKSDB=ON but no system RocksDB was found. Install the "
                "RocksDB development package (or point CMAKE_PREFIX_PATH at it), or "
                "re-run with -DWRKZ_SYSTEM_ROCKSDB=OFF to build the bundled copy.")
        endif ()
        add_library(wrkz-system-rocksdb INTERFACE)
        target_include_directories(wrkz-system-rocksdb INTERFACE ${WRKZ_ROCKSDB_INCLUDE_DIR})
        target_link_libraries(wrkz-system-rocksdb INTERFACE ${WRKZ_ROCKSDB_LIBRARY})
        set(WRKZ_ROCKSDB_LIBS wrkz-system-rocksdb PARENT_SCOPE)
        message(STATUS "RocksDB: using system library ${WRKZ_ROCKSDB_LIBRARY}")
    endif ()
    set_property(TARGET libminiupnpc-static argon2 PROPERTY FOLDER "external")
elseif (WRKZ_BUILD_EXECUTABLES AND NOT EMSCRIPTEN)
    # zstd (new layout): CMake entrypoint lives under build/cmake
    set(ZSTD_BUILD_STATIC ON CACHE BOOL "Build static zstd" FORCE)
    set(ZSTD_BUILD_SHARED OFF CACHE BOOL "Disable shared zstd" FORCE)
    set(ZSTD_BUILD_PROGRAMS OFF CACHE BOOL "Disable zstd CLI tools" FORCE)
    set(ZSTD_BUILD_TESTS OFF CACHE BOOL "Disable zstd tests" FORCE)
    set(ZSTD_BUILD_CONTRIB OFF CACHE BOOL "Disable zstd contrib" FORCE)
    add_subdirectory(zstd/build/cmake)
    set(WITH_SNAPPY OFF CACHE BOOL "Disable Snappy in RocksDB" FORCE)

    # RocksDB defaults WITH_BENCHMARK_TOOLS / WITH_CORE_TOOLS / WITH_TOOLS /
    # WITH_TRACE_TOOLS to ON, which drags in db_bench, ldb, sst_dump,
    # trace_analyzer and the bundled gtest. We only want librocksdb. These are
    # kept in sync with the -D flags passed to the out-of-tree sub-build below;
    # the pruned vendored tree no longer ships the sources they would need.
    set(WITH_TESTS OFF CACHE BOOL "Disable RocksDB tests" FORCE)
    set(WITH_BENCHMARK_TOOLS OFF CACHE BOOL "Disable RocksDB benchmarks" FORCE)
    set(WITH_CORE_TOOLS OFF CACHE BOOL "Disable RocksDB ldb/sst_dump" FORCE)
    set(WITH_TOOLS OFF CACHE BOOL "Disable RocksDB tools" FORCE)
    set(WITH_TRACE_TOOLS OFF CACHE BOOL "Disable RocksDB trace tools" FORCE)
    set(WITH_JNI OFF CACHE BOOL "Disable RocksDB JNI bindings" FORCE)
    set(WITH_EXAMPLES OFF CACHE BOOL "Disable RocksDB examples" FORCE)
    set(WITH_BENCHMARK OFF CACHE BOOL "Disable RocksDB microbenchmarks" FORCE)

    if (MSVC)
        add_subdirectory(rocksdb EXCLUDE_FROM_ALL)
        set_property(TARGET libminiupnpc-static rocksdb libzstd libzstd_static argon2 PROPERTY FOLDER "external")
        set(WRKZ_ROCKSDB_LIBS rocksdb libzstd PARENT_SCOPE)
    else ()
        set_property(TARGET libminiupnpc-static libzstd libzstd_static argon2 PROPERTY FOLDER "external")
        if (DEFINED ROCKSDB_BUILD_PARALLEL AND NOT ROCKSDB_BUILD_PARALLEL STREQUAL "")
            set(_WRKZ_ROCKSDB_FALLBACK_JOBS "${ROCKSDB_BUILD_PARALLEL}")
            set(_WRKZ_ROCKSDB_PARALLEL_SOURCE "ROCKSDB_BUILD_PARALLEL")
        elseif (DEFINED ENV{CMAKE_BUILD_PARALLEL_LEVEL} AND NOT "$ENV{CMAKE_BUILD_PARALLEL_LEVEL}" STREQUAL "")
            set(_WRKZ_ROCKSDB_FALLBACK_JOBS "$ENV{CMAKE_BUILD_PARALLEL_LEVEL}")
            set(_WRKZ_ROCKSDB_PARALLEL_SOURCE "CMAKE_BUILD_PARALLEL_LEVEL")
        else ()
            include(ProcessorCount)
            ProcessorCount(_WRKZ_CPU_COUNT)
            if (NOT _WRKZ_CPU_COUNT GREATER 0)
                set(_WRKZ_CPU_COUNT 1)
            endif ()

            set(_WRKZ_ROCKSDB_FALLBACK_JOBS "${_WRKZ_CPU_COUNT}")
            if (DEFINED ROCKSDB_BUILD_PARALLEL_CAP AND NOT ROCKSDB_BUILD_PARALLEL_CAP STREQUAL ""
                AND ROCKSDB_BUILD_PARALLEL_CAP GREATER 0
                AND _WRKZ_ROCKSDB_FALLBACK_JOBS GREATER ROCKSDB_BUILD_PARALLEL_CAP)
                set(_WRKZ_ROCKSDB_FALLBACK_JOBS "${ROCKSDB_BUILD_PARALLEL_CAP}")
            endif ()
            set(_WRKZ_ROCKSDB_PARALLEL_SOURCE "cpu_count_capped")
            unset(_WRKZ_CPU_COUNT)
        endif ()
        message(STATUS
            "RocksDB sub-build parallel jobs fallback: ${_WRKZ_ROCKSDB_FALLBACK_JOBS} "
            "(source=${_WRKZ_ROCKSDB_PARALLEL_SOURCE}, cap=${ROCKSDB_BUILD_PARALLEL_CAP})")

        add_custom_target(
                rocksdb
                BYPRODUCTS
                    ${PROJECT_BINARY_DIR}/rocksdb/librocksdb.a
                COMMAND ${CMAKE_COMMAND}
                    -DROCKSDB_BUILD_DIR=${PROJECT_BINARY_DIR}/rocksdb
                    -DROCKSDB_TARGET=rocksdb
                    -DROCKSDB_BUILD_PARALLEL=${ROCKSDB_BUILD_PARALLEL}
                    -DROCKSDB_FALLBACK_PARALLEL=${_WRKZ_ROCKSDB_FALLBACK_JOBS}
                    -DROCKSDB_BUILD_PARALLEL_CAP=${ROCKSDB_BUILD_PARALLEL_CAP}
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/../cmake/BuildRocksDB.cmake
                WORKING_DIRECTORY ${PROJECT_BINARY_DIR}/rocksdb
        )
        add_dependencies(rocksdb libzstd_static)
        add_library(rocksdblib STATIC IMPORTED GLOBAL)
        set_target_properties(rocksdblib PROPERTIES IMPORTED_LOCATION ${PROJECT_BINARY_DIR}/rocksdb/librocksdb.a)
        add_dependencies(rocksdblib rocksdb)
        set(WRKZ_ROCKSDB_LIBS rocksdblib libzstd PARENT_SCOPE)

        unset(_WRKZ_ROCKSDB_FALLBACK_JOBS)
        unset(_WRKZ_ROCKSDB_PARALLEL_SOURCE)
    endif ()
else ()
    # No daemon dependencies built: only set the folder property for the targets that exist
    set_property(TARGET libminiupnpc-static argon2 PROPERTY FOLDER "external")
endif ()

if (MSVC)
    set_property(TARGET libminiupnpc-static APPEND_STRING PROPERTY COMPILE_FLAGS " -wd4244 -wd4267")
else ()
    set_property(TARGET libminiupnpc-static APPEND_STRING PROPERTY COMPILE_FLAGS " -Wno-undef -Wno-unused-result -Wno-unused-value")
endif ()
