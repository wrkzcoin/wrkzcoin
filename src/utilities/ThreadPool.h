// Copyright (c) 2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace Utilities
{
    template<typename ReturnValue>
    class ThreadPool
    {
        public:
            /////////////////
            /* CONSTRUCTOR */
            /////////////////

            ThreadPool() : ThreadPool(std::thread::hardware_concurrency())
            {
            }

            ThreadPool(uint64_t threadCount) :
                m_shouldStop(false)
            {
                if (threadCount == 0)
                {
                    threadCount = 1;
                }

                m_threadCount = threadCount;

                /* Launch our worker threads */
                for (uint64_t i = 0; i < threadCount; i++)
                {
                    m_threads.push_back(std::thread(&ThreadPool::waitForJob, this));
                }
            }

            ////////////////
            /* DESTRUCTOR */
            ////////////////

            ~ThreadPool()
            {
                /* Signal threads to stop */
                m_shouldStop = true;

                /* Wake them all up */
                m_haveJob.notify_all();

                /* Wait for them to stop */
                for (auto &thread : m_threads)
                {
                    thread.join();
                }
            }

            /////////////////////////////
            /* PUBLIC MEMBER FUNCTIONS */
            /////////////////////////////

            std::future<ReturnValue> addJob(const std::function<ReturnValue()> job)
            {
                std::promise<ReturnValue> promise;

                std::future<ReturnValue> result = promise.get_future();

                /* Aquire lock */
                std::unique_lock<std::mutex> lock(m_mutex);

                /* Add job to queue */
                m_queue.push({ job, std::move(promise) });

                /* Manually unlocking here so we don't wake up and instantly
                 * sleep upon notify_one */
                lock.unlock();

                /* Wake up a thread to process the job */
                m_haveJob.notify_one();

                return result;
            }

        private:

            //////////////////////////////
            /* PRIVATE MEMBER FUNCTIONS */
            //////////////////////////////

            void waitForJob()
            {
                while (!m_shouldStop)
                {
                    std::unique_lock<std::mutex> lock(m_mutex);

                    /* Wait for data to become available or to be stopped */
                    m_haveJob.wait(lock, [&] {
                        if (m_shouldStop)
                        {
                            return true;
                        }

                        return !m_queue.empty();
                    });

                    if (m_shouldStop)
                    {
                        return;
                    }

                    /* Take next job from queue */
                    auto [job, result] = std::move(m_queue.front());

                    /* Remove item from queue */
                    m_queue.pop();

                    /* Undo the lock so other threads can process jobs */
                    lock.unlock();

                    /* Process job and set return value */
                    result.set_value(job());
                }
            }

            //////////////////////////////
            /* PRIVATE MEMBER VARIABLES */
            //////////////////////////////

            /* The work threads */
            std::vector<std::thread> m_threads;

            /* The amount of threads to launch */
            uint64_t m_threadCount;

            /* Whether we're stopping */
            std::atomic<bool> m_shouldStop;

            /* Whether we have a new job to process */
            std::condition_variable m_haveJob;

            /* Mutex to serialize access to job queue */
            std::mutex m_mutex;

            /* The queue of job functions and job results */
            std::queue<
                std::tuple<std::function<ReturnValue()>, std::promise<ReturnValue>>
            > m_queue;
    };
}
