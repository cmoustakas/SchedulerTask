#pragma once

#include <SchedulerStats.hpp>
#include <Task.hpp>

#include <atomic>
#include <functional>
#include <future>
#include <optional>
#include <queue>

namespace scheduler_module {

using TaskFunction = std::function<void()>;
enum class ThreadState { RUNNING = 0, FINISHED = 1 };

class Scheduler
{
public:
    explicit Scheduler(size_t num_threads = std::thread::hardware_concurrency());
    ~Scheduler();

    /**
     * @brief schedule
     * @param task
     * @param priority
     * @param deadline
     */
    void schedule(TaskFunction &&task_fn,
                  const Task::Priority priority,
                  std::optional<std::chrono::steady_clock::time_point> deadline = std::nullopt);

    /**
     * @brief scheduleRecurring
     * @param task
     * @param priority
     * @param interval
     */
    void scheduleRecurring(TaskFunction &&task_fn,
                           const Task::Priority priority,
                           std::chrono::milliseconds interval);

    /**
     * @brief getLatencyStatistics
     * @return 
     */
    std::tuple<double, double, double, double> getLatencyStatistics() const noexcept;

private:
    std::priority_queue<Task> m_task_queue;
    SchedulerStats m_statistics;

    std::future<void> m_task_executor;
    std::future<void> m_recurring_enqueuer;

    std::atomic<ThreadState> m_executor_state = ThreadState::FINISHED;

    std::unordered_map<double, std::vector<Task>> m_recurring_tasks;

    std::mutex m_queue_mtx;
    std::mutex m_recurring_mtx;
};

} // namespace scheduler_module
