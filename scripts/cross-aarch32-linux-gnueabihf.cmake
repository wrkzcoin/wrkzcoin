message(STATUS "Activating GCC 8.2 Cross-Compiler: aarch32-linux-gnueabihf")

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch32)

set(CMAKE_C_COMPILER "$ENV{HOME}/toolchain/gcc-linaro-7.5.0-2019.12-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-gcc")
set(CMAKE_CXX_COMPILER "$ENV{HOME}/toolchain/gcc-linaro-7.5.0-2019.12-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-g++")
set(CMAKE_FIND_ROOT_PATH "$ENV{HOME}/toolchain/gcc-linaro-7.5.0-2019.12-x86_64_arm-linux-gnueabihf")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)

set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
