// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

//////////////////////////
#include <rpc/EventStream.h>

#include "httplib.h"
//////////////////////////

/* windows.h, which httplib brings in, defines ERROR - which the logging
   headers below need as Logging::ERROR. */
#undef ERROR

#include <algorithm>
#include <chrono>
#include <rpc/ChainEvents.h>

EventStream::EventStream(EventStreamConfig config): m_config([&config] {
    config.maxClients = std::max<size_t>(1, config.maxClients);
    return config;
}())
{
}

EventStream::~EventStream()
{
    stop();
}

const EventStreamConfig &EventStream::config() const
{
    return m_config;
}

std::optional<std::string> EventStream::parseTopics(const std::string &value, std::vector<std::string> &prefixes)
{
    prefixes.clear();

    size_t start = 0;

    while (start <= value.size())
    {
        const size_t comma = value.find(',', start);
        const size_t end = comma == std::string::npos ? value.size() : comma;

        std::string prefix = value.substr(start, end - start);

        const size_t first = prefix.find_first_not_of(' ');
        const size_t last = prefix.find_last_not_of(' ');

        prefix = first == std::string::npos ? "" : prefix.substr(first, last - first + 1);

        if (!prefix.empty())
        {
            prefixes.push_back(prefix);
        }

        if (comma == std::string::npos)
        {
            break;
        }

        start = comma + 1;
    }

    if (prefixes.size() > MAX_TOPIC_PREFIXES)
    {
        prefixes.clear();
        return "at most " + std::to_string(MAX_TOPIC_PREFIXES) + " topics";
    }

    const auto &all = ChainEvents::topics();

    for (const auto &prefix : prefixes)
    {
        const bool matches = std::any_of(all.begin(), all.end(), [&prefix](const std::string &topic) {
            return topic.rfind(prefix, 0) == 0;
        });

        if (!matches)
        {
            std::string known;

            for (const auto &topic : all)
            {
                known += (known.empty() ? "" : ", ") + topic;
            }

            const std::string error = "no topic starts with '" + prefix + "'; the topics are " + known;
            prefixes.clear();
            return error;
        }
    }

    return std::nullopt;
}

std::vector<std::string> EventStream::carriedTopics(const std::vector<std::string> &prefixes)
{
    std::vector<std::string> carried;

    for (const auto &topic : ChainEvents::topics())
    {
        if (wants(prefixes, topic))
        {
            carried.push_back(topic);
        }
    }

    return carried;
}

bool EventStream::wants(const std::vector<std::string> &prefixes, const std::string &topic)
{
    if (prefixes.empty())
    {
        return true;
    }

    return std::any_of(prefixes.begin(), prefixes.end(), [&topic](const std::string &prefix) {
        return topic.rfind(prefix, 0) == 0;
    });
}

std::string EventStream::frame(const std::string &topic, const std::string &body)
{
    return "{\"topic\":\"" + topic + "\",\"data\":" + body + "}";
}

EventStream::Refusal EventStream::wouldRefuse(const std::string &ip, const bool exempt) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return wouldRefuseLocked(ip, exempt);
}

EventStream::Refusal EventStream::wouldRefuseLocked(const std::string &ip, const bool exempt) const
{
    if (!m_running || m_subscribers.size() >= m_config.maxClients)
    {
        return Refusal::Full;
    }

    if (!exempt && m_config.maxClientsPerIp != 0)
    {
        const size_t fromAddress = std::count_if(
            m_subscribers.begin(), m_subscribers.end(), [&ip](const auto &subscriber) { return subscriber->ip == ip; });

        if (fromAddress >= m_config.maxClientsPerIp)
        {
            return Refusal::TooManyFromAddress;
        }
    }

    return Refusal::None;
}

