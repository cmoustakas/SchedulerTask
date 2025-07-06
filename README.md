#  Multithreaded Task Scheduler

This repository implements a **multithreaded task scheduler** in C++. It supports both **one-off** and **recurring tasks**, prioritizing them based on a custom scheduling policy.

##  Features

-  Support for one-time tasks.
-  Support for recurring tasks with fixed intervals.
-  Internally managed thread pool with:
  - A **task dispatcher thread** for executing scheduled tasks.
  - A **recurring task poller thread** that periodically enqueues recurring jobs.
-  **Priority-based execution**:
  - Tasks are prioritized first by user-defined **priority level**.
  - Ties are resolved by the **tightest deadline** (earliest to expire).

##  Architecture Overview

When a `Scheduler` object is instantiated, two dedicated threads are launched:

1. **Task Execution Thread**  
   - Continuously monitors the task queue.
   - Picks and executes tasks in priority order.

2. **Recurring Task Poller Thread**  
   - Monitors registered recurring tasks.
   - Enqueues them into the task queue when their interval expires.

## 📦 Build Instructions

This project uses CMake. Make sure you have CMake (≥ 3.14) and a C++17-compatible compiler.

```bash
git clone https://github.com/cmoustakas/SchedulerTask.git
cd SchedulerTask
git submodule update --init --recursive
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)
./SchedulerTests