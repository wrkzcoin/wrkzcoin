// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#include "ZmqPublisher.h"

#include <common/CryptoNoteTools.h>
#include <algorithm>
#include <cerrno>
#include <cctype>
#include <sstream>
#include <system/InterruptedException.h>

#ifdef WRKZ_ENABLE_ZMQ
#include <zmq.h>
#endif

using namespace Logging;

namespace Daemon
{
    ZmqPublisher::ZmqPublisher(
        System::Dispatcher &dispatcher,
        CryptoNote::ICore &core,
        std::shared_ptr<Logging::ILogger> logger,
        std::string endpoint):
        m_dispatcher(dispatcher),
        m_core(core),
        m_logger(logger, "ZmqPublisher"),
        m_endpoint(std::move(endpoint)),
        m_queue(dispatcher),
        m_contextGroup(dispatcher),
        m_running(false),
        m_published(0),
        m_dropped(0)
#ifdef WRKZ_ENABLE_ZMQ
        ,
        m_zmqContext(nullptr),
        m_zmqSocket(nullptr)
#endif
    {
    }

    ZmqPublisher::~ZmqPublisher()
    {
        stop();
    }

    bool ZmqPublisher::start()
    {
        if (m_running)
        {
            return true;
        }

        if (m_endpoint.empty())
        {
            return false;
        }

#ifndef WRKZ_ENABLE_ZMQ
        m_logger(Logging::WARNING)
            << "ZMQ requested via --zmq-pub but this build was compiled without ZMQ support.";
        return false;
#else
        m_zmqContext = zmq_ctx_new();
        if (m_zmqContext == nullptr)
        {
            m_logger(Logging::WARNING) << "Failed to create ZMQ context: " << zmq_strerror(zmq_errno());
            return false;
        }

        m_zmqSocket = zmq_socket(m_zmqContext, ZMQ_PUB);
        if (m_zmqSocket == nullptr)
        {
            m_logger(Logging::WARNING) << "Failed to create ZMQ PUB socket: " << zmq_strerror(zmq_errno());
            zmq_ctx_term(m_zmqContext);
            m_zmqContext = nullptr;
            return false;
        }

        const int lingerMs = 0;
        zmq_setsockopt(m_zmqSocket, ZMQ_LINGER, &lingerMs, sizeof(lingerMs));

        if (zmq_bind(m_zmqSocket, m_endpoint.c_str()) != 0)
        {
            m_logger(Logging::WARNING) << "Failed to bind ZMQ endpoint " << m_endpoint << ": "
                                       << zmq_strerror(zmq_errno());
            zmq_close(m_zmqSocket);
            zmq_ctx_term(m_zmqContext);
            m_zmqSocket = nullptr;
            m_zmqContext = nullptr;
            return false;
        }

        if (isNonLoopbackTcpEndpoint(m_endpoint))
        {
            m_logger(Logging::WARNING) << "ZMQ PUB endpoint is non-loopback: " << m_endpoint
                                       << ". Ensure network-level access controls are in place.";
        }

        m_queueGuard.reset(new QueueGuard(m_core, m_queue));
        m_running = true;
        m_contextGroup.spawn([this] { consumeLoop(); });

        m_logger(Logging::INFO) << "ZMQ publisher started on " << m_endpoint;
        return true;
#endif
    }

    void ZmqPublisher::stop()
    {
        if (!m_running)
        {
            return;
        }

        m_running = false;
        m_queue.stop();
        m_contextGroup.interrupt();
        m_contextGroup.wait();
        m_queueGuard.reset();

#ifdef WRKZ_ENABLE_ZMQ
        if (m_zmqSocket != nullptr)
        {
            zmq_close(m_zmqSocket);
            m_zmqSocket = nullptr;
        }

        if (m_zmqContext != nullptr)
        {
            zmq_ctx_term(m_zmqContext);
            m_zmqContext = nullptr;
        }
#endif

        m_logger(Logging::INFO) << "ZMQ publisher stopped. Published=" << m_published << ", dropped=" << m_dropped;
    }

    void ZmqPublisher::consumeLoop()
    {
        while (true)
        {
            try
            {
                const CryptoNote::BlockchainMessage message = m_queue.front();
                m_queue.pop();
                publishMessage(message);
            }
            catch (const System::InterruptedException &)
            {
                break;
            }
            catch (const std::exception &e)
            {
                m_logger(Logging::WARNING) << "ZMQ publisher loop error: " << e.what();
            }
        }
    }

