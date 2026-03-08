// Portions Copyright (c) 2018-2019, The Catalyst Developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

///////////////////////////////////////////////
#include <zedwallet++/CommandImplementations.h>
///////////////////////////////////////////////

#include <arpa/inet.h>
#include <config/CryptoNoteConfig.h>
#include <config/WalletConfig.h>
#include <crypto/crypto.h>
#include <crypto/hash.h>
#include <crypto/random.h>
#include <common/StringTools.h>
#include <array>
#include <errors/ValidateParameters.h>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <logger/Logger.h>
#include <utilities/Addresses.h>
#include <utilities/ColouredMsg.h>
#include <utilities/FormatTools.h>
#include <utilities/Input.h>
#include <utilities/Mixins.h>
#include <utilities/String.h>
#include <zedwallet++/Commands.h>
#include <zedwallet++/GetInput.h>
#include <zedwallet++/Menu.h>
#include <zedwallet++/Open.h>
#include <zedwallet++/Sync.h>
#include <zedwallet++/Utilities.h>

void changePassword(const std::shared_ptr<WalletBackend> walletBackend)
{
    /* Check the user knows the current password */
    ZedUtilities::confirmPassword(walletBackend, "Confirm your current password: ");

    /* Get a new password for the wallet */
    const std::string newPassword = getWalletPassword(true, "Enter your new password: ");

    /* Change the wallet password */
    Error error = walletBackend->changePassword(newPassword);

    if (error)
    {
        std::cout << WarningMsg("Your password has been changed, but saving "
                                "the updated wallet failed. If you quit without "
                                "saving succeeding, your password may not "
                                "update.")
                  << std::endl;
    }
    else
    {
        std::cout << SuccessMsg("Your password has been changed!") << std::endl;
    }
}

void backup(const std::shared_ptr<WalletBackend> walletBackend)
{
    ZedUtilities::confirmPassword(walletBackend, "Confirm your current password: ");
    printPrivateKeys(walletBackend);
}

void printPrivateKeys(const std::shared_ptr<WalletBackend> walletBackend)
{
    const auto [privateSpendKey, privateViewKey] = walletBackend->getPrimaryAddressPrivateKeys();

    const auto [error, mnemonicSeed] = walletBackend->getMnemonicSeed();

    /* If this isn't a view only wallet, print out the spend key and mnemonic if available */
    if (!walletBackend->isViewWallet())
    {
        std::cout << SuccessMsg("\nPrivate spend key:\n") << SuccessMsg(privateSpendKey) << "\n";
    }

    std::cout << SuccessMsg("Private view key:\n") << SuccessMsg(privateViewKey) << "\n";

    if (!error)
    {
        std::cout << SuccessMsg("\nMnemonic seed:\n") << SuccessMsg(mnemonicSeed) << "\n";
    }
}

void balance(const std::shared_ptr<WalletBackend> walletBackend)
{
    auto [unlockedBalance, lockedBalance] = walletBackend->getTotalBalance();

    /* We can make a better approximation of the view wallet balance if we
       ignore fusion transactions.
       See https://github.com/turtlecoin/turtlecoin/issues/531 */
    if (walletBackend->isViewWallet())
    {
        unlockedBalance = 0;

        const auto transactions = walletBackend->getTransactions();

        for (const auto &tx : transactions)
        {
            if (!tx.isFusionTransaction())
            {
                unlockedBalance += tx.totalAmount();
            }
        }
    }

    const uint64_t totalBalance = unlockedBalance + lockedBalance;

    std::cout << "Available balance: " << SuccessMsg(Utilities::formatAmount(unlockedBalance)) << "\n"
              << "Locked (unconfirmed) balance: " << WarningMsg(Utilities::formatAmount(lockedBalance))
              << "\nTotal balance: " << InformationMsg(Utilities::formatAmount(totalBalance)) << "\n";

    if (walletBackend->isViewWallet())
    {
        std::cout << InformationMsg("\nPlease note that view only wallets "
                                    "can only track incoming transactions,\n")
                  << InformationMsg("and so your wallet balance may appear "
                                    "inflated.\n");
    }

    const auto [walletBlockCount, localDaemonBlockCount, networkBlockCount] = walletBackend->getSyncStatus();

    if (localDaemonBlockCount < networkBlockCount)
    {
        std::cout << InformationMsg("\nYour daemon is not fully synced with "
                                    "the network!\n")
                  << "Your balance may be incorrect until you are fully "
                  << "synced!\n";
    }
    /* Small buffer because wallet height doesn't update instantly like node
       height does */
    else if (walletBlockCount + 1000 < networkBlockCount)
    {
        std::cout << InformationMsg("\nThe blockchain is still being scanned for "
                                    "your transactions.\n")
                  << "Balances might be incorrect whilst this is ongoing.\n";
    }
}

void printHeights(
    const uint64_t localDaemonBlockCount,
    const uint64_t networkBlockCount,
    const uint64_t walletBlockCount)
{
    /* This is the height that the wallet has been scanned to. The blockchain
       can be fully updated, but we have to walk the chain to find our
       transactions, and this number indicates that progress. */
    std::cout << "Wallet blockchain height: ";

    /* Small buffer because wallet height doesn't update instantly like node
       height does */
    if (walletBlockCount + 1000 > networkBlockCount)
    {
        std::cout << SuccessMsg(walletBlockCount);
    }
    else
    {
        std::cout << WarningMsg(walletBlockCount);
    }

    std::cout << "\nLocal blockchain height: ";

    if (localDaemonBlockCount == networkBlockCount)
    {
        std::cout << SuccessMsg(localDaemonBlockCount);
    }
    else
    {
        std::cout << WarningMsg(localDaemonBlockCount);
    }

    std::cout << "\nNetwork blockchain height: " << SuccessMsg(networkBlockCount) << "\n";
}

void printSyncStatus(
    const uint64_t localDaemonBlockCount,
    const uint64_t networkBlockCount,
    const uint64_t walletBlockCount)
{
    std::string networkSyncPercentage = Utilities::get_sync_percentage(localDaemonBlockCount, networkBlockCount) + "%";

    std::string walletSyncPercentage = Utilities::get_sync_percentage(walletBlockCount, networkBlockCount) + "%";

    std::cout << "Network sync status: ";

    if (localDaemonBlockCount == networkBlockCount)
    {
        std::cout << SuccessMsg(networkSyncPercentage) << std::endl;
    }
    else
    {
        std::cout << WarningMsg(networkSyncPercentage) << std::endl;
    }

    std::cout << "Wallet sync status: ";

    /* Small buffer because wallet height is not always completely accurate */
    if (walletBlockCount + 10 > networkBlockCount)
    {
        std::cout << SuccessMsg(walletSyncPercentage) << std::endl;
    }
    else
    {
        std::cout << WarningMsg(walletSyncPercentage) << std::endl;
    }
}

