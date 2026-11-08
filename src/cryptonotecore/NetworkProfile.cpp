// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#include "NetworkProfile.h"

#include <optional>
#include <system_error>

namespace CryptoNote
{
    namespace
    {
        const std::string NETWORK_PROFILE_KEY = "network_profile";

        const std::string NETWORK_SIMNET = "simnet";

        /* Written by DatabaseBlockchainCache the first time a database is
           opened, so its absence means the database is brand new. Must match
           DB_VERSION_KEY in DatabaseBlockchainCache.cpp. */
        const std::string DB_SCHEME_VERSION_KEY = "db_scheme_version";

        class SettingReadBatch : public IReadBatch
        {
          public:
            explicit SettingReadBatch(std::string key): m_key(std::move(key)) {}

            std::vector<std::string> getRawKeys() const override
            {
                return {m_key};
            }

            void submitRawResult(const std::vector<std::string> &values, const std::vector<bool> &resultStates) override
            {
                if (values.size() == 1 && resultStates.size() == 1 && resultStates[0])
                {
                    m_value = values[0];
                }
            }

            std::optional<std::string> value() const
            {
                return m_value;
            }

          private:
            std::string m_key;

            std::optional<std::string> m_value;
        };

        class SettingWriteBatch : public IWriteBatch
        {
          public:
            SettingWriteBatch(std::string key, std::string value): m_key(std::move(key)), m_value(std::move(value)) {}

            std::vector<std::pair<std::string, std::string>> extractRawDataToInsert() override
            {
                return {std::make_pair(m_key, m_value)};
            }

            std::vector<std::string> extractRawKeysToRemove() override
            {
                return {};
            }

          private:
            std::string m_key;

            std::string m_value;
        };

        std::optional<std::string> readSetting(IDataBase &database, const std::string &key)
        {
            SettingReadBatch batch(key);

            if (const auto error = database.read(batch))
            {
                throw std::system_error(error);
            }

            return batch.value();
        }
    } // namespace

    std::string settleNetworkProfile(IDataBase &database, const bool simnet)
    {
        const auto recorded = readSetting(database, NETWORK_PROFILE_KEY);

        if (recorded && *recorded != NETWORK_SIMNET)
        {
            return "This database records a network this build does not know ('" + *recorded
                   + "'). Refusing to start rather than guess which chain it holds.";
        }

        const bool recordedSimnet = recorded.has_value();

        if (recordedSimnet == simnet)
        {
            return "";
        }

        if (recordedSimnet)
        {
            return "This database belongs to a simnet, whose blocks carry no proof of work; it can never be opened "
                   "as mainnet. Pass --simnet, or point --data-dir at a mainnet database.";
        }

        /* Asked for a simnet, nothing recorded: only a database that has never
           held a chain may become one. */
        if (readSetting(database, DB_SCHEME_VERSION_KEY).has_value())
        {
            return "This database holds a mainnet chain and cannot be opened as a simnet. Start the simnet in an "
                   "empty --data-dir.";
        }

        SettingWriteBatch batch(NETWORK_PROFILE_KEY, NETWORK_SIMNET);

        if (const auto error = database.write(batch))
        {
            throw std::system_error(error);
        }

        return "";
    }
} // namespace CryptoNote
