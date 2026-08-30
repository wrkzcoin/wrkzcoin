// Copyright (c) 2018-2019, The TurtleCoin Developers
// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

////////////////////////////////////////
#include <walletbackend/WalletBackend.h>
////////////////////////////////////////

#include "JsonHelper.h"

#include <common/Base58.h>
#include <common/FileSystemShim.h>
#if defined(__EMSCRIPTEN__)
#include <wasm_fs_bridge.h>
#endif
#include <config/Constants.h>
#include <config/CryptoNoteConfig.h>
#include <config/WalletConfig.h>
#include <crypto/crypto.h>
#include <crypto/random.h>
#include <cryptonotecore/Currency.h>
#include "crypto/WalletCrypto.h"

#include <errors/ValidateParameters.h>
#include <fstream>
#include <future>
#include <iomanip>
#include <logger/Logger.h>
#include <logging/LoggerManager.h>
#include <mnemonics/Mnemonics.h>
#include <noderpcproxy/NodeRpcProxy.h>
#include <utilities/Addresses.h>
#include <utilities/Mixins.h>
#include <utilities/Utilities.h>
#include <walletbackend/Constants.h>
#include <walletbackend/Transfer.h>


//////////////////////////
/* NON MEMBER FUNCTIONS */
//////////////////////////

/* Anonymous namespace so it doesn't clash with anything else */
namespace
{
    /* Check data has the magic indicator from first : last, and remove it if
       it does. Else, return an error depending on where we failed */
    template<class Buffer, class Identifier>
    Error hasMagicIdentifier(
        Buffer &data,
        const Identifier &identifier,
        const Error tooSmallError,
        const Error wrongIdentifierError)
    {
        /* Check we've got space for the identifier */
        if (data.size() < identifier.size())
        {
            return tooSmallError;
        }

        if (!std::equal(identifier.begin(), identifier.end(), data.begin()))
        {
            return wrongIdentifierError;
        }

        /* Remove the identifier from the string */
        data.erase(data.begin(), data.begin() + identifier.size());

        return SUCCESS;
    }

    /* Check the wallet filename for the new wallet to be created is valid */
    Error checkNewWalletFilename(std::string filename)
    {
#if defined(__EMSCRIPTEN__)
        /* In WASM mode we use in-memory store — just check name isn't taken */
        if (WasmFs::exists(filename))
        {
            return WALLET_FILE_ALREADY_EXISTS;
        }
        if (filename.empty())
        {
            return INVALID_WALLET_FILENAME;
        }
        return SUCCESS;
#else
        /* Check the file doesn't exist */
        if (std::ifstream(filename))
        {
            return WALLET_FILE_ALREADY_EXISTS;
        }

        /* Check we can open the file */
        if (!std::ofstream(filename))
        {
            return INVALID_WALLET_FILENAME;
        }

        /* Don't leave random files around if we fail later down the road */
        fs::remove(filename);

        return SUCCESS;
#endif
    }

} // namespace

///////////////////////////////////
/* CONSTRUCTORS / DECONSTRUCTORS */
///////////////////////////////////

/* Constructor */
WalletBackend::WalletBackend()
{
    m_eventHandler = std::make_shared<EventHandler>();

    /* Remember to correctly initialize the daemon -
    we can't do it here since we don't have the host/port, and the json
    serialization uses the default constructor */
}

/* Deconstructor */
WalletBackend::~WalletBackend()
{
    /* Save, but only if the non default constructor was used - else things
       will be uninitialized, and crash */
    if (m_daemon != nullptr && !m_filename.empty())
    {
        save();
    }
}

/* Standard Constructor */
WalletBackend::WalletBackend(
    const std::string filename,
    const std::string password,
    const Crypto::SecretKey privateSpendKey,
    const Crypto::SecretKey privateViewKey,
    const uint64_t scanHeight,
    const bool newWallet,
    const std::string daemonHost,
    const uint16_t daemonPort,
    const bool daemonSSL,
    const unsigned int syncThreadCount):

    m_filename(filename),
    m_password(password),
    m_daemon(std::make_shared<Nigel>(daemonHost, daemonPort, daemonSSL)),
    m_syncThreadCount(syncThreadCount)
{
    /* Generate the address from the two private keys */
    std::string address = Utilities::privateKeysToAddress(privateSpendKey, privateViewKey);

    m_eventHandler = std::make_shared<EventHandler>();

    m_subWallets = std::make_shared<SubWallets>(privateSpendKey, privateViewKey, address, scanHeight, newWallet);
}

/* View Wallet Constructor */
WalletBackend::WalletBackend(
    const std::string filename,
    const std::string password,
    const Crypto::SecretKey privateViewKey,
    const std::string address,
    const uint64_t scanHeight,
    const std::string daemonHost,
    const uint16_t daemonPort,
    const bool daemonSSL,
    const unsigned int syncThreadCount):

    m_filename(filename),
    m_password(password),
    m_daemon(std::make_shared<Nigel>(daemonHost, daemonPort, daemonSSL)),
    m_syncThreadCount(syncThreadCount)
{
    bool newWallet = false;

    m_eventHandler = std::make_shared<EventHandler>();

    m_subWallets = std::make_shared<SubWallets>(privateViewKey, address, scanHeight, newWallet);
}

//////////////////////
/* STATIC FUNCTIONS */
//////////////////////

/* Imports a wallet from a mnemonic seed. Returns the wallet class,
   or an error. */
std::tuple<Error, std::shared_ptr<WalletBackend>> WalletBackend::importWalletFromSeed(
    const std::string mnemonicSeed,
    const std::string filename,
    const std::string password,
    const uint64_t scanHeight,
    const std::string daemonHost,
    const uint16_t daemonPort,
    const bool daemonSSL,
    const unsigned int syncThreadCount)
{
    /* Check the filename is valid */
    if (Error error = checkNewWalletFilename(filename); error != SUCCESS)
    {
        return {error, nullptr};
    }

    /* Convert the mnemonic into a private spend key */
    auto [mnemonicError, privateSpendKey] = Mnemonics::MnemonicToPrivateKey(mnemonicSeed);

    if (mnemonicError)
    {
        return {mnemonicError, nullptr};
    }

    Crypto::SecretKey privateViewKey;

    /* Derive the private view key from the private spend key */
    Crypto::crypto_ops::generateViewFromSpend(privateSpendKey, privateViewKey);

    if (Error error = validatePrivateKey(privateViewKey); error != SUCCESS)
    {
        return {error, nullptr};
    }

    /* Just defining here so it's more obvious what we're doing in the
       constructor */
    bool newWallet = false;

    const std::shared_ptr<WalletBackend> wallet(new WalletBackend(
        filename,
        password,
        privateSpendKey,
        privateViewKey,
        scanHeight,
        newWallet,
        daemonHost,
        daemonPort,
        daemonSSL,
        syncThreadCount));

    wallet->init();

    /* Save to disk */
    Error error = wallet->save();

    return {error, wallet};
}

/* Imports a wallet from a private spend key and a view key. Returns
   the wallet class, or an error. */
