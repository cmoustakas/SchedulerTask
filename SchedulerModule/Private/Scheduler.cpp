#include <Scheduler.hpp>

#include <cassert>
#include <thread>

#if defined(__GNUC__) || defined(__clang__)
#define __try_branch_pred_hint(cond, likely) \
    (__builtin_expect(!!(cond), (likely))) ///< [!!]: ensures boolean of the condition
#define __try_prefetch_on_cache(address) (__builtin_prefetch(address))
#else
// On MSVC or other compilers, do nothing
#define __try_branch_pred_hint(cond, likely) (cond)
#define __try_prefetch_on_cache(address) (void)
#endif

using ThreadPool = std::vector<std::future<void>>;

namespace scheduler_module {

static void workerWrapper(Task &task, SchedulerStats &statistics)
{
    assert(task.m_task_fn != nullptr);

    std::chrono::duration<double> begin_execution_duration = std::chrono::steady_clock::now()
                                                             - task.m_enqueued_timestamp;
    task.m_task_fn();
    statistics.updateMetrics(begin_execution_duration.count());
}

static inline bool garbageCollect(ThreadPool &pool)
{
    const auto fastErrase = [](int &index, ThreadPool &pool) {
        // The code below is a workaround to avoid using
        // std::vector::erase that is gonna make copies and mallocs.
        auto last_it = pool.end() - 1;
        std::swap(pool[index], *last_it);
        pool.pop_back();
        index--;
    };

    const int full_size = pool.size();

    for (int i = 0; i < static_cast<int>(pool.size()); ++i) {
        auto &future = pool[i];
        const bool thread_completed = future.wait_for(std::chrono::seconds(0))
                                      == std::future_status::ready;

        if (not thread_completed) {
            continue;
        }

        // If the thread is finished just pop it from the active pool.
        fastErrase(i, pool);
    }

    // If the size of the pool remained the same the queue can not be served because it is still full
    const bool can_serve_queue = pool.size() == full_size;
    return can_serve_queue;
}

static void executeEnqueuedTasks(std::priority_queue<Task> &task_queue,
                                 std::atomic<ThreadState> &executor_state,
                                 SchedulerStats &statistics,
                                 std::mutex &mtx,
                                 const size_t max_num_of_threads)
{
    ThreadPool pool;
    pool.reserve(max_num_of_threads);
    // I am assuming GNU compiler to fetch the vector to the Cache
    __try_prefetch_on_cache(pool.data());

    while (executor_state == ThreadState::RUNNING) {
        // If my pool is not full serve the queue, else garbage collect first
        const bool must_garbage_collect = pool.size() == max_num_of_threads;
        bool can_serve_queue = must_garbage_collect ? (garbageCollect(pool)) : (true);

        {
            std::lock_guard<std::mutex> lock(mtx);
            can_serve_queue &= !task_queue.empty();
            // Most of the times the queue will be ready to be served, hopefully...
            if (__try_branch_pred_hint(can_serve_queue, true)) {
                assert(!task_queue.empty());
                Task most_urgent_task = task_queue.top();
                task_queue.pop();

                pool.emplace_back(std::async(std::launch::async,
                                             workerWrapper,
                                             std::ref(most_urgent_task),
                                             std::ref(statistics)));
            }
        }
    }
}

static void pollRecurringTasks(std::priority_queue<Task> &task_queue,
                               std::atomic<ThreadState> &executor_state,
                               std::unordered_map<double, std::vector<Task>> &recurring_tasks,
                               std::mutex &queue_mtx,
                               std::mutex &recurring_mtx)
{
    std::unordered_map<double, std::vector<Task>>::iterator last_it = recurring_tasks.begin();
    auto begin_timestamp = std::chrono::steady_clock::now();

    while (executor_state == ThreadState::RUNNING) {
        std::lock_guard<std::mutex> lock_r(recurring_mtx);
        {
            // Boundary checking on the iterator
            if (last_it == recurring_tasks.end()) {
                last_it = recurring_tasks.begin();
                begin_timestamp = std::chrono::steady_clock::now();
            }

            const double interval = last_it->first;
            auto &tasks = last_it->second;

            auto current_timestamp = std::chrono::steady_clock::now();
            const double duration = std::chrono::duration<double, std::milli>(current_timestamp
                                                                              - begin_timestamp)
                                        .count();

            if (duration >= interval) {
                std::lock_guard<std::mutex> lock_q(queue_mtx);
                for (auto &task : tasks) {
                    //Update the enqueued timestamp
                    task.m_enqueued_timestamp = std::chrono::steady_clock::now();
                    task_queue.push(task);
                }
            }

            last_it++;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

Scheduler::Scheduler(size_t num_threads)
{
    if (num_threads == 0) {
        throw std::runtime_error("Number of threads are expected to be non zero");
    }

    // Set the state and fire up the two main threads of the scheduler,
    // the first one (m_task_executor) gets the enqueued tasks and executes them,
    // in the meantime it also garbage collects the pool.
    // The second one polls the recurring tasks and enqueues them periodically.
    m_executor_state = ThreadState::RUNNING;
    m_task_executor = std::async(std::launch::async,
                                 executeEnqueuedTasks,
                                 std::ref(m_task_queue),
                                 std::ref(m_executor_state),
                                 std::ref(m_stats),
                                 std::ref(m_queue_mtx),
                                 num_threads);

    m_recurring_enqueuer = std::async(std::launch::async,
                                      pollRecurringTasks,
                                      std::ref(m_task_queue),
                                      std::ref(m_executor_state),
                                      std::ref(m_recurring_tasks),
                                      std::ref(m_queue_mtx),
                                      std::ref(m_recurring_mtx));
}

Scheduler::~Scheduler()
{
    m_executor_state = ThreadState::FINISHED;
    m_task_executor.wait();
    m_recurring_enqueuer.wait();

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
}

void Scheduler::scheduleRecurring(TaskFunction &&task_fn,
                                  const Task::Priority priority,
                                  std::chrono::milliseconds interval)
{
    std::lock_guard<std::mutex> lock(m_recurring_mtx);
    const auto dummy_time = std::chrono::steady_clock::now();
    Task task = {std::move(task_fn), priority, std::nullopt, dummy_time};
    m_recurring_tasks[interval.count()].push_back(task);
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
