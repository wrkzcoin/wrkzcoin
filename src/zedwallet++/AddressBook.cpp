// Copyright (c) 2018-2019, The TurtleCoin Developers
// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

////////////////////////////////////
#include <cstdio>
#include <zedwallet++/AddressBook.h>
////////////////////////////////////

#include <config/WalletConfig.h>
#include <errors/ValidateParameters.h>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <utilities/Addresses.h>
#include <utilities/ColouredMsg.h>
#include <utilities/Input.h>
#include <utilities/String.h>
#include <zedwallet++/GetInput.h>
#include <zedwallet++/Transfer.h>
#include <zedwallet++/Utilities.h>

const std::string getAddressBookName(const std::vector<AddressBookEntry> addressBook)
{
    while (true)
    {
        std::string friendlyName;

        std::cout << InformationMsg("What friendly name do you want to ")
                  << InformationMsg("give this address book entry?: ");

        std::getline(std::cin, friendlyName);

        Utilities::trim(friendlyName);

        if (friendlyName.empty())
        {
            std::cout << WarningMsg("Friendly name cannot be empty.") << std::endl << std::endl;
            continue;
        }

        const auto it = std::find(addressBook.begin(), addressBook.end(), AddressBookEntry(friendlyName));

        if (it != addressBook.end())
        {
            std::cout << WarningMsg("An address book entry with this ") << WarningMsg("name already exists!")
                      << std::endl
                      << std::endl;

            continue;
        }

        return friendlyName;
    }
}

void addToAddressBook()
{
    std::cout << InformationMsg("Note: You can type cancel at any time to "
                                "cancel adding someone to your address book.")
              << std::endl
              << std::endl;

    auto addressBook = getAddressBook();

    const std::string friendlyName = getAddressBookName(addressBook);

    if (friendlyName == "cancel")
    {
        std::cout << WarningMsg("Cancelling addition to address book.") << std::endl;
        return;
    }

    const bool integratedAddressesAllowed(true), cancelAllowed(true);

    const std::string address =
        getAddress("\nWhat address does this user have?: ", integratedAddressesAllowed, cancelAllowed);

    if (address == "cancel")
    {
        std::cout << WarningMsg("Cancelling addition to address book.") << std::endl;
        return;
    }

    if (!Utilities::isIntegratedAddress(address))
    {
        std::cout << InformationMsg("Address type: ") << SuccessMsg("standard") << std::endl;
    }
    else if (address.length() == WalletConfig::integratedAddressLength)
    {
        std::cout << InformationMsg("Address type: ") << SuccessMsg("integrated-short") << std::endl;
    }
    else
    {
        std::cout << InformationMsg("Address type: ") << SuccessMsg("integrated-long") << std::endl;
    }

    std::string paymentID;

    /* Don't prompt for a payment ID if we have an integrated address */
    if (!Utilities::isIntegratedAddress(address))
    {
        const bool cancelAllowed = true;

        paymentID = getPaymentID(
            "\nDoes this address book entry have a payment ID associated "
            "with it?\n",
            cancelAllowed);

        if (paymentID == "cancel")
        {
            std::cout << WarningMsg("Cancelling addition to address book.") << std::endl;

            return;
        }
    }
    else
    {
        std::cout << InformationMsg("Integrated address detected. Payment ID is already embedded.") << std::endl;
    }

    addressBook.emplace_back(friendlyName, address, paymentID);

    if (saveAddressBook(addressBook))
    {
        std::cout << SuccessMsg("\nA new entry has been added to your address "
                                "book!\n");
    }
}

const std::tuple<bool, AddressBookEntry> getAddressBookEntry(const std::vector<AddressBookEntry> addressBook)
{
    while (true)
    {
        std::string friendlyName;

        std::cout << InformationMsg("Who do you want to send to from your ") << InformationMsg("address book?: ");

        std::getline(std::cin, friendlyName);

        Utilities::trim(friendlyName);

        /* \n == no-op */
        if (friendlyName == "")
        {
            continue;
        }

        if (friendlyName == "cancel")
        {
            return {true, AddressBookEntry()};
        }

        try
        {
            const int selectionNum = std::stoi(friendlyName) - 1;

            const int numCommands = static_cast<int>(addressBook.size());

            if (selectionNum < 0 || selectionNum >= numCommands)
            {
                std::cout << WarningMsg("Bad input, expected a friendly name, ") << WarningMsg("or number from ")
                          << InformationMsg("1") << WarningMsg(" to ") << InformationMsg(numCommands) << "\n\n";

                continue;
            }

            return {false, addressBook[selectionNum]};
        }
        catch (const std::out_of_range &)
        {
            const int numCommands = static_cast<int>(addressBook.size());

            std::cout << WarningMsg("Bad input, expected a friendly name, ") << WarningMsg("or number from ")
                      << InformationMsg("1") << WarningMsg(" to ") << InformationMsg(numCommands) << "\n\n";

            continue;
        }
        /* Input isn't a number */
        catch (const std::invalid_argument &)
        {
            const auto it = std::find(addressBook.begin(), addressBook.end(), AddressBookEntry(friendlyName));

            if (it != addressBook.end())
            {
                return {false, *it};
            }

            std::cout << std::endl
                      << WarningMsg("Could not find a user with the name of ") << InformationMsg(friendlyName)
                      << WarningMsg(" in your address book!") << std::endl
                      << std::endl;
        }

        const bool list = Utilities::confirm("Would you like to list everyone in your address book?");

        std::cout << "\n";

        if (list)
        {
            listAddressBook();
        }
    }
}