std::tuple<Error, std::shared_ptr<WalletBackend>> WalletBackend::importWalletFromKeys(
    const Crypto::SecretKey privateSpendKey,
    const Crypto::SecretKey privateViewKey,
    const std::string filename,
    const std::string password,
    const uint64_t scanHeight,
    const std::string daemonHost,
    const uint16_t daemonPort,
    const bool daemonSSL,
    const unsigned int syncThreadCount)
{
    /* Check the filename is valid */
    if (Error error = checkNewWalletFilename(filename); error != SUCCESS)
    {
        return {error, nullptr};
    }

    if (Error error = validatePrivateKey(privateViewKey); error != SUCCESS)
    {
        return {error, nullptr};
    }

    if (Error error = validatePrivateKey(privateSpendKey); error != SUCCESS)
    {
        return {error, nullptr};
    }

    /* Just defining here so it's more obvious what we're doing in the
       constructor */
    bool newWallet = false;

    const std::shared_ptr<WalletBackend> wallet(new WalletBackend(
        filename,
        password,
        privateSpendKey,
        privateViewKey,
        scanHeight,
        newWallet,
        daemonHost,
        daemonPort,
        daemonSSL,
        syncThreadCount));

    wallet->init();

    /* Save to disk */
    Error error = wallet->save();

    return {error, wallet};
}

std::tuple<Error, std::shared_ptr<WalletBackend>> WalletBackend::importWalletFromKeysTransient(
    const Crypto::SecretKey privateSpendKey,
    const Crypto::SecretKey privateViewKey,
    const std::string password,
    const uint64_t scanHeight,
    const std::string daemonHost,
    const uint16_t daemonPort,
    const bool daemonSSL,
    const unsigned int syncThreadCount)
{
    if (Error error = validatePrivateKey(privateViewKey); error != SUCCESS)
    {
        return {error, nullptr};
    }

    if (Error error = validatePrivateKey(privateSpendKey); error != SUCCESS)
    {
        return {error, nullptr};
    }

    bool newWallet = false;

    const std::shared_ptr<WalletBackend> wallet(new WalletBackend(
        "" /* transient wallet: no file persistence */,
        password,
        privateSpendKey,
        privateViewKey,
        scanHeight,
        newWallet,
        daemonHost,
        daemonPort,
        daemonSSL,
        syncThreadCount));

    wallet->init();

    return {SUCCESS, wallet};
}

/* Imports a view wallet from a private view key and an address.
   Returns the wallet class, or an error. */
std::tuple<Error, std::shared_ptr<WalletBackend>> WalletBackend::importViewWallet(
    const Crypto::SecretKey privateViewKey,
    const std::string address,
    const std::string filename,
    const std::string password,
    const uint64_t scanHeight,
    const std::string daemonHost,
    const uint16_t daemonPort,
    const bool daemonSSL,
    const unsigned int syncThreadCount)
{
    /* Check the filename is valid */
    if (Error error = checkNewWalletFilename(filename); error != SUCCESS)
    {
        return {error, nullptr};
    }

    if (Error error = validatePrivateKey(privateViewKey); error != SUCCESS)
    {
        return {error, nullptr};
    }

    const bool allowIntegratedAddresses = false;

    if (Error error = validateAddresses({address}, allowIntegratedAddresses); error != SUCCESS)
    {
        return {error, nullptr};
    }

    const std::shared_ptr<WalletBackend> wallet(new WalletBackend(
        filename, password, privateViewKey, address, scanHeight, daemonHost, daemonPort, daemonSSL, syncThreadCount));

    wallet->init();

    /* Save to disk */
    Error error = wallet->save();

    return {error, wallet};
}

/* Creates a new wallet with the given filename and password */
std::tuple<Error, std::shared_ptr<WalletBackend>> WalletBackend::createWallet(
    const std::string filename,
    const std::string password,
    const std::string daemonHost,
    const uint16_t daemonPort,
    const bool daemonSSL,
    const unsigned int syncThreadCount)
{
    /* Check the filename is valid */
    if (Error error = checkNewWalletFilename(filename); error != SUCCESS)
    {
        return {error, nullptr};
    }

    CryptoNote::KeyPair spendKey;
    Crypto::SecretKey privateViewKey;
    Crypto::PublicKey publicViewKey;

    /* Generate a spend key */
    Crypto::generate_keys(spendKey.publicKey, spendKey.secretKey);

    /* Derive the view key from the spend key */
    Crypto::crypto_ops::generateViewFromSpend(spendKey.secretKey, privateViewKey, publicViewKey);

    /* Just defining here so it's more obvious what we're doing in the
       constructor */
    bool newWallet = true;
    uint64_t scanHeight = 0;

    const std::shared_ptr<WalletBackend> wallet(new WalletBackend(
        filename,
        password,
        spendKey.secretKey,
        privateViewKey,
        scanHeight,
        newWallet,
        daemonHost,
        daemonPort,
        daemonSSL,
        syncThreadCount));

    wallet->init();

    /* Save to disk */
    Error error = wallet->save();

    return {error, wallet};
}

bool WalletBackend::tryUpgradeWalletFormat(
    const std::string filename,
    const std::string password,
    const std::string daemonHost,
    const uint16_t daemonPort)
{
    try
    {
        const auto logManager = std::make_shared<Logging::LoggerManager>();

        /* Currency contains our coin parameters, such as decimal places, supply */
        const CryptoNote::Currency currency = CryptoNote::CurrencyBuilder(logManager).currency();

        System::Dispatcher localDispatcher;
        System::Dispatcher *dispatcher = &localDispatcher;

        /* Our connection to turtlecoind */
        std::unique_ptr<CryptoNote::INode> node(new CryptoNote::NodeRpcProxy(daemonHost, daemonPort, 10, logManager));

        /* Save the old wallet to the backup file via simple file copy operation */
        std::error_code backupError;

        fs::path filepath = filename;
        fs::path backupFilepath = filepath.parent_path() / "old-version-backup-" += filepath.filename();

        fs::copy(filename, backupFilepath, fs::copy_options::overwrite_existing, backupError);

        /* If we could not backup the file then instantly fail for safety sake */
        if (backupError)
        {
            return false;
        }

        CryptoNote::WalletGreen wallet(*dispatcher, currency, *node, logManager);

        /* Attempt to open the specified file as a wallet */
        wallet.load(filename, password);

        /* Cool, it worked. Upgrade to the new format. */
        const std::string json = wallet.toNewFormatJSON();

        /* We have to close the wallet before we can overwrite it */
        wallet.shutdown();

        /* Save to disk with the new format. */
        Error error = saveWalletJSONToDisk(json, filename, password);

        if (error)
        {
            return false;
        }

        return true;
    }
    /* Not a WalletGreen format. */
    catch (const std::system_error &)
    {
        return false;
    }
}

