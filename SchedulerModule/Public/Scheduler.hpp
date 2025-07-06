#pragma once

#include <SchedulerStats.hpp>
#include <Task.hpp>

#include <atomic>
#include <condition_variable>
#include <functional>
#include <optional>
#include <queue>
#include <thread>

namespace scheduler_module {

using TaskFunction = std::function<void()>;
using ThreadPool = std::vector<std::thread>;

class Scheduler
{
public:
    enum class SchedulerState { RUNNING = 0, FINISHED = 1 };

    explicit Scheduler(size_t num_threads = std::thread::hardware_concurrency());
    ~Scheduler();

    /**
     * @brief schedule Schedules the one-off task
     * @param task The task
     * @param priority The priority of the task
     * @param deadline The deadline of the task
     */
    void schedule(TaskFunction &&task_fn,
                  const Task::Priority priority,
                  std::optional<std::chrono::steady_clock::time_point> deadline = std::nullopt);

    /**
     * @brief scheduleRecurring Schedules tasks with specific interval
     * @param task The task
     * @param priority The priority
     * @param interval The interval
     */
    void scheduleRecurring(TaskFunction &&task_fn,
                           const Task::Priority priority,
                           std::chrono::milliseconds interval);

    /**
     * @brief getLatencyStatistics returns the latency statistics
     * @return 
     */
    std::tuple<double, double, double, double> getLatencyStatistics() const noexcept;

private:
    /**
     * @brief workerWrapper Core loop for all the workers in the pool
     */
    void workerWrapper();

    /**
     * @brief pollRecurringTasks Loop of the worker that enqueues the recurring tasks
     */
    void pollRecurringTasks();

    std::atomic<SchedulerState> m_state = SchedulerState::FINISHED;

    SchedulerStats m_stats;

    ThreadPool m_pool;
    std::priority_queue<Task> m_task_queue;
    std::mutex m_queue_mtx;

    std::thread m_recurring_enqueuer;
    std::unordered_map<double, std::vector<Task>> m_recurring_tasks;
    std::mutex m_recurring_mtx;

    std::condition_variable m_condition;
};

} // namespace scheduler_module