void EventStream::publish(const std::string &topic, const std::string &body)
{
    /* Encoded once, however many subscribers receive it. */
    const auto message = std::make_shared<const std::string>(frame(topic, body));

    m_published++;

    std::lock_guard<std::mutex> lock(m_mutex);

    for (const auto &subscriber : m_subscribers)
    {
        if (!wants(subscriber->prefixes, topic))
        {
            continue;
        }

        std::lock_guard<std::mutex> subscriberLock(subscriber->mutex);

        if (subscriber->closing)
        {
            continue;
        }

        if (subscriber->queue.size() >= QUEUE_MESSAGES)
        {
            /* Skipping would leave a stream that looks complete and is not. */
            subscriber->closing = true;
            subscriber->queue.clear();
            m_disconnectedSlow++;
        }
        else
        {
            subscriber->queue.push_back(message);
        }

        subscriber->wake.notify_one();
    }
}

void EventStream::serve(
    httplib::ws::WebSocket &ws,
    const std::string &ip,
    const bool exempt,
    const std::vector<std::string> &prefixes,
    const std::string &helloBody)
{
    auto subscriber = std::make_shared<Subscriber>();
    subscriber->ip = ip;
    subscriber->exempt = exempt;
    subscriber->prefixes = prefixes;

    {
        std::unique_lock<std::mutex> lock(m_mutex);

        /* The check before the 101 can be overtaken by another upgrade, and
           the stream may have started stopping since. */
        if (wouldRefuseLocked(ip, exempt) != Refusal::None)
        {
            const bool stopping = !m_running;
            lock.unlock();

            ws.close(
                stopping ? httplib::ws::CloseStatus::GoingAway : httplib::ws::CloseStatus::PolicyViolation,
                stopping ? "node stopping" : "subscriptions are full, retry later");
            return;
        }

        m_subscribers.push_back(subscriber);
    }

    const std::string heartbeat = frame("heartbeat", "{}");

    bool open = ws.send(frame("hello", helloBody));

    bool closing = false;

    bool goingAway = false;

    while (open)
    {
        std::deque<std::shared_ptr<const std::string>> pending;

        {
            std::unique_lock<std::mutex> lock(subscriber->mutex);

            subscriber->wake.wait_for(lock, std::chrono::seconds(HEARTBEAT_SECONDS), [&subscriber] {
                return subscriber->closing || !subscriber->queue.empty();
            });

            closing = subscriber->closing;
            goingAway = subscriber->goingAway;
            pending.swap(subscriber->queue);
        }

        if (closing)
        {
            break;
        }

        /* Nothing arrived for a while: a heartbeat, for browsers, whose scripts
           never see the pings httplib sends alongside it. A write that fails
           here is also how a subscriber that went away is found. */
        if (pending.empty())
        {
            open = ws.send(heartbeat);
        }

        for (const auto &message : pending)
        {
            if (!open)
            {
                break;
            }

            open = ws.send(*message);
        }

        /* httplib's pinger marks the connection closed when a ping fails. */
        open = open && ws.is_open();
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);

        m_subscribers.erase(
            std::remove(m_subscribers.begin(), m_subscribers.end(), subscriber), m_subscribers.end());
    }

    if (ws.is_open())
    {
        if (goingAway)
        {
            ws.close(httplib::ws::CloseStatus::GoingAway, "node stopping");
        }
        else if (closing)
        {
            ws.close(httplib::ws::CloseStatus::PolicyViolation, "fell too far behind");
        }
        else
        {
            ws.close();
        }
    }
}

void EventStream::stop()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    m_running = false;

    for (const auto &subscriber : m_subscribers)
    {
        std::lock_guard<std::mutex> subscriberLock(subscriber->mutex);
        subscriber->closing = true;
        subscriber->goingAway = true;
        subscriber->wake.notify_one();
    }
}

size_t EventStream::clients() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_subscribers.size();
}

uint64_t EventStream::published() const
{
    return m_published;
}

uint64_t EventStream::disconnectedSlow() const
{
    return m_disconnectedSlow;
}
