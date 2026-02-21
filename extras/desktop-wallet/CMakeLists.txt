cmake_minimum_required(VERSION 3.21)

project(gui_wallet LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

option(DESKTOP_WALLET_REQUIRE_STATIC_WALLET_CAPI "Require static libwallet_capi_c.a backend linkage" ON)

find_package(Qt6 REQUIRED COMPONENTS Gui Qml Concurrent)

set(REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../..")
set(WALLET_CAPI_INCLUDE_DIR "${WALLET_CAPI_INCLUDE_DIR}" CACHE PATH
    "Path containing walletcapi/wallet_capi.h (default: repo include)")
if (NOT WALLET_CAPI_INCLUDE_DIR)
    set(WALLET_CAPI_INCLUDE_DIR "${REPO_ROOT}/include")
endif ()

set(WALLET_CAPI_LIBRARY "${WALLET_CAPI_LIBRARY}" CACHE FILEPATH
    "Path to wallet_capi library (.so/.a/.dll/.lib)")
if (NOT WALLET_CAPI_LIBRARY)
    find_library(
        WALLET_CAPI_LIBRARY
        NAMES wallet_capi wallet_capi_c libwallet_capi libwallet_capi_c
        HINTS
            "${REPO_ROOT}/build-linux-wallet-capi/src"
            "${REPO_ROOT}/build-android-arm64/src"
            "${REPO_ROOT}/build-windows-wallet-x86_64/src"
    )
endif ()

if (NOT EXISTS "${WALLET_CAPI_INCLUDE_DIR}/walletcapi/wallet_capi.h")
    message(FATAL_ERROR
        "wallet_capi.h not found under WALLET_CAPI_INCLUDE_DIR='${WALLET_CAPI_INCLUDE_DIR}'.")
endif ()

if (NOT WALLET_CAPI_LIBRARY)
    message(FATAL_ERROR
        "WALLET_CAPI_LIBRARY not found.\n"
        "Set -DWALLET_CAPI_LIBRARY=/path/to/libwallet_capi.so (or static/import lib).")
endif ()

if (NOT EXISTS "${WALLET_CAPI_LIBRARY}")
    message(FATAL_ERROR
        "WALLET_CAPI_LIBRARY path does not exist:\n"
        "  ${WALLET_CAPI_LIBRARY}\n"
        "If you built static-only wallet lib, use:\n"
        "  -DWALLET_CAPI_LIBRARY=<repo>/build-linux-wallet-capi/src/libwallet_capi_c.a\n"
        "If you need shared, build with:\n"
        "  WALLET_LIB_KIND=both bash scripts/build-linux-wallet-lib.sh")
endif ()

if (DESKTOP_WALLET_REQUIRE_STATIC_WALLET_CAPI)
    get_filename_component(_wallet_capi_ext "${WALLET_CAPI_LIBRARY}" EXT)
    if (NOT _wallet_capi_ext STREQUAL ".a")
        message(FATAL_ERROR
            "DESKTOP_WALLET_REQUIRE_STATIC_WALLET_CAPI=ON but WALLET_CAPI_LIBRARY is not a static archive:\n"
            "  ${WALLET_CAPI_LIBRARY}\n"
            "Use libwallet_capi_c.a, e.g.:\n"
            "  -DWALLET_CAPI_LIBRARY=<repo>/build-linux-wallet-capi/src/libwallet_capi_c.a")
    endif ()
endif ()

if (COMMAND qt_standard_project_setup)
    qt_standard_project_setup()
endif ()

qt_add_executable(gui_wallet
    src/main.cpp
    src/wallet_bridge.h
    src/wallet_bridge.cpp
)

qt_add_resources(gui_wallet "gui_wallet_qml"
    PREFIX "/"
    FILES
        qml/Main.qml
        qml/assets/logo-placeholder.svg
)

target_include_directories(gui_wallet PRIVATE
    "${WALLET_CAPI_INCLUDE_DIR}"
)

target_link_libraries(gui_wallet PRIVATE
    Qt6::Gui
    Qt6::Qml
    Qt6::Concurrent
)

set(_wallet_capi_name "${WALLET_CAPI_LIBRARY}")
get_filename_component(_wallet_capi_name "${_wallet_capi_name}" NAME)

if (UNIX AND NOT APPLE AND _wallet_capi_name MATCHES "libwallet_capi_c\\.a$")
    # Static wallet_capi_c is not a monolithic archive; pull transitive static
    # deps from the same wallet build tree and link as one group.
    get_filename_component(_wallet_build_src_dir "${WALLET_CAPI_LIBRARY}" DIRECTORY)
    get_filename_component(_wallet_build_root "${_wallet_build_src_dir}" DIRECTORY)

    file(GLOB_RECURSE _wallet_dep_archives
        "${_wallet_build_root}/src/lib*.a"
        "${_wallet_build_root}/external/*.a"
        "${_wallet_build_root}/external/*/*.a"
        "${_wallet_build_root}/external/*/*/*.a"
    )

    list(FILTER _wallet_dep_archives EXCLUDE REGEX ".*/CMakeFiles/.*")

    # Keep provided wallet_capi archive first in the group.
    list(REMOVE_ITEM _wallet_dep_archives "${WALLET_CAPI_LIBRARY}")
    list(INSERT _wallet_dep_archives 0 "${WALLET_CAPI_LIBRARY}")

    target_link_libraries(gui_wallet PRIVATE
        "-Wl,--start-group"
        ${_wallet_dep_archives}
        "-Wl,--end-group"
        ssl
        crypto
        dl
        pthread
        m
    )
else ()
    target_link_libraries(gui_wallet PRIVATE "${WALLET_CAPI_LIBRARY}")
endif ()

if (WIN32)
    set_target_properties(gui_wallet PROPERTIES WIN32_EXECUTABLE ON)
endif ()
