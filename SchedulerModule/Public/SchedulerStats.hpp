#pragma once

#include <cstdint>
#include <limits>
#include <mutex>

namespace scheduler_module {

class SchedulerStats
{
public:
    struct Metrics
    {
        double m_mean = 0.0;
        double m_min = std::numeric_limits<double>::max();
        double m_max = 0.0;
        double m_variance = 0.0;
        uint64_t m_num_samples = 0;
    };

    SchedulerStats() = default;
    ~SchedulerStats() = default;

    /**
     * @brief updateMetrics Update the statistic metrics member
     * @param duration the current duration = enqueue - start_execution
     */
    void updateMetrics(const double duration) noexcept;

    /**
     * @brief getMetricsSoFar Returns the metrics
     * @return 
     */
    [[nodiscard]] Metrics getMetricsSoFar() const noexcept
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        return m_metrics;
    }

private:
    Metrics m_metrics;
    mutable std::mutex m_mtx;
};

} // namespace scheduler_module
