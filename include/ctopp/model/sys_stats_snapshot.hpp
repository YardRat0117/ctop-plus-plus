#pragma once

#include <cstdint>
#include <vector>

namespace ctopp {

struct CpuStats {
    float total_pct = 0.0f;
    std::vector<float> per_core_pct;
};

struct MemStats {
    uint64_t total_kb = 0;
    uint64_t avail_kb = 0;
    uint64_t swap_used_kb = 0;
};

struct DiskStats {
    float read_kbps = 0.0f;
    float write_kbps = 0.0f;
};

struct NetIfStats {
    float rx_kbps = 0.0f;
    float tx_kbps = 0.0f;
};

struct SysStatsSnapshot {
    CpuStats   cpu;
    MemStats   mem;
    DiskStats  disk;
    NetIfStats net_if;
};

} // namespace ctopp