void printSyncSummary(
    const uint64_t localDaemonBlockCount,
    const uint64_t networkBlockCount,
    const uint64_t walletBlockCount)
{
    if (localDaemonBlockCount == 0 && networkBlockCount == 0)
    {
        std::cout << WarningMsg("Uh oh, it looks like you don't have ") << WarningMsg(WalletConfig::daemonName)
                  << WarningMsg(" open!") << std::endl;
    }
    else if (walletBlockCount + 1000 < networkBlockCount && localDaemonBlockCount == networkBlockCount)
    {
        std::cout << InformationMsg("You are synced with the network, but the "
                                    "blockchain is still being scanned for "
                                    "your transactions.")
                  << std::endl
                  << "Balances might be incorrect whilst this is ongoing." << std::endl;
    }
    else if (localDaemonBlockCount == networkBlockCount)
    {
        std::cout << SuccessMsg("Yay! You are synced!") << std::endl;
    }
    else
    {
        std::cout << WarningMsg("Be patient, you are still syncing with the "
                                "network!")
                  << std::endl;
    }
}

void printHashrate(const uint64_t hashrate)
{
    /* Offline node / not responding */
    if (hashrate == 0)
    {
        return;
    }

    std::cout << "Network hashrate: " << SuccessMsg(Utilities::get_mining_speed(hashrate))
              << " (Based on the last local block)" << std::endl;
}

void status(const std::shared_ptr<WalletBackend> walletBackend)
{
    const WalletTypes::WalletStatus status = walletBackend->getStatus();

    /* Print the heights of local, remote, and wallet */
    printHeights(status.localDaemonBlockCount, status.networkBlockCount, status.walletBlockCount);

    std::cout << "\n";

    /* Print the network and wallet sync status in percentage */
    printSyncStatus(status.localDaemonBlockCount, status.networkBlockCount, status.walletBlockCount);

    std::cout << "\n";

    /* Print the network hashrate, based on the last local block */
    printHashrate(status.lastKnownHashrate);

    /* Print the amount of peers we have */
    std::cout << "Peers: " << SuccessMsg(status.peerCount) << "\n\n";

    /* Print a summary of the sync status */
    printSyncSummary(status.localDaemonBlockCount, status.networkBlockCount, status.walletBlockCount);
}

void reset(const std::shared_ptr<WalletBackend> walletBackend)
{
    const uint64_t scanHeight = ZedUtilities::getScanHeight();

    std::cout << std::endl
              << InformationMsg("This process may take some time to complete.") << std::endl
              << InformationMsg("You can't make any transactions during the ") << InformationMsg("process.")
              << std::endl
              << std::endl;

    if (!Utilities::confirm("Are you sure?"))
    {
        return;
    }

    std::cout << InformationMsg("Resetting wallet...") << std::endl;

    const uint64_t timestamp = 0;

    /* Don't want to queue up transaction events, since sync wallet will print
       them out */
    walletBackend->m_eventHandler->onTransaction.pause();

    walletBackend->reset(scanHeight, timestamp);

    syncWallet(walletBackend);

    /* Readd the event handler for new events */
    walletBackend->m_eventHandler->onTransaction.resume();
}

void refresh(const std::shared_ptr<WalletBackend> walletBackend)
{
    walletBackend->m_eventHandler->onTransaction.pause();
    syncWallet(walletBackend);
    walletBackend->m_eventHandler->onTransaction.resume();
}

void saveCSV(const std::shared_ptr<WalletBackend> walletBackend)
{
    const auto transactions = walletBackend->getTransactions();

    if (transactions.empty())
    {
        std::cout << WarningMsg("You have no transactions to save to the CSV!\n");
        return;
    }

    std::ofstream csv(WalletConfig::csvFilename);

    if (!csv)
    {
        std::cout << WarningMsg("Couldn't open transactions.csv file for "
                                "saving!")
                  << std::endl
                  << WarningMsg("Ensure it is not open in any other "
                                "application.")
                  << std::endl;
        return;
    }

    std::cout << InformationMsg("Saving CSV file...") << std::endl;

    /* Create CSV header */
    csv << "Timestamp,Block Height,Hash,Amount,In/Out" << std::endl;

    for (const auto &tx : transactions)
    {
        /* Ignore fusion transactions */
        if (tx.isFusionTransaction())
        {
            continue;
        }

        const std::string amount = Utilities::formatAmountBasic(std::abs(tx.totalAmount()));

        const std::string direction = tx.totalAmount() > 0 ? "IN" : "OUT";

        csv << Utilities::unixTimeToDate(tx.timestamp) << "," /* Timestamp */
            << tx.blockHeight << "," /* Block Height */
            << tx.hash << "," /* Hash */
            << amount << "," /* Amount */
            << direction /* In/Out */
            << std::endl;
    }

    std::cout << SuccessMsg("CSV successfully written to ") << SuccessMsg(WalletConfig::csvFilename) << SuccessMsg("!")
              << std::endl;
}

void printOutgoingTransfer(const WalletTypes::Transaction tx)
{
    std::stringstream stream;

    const int64_t amount = std::abs(tx.totalAmount());

    stream << "Outgoing transfer:\nHash: " << tx.hash << "\n";

    /* These will not be initialized for outgoing, unconfirmed transactions */
    if (tx.blockHeight != 0 && tx.timestamp != 0)
    {
        stream << "Block height: " << tx.blockHeight << "\n"
               << "Timestamp: " << Utilities::unixTimeToDate(tx.timestamp) << "\n";
    }

    stream << "Spent: " << Utilities::formatAmount(amount - tx.fee) << "\n"
           << "Fee: " << Utilities::formatAmount(tx.fee) << "\n"
           << "Total Spent: " << Utilities::formatAmount(amount) << "\n";

    if (tx.paymentID != "")
    {
        stream << "Payment ID: " << tx.paymentID << "\n";
    }

    std::cout << WarningMsg(stream.str()) << std::endl;
}

void printIncomingTransfer(const WalletTypes::Transaction tx)
{
    std::stringstream stream;

    const int64_t amount = tx.totalAmount();

    stream << "Incoming transfer:\nHash: " << tx.hash << "\n"
           << "Block height: " << tx.blockHeight << "\n"
           << "Timestamp: " << Utilities::unixTimeToDate(tx.timestamp) << "\n"
           << "Amount: " << Utilities::formatAmount(amount) << "\n";

    if (tx.paymentID != "")
    {
        stream << "Payment ID: " << tx.paymentID << "\n";
    }

    /* Display Unlock time, if applicable; otherwise, don't */
    int64_t difference = tx.unlockTime - tx.blockHeight;

    /* Here we treat Unlock as a block, and treat it that way in the future */
    if (tx.unlockTime != 0 && difference > 0 && tx.unlockTime < CryptoNote::parameters::CRYPTONOTE_MAX_BLOCK_NUMBER)
    {
        int64_t unlockInUnixTime = tx.timestamp + (difference * CryptoNote::parameters::DIFFICULTY_TARGET);

        std::cout << SuccessMsg(stream.str()) << InformationMsg("Unlock height: ") << InformationMsg(tx.unlockTime)
                  << std::endl
                  << InformationMsg("Unlocks at approximately: ")
                  << InformationMsg(Utilities::unixTimeToDate(unlockInUnixTime)) << std::endl
                  << std::endl;
    }
    /* Here we treat Unlock as Unix time, and treat it that way in the future */
    else if (tx.unlockTime > static_cast<uint64_t>(std::time(nullptr)))
    {
        std::cout << SuccessMsg(stream.str()) << InformationMsg("Unlocks at: ")
                  << InformationMsg(Utilities::unixTimeToDate(tx.unlockTime)) << std::endl
                  << std::endl;
    }
    else
    {
        std::cout << SuccessMsg(stream.str()) << std::endl;
    }
}

