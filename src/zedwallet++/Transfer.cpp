// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

/////////////////////////////////
#include <zedwallet++/Transfer.h>
/////////////////////////////////

#include <config/WalletConfig.h>
#include <iostream>
#include <utilities/ColouredMsg.h>
#include <utilities/FormatTools.h>
#include <utilities/Input.h>
#include <zedwallet++/Fusion.h>
#include <zedwallet++/GetInput.h>
#include <zedwallet++/Utilities.h>

namespace
{
    void cancel()
    {
        std::cout << WarningMsg("Cancelling transaction.\n");
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

    std::cout << "\n";

    std::string paymentID;

    if (address.length() == WalletConfig::standardAddressLength)
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
        address,
        amount,
        paymentID,
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
        std::cout << WarningMsg("Your transaction is too large to be accepted "
                                "by the network!\n")
                  << InformationMsg("We're attempting to optimize your wallet,\n"
                                    "which hopefully will make the transaction small "
                                    "enough to fit in a block.\n"
                                    "Please wait, this will take some time...\n\n");

        /* Try and perform some fusion transactions to make our inputs bigger */
        optimize(walletBackend);

        /* Resend the transaction */
        std::tie(error, std::ignore, preparedTransaction) = walletBackend->sendTransactionBasic(
            address,
            amount,
            paymentID,
            sendAll,
            false /* Don't relay to network */
        );

        /* Still too big, split it up (with users approval) */
        if (error == TOO_MANY_INPUTS_TO_FIT_IN_BLOCK)
        {
            std::cout << WarningMsg(
                "Your transaction is still too large to be accepted "
                "by the network. Try splitting your transaction up into smaller "
                "amounts.");

            cancel();

            return;
        }
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

    if (!confirmTransaction(walletBackend, address, actualAmount, paymentID, nodeFee, preparedTransaction.fee))
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

    if (Utilities::confirm("Is this correct?"))
    {
        /* Use default message */
        ZedUtilities::confirmPassword(walletBackend, "Confirm your password: ");
        return true;
    }

    return false;
}
