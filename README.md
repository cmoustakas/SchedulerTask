# 🧵 Multithreaded Task Scheduler

This repository implements a high-performance, multithreaded task scheduler in C++. It supports both one-off and recurring tasks and prioritizes execution based on priority level and deadline urgency.

## Features

  One-off Task Scheduling — Submit individual tasks with optional deadlines.

  Recurring Task Scheduling — Schedule periodic tasks with millisecond precision.

  Prioritized Execution — Tasks are executed based on:

  + Priority level (high, medium, low),

  + Deadline tightness (earlier deadlines first).

  Custom Worker Thread Pool — Threads are preallocated, reducing dynamic memory and thread creation overhead.

  Execution Metrics — Automatically collects and prints min, max, mean, and variance of task latency at destruction.

  Efficient Synchronization — Uses std::condition_variable, std::mutex, and prefetching optimizations for low-latency execution.

## Overview

Upon construction of a Scheduler instance:

  A thread pool is created to consume and execute scheduled tasks from a shared priority queue.

  A separate thread polls recurring tasks and periodically enqueues them if their interval has expired.

## Execution Flow

  Worker threads wait efficiently until tasks become available.

  When signaled, the most urgent task is popped and executed.

  Recurring tasks are managed using a timestamp detector, triggering their enqueue when their interval has passed.

## 📦 Build Instructions

This project uses CMake. Make sure you have CMake (≥ 3.14) and a C++17-compatible compiler.

```bash
git clone https://github.com/cmoustakas/SchedulerTask.git
cd SchedulerTask
# Add gtests as submodule to the repo
git submodule update --init --recursive
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)
./SchedulerTests
