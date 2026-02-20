# Build statically
add_definitions(-DSTATICLIB)

# Add external libraries as include dirs, so we can do #include "httplib" for example
include_directories(${CMAKE_SOURCE_DIR}/external/rocksdb/include)
include_directories(${CMAKE_SOURCE_DIR}/external/cpp-linenoise)
include_directories(${CMAKE_SOURCE_DIR}/external/cpp-httplib)
include_directories(${CMAKE_SOURCE_DIR}/external/nlohmann-json)
include_directories(${CMAKE_SOURCE_DIR}/external/rapidjson)
include_directories(${CMAKE_SOURCE_DIR}/external/cxxopts)
include_directories(${CMAKE_SOURCE_DIR}/external/cryptopp)
include_directories(${CMAKE_SOURCE_DIR}/external/argon2/include)
include_directories(${CMAKE_SOURCE_DIR}/external/zstd)

# Show cmake where the source files are
# Note, if you add remove a source file, you will need to re-run cmake so it
# can find the new file
file(GLOB_RECURSE Common common/*)
file(GLOB_RECURSE Config config/*)
file(GLOB_RECURSE Crypto crypto/*)
file(GLOB_RECURSE CryptoNoteCore cryptonotecore/* CryptoNoteConfig.h)
file(GLOB_RECURSE CryptoNoteProtocol cryptonoteprotocol/*)
file(GLOB_RECURSE CryptoTest cryptotest/*)
file(GLOB_RECURSE Errors errors/*)
file(GLOB_RECURSE Http http/*)
file(GLOB_RECURSE JsonRpcServer jsonrpcserver/*)
file(GLOB_RECURSE Logging logging/*)
file(GLOB_RECURSE Logger logger/*)
file(GLOB_RECURSE miner miner/*)
file(GLOB_RECURSE Mnemonics mnemonics/*)
file(GLOB_RECURSE Nigel nigel/*)
file(GLOB_RECURSE NodeRpcProxy noderpcproxy/*)
file(GLOB_RECURSE P2p p2p/*)
file(GLOB_RECURSE Rpc rpc/*)
file(GLOB_RECURSE Serialization serialization/*)
file(GLOB_RECURSE SubWallets subwallets/*)
file(GLOB_RECURSE Transfers transfers/*)
file(GLOB_RECURSE Wrkzd daemon/*)
file(GLOB_RECURSE Utilities utilities/*)
file(GLOB_RECURSE Wallet wallet/*)
file(GLOB_RECURSE WalletApi walletapi/*)
file(GLOB_RECURSE WalletBackend walletbackend/*)
file(GLOB_RECURSE WalletService walletservice/*)
file(GLOB_RECURSE WalletUpgrader walletupgrader/*)
file(GLOB_RECURSE zedwallet++ zedwallet++/*)

if (WIN32)
    file(GLOB_RECURSE System system/* platform/windows/system/*)
elseif (APPLE)
    file(GLOB_RECURSE System system/* platform/osx/system/* platform/posix/system/*)
else ()
    file(GLOB_RECURSE System system/* platform/linux/system/* platform/posix/system/*)
endif ()

# Group the files together in IDEs
source_group("" FILES $${Common} ${Config} ${Crypto} ${CryptoNoteCore} ${CryptoNoteProtocol} ${Wrkzd} ${JsonRpcServer} ${Http} ${Logging} ${Logger} ${miner} ${Mnemonics} ${Nigel} ${NodeRpcProxy} ${P2p} ${Rpc} ${Serialization} ${System} ${Transfers} ${Wallet} ${WalletApi} ${WalletBackend} ${WalletService} ${zedwallet++} ${CryptoTest} ${Errors} ${Utilities} ${WalletUpgrader} ${SubWallets})

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

add_executable(cryptotest ${CryptoTest} ${CT_SOURCES_OS})
add_executable(miner ${miner} ${MINER_SOURCES_OS})
add_executable(WalletService ${WalletService} ${PG_SOURCES_OS})
add_executable(Wrkzd ${Wrkzd} ${DAEMON_SOURCES_OS})
add_executable(WalletApi ${WalletApi} ${WALLET_API_SOURCES_OS})
add_executable(WalletUpgrader ${WalletUpgrader} ${WALLET_UPGRADER_SOURCES_OS})
add_executable(zedwallet++ ${zedwallet++} ${ZED_WALLET_SOURCES_OS})

if (WIN32)
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
endif ()

# A bit of hackery so we don't have to do the if/else/ for every target that
# wants to use filesystem
add_library(__filesystem INTERFACE)

# Windows works out of the box
if (APPLE)
    target_link_libraries(__filesystem INTERFACE /usr/local/opt/llvm/lib/libc++fs.a)
elseif (UNIX)
    target_link_libraries(__filesystem INTERFACE stdc++fs)
endif ()

if (MSVC)
    target_link_libraries(Wrkzd System CryptoNoteCore rocksdb libzstd Errors)
else ()
    target_link_libraries(Wrkzd System CryptoNoteCore rocksdblib libzstd Errors)
endif ()

# Add the dependencies we need
target_link_libraries(Common __filesystem)
target_link_libraries(Crypto argon2)
target_link_libraries(CryptoNoteCore Utilities Common Logging Crypto P2P Rpc Http Serialization System WalletBackend)
target_link_libraries(cryptotest Crypto Common)
target_link_libraries(Errors Crypto SubWallets Utilities)
target_link_libraries(Logging Common ${Boost_LIBRARIES})
target_link_libraries(miner Crypto Errors Utilities System Serialization)
target_link_libraries(Nigel Errors CryptoNoteCore)
target_link_libraries(NodeRpcProxy Rpc)
target_link_libraries(P2P upnpc-static Serialization System CryptoNoteCore)
target_link_libraries(Rpc P2P Utilities CryptoNoteCore)
target_link_libraries(Serialization Common Crypto)
target_link_libraries(SubWallets Common Logger)
target_link_libraries(Transfers CryptoNoteCore)
target_link_libraries(Utilities Common Errors)
target_link_libraries(Wallet NodeRpcProxy Transfers CryptoNoteCore Common WalletBackend)
target_link_libraries(WalletApi WalletBackend)
target_link_libraries(WalletBackend Serialization Mnemonics Nigel cryptopp-static __filesystem Utilities SubWallets Logger Config Wallet)
target_link_libraries(WalletService JsonRpcServer Wallet Mnemonics Errors)
target_link_libraries(WalletUpgrader Utilities WalletBackend Common)
target_link_libraries(zedwallet++ WalletBackend)

if (OPENSSL_FOUND)
    target_link_libraries(miner ${OPENSSL_LIBRARIES})
    target_link_libraries(Nigel ${OPENSSL_LIBRARIES})
    target_link_libraries(WalletApi ${OPENSSL_LIBRARIES})
    target_link_libraries(zedwallet++ ${OPENSSL_LIBRARIES})
    target_link_libraries(WalletService ${OPENSSL_LIBRARIES})
    target_link_libraries(Wrkzd ${OPENSSL_LIBRARIES})
endif ()

if (ENABLE_ZMQ)
    target_link_libraries(Wrkzd ${ZMQ_LIBRARY})
    if (ZMQ_EXTRA_LIBS)
        target_link_libraries(Wrkzd ${ZMQ_EXTRA_LIBS})
    endif ()
    target_compile_definitions(Wrkzd PRIVATE WRKZ_ENABLE_ZMQ=1)
endif ()

if (WIN32 AND NOT MSVC)
    # RocksDB Windows objects require shlwapi and rpc symbols; keep them late.
    target_link_libraries(Wrkzd shlwapi rpcrt4)
endif ()

# Add dependencies means we have to build the latter before we build the former
# In this case it's because we need to have the current version name rather
# than a cached one
add_dependencies(cryptotest version)
add_dependencies(miner version)
add_dependencies(JsonRpcServer version)
add_dependencies(P2P version)
add_dependencies(Rpc version)
add_dependencies(Wrkzd version)
add_dependencies(WalletUpgrader version)
add_dependencies(WalletApi version)
add_dependencies(WalletService version)
add_dependencies(zedwallet++ version)

# Finally build the binaries
set_property(TARGET Wrkzd PROPERTY OUTPUT_NAME "Wrkzd")
set_property(TARGET zedwallet++ PROPERTY OUTPUT_NAME "wrkz-wallet")
set_property(TARGET WalletService PROPERTY OUTPUT_NAME "wrkz-service")
set_property(TARGET miner PROPERTY OUTPUT_NAME "miner")
set_property(TARGET cryptotest PROPERTY OUTPUT_NAME "cryptotest")
set_property(TARGET WalletApi PROPERTY OUTPUT_NAME "wrkz-wallet-api")
set_property(TARGET WalletUpgrader PROPERTY OUTPUT_NAME "wallet-upgrader")

# Additional make targets, can be used to build a subset of the targets
# e.g. make pool will build only Wrkzd and service
add_custom_target(pool DEPENDS Wrkzd WalletService)
add_custom_target(solominer DEPENDS Wrkzd zedwallet++ miner)
add_custom_target(cli DEPENDS Wrkzd zedwallet++)
add_custom_target(wrkz-wallet DEPENDS zedwallet++)