/* Opens a wallet already on disk with the given filename + password */
std::tuple<Error, std::shared_ptr<WalletBackend>> WalletBackend::openWallet(
    const std::string filename,
    const std::string password,
    const std::string daemonHost,
    const uint16_t daemonPort,
    const bool daemonSSL,
    const unsigned int syncThreadCount)
{
#if defined(__EMSCRIPTEN__)
    /* WASM: read from in-memory store (backed by browser IndexedDB) */
    std::vector<char> buffer = WasmFs::read(filename);
    if (buffer.empty())
    {
        return {FILENAME_NON_EXISTENT, nullptr};
    }
#else
    /* Open in binary mode, since we have encrypted data */
    std::ifstream file(filename, std::ios_base::binary);

    /* Check we successfully opened the file */
    if (!file)
    {
        return {FILENAME_NON_EXISTENT, nullptr};
    }

    /* Read file into a buffer */
    std::vector<char> buffer((std::istreambuf_iterator<char>(file)), (std::istreambuf_iterator<char>()));
#endif

    /* Check that the decrypted data has the 'isAWallet' identifier,
       and remove it it does. If it doesn't, return an error. */
    Error error = hasMagicIdentifier(buffer, Constants::IS_A_WALLET_IDENTIFIER, NOT_A_WALLET_FILE, NOT_A_WALLET_FILE);

    /* Not a WalletBackend wallet */
    if (error)
    {
        /* See if it's a WalletGreen wallet, and upgrade if it is */
        const bool isWalletGreenFile = tryUpgradeWalletFormat(filename, password, daemonHost, daemonPort);

        if (isWalletGreenFile)
        {
            /* Then try and open again */
            return openWallet(filename, password, daemonHost, daemonPort, daemonSSL, syncThreadCount);
        }
        else
        {
            return {error, nullptr};
        }
    }

    /* The salt we use for both PBKDF2, and AES decryption */
    uint8_t salt[WalletCrypto::SALT_SIZE];

    /* Check the file is large enough for the salt */
    if (buffer.size() < sizeof(salt))
    {
        return {WALLET_FILE_CORRUPTED, nullptr};
    }

    /* Copy the salt to the salt array */
    std::copy(buffer.begin(), buffer.begin() + sizeof(salt), salt);

    /* Remove the salt, don't need it anymore */
    buffer.erase(buffer.begin(), buffer.begin() + sizeof(salt));

    /* The key we use for AES decryption, generated with PBKDF2-HMAC-SHA256 */
    const auto key =
        WalletCrypto::deriveKey(password, salt, sizeof(salt), Constants::PBKDF2_ITERATIONS, WalletCrypto::KEY_SIZE);

    /* Decrypt, handling padding. The salt doubles as the IV.

       do NOT report an alternate error for invalid padding. It allows them
       to do a padding oracle attack, I believe. Just report the wrong password
       error. */
    const auto decrypted = WalletCrypto::decrypt(std::string(buffer.begin(), buffer.end()), key.data(), salt);

    if (!decrypted)
    {
        return {WRONG_PASSWORD, nullptr};
    }

    /* This stores the decrypted data. Not const: hasMagicIdentifier() strips
       the identifier prefix from it in place below. */
    std::string decryptedData = *decrypted;

    /* Check that the decrypted data has the 'isCorrectPassword' identifier,
       and remove it it does. If it doesn't, return an error. */
    error = hasMagicIdentifier(
        decryptedData, Constants::IS_CORRECT_PASSWORD_IDENTIFIER, WALLET_FILE_CORRUPTED, WRONG_PASSWORD);

    if (error)
    {
        return {error, nullptr};
    }

    try
    {
        const bool dumpJson = false;

        /* For debugging purposes */
        if (dumpJson)
        {
            std::ofstream o("walletData.json");
            o << decryptedData << std::endl;
        }

        nlohmann::json walletJson;
        try
        {
            walletJson = nlohmann::json::parse(decryptedData);
        }
        catch (const nlohmann::json::parse_error &)
        {
            return {WALLET_FILE_CORRUPTED, nullptr};
        }

        /* Make our wallet object */
        const auto wallet = std::make_shared<WalletBackend>();

        /* Initialize it from the json (We could do this in less steps, but it
           requires a move/copy constructor) */
        error = wallet->fromJSON(walletJson, filename, password, daemonHost, daemonPort, daemonSSL, syncThreadCount);

        return {error, wallet};
    }
    catch (const std::invalid_argument &e)
    {
        Logger::logger.log(
            std::string("Failed to open wallet file: ") + e.what(), Logger::FATAL, {Logger::FILESYSTEM, Logger::SAVE});

        return {WALLET_FILE_CORRUPTED, nullptr};
    }
}

Error WalletBackend::saveWalletJSONToDisk(std::string walletJSON, std::string filename, std::string password)
{
    /* Add an identifier to the start of the string so we can verify the wallet
       has been correctly decrypted */
    std::string identiferAsString(
        Constants::IS_CORRECT_PASSWORD_IDENTIFIER.begin(), Constants::IS_CORRECT_PASSWORD_IDENTIFIER.end());

    /* Add magic identifier, and get wallet as a JSON string */
    std::string walletData = identiferAsString + walletJSON;

    /* The salt we use for both PBKDF2, and AES Encryption */
    uint8_t salt[WalletCrypto::SALT_SIZE];

    /* Generate 16 random bytes for the salt */
    Random::randomBytes(sizeof(salt), salt);

    /* The key we use for AES encryption, generated with PBKDF2-HMAC-SHA256 */
    const auto key =
        WalletCrypto::deriveKey(password, salt, sizeof(salt), Constants::PBKDF2_ITERATIONS, WalletCrypto::KEY_SIZE);

    /* Encrypt, and pad. The salt doubles as the IV. */
    const std::string encryptedData = WalletCrypto::encrypt(walletData, key.data(), salt);

#if defined(__EMSCRIPTEN__)
    /* WASM: write to in-memory store (JS side persists to IndexedDB) */
    if (filename.empty())
    {
        Logger::logger.log(
            std::string("Wallet filename is empty"),
            Logger::FATAL,
            {Logger::FILESYSTEM, Logger::SAVE});
        return INVALID_WALLET_FILENAME;
    }

    /* Build the full file data: identifier + salt + encrypted payload */
    std::vector<char> fileData;
    fileData.insert(fileData.end(),
        Constants::IS_A_WALLET_IDENTIFIER.begin(),
        Constants::IS_A_WALLET_IDENTIFIER.end());
    fileData.insert(fileData.end(), std::begin(salt), std::end(salt));
    fileData.insert(fileData.end(), encryptedData.begin(), encryptedData.end());

    WasmFs::write(filename, fileData);

    return SUCCESS;
#else
    std::ofstream file(filename, std::ios_base::binary);

    if (!file)
    {
        Logger::logger.log(
            std::string("Wallet filename: ") + filename + " is invalid",
            Logger::FATAL,
            {Logger::FILESYSTEM, Logger::SAVE});

        return INVALID_WALLET_FILENAME;
    }

    std::string saltString = std::string(salt, salt + sizeof(salt));

    /* Write the isAWalletIdentifier to the file, so when we open it we can
       verify that it is a wallet file */
    std::copy(
        Constants::IS_A_WALLET_IDENTIFIER.begin(),
        Constants::IS_A_WALLET_IDENTIFIER.end(),
        std::ostreambuf_iterator<char>(file));

    /* Write the salt to the file, so we can use it to unencrypt the file
       later. Note that the salt is unencrypted. */
    std::copy(std::begin(salt), std::end(salt), std::ostreambuf_iterator<char>(file));

    /* Write the encrypted wallet data to the file */
    std::copy(encryptedData.begin(), encryptedData.end(), std::ostreambuf_iterator<char>(file));

    return SUCCESS;
#endif
}

/////////////////////
/* CLASS FUNCTIONS */
/////////////////////

