// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

/////////////////////////////////
#include <zedwallet++/Commands.h>
/////////////////////////////////

#include <config/WalletConfig.h>
#include <utilities/Container.h>

std::vector<Command> startupCommands()
{
    return {
        Command("open", "Open a wallet already on your system"),
        Command("create", "Create a new wallet"),
        Command("seed_restore", "Restore a wallet using a seed phrase of words"),
        Command("key_restore", "Restore a wallet using a view and spend key"),
        Command("view_wallet", "Import a view only wallet"),
        Command("exit", "Exit the program"),
    };
}

std::vector<Command> nodeDownCommands()
{
    return {
        Command("try_again", "Try to connect to the node again"),
        Command("continue", "Continue to the wallet interface regardless"),
        Command("swap_node", "Specify a new daemon address/port to connect to"),
        Command("exit", "Exit the program"),
    };
}

std::vector<AdvancedCommand> allCommands()
{
    return {
        AdvancedCommand("help", "[General] List all commands", true, false),
        AdvancedCommand("advanced", "[General] Alias for help (list all commands)", true, false),
        AdvancedCommand("exit", "[General] Exit and save your wallet", true, false),
        AdvancedCommand("status", "[Wallet/Network Info] Display sync status and network hashrate", true, true),
        AdvancedCommand("refresh", "[Wallet/Network Info] Retry syncing with daemon now", true, false),
        AdvancedCommand("swap_node", "[Wallet/Network Info] Specify a new daemon address/port to sync from", true, true),
        AdvancedCommand("address", "[Wallet Info] Display your payment address", true, false),
        AdvancedCommand("addr", "[Wallet Info] Alias for address", true, true),
        AdvancedCommand("balance", "[Wallet Info] Display how much " + WalletConfig::ticker + " you have", true, false),
        AdvancedCommand("bal", "[Wallet Info] Alias for balance", true, true),
        AdvancedCommand("incoming_transfers", "[Transactions] Show incoming transfers", true, true),
        AdvancedCommand("in", "[Transactions] Alias for incoming_transfers", true, true),
        AdvancedCommand("outgoing_transfers", "[Transactions] Show outgoing transfers", false, true),
        AdvancedCommand("out", "[Transactions] Alias for outgoing_transfers", false, true),
        AdvancedCommand("list_transfers", "[Transactions] Show all transfers", false, true),
        AdvancedCommand("txs", "[Transactions] Show all transfers in one-line format", false, true),
        AdvancedCommand("txs_full", "[Transactions] Show all transfers with full details", false, true),
        AdvancedCommand("transfer", "[Transactions] Send " + WalletConfig::ticker + " to someone", false, false),
        AdvancedCommand("ab_send", "[Transactions / Address Book] Send " + WalletConfig::ticker + " to someone in your address book", false, true),
        AdvancedCommand("send_all", "[Transactions] Send all your balance to someone", false, true),
        AdvancedCommand("get_tx_private_key", "[Transactions] Get the private key of a transaction", true, true),
        AdvancedCommand("ab_add", "[Address Book] Add a person to your address book", true, true),
        AdvancedCommand("ab_delete", "[Address Book] Delete a person in your address book", true, true),
        AdvancedCommand("ab_list", "[Address Book] List everyone in your address book", true, true),
        AdvancedCommand("make_integrated_address", "[Address / Payment Tools] Make a combined address + payment ID", true, true),
        AdvancedCommand("backup", "[Security & Recovery] Backup your private keys and/or seed", true, false),
        AdvancedCommand("change_password", "[Security] Change your wallet password", true, true),
        AdvancedCommand("save", "[Maintenance] Save your wallet state", true, true),
        AdvancedCommand("save_csv", "[Export] Save all wallet transactions to a CSV file", true, true),
        AdvancedCommand("optimize", "[Maintenance] Optimize your wallet to send large amounts", false, true),
        AdvancedCommand("reset", "[Maintenance] Recheck the chain from zero for transactions", true, true),
        AdvancedCommand("set_log_level", "[Maintenance] Alter the logging level", true, true),
    };
}

std::vector<AdvancedCommand> basicCommands()
{
    return Utilities::filter(allCommands(), [](AdvancedCommand c) { return !c.advanced; });
}

std::vector<AdvancedCommand> advancedCommands()
{
    return Utilities::filter(allCommands(), [](AdvancedCommand c) { return c.advanced; });
}

std::vector<AdvancedCommand> basicViewWalletCommands()
{
    return Utilities::filter(basicCommands(), [](AdvancedCommand c) { return c.viewWalletSupport; });
}

std::vector<AdvancedCommand> advancedViewWalletCommands()
{
    return Utilities::filter(advancedCommands(), [](AdvancedCommand c) { return c.viewWalletSupport; });
}

std::vector<AdvancedCommand> allViewWalletCommands()
{
    return Utilities::filter(allCommands(), [](AdvancedCommand c) { return c.viewWalletSupport; });
}
