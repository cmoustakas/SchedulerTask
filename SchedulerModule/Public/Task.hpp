#pragma once

#include <chrono>
#include <functional>
#include <optional>

namespace scheduler_module {

using TaskFunction = std::function<void()>;

struct Task
{
    enum class Priority { LOW = 0, MID = 1, HIGH = 2 };

    // It is essential to be able to compare two tasks, the first criterion is the priority and the second the deadline
    // Ref: https://www.geeksforgeeks.org/cpp/custom-comparator-in-priority_queue-in-cpp-stl/
    bool operator<(const Task &other) const
    {
        // Assuming GNU Compiler for C++17, or use [[likely/unlikely]] for C++20
        if (__builtin_expect(m_priority == other.m_priority, false)) {
            return m_deadline > other.m_deadline;
        }
        return m_priority < other.m_priority;
    }

    TaskFunction m_task_fn = nullptr;
    Priority m_priority = Priority::LOW;
    std::optional<std::chrono::steady_clock::time_point> m_deadline = std::nullopt;
    std::chrono::steady_clock::time_point m_enqueued_timestamp;
};
} // namespace scheduler_module
