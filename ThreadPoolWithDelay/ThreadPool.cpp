//
//  main.cpp
//  ThreadPoolWithDelay
//
//  Created by Priya Agrawal on 15/11/26.
//

#include "ThreadPool.h"

void ThreadPool::Worker::run(ThreadPool* pool, const int worker_id){
    auto stop_flag = pool->mStop;
    while (!stop_flag->load()){
        std::function<void()> cur_task;
        std::string cur_task_name;
        NamedTask named_task;
        {
            std::unique_lock<std::mutex> lock(pool->mTaskMutex);
            pool->mTaskCV.wait(lock, [&](){
                return stop_flag->load() || !pool->mTaskQueue.empty();
            });
            
            if(stop_flag->load())
            {
                break;
            }
            
            const auto nextTime = pool->mTaskQueue.top().executeAt;
            
            pool->mTaskCV.wait_until(lock, nextTime, [&](){
                return stop_flag->load();
            });
            
            if(stop_flag->load())
            {
                break;
            }
            
            if(pool->mTaskQueue.empty() || pool->mTaskQueue.top().executeAt > std::chrono::steady_clock::now())
                continue;
            
            named_task = pool->mTaskQueue.top();
            pool->mTaskQueue.pop();
        }
        
        cur_task = named_task.mTaskFunc;
        
        cur_task();
    }
}

ThreadPool::ThreadPool(std::size_t nr_workers/*=std::thread::hardware_concurrency()*/)
    : mMaxWorkersCount(nr_workers), mWorkersCompletedStatus(nr_workers, false)
{
        mStop = std::make_shared<std::atomic_bool>(false);
    }

void ThreadPool::Stop(){
    mStop->store(true);
    
    mTaskCV.notify_all();
}

ThreadPool::~ThreadPool() noexcept{
    auto start = std::chrono::high_resolution_clock::now();
    
    try
    {
        {
            std::unique_lock<std::mutex> lock(mWorkersCompletionCountMutex);
            mWorkersCompletionCV.wait_for(lock, std::chrono::seconds(2), [&] { return mWorkersCompletedCount == mMaxWorkersCount; });
        }
    }
    catch (...)
    { }
    
    int workersSize = static_cast<int>(mWorkers.size());
    for(int i=0; i < workersSize; i++)
    {
        try
        {
            std::unique_lock<std::mutex> lock(mWorkersCompletionCountMutex);
            
            mWorkers[i].join();
            
        }
        catch (...)
        {
            
        }
    }
    
    mWorkers.clear();
    
    auto end = std::chrono::high_resolution_clock::now();
    auto diff = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
}