void printTransferOneLine(const WalletTypes::Transaction tx)
{
    const bool incoming = tx.totalAmount() >= 0;
    const int64_t amount = std::abs(tx.totalAmount());

    std::stringstream stream;

    stream << (incoming ? "[IN] " : "[OUT] ");

    if (tx.blockHeight == 0 || tx.timestamp == 0)
    {
        stream << "h:pending t:pending ";
    }
    else
    {
        stream << "h:" << tx.blockHeight << " ";
        stream << "t:" << Utilities::unixTimeToDate(tx.timestamp) << " ";
    }

    stream << (incoming ? "+" : "-") << Utilities::formatAmount(amount) << " ";

    if (!incoming && tx.fee != 0)
    {
        stream << "fee:" << Utilities::formatAmount(tx.fee) << " ";
    }

    stream << "tx:" << tx.hash;

    if (incoming)
    {
        const int64_t difference = tx.unlockTime - tx.blockHeight;

        if (tx.unlockTime != 0 && difference > 0
            && tx.unlockTime < CryptoNote::parameters::CRYPTONOTE_MAX_BLOCK_NUMBER)
        {
            stream << " unlock_h:" << tx.unlockTime;
        }
        else if (tx.unlockTime > static_cast<uint64_t>(std::time(nullptr)))
        {
            stream << " unlock_t:" << Utilities::unixTimeToDate(tx.unlockTime);
        }
    }

    if (!tx.paymentID.empty())
    {
        stream << " pid:" << tx.paymentID;
    }

    if (incoming)
    {
        std::cout << SuccessMsg(stream.str()) << std::endl;
    }
    else
    {
        std::cout << WarningMsg(stream.str()) << std::endl;
    }
}

void listTransfers(const bool incoming, const bool outgoing, const std::shared_ptr<WalletBackend> walletBackend)
{
    uint64_t totalSpent = 0;
    uint64_t totalReceived = 0;

    uint64_t numIncomingTransactions = 0;
    uint64_t numOutgoingTransactions = 0;

    /* Grab confirmed transactions */
    std::vector<WalletTypes::Transaction> transactions = walletBackend->getTransactions();

    /* Grab any outgoing transactions still in the pool */
    const auto unconfirmedTransactions = walletBackend->getUnconfirmedTransactions();

    /* Append them, unconfirmed transactions last */
    transactions.insert(transactions.end(), unconfirmedTransactions.begin(), unconfirmedTransactions.end());

    for (const auto &tx : transactions)
    {
        /* Is a fusion transaction (on a view only wallet). It appears to have
           an incoming amount, because we can't detract the outputs (can't
           decrypt them) */
        if (tx.isFusionTransaction())
        {
            continue;
        }

        const int64_t amount = tx.totalAmount();

        if (amount < 0 && outgoing)
        {
            printTransferOneLine(tx);

            totalSpent += -amount;
            numOutgoingTransactions++;
        }
        else if (amount > 0 && incoming)
        {
            printTransferOneLine(tx);

            totalReceived += amount;
            numIncomingTransactions++;
        }
    }

    std::cout << InformationMsg("Summary:\n\n");

    if (incoming)
    {
        std::cout << SuccessMsg(numIncomingTransactions) << SuccessMsg(" incoming transactions, totalling ")
                  << SuccessMsg(Utilities::formatAmount(totalReceived)) << std::endl;
    }

    if (outgoing)
    {
        std::cout << WarningMsg(numOutgoingTransactions) << WarningMsg(" outgoing transactions, totalling ")
                  << WarningMsg(Utilities::formatAmount(totalSpent)) << std::endl;
    }
}

void listTransfersVerbose(const bool incoming, const bool outgoing, const std::shared_ptr<WalletBackend> walletBackend)
{
    uint64_t totalSpent = 0;
    uint64_t totalReceived = 0;

    uint64_t numIncomingTransactions = 0;
    uint64_t numOutgoingTransactions = 0;

    std::vector<WalletTypes::Transaction> transactions = walletBackend->getTransactions();
    const auto unconfirmedTransactions = walletBackend->getUnconfirmedTransactions();
    transactions.insert(transactions.end(), unconfirmedTransactions.begin(), unconfirmedTransactions.end());

    for (const auto &tx : transactions)
    {
        if (tx.isFusionTransaction())
        {
            continue;
        }

        const int64_t amount = tx.totalAmount();

        if (amount < 0 && outgoing)
        {
            printOutgoingTransfer(tx);

            totalSpent += -amount;
            numOutgoingTransactions++;
        }
        else if (amount > 0 && incoming)
        {
            printIncomingTransfer(tx);

            totalReceived += amount;
            numIncomingTransactions++;
        }
    }

    std::cout << InformationMsg("Summary:\n\n");

    if (incoming)
    {
        std::cout << SuccessMsg(numIncomingTransactions) << SuccessMsg(" incoming transactions, totalling ")
                  << SuccessMsg(Utilities::formatAmount(totalReceived)) << std::endl;
    }

    if (outgoing)
    {
        std::cout << WarningMsg(numOutgoingTransactions) << WarningMsg(" outgoing transactions, totalling ")
                  << WarningMsg(Utilities::formatAmount(totalSpent)) << std::endl;
    }
}

void save(const std::shared_ptr<WalletBackend> walletBackend)
{
    std::cout << InformationMsg("Saving.") << std::endl;

    Error error = walletBackend->save();

    if (error)
    {
        std::cout << WarningMsg("Failed to save wallet! Error: ") << WarningMsg(error) << std::endl;
    }
    else
    {
        std::cout << InformationMsg("Saved.") << std::endl;
    }
}

