cmake_minimum_required(VERSION 3.21)

project(gui_wallet LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Quick Qml QuickControls2 Concurrent)

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

qt_standard_project_setup()

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
    Qt6::Quick
    Qt6::Qml
    Qt6::QuickControls2
    Qt6::Concurrent
    "${WALLET_CAPI_LIBRARY}"
)

if (WIN32)
    set_target_properties(gui_wallet PROPERTIES WIN32_EXECUTABLE ON)
endif ()
