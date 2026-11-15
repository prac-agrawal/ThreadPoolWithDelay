//
//  ThreadPool.cpp
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

#include "ThreadPool.h"

#include <algorithm>

// Worker thread main loop.
// Each iteration: wait until the queue has a task, sleep until the soonest
// task is due, take it off the queue, then run it outside the lock.
void ThreadPool::Worker::run(ThreadPool* pool, std::size_t /*worker_id*/){
    // Keep a local copy of the shared stop flag.
    auto stop_flag = pool->mStop;
    while (!stop_flag->load()){
        NamedTask named_task;
        {
            std::unique_lock<std::mutex> lock(pool->mTaskMutex);

            // 1. Sleep until there is at least one task or we are told to stop.
            pool->mTaskCV.wait(lock, [&](){
                return stop_flag->load() || !pool->mTaskQueue.empty();
            });

            if(stop_flag->load())
            {
                break;
            }

            // 2. Sleep until the soonest task is due. Wake early if:
            //    - Stop() was called,
            //    - another worker took the task (queue emptied), or
            //    - a new task arrived that is due before the one we waited for.
            const auto nextTime = pool->mTaskQueue.top().executeAt;

            pool->mTaskCV.wait_until(lock, nextTime, [&](){
                return stop_flag->load()
                    || pool->mTaskQueue.empty()
                    || pool->mTaskQueue.top().executeAt < nextTime;
            });

            if(stop_flag->load())
            {
                break;
            }

            // 3. If the queue is empty or its top task isn't due yet (because
            //    we woke early, see above), go back and wait again.
            if(pool->mTaskQueue.empty() || pool->mTaskQueue.top().executeAt > std::chrono::steady_clock::now())
                continue;

            // 4. Take the due task off the queue.
            named_task = pool->mTaskQueue.top();
            pool->mTaskQueue.pop();
        }

        // 5. Run the task without holding the lock, so other workers can
        //    keep pulling tasks meanwhile. Exceptions are captured by the
        //    packaged_task and delivered through the task's future.
        named_task.mTaskFunc();
    }
}

// Store the worker limit (at least 1, since hardware_concurrency() may
// return 0) and create the stop flag. Worker threads are started later,
// by enqueue().
ThreadPool::ThreadPool(std::size_t nr_workers/*=std::thread::hardware_concurrency()*/)
    : mMaxWorkersCount(std::max<std::size_t>(nr_workers, 1)),
      mStop(std::make_shared<std::atomic_bool>(false))
{
}

// Set the stop flag and wake every worker so it notices and exits.
// Does not wait for workers to finish; the destructor does that.
void ThreadPool::Stop(){
    {
        // Setting the flag under the lock means a worker can't check the
        // flag, miss the notify below, and then sleep forever.
        std::lock_guard<std::mutex> lock(mTaskMutex);
        mStop->store(true);
    }
    mTaskCV.notify_all();
}

// Shut down: stop the workers, then join every thread.
ThreadPool::~ThreadPool() noexcept{
    Stop();

    // Once Stop() has run, every worker leaves its loop, so join() returns
    // promptly. join() can only throw if the thread isn't joinable, which
    // the check rules out.
    for (auto& worker : mWorkers)
    {
        if (worker.joinable())
            worker.join();
    }
    mWorkers.clear();
}
