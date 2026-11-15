//
//  ThreadPool.h
//  ThreadPoolWithDelay
//
//  Created by Priya Agrawal on 15/11/26.
//

#include<iostream>
#include <thread>
#include <future>
#include <functional>
#include <chrono>
#include <queue>
using namespace std;
/**
 Class to encapsulate thread pool
 */

class ThreadPool
{
    
    struct NamedTask{
        std::function<void()> mTaskFunc;
        std::chrono::steady_clock::time_point executeAt;
    };
    
    struct TaskCompare{
        bool operator()(const NamedTask& a, const NamedTask& b) const
        {
            return a.executeAt > b.executeAt;
        }
    };
    
    int mWorkersCompletedCount = 0;
    const int mMaxWorkersCount = 0;
    
    std::mutex mWorkersCompletionCountMutex;
    std::mutex mTaskMutex;
    
    std::condition_variable mWorkersCompletionCV;
    std::condition_variable mTaskCV;
    
    std::shared_ptr<std::atomic_bool> mStop;
    
    std::vector<std::thread> mWorkers;
    std::vector<bool> mWorkersCompletedStatus;
    
    std::priority_queue<NamedTask, std::vector<NamedTask>, TaskCompare> mTaskQueue;
    
private:
    class Worker{
    public:
        static void run(ThreadPool* pool, const int worker_id);
    };
    
public:
    ThreadPool(std::size_t nr_workers = std::thread::hardware_concurrency());
    ~ThreadPool() noexcept;
    
    template <typename F, typename... Args>
    auto enqueue(F&& f, std::chrono::milliseconds delay, Args&&... args)
        -> std::future<std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>>
    {
        using ReturnType = std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>;

        auto task = std::make_shared<std::packaged_task<ReturnType()>>(
            [fn = std::forward<F>(f),
             tup = std::make_tuple(std::forward<Args>(args)...)]() mutable -> ReturnType {
                return std::apply(std::move(fn), std::move(tup));
            });

        std::future<ReturnType> result = task->get_future();
        {
            std::lock_guard<std::mutex> lock(mTaskMutex);
            mTaskQueue.emplace(NamedTask{
                [task] { (*task)(); },
                std::chrono::steady_clock::now() + delay
            });

            if (mWorkers.size() < mMaxWorkersCount) {
                // adjust to however your worker entry point is actually declared
                mWorkers.emplace_back(&ThreadPool::Worker::run, this, mWorkers.size());
            }
        }
        mTaskCV.notify_one();
        return result;
    }
    
    void Stop();
};
