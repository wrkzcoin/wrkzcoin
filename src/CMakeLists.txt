# Build statically when requested by top-level STATIC option.
if (STATIC)
    add_definitions(-DSTATICLIB)
endif ()

# Add external libraries as include dirs, so we can do #include "httplib" for example
# With WRKZ_SYSTEM_ROCKSDB the headers come from the found package/library target
# instead, so the bundled tree must stay off the include path.
if (NOT WRKZ_SYSTEM_ROCKSDB)
    include_directories(${CMAKE_SOURCE_DIR}/external/rocksdb/include)
endif ()
include_directories(${CMAKE_SOURCE_DIR}/external/cpp-linenoise)
include_directories(${CMAKE_SOURCE_DIR}/external/cpp-httplib)
include_directories(${CMAKE_SOURCE_DIR}/external/nlohmann-json)
include_directories(${CMAKE_SOURCE_DIR}/external/cxxopts)
include_directories(${CMAKE_SOURCE_DIR}/external/argon2/include)
include_directories(${CMAKE_SOURCE_DIR}/external/zstd)

# wallet_capi shared target links many internal static libraries. Ensure they
# are built with PIC so shared linking succeeds on ELF/Mach-O platforms.
if (WRKZ_BUILD_WALLET_CAPI AND NOT WIN32)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif ()

