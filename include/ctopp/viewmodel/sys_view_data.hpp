#pragma once

#include <cstdint>
#include <deque>
#include <vector>
#include <string>

namespace ctopp {

struct SysViewData {
    // --- Real-time values ---
    float cpu_total_pct = 0.0f;
    std::vector<float> cpu_per_core_pct;
    float mem_used_pct = 0.0f;
    uint64_t mem_total_gb = 0;
    uint64_t mem_avail_gb = 0;
    uint64_t mem_total_mb = 0;
    uint64_t mem_used_mb = 0;
    float disk_read_mbps = 0.0f;
    float disk_write_mbps = 0.0f;
    float net_rx_mbps = 0.0f;
    float net_tx_mbps = 0.0f;

    // --- History (for line charts, up to 60 data points = 60 seconds) ---
    static constexpr size_t kMaxHistory = 60;
    std::deque<float> cpu_history;
    std::deque<float> mem_history;
};

} // namespace ctopp
