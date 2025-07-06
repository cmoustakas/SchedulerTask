#include <Scheduler.hpp>

#include <cassert>
#include <iostream>
#include <sstream>

#if defined(__GNUC__) || defined(__clang__)
#define __try_branch_pred_hint(cond, likely) \
    (__builtin_expect(!!(cond), (likely))) ///< [!!]: ensures boolean of the condition
#define __try_prefetch_on_cache(address) (__builtin_prefetch(address))
#else
// On MSVC or other compilers, do nothing
#define __try_branch_pred_hint(cond, likely) (cond)
#define __try_prefetch_on_cache(address) (void)
#endif

namespace scheduler_module {

Scheduler::Scheduler(size_t num_threads)
{
    if (num_threads == 0) {
        throw std::runtime_error("Number of threads are expected to be non zero");
    }

    // Set the state and fire up the two main threads of the scheduler,
    // the first one (m_task_executor) gets the enqueued tasks and executes them,
    // in the meantime it also garbage collects the pool.
    // The second one polls the recurring tasks and enqueues them periodically.
    m_executor_state = SchedulerState::RUNNING;
    m_pool.reserve(num_threads);

    // I am assuming GNU compiler to fetch the vector to the Cache
    __try_prefetch_on_cache(m_pool.data());
    for (size_t i = 0; i < num_threads; ++i) {
        m_pool.emplace_back([this]() { workerWrapper(); });
    }

    m_recurring_enqueuer = std::thread([this] { pollRecurringTasks(); });
}

Scheduler::~Scheduler()
{
    m_executor_state = SchedulerState::FINISHED;
    m_condition.notify_all();

    for (auto &t : m_pool) {
        t.join();
    }
    m_recurring_enqueuer.join();

    //Log the latency metrics at destruction
    const auto metrix = m_stats.getMetricsSoFar();

    std::cout << "[+] Latency Statistics(ms): \n";
    std::cout << "\t Min = " << metrix.m_min << "\n";
    std::cout << "\t Max = " << metrix.m_max << "\n";
    std::cout << "\t Mean = " << metrix.m_mean << "\n";
    std::cout << "\t Variance = " << metrix.m_variance << "\n";
}
/**
     * @brief schedule
     * @param task
     * @param priority
     * @param deadline
     */
void Scheduler::schedule(TaskFunction &&task_fn,
                         const Task::Priority priority,
                         std::optional<std::chrono::steady_clock::time_point> deadline)
{
    //Construct a task struct.
    std::lock_guard<std::mutex> lock(m_queue_mtx);
    const auto enqueued_time = std::chrono::steady_clock::now();
    Task task = {std::move(task_fn), priority, deadline, enqueued_time};
    m_task_queue.push(std::move(task));

    m_condition.notify_one();
}

void Scheduler::scheduleRecurring(TaskFunction &&task_fn,
                                  const Task::Priority priority,
                                  std::chrono::milliseconds interval)
{
    constexpr int kMaxRecurringTasksLen = 1e2;

    std::lock_guard<std::mutex> lock(m_recurring_mtx);
    const auto dummy_time = std::chrono::steady_clock::now();
    Task task = {std::move(task_fn), priority, std::nullopt, dummy_time};
    //Reserve a big enough chunk of memory to prevent multiple copies and allocations.
    // 100 tasks with the same millisecond interval is very unlinkely I guess.
    double interval_key = interval.count();
    if (interval_key <= 0) {
        throw std::runtime_error("Interval negative values are not allowed");
    }
    auto &task_vec = m_recurring_tasks[interval_key];

    if (task_vec.size() == kMaxRecurringTasksLen) {
        std::stringstream err;
        err << "[-] Maximum number of recurring tasks has been reached for interval: "
            << interval_key << "\n";

        throw std::runtime_error(err.str());
    }
    task_vec.reserve(kMaxRecurringTasksLen);
    task_vec.push_back(task);
}

void Scheduler::workerWrapper()
{
    while (m_executor_state == SchedulerState::RUNNING) {
        Task most_urgent_task;
        {
            std::unique_lock<std::mutex> lock(m_queue_mtx);
            m_condition.wait(lock, [this] {
                return m_executor_state == SchedulerState::FINISHED || !m_task_queue.empty();
            });

            if (m_executor_state == SchedulerState::FINISHED && m_task_queue.empty()) {
                return;
            }

            most_urgent_task = std::move(m_task_queue.top());
            m_task_queue.pop();
        }

        assert(most_urgent_task.m_task_fn != nullptr);

        std::chrono::duration<double, std::milli> begin_execution_duration
            = std::chrono::steady_clock::now() - most_urgent_task.m_enqueued_timestamp;
        most_urgent_task.m_task_fn();
        m_stats.updateMetrics(begin_execution_duration.count());
    }
}

void Scheduler::pollRecurringTasks()
{
    std::unordered_map<double, std::chrono::steady_clock::time_point> expiration_detector;

    while (m_executor_state == SchedulerState::RUNNING) {
        auto current_timestamp = std::chrono::steady_clock::now();
        {
            std::lock_guard<std::mutex> lock(m_recurring_mtx);

            for (auto &[interval, tasks] : m_recurring_tasks) {
                //If interval has just been inserted just set the current timestamp as the init point
                if (expiration_detector.find(interval) == expiration_detector.end()) {
                    expiration_detector[interval] = current_timestamp;
                    continue;
                }

                double delta = std::chrono::duration<double, std::milli>(
                                   current_timestamp - expiration_detector[interval])
                                   .count();

                if (delta >= interval) {
                    // Time expired push all the tasks to the queue and
                    // update the init point of my expiration detector
                    expiration_detector[interval] = current_timestamp;
                    std::lock_guard<std::mutex> lock_q(m_queue_mtx);
                    for (auto &task : tasks) {
                        //Update the enqueued timestamp
                        task.m_enqueued_timestamp = std::chrono::steady_clock::now();
                        m_task_queue.push(task);
                        m_condition.notify_one();
                    }
                }
            }
        }
    }
}

std::tuple<double, double, double, double> Scheduler::getLatencyStatistics() const noexcept
{
    const auto metrix = m_stats.getMetricsSoFar();

    std::tuple<double, double, double, double> t = {metrix.m_min,
                                                    metrix.m_max,
                                                    metrix.m_mean,
                                                    metrix.m_variance};
    return t;
}

} // namespace scheduler_module
