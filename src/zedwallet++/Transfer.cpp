// Copyright (c) 2018-2019, The TurtleCoin Developers
// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

/////////////////////////////////
#include <zedwallet++/Transfer.h>
/////////////////////////////////

#include <config/WalletConfig.h>
#include <config/CryptoNoteConfig.h>
#include <algorithm>
#include <chrono>
#include <iostream>
#include <thread>
#include <utilities/Addresses.h>
#include <utilities/ColouredMsg.h>
#include <utilities/FormatTools.h>
#include <utilities/Input.h>
#include <zedwallet++/GetInput.h>
#include <zedwallet++/Utilities.h>

namespace
{
    void cancel()
    {
        std::cout << WarningMsg("Cancelling transaction.\n");
    }

    std::string getAddressTypeLabel(const std::string &address)
    {
        if (!Utilities::isIntegratedAddress(address))
        {
            return "standard";
        }

        if (address.length() == WalletConfig::integratedAddressLength)
        {
            return "integrated-short";
        }

        return "integrated-long";
    }

    void watchTransactionUntilConfirmed(
        const std::shared_ptr<WalletBackend> &walletBackend,
        const Crypto::Hash &hash,
        const uint64_t waitSeconds = 20)
    {
        const uint64_t intervalSeconds = 5;
        const uint64_t maxChecks = std::max<uint64_t>(1, waitSeconds / intervalSeconds);

        for (uint64_t i = 0; i < maxChecks; i++)
        {
            const auto unconfirmed = walletBackend->getUnconfirmedTransactions();

            const auto inPool = std::find_if(unconfirmed.begin(), unconfirmed.end(), [&hash](const auto &tx) {
                return tx.hash == hash;
            });

            if (inPool != unconfirmed.end())
            {
                std::cout << InformationMsg("Status: pending in pool...") << std::endl;
                std::this_thread::sleep_for(std::chrono::seconds(intervalSeconds));
                continue;
            }

            const auto confirmed = walletBackend->getTransactions();
            const auto inBlock = std::find_if(confirmed.begin(), confirmed.end(), [&hash](const auto &tx) {
                return tx.hash == hash;
            });

            if (inBlock != confirmed.end())
            {
                std::cout << SuccessMsg("Status: confirmed in block ") << SuccessMsg(inBlock->blockHeight) << std::endl;
                return;
            }

            std::this_thread::sleep_for(std::chrono::seconds(intervalSeconds));
        }

        std::cout << WarningMsg("Status still pending. Returning to prompt so you can continue using the wallet.")
                  << std::endl;
        std::cout << InformationMsg("Use ")
                  << SuccessMsg("txs")
                  << InformationMsg(" / ")
                  << SuccessMsg("list_transfers")
                  << InformationMsg(" to check confirmation later.")
                  << std::endl;
    }
} // namespace