void createIntegratedAddress(const std::shared_ptr<WalletBackend> walletBackend)
{
    std::cout << InformationMsg("Creating an integrated address from an ")
              << InformationMsg("address and payment ID pair...") << std::endl
              << std::endl;

    std::string address;
    std::string paymentID;

    while (true)
    {
        std::cout << InformationMsg("Address: ");

        std::getline(std::cin, address);

        Utilities::trim(address);

        if (address.empty())
        {
            address = walletBackend->getPrimaryAddress();
            std::cout << InformationMsg("No address provided. Using primary wallet address: ")
                      << SuccessMsg(address) << std::endl;
        }

        const bool integratedAddressesAllowed = false;

        if (Error error = validateAddresses({address}, integratedAddressesAllowed); error != SUCCESS)
        {
            std::cout << WarningMsg("Invalid address: ") << WarningMsg(error) << std::endl;
        }
        else
        {
            break;
        }
    }

    while (true)
    {
        std::cout << InformationMsg("Payment ID: ");

        std::getline(std::cin, paymentID);

        Utilities::trim(paymentID);

        if (paymentID.empty())
        {
            paymentID = Common::toHex(Random::randomBytes(8));
            std::cout << InformationMsg("No payment ID provided. Generated random short payment ID: ")
                      << SuccessMsg(paymentID) << std::endl;
        }

        /* Validate the payment ID */
        if (Error error = validatePaymentID(paymentID); error != SUCCESS)
        {
            std::cout << WarningMsg("Invalid payment ID: ") << WarningMsg(error) << std::endl;
        }
        else
        {
            break;
        }
    }

    const auto [error, integratedAddress] = Utilities::createIntegratedAddress(address, paymentID);

    /* Shouldn't happen, but lets check anyway */
    if (error)
    {
        std::cout << WarningMsg("Failed to create integrated address: ") << WarningMsg(error) << std::endl;
    }
    else
    {
        std::cout << InformationMsg(integratedAddress) << std::endl;
    }
}

void help(const std::shared_ptr<WalletBackend> walletBackend)
{
    if (walletBackend->isViewWallet())
    {
        printCommands(allViewWalletCommands());
    }
    else
    {
        printCommands(allCommands());
    }
}

void listTransfersBrief(const bool incoming, const bool outgoing, const std::shared_ptr<WalletBackend> walletBackend)
{
    std::vector<WalletTypes::Transaction> transactions = walletBackend->getTransactions();
    const auto unconfirmedTransactions = walletBackend->getUnconfirmedTransactions();
    transactions.insert(transactions.end(), unconfirmedTransactions.begin(), unconfirmedTransactions.end());

    uint64_t displayed = 0;
    uint64_t matched = 0;
    constexpr uint64_t pageSize = 25;

    for (const auto &tx : transactions)
    {
        if (tx.isFusionTransaction())
        {
            continue;
        }

        const int64_t amount = tx.totalAmount();
        const bool isIncoming = amount > 0;
        const bool isOutgoing = amount < 0;

        if ((isIncoming && !incoming) || (isOutgoing && !outgoing) || (!isIncoming && !isOutgoing))
        {
            continue;
        }

        printTransferOneLine(tx);
        matched++;
        displayed++;

        if (displayed % pageSize == 0)
        {
            std::cout << InformationMsg("Press Enter for more, or type q to stop: ");
            std::string input;
            std::getline(std::cin, input);
            Utilities::trim(input);

            if (input == "q" || input == "Q")
            {
                break;
            }
        }
    }

    std::cout << InformationMsg("Displayed ") << SuccessMsg(matched) << InformationMsg(" transfer(s).") << std::endl;
}

void advanced(const std::shared_ptr<WalletBackend> walletBackend)
{
    help(walletBackend);
}

void swapNode(const std::shared_ptr<WalletBackend> walletBackend)
{
    const auto [host, port, ssl] = getDaemonAddress();

    std::cout << InformationMsg("\nSwapping node, this may take some time...\n");

    walletBackend->swapNode(host, port, ssl);

    std::cout << SuccessMsg("Node swap complete.\n\n");
}

void getTxPrivateKey(const std::shared_ptr<WalletBackend> walletBackend)
{
    const std::string txHash = getHash("What transaction hash do you want to get the private key of?: ", true);

    if (txHash == "cancel")
    {
        return;
    }

    Crypto::Hash hash;

    Common::podFromHex(txHash, hash);

    const auto [error, key] = walletBackend->getTxPrivateKey(hash);

    if (error)
    {
        std::cout << WarningMsg(error) << std::endl;
    }
    else
    {
        std::cout << InformationMsg("Transaction private key: ") << SuccessMsg(key) << std::endl;
    }
}

void checkTx(const std::shared_ptr<WalletBackend> walletBackend, const std::string commandInput)
{
    std::string txHash;

    if (commandInput.rfind("check_tx ", 0) == 0)
    {
        txHash = commandInput.substr(std::string("check_tx ").size());
        Utilities::trim(txHash);
    }

    if (txHash.empty())
    {
        txHash = getHash("Transaction hash to check (or cancel): ", true);
    }

    if (txHash == "cancel")
    {
        return;
    }

    if (Error error = validateHash(txHash); error != SUCCESS)
    {
        std::cout << WarningMsg("Invalid hash: ") << WarningMsg(error) << std::endl;
        return;
    }

    Crypto::Hash hash;
    Common::podFromHex(txHash, hash);

    const auto unconfirmed = walletBackend->getUnconfirmedTransactions();
    const auto unconfirmedIt = std::find_if(unconfirmed.begin(), unconfirmed.end(), [&hash](const auto &tx) {
        return tx.hash == hash;
    });

    const auto confirmed = walletBackend->getTransactions();
    const auto confirmedIt = std::find_if(confirmed.begin(), confirmed.end(), [&hash](const auto &tx) {
        return tx.hash == hash;
    });

    std::cout << InformationMsg("Wallet lookup: ");
    if (unconfirmedIt != unconfirmed.end())
    {
        std::cout << WarningMsg("found (pending outgoing in pool)") << std::endl;
        std::cout << "  amount: " << WarningMsg(Utilities::formatAmount(std::abs(unconfirmedIt->totalAmount())))
                  << ", fee: " << WarningMsg(Utilities::formatAmount(unconfirmedIt->fee)) << std::endl;
    }
    else if (confirmedIt != confirmed.end())
    {
        const bool incoming = confirmedIt->totalAmount() > 0;
        std::cout << SuccessMsg("found (confirmed ") << SuccessMsg(incoming ? "incoming" : "outgoing")
                  << SuccessMsg(")") << std::endl;
        std::cout << "  block: " << SuccessMsg(confirmedIt->blockHeight) << ", amount: ";
        if (incoming)
        {
            std::cout << SuccessMsg(Utilities::formatAmount(confirmedIt->totalAmount()));
        }
        else
        {
            std::cout << WarningMsg(Utilities::formatAmount(std::abs(confirmedIt->totalAmount())));
        }
        std::cout << std::endl;
        if (!confirmedIt->paymentID.empty())
        {
            std::cout << "  payment ID: " << SuccessMsg(confirmedIt->paymentID) << std::endl;
        }
    }
    else
    {
        std::cout << WarningMsg("not found in this wallet") << std::endl;
    }

    std::unordered_set<Crypto::Hash> hashes = {hash};
    std::unordered_set<Crypto::Hash> inPool;
    std::unordered_set<Crypto::Hash> inBlock;
    std::unordered_set<Crypto::Hash> unknown;

    const bool daemonOk = walletBackend->getTransactionsStatus(hashes, inPool, inBlock, unknown);

    std::cout << InformationMsg("Node lookup: ");
    if (!daemonOk)
    {
        std::cout << WarningMsg("failed to query daemon") << std::endl;
        return;
    }

    if (inPool.find(hash) != inPool.end())
    {
        std::cout << WarningMsg("transaction is in pool") << std::endl;
    }
    else if (inBlock.find(hash) != inBlock.end())
    {
        std::cout << SuccessMsg("transaction is in a block") << std::endl;
    }
    else if (unknown.find(hash) != unknown.end())
    {
        std::cout << WarningMsg("transaction is unknown to daemon") << std::endl;
    }
    else
    {
        std::cout << WarningMsg("daemon returned no status for this transaction") << std::endl;
    }
}