void sendFromAddressBook(const std::shared_ptr<WalletBackend> walletBackend)
{
    auto addressBook = getAddressBook();

    if (isAddressBookEmpty(addressBook))
    {
        return;
    }

    std::cout << InformationMsg("Note: You can type cancel at any time to ")
              << InformationMsg("cancel the transaction\n\n");

    const auto [cancel, addressBookEntry] = getAddressBookEntry(addressBook);

    if (cancel)
    {
        std::cout << WarningMsg("Cancelling transaction.\n");
        return;
    }

    const bool cancelAllowed = true;

    const auto [success, amount] =
        getAmountToAtomic("How much " + WalletConfig::ticker + " do you want to send?: ", cancelAllowed);

    if (!success)
    {
        std::cout << WarningMsg("Cancelling transaction.\n");
        return;
    }

    sendTransaction(walletBackend, addressBookEntry.address, amount, addressBookEntry.paymentID);
}

bool isAddressBookEmpty(const std::vector<AddressBookEntry> addressBook)
{
    if (addressBook.empty())
    {
        std::cout << WarningMsg("Your address book is empty! Add some people ") << WarningMsg("to it first.")
                  << std::endl;

        return true;
    }

    return false;
}

void deleteFromAddressBook()
{
    auto addressBook = getAddressBook();

    if (isAddressBookEmpty(addressBook))
    {
        return;
    }

    while (true)
    {
        std::cout << InformationMsg("Note: You can type cancel at any time ")
                  << InformationMsg("to cancel the deletion.\n\n");

        std::string friendlyName;

        std::cout << InformationMsg("What address book entry do you want to ") << InformationMsg("delete?: ");

        std::getline(std::cin, friendlyName);

        Utilities::trim(friendlyName);

        if (friendlyName == "cancel")
        {
            std::cout << WarningMsg("Cancelling deletion.\n");
            return;
        }

        const auto it = std::remove(addressBook.begin(), addressBook.end(), AddressBookEntry(friendlyName));

        if (it != addressBook.end())
        {
            addressBook.erase(it, addressBook.end());

            if (saveAddressBook(addressBook))
            {
                std::cout << std::endl
                          << SuccessMsg("This entry has been deleted from ") << SuccessMsg("your address book!")
                          << std::endl;
            }

            return;
        }

        std::cout << WarningMsg("\nCould not find a user with the name of ") << InformationMsg(friendlyName)
                  << WarningMsg(" in your address book!\n\n");

        const bool list = Utilities::confirm("Would you like to list everyone in your address book?");

        std::cout << "\n";

        if (list)
        {
            listAddressBook();
        }
    }
}

void listAddressBook()
{
    const std::vector<AddressBookEntry> addressBook = getAddressBook();

    if (isAddressBookEmpty(addressBook))
    {
        return;
    }

    size_t i = 1;

    for (const auto &entry : addressBook)
    {
        std::cout << InformationMsg("Address Book Entry: ") << InformationMsg(i) << InformationMsg(" | ")
                  << SuccessMsg(entry.friendlyName) << "\n"
                  << InformationMsg("Address: ") << SuccessMsg(entry.address) << "\n";

        if (entry.paymentID != "")
        {
            std::cout << InformationMsg("Payment ID: ") << SuccessMsg(entry.paymentID) << "\n\n";
        }
        else
        {
            std::cout << "\n";
        }

        i++;
    }
}

std::vector<AddressBookEntry> getAddressBook()
{
    std::vector<AddressBookEntry> addressBook;

    std::ifstream input(WalletConfig::addressBookFilename);

    /* If file exists, read current values */
    if (input)
    {
        nlohmann::json j;
        try
        {
            j = nlohmann::json::parse(input);
        }
        catch (const nlohmann::json::parse_error &)
        {
            std::cout << WarningMsg("Failed to parse address book JSON. Using empty address book.") << std::endl;
            return addressBook;
        }

        if (!j.is_array())
        {
            std::cout << WarningMsg("Address book file has invalid format. Using empty address book.") << std::endl;
            return addressBook;
        }

        for (auto &v : j)
        {
            if (!v.is_object())
            {
                std::cout << WarningMsg("Skipping invalid address book entry (expected JSON object).") << std::endl;
                continue;
            }

            try
            {
                AddressBookEntry entry;
                entry.fromJSON(v);
                addressBook.push_back(entry);
            }
            catch (const std::exception &)
            {
                std::cout << WarningMsg("Skipping malformed address book entry.") << std::endl;
            }
        }
    }

    return addressBook;
}

bool saveAddressBook(const std::vector<AddressBookEntry> addressBook)
{
    std::ofstream output(WalletConfig::addressBookFilename);

    if (output)
    {
        nlohmann::json arr = nlohmann::json::array();
        for (auto &entry : addressBook)
        {
            arr.push_back(entry.toJSON());
        }
        output << std::setw(2) << arr;
    }
    else
    {
        std::cout << WarningMsg("Failed to save address book to disk!") << std::endl
                  << WarningMsg("Check you are able to write files to your ") << WarningMsg("current directory.")
                  << std::endl;

        return false;
    }

    output.close();

    return true;
}