void WalletBackend::init()
{
    if (m_daemon == nullptr)
    {
        throw std::runtime_error("Daemon has not been initialized!");
    }

    /* When syncThreadCount == 0 (WASM single-threaded mode), skip Nigel's
       background refresh thread — daemon info is fetched on demand instead. */
    m_daemon->init(m_syncThreadCount > 0);

    /* Init the wallet synchronizer if it hasn't been loaded from the wallet
       file */
    if (m_walletSynchronizer == nullptr)
    {
        auto [startHeight, startTimestamp] = m_subWallets->getMinInitialSyncStart();

        /* New wallets may store both a creation timestamp and an implied
           creation height candidate. Use the lower scan point to avoid
           missing first transactions and avoid requiring an immediate reset. */
        if (startHeight == 0 && startTimestamp != 0)
        {
            const uint64_t createdHeight = m_daemon->networkBlockCount();
            const uint64_t timestampHeight = Utilities::timestampToScanHeight(startTimestamp);
            startHeight = std::min(createdHeight, timestampHeight);
            startTimestamp = 0;
        }

        m_walletSynchronizer = std::make_shared<WalletSynchronizer>(
            m_daemon,
            startHeight,
            startTimestamp,
            m_subWallets->getPrivateViewKey(),
            m_eventHandler,
            m_syncThreadCount);
    }
    /* If it has, just initialize the stuff we can't from file */
    else
    {
        m_walletSynchronizer->initializeAfterLoad(m_daemon, m_eventHandler, m_syncThreadCount);
    }

    m_walletSynchronizer->setSubWallets(m_subWallets);

    /* Launch the wallet sync process in a background thread */
    m_walletSynchronizer->start();

    m_syncRAIIWrapper = std::make_shared<WalletSynchronizerRAIIWrapper>(m_walletSynchronizer);
}

Error WalletBackend::save() const
{
    /* Only the snapshot needs the synchronizer held still. Writing it out is
       half a million rounds of PBKDF2 followed by an AES pass over the whole
       wallet, none of which touches wallet state - keeping sync stopped
       through all of that is what makes a save show up as a stall in the sync
       rate, and during an initial sync we save every ten thousand blocks. */
    const std::string walletJSON =
        m_syncRAIIWrapper->pauseSynchronizerToRunFunction([this]() { return unsafeToJSON(); });

    /* The pause used to serialise saves against each other as a side effect of
       running everything inside it. Two overlapping saves writing the same
       file would interleave their bytes, so keep them ordered explicitly now
       that the write happens outside. */
    std::scoped_lock lock(m_saveMutex);

    return saveWalletJSONToDisk(walletJSON, m_filename, m_password);
}

/* Unsafe because it doesn't lock any data structures - need to stop the
   blockchain synchronizer first (Call save()) */
Error WalletBackend::unsafeSave() const
{
    const std::string walletJSON = unsafeToJSON();

    /* Unsafe refers to the wallet state, which the caller has already stopped
       the synchronizer to take. The file still has to be written one save at a
       time. */
    std::scoped_lock lock(m_saveMutex);

    return WalletBackend::saveWalletJSONToDisk(walletJSON, m_filename, m_password);
}

/* Get the balance for one subwallet (error, unlocked, locked) */
std::tuple<Error, uint64_t, uint64_t> WalletBackend::getBalance(const std::string address) const
{
    /* Verify the address is good, and one of our subwallets */
    if (Error error = validateOurAddresses({address}, m_subWallets); error != SUCCESS)
    {
        return {error, 0, 0};
    }

    const bool takeFromAll = false;

    const auto [unlockedBalance, lockedBalance] = m_subWallets->getBalance(
        Utilities::addressesToSpendKeys({address}), takeFromAll, m_daemon->networkBlockCount());

    return {SUCCESS, unlockedBalance, lockedBalance};
}

/* Gets the combined balance for all wallets in the container */
std::tuple<uint64_t, uint64_t> WalletBackend::getTotalBalance() const
{
    const bool takeFromAll = true;

    /* Get combined balance from every container */
    return m_subWallets->getBalance({}, takeFromAll, m_daemon->networkBlockCount());
}

uint64_t WalletBackend::getTotalUnlockedBalance() const
{
    const auto [unlockedBalance, lockedBalance] = getTotalBalance();

    return unlockedBalance;
}

bool WalletBackend::removePreparedTransaction(const Crypto::Hash &transactionHash)
{
    const bool removed = m_preparedTransactions.erase(transactionHash) == 1;

    std::stringstream stream;

    if (removed)
    {
        stream << "Removed prepared transaction " << transactionHash
               << " as it is no longer valid or has just been sent.";
    }
    else
    {
        stream << "Could not remove prepared transaction: " << transactionHash
               << " as it does not exist in the prepared transaction container.";
    }

    Logger::logger.log(
        stream.str(),
        Logger::INFO,
        { Logger::TRANSACTIONS }
    );

    return removed;
}

bool WalletBackend::deletePreparedTransaction(const Crypto::Hash &transactionHash)
{
    /* removePreparedTransaction() is called from sendPreparedTransaction() with
       m_transactionMutex already held, so it does not lock itself. External
       callers come through here to take the lock. */
    std::scoped_lock lock(m_transactionMutex);

    return removePreparedTransaction(transactionHash);
}

std::tuple<Error, Crypto::Hash> WalletBackend::sendPreparedTransaction(
    const Crypto::Hash transactionHash)
{
    std::scoped_lock lock(m_transactionMutex);

    auto it = m_preparedTransactions.find(transactionHash);

    if (it == m_preparedTransactions.end())
    {
        return {PREPARED_TRANSACTION_NOT_FOUND, Crypto::Hash()};
    }

    const auto preparedTransaction = it->second;

    const auto [error, hash] = SendTransaction::sendPreparedTransaction(
        preparedTransaction,
        m_daemon,
        m_subWallets
    );

    /* Remove the prepared transaction if we just sent it or it's no longer
     * valid */
    if (error == PREPARED_TRANSACTION_EXPIRED || !error)
    {
        removePreparedTransaction(preparedTransaction.transactionHash);
    }

    return {error, hash};
}

/* This is simply a wrapper for Transfer::sendTransactionBasic - we need to
   pass in the daemon and subwallets instance */
std::tuple<Error, Crypto::Hash, WalletTypes::PreparedTransactionInfo> WalletBackend::sendTransactionBasic(
    const std::string destination,
    const uint64_t amount,
    const std::string paymentID,
    const bool sendAll,
    const bool sendTransaction)
{
    std::scoped_lock lock(m_transactionMutex);

    const auto [error, hash, preparedTransaction] = SendTransaction::sendTransactionBasic(
        destination,
        amount,
        paymentID,
        m_daemon,
        m_subWallets,
        sendAll,
        sendTransaction
    );

    if (!sendTransaction && !error)
    {
        m_preparedTransactions[hash] = preparedTransaction;
    }

    return {error, hash, preparedTransaction};
}

