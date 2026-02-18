// Portions Copyright (c) 2018-2019, The Catalyst Developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

///////////////////////////////////////////////
#include <zedwallet++/CommandImplementations.h>
///////////////////////////////////////////////

#include <config/CryptoNoteConfig.h>
#include <config/WalletConfig.h>
#include <crypto/random.h>
#include <common/StringTools.h>
#include <errors/ValidateParameters.h>
#include <cstdlib>
#include <fstream>
#include <logger/Logger.h>
#include <utilities/Addresses.h>
#include <utilities/ColouredMsg.h>
#include <utilities/FormatTools.h>
#include <utilities/Input.h>
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
        else if (paymentID.length() != WalletConfig::shortPaymentIDLength)
        {
            std::cout << WarningMsg("Invalid payment ID: ")
                      << WarningMsg("Integrated addresses require a short payment ID of 16 hex characters.")
                      << std::endl;
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