void decodeIntegrated(const std::shared_ptr<WalletBackend> walletBackend, const std::string commandInput)
{
    std::string integratedAddress;

    if (commandInput.rfind("decode_integrated ", 0) == 0)
    {
        integratedAddress = commandInput.substr(std::string("decode_integrated ").size());
        Utilities::trim(integratedAddress);
    }

    if (integratedAddress.empty())
    {
        std::cout << InformationMsg("Integrated address to decode (or cancel): ");
        std::getline(std::cin, integratedAddress);
        Utilities::trim(integratedAddress);
    }

    if (integratedAddress == "cancel")
    {
        return;
    }

    if (!Utilities::isIntegratedAddress(integratedAddress))
    {
        std::cout << WarningMsg("This is not an integrated address format for this network.") << std::endl;
        std::cout << InformationMsg("Expected length: ") << SuccessMsg(WalletConfig::integratedAddressLength)
                  << InformationMsg(" (short) or ") << SuccessMsg(WalletConfig::integratedAddressLengthLong)
                  << InformationMsg(" (long).") << std::endl;
        return;
    }

    if (Error error = validateAddresses({integratedAddress}, true); error != SUCCESS)
    {
        std::cout << WarningMsg("Invalid integrated address: ") << WarningMsg(error) << std::endl;
        return;
    }

    const auto [actualAddress, paymentID] = Utilities::extractIntegratedAddressData(integratedAddress);

    std::cout << InformationMsg("Decoded address: ") << SuccessMsg(actualAddress) << std::endl;
    std::cout << InformationMsg("Embedded payment ID: ") << SuccessMsg(paymentID) << std::endl;

    if (walletBackend != nullptr && actualAddress == walletBackend->getPrimaryAddress())
    {
        std::cout << SuccessMsg("This integrated address maps to your primary wallet address.") << std::endl;
    }
}