    void ZmqPublisher::publishMessage(const CryptoNote::BlockchainMessage &message)
    {
#ifndef WRKZ_ENABLE_ZMQ
        (void)message;
        return;
#else
        message.match(
            [this](const CryptoNote::Messages::NewBlock &m) {
                std::ostringstream body;
                body << "{\"height\":" << m.blockIndex << ",\"hash\":\"" << hashToString(m.blockHash) << "\"}";
                sendMultipart("hashblock", body.str());

                try
                {
                    const auto block = m_core.getBlockByHash(m.blockHash);
                    std::vector<Crypto::Hash> transactionHashes;
                    transactionHashes.reserve(block.transactionHashes.size() + 1);
                    transactionHashes.push_back(CryptoNote::getObjectHash(block.baseTransaction));
                    transactionHashes.insert(
                        transactionHashes.end(), block.transactionHashes.begin(), block.transactionHashes.end());

                    std::ostringstream prefetchBody;
                    prefetchBody << "{\"height\":" << m.blockIndex << ",\"hash\":\"" << hashToString(m.blockHash)
                                 << "\",\"transaction_hashes\":" << hashesToJsonArray(transactionHashes) << "}";
                    sendMultipart("chain_main", prefetchBody.str());
                }
                catch (const std::exception &e)
                {
                    m_logger(Logging::WARNING) << "Failed to build chain_main prefetch payload for block "
                                               << hashToString(m.blockHash) << ": " << e.what();
                }
            },
            [this](const CryptoNote::Messages::NewAlternativeBlock &m) {
                std::ostringstream body;
                body << "{\"height\":" << m.blockIndex << ",\"hash\":\"" << hashToString(m.blockHash) << "\"}";
                sendMultipart("hashblock_alt", body.str());
            },
            [this](const CryptoNote::Messages::ChainSwitch &m) {
                std::ostringstream body;
                body << "{\"common_root_height\":" << m.commonRootIndex << ",\"hashes\":"
                     << hashesToJsonArray(m.blocksFromCommonRoot) << "}";
                sendMultipart("chainswitch", body.str());
            },
            [this](const CryptoNote::Messages::AddTransaction &m) {
                std::ostringstream body;
                body << "{\"hashes\":" << hashesToJsonArray(m.hashes) << "}";
                sendMultipart("txpool_add", body.str());
            },
            [this](const CryptoNote::Messages::DeleteTransaction &m) {
                std::ostringstream body;
                body << "{\"hashes\":" << hashesToJsonArray(m.hashes) << ",\"reason\":\""
                     << deleteReasonToString(m.reason) << "\"}";
                sendMultipart("txpool_del", body.str());
            });
#endif
    }

    bool ZmqPublisher::sendMultipart(const std::string &topic, const std::string &payload)
    {
#ifndef WRKZ_ENABLE_ZMQ
        (void)topic;
        (void)payload;
        return false;
#else
        if (m_zmqSocket == nullptr)
        {
            return false;
        }

        if (zmq_send(m_zmqSocket, topic.data(), topic.size(), ZMQ_DONTWAIT | ZMQ_SNDMORE) == -1)
        {
            ++m_dropped;
            if (zmq_errno() != EAGAIN)
            {
                m_logger(Logging::WARNING) << "ZMQ send failed on topic " << topic << ": "
                                           << zmq_strerror(zmq_errno());
            }
            return false;
        }

        if (zmq_send(m_zmqSocket, payload.data(), payload.size(), ZMQ_DONTWAIT) == -1)
        {
            ++m_dropped;
            if (zmq_errno() != EAGAIN)
            {
                m_logger(Logging::WARNING) << "ZMQ send failed on topic " << topic << ": "
                                           << zmq_strerror(zmq_errno());
            }
            return false;
        }

        ++m_published;
        return true;
#endif
    }

    std::string ZmqPublisher::hashToString(const Crypto::Hash &hash)
    {
        std::ostringstream out;
        out << hash;
        return out.str();
    }

    std::string ZmqPublisher::hashesToJsonArray(const std::vector<Crypto::Hash> &hashes)
    {
        std::ostringstream out;
        out << "[";
        for (size_t i = 0; i < hashes.size(); ++i)
        {
            if (i != 0)
            {
                out << ",";
            }

            out << "\"" << hashToString(hashes[i]) << "\"";
        }
        out << "]";
        return out.str();
    }

    bool ZmqPublisher::isNonLoopbackTcpEndpoint(const std::string &endpoint)
    {
        if (endpoint.rfind("tcp://", 0) != 0)
        {
            return false;
        }

        const auto address = endpoint.substr(6);
        if (address.empty())
        {
            return false;
        }

        std::string host;
        if (address[0] == '[')
        {
            const auto close = address.find(']');
            if (close == std::string::npos)
            {
                return false;
            }

            host = address.substr(1, close - 1);
        }
        else
        {
            const auto colon = address.rfind(':');
            host = (colon == std::string::npos) ? address : address.substr(0, colon);
        }

        std::transform(
            host.begin(),
            host.end(),
            host.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        return !(host == "127.0.0.1" || host == "localhost" || host == "::1");
    }

    const char *ZmqPublisher::deleteReasonToString(CryptoNote::Messages::DeleteTransaction::Reason reason)
    {
        switch (reason)
        {
            case CryptoNote::Messages::DeleteTransaction::Reason::InBlock:
                return "InBlock";
            case CryptoNote::Messages::DeleteTransaction::Reason::Outdated:
                return "Outdated";
            case CryptoNote::Messages::DeleteTransaction::Reason::NotActual:
                return "NotActual";
        }

        return "Unknown";
    }
} // namespace Daemon
