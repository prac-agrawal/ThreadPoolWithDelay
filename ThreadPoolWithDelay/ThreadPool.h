//
//  ThreadPool.h
//  ThreadPoolWithDelay
//
//  Created by Priya Agrawal on 15/11/26.
//
//  Copyright (C) 2026 Priya Agrawal
//  SPDX-License-Identifier: GPL-3.0-or-later
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <https://www.gnu.org/licenses/>.
//

#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

/**
 ThreadPool: a pool of worker threads that run tasks after a delay.

 - enqueue(f, delay, args...) schedules f(args...) to run no earlier than
   `delay` from now and returns a std::future for its result.
 - Worker threads are created lazily: one new thread per enqueue() call,
   up to the maximum given to the constructor.
 - Pending tasks are kept in a min-heap ordered by their due time, so the
   task that is due soonest is always at the top.
 - Stop() asks the workers to exit. The destructor calls Stop() itself and
   joins the workers, so calling Stop() explicitly is optional.
 */
class ThreadPool
{
    // A queued task together with the time at which it may start.
    struct NamedTask{
        std::function<void()> mTaskFunc;                    // the work to run
        std::chrono::steady_clock::time_point executeAt;    // earliest start time
    };

    // Ordering for std::priority_queue. priority_queue is a max-heap, so
    // comparing with '>' puts the task with the EARLIEST executeAt on top.
    struct TaskCompare{
        bool operator()(const NamedTask& a, const NamedTask& b) const
        {
            return a.executeAt > b.executeAt;
        }
    };

    // Upper limit on the number of worker threads (always at least 1).
    const std::size_t mMaxWorkersCount;

    // Guards mTaskQueue and mWorkers.
    std::mutex mTaskMutex;

    // Signalled when a task is added or Stop() is called; workers wait on it.
    std::condition_variable mTaskCV;

    // Shared stop flag. Each worker keeps its own copy of the shared_ptr.
    std::shared_ptr<std::atomic_bool> mStop;

    // Worker threads started so far (grows up to mMaxWorkersCount).
    std::vector<std::thread> mWorkers;

    // Pending tasks, soonest-due first (see TaskCompare).
    std::priority_queue<NamedTask, std::vector<NamedTask>, TaskCompare> mTaskQueue;

private:
    // Holds the worker thread entry point. Nested so it can reach the
    // pool's private members.
    class Worker{
    public:
        // Loop run by every worker thread: wait for a task to become due,
        // run it, repeat until the pool is stopped.
        static void run(ThreadPool* pool, std::size_t worker_id);
    };

public:
    // nr_workers: maximum number of worker threads (defaults to the number
    // of hardware threads; 0 is treated as 1). No threads are started until
    // enqueue() is called.
    explicit ThreadPool(std::size_t nr_workers = std::thread::hardware_concurrency());

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    // Calls Stop() and joins every worker thread.
    ~ThreadPool() noexcept;

    // Schedule f(args...) to run after `delay`.
    // Returns a future holding f's return value, or the exception it threw.
    template <typename F, typename... Args>
    auto enqueue(F&& f, std::chrono::milliseconds delay, Args&&... args)
        -> std::future<std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>>
    {
        using ReturnType = std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>;

        // Bundle the callable and its arguments into one no-argument task.
        // Arguments are moved into a tuple so move-only types (unique_ptr, ...)
        // work. packaged_task is move-only but std::function must be
        // copyable, so it is held through a shared_ptr.
        auto task = std::make_shared<std::packaged_task<ReturnType()>>(
            [fn = std::forward<F>(f),
             tup = std::make_tuple(std::forward<Args>(args)...)]() mutable -> ReturnType {
                return std::apply(std::move(fn), std::move(tup));
            });

        // Grab the future before the task is handed to another thread.
        std::future<ReturnType> result = task->get_future();
        {
            std::lock_guard<std::mutex> lock(mTaskMutex);

            // Queue the task with its due time.
            mTaskQueue.emplace(NamedTask{
                [task] { (*task)(); },
                std::chrono::steady_clock::now() + delay
            });

            // Start one more worker per enqueue() until the limit is reached.
            // Done under the lock so concurrent enqueue() calls don't race
            // on mWorkers.
            if (mWorkers.size() < mMaxWorkersCount) {
                mWorkers.emplace_back(&ThreadPool::Worker::run, this, mWorkers.size());
            }
        }

        // Wake one waiting worker so it can look at the new task. A worker
        // already sleeping until a later task re-checks the queue top, so an
        // earlier task is not held back.
        mTaskCV.notify_one();
        return result;
    }

    // Tell all workers to exit. Tasks that have not started yet are dropped,
    // and their futures report std::future_error (broken_promise) when the
    // pool is destroyed. Safe to call more than once.
    void Stop();
};
