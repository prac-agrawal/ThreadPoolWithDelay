# ThreadPoolWithDelay

A C++17 thread pool that runs tasks after a delay. You schedule a function with `enqueue(f, delay, args...)` and get back a `std::future` for its result.

## Features

- **Delayed tasks**: each task starts no earlier than its delay. Tasks run in due-time order, whatever order they were added in.
- **Futures**: every task returns a `std::future` holding its return value, or the exception it threw.
- **Any callable**: lambdas, functions and function objects, with any arguments, including move-only types like `std::unique_ptr`.
- **Lazy workers**: threads start only as tasks arrive, up to a maximum you choose (by default, the number of hardware threads).
- **Clean shutdown**: the destructor stops the workers and joins them, so you don't have to.
- **Standard library only**: no dependencies.

## Usage

```cpp
#include "ThreadPool.h"
#include <iostream>

int main()
{
    ThreadPool pool(4);   // at most 4 worker threads

    // Run after 500 ms and return a value.
    auto sum = pool.enqueue([](int a, int b) { return a + b; },
                            std::chrono::milliseconds(500), 2, 3);

    // Run after 1 second, no return value.
    auto done = pool.enqueue([] { std::cout << "one second later\n"; },
                             std::chrono::seconds(1));

    std::cout << "sum = " << sum.get() << '\n';   // waits ~500 ms, prints 5
    done.get();                                   // waits ~1 s
}   // the pool's destructor stops and joins the workers
```

Exceptions thrown inside a task are rethrown when you call `get()` on its future:

```cpp
auto bad = pool.enqueue([]() -> int { throw std::runtime_error("boom"); },
                        std::chrono::milliseconds(0));
try {
    (void)bad.get();   // rethrows the task's exception
} catch (const std::exception& e) {
    std::cout << "caught: " << e.what() << '\n';
}
```

## API

| Member | Description |
|---|---|
| `ThreadPool(std::size_t nr_workers = hardware_concurrency())` | Create a pool with at most `nr_workers` threads (0 is treated as 1). No threads start until the first `enqueue`. |
| `enqueue(f, delay, args...)` | Schedule `f(args...)` to run after `delay` (`std::chrono::milliseconds`; seconds etc. convert automatically). Returns `std::future<R>`, where `R` is `f`'s return type. |
| `Stop()` | Tell the workers to exit. Tasks that haven't started are dropped. Calling it more than once is safe. |
| `~ThreadPool()` | Calls `Stop()` and joins all worker threads. |

**About `Stop()`:** tasks that are still waiting for their delay are discarded. Calling `get()` on one of their futures then throws `std::future_error` (`broken_promise`) once the pool is destroyed. If you need every task to run, wait on the futures before stopping or destroying the pool.

## How it works

- Pending tasks sit in a priority queue (a min-heap) ordered by due time, so the soonest task is always on top.
- Each worker waits until the queue has a task, then sleeps until that task is due. It wakes early if `Stop()` is called, or if a sooner task is added.
- When the top task is due, a worker takes it off the queue and runs it without holding the lock, so other workers can keep picking up tasks.
- Each `enqueue` starts one more worker until the maximum is reached.

## Building

**Xcode:** open `ThreadPoolWithDelay.xcodeproj` and run the `ThreadPoolWithDelay` scheme.

**Command line** (clang or GCC, C++17 or later):

```bash
clang++ -std=c++17 -O2 -pthread ThreadPool.cpp main.cpp -o ThreadPoolWithDelay
./ThreadPoolWithDelay
```

The demo in `main.cpp` schedules three tasks with 1, 2 and 3 second delays and prints when each one actually runs:

```
[1001 ms] Hello World from Worker with delay = 1 seconds
[2004 ms] Hello World from Worker with delay = 2 seconds
[3005 ms] Hello World from Worker with delay = 3 seconds
[5004 ms] Stopping thread pool
[5005 ms] Thread pool stopped
```

## Using it in your project

Copy `ThreadPool.h` and `ThreadPool.cpp` into your project, add `ThreadPool.cpp` to your build, and `#include "ThreadPool.h"`.

## License

Copyright (C) 2026 Priya Agrawal

This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version. See [COPYING](COPYING) for the full text.
