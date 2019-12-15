// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "IFusionManager.h"
#include "WalletIndices.h"
#include "logging/LoggerRef.h"
#include "transfers/BlockchainSynchronizer.h"
#include "transfers/TransfersSynchronizer.h"

#include <queue>
#include <system/Dispatcher.h>
#include <system/Event.h>
#include <unordered_map>

namespace CryptoNote
{
    struct PreparedTransaction
    {
        std::shared_ptr<ITransaction> transaction;
        std::vector<WalletTransfer> destinations;
        uint64_t neededMoney;
        uint64_t changeAmount;
    };

    class WalletGreen :
        ITransfersObserver,
        IBlockchainSynchronizerObserver,
        ITransfersSynchronizerObserver,
        public IFusionManager
    {
      public:
        WalletGreen(
            System::Dispatcher &dispatcher,
            const Currency &currency,
            INode &node,
            std::shared_ptr<Logging::ILogger> logger,
            uint32_t transactionSoftLockTime = 1);

        virtual ~WalletGreen();

        virtual void initializeWithViewKey(
            const std::string &path,
            const std::string &password,
            const Crypto::SecretKey &viewSecretKey,
            const uint64_t scanHeight,
            const bool newAddress);

        virtual void load(const std::string &path, const std::string &password, std::string &extra);

        virtual void load(const std::string &path, const std::string &password);

        virtual void shutdown();

        virtual void changePassword(const std::string &oldPassword, const std::string &newPassword);

        virtual void save(WalletSaveLevel saveLevel = WalletSaveLevel::SAVE_ALL, const std::string &extra = "");

        virtual void reset(const uint64_t scanHeight);

        virtual void exportWallet(
            const std::string &path,
            bool encrypt = true,
            WalletSaveLevel saveLevel = WalletSaveLevel::SAVE_ALL,
            const std::string &extra = "");

        virtual size_t getAddressCount() const;

        virtual std::string getAddress(size_t index) const;

        virtual KeyPair getAddressSpendKey(size_t index) const;

        virtual KeyPair getAddressSpendKey(const std::string &address) const;

        virtual KeyPair getViewKey() const;

        virtual std::string createAddress();

        virtual std::string
            createAddress(const Crypto::SecretKey &spendSecretKey, const uint64_t scanHeight, const bool newAddress);

        virtual std::string
            createAddress(const Crypto::PublicKey &spendPublicKey, const uint64_t scanHeight, const bool newAddress);

        virtual std::vector<std::string> createAddressList(
            const std::vector<Crypto::SecretKey> &spendSecretKeys,
            const uint64_t scanHeight,
            const bool newAddress);

        virtual void deleteAddress(const std::string &address);

        virtual uint64_t getActualBalance() const;

        virtual uint64_t getActualBalance(const std::string &address) const;

        virtual uint64_t getPendingBalance() const;

        virtual uint64_t getPendingBalance(const std::string &address) const;

        virtual size_t getTransactionCount() const;

        virtual WalletTransaction getTransaction(size_t transactionIndex) const;

        virtual WalletTransactionWithTransfers getTransaction(const Crypto::Hash &transactionHash) const;

        virtual std::vector<TransactionsInBlockInfo> getTransactions(const Crypto::Hash &blockHash, size_t count) const;

        virtual std::vector<TransactionsInBlockInfo> getTransactions(uint32_t blockIndex, size_t count) const;

        virtual std::vector<Crypto::Hash> getBlockHashes(uint32_t blockIndex, size_t count) const;

        virtual uint32_t getBlockCount() const;

        virtual std::vector<WalletTransactionWithTransfers> getUnconfirmedTransactions() const;

        virtual std::vector<size_t> getDelayedTransactionIds() const;

        virtual size_t transfer(const TransactionParameters &transactionParameters);

        virtual size_t makeTransaction(const TransactionParameters &sendingTransaction);

        virtual void commitTransaction(size_t);

        virtual void rollbackUncommitedTransaction(size_t);

        size_t transfer(const PreparedTransaction &preparedTransaction);

        bool txIsTooLarge(const PreparedTransaction &p);

        size_t getTxSize(const PreparedTransaction &p);

        size_t getMaxTxSize();

        PreparedTransaction formTransaction(const TransactionParameters &sendingTransaction);

        void updateInternalCache();

        void clearCaches(bool clearTransactions, bool clearCachedData);

        void createViewWallet(
            const std::string &path,
            const std::string &password,
            const std::string address,
            const Crypto::SecretKey &viewSecretKey,
            const uint64_t scanHeight,
            const bool newAddress);

        uint64_t getBalanceMinusDust(const std::vector<std::string> &addresses);

        virtual void start();

        virtual void stop();

        virtual WalletEvent getEvent();

        virtual size_t createFusionTransaction(
            uint64_t threshold,
            uint16_t mixin,
            const std::vector<std::string> &sourceAddresses = {},
            const std::string &destinationAddress = "") override;

        virtual bool isFusionTransaction(size_t transactionId) const override;

        virtual IFusionManager::EstimateResult
            estimate(uint64_t threshold, const std::vector<std::string> &sourceAddresses = {}) const override;

        std::string toNewFormatJSON() const;

      protected:
        struct NewAddressData
        {
            Crypto::PublicKey spendPublicKey;
            Crypto::SecretKey spendSecretKey;
        };

        void throwIfNotInitialized() const;

        void throwIfStopped() const;

        void throwIfTrackingMode() const;

        void doShutdown();

        void convertAndLoadWalletFile(const std::string &path, std::ifstream &&walletFileStream);

        static void decryptKeyPair(
            const EncryptedWalletRecord &cipher,
            Crypto::PublicKey &publicKey,
            Crypto::SecretKey &secretKey,
            uint64_t &creationTimestamp,
            const Crypto::chacha8_key &key);

        void decryptKeyPair(
            const EncryptedWalletRecord &cipher,
            Crypto::PublicKey &publicKey,
            Crypto::SecretKey &secretKey,
            uint64_t &creationTimestamp) const;

        static EncryptedWalletRecord encryptKeyPair(
            const Crypto::PublicKey &publicKey,
            const Crypto::SecretKey &secretKey,
            uint64_t creationTimestamp,
            const Crypto::chacha8_key &key,
            const Crypto::chacha8_iv &iv);

        EncryptedWalletRecord encryptKeyPair(
            const Crypto::PublicKey &publicKey,
            const Crypto::SecretKey &secretKey,
            uint64_t creationTimestamp) const;

        Crypto::chacha8_iv getNextIv() const;

        static void incIv(Crypto::chacha8_iv &iv);

        void incNextIv();

        void initWithKeys(
            const std::string &path,
            const std::string &password,
            const Crypto::PublicKey &viewPublicKey,
            const Crypto::SecretKey &viewSecretKey,
            const uint64_t scanHeight,
            const bool newAddress);

        std::string doCreateAddress(
            const Crypto::PublicKey &spendPublicKey,
            const Crypto::SecretKey &spendSecretKey,
            const uint64_t scanHeight,
            const bool newAddress);

        std::vector<std::string> doCreateAddressList(
            const std::vector<NewAddressData> &addressDataList,
            const uint64_t scanHeight,
            const bool newAddress);

        CryptoNote::BlockDetails getBlock(const uint64_t blockHeight);

        uint64_t scanHeightToTimestamp(const uint64_t scanHeight);

        uint64_t getCurrentTimestampAdjusted();

        struct InputInfo
        {
            TransactionTypes::InputKeyInfo keyInfo;
            WalletRecord *walletRecord = nullptr;
            KeyPair ephKeys;
        };

        struct OutputToTransfer
        {
            TransactionOutputInformation out;
            WalletRecord *wallet;
        };

        struct ReceiverAmounts
        {
            CryptoNote::AccountPublicAddress receiver;
            std::vector<uint64_t> amounts;
        };

        struct WalletOuts
        {
            WalletRecord *wallet;
            std::vector<TransactionOutputInformation> outs;
        };

        typedef std::pair<WalletTransfers::const_iterator, WalletTransfers::const_iterator> TransfersRange;

        struct AddressAmounts
        {
            int64_t input = 0;
            int64_t output = 0;
        };

        struct ContainerAmounts
        {
            ITransfersContainer *container;
            AddressAmounts amounts;
        };

#pragma pack(push, 1)
        struct ContainerStoragePrefix
        {
            uint8_t version;
            Crypto::chacha8_iv nextIv;
            EncryptedWalletRecord encryptedViewKeys;
        };
#pragma pack(pop)

        typedef std::unordered_map<std::string, AddressAmounts> TransfersMap;

        virtual void onError(ITransfersSubscription *object, uint32_t height, std::error_code ec) override;

        virtual void onTransactionUpdated(ITransfersSubscription *object, const Crypto::Hash &transactionHash) override;

        virtual void onTransactionUpdated(
            const Crypto::PublicKey &viewPublicKey,
            const Crypto::Hash &transactionHash,
            const std::vector<ITransfersContainer *> &containers) override;

        void transactionUpdated(
            const TransactionInformation &transactionInfo,
            const std::vector<ContainerAmounts> &containerAmountsList);

        virtual void onTransactionDeleted(ITransfersSubscription *object, const Crypto::Hash &transactionHash) override;

        void transactionDeleted(ITransfersSubscription *object, const Crypto::Hash &transactionHash);

        virtual void synchronizationProgressUpdated(uint32_t processedBlockCount, uint32_t totalBlockCount) override;

        virtual void synchronizationCompleted(std::error_code result) override;

        void onSynchronizationProgressUpdated(uint32_t processedBlockCount, uint32_t totalBlockCount);

        void onSynchronizationCompleted();

        virtual void onBlocksAdded(const Crypto::PublicKey &viewPublicKey, const std::vector<Crypto::Hash> &blockHashes)
            override;

        void blocksAdded(const std::vector<Crypto::Hash> &blockHashes);

        virtual void onBlockchainDetach(const Crypto::PublicKey &viewPublicKey, uint32_t blockIndex) override;

        void blocksRollback(uint32_t blockIndex);

        virtual void
            onTransactionDeleteBegin(const Crypto::PublicKey &viewPublicKey, Crypto::Hash transactionHash) override;

        void transactionDeleteBegin(Crypto::Hash transactionHash);

        virtual void
            onTransactionDeleteEnd(const Crypto::PublicKey &viewPublicKey, Crypto::Hash transactionHash) override;

        void transactionDeleteEnd(Crypto::Hash transactionHash);

        std::vector<WalletOuts> pickWalletsWithMoney() const;

        WalletOuts pickWallet(const std::string &address) const;

        std::vector<WalletOuts> pickWallets(const std::vector<std::string> &addresses) const;

        void updateBalance(CryptoNote::ITransfersContainer *container);

        void unlockBalances(uint32_t height);

        const WalletRecord &getWalletRecord(const Crypto::PublicKey &key) const;

        const WalletRecord &getWalletRecord(const std::string &address) const;

        const WalletRecord &getWalletRecord(CryptoNote::ITransfersContainer *container) const;

        CryptoNote::AccountPublicAddress parseAddress(const std::string &address) const;

        std::string addWallet(const NewAddressData &addressData, uint64_t scanHeight, bool newAddress);

        AccountKeys makeAccountKeys(const WalletRecord &wallet) const;

        size_t getTransactionId(const Crypto::Hash &transactionHash) const;

        void pushEvent(const WalletEvent &event);

        bool isFusionTransaction(const WalletTransaction &walletTx) const;

        void prepareTransaction(
            std::vector<WalletOuts> &&wallets,
            const std::vector<WalletOrder> &orders,
            WalletTypes::FeeType fee,
            uint16_t mixIn,
            const std::string &extra,
            uint64_t unlockTimestamp,
            const DonationSettings &donation,
            const CryptoNote::AccountPublicAddress &changeDestinationAddress,
            PreparedTransaction &preparedTransaction);

        size_t doTransfer(const TransactionParameters &transactionParameters);

        void checkIfEnoughMixins(std::vector<CryptoNote::RandomOuts> &mixinResult, uint16_t mixIn) const;

        std::vector<WalletTransfer> convertOrdersToTransfers(const std::vector<WalletOrder> &orders) const;

        uint64_t countNeededMoney(const std::vector<CryptoNote::WalletTransfer> &destinations, uint64_t fee) const;

        CryptoNote::AccountPublicAddress parseAccountAddressString(const std::string &addressString) const;

        uint64_t pushDonationTransferIfPossible(
            const DonationSettings &donation,
            uint64_t freeAmount,
            uint64_t dustThreshold,
            std::vector<WalletTransfer> &destinations) const;

        void validateAddresses(const std::vector<std::string> &addresses) const;

        void validateOrders(const std::vector<WalletOrder> &orders) const;

        void validateChangeDestination(
            const std::vector<std::string> &sourceAddresses,
            const std::string &changeDestination,
            bool isFusion) const;

        void validateSourceAddresses(const std::vector<std::string> &sourceAddresses) const;

        void validateTransactionParameters(const TransactionParameters &transactionParameters) const;

        void requestMixinOuts(
            const std::vector<OutputToTransfer> &selectedTransfers,
            uint16_t mixIn,
            std::vector<CryptoNote::RandomOuts> &mixinResult);

        void prepareInputs(
            const std::vector<OutputToTransfer> &selectedTransfers,
            std::vector<CryptoNote::RandomOuts> &mixinResult,
            uint16_t mixIn,
            std::vector<InputInfo> &keysInfo);

        uint64_t selectTransfers(
            uint64_t needeMoney,
            bool dust,
            uint64_t dustThreshold,
            std::vector<WalletOuts> &&wallets,
            std::vector<OutputToTransfer> &selectedTransfers);

        std::vector<ReceiverAmounts> splitDestinations(
            const std::vector<WalletTransfer> &destinations,
            uint64_t dustThreshold,
            const Currency &currency);

        ReceiverAmounts splitAmount(uint64_t amount, const AccountPublicAddress &destination, uint64_t dustThreshold);

        std::unique_ptr<CryptoNote::ITransaction> makeTransaction(
            const std::vector<ReceiverAmounts> &decomposedOutputs,
            std::vector<InputInfo> &keysInfo,
            const std::string &extra,
            uint64_t unlockTimestamp);

        void sendTransaction(const CryptoNote::Transaction &cryptoNoteTransaction);

        size_t validateSaveAndSendTransaction(
            const ITransactionReader &transaction,
            const std::vector<WalletTransfer> &destinations,
            bool isFusion,
            bool send);

        size_t insertBlockchainTransaction(const TransactionInformation &info, int64_t txBalance);

        size_t insertOutgoingTransactionAndPushEvent(
            const Crypto::Hash &transactionHash,
            uint64_t fee,
            const BinaryArray &extra,
            uint64_t unlockTimestamp);

        void updateTransactionStateAndPushEvent(size_t transactionId, WalletTransactionState state);

        bool updateWalletTransactionInfo(
            size_t transactionId,
            const CryptoNote::TransactionInformation &info,
            int64_t totalAmount);

        bool updateTransactionTransfers(
            size_t transactionId,
            const std::vector<ContainerAmounts> &containerAmountsList,
            int64_t allInputsAmount,
            int64_t allOutputsAmount);

        TransfersMap getKnownTransfersMap(size_t transactionId, size_t firstTransferIdx) const;

        bool updateAddressTransfers(
            size_t transactionId,
            size_t firstTransferIdx,
            const std::string &address,
            int64_t knownAmount,
            int64_t targetAmount);

        bool updateUnknownTransfers(
            size_t transactionId,
            size_t firstTransferIdx,
            const std::unordered_set<std::string> &myAddresses,
            int64_t knownAmount,
            int64_t myAmount,
            int64_t totalAmount,
            bool isOutput);

        void appendTransfer(size_t transactionId, size_t firstTransferIdx, const std::string &address, int64_t amount);

        bool adjustTransfer(size_t transactionId, size_t firstTransferIdx, const std::string &address, int64_t amount);

        bool eraseTransfers(
            size_t transactionId,
            size_t firstTransferIdx,
            std::function<bool(bool, const std::string &)> &&predicate);

        bool eraseTransfersByAddress(
            size_t transactionId,
            size_t firstTransferIdx,
            const std::string &address,
            bool eraseOutputTransfers);

        bool eraseForeignTransfers(
            size_t transactionId,
            size_t firstTransferIdx,
            const std::unordered_set<std::string> &knownAddresses,
            bool eraseOutputTransfers);

        void pushBackOutgoingTransfers(size_t txId, const std::vector<WalletTransfer> &destinations);

        void insertUnlockTransactionJob(
            const Crypto::Hash &transactionHash,
            uint32_t blockHeight,
            CryptoNote::ITransfersContainer *container);

        void deleteUnlockTransactionJob(const Crypto::Hash &transactionHash);

        void startBlockchainSynchronizer();

        void stopBlockchainSynchronizer();

        void addUnconfirmedTransaction(const ITransactionReader &transaction);

        void removeUnconfirmedTransaction(const Crypto::Hash &transactionHash);

        void copyContainerStorageKeys(
            ContainerStorage &src,
            const Crypto::chacha8_key &srcKey,
            ContainerStorage &dst,
            const Crypto::chacha8_key &dstKey);

        static void copyContainerStoragePrefix(
            ContainerStorage &src,
            const Crypto::chacha8_key &srcKey,
            ContainerStorage &dst,
            const Crypto::chacha8_key &dstKey);

        void deleteOrphanTransactions(const std::unordered_set<Crypto::PublicKey> &deletedKeys);

        static void encryptAndSaveContainerData(
            ContainerStorage &storage,
            const Crypto::chacha8_key &key,
            const void *containerData,
            size_t containerDataSize);

        static void loadAndDecryptContainerData(
            ContainerStorage &storage,
            const Crypto::chacha8_key &key,
            BinaryArray &containerData);

        void initTransactionPool();

        void loadSpendKeys();

        void loadContainerStorage(const std::string &path);

        void loadWalletCache(
            std::unordered_set<Crypto::PublicKey> &addedKeys,
            std::unordered_set<Crypto::PublicKey> &deletedKeys,
            std::string &extra);

        void saveWalletCache(
            ContainerStorage &storage,
            const Crypto::chacha8_key &key,
            WalletSaveLevel saveLevel,
            const std::string &extra);

        void subscribeWallets();

        std::vector<OutputToTransfer> pickRandomFusionInputs(
            const std::vector<std::string> &addresses,
            uint64_t threshold,
            size_t minInputCount,
            size_t maxInputCount);

        static ReceiverAmounts decomposeFusionOutputs(const AccountPublicAddress &address, uint64_t inputsAmount);

        enum class WalletState
        {
            INITIALIZED,
            NOT_INITIALIZED
        };

        enum class WalletTrackingMode
        {
            TRACKING,
            NOT_TRACKING,
            NO_ADDRESSES
        };

        WalletTrackingMode getTrackingMode() const;

        TransfersRange getTransactionTransfersRange(size_t transactionIndex) const;

        std::vector<TransactionsInBlockInfo> getTransactionsInBlocks(uint32_t blockIndex, size_t count) const;

        Crypto::Hash getBlockHashByIndex(uint32_t blockIndex) const;

        std::vector<WalletTransfer> getTransactionTransfers(const WalletTransaction &transaction) const;

        void filterOutTransactions(
            WalletTransactions &transactions,
            WalletTransfers &transfers,
            std::function<bool(const WalletTransaction &)> &&pred) const;

        void initBlockchain(const Crypto::PublicKey &viewPublicKey);

        CryptoNote::AccountPublicAddress getChangeDestination(
            const std::string &changeDestinationAddress,
            const std::vector<std::string> &sourceAddresses) const;

        bool isMyAddress(const std::string &address) const;

        void deleteContainerFromUnlockTransactionJobs(const ITransfersContainer *container);

        std::vector<size_t>
            deleteTransfersForAddress(const std::string &address, std::vector<size_t> &deletedTransactions);

        void deleteFromUncommitedTransactions(const std::vector<size_t> &deletedTransactions);

        /****************************************************************************/
        /* These set of functions are used to assist in upgrading the wallet format */

        uint64_t getMinTimestamp() const;

        std::vector<Crypto::PublicKey> getPublicSpendKeys() const;

        std::string getPrimaryAddress() const;

        std::vector<std::tuple<WalletTypes::TransactionInput, Crypto::Hash>>
            getInputs(const WalletRecord subWallet, const bool isViewWallet, const bool unspent) const;

        Crypto::KeyImage getKeyImage(
            const Crypto::PublicKey transactionPublicKey,
            const uint64_t outputIndex,
            const Crypto::PublicKey key,
            const Crypto::SecretKey privateSpendKey,
            const Crypto::PublicKey publicSpendKey) const;

        /****************************************************************************/

        System::Dispatcher &m_dispatcher;

        const Currency &m_currency;

        INode &m_node;

        mutable Logging::LoggerRef m_logger;

        bool m_stopped;

        WalletsContainer m_walletsContainer;

        ContainerStorage m_containerStorage;

        UnlockTransactionJobs m_unlockTransactionsJob;

        WalletTransactions m_transactions;

        WalletTransfers m_transfers; // sorted
        mutable std::unordered_map<size_t, bool> m_fusionTxsCache; // txIndex -> isFusion
        UncommitedTransactions m_uncommitedTransactions;

        bool m_blockchainSynchronizerStarted;

        BlockchainSynchronizer m_blockchainSynchronizer;

        TransfersSyncronizer m_synchronizer;

        System::Event m_eventOccurred;

        std::queue<WalletEvent> m_events;

        mutable System::Event m_readyEvent;

        WalletState m_state;

        std::string m_password;

        Crypto::chacha8_key m_key;

        std::string m_path;

        std::string m_extra; // workaround for wallet reset

        Crypto::PublicKey m_viewPublicKey;

        Crypto::SecretKey m_viewSecretKey;

        uint64_t m_actualBalance;

        uint64_t m_pendingBalance;

        uint32_t m_transactionSoftLockTime;

        BlockHashesContainer m_blockchain;

        friend std::ostream &operator<<(std::ostream &os, CryptoNote::WalletGreen::WalletState state);

        friend std::ostream &operator<<(std::ostream &os, CryptoNote::WalletGreen::WalletTrackingMode mode);

        friend class TransferListFormatter;
    };

} // namespace CryptoNote
