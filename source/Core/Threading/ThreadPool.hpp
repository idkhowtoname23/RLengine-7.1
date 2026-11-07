#pragma once

#include "Core/Types.hpp"

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <atomic>
#include <algorithm>
#include <memory>
#include <stdexcept>

namespace RL
{
    class ThreadPool
    {
    public:
        static ThreadPool& Get() noexcept
        {
            static ThreadPool instance;
            return instance;
        }

        explicit ThreadPool(u32 threadCount = 0)
        {
            if (threadCount == 0)
            {
                threadCount = std::max(1u, std::thread::hardware_concurrency());
            }

            m_workers.reserve(threadCount);
            for (u32 i = 0; i < threadCount; ++i)
            {
                m_workers.emplace_back([this]() {
                    WorkerLoop();
                });
            }
        }

        ~ThreadPool()
        {
            {
                std::unique_lock<std::mutex> lock(m_queueMutex);
                m_stopping = true;
            }
            m_cv.notify_all();

            for (auto& worker : m_workers)
            {
                if (worker.joinable())
                {
                    worker.join();
                }
            }
        }

        ThreadPool(const ThreadPool&) = delete;
        ThreadPool& operator=(const ThreadPool&) = delete;
        ThreadPool(ThreadPool&&) = delete;
        ThreadPool& operator=(ThreadPool&&) = delete;

        template <typename F, typename... Args>
        auto Enqueue(F&& f, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>>
        {
            using ReturnType = std::invoke_result_t<F, Args...>;

            auto task = std::make_shared<std::packaged_task<ReturnType()>>(
                [func = std::forward<F>(f), ...capturedArgs = std::forward<Args>(args)]() mutable {
                    return func(capturedArgs...);
                }
            );

            std::future<ReturnType> res = task->get_future();
            {
                std::unique_lock<std::mutex> lock(m_queueMutex);
                if (m_stopping)
                {
                    throw std::runtime_error("Enqueue on stopped ThreadPool");
                }
                m_tasks.emplace([task]() { (*task)(); });
            }
            m_cv.notify_one();
            return res;
        }

        template <typename Func>
        void ParallelFor(usize count, usize minChunkSize, Func&& func)
        {
            if (count == 0) return;

            const u32 numWorkers = static_cast<u32>(m_workers.size());
            if (count <= minChunkSize || numWorkers <= 1)
            {
                func(0, count);
                return;
            }

            const usize calculatedChunk = (count + numWorkers - 1) / numWorkers;
            const usize chunkSize = std::max(minChunkSize, calculatedChunk);
            const usize numChunks = (count + chunkSize - 1) / chunkSize;

            if (numChunks <= 1)
            {
                func(0, count);
                return;
            }

            std::atomic<usize> remainingChunks{ numChunks };
            std::mutex waitMutex;
            std::condition_variable waitCv;
            std::mutex exceptionMutex;
            std::exception_ptr caughtException = nullptr;

            for (usize c = 1; c < numChunks; ++c)
            {
                const usize start = c * chunkSize;
                const usize end = std::min(start + chunkSize, count);

                {
                    std::unique_lock<std::mutex> lock(m_queueMutex);
                    m_tasks.emplace([&, start, end]() {
                        try
                        {
                            func(start, end);
                        }
                        catch (...)
                        {
                            std::unique_lock<std::mutex> eLock(exceptionMutex);
                            if (!caughtException)
                            {
                                caughtException = std::current_exception();
                            }
                        }

                        if (remainingChunks.fetch_sub(1, std::memory_order_acq_rel) == 1)
                        {
                            std::unique_lock<std::mutex> wLock(waitMutex);
                            waitCv.notify_all();
                        }
                    });
                }
                m_cv.notify_one();
            }

            try
            {
                func(0, std::min(chunkSize, count));
            }
            catch (...)
            {
                std::unique_lock<std::mutex> eLock(exceptionMutex);
                if (!caughtException)
                {
                    caughtException = std::current_exception();
                }
            }

            if (remainingChunks.fetch_sub(1, std::memory_order_acq_rel) > 1)
            {
                std::unique_lock<std::mutex> wLock(waitMutex);
                waitCv.wait(wLock, [&]() {
                    return remainingChunks.load(std::memory_order_acquire) == 0;
                });
            }

            if (caughtException)
            {
                std::rethrow_exception(caughtException);
            }
        }

        template <typename Func>
        void ParallelFor(usize count, Func&& func)
        {
            ParallelFor(count, 1024, std::forward<Func>(func));
        }

        [[nodiscard]] u32 GetWorkerCount() const noexcept
        {
            return static_cast<u32>(m_workers.size());
        }

        [[nodiscard]] u32 GetThreadCount() const noexcept
        {
            return static_cast<u32>(m_workers.size());
        }

    private:
        void WorkerLoop()
        {
            while (true)
            {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(m_queueMutex);
                    m_cv.wait(lock, [this]() {
                        return m_stopping || !m_tasks.empty();
                    });

                    if (m_stopping && m_tasks.empty())
                    {
                        return;
                    }

                    task = std::move(m_tasks.front());
                    m_tasks.pop();
                }

                task();
            }
        }

    private:
        std::vector<std::thread> m_workers;
        std::queue<std::function<void()>> m_tasks;
        std::mutex m_queueMutex;
        std::condition_variable m_cv;
        bool m_stopping = false;
    };
}
