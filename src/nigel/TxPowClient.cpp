// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#include "TxPowClient.h"

#include "httplib.h"
#include "json.hpp"

#include <common/StringTools.h>
#include <logger/Logger.h>

#include <cctype>
#include <chrono>
#include <cstdlib>
#include <mutex>
#include <optional>
#include <thread>

#if defined(__EMSCRIPTEN__)
/* Defined in Nigel.cpp: a synchronous XMLHttpRequest that works from the
   WASM worker thread, where sockets do not exist. */
extern "C" char *wrkzSyncXhr(
    const char *url,
    const char *method,
    const char *body,
    int body_len,
    int *out_status,
    int *out_body_len);
#endif

namespace
{
    std::mutex g_settingsMutex;

    TxPowClient::Settings g_settings;

    /* One long-poll on the server. The server caps this at its own maximum,
       so asking for more than it allows only shortens the wait. */
    constexpr int WAIT_MS_PER_REQUEST = 20000;

    constexpr int CONNECT_TIMEOUT_SECONDS = 5;

    /* Pause between polls when the server answers "pending" immediately, so
       a server with long polling disabled is not hammered. */
    constexpr int POLL_PAUSE_MS = 500;

    struct HttpReply
    {
        /* 0 means the request never reached the server. */
        int status = 0;

        std::string body;
    };

    std::string baseUrl(const TxPowClient::Settings &s)
    {
        std::string host = s.host;

        /* A bare IPv6 literal needs brackets inside a URL. */
        if (host.find(':') != std::string::npos && host.front() != '[')
        {
            host = "[" + host + "]";
        }

        return std::string(s.ssl ? "https://" : "http://") + host + ":" + std::to_string(s.port) + s.basePath;
    }

    HttpReply request(
        const TxPowClient::Settings &s,
        const std::string &method,
        const std::string &path,
        const std::string *body,
        const int readTimeoutSeconds)
    {
        HttpReply reply;

#if defined(__EMSCRIPTEN__)
        (void)readTimeoutSeconds;

        const std::string url = baseUrl(s) + path;

        int status = 0;
        int length = 0;

        char *buffer = wrkzSyncXhr(
            url.c_str(),
            method.c_str(),
            body ? body->c_str() : nullptr,
            body ? static_cast<int>(body->size()) : 0,
            &status,
            &length);

        reply.status = status;

        if (buffer != nullptr)
        {
            if (length > 0)
            {
                reply.body.assign(buffer, static_cast<size_t>(length));
            }

            std::free(buffer);
        }
#else
        httplib::Client client(baseUrl(s));

        client.set_connection_timeout(CONNECT_TIMEOUT_SECONDS, 0);
        client.set_read_timeout(readTimeoutSeconds, 0);
        client.set_write_timeout(readTimeoutSeconds, 0);
        client.set_follow_location(false);

        httplib::Result result;

        if (method == "POST")
        {
            result = client.Post(path, body ? *body : std::string(), "application/json");
        }
        else if (method == "DELETE")
        {
            result = client.Delete(path);
        }
        else
        {
            result = client.Get(path);
        }

        if (result)
        {
            reply.status = result->status;
            reply.body = result->body;
        }
#endif

        return reply;
    }

    void logRemote(const std::string &message, const Logger::LogLevel level)
    {
        Logger::logger.log("Tx PoW server: " + message, level, {Logger::TRANSACTIONS});
    }

    /* Reads a finished or pending job reply. Returns true when the nonce was
       extracted, false when the job is still pending (jobId set) or failed
       (jobId cleared). */
    bool parseReply(
        const HttpReply &reply,
        std::string &jobId,
        std::array<uint8_t, CryptoNote::TX_POW_NONCE_SIZE> &nonce)
    {
        if (reply.status == 0)
        {
            logRemote("no response", Logger::WARNING);
            jobId.clear();
            return false;
        }

        nlohmann::json j;

        try
        {
            j = nlohmann::json::parse(reply.body);
        }
        catch (const std::exception &)
        {
            logRemote("could not parse reply (HTTP " + std::to_string(reply.status) + ")", Logger::WARNING);
            jobId.clear();
            return false;
        }

        const std::string status = j.value("status", std::string());

        if (reply.status != 200 || status == "error")
        {
            logRemote(
                "refused the job (HTTP " + std::to_string(reply.status) + "): " + j.value("error", std::string("no reason given")),
                Logger::WARNING);
            jobId.clear();
            return false;
        }

        if (j.contains("job_id") && j["job_id"].is_string())
        {
            jobId = j["job_id"].get<std::string>();
        }

        if (status == "pending")
        {
            return false;
        }

        if (status != "done")
        {
            logRemote("job ended with status '" + status + "'", Logger::WARNING);
            jobId.clear();
            return false;
        }

        const std::string nonceHex = j.value("nonce", std::string());

        std::vector<uint8_t> nonceBytes;

        if (!Common::fromHex(nonceHex, nonceBytes) || nonceBytes.size() != nonce.size())
        {
            logRemote("returned a malformed nonce", Logger::WARNING);
            jobId.clear();
            return false;
        }

        std::copy(nonceBytes.begin(), nonceBytes.end(), nonce.begin());

        return true;
    }