void transfer(const std::shared_ptr<WalletBackend> walletBackend, const bool sendAll)
{
    std::cout << InformationMsg("Note: You can type cancel at any time to "
                                "cancel the transaction\n\n");

    const bool integratedAddressesAllowed(true), cancelAllowed(true);

    const auto unlockedBalance = walletBackend->getTotalUnlockedBalance();

    /* nodeFee will be zero if using a node without a fee, so we can add this
       safely */
    const auto [nodeFee, nodeAddress] = walletBackend->getNodeFee();

    
    std::string address =
        getAddress("What address do you want to transfer to?: ", integratedAddressesAllowed, cancelAllowed);

    if (address == "cancel")
    {
        cancel();
        return;
    }

    std::cout << InformationMsg("Address type: ") << SuccessMsg(getAddressTypeLabel(address)) << std::endl;
    std::cout << "\n";

    std::string paymentID;

    if (!Utilities::isIntegratedAddress(address))
    {
        paymentID = getPaymentID(
            "What payment ID do you want to use?\n"
            "These are usually used for sending to exchanges.",
            cancelAllowed);

        if (paymentID == "cancel")
        {
            cancel();
            return;
        }

        std::cout << "\n";
    }
    else
    {
        std::cout << InformationMsg("Integrated address detected. Payment ID is embedded; skipping payment ID prompt.")
                  << std::endl
                  << std::endl;
    }

    /* If we're using send all, then we'll work out the max in the WalletBackend
     * code, since we need to take into account fee per byte. For now, we'll
     * just set the amount to all balance minus nodeFee. */
    uint64_t amount = unlockedBalance - nodeFee;
    
    if (!sendAll)
    {
        bool success;

        std::tie(success, amount) =
            getAmountToAtomic("How much " + WalletConfig::ticker + " do you want to send?: ", cancelAllowed);

        std::cout << "\n";

        if (!success)
        {
            cancel();
            return;
        }
    }

    if (nodeFee >= unlockedBalance && sendAll)
    {
        std::cout << WarningMsg("\nYou don't have enough funds to cover "
                                "this transaction!\n\n")
                  << "Funds needed: " << InformationMsg(Utilities::formatAmount(nodeFee + WalletConfig::minimumSend))
                  << " (Includes a node fee of " << InformationMsg(Utilities::formatAmount(nodeFee))
                  << ")\nFunds available: " << SuccessMsg(Utilities::formatAmount(unlockedBalance)) << "\n\n";

        cancel();

        return;
    }

    sendTransaction(walletBackend, address, amount, paymentID, sendAll);
}

void sendTransaction(
    const std::shared_ptr<WalletBackend> walletBackend,
    const std::string address,
    const uint64_t amount,
    const std::string paymentID,
    const bool sendAll)
{
    std::string destination = address;
    std::string effectivePaymentID = paymentID;

    if (Utilities::isIntegratedAddress(destination))
    {
        const auto [extractedAddress, extractedPaymentID] = Utilities::extractIntegratedAddressData(destination);
        destination = extractedAddress;

        if (effectivePaymentID.empty())
        {
            effectivePaymentID = extractedPaymentID;
        }
        else if (effectivePaymentID != extractedPaymentID)
        {
            std::cout << WarningMsg("Conflicting payment IDs detected between integrated address and input PID.")
                      << std::endl;
            cancel();
            return;
        }
    }

    const auto unlockedBalance = walletBackend->getTotalUnlockedBalance();

    /* nodeFee will be zero if using a node without a fee, so we can add this
       safely */
    const auto [nodeFee, nodeAddress] = walletBackend->getNodeFee();

    /* The total balance required with fees added (Doesn't include network
     * fee, since that's done per byte and is hard to guess) */
    const uint64_t total = amount + nodeFee;

    if (total > unlockedBalance)
    {
        std::cout << WarningMsg("\nYou don't have enough funds to cover "
                                "this transaction!\n\n")
                  << "Funds needed: " << InformationMsg(Utilities::formatAmount(amount + nodeFee))
                  << " (Includes a node fee of " << InformationMsg(Utilities::formatAmount(nodeFee))
                  << ")\nFunds available: " << SuccessMsg(Utilities::formatAmount(unlockedBalance)) << "\n\n";

        cancel();

        return;
    }

    Error error;
    WalletTypes::PreparedTransactionInfo preparedTransaction;

    std::tie(error, std::ignore, preparedTransaction) = walletBackend->sendTransactionBasic(
        destination,
        amount,
        effectivePaymentID,
        sendAll,
        false /* Don't relay to network */
    );

    if (error == NOT_ENOUGH_BALANCE)
    {
        const uint64_t actualAmount = sendAll ? WalletConfig::minimumSend : amount;

        std::cout << WarningMsg("\nYou don't have enough funds to cover "
                                "this transaction!\n\n")
                  << "Funds needed: " << InformationMsg(Utilities::formatAmount(actualAmount + preparedTransaction.fee + nodeFee))
                  << " (Includes a network fee of " << InformationMsg(Utilities::formatAmount(preparedTransaction.fee))
                  << " and a node fee of " << InformationMsg(Utilities::formatAmount(nodeFee))
                  << ")\nFunds available: " << SuccessMsg(Utilities::formatAmount(unlockedBalance)) << "\n\n";

        cancel();

        return;
    }
    else if (error == TOO_MANY_INPUTS_TO_FIT_IN_BLOCK)
    {
        std::cout << WarningMsg("Your transaction is too large to fit in a block.\n\n")
                  << InformationMsg("This usually means you have many small inputs that cannot be\n"
                                    "combined into a single transaction within the block size limit.\n\n")
                  << "Use the " << SuccessMsg("sweep") << InformationMsg(" command to send this amount across\n")
                  << InformationMsg("multiple transactions automatically, without needing to optimize first.\n\n")
                  << InformationMsg("Example: ") << SuccessMsg("sweep") << InformationMsg(" — then enter the destination address and amount.\n");

        cancel();

        return;
    }

    if (error)
    {
        std::cout << WarningMsg("Failed to send transaction: ") << WarningMsg(error) << std::endl;
        return;
    }

    /* Figure out the actual amount if we're performing a send_all now we have
     * the fee worked out. */
    const uint64_t actualAmount = sendAll
        ? unlockedBalance - nodeFee - preparedTransaction.fee
        : amount;

    if (!confirmTransaction(walletBackend, destination, actualAmount, effectivePaymentID, nodeFee, preparedTransaction.fee))
    {
        cancel();
        return;
    }

    Crypto::Hash hash;

    std::tie(error, hash) = walletBackend->sendPreparedTransaction(preparedTransaction.transactionHash);

    if (error)
    {
        std::cout << WarningMsg("Failed to send transaction: ") << WarningMsg(error) << std::endl;
    }
    else
    {
        std::cout << SuccessMsg("Transaction has been sent!\nHash: ") << SuccessMsg(hash) << std::endl;

        if (Utilities::confirm("Watch transaction status now (pool -> block)?"))
        {
            watchTransactionUntilConfirmed(walletBackend, hash);
        }

        std::cout << InformationMsg(
                         "Note: recipients typically see incoming funds once a block confirms the transaction.")
                  << std::endl;
    }
}

