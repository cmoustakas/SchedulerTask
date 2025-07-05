#pragma once

#include <mutex>

namespace scheduler_module {

class SchedulerStats
{
public:
    struct Metrics
    {
        double m_mean = 0.0;
        double m_min = 0.0;
        double m_max = 0.0;
        double variance = 0.0;
        int num_samples = 0.0;
    };

    SchedulerStats() = default;
    ~SchedulerStats() = default;

    /**
     * @brief updateMetrics
     * @param duration
     */
    void updateMetrics(const double duration) noexcept;

    /**
     * @brief getMetricsSoFar
     * @return 
     */
    [[nodiscard]] Metrics getMetricsSoFar() const noexcept { return m_metrics; }

private:
    Metrics m_metrics;
    std::mutex m_mtx;
};

} // namespace scheduler_module
