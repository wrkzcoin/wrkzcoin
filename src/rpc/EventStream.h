// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace httplib
{
    namespace ws
    {
        class WebSocket;
    }
} // namespace httplib

/* Limits of GET /ws (--enable-websocket). */
struct EventStreamConfig
{
    /* --ws-max-clients: subscribers at once; past it an upgrade is a 503. */
    size_t maxClients = 128;

    /* --ws-max-clients-per-ip: from one address, loopback exempt; past it a
       429. 0 is no per-address cap. */
    size_t maxClientsPerIp = 4;
};

/* The subscribers of GET /ws, and what the chain and the pool publish to.

   Every message is a text frame holding one JSON object,
   {"topic":"...","data":{...}}, where data is byte for byte the body the ZMQ
   socket publishes under the same topic (ChainEvents::describe). A
   subscription opens with a hello - the tip, and the topics it carries - and
   hears a heartbeat every 30 seconds.

   The stream is a notification, not a ledger: it never replays what a
   subscriber missed, so a client catches up over the ordinary RPC after
   connecting.

   Each subscriber is served on the RPC worker thread that took its upgrade,
   and that thread only ever writes to it. httplib's WebSocket keeps one
   unsynchronised read buffer per connection, and closing a connection reads
   the peer's close reply through it, so the one thread that closes must also
   be the only one that reads - and nothing here needs to hear from a
   subscriber anyway. What it sends is left unread; a subscriber that is gone
   is found when a write to it fails, which the 30 second heartbeat and
   httplib's own pings make sure happens. So a subscriber costs two threads
   (its writer and httplib's pinger) and a queue of QUEUE_MESSAGES frames.

   Publishing never blocks on a subscriber. One that falls QUEUE_MESSAGES
   behind is disconnected, not skipped, so a stream that looks complete is
   complete. */
class EventStream
{
  public:
    /* Frames waiting for one subscriber before it is disconnected: the ZMQ
       socket's high-water mark. */
    static constexpr size_t QUEUE_MESSAGES = 1000;

    /* Most prefixes one ?topics= may name. */
    static constexpr size_t MAX_TOPIC_PREFIXES = 16;

    /* How often a subscriber that heard nothing else gets a heartbeat. */
    static constexpr unsigned HEARTBEAT_SECONDS = 30;

    enum class Refusal
    {
        None,
        /* Every place is taken, or the stream is stopping: 503. */
        Full,
        /* This address holds its share: 429. */
        TooManyFromAddress,
    };

    explicit EventStream(EventStreamConfig config);

    ~EventStream();

    const EventStreamConfig &config() const;

    /* ?topics= picks topics by prefix, comma-separated, as a ZMQ subscription
       does: "hashblock" brings hashblock_alt too. Absent or empty is every
       topic. A prefix matching no topic is an error, so a misspelling is a 400
       rather than a stream that stays silent. On success fills prefixes (empty
       meaning every topic) and returns nothing; otherwise the error. */
    static std::optional<std::string> parseTopics(const std::string &value, std::vector<std::string> &prefixes);

    /* The topics a subscription with these prefixes carries, for its hello. */
    static std::vector<std::string> carriedTopics(const std::vector<std::string> &prefixes);

    /* Whether an upgrade from this address would be admitted now. Checked
       before the 101 so a refusal is an ordinary HTTP answer; serve() checks
       again, since another upgrade may have taken the place meanwhile. exempt
       (loopback) skips the per-address cap but not the total. */
    Refusal wouldRefuse(const std::string &ip, const bool exempt) const;

    /* Sends one message to every subscriber that wants the topic. Any thread;
       never blocks on a subscriber. body is the JSON object the ZMQ socket
       would publish. */
    void publish(const std::string &topic, const std::string &body);

    /* Serves one upgraded connection until it goes away, falls behind, or the
       stream stops. Runs on the RPC worker thread that took the upgrade. */
    void serve(
        httplib::ws::WebSocket &ws,
        const std::string &ip,
        const bool exempt,
        const std::vector<std::string> &prefixes,
        const std::string &helloBody);

    /* Closes every subscriber with 1001 and refuses new ones. Returns at once;
       each subscriber's thread finishes on its own, which is what the RPC
       server's stop then waits for. */
    void stop();

    size_t clients() const;

    uint64_t published() const;

    uint64_t disconnectedSlow() const;

  private:
    struct Subscriber
    {
        std::string ip;

        bool exempt = false;

        std::vector<std::string> prefixes;

        std::mutex mutex;

        std::condition_variable wake;

        std::deque<std::shared_ptr<const std::string>> queue;

        /* Set when this subscriber must be closed: it fell behind, or the
           stream is stopping. */
        bool closing = false;

        bool goingAway = false;
    };

    static bool wants(const std::vector<std::string> &prefixes, const std::string &topic);

    static std::string frame(const std::string &topic, const std::string &body);

    Refusal wouldRefuseLocked(const std::string &ip, const bool exempt) const;

    const EventStreamConfig m_config;

    mutable std::mutex m_mutex;

    std::vector<std::shared_ptr<Subscriber>> m_subscribers;

    bool m_running = true;

    std::atomic<uint64_t> m_published = 0;

    std::atomic<uint64_t> m_disconnectedSlow = 0;
};