bool confirmTransaction(
    const std::shared_ptr<WalletBackend> walletBackend,
    const std::string address,
    const uint64_t amount,
    const std::string paymentID,
    const uint64_t nodeFee,
    const uint64_t fee)
{
    std::cout << InformationMsg("\nConfirm Transaction?\n");

    const uint64_t totalAmount = amount + fee + nodeFee;

    std::cout << "You are sending " << SuccessMsg(Utilities::formatAmount(amount)) << ", with a network fee of "
              << SuccessMsg(Utilities::formatAmount(fee)) << ",\nand a node fee of "
              << SuccessMsg(Utilities::formatAmount(nodeFee))
              << ", for a total of " << SuccessMsg(Utilities::formatAmount(totalAmount));

    if (paymentID != "")
    {
        std::cout << ",\nand a Payment ID of " << SuccessMsg(paymentID);
    }
    else
    {
        std::cout << ".";
    }

    std::cout << "\n\nFROM: " << SuccessMsg(walletBackend->getWalletLocation()) << "\nTO: " << SuccessMsg(address)
              << "\n\n";

    std::cout << InformationMsg("Estimated minimum spendable delay after confirmation: ")
              << SuccessMsg(CryptoNote::parameters::MINIMUM_UNLOCK_TIME_BLOCKS) << InformationMsg(" blocks")
              << std::endl;

    if (Utilities::confirm("Is this correct?"))
    {
        /* Use default message */
        ZedUtilities::confirmPassword(walletBackend, "Confirm your password: ");
        return true;
    }

    return false;
}

