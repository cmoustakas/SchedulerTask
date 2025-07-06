#include <gtest/gtest.h>

#include <Scheduler.hpp>
#include <chrono>
#include <thread>
#include <tuple>

TEST(SchedulerTest, UninitializedLatency)
{
    scheduler_module::Scheduler sch;
    std::tuple<double, double, double, double> stats = sch.getLatencyStatistics();
    const bool all_uninitialized = std::get<0>(stats) == 0.0 && std::get<1>(stats) == 0.0
                                   && std::get<2>(stats) == 0.0 && std::get<3>(stats) == 0.0;

    EXPECT_TRUE(all_uninitialized);
}

TEST(SchedulerTest, ZeroNumOfThreads)
{
    EXPECT_THROW(scheduler_module::Scheduler sch(0), std::runtime_error);
}

TEST(SchedulerTest, PriorityDeterministic)
{
    //Make the pool full and check if the priority is valid based on logging
    constexpr size_t num_of_threads = 1;
    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();

    std::chrono::steady_clock::time_point t_irrelevant;
    std::chrono::steady_clock::time_point t_high;
    std::chrono::steady_clock::time_point t_low;

    std::function<void()> fn_low_first = [&t_irrelevant]() {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        t_irrelevant = std::chrono::steady_clock::now();
    };

    std::function<void()> fn_low_second = [&t_low]() {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        t_low = std::chrono::steady_clock::now();
    };

    std::function<void()> fn_high_third = [&t_high]() {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        t_high = std::chrono::steady_clock::now();
    };

    scheduler_module::Scheduler sch(num_of_threads);
    sch.schedule(std::move(fn_low_first), scheduler_module::Task::Priority::LOW);
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Now the pool is full and we have to ensure that
    // the highest priority task is gonna be executed first
    // Despite the fact that it came last
    sch.schedule(std::move(fn_low_second), scheduler_module::Task::Priority::LOW);
    sch.schedule(std::move(fn_high_third), scheduler_module::Task::Priority::HIGH);

    //Just give some time in order to make sure that the threads are executed
    std::this_thread::sleep_for(std::chrono::seconds(5));

    double high_ms = std::chrono::duration<double, std::deci>(t_high - start).count();
    double low_sec_ms = std::chrono::duration<double, std::deci>(t_low - start).count();

    const bool high_executed_first = high_ms < low_sec_ms;
    EXPECT_TRUE(high_executed_first);
}

TEST(SchedulerTest, PriorityDeterministicOnDeadline)
{
    constexpr size_t num_of_threads = 1;

    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();

    std::chrono::steady_clock::time_point t_irrelevant;
    std::chrono::steady_clock::time_point t_tight;
    std::chrono::steady_clock::time_point t_loose;

    std::chrono::steady_clock::time_point deadline_tight = std::chrono::steady_clock::now();
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::chrono::steady_clock::time_point deadline_loose = std::chrono::steady_clock::now();

    std::function<void()> fn_irrelevant = [&t_irrelevant]() {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        t_irrelevant = std::chrono::steady_clock::now();
    };

    std::function<void()> fn_deadline_tight = [&t_tight]() {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        t_tight = std::chrono::steady_clock::now();
    };

    std::function<void()> fn_deadline_loose = [&t_loose]() {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        t_loose = std::chrono::steady_clock::now();
    };

    scheduler_module::Scheduler sch(num_of_threads);
    sch.schedule(std::move(fn_irrelevant), scheduler_module::Task::Priority::LOW);
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Now the pool is full and we have to ensure that
    // for the same priority task the tightest deadline is executed
    sch.schedule(std::move(fn_deadline_loose),
                 scheduler_module::Task::Priority::MID,
                 deadline_loose);
    sch.schedule(std::move(fn_deadline_tight),
                 scheduler_module::Task::Priority::MID,
                 deadline_tight);

    //Just give some time in order to make sure that the threads are executed
    std::this_thread::sleep_for(std::chrono::seconds(5));

    double tight_ms = std::chrono::duration<double, std::milli>(t_tight - start).count();
    double loose_ms = std::chrono::duration<double, std::milli>(t_loose - start).count();

    assert(t_tight != start);
    assert(t_loose != start);

    const bool tight_executed_first = tight_ms < loose_ms;
    EXPECT_TRUE(tight_executed_first);
}

TEST(SchedulerTest, RecurringTasksExplodeMapThrow)
{
    scheduler_module::Scheduler sch;
    std::chrono::milliseconds standard_interval(1);
    constexpr int kExplodeRecurringTasksOnStandardInterval = 1e2;

    for (int i = 0; i < kExplodeRecurringTasksOnStandardInterval; ++i) {
        std::function<void()> dummy_fn = []() { return; };

        sch.scheduleRecurring(std::move(dummy_fn),
                              scheduler_module::Task::Priority::MID,
                              standard_interval);
    }

    std::function<void()> dummy_fn = []() { return; };

    EXPECT_THROW(sch.scheduleRecurring(std::move(dummy_fn),
                                       scheduler_module::Task::Priority::MID,
                                       standard_interval),
                 std::runtime_error);
}

TEST(SchedulerTest, RecurringTasksNormal)
{
    std::chrono::milliseconds interval(25);

    int execution_times = 0;
    constexpr int kExpectedExecutions = 4;
    {
        std::function<void()> job = [&execution_times]() {
            execution_times++;
            return;
        };

        scheduler_module::Scheduler sch;
        sch.scheduleRecurring(std::move(job), scheduler_module::Task::Priority::MID, interval);

        std::this_thread::sleep_for(std::chrono::milliseconds(105));
    }

    //+- 1 deviation
    constexpr int kTollerance = 1;
    EXPECT_NEAR(execution_times, kExpectedExecutions, kTollerance);
}

TEST(SchedulerTest, RecurringTwoTasks)
{
    std::chrono::milliseconds interval_one(30);
    std::chrono::milliseconds interval_two(50);

    int execution_times_one = 0;
    int execution_times_two = 0;

    constexpr int kExpectedExecutionsOne = 3;
    constexpr int kExpectedExecutionsTwo = 2;
    {
        std::function<void()> job_one = [&execution_times_one]() {
            execution_times_one++;
            return;
        };

        std::function<void()> job_two = [&execution_times_two]() {
            execution_times_two++;
            return;
        };

        scheduler_module::Scheduler sch;
        sch.scheduleRecurring(std::move(job_one),
                              scheduler_module::Task::Priority::MID,
                              interval_one);

        sch.scheduleRecurring(std::move(job_two),
                              scheduler_module::Task::Priority::HIGH,
                              interval_two);

        std::this_thread::sleep_for(std::chrono::milliseconds(105));
    }

    //+- 1 deviation
    constexpr int kTollerance = 1;
    EXPECT_NEAR(execution_times_one, kExpectedExecutionsOne, kTollerance);
    EXPECT_NEAR(execution_times_two, kExpectedExecutionsTwo, kTollerance);
}
