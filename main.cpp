//
//  main.cpp
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

// Demo: schedule three tasks with 1, 2 and 3 second delays and print when
// each one actually runs, measured from program start.

#include <chrono>
#include <iostream>
#include <mutex>
#include <string>
#include "ThreadPool.h"

int main()
{
    // Reference point for all printed timestamps.
    using Clock = std::chrono::steady_clock;
    const auto start = Clock::now();

    // Print a message prefixed with the milliseconds elapsed since start.
    // Called from several threads, so output is serialised with a mutex.
    std::mutex coutMutex;
    auto log = [&](const std::string& msg) {
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                      Clock::now() - start).count();
        std::lock_guard<std::mutex> lock(coutMutex);   // keep lines from interleaving
        std::cout << "[" << ms << " ms] " << msg << '\n';
    };

    // Pool with one worker per hardware thread (the default).
    ThreadPool pool;

    // Schedule three tasks. Each should print roughly `delay` ms after start.
    pool.enqueue([&]{ log("Hello World from Worker with delay = 1 seconds"); }, std::chrono::seconds(1));
    pool.enqueue([&]{ log("Hello World from Worker with delay = 2 seconds"); }, std::chrono::seconds(2));
    pool.enqueue([&]{ log("Hello World from Worker with delay = 3 seconds"); }, std::chrono::seconds(3));

    // Give every task time to run: Stop() drops tasks that haven't started.
    std::this_thread::sleep_for(std::chrono::seconds(5));

    log("Stopping thread pool");
    pool.Stop();
    log("Thread pool stopped");

    // The pool's destructor joins the worker threads here.
    return 0;
}
