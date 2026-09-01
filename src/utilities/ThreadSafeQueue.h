// Copyright (c) 2018-2019, The TurtleCoin Developers
// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <utility>

template<typename T> class ThreadSafeQueue
{
  public:
    ThreadSafeQueue(): m_shouldStop(false) {}

    ThreadSafeQueue(bool startStopped): m_shouldStop(startStopped) {}

    bool pushMove(T &&item)
    {
        /* Acquire the lock */
        std::unique_lock<std::mutex> lock(m_mutex);

        /* Stopping, don't push data */
        if (m_shouldStop)
        {
            return false;
        }

        /* Add the item to the front of the queue */
        m_queue.push(std::move(item));

        /* Unlock the mutex before notifying, so it doesn't block after
           waking up */
        lock.unlock();

        /* Notify the consumer that we have some data */
        m_haveData.notify_all();

        return true;
    }

    bool push(T item)
    {
        /* Acquire the lock */
        std::unique_lock<std::mutex> lock(m_mutex);

        /* Stopping, don't push data */
        if (m_shouldStop)
        {
            return false;
        }

        /* Add the item to the front of the queue */
        m_queue.push(item);

        /* Unlock the mutex before notifying, so it doesn't block after
           waking up */
        lock.unlock();

        /* Notify the consumer that we have some data */
        m_haveData.notify_all();

        return true;
    }

    /* Delete the front item from the queue */
    void deleteFront()
    {
        /* Acquire the lock */
        std::unique_lock<std::mutex> lock(m_mutex);

        /* Whilst we could allow deleting from an empty queue, i.e, waiting
           for an item, then removing it, this could cause us to be stuck
           waiting on data to arrive when the queue is empty. We can't
           really return without removing the item. We could return a bool
           saying if we completed it, but then the user has no real way to
           force a removal short of running it in a while loop.
           Instead, if we just force the queue to have to have data in,
           we can make sure a removal always succeeds. */
        if (m_queue.empty())
        {
            throw std::runtime_error("Cannot remove from an empty queue!");
        }

        /* Remove the first item from the queue */
        m_queue.pop();

        /* Unlock the mutex before notifying, so it doesn't block after
           waking up */
        lock.unlock();

        m_consumedData.notify_all();
    }
    
    /* Take a copy of the item at the front of the queue, and do NOT remove
       it. Returns a default constructed T when the queue is stopping - the
       caller is expected to be checking for that itself, as the one caller
       does before it asks. */
    T front()
    {
        /* Acquire the lock */
        std::unique_lock<std::mutex> lock(m_mutex);

        const T *item = getFirstItem(lock);

        return item == nullptr ? T() : *item;
    }

    /* Take and remove an item from the front of the queue */
    T pop()
    {
        /* Acquire the lock */
        std::unique_lock<std::mutex> lock(m_mutex);

        T *item = getFirstItem(lock);

        if (item == nullptr)
        {
            return T();
        }

        T taken = std::move(*item);

        /* Remove item */
        m_queue.pop();

        /* Unlock the mutex before notifying, so it doesn't block after
           waking up */
        lock.unlock();

        m_consumedData.notify_all();

        return taken;
    }

    /* Stop the queue if something is waiting on it, so we don't block
       whilst closing */
    void stop()
    {
        /* Make sure the queue knows to return */
        m_shouldStop = true;

        /* Wake up anything waiting on data */
        m_haveData.notify_all();

        /* Make sure not to call .unlock() on the mutex here - it's
           undefined behaviour if it isn't locked. */

        m_consumedData.notify_all();
    }

    void start()
    {
        m_shouldStop = false;
    }

    size_t size() const
    {
        std::scoped_lock lock(m_mutex);
        return m_queue.size();
    }

  private:
    /* Returns the front of the queue, or nullptr when the queue is stopping
       and there is nothing to hand back.

       This used to return T& and, on the stopping path, default construct a
       local and return a reference to it - a reference to a destroyed object,
       every time. front() handed that straight to its caller, and pop() move
       constructed from it, which for a type owning heap memory (the queue
       holds WalletTypes::Transaction in zedwallet++, and std::future in the
       daemon's block export) is a move out of freed stack rather than merely a
       garbage read. Both callers already discard the value when stopping, so
       there was never anything to return; saying so with a pointer is what the
       code always meant. */
    T *getFirstItem(std::unique_lock<std::mutex> &lock)
    {
        /* Stopping, don't return data */
        if (m_shouldStop)
        {
            return nullptr;
        }

        /* Wait for data to become available (releases the lock whilst
           it's not, so we don't block the producer) */
        m_haveData.wait(lock, [&] {
            /* Stopping, don't block */
            if (m_shouldStop)
            {
                return true;
            }

            return !m_queue.empty();
        });

        /* Stopping, don't return data */
        if (m_shouldStop)
        {
            return nullptr;
        }

        /* Get the first item in the queue */
        return &m_queue.front();
    }

    /* The deque data structure */
    std::queue<T> m_queue;

    /* The mutex, to ensure we have atomic access to the queue */
    mutable std::mutex m_mutex;

    /* Whether we have data or not */
    std::condition_variable m_haveData;

    /* Triggered when data is consumed */
    std::condition_variable m_consumedData;

    /* Whether we're stopping */
    std::atomic<bool> m_shouldStop;
};