# Show cmake where the source files are
# Note, if you add remove a source file, you will need to re-run cmake so it
# can find the new file
file(GLOB_RECURSE Common CONFIGURE_DEPENDS common/*)
file(GLOB_RECURSE Config CONFIGURE_DEPENDS config/*.cpp config/*.h)
file(GLOB_RECURSE Crypto CONFIGURE_DEPENDS crypto/*)
file(GLOB_RECURSE CryptoNoteCore CONFIGURE_DEPENDS cryptonotecore/*)
file(GLOB_RECURSE CryptoNoteProtocol CONFIGURE_DEPENDS cryptonoteprotocol/*)
file(GLOB_RECURSE CryptoTest CONFIGURE_DEPENDS cryptotest/*)
file(GLOB_RECURSE MasternodeTest CONFIGURE_DEPENDS masternodetest/*)
file(GLOB_RECURSE Errors CONFIGURE_DEPENDS errors/*)
file(GLOB_RECURSE Http CONFIGURE_DEPENDS http/*)
file(GLOB_RECURSE JsonRpcServer CONFIGURE_DEPENDS jsonrpcserver/*)
file(GLOB_RECURSE Logging CONFIGURE_DEPENDS logging/*)
file(GLOB_RECURSE Logger CONFIGURE_DEPENDS logger/*)
file(GLOB_RECURSE miner CONFIGURE_DEPENDS miner/*)
file(GLOB_RECURSE Mnemonics CONFIGURE_DEPENDS mnemonics/*)
file(GLOB_RECURSE Nigel CONFIGURE_DEPENDS nigel/*)
file(GLOB_RECURSE NodeRpcProxy CONFIGURE_DEPENDS noderpcproxy/*)
file(GLOB_RECURSE P2p CONFIGURE_DEPENDS p2p/*)
file(GLOB_RECURSE Rpc CONFIGURE_DEPENDS rpc/*)
file(GLOB_RECURSE Serialization CONFIGURE_DEPENDS serialization/*)
file(GLOB_RECURSE SubWallets CONFIGURE_DEPENDS subwallets/*)
file(GLOB_RECURSE Transfers CONFIGURE_DEPENDS transfers/*)
file(GLOB_RECURSE Wrkzd CONFIGURE_DEPENDS daemon/*)
file(GLOB_RECURSE Utilities CONFIGURE_DEPENDS utilities/*)
file(GLOB_RECURSE Wallet CONFIGURE_DEPENDS wallet/*)
file(GLOB_RECURSE WalletApi CONFIGURE_DEPENDS walletapi/*)
file(GLOB_RECURSE WalletBackend CONFIGURE_DEPENDS walletbackend/*)
file(GLOB_RECURSE WalletService CONFIGURE_DEPENDS walletservice/*)
file(GLOB_RECURSE WalletUpgrader CONFIGURE_DEPENDS walletupgrader/*)
file(GLOB_RECURSE zedwallet++ CONFIGURE_DEPENDS zedwallet++/*)
if (WRKZ_BUILD_WALLET_CAPI)
    file(GLOB_RECURSE WalletCAPI CONFIGURE_DEPENDS walletcapi/*)
endif ()
if (WRKZ_BUILD_WALLET_WASM)
    set(WalletWasmBridge
        ${CMAKE_SOURCE_DIR}/extras/web-wallet-wasm/wasm/src/wallet_wasm_exports.cpp
    )
endif ()

if (WIN32)
    file(GLOB_RECURSE System CONFIGURE_DEPENDS system/* platform/windows/system/*)
elseif (ANDROID)
    # Android shares the Linux (epoll/ucontext) implementation; bionic specifics are #ifdef'd there.
    file(GLOB_RECURSE System CONFIGURE_DEPENDS system/* platform/linux/system/* platform/posix/system/*)
elseif (APPLE)
    file(GLOB_RECURSE System CONFIGURE_DEPENDS system/* platform/osx/system/* platform/posix/system/*)
else ()
    file(GLOB_RECURSE System CONFIGURE_DEPENDS system/* platform/linux/system/* platform/posix/system/*)
endif ()

# Group the files together in IDEs
source_group("" FILES ${Common} ${Config} ${Crypto} ${CryptoNoteCore} ${CryptoNoteProtocol} ${Wrkzd} ${JsonRpcServer} ${Http} ${Logging} ${Logger} ${miner} ${Mnemonics} ${Nigel} ${NodeRpcProxy} ${P2p} ${Rpc} ${Serialization} ${System} ${Transfers} ${Wallet} ${WalletApi} ${WalletBackend} ${WalletService} ${zedwallet++} ${CryptoTest} ${Errors} ${Utilities} ${WalletUpgrader} ${SubWallets})

# Define a group of files as a library to link against
add_library(Common STATIC ${Common})
add_library(Config STATIC ${Config})
add_library(Crypto STATIC ${Crypto})
add_library(CryptoNoteCore STATIC ${CryptoNoteCore})
add_library(Errors STATIC ${Errors})
add_library(Http STATIC ${Http})
add_library(JsonRpcServer STATIC ${JsonRpcServer})
add_library(Logging STATIC ${Logging})
add_library(Logger STATIC ${Logger})
add_library(Mnemonics STATIC ${Mnemonics})
add_library(Nigel STATIC ${Nigel})
add_library(NodeRpcProxy STATIC ${NodeRpcProxy})
add_library(P2P STATIC ${CryptoNoteProtocol} ${P2p})
add_library(Rpc STATIC ${Rpc})
add_library(Serialization STATIC ${Serialization})
add_library(SubWallets STATIC ${SubWallets})
add_library(System STATIC ${System})
add_library(Transfers STATIC ${Transfers})
add_library(Utilities STATIC ${Utilities})
add_library(Wallet STATIC ${Wallet})
add_library(WalletBackend STATIC ${WalletBackend})
add_library(wallet_core ALIAS WalletBackend)
if (WRKZ_BUILD_WALLET_CAPI)
    add_library(wallet_capi_c STATIC ${WalletCAPI})
    add_library(wallet_capi SHARED ${WalletCAPI})
endif ()
if (WRKZ_BUILD_WALLET_WASM)
    add_executable(wallet_wasm ${WalletWasmBridge})

    # WalletBackend, wallet_capi_c, and wallet_capi also #include <wasm_fs_bridge.h>
    # under __EMSCRIPTEN__, so they need the include path too.
    target_include_directories(WalletBackend PRIVATE
        ${CMAKE_SOURCE_DIR}/extras/web-wallet-wasm/wasm/include
    )
    target_include_directories(wallet_capi_c PRIVATE
        ${CMAKE_SOURCE_DIR}/extras/web-wallet-wasm/wasm/include
    )
    if (TARGET wallet_capi)
        target_include_directories(wallet_capi PRIVATE
            ${CMAKE_SOURCE_DIR}/extras/web-wallet-wasm/wasm/include
        )
    endif ()
endif ()

if (WRKZ_BUILD_WALLET_CAPI AND NOT WIN32)
    # All static libs that end up linked into wallet_capi.so must be compiled
    # with -fPIC.  This covers both internal libs and external deps that do not
    # set POSITION_INDEPENDENT_CODE themselves.
    set(_WRKZ_WALLET_CAPI_DEPS
        Common Config Crypto CryptoNoteCore Errors Http JsonRpcServer Logging
        Logger Mnemonics Nigel NodeRpcProxy P2P Rpc Serialization SubWallets
        System Transfers Utilities Wallet WalletBackend
        # external dep pulled in transitively:
        argon2          # Crypto → argon2 (no PIC by default)
        # rocksdb not needed: DatabaseBlockchainCache/RocksDBWrapper are
        # daemon-only dead code that the lazy linker never pulls in.
    )
    foreach(_t IN LISTS _WRKZ_WALLET_CAPI_DEPS)
        if (TARGET ${_t})
            set_target_properties(${_t} PROPERTIES POSITION_INDEPENDENT_CODE ON)
        endif ()
    endforeach ()
endif ()

# slow-hash-x86.c uses AES intrinsics and requires -maes when built on x86/x86_64.
# Keep this flag local to the file so global CPU portability settings remain intact.
if (NOT MSVC)
    set(SLOW_HASH_X86_SRC "${CMAKE_CURRENT_SOURCE_DIR}/crypto/slow-hash-x86.c")
    string(TOLOWER "${CMAKE_C_COMPILER}" WRKZ_C_COMPILER_LC)
    string(TOLOWER "${CMAKE_C_COMPILER_TARGET}" WRKZ_C_COMPILER_TARGET_LC)
    set(WRKZ_SLOWHASH_ARM_TARGET FALSE)
    set(WRKZ_SLOWHASH_X86_TARGET FALSE)
    if (
        WRKZ_C_COMPILER_LC MATCHES "aarch64|arm64|arm-linux-gnueabihf|arm-linux-gnueabi"
        OR WRKZ_C_COMPILER_TARGET_LC MATCHES "aarch64|arm64|arm"
        OR CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|arm"
    )
        set(WRKZ_SLOWHASH_ARM_TARGET TRUE)
    endif ()
    if (
        NOT WRKZ_SLOWHASH_ARM_TARGET
        AND (
            CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|AMD64|x86|i.86"
            OR WRKZ_C_COMPILER_LC MATCHES "x86_64|amd64|i.86|mingw32"
            OR WRKZ_C_COMPILER_TARGET_LC MATCHES "x86_64|amd64|i.86"
        )
    )
        set(WRKZ_SLOWHASH_X86_TARGET TRUE)
    endif ()
    if (
        EXISTS "${SLOW_HASH_X86_SRC}"
        AND WRKZ_SLOWHASH_X86_TARGET
        AND NOT EMSCRIPTEN
    )
        set_source_files_properties("${SLOW_HASH_X86_SRC}" PROPERTIES COMPILE_OPTIONS "-maes")
    endif ()
endif ()

if (WIN32)
    set(DAEMON_SOURCES_OS
            binaryinfo/daemon.rc
            )
    set(ZED_WALLET_SOURCES_OS
            binaryinfo/zedwallet.rc
            )
    set(MINER_SOURCES_OS
            binaryinfo/miner.rc
            )
    set(PG_SOURCES_OS
            binaryinfo/service.rc
            )
    set(CT_SOURCES_OS
            binaryinfo/cryptotest.rc
            )
    set(WALLET_API_SOURCES_OS
            binaryinfo/walletapi.rc
            )
    set(WALLET_UPGRADER_SOURCES_OS
            binaryinfo/walletupgrader.rc
            )
endif ()

if (WRKZ_BUILD_EXECUTABLES)
    add_executable(cryptotest ${CryptoTest} ${CT_SOURCES_OS})
    add_executable(masternodetest ${MasternodeTest})
    add_executable(miner ${miner} ${MINER_SOURCES_OS})
    add_executable(WalletService ${WalletService} ${PG_SOURCES_OS})
    add_executable(Wrkzd ${Wrkzd} ${DAEMON_SOURCES_OS})
    add_executable(WalletApi ${WalletApi} ${WALLET_API_SOURCES_OS})
    add_executable(WalletUpgrader ${WalletUpgrader} ${WALLET_UPGRADER_SOURCES_OS})
    add_executable(zedwallet++ ${zedwallet++} ${ZED_WALLET_SOURCES_OS})
endif ()

## Optional precompiled headers (opt-in: -DWRKZ_ENABLE_PCH=ON). Each listed
## target precompiles the std headers (plus json.hpp where JSON is
## pervasive) that nearly every one of its translation units parses; the
## COMPILE_LANGUAGE guard leaves the C sources alone (crypto/*.c, the osx
## ucontext shim). Measured with MSVC 2022 on a 16-core machine, a clean
## build of the src targets went from 7m50s to 13m49s with this PCH set
## (each .pch is 150-280 MB), so it stays OFF by default. GCC/Clang load PCH
## far more cheaply - measure there before enabling it in a build script.
option(WRKZ_ENABLE_PCH "Use precompiled headers for the larger internal targets (measure first)" OFF)
if (EMSCRIPTEN)
    set(WRKZ_ENABLE_PCH OFF)
endif ()
if (WRKZ_ENABLE_PCH)
    set(WRKZ_PCH_STD
        algorithm array atomic cassert chrono condition_variable cstddef cstdint cstring
        exception functional future iomanip iostream iterator limits list map memory mutex
        numeric optional queue random set shared_mutex sstream stack stdexcept string
        system_error thread tuple type_traits unordered_map unordered_set utility vector)

    function(wrkz_pch target)
        if (NOT TARGET ${target})
            return()
        endif ()
        set(_hdrs "")
        foreach(_h IN LISTS ARGN)
            list(APPEND _hdrs "$<$<COMPILE_LANGUAGE:CXX>:<${_h}$<ANGLE-R>>")
        endforeach ()
        target_precompile_headers(${target} PRIVATE ${_hdrs})
    endfunction ()

    # std-only: no json in these
    foreach(_t IN ITEMS System Logging)
        wrkz_pch(${_t} ${WRKZ_PCH_STD})
    endforeach ()
    # std only
    foreach(_t IN ITEMS Common Serialization Utilities P2P)
        wrkz_pch(${_t} ${WRKZ_PCH_STD})
    endforeach ()
    # std + json
    foreach(_t IN ITEMS CryptoNoteCore Transfers Wallet WalletBackend Rpc)
        wrkz_pch(${_t} ${WRKZ_PCH_STD} json.hpp)
    endforeach ()
    if (WRKZ_BUILD_EXECUTABLES)
        foreach(_t IN ITEMS Wrkzd WalletService WalletApi zedwallet++ miner)
            wrkz_pch(${_t} ${WRKZ_PCH_STD} json.hpp)
        endforeach ()
    endif ()
    if (CCACHE_PROGRAM)
        message(STATUS "PCH + ccache: set CCACHE_SLOPPINESS=pch_defines,time_macros so ccache can cache PCH compiles")
    endif ()
endif ()

if (WIN32 AND WRKZ_BUILD_EXECUTABLES)
    target_link_libraries(System ws2_32)
    if (MSVC)
        target_link_libraries(Wrkzd ws2_32 advapi32 crypt32 gdi32 user32)
        target_link_libraries(WalletService Rpcrt4 ws2_32 advapi32 crypt32 gdi32 user32)
    else ()
        target_link_libraries(Wrkzd ws2_32 advapi32 crypt32 gdi32 user32)
        target_link_libraries(WalletService rpcrt4 ws2_32 advapi32 crypt32 gdi32 user32)
    endif ()
    target_link_libraries(zedwallet++ ws2_32 advapi32 crypt32 gdi32 user32)
    target_link_libraries(WalletApi ws2_32 advapi32 crypt32 gdi32 user32)
    target_link_libraries(miner ws2_32 advapi32 crypt32 gdi32 user32)
    target_link_libraries(WalletUpgrader ws2_32 advapi32 crypt32 gdi32 user32)
    target_link_libraries(masternodetest ws2_32 advapi32 crypt32 gdi32 user32)
endif ()

if (ANDROID)
    set(WRKZ_ANDROID_UCONTEXT_LIB "")
    if (LIBUCONTEXT_ROOT)
        set(WRKZ_LIBUCONTEXT_LIB "${LIBUCONTEXT_ROOT}/lib/libucontext.a")
        if (EXISTS "${WRKZ_LIBUCONTEXT_LIB}")
            if (EXISTS "${LIBUCONTEXT_ROOT}/include")
                target_include_directories(System PRIVATE "${LIBUCONTEXT_ROOT}/include")
            endif ()
            target_link_libraries(System PUBLIC "${WRKZ_LIBUCONTEXT_LIB}")
            set(WRKZ_ANDROID_UCONTEXT_LIB "${WRKZ_LIBUCONTEXT_LIB}")
            message(STATUS "Android System: linking libucontext from ${WRKZ_LIBUCONTEXT_LIB}")
        else ()
            if (WRKZ_BUILD_EXECUTABLES OR WRKZ_BUILD_WALLET_CAPI)
                message(FATAL_ERROR
                    "Android build requires libucontext, but it was not found at:\n"
                    "  ${WRKZ_LIBUCONTEXT_LIB}\n"
                    "Build it first with scripts/build-libucontext-android.sh and set LIBUCONTEXT_ROOT.")
            else ()
                message(WARNING
                    "LIBUCONTEXT_ROOT is set but libucontext was not found at ${WRKZ_LIBUCONTEXT_LIB}. "
                    "Continuing without libucontext.")
            endif ()
        endif ()
    else ()
        if (WRKZ_BUILD_EXECUTABLES OR WRKZ_BUILD_WALLET_CAPI)
            message(FATAL_ERROR
                "Android build requires LIBUCONTEXT_ROOT.\n"
                "Build libucontext first with scripts/build-libucontext-android.sh.")
        else ()
            message(STATUS "Android System: LIBUCONTEXT_ROOT not set, building without libucontext")
        endif ()
    endif ()
endif ()

if (EMSCRIPTEN)
    # Provide stub platform headers (e.g. sys/epoll.h) that are absent from
    # the Emscripten sysroot. The System library compiles but is never called
    # at runtime in WASM; networking goes through wrkzSyncXhr in Nigel.
    target_include_directories(System BEFORE PRIVATE
        "${CMAKE_SOURCE_DIR}/src/platform/emscripten"
    )
endif ()

# A bit of hackery so we don't have to do the if/else/ for every target that
# wants to use filesystem
add_library(__filesystem INTERFACE)

# Windows works out of the box
if (APPLE)
    # Only old Homebrew LLVM toolchains ship std::filesystem as a separate
    # archive; current Xcode / osxcross libc++ has it built in.
    if (EXISTS "/usr/local/opt/llvm/lib/libc++fs.a")
        target_link_libraries(__filesystem INTERFACE /usr/local/opt/llvm/lib/libc++fs.a)
    endif ()
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(__filesystem INTERFACE stdc++fs)
endif ()

if (WRKZ_BUILD_EXECUTABLES)
    # WRKZ_ROCKSDB_LIBS is set by external/CMakeLists.txt and names whichever
    # RocksDB (plus bundled zstd, when we build it) this configuration uses.
    target_link_libraries(Wrkzd System CryptoNoteCore ${WRKZ_ROCKSDB_LIBS} Errors)
endif ()

# Add the dependencies we need
target_link_libraries(Common __filesystem)
target_link_libraries(Crypto argon2)
# Config: the masternode code uses the network-parameter helpers defined in src/config.
target_link_libraries(CryptoNoteCore Utilities Common Logging Crypto P2P Rpc Http Serialization System Config)
target_link_libraries(Errors Crypto SubWallets Utilities)
target_link_libraries(Logging Common)
target_link_libraries(Nigel Errors CryptoNoteCore)
target_link_libraries(NodeRpcProxy Rpc)
target_link_libraries(P2P libminiupnpc-static Serialization System CryptoNoteCore)
target_link_libraries(Rpc P2P Utilities CryptoNoteCore)
target_link_libraries(Serialization Common Crypto)
target_link_libraries(SubWallets Common Logger)
target_link_libraries(Transfers CryptoNoteCore)
target_link_libraries(Utilities Common Errors Crypto)
target_link_libraries(Wallet NodeRpcProxy Transfers CryptoNoteCore Common WalletBackend)
target_link_libraries(WalletBackend Serialization Mnemonics Nigel __filesystem Utilities SubWallets Logger Config Wallet)
if (WRKZ_BUILD_EXECUTABLES)
    # Serialization is needed by common/TransactionExtra.cpp, which the payment
    # ID tests use to build a real tx_extra and read it back.
    target_link_libraries(cryptotest Crypto Common Utilities Serialization)
    target_link_libraries(masternodetest CryptoNoteCore Utilities Common Crypto)
    target_link_libraries(miner Crypto Errors Utilities System Serialization)
    target_link_libraries(WalletApi WalletBackend)
    target_link_libraries(WalletService JsonRpcServer Wallet Mnemonics Errors)
    target_link_libraries(WalletUpgrader Utilities WalletBackend Common)
    target_link_libraries(zedwallet++ WalletBackend)
endif ()
if (WRKZ_BUILD_WALLET_CAPI)
    target_link_libraries(wallet_capi_c WalletBackend)
    target_link_libraries(wallet_capi PRIVATE WalletBackend)
    set_property(TARGET wallet_capi PROPERTY OUTPUT_NAME "wallet_capi")
    # Strip debug symbols in Release builds — reduces .so from ~90MB to ~15-20MB
    if (NOT WIN32 AND CMAKE_BUILD_TYPE STREQUAL "Release")
        add_custom_command(TARGET wallet_capi POST_BUILD
            COMMAND ${CMAKE_STRIP} -s "$<TARGET_FILE:wallet_capi>"
            COMMENT "Stripping wallet_capi shared library"
        )
    endif ()
endif ()
if (WRKZ_BUILD_WALLET_WASM)
    if (NOT WRKZ_BUILD_WALLET_CAPI)
        message(FATAL_ERROR "WRKZ_BUILD_WALLET_WASM requires WRKZ_BUILD_WALLET_CAPI=ON")
    endif ()
    if (NOT EMSCRIPTEN)
        message(FATAL_ERROR "WRKZ_BUILD_WALLET_WASM requires Emscripten toolchain")
    endif ()

    target_include_directories(wallet_wasm PRIVATE
        ${CMAKE_SOURCE_DIR}/extras/web-wallet-wasm/wasm/include
        ${CMAKE_SOURCE_DIR}/src
    )
    target_link_libraries(wallet_wasm PRIVATE wallet_capi_c)
    set_target_properties(wallet_wasm PROPERTIES
        OUTPUT_NAME "wallet_wasm"
        SUFFIX ".js"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/wasm"
    )
    target_compile_options(wallet_wasm PRIVATE
        -fexceptions
    )
    if (WRKZ_WASM_PTHREADS)
        target_compile_options(wallet_wasm PRIVATE
            -pthread
            -matomics
            -mbulk-memory
        )
    endif ()
    target_link_options(wallet_wasm PRIVATE
        -fexceptions
        -sWASM=1
        -sALLOW_MEMORY_GROWTH=1
        -sMODULARIZE=1
        -sEXPORT_ES6=1
        -sENVIRONMENT=web,worker
        -sNO_EXIT_RUNTIME=1
        -sEXPORTED_FUNCTIONS=['_wallet_wasm_request','_malloc','_free']
        -sEXPORTED_RUNTIME_METHODS=['ccall','cwrap','UTF8ToString','stringToUTF8','lengthBytesUTF8']
        -sERROR_ON_UNDEFINED_SYMBOLS=0
        --no-entry
    )
    if (WRKZ_WASM_PTHREADS)
        target_link_options(wallet_wasm PRIVATE
            -pthread
            -sUSE_PTHREADS=1
            -sPTHREAD_POOL_SIZE=4
        )
    endif ()
endif ()

if (OPENSSL_FOUND)
    target_link_libraries(Nigel ${OPENSSL_LIBRARIES})
    if (WRKZ_BUILD_EXECUTABLES)
        target_link_libraries(miner ${OPENSSL_LIBRARIES})
        target_link_libraries(WalletApi ${OPENSSL_LIBRARIES})
        target_link_libraries(zedwallet++ ${OPENSSL_LIBRARIES})
        target_link_libraries(WalletService ${OPENSSL_LIBRARIES})
        target_link_libraries(Wrkzd ${OPENSSL_LIBRARIES})
    endif ()
endif ()

## cpp-httplib's gzip support is header only but needs zlib at link time.
## Everything here either serves or consumes HTTP through it.
if (ZLIB_FOUND)
    target_link_libraries(Nigel ${ZLIB_LIBRARIES})
    if (WRKZ_BUILD_EXECUTABLES)
        target_link_libraries(miner ${ZLIB_LIBRARIES})
        target_link_libraries(WalletApi ${ZLIB_LIBRARIES})
        target_link_libraries(zedwallet++ ${ZLIB_LIBRARIES})
        target_link_libraries(WalletService ${ZLIB_LIBRARIES})
        target_link_libraries(Wrkzd ${ZLIB_LIBRARIES})
    endif ()
endif ()

if (ENABLE_ZMQ AND WRKZ_BUILD_EXECUTABLES)
    target_link_libraries(Wrkzd ${WRKZ_ZMQ_TARGET})
    target_compile_definitions(Wrkzd PRIVATE WRKZ_ENABLE_ZMQ=1)
    if (WIN32 AND WRKZ_ZMQ_TARGET STREQUAL "libzmq-static")
        target_compile_definitions(Wrkzd PRIVATE ZMQ_STATIC=1)
    endif ()
    if (WIN32 AND WRKZ_ZMQ_TARGET STREQUAL "libzmq" AND TARGET libzmq)
        # Stage runtime DLL next to Wrkzd.exe on every build, even if Wrkzd
        # itself was not relinked in this invocation.
        add_custom_target(wrkzd_stage_libzmq ALL
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:libzmq>
                $<TARGET_FILE_DIR:Wrkzd>
            DEPENDS Wrkzd libzmq
            COMMENT "Copying libzmq runtime DLL next to Wrkzd.exe"
        )
    endif ()
endif ()

if (WIN32 AND NOT MSVC AND WRKZ_BUILD_EXECUTABLES)
    # RocksDB Windows objects require shlwapi and rpc symbols; keep them late.
    target_link_libraries(Wrkzd shlwapi rpcrt4)
endif ()

if (ANDROID AND WRKZ_ANDROID_UCONTEXT_LIB)
    # Keep libucontext late in link order for static archives.
    # Shared libs (wallet_capi) need explicit linking because static archive
    # symbols are not automatically pulled into the final .so.
    if (WRKZ_BUILD_WALLET_CAPI)
        target_link_libraries(wallet_capi PRIVATE "${WRKZ_ANDROID_UCONTEXT_LIB}")
    endif ()
    if (WRKZ_BUILD_EXECUTABLES)
        target_link_libraries(cryptotest "${WRKZ_ANDROID_UCONTEXT_LIB}")
        target_link_libraries(masternodetest "${WRKZ_ANDROID_UCONTEXT_LIB}")
        target_link_libraries(miner "${WRKZ_ANDROID_UCONTEXT_LIB}")
        target_link_libraries(WalletApi "${WRKZ_ANDROID_UCONTEXT_LIB}")
        target_link_libraries(WalletService "${WRKZ_ANDROID_UCONTEXT_LIB}")
        target_link_libraries(WalletUpgrader "${WRKZ_ANDROID_UCONTEXT_LIB}")
        target_link_libraries(zedwallet++ "${WRKZ_ANDROID_UCONTEXT_LIB}")
        target_link_libraries(Wrkzd "${WRKZ_ANDROID_UCONTEXT_LIB}")
    endif ()
endif ()

# Add dependencies means we have to build the latter before we build the former
# In this case it's because we need to have the current version name rather
# than a cached one
add_dependencies(JsonRpcServer version)
add_dependencies(P2P version)
add_dependencies(Rpc version)
if (WRKZ_BUILD_EXECUTABLES)
    add_dependencies(cryptotest version)
    add_dependencies(miner version)
    add_dependencies(Wrkzd version)
    add_dependencies(WalletUpgrader version)
    add_dependencies(WalletApi version)
    add_dependencies(WalletService version)
    add_dependencies(zedwallet++ version)
endif ()
if (WRKZ_BUILD_WALLET_CAPI)
    add_dependencies(wallet_capi_c version)
    add_dependencies(wallet_capi version)
endif ()
if (WRKZ_BUILD_WALLET_WASM)
    add_dependencies(wallet_wasm version)
endif ()

# Finally build the binaries
if (WRKZ_BUILD_EXECUTABLES)
    set_property(TARGET Wrkzd PROPERTY OUTPUT_NAME "Wrkzd")
    set_property(TARGET zedwallet++ PROPERTY OUTPUT_NAME "wrkz-wallet")
    set_property(TARGET WalletService PROPERTY OUTPUT_NAME "wrkz-service")
    set_property(TARGET miner PROPERTY OUTPUT_NAME "miner")
    set_property(TARGET cryptotest PROPERTY OUTPUT_NAME "cryptotest")
    set_property(TARGET masternodetest PROPERTY OUTPUT_NAME "masternodetest")
    set_property(TARGET WalletApi PROPERTY OUTPUT_NAME "wrkz-wallet-api")
    set_property(TARGET WalletUpgrader PROPERTY OUTPUT_NAME "wallet-upgrader")
endif ()

# Additional make targets, can be used to build a subset of the targets
# e.g. make pool will build only Wrkzd and service
if (WRKZ_BUILD_EXECUTABLES)
    add_custom_target(pool DEPENDS Wrkzd WalletService)
    add_custom_target(solominer DEPENDS Wrkzd zedwallet++ miner)
    add_custom_target(cli DEPENDS Wrkzd zedwallet++)
    add_custom_target(wrkz-wallet DEPENDS zedwallet++)
endif ()