void masternodeRegister(const std::shared_ptr<WalletBackend> walletBackend, const std::string commandInput)
{
    if (walletBackend->isViewWallet())
    {
        std::cout << WarningMsg("Masternode registration is not available for view wallets.") << std::endl;
        return;
    }

    std::string token;
    std::string endpointInput;
    if (commandInput.rfind("mn_register ", 0) == 0)
    {
        const auto args = Utilities::split(commandInput.substr(std::string("mn_register ").size()), ' ');
        if (!args.empty())
        {
            token = args[0];
        }
        if (args.size() > 1)
        {
            endpointInput = args[1];
        }
        Utilities::trim(token);
        Utilities::trim(endpointInput);
    }

    if (token.empty())
    {
        std::cout << InformationMsg("Enter daemon token string (or cancel): ");
        std::getline(std::cin, token);
        Utilities::trim(token);
    }

    if (token == "cancel")
    {
        return;
    }

    if (endpointInput.empty())
    {
        std::cout << InformationMsg("Enter public endpoint for one-IP commitment (IPv4:port or [IPv6]:port, or cancel): ");
        std::getline(std::cin, endpointInput);
        Utilities::trim(endpointInput);
    }

    if (endpointInput == "cancel")
    {
        return;
    }

    /* Canonicalize an endpoint string into a stable form for the on-chain commitment preimage.
     * Accepted formats:
     *   IPv4:  "1.2.3.4:17855"      -> canonical "1.2.3.4:17855"
     *   IPv6:  "[2001:db8::1]:17855" -> canonical "[2001:db8::1]:17855"
     * The canonical string is then hashed: cn_fast_hash("MNIP1|<canonical>")
     * Only the 32-byte hash is stored on-chain; the raw address is never committed. */
    auto canonicalizeEndpoint = [](const std::string &raw, std::string &canonical) -> bool {
        std::string ip;
        std::string portStr;

        if (!raw.empty() && raw[0] == '[')
        {
            /* IPv6 bracket notation: [addr]:port */
            const auto closeBracket = raw.find(']');
            if (closeBracket == std::string::npos || closeBracket < 2)
            {
                return false;
            }
            ip = raw.substr(1, closeBracket - 1);
            if (closeBracket + 1 >= raw.size() || raw[closeBracket + 1] != ':')
            {
                return false;
            }
            portStr = raw.substr(closeBracket + 2);

            /* Basic sanity: must contain at least two colons (IPv6 has ≥2) and no dots */
            if (std::count(ip.begin(), ip.end(), ':') < 2)
            {
                return false;
            }

            /* Validate and canonicalize port */
            uint64_t port = 0;
            try
            {
                port = std::stoull(portStr);
            }
            catch (const std::exception &)
            {
                return false;
            }
            if (port == 0 || port > 65535)
            {
                return false;
            }

            /* Use inet_pton + inet_ntop for RFC 5952 canonical form.
             * This normalises leading zeros, expanded groups, and :: compression,
             * so "2001:0DB8::0001" and "2001:db8::1" produce the same commitment. */
            struct in6_addr addr6;
            if (inet_pton(AF_INET6, ip.c_str(), &addr6) != 1)
            {
                return false;
            }
            char canonBuf[INET6_ADDRSTRLEN];
            if (inet_ntop(AF_INET6, &addr6, canonBuf, sizeof(canonBuf)) == nullptr)
            {
                return false;
            }
            canonical = "[" + std::string(canonBuf) + "]:" + std::to_string(port);
            return true;
        }
        else
        {
            /* IPv4: addr:port — use rfind(':') so dotted-decimal is not confused with IPv6 colons */
            const auto colonPos = raw.rfind(':');
            if (colonPos == std::string::npos || colonPos == 0 || colonPos + 1 >= raw.size())
            {
                return false;
            }

            ip = raw.substr(0, colonPos);
            portStr = raw.substr(colonPos + 1);
            uint64_t port = 0;
            try
            {
                port = std::stoull(portStr);
            }
            catch (const std::exception &)
            {
                return false;
            }
            if (port == 0 || port > 65535)
            {
                return false;
            }

            const auto octets = Utilities::split(ip, '.');
            if (octets.size() != 4)
            {
                return false;
            }

            std::array<uint64_t, 4> nums {{0, 0, 0, 0}};
            for (size_t i = 0; i < octets.size(); ++i)
            {
                try
                {
                    if (octets[i].empty() || octets[i].size() > 3)
                    {
                        return false;
                    }
                    nums[i] = std::stoull(octets[i]);
                }
                catch (const std::exception &)
                {
                    return false;
                }
                if (nums[i] > 255)
                {
                    return false;
                }
            }

            canonical = std::to_string(nums[0]) + "." + std::to_string(nums[1]) + "." + std::to_string(nums[2])
                + "." + std::to_string(nums[3]) + ":" + std::to_string(port);
            return true;
        }
    };

    std::string canonicalEndpoint;
    if (!canonicalizeEndpoint(endpointInput, canonicalEndpoint))
    {
        std::cout << WarningMsg("Invalid endpoint format. Expected IPv4:port or [IPv6]:port (e.g. 1.2.3.4:17855 or [2001:db8::1]:17855)") << std::endl;
        return;
    }

    const std::string endpointCommitmentPreimage = "MNIP1|" + canonicalEndpoint;
    Crypto::Hash endpointCommitment =
        Crypto::cn_fast_hash(endpointCommitmentPreimage.data(), endpointCommitmentPreimage.size());

    Crypto::Hash masternodeId;
    Crypto::Hash registrationTokenId;
    uint32_t expiresAtHeight = 0;

    if (token.rfind("MNREG2:", 0) == 0)
    {
        const auto parts = Utilities::split(token, ':');
        if (parts.size() != 4)
        {
            std::cout << WarningMsg("Invalid MNREG2 token format.") << std::endl;
            return;
        }

        if (!Common::podFromHex(parts[1], masternodeId) || !Common::podFromHex(parts[2], registrationTokenId))
        {
            std::cout << WarningMsg("Invalid masternode id or registration token id in token string.") << std::endl;
            return;
        }

        try
        {
            expiresAtHeight = static_cast<uint32_t>(std::stoul(parts[3]));
        }
        catch (const std::exception &)
        {
            std::cout << WarningMsg("Invalid token expiry height in token string.") << std::endl;
            return;
        }
    }
    else if (token.rfind("MNREG1:", 0) == 0)
    {
        token = token.substr(std::string("MNREG1:").size());
        Utilities::trim(token);
        if (!Common::podFromHex(token, masternodeId))
        {
            std::cout << WarningMsg("Invalid legacy masternode registration token.") << std::endl;
            return;
        }

        Random::randomBytes(sizeof(registrationTokenId), registrationTokenId.data);
        expiresAtHeight = static_cast<uint32_t>(
            walletBackend->getStatus().networkBlockCount + CryptoNote::parameters::MASTERNODE_REGISTRATION_TOKEN_TTL_BLOCKS);
    }
    else
    {
        std::cout << WarningMsg("Invalid masternode registration token.") << std::endl;
        return;
    }

    const uint64_t networkHeight = walletBackend->getStatus().networkBlockCount;
    if (expiresAtHeight <= networkHeight)
    {
        std::cout << WarningMsg("Registration token is already expired at current network height.") << std::endl;
        return;
    }

    const auto primaryAddress = walletBackend->getPrimaryAddress();
    const auto [keysError, publicSpendKey, privateSpendKey, walletIndex] = walletBackend->getSpendKeys(primaryAddress);
    (void)walletIndex;
    if (keysError)
    {
        std::cout << WarningMsg("Failed to read wallet spend keys: ") << WarningMsg(keysError) << std::endl;
        return;
    }

    const uint64_t minCollateralAmount = CryptoNote::parameters::MASTERNODE_COLLATERAL_LOCK_AMOUNT;
    const auto [inputsError, spendableInputs] = walletBackend->getSpendableInputs(primaryAddress);
    if (inputsError)
    {
        std::cout << WarningMsg("Failed to read spendable inputs: ") << WarningMsg(inputsError) << std::endl;
        return;
    }

    std::optional<WalletTypes::TransactionInput> collateralInput;
    for (const auto &input : spendableInputs)
    {
        if (input.amount < minCollateralAmount)
        {
            continue;
        }

        if (!input.globalOutputIndex.has_value())
        {
            continue;
        }

        if (!input.privateEphemeral.has_value())
        {
            continue;
        }

        if (!collateralInput.has_value() || input.amount < collateralInput->amount)
        {
            collateralInput = input;
        }
    }

    if (!collateralInput.has_value())
    {
        std::cout << WarningMsg("No spendable collateral output found in this wallet.") << std::endl;
        std::cout << InformationMsg("Requirement: unlocked output >= ")
                  << SuccessMsg(Utilities::formatAmount(minCollateralAmount))
                  << std::endl;
        return;
    }

    if (*collateralInput->globalOutputIndex > std::numeric_limits<uint32_t>::max())
    {
        std::cout << WarningMsg("Collateral global output index exceeds supported range.") << std::endl;
        return;
    }

    const uint64_t collateralAmount = collateralInput->amount;
    const uint32_t collateralGlobalOutputIndex = static_cast<uint32_t>(*collateralInput->globalOutputIndex);
    const Crypto::KeyImage collateralKeyImage = collateralInput->keyImage;
    const Crypto::PublicKey collateralOutputKey = collateralInput->key;
    const Crypto::SecretKey collateralOutputSecret = *collateralInput->privateEphemeral;
    Crypto::KeyImage recomputedCollateralKeyImage;
    Crypto::generate_key_image(collateralOutputKey, collateralOutputSecret, recomputedCollateralKeyImage);
    if (recomputedCollateralKeyImage != collateralKeyImage)
    {
        std::cout << WarningMsg("Selected collateral input key image consistency check failed.") << std::endl;
        return;
    }

    // Generate a dedicated signing keypair for ChainLock/InstantSend participation.
    // The private key must be saved by the operator and supplied via --mn-signing-key.
    Crypto::PublicKey signingPublicKey;
    Crypto::SecretKey signingPrivateKey;
    Crypto::generate_keys(signingPublicKey, signingPrivateKey);

    std::vector<uint8_t> unsignedPayload;
    unsignedPayload.reserve(
        4 + 1 + sizeof(Crypto::Hash) + sizeof(Crypto::PublicKey) + sizeof(Crypto::Hash) + sizeof(uint32_t)
        + sizeof(uint64_t) + sizeof(uint32_t) + sizeof(Crypto::KeyImage) + sizeof(Crypto::PublicKey)
        + sizeof(Crypto::Hash) + sizeof(Crypto::PublicKey) /* signingKey */);
    unsignedPayload.push_back('M');
    unsignedPayload.push_back('N');
    unsignedPayload.push_back('0');
    unsignedPayload.push_back('1');
    unsignedPayload.push_back(static_cast<uint8_t>(1)); // Register
    unsignedPayload.insert(unsignedPayload.end(), masternodeId.data, masternodeId.data + sizeof(masternodeId.data));
    unsignedPayload.insert(
        unsignedPayload.end(),
        publicSpendKey.data,
        publicSpendKey.data + sizeof(publicSpendKey.data));
    unsignedPayload.insert(
        unsignedPayload.end(),
        registrationTokenId.data,
        registrationTokenId.data + sizeof(registrationTokenId.data));
    unsignedPayload.push_back(static_cast<uint8_t>(expiresAtHeight & 0xff));
    unsignedPayload.push_back(static_cast<uint8_t>((expiresAtHeight >> 8) & 0xff));
    unsignedPayload.push_back(static_cast<uint8_t>((expiresAtHeight >> 16) & 0xff));
    unsignedPayload.push_back(static_cast<uint8_t>((expiresAtHeight >> 24) & 0xff));
    for (size_t i = 0; i < sizeof(uint64_t); ++i)
    {
        unsignedPayload.push_back(static_cast<uint8_t>((collateralAmount >> (8 * i)) & 0xff));
    }
    unsignedPayload.push_back(static_cast<uint8_t>(collateralGlobalOutputIndex & 0xff));
    unsignedPayload.push_back(static_cast<uint8_t>((collateralGlobalOutputIndex >> 8) & 0xff));
    unsignedPayload.push_back(static_cast<uint8_t>((collateralGlobalOutputIndex >> 16) & 0xff));
    unsignedPayload.push_back(static_cast<uint8_t>((collateralGlobalOutputIndex >> 24) & 0xff));
    unsignedPayload.insert(
        unsignedPayload.end(),
        collateralKeyImage.data,
        collateralKeyImage.data + sizeof(collateralKeyImage.data));
    unsignedPayload.insert(
        unsignedPayload.end(),
        collateralOutputKey.data,
        collateralOutputKey.data + sizeof(collateralOutputKey.data));
    unsignedPayload.insert(
        unsignedPayload.end(),
        endpointCommitment.data,
        endpointCommitment.data + sizeof(endpointCommitment.data));
    // v2: append signing public key to unsigned payload
    unsignedPayload.insert(
        unsignedPayload.end(),
        signingPublicKey.data,
        signingPublicKey.data + sizeof(signingPublicKey.data));

    Crypto::Hash signingHash = Crypto::cn_fast_hash(unsignedPayload.data(), unsignedPayload.size());
    Crypto::Signature signature;
    Crypto::generate_signature(signingHash, publicSpendKey, privateSpendKey, signature);
    Crypto::Signature collateralSignature;
    if (
        !Crypto::generate_key_image_dleq_proof(
            signingHash,
            collateralOutputKey,
            collateralOutputSecret,
            collateralSignature))
    {
        std::cout << WarningMsg("Failed to generate collateral linkage proof.") << std::endl;
        return;
    }

    std::vector<uint8_t> extraData = unsignedPayload;
    extraData.insert(extraData.end(), signature.data, signature.data + sizeof(signature.data));
    extraData.insert(
        extraData.end(),
        collateralSignature.data,
        collateralSignature.data + sizeof(collateralSignature.data));

    const uint64_t registrationOutputAmount = WalletConfig::minimumSend;
    const uint64_t unlockedBalance = walletBackend->getTotalUnlockedBalance();

    if (unlockedBalance < registrationOutputAmount)
    {
        std::cout << WarningMsg("Insufficient unlocked balance for masternode registration.") << std::endl;
        std::cout << InformationMsg("Required at least: ")
                  << SuccessMsg(Utilities::formatAmount(registrationOutputAmount))
                  << std::endl;
        std::cout << InformationMsg("Unlocked balance: ")
                  << WarningMsg(Utilities::formatAmount(unlockedBalance))
                  << std::endl;
        return;
    }

    const auto [minMixin, maxMixin, defaultMixin] =
        Utilities::getMixinAllowableRange(walletBackend->getStatus().networkBlockCount);
    (void)minMixin;
    (void)maxMixin;

    Error error;
    WalletTypes::PreparedTransactionInfo preparedTransaction;
    std::tie(error, std::ignore, preparedTransaction) = walletBackend->sendTransactionAdvanced(
        {{primaryAddress, registrationOutputAmount}},
        defaultMixin,
        WalletTypes::FeeType::MinimumFee(),
        "",
        {},
        primaryAddress,
        0,
        extraData,
        false,
        false /* prepare only */
    );

    if (error)
    {
        if (error == TOO_MANY_INPUTS_TO_FIT_IN_BLOCK)
        {
            std::cout << WarningMsg("Registration transaction is too large due to input fragmentation.") << std::endl;
            std::cout << InformationMsg("Run ") << SuccessMsg("optimize")
                      << InformationMsg(" in wallet, wait confirmations, then run ")
                      << SuccessMsg("mn_register")
                      << InformationMsg(" again.") << std::endl;
            return;
        }

        std::cout << WarningMsg("Failed to prepare masternode registration transaction: ") << WarningMsg(error) << std::endl;
        return;
    }

    std::cout << InformationMsg("Masternode ID: ") << SuccessMsg(Common::podToHex(masternodeId)) << std::endl;
    std::cout << InformationMsg("Registration token ID: ") << SuccessMsg(Common::podToHex(registrationTokenId))
              << std::endl;
    std::cout << InformationMsg("Token expires at height: ") << SuccessMsg(std::to_string(expiresAtHeight))
              << std::endl;
    std::cout << InformationMsg("Collateral amount locked: ")
              << SuccessMsg(Utilities::formatAmount(collateralAmount)) << std::endl;
    std::cout << InformationMsg("Collateral global index: ")
              << SuccessMsg(std::to_string(collateralGlobalOutputIndex)) << std::endl;
    std::cout << InformationMsg("Collateral key image: ")
              << SuccessMsg(Common::podToHex(collateralKeyImage)) << std::endl;
    std::cout << InformationMsg("Endpoint commitment: ")
              << SuccessMsg(Common::podToHex(endpointCommitment)) << std::endl;
    std::cout << InformationMsg("Signing public key:  ")
              << SuccessMsg(Common::podToHex(signingPublicKey)) << std::endl;
    std::cout << WarningMsg("Signing private key: ") << WarningMsg(Common::podToHex(signingPrivateKey)) << std::endl;
    std::cout << WarningMsg("IMPORTANT: Save the signing private key above. Pass it to your daemon with")  << std::endl;
    std::cout << WarningMsg("           --mn-signing-key=<hex> to enable ChainLock/InstantSend signing.") << std::endl;
    std::cout << InformationMsg("This operation binds and locks the selected collateral output in consensus.")
              << std::endl;
    std::cout << WarningMsg("Recommendation: use a dedicated wallet for masternode registration.")
              << std::endl;

    if (!Utilities::confirm("Submit masternode registration transaction to blockchain?"))
    {
        std::cout << WarningMsg("Masternode registration cancelled.") << std::endl;
        return;
    }

    ZedUtilities::confirmPassword(walletBackend, "Confirm your password: ");

    Crypto::Hash txHash;
    std::tie(error, txHash) = walletBackend->sendPreparedTransaction(preparedTransaction.transactionHash);
    if (error)
    {
        std::cout << WarningMsg("Failed to submit masternode registration transaction: ") << WarningMsg(error)
                  << std::endl;
        return;
    }

    std::cout << SuccessMsg("Masternode registration transaction submitted.") << std::endl;
    std::cout << InformationMsg("Hash: ") << SuccessMsg(txHash) << std::endl;
}

