#include <Scheduler.hpp>

#include <cassert>

namespace scheduler_module {
void SchedulerStats::updateMetrics(const double duration) noexcept
{
    // Reference: https://en.wikipedia.org/wiki/Algorithms_for_calculating_variance#Welford's_online_algorithm
    std::lock_guard<std::mutex> lock(m_mtx);

    auto &m = m_metrics;

    m.m_min = std::min(m.m_min, duration);
    m.m_max = std::max(m.m_max, duration);

    m.m_variance *= m.m_num_samples;
    m.m_num_samples++;

    //Update mean and variance on the fly without need to re-iterate
    const auto diff = duration - m.m_mean;
    m.m_mean += diff / m.m_num_samples;

    m.m_variance += diff * (duration - m.m_mean);

    assert(m.m_num_samples > 0);
    m.m_variance /= m.m_num_samples;
}

} // namespace scheduler_module