void sweep(const std::shared_ptr<WalletBackend> walletBackend, const bool sweepAll)
{
    std::cout << InformationMsg("Note: You can type cancel at any time to "
                                "cancel the sweep\n\n");

    if (walletBackend->isViewWallet())
    {
        std::cout << WarningMsg("Sweep is not available for view-only wallets.\n");
        return;
    }

    const bool integratedAddressesAllowed(true), cancelAllowed(true);

    const auto [nodeFee, nodeAddress] = walletBackend->getNodeFee();
    const uint64_t unlockedBalance = walletBackend->getTotalUnlockedBalance();

    if (unlockedBalance == 0)
    {
        std::cout << WarningMsg("You have no unlocked balance to sweep.\n");
        return;
    }

    std::string address =
        getAddress("What address do you want to sweep to?: ", integratedAddressesAllowed, cancelAllowed);

    if (address == "cancel")
    {
        std::cout << WarningMsg("Cancelling sweep.\n");
        return;
    }

    std::cout << InformationMsg("Address type: ") << SuccessMsg(getAddressTypeLabel(address)) << "\n\n";

    std::string paymentID;

    if (!Utilities::isIntegratedAddress(address))
    {
        paymentID = getPaymentID(
            "What payment ID do you want to use?\n"
            "These are usually used for sending to exchanges.",
            cancelAllowed);

        if (paymentID == "cancel")
        {
            std::cout << WarningMsg("Cancelling sweep.\n");
            return;
        }

        std::cout << "\n";
    }
    else
    {
        std::cout << InformationMsg("Integrated address detected. Payment ID is embedded; skipping payment ID prompt.")
                  << "\n\n";
    }

    uint64_t amountToSweep = 0; /* 0 = sweep all */

    if (!sweepAll)
    {
        bool success;
        std::tie(success, amountToSweep) =
            getAmountToAtomic("How much " + WalletConfig::ticker + " do you want to sweep?: ", cancelAllowed);

        std::cout << "\n";

        if (!success)
        {
            std::cout << WarningMsg("Cancelling sweep.\n");
            return;
        }

        if (amountToSweep > unlockedBalance)
        {
            std::cout << WarningMsg("Amount exceeds unlocked balance of ")
                      << SuccessMsg(Utilities::formatAmount(unlockedBalance)) << "\n";
            return;
        }
    }

    const std::string sweepDesc = sweepAll
        ? "entire unlocked balance (" + Utilities::formatAmount(unlockedBalance) + ")"
        : Utilities::formatAmount(amountToSweep);

    std::cout << InformationMsg("\nSweep Summary\n");
    std::cout << "Sweeping: " << SuccessMsg(sweepDesc) << "\n";
    std::cout << "To:       " << SuccessMsg(address) << "\n";
    if (!paymentID.empty())
    {
        std::cout << "Payment ID: " << SuccessMsg(paymentID) << "\n";
    }
    std::cout << "\n"
              << InformationMsg("This may send multiple transactions to cover all inputs.\n")
              << InformationMsg("Each transaction incurs its own network fee.\n\n");

    if (!Utilities::confirm("Proceed with sweep?"))
    {
        std::cout << WarningMsg("Cancelling sweep.\n");
        return;
    }

    ZedUtilities::confirmPassword(walletBackend, "Confirm your password: ");

    std::cout << "\n" << InformationMsg("Sending sweep transactions...\n\n");

    const auto results = walletBackend->sweepToAddress(address, paymentID, amountToSweep);

    uint64_t successCount = 0;
    uint64_t failCount = 0;

    for (size_t i = 0; i < results.size(); i++)
    {
        const auto &[err, txHash] = results[i];

        std::cout << InformationMsg("Batch ") << SuccessMsg(i + 1) << InformationMsg("/") << SuccessMsg(results.size())
                  << ": ";

        if (err)
        {
            std::cout << WarningMsg("Failed — ") << WarningMsg(err) << "\n";
            failCount++;
        }
        else
        {
            std::cout << SuccessMsg("Sent — Hash: ") << SuccessMsg(txHash) << "\n";
            successCount++;
        }
    }

    std::cout << "\n";

    if (successCount > 0 && failCount == 0)
    {
        std::cout << SuccessMsg("Sweep complete. ") << SuccessMsg(successCount)
                  << SuccessMsg(" transaction(s) sent successfully.\n");
    }
    else if (successCount > 0)
    {
        std::cout << WarningMsg("Sweep partially complete. ") << SuccessMsg(successCount)
                  << InformationMsg(" sent, ") << WarningMsg(failCount) << WarningMsg(" failed.\n");
    }
    else
    {
        std::cout << WarningMsg("Sweep failed. No transactions were sent successfully.\n");
    }
}