void masternodeAttest(const std::shared_ptr<WalletBackend> walletBackend, const std::string commandInput)
{
    if (walletBackend->isViewWallet())
    {
        std::cout << WarningMsg("Masternode attestation is not available for view wallets.") << std::endl;
        return;
    }

    std::string mnIdHex;
    std::string healthyFlagStr;
    if (commandInput.rfind("mn_attest ", 0) == 0)
    {
        const auto args = Utilities::split(commandInput.substr(std::string("mn_attest ").size()), ' ');
        if (args.size() >= 2)
        {
            mnIdHex = args[0];
            healthyFlagStr = args[1];
        }
    }

    if (mnIdHex.empty())
    {
        std::cout << InformationMsg("Enter masternode id hex (or cancel): ");
        std::getline(std::cin, mnIdHex);
        Utilities::trim(mnIdHex);
    }

    if (mnIdHex == "cancel")
    {
        return;
    }

    if (healthyFlagStr.empty())
    {
        std::cout << InformationMsg("Healthy flag (1 healthy, 0 unhealthy): ");
        std::getline(std::cin, healthyFlagStr);
        Utilities::trim(healthyFlagStr);
    }

    bool healthy = false;
    if (healthyFlagStr == "1")
    {
        healthy = true;
    }
    else if (healthyFlagStr == "0")
    {
        healthy = false;
    }
    else
    {
        std::cout << WarningMsg("Invalid healthy flag. Use 1 or 0.") << std::endl;
        return;
    }

    Crypto::Hash masternodeId;
    if (!Common::podFromHex(mnIdHex, masternodeId))
    {
        std::cout << WarningMsg("Invalid masternode id hex.") << std::endl;
        return;
    }

    const auto verifierAddress = walletBackend->getPrimaryAddress();
    const auto [keysError, verifierPublicKey, verifierPrivateKey, walletIndex] = walletBackend->getSpendKeys(verifierAddress);
    (void)walletIndex;
    if (keysError)
    {
        std::cout << WarningMsg("Failed to read verifier spend keys: ") << WarningMsg(keysError) << std::endl;
        return;
    }

    std::vector<uint8_t> unsignedPayload;
    unsignedPayload.reserve(4 + 1 + sizeof(Crypto::Hash) + sizeof(Crypto::PublicKey) + 1);
    unsignedPayload.push_back('M');
    unsignedPayload.push_back('N');
    unsignedPayload.push_back('0');
    unsignedPayload.push_back('1');
    unsignedPayload.push_back(static_cast<uint8_t>(7)); // Attest
    unsignedPayload.insert(unsignedPayload.end(), masternodeId.data, masternodeId.data + sizeof(masternodeId.data));
    unsignedPayload.insert(
        unsignedPayload.end(),
        verifierPublicKey.data,
        verifierPublicKey.data + sizeof(verifierPublicKey.data));
    unsignedPayload.push_back(healthy ? 1 : 0);

    Crypto::Hash signingHash = Crypto::cn_fast_hash(unsignedPayload.data(), unsignedPayload.size());
    Crypto::Signature signature;
    Crypto::generate_signature(signingHash, verifierPublicKey, verifierPrivateKey, signature);

    std::vector<uint8_t> extraData = unsignedPayload;
    extraData.insert(extraData.end(), signature.data, signature.data + sizeof(signature.data));

    const uint64_t attestationOutputAmount = WalletConfig::minimumSend;
    if (walletBackend->getTotalUnlockedBalance() < attestationOutputAmount)
    {
        std::cout << WarningMsg("Insufficient unlocked balance for attestation transaction.") << std::endl;
        return;
    }

    const auto [minMixin, maxMixin, defaultMixin] =
        Utilities::getMixinAllowableRange(walletBackend->getStatus().networkBlockCount);
    (void)minMixin;
    (void)maxMixin;

    Error error;
    WalletTypes::PreparedTransactionInfo preparedTransaction;
    std::tie(error, std::ignore, preparedTransaction) = walletBackend->sendTransactionAdvanced(
        {{verifierAddress, attestationOutputAmount}},
        defaultMixin,
        WalletTypes::FeeType::MinimumFee(),
        "",
        {},
        verifierAddress,
        0,
        extraData,
        false,
        false);

    if (error)
    {
        if (error == TOO_MANY_INPUTS_TO_FIT_IN_BLOCK)
        {
            std::cout << WarningMsg("Attestation transaction is too large.") << std::endl;
            std::cout << InformationMsg("Run ") << SuccessMsg("optimize")
                      << InformationMsg(" and retry.") << std::endl;
            return;
        }

        std::cout << WarningMsg("Failed to prepare masternode attestation transaction: ")
                  << WarningMsg(error) << std::endl;
        return;
    }

    std::cout << InformationMsg("Masternode ID: ") << SuccessMsg(Common::podToHex(masternodeId)) << std::endl;
    std::cout << InformationMsg("Verifier key: ") << SuccessMsg(Common::podToHex(verifierPublicKey)) << std::endl;
    std::cout << InformationMsg("Attestation verdict: ") << SuccessMsg(healthy ? "healthy" : "unhealthy")
              << std::endl;

    if (!Utilities::confirm("Submit masternode attestation transaction to blockchain?"))
    {
        std::cout << WarningMsg("Masternode attestation cancelled.") << std::endl;
        return;
    }

    ZedUtilities::confirmPassword(walletBackend, "Confirm your password: ");

    Crypto::Hash txHash;
    std::tie(error, txHash) = walletBackend->sendPreparedTransaction(preparedTransaction.transactionHash);
    if (error)
    {
        std::cout << WarningMsg("Failed to submit masternode attestation transaction: ")
                  << WarningMsg(error) << std::endl;
        return;
    }

    std::cout << SuccessMsg("Masternode attestation transaction submitted.") << std::endl;
    std::cout << InformationMsg("Hash: ") << SuccessMsg(txHash) << std::endl;
}

void setLogLevel()
{
    const std::vector<Command> logLevels = {
        Command("Trace",    "Display extremely detailed logging output"),
        Command("Debug",    "Display highly detailed logging output"),
        Command("Info",     "Display detailed logging output"),
        Command("Warning",  "Display only warning and error logging output"),
        Command("Fatal",    "Display only error logging output"),
        Command("Disabled", "Don't display any logging output"),
    };

    printCommands(logLevels);

    std::string level = parseCommand(logLevels, logLevels, "What log level do you want to use?: ");

    if (level == "exit")
    {
        return;
    }

    Logger::logger.setLogLevel(Logger::stringToLogLevel(level));
}
