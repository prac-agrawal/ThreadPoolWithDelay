//
//  main.cpp
//  ThreadPoolWithDelay
//
//  Created by Priya Agrawal on 15/11/26.
//

#include <chrono>
#include <iostream>
#include <mutex>
#include <string>
#include "ThreadPool.h"

int main()
{
    using Clock = std::chrono::steady_clock;
    const auto start = Clock::now();

    std::mutex coutMutex;
    auto log = [&](const std::string& msg) {
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                      Clock::now() - start).count();
        std::lock_guard<std::mutex> lock(coutMutex);   // keep lines from interleaving
        std::cout << "[" << ms << " ms] " << msg << '\n';
    };

    ThreadPool pool;
    pool.enqueue([&]{ log("Hello World from Worker with delay = 1 seconds"); }, std::chrono::seconds(1));
    pool.enqueue([&]{ log("Hello World from Worker with delay = 2 seconds"); }, std::chrono::seconds(2));
    pool.enqueue([&]{ log("Hello World from Worker with delay = 3 seconds"); }, std::chrono::seconds(3));

    std::this_thread::sleep_for(std::chrono::seconds(5));
    log("Stopping thread pool");
    pool.Stop();
    log("Thread pool stopped");

    return 0;
}