std::tuple<Error, Crypto::Hash, WalletTypes::PreparedTransactionInfo> WalletBackend::sendTransactionAdvanced(
    const std::vector<std::pair<std::string, uint64_t>> destinations,
    const uint64_t mixin,
    const WalletTypes::FeeType fee,
    const std::string paymentID,
    const std::vector<std::string> subWalletsToTakeFrom,
    const std::string changeAddress,
    const uint64_t unlockTime,
    const std::vector<uint8_t> extraData,
    const bool sendAll,
    const bool sendTransaction)
{
    std::scoped_lock lock(m_transactionMutex);

    const auto [error, hash, preparedTransaction] = SendTransaction::sendTransactionAdvanced(
        destinations,
        mixin,
        fee,
        paymentID,
        subWalletsToTakeFrom,
        changeAddress,
        m_daemon,
        m_subWallets,
        unlockTime,
        extraData,
        sendAll,
        sendTransaction
    );

    if (!sendTransaction && !error)
    {
        m_preparedTransactions[hash] = preparedTransaction;
    }

    return {error, hash, preparedTransaction};
}

std::vector<std::tuple<Error, Crypto::Hash>> WalletBackend::sweepToAddress(
    const std::string destination,
    const std::string paymentID,
    const uint64_t amountToSweep)
{
    std::scoped_lock lock(m_transactionMutex);

    if (isViewWallet())
    {
        return {{ILLEGAL_VIEW_WALLET_OPERATION, Crypto::Hash()}};
    }

    /* Validate destination — integrated addresses (short or long payment ID) are allowed */
    if (Error err = validateAddresses({destination}, /* allowIntegrated */ true); err != SUCCESS)
    {
        return {{err, Crypto::Hash()}};
    }

    /* Extract the base address and payment ID from an integrated address, mirroring
       the same handling in SendTransaction::sendTransactionAdvanced (Transfer.cpp). */
    std::string resolvedDest = destination;
    std::string resolvedPaymentID = paymentID;
    if (Utilities::isIntegratedAddress(destination))
    {
        auto [baseAddress, embeddedPaymentID] = Utilities::extractIntegratedAddressData(destination);
        resolvedDest = baseAddress;
        if (resolvedPaymentID.empty())
        {
            resolvedPaymentID = embeddedPaymentID;
        }
    }

    /* A short payment ID is encrypted to the receiver, so makeTransaction needs
       their public view key. Every batch below sends to the single resolved
       destination, so there is no ambiguity about who that is. */
    Crypto::PublicKey recipientViewKey = Constants::NULL_PUBLIC_KEY;

    if (resolvedPaymentID.length() == WalletConfig::shortPaymentIDLength)
    {
        std::tie(std::ignore, recipientViewKey) = Utilities::addressToKeys(resolvedDest);
    }

    const uint64_t height = m_daemon->networkBlockCount();
    const auto [minMixin, maxMixin, defaultMixin] = Utilities::getMixinAllowableRange(height);
    const uint64_t mixin = defaultMixin;

    /* Compute unlock time */
    uint64_t unlock_blocks = CryptoNote::parameters::UNLOCK_TIME_TRANSACTION_POOL_WINDOW;
    if (height > CryptoNote::parameters::UNLOCK_TIME_HEIGHT_V2)
    {
        unlock_blocks = CryptoNote::parameters::UNLOCK_TIME_TRANSACTION_POOL_WINDOW_V2;
    }
    const uint64_t unlockTime = height + unlock_blocks + CryptoNote::parameters::MINIMUM_UNLOCK_TIME_BLOCKS;

    const std::string changeAddress = m_subWallets->getPrimaryAddress();

    /* All spendable inputs across all subwallets */
    auto allInputs = m_subWallets->getSpendableTransactionInputs(true, {}, height);

    if (allInputs.empty())
    {
        return {{NOT_ENOUGH_BALANCE, Crypto::Hash()}};
    }

    /* Maximum inputs that fit in one tx (leave room for 2 outputs: dest + change) */
    const size_t maxTxSize = static_cast<size_t>(Utilities::getMaxTxSize(height));
    const size_t maxInputsPerTx = Utilities::getApproximateMaximumInputCount(maxTxSize, 2, mixin);

    if (maxInputsPerTx == 0)
    {
        return {{TOO_MANY_INPUTS_TO_FIT_IN_BLOCK, Crypto::Hash()}};
    }

    /* If sweeping a specific amount, collect only enough inputs to cover it */
    if (amountToSweep > 0)
    {
        /* Worst-case fee per batch (full batch, 1 destination output) */
        const size_t worstCaseSize = Utilities::estimateTransactionSize(mixin, maxInputsPerTx, 1, resolvedPaymentID != "", 0);
        const uint64_t feePerBatch = Utilities::getMinimumTransactionFee(worstCaseSize, height);

        uint64_t sum = 0;
        std::vector<WalletTypes::TxInputAndOwner> needed;

        for (const auto &inp : allInputs)
        {
            needed.push_back(inp);
            sum += inp.input.amount;
            const size_t batchCount = (needed.size() + maxInputsPerTx - 1) / maxInputsPerTx;
            if (sum >= amountToSweep + batchCount * feePerBatch)
            {
                break;
            }
        }

        allInputs = std::move(needed);
    }

    std::vector<std::tuple<Error, Crypto::Hash>> results;

    for (size_t i = 0; i < allInputs.size(); i += maxInputsPerTx)
    {
        const size_t end = std::min(i + maxInputsPerTx, allInputs.size());
        const std::vector<WalletTypes::TxInputAndOwner> batch(allInputs.begin() + i, allInputs.begin() + end);

        uint64_t batchSum = 0;
        for (const auto &inp : batch)
        {
            batchSum += inp.input.amount;
        }

        /* Iteratively estimate fee: the destination amount is decomposed into canonical
           denominations, so the output count depends on the fee, which depends on the
           output count.
           In WASM builds the fee is clamped to TRANSACTION_POW_PASS_WITH_FEE so the
           extremely slow single-threaded PoW is bypassed entirely.  In native builds
           we still add 9 bytes overhead for the PoW nonce that makeTransaction appends. */
#if defined(__EMSCRIPTEN__)
        const size_t POW_NONCE_OVERHEAD = 0;
#else
        const size_t POW_NONCE_OVERHEAD = 9;
#endif
        size_t numOutputs = 1;
        uint64_t estimatedFee = 0;

        for (int iter = 0; iter < 3; iter++)
        {
            const size_t estimatedSize = Utilities::estimateTransactionSize(
                mixin, batch.size(), numOutputs, resolvedPaymentID != "", 0) + POW_NONCE_OVERHEAD;
            estimatedFee = Utilities::getMinimumTransactionFee(estimatedSize, height);
#if defined(__EMSCRIPTEN__)
            /* Ensure fee is high enough to bypass tx PoW in WASM */
            if (estimatedFee < CryptoNote::parameters::TRANSACTION_POW_PASS_WITH_FEE)
            {
                estimatedFee = CryptoNote::parameters::TRANSACTION_POW_PASS_WITH_FEE;
            }
#endif

            if (estimatedFee >= batchSum)
                break;

            size_t newNumOutputs;
            if (amountToSweep > 0)
            {
                /* Specific-amount sweep: destination gets amountToSweep, change returns to self */
                const uint64_t available = batchSum - estimatedFee;
                const uint64_t destAmt   = std::min(amountToSweep, available);
                const uint64_t changeAmt = available > amountToSweep ? available - amountToSweep : 0;
                newNumOutputs =
                    SendTransaction::splitAmountIntoDenominations(destAmt).size() +
                    (changeAmt > 0 ? SendTransaction::splitAmountIntoDenominations(changeAmt).size() : 0);
            }
            else
            {
                /* Sweep-all: entire batch goes to destination, no change */
                newNumOutputs =
                    SendTransaction::splitAmountIntoDenominations(batchSum - estimatedFee).size();
            }

            if (newNumOutputs == numOutputs)
                break;

            numOutputs = newNumOutputs;
        }

        if (estimatedFee >= batchSum)
        {
            /* Fee would consume the entire batch — skip it */
            results.push_back({NOT_ENOUGH_BALANCE, Crypto::Hash()});
            continue;
        }

        /* Compute destination amount and change */
        const uint64_t available = batchSum - estimatedFee;
        const uint64_t destinationAmount = (amountToSweep > 0) ? std::min(amountToSweep, available) : available;
        const uint64_t changeRequired    = (amountToSweep > 0 && available > amountToSweep)
                                               ? available - amountToSweep
                                               : 0;

        auto destinations = SendTransaction::setupDestinations(
            {{resolvedDest, destinationAmount}},
            changeRequired,
            changeAddress
        );

        uint64_t batchMixin = mixin;

        auto txResult = SendTransaction::makeTransaction(
            batchMixin,
            m_daemon,
            batch,
            resolvedPaymentID,
            recipientViewKey,
            destinations,
            m_subWallets,
            unlockTime,
            {} /* extraData */
        );

        /* The same fallback sendTransactionAdvanced() applies, because this path
           builds its transactions itself rather than going through it. Sweep is
           where a thin denomination is most likely to be hit at all: it
           deliberately gathers every denomination the wallet holds, including
           the ones with barely any outputs on chain, so without this one such
           input fails a whole batch.

           The fee and input count for this batch were sized at the requested
           mixin, so a smaller ring only makes the transaction shorter than
           planned. That overpays the fee slightly rather than underpaying it,
           which is the safe direction to be wrong in. */
        for (int attempt = 0; attempt < 2 && txResult.error == NOT_ENOUGH_FAKE_OUTPUTS; attempt++)
        {
            const auto retryMixin = SendTransaction::nextFallbackMixin(
                batchMixin, attempt == 0 ? txResult.achievableMixin : minMixin, minMixin);

            if (!retryMixin)
            {
                break;
            }

            batchMixin = *retryMixin;

            txResult = SendTransaction::makeTransaction(
                batchMixin,
                m_daemon,
                batch,
                resolvedPaymentID,
                recipientViewKey,
                destinations,
                m_subWallets,
                unlockTime,
                {} /* extraData */
            );
        }

        if (txResult.error)
        {
            results.push_back({txResult.error, Crypto::Hash()});
            continue;
        }

        if (batchMixin < mixin)
        {
            Logger::logger.log(
                "Sweep batch built with a ring size of " + std::to_string(batchMixin + 1) + " instead of "
                    + std::to_string(mixin + 1) + ": the denominations in it do not have enough outputs on chain",
                Logger::WARNING,
                { Logger::TRANSACTIONS }
            );
        }

        Error sizeError = SendTransaction::isTransactionPayloadTooBig(txResult.transaction, height);
        if (sizeError)
        {
            results.push_back({sizeError, Crypto::Hash()});
            continue;
        }

        if (!SendTransaction::verifyAmounts(txResult.transaction))
        {
            results.push_back({AMOUNTS_NOT_PRETTY, Crypto::Hash()});
            continue;
        }

        const uint64_t actualFee = SendTransaction::sumTransactionFee(txResult.transaction);

        const auto [sendError, txHash] = SendTransaction::relayTransaction(txResult.transaction, m_daemon);
        if (sendError)
        {
            results.push_back({sendError, Crypto::Hash()});
            continue;
        }

        SendTransaction::storeSentTransaction(
            txHash, actualFee, resolvedPaymentID, batch, changeAddress,
            changeRequired, m_subWallets
        );

        SendTransaction::storeUnconfirmedIncomingInputs(
            m_subWallets, txResult.outputs, txResult.txKeyPair.publicKey, txHash
        );

        m_subWallets->storeTxPrivateKey(txResult.txKeyPair.secretKey, txHash);

        for (const auto &inp : batch)
        {
            m_subWallets->markInputAsLocked(inp.input.keyImage, inp.publicSpendKey);
        }

        results.push_back({SUCCESS, txHash});
    }

    return results;
}