    bool solveRemotely(
        const TxPowClient::Settings &s,
        const std::vector<uint8_t> &prefix,
        const uint64_t difficulty,
        std::array<uint8_t, CryptoNote::TX_POW_NONCE_SIZE> &nonce)
    {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(s.timeoutSeconds);

        logRemote(
            "asking " + baseUrl(s) + " for a nonce at difficulty " + std::to_string(difficulty), Logger::INFO);

        nlohmann::json submit = {{"prefix", Common::toHex(prefix)}, {"wait_ms", WAIT_MS_PER_REQUEST}};

        const std::string submitBody = submit.dump();

        const int readTimeout = WAIT_MS_PER_REQUEST / 1000 + 15;

        std::string jobId;

        if (parseReply(request(s, "POST", "/pow", &submitBody, readTimeout), jobId, nonce))
        {
            logRemote("nonce received", Logger::INFO);
            return true;
        }

        if (jobId.empty())
        {
            return false;
        }

        while (std::chrono::steady_clock::now() < deadline)
        {
            const auto pollStart = std::chrono::steady_clock::now();

            const auto reply = request(
                s, "GET", "/pow/" + jobId + "?wait_ms=" + std::to_string(WAIT_MS_PER_REQUEST), nullptr, readTimeout);

            std::string pollJobId = jobId;

            if (parseReply(reply, pollJobId, nonce))
            {
                logRemote("nonce received", Logger::INFO);
                return true;
            }

            if (pollJobId.empty())
            {
                return false;
            }

            /* The server answered "pending" at once rather than holding the
               request; do not spin. */
            if (std::chrono::steady_clock::now() - pollStart < std::chrono::milliseconds(POLL_PAUSE_MS))
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(POLL_PAUSE_MS));
            }
        }

        logRemote(
            "gave no answer within " + std::to_string(s.timeoutSeconds) + " seconds, computing locally instead",
            Logger::WARNING);

#if !defined(__EMSCRIPTEN__)
        /* Best effort: free the server's queue slot. */
        request(s, "DELETE", "/pow/" + jobId, nullptr, CONNECT_TIMEOUT_SECONDS);
#endif

        return false;
    }
} // namespace

namespace TxPowClient
{
    void configure(const std::string &hostIn, const uint16_t port, const bool ssl)
    {
        std::string host = hostIn;
        bool useSsl = ssl;
        std::string basePath;

        const auto trim = [](std::string &s) {
            while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())))
            {
                s.pop_back();
            }
            while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front())))
            {
                s.erase(s.begin());
            }
        };

        trim(host);

        /* Accept a pasted URL: an explicit scheme decides SSL, and a path is
           the prefix a reverse proxy mounts the server under. */
        if (host.rfind("https://", 0) == 0)
        {
            host.erase(0, 8);
            useSsl = true;
        }
        else if (host.rfind("http://", 0) == 0)
        {
            host.erase(0, 7);
            useSsl = false;
        }

        const auto slash = host.find('/');

        if (slash != std::string::npos)
        {
            basePath = host.substr(slash);
            host.erase(slash);
        }

        while (!basePath.empty() && basePath.back() == '/')
        {
            basePath.pop_back();
        }

        std::lock_guard<std::mutex> lock(g_settingsMutex);

        g_settings.host = host;
        g_settings.port = port;
        g_settings.ssl = useSsl;
        g_settings.basePath = basePath;
    }

    Settings settings()
    {
        std::lock_guard<std::mutex> lock(g_settingsMutex);

        return g_settings;
    }

    bool configured()
    {
        const Settings s = settings();

        return !s.host.empty() && s.port != 0;
    }

    CryptoNote::RemotePoWSolver solver()
    {
        const Settings s = settings();

        if (s.host.empty() || s.port == 0)
        {
            return {};
        }

#if !defined(CPPHTTPLIB_OPENSSL_SUPPORT) && !defined(__EMSCRIPTEN__)
        if (s.ssl)
        {
            logRemote("SSL requested but this build has no SSL support, computing locally instead", Logger::WARNING);
            return {};
        }
#endif

        return [s](const std::vector<uint8_t> &prefix,
                   const uint64_t difficulty,
                   std::array<uint8_t, CryptoNote::TX_POW_NONCE_SIZE> &nonce) {
            return solveRemotely(s, prefix, difficulty, nonce);
        };
    }
} // namespace TxPowClient
