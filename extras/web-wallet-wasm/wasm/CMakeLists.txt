cmake_minimum_required(VERSION 3.20)
project(wallet_wasm_bridge LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Scaffold target. Integrate with root build graph later so this target links
# to existing wallet_capi/walletbackend dependencies from src/.
add_library(wallet_wasm_bridge STATIC
  src/wallet_wasm_exports.cpp
)

target_include_directories(wallet_wasm_bridge PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