std::tuple<size_t, uint64_t> WalletBackend::estimateSweep(
    const std::string paymentID,
    const uint64_t amountToSweep) const
{
    const uint64_t height = m_daemon->networkBlockCount();
    const auto [minMixin, maxMixin, defaultMixin] = Utilities::getMixinAllowableRange(height);
    const uint64_t mixin = defaultMixin;

    const size_t maxTxSize = static_cast<size_t>(Utilities::getMaxTxSize(height));
    const size_t maxInputsPerTx = Utilities::getApproximateMaximumInputCount(maxTxSize, 2, mixin);

    if (maxInputsPerTx == 0)
    {
        return {0, 0};
    }

    auto allInputs = m_subWallets->getSpendableTransactionInputs(true, {}, height);

    if (allInputs.empty())
    {
        return {0, 0};
    }

    if (amountToSweep > 0)
    {
        const size_t worstCaseSize = Utilities::estimateTransactionSize(mixin, maxInputsPerTx, 1, paymentID != "", 0);
        const uint64_t feePerBatch = Utilities::getMinimumTransactionFee(worstCaseSize, height);

        uint64_t sum = 0;
        std::vector<WalletTypes::TxInputAndOwner> needed;

        for (const auto &inp : allInputs)
        {
            needed.push_back(inp);
            sum += inp.input.amount;
            const size_t batchCount = (needed.size() + maxInputsPerTx - 1) / maxInputsPerTx;
            if (sum >= amountToSweep + batchCount * feePerBatch)
            {
                break;
            }
        }

        allInputs = std::move(needed);
    }

    size_t txCount = 0;
    uint64_t totalFee = 0;

    for (size_t i = 0; i < allInputs.size(); i += maxInputsPerTx)
    {
        const size_t end = std::min(i + maxInputsPerTx, allInputs.size());
        const size_t batchSize = end - i;

        uint64_t batchSum = 0;
        for (size_t j = i; j < end; j++)
        {
            batchSum += allInputs[j].input.amount;
        }

        const size_t POW_NONCE_OVERHEAD = 9;
        size_t numOutputs = 1;
        uint64_t estimatedFee = 0;

        for (int iter = 0; iter < 3; iter++)
        {
            const size_t estimatedSize = Utilities::estimateTransactionSize(
                mixin, batchSize, numOutputs, paymentID != "", 0) + POW_NONCE_OVERHEAD;
            estimatedFee = Utilities::getMinimumTransactionFee(estimatedSize, height);

            if (estimatedFee >= batchSum)
                break;

            size_t newNumOutputs;
            if (amountToSweep > 0)
            {
                const uint64_t available = batchSum - estimatedFee;
                const uint64_t destAmt   = std::min(amountToSweep, available);
                const uint64_t changeAmt = available > amountToSweep ? available - amountToSweep : 0;
                newNumOutputs =
                    SendTransaction::splitAmountIntoDenominations(destAmt).size() +
                    (changeAmt > 0 ? SendTransaction::splitAmountIntoDenominations(changeAmt).size() : 0);
            }
            else
            {
                newNumOutputs =
                    SendTransaction::splitAmountIntoDenominations(batchSum - estimatedFee).size();
            }

            if (newNumOutputs == numOutputs)
                break;

            numOutputs = newNumOutputs;
        }

        if (estimatedFee >= batchSum)
        {
            continue; /* would be skipped in actual sweep */
        }

        txCount++;
        totalFee += estimatedFee;
    }

    return {txCount, totalFee};
}

void WalletBackend::reset(uint64_t scanHeight, uint64_t timestamp)
{
    m_syncRAIIWrapper->pauseSynchronizerToRunFunction([this, scanHeight, timestamp]() mutable {
        /* Though the wallet synchronizer can support both a timestamp and a
           scanheight, we need a fixed scan height to cut transactions from.
           Since a transaction in block 10 could have a timestamp before a
           transaction in block 9, we can't rely on timestamps to reset accurately. */
        if (timestamp != 0)
        {
            scanHeight = Utilities::timestampToScanHeight(timestamp);
            timestamp = 0;
        }

        /* Empty the sync status and reset the start height */
        m_walletSynchronizer->reset(scanHeight);

        /* Reset transactions, inputs, etc */
        m_subWallets->reset(scanHeight);

        /* Save the resetted wallet - don't need safe save, already stopped wallet
           synchronizer */
        unsafeSave();

        return 0;
    });
}

std::tuple<Error, std::string, Crypto::SecretKey, uint64_t> WalletBackend::addSubWallet()
{
    return m_syncRAIIWrapper->pauseSynchronizerToRunFunction([this]() {
        /* Add the sub wallet */
        return m_subWallets->addSubWallet();
    });
}

std::tuple<Error, std::string>
    WalletBackend::importSubWallet(const Crypto::SecretKey privateSpendKey, const uint64_t scanHeight)
{
    if (Error error = validatePrivateKey(privateSpendKey); error != SUCCESS)
    {
        return {error, std::string()};
    }

    return m_syncRAIIWrapper->pauseSynchronizerToRunFunction([&, this]() {
        /* Add the sub wallet */
        const auto [error, address] = m_subWallets->importSubWallet(privateSpendKey, scanHeight);

        if (!error)
        {
            /* If we're not making a new wallet, check if we need to reset the scan
               height of the wallet synchronizer, to pick up the new wallet data
               from the requested height */
            uint64_t currentHeight = m_walletSynchronizer->getCurrentScanHeight();

            if (currentHeight >= scanHeight)
            {
                /* Empty the sync status and reset the start height */
                m_walletSynchronizer->reset(scanHeight);

                /* Reset transactions, inputs, etc */
                m_subWallets->reset(scanHeight);
            }
        }

        return std::make_tuple(error, address);
    });
}

std::tuple<Error, std::string>
    WalletBackend::importSubWallet(const uint64_t walletIndex, const uint64_t scanHeight)
{
    return m_syncRAIIWrapper->pauseSynchronizerToRunFunction([&, this]() {
        /* Add the sub wallet */
        const auto [error, address] = m_subWallets->importSubWallet(walletIndex, scanHeight);

        if (!error)
        {
            /* If we're not making a new wallet, check if we need to reset the scan
               height of the wallet synchronizer, to pick up the new wallet data
               from the requested height */
            uint64_t currentHeight = m_walletSynchronizer->getCurrentScanHeight();

            if (currentHeight >= scanHeight)
            {
                /* Empty the sync status and reset the start height */
                m_walletSynchronizer->reset(scanHeight);

                /* Reset transactions, inputs, etc */
                m_subWallets->reset(scanHeight);
            }
        }

        return std::make_tuple(error, address);
    });
}

std::tuple<Error, std::string>
    WalletBackend::importViewSubWallet(const Crypto::PublicKey publicSpendKey, const uint64_t scanHeight)
{
    if (Error error = validatePublicKey(publicSpendKey); error != SUCCESS)
    {
        return {error, std::string()};
    }

    return m_syncRAIIWrapper->pauseSynchronizerToRunFunction([&, this]() {
        /* Add the sub wallet */
        const auto [error, address] = m_subWallets->importViewSubWallet(publicSpendKey, scanHeight);

        if (!error)
        {
            /* If we're not making a new wallet, check if we need to reset the scan
               height of the wallet synchronizer, to pick up the new wallet data
               from the requested height */
            uint64_t currentHeight = m_walletSynchronizer->getCurrentScanHeight();

            if (currentHeight >= scanHeight)
            {
                /* Empty the sync status and reset the start height */
                m_walletSynchronizer->reset(scanHeight);

                /* Reset transactions, inputs, etc */
                m_subWallets->reset(scanHeight);
            }
        }

        return std::make_tuple(error, address);
    });
}

Error WalletBackend::deleteSubWallet(const std::string address)
{
    const bool allowIntegratedAddresses = false;

    if (Error error = validateAddresses({address}, allowIntegratedAddresses); error != SUCCESS)
    {
        return error;
    }

    return m_syncRAIIWrapper->pauseSynchronizerToRunFunction(
        [&, this]() { return m_subWallets->deleteSubWallet(address); });
}

bool WalletBackend::isViewWallet() const
{
    return m_subWallets->isViewWallet();
}

std::string WalletBackend::getWalletLocation() const
{
    return m_filename;
}

std::string WalletBackend::getPrimaryAddress() const
{
    return m_subWallets->getPrimaryAddress();
}

std::vector<std::string> WalletBackend::getAddresses() const
{
    return m_subWallets->getAddresses();
}

uint64_t WalletBackend::getWalletCount() const
{
    return m_subWallets->getWalletCount();
}

bool WalletBackend::syncStep()
{
    return m_walletSynchronizer->syncStep();
}

std::tuple<uint64_t, uint64_t, uint64_t> WalletBackend::getSyncStatus() const
{
    /* The last block the wallet has synced */
    uint64_t walletBlockCount = m_walletSynchronizer->getCurrentScanHeight();

    /* The last block the daemon has synced */
    uint64_t localDaemonBlockCount = m_daemon->localDaemonBlockCount();

    /* The last block on the network, that the daemon is aware of */
    uint64_t networkBlockCount = m_daemon->networkBlockCount();

    return {walletBlockCount, localDaemonBlockCount, networkBlockCount};
}

std::string WalletBackend::getWalletPassword() const
{
    return m_password;
}

Error WalletBackend::changePassword(const std::string newPassword)
{
    /* Saving is a tad slow because of pbkdf2, might as well take the
       optimization here */
    if (m_password == newPassword)
    {
        return SUCCESS;
    }

    m_password = newPassword;

    return save();
}

std::tuple<Error, Crypto::PublicKey, Crypto::SecretKey, uint64_t> WalletBackend::getSpendKeys(const std::string &address) const
{
    const bool allowIntegratedAddresses = false;

    if (Error error = validateAddresses({address}, allowIntegratedAddresses); error != SUCCESS)
    {
        return {error, Crypto::PublicKey(), Crypto::SecretKey(), 0};
    }

    const auto [publicSpendKey, publicViewKey] = Utilities::addressToKeys(address);

    const auto [success, privateSpendKey, walletIndex] = m_subWallets->getPrivateSpendKey(publicSpendKey);

    return {success, publicSpendKey, privateSpendKey, walletIndex};
}

Crypto::SecretKey WalletBackend::getPrivateViewKey() const
{
    return m_subWallets->getPrivateViewKey();
}

/* Returns the private spend key for the primary address, and the shared private view key */
std::tuple<Crypto::SecretKey, Crypto::SecretKey> WalletBackend::getPrimaryAddressPrivateKeys() const
{
    return {m_subWallets->getPrimaryPrivateSpendKey(), m_subWallets->getPrivateViewKey()};
}

std::tuple<Error, std::string> WalletBackend::getMnemonicSeed() const
{
    return getMnemonicSeedForAddress(getPrimaryAddress());
}

std::tuple<Error, std::string> WalletBackend::getMnemonicSeedForAddress(const std::string &address) const
{
    const bool allowIntegratedAddresses = false;

    if (Error error = validateAddresses({address}, allowIntegratedAddresses); error != SUCCESS)
    {
        return {error, std::string()};
    }

    const auto privateViewKey = getPrivateViewKey();
    const auto [error, publicSpendKey, privateSpendKey, walletIndex] = getSpendKeys(address);

    if (error)
    {
        return {error, std::string()};
    }

    Crypto::SecretKey derivedPrivateViewKey;

    /* Derive the view key from the spend key, and check if it matches the
       actual view key */
    Crypto::crypto_ops::generateViewFromSpend(privateSpendKey, derivedPrivateViewKey);

    if (derivedPrivateViewKey != privateViewKey)
    {
        return {KEYS_NOT_DETERMINISTIC, std::string()};
    }

    return {SUCCESS, Mnemonics::PrivateKeyToMnemonic(privateSpendKey)};
}

std::vector<WalletTypes::Transaction> WalletBackend::getTransactions() const
{
    return m_subWallets->getTransactions();
}

std::vector<WalletTypes::Transaction> WalletBackend::getUnconfirmedTransactions() const
{
    return m_subWallets->getUnconfirmedTransactions();
}

WalletTypes::WalletStatus WalletBackend::getStatus() const
{
    const auto [walletBlockCount, localDaemonBlockCount, networkBlockCount] = getSyncStatus();

    WalletTypes::WalletStatus status;

    status.walletBlockCount = walletBlockCount;
    status.localDaemonBlockCount = localDaemonBlockCount;
    status.networkBlockCount = networkBlockCount;

    status.peerCount = m_daemon->peerCount();
    status.lastKnownHashrate = m_daemon->hashrate();

    return status;
}

/* Returns transactions in the range [startHeight, endHeight - 1] - so if
   we give 1, 100, it will return transactions from block 1 to block 99 */
std::vector<WalletTypes::Transaction>
    WalletBackend::getTransactionsRange(const uint64_t startHeight, const uint64_t endHeight) const
{
    std::vector<WalletTypes::Transaction> result;

    try {
        const auto transactions = getTransactions();

        if (!transactions.empty())
        {
            std::copy_if(
                transactions.begin(),
                transactions.end(),
                std::back_inserter(result),
                [&startHeight, &endHeight](const auto tx) {
                    return tx.blockHeight >= startHeight && tx.blockHeight < endHeight;
                });

            return result;
        } else
        {
            return std::vector<WalletTypes::Transaction> {};
        }
    } catch (const std::exception &e)
    {
    }
}

std::tuple<uint64_t, std::string> WalletBackend::getNodeFee() const
{
    return m_daemon->nodeFee();
}

std::tuple<std::string, uint16_t, bool> WalletBackend::getNodeAddress() const
{
    return m_daemon->nodeAddress();
}

void WalletBackend::swapNode(std::string daemonHost, uint16_t daemonPort, bool daemonSSL)
{
    m_syncRAIIWrapper->pauseSynchronizerToRunFunction([&, this]() {
        /* Swap and init the node */
        m_daemon->swapNode(daemonHost, daemonPort, daemonSSL);

        /* Give the synchronizer the new daemon */
        m_walletSynchronizer->swapNode(m_daemon);

        return 0;
    });
}

bool WalletBackend::daemonOnline() const
{
    return m_daemon->isOnline();
}

std::tuple<Error, std::string> WalletBackend::getAddress(const Crypto::PublicKey spendKey) const
{
    if (Error error = validatePublicKey(spendKey); error != SUCCESS)
    {
        return {error, std::string()};
    }

    return m_subWallets->getAddress(spendKey);
}

std::tuple<Error, Crypto::SecretKey> WalletBackend::getTxPrivateKey(const Crypto::Hash txHash) const
{
    const auto [success, key] = m_subWallets->getTxPrivateKey(txHash);

    if (success)
    {
        return {SUCCESS, key};
    }

    return {TX_PRIVATE_KEY_NOT_FOUND, key};
}

bool WalletBackend::getTransactionsStatus(
    const std::unordered_set<Crypto::Hash> transactionHashes,
    std::unordered_set<Crypto::Hash> &transactionsInPool,
    std::unordered_set<Crypto::Hash> &transactionsInBlock,
    std::unordered_set<Crypto::Hash> &transactionsUnknown) const
{
    return m_daemon->getTransactionsStatus(
        transactionHashes, transactionsInPool, transactionsInBlock, transactionsUnknown);
}

std::vector<std::tuple<std::string, uint64_t, uint64_t>> WalletBackend::getBalances() const
{
    return m_subWallets->getBalances(m_daemon->networkBlockCount());
}

std::string WalletBackend::toJSON() const
{
    return m_syncRAIIWrapper->pauseSynchronizerToRunFunction([this]() { return unsafeToJSON(); });
}

std::string WalletBackend::unsafeToJSON() const
{
    nlohmann::json j;
    j["walletFileFormatVersion"] = Constants::WALLET_FILE_FORMAT_VERSION;
    j["subWallets"] = m_subWallets->toJSON();
    j["walletSynchronizer"] = m_walletSynchronizer->toJSON();
    return j.dump();
}

Error WalletBackend::fromJSON(const nlohmann::json &j)
{
    uint64_t version = getUint64FromJSON(j, "walletFileFormatVersion");

    if (version != Constants::WALLET_FILE_FORMAT_VERSION)
    {
        return UNSUPPORTED_WALLET_FILE_FORMAT_VERSION;
    }

    m_subWallets = std::make_shared<SubWallets>();
    m_subWallets->fromJSON(getObjectFromJSON(j, "subWallets"));

    m_walletSynchronizer = std::make_shared<WalletSynchronizer>();
    m_walletSynchronizer->fromJSON(getObjectFromJSON(j, "walletSynchronizer"));

    return SUCCESS;
}

Error WalletBackend::fromJSON(
    const nlohmann::json &j,
    const std::string filename,
    const std::string password,
    const std::string daemonHost,
    const uint16_t daemonPort,
    const bool daemonSSL,
    const unsigned int syncThreadCount)
{
    if (Error error = fromJSON(j); error != SUCCESS)
    {
        return error;
    }

    m_filename = filename;
    m_password = password;
    m_syncThreadCount = syncThreadCount;

    m_daemon = std::make_shared<Nigel>(daemonHost, daemonPort, daemonSSL);

    init();

    return SUCCESS;
}
