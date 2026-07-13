#pragma once

#include "ctopp/model/sys_stats_snapshot.hpp"

#include <cstdint>
#include <istream>
#include <string>
#include <unordered_set>
#include <vector>

namespace ctopp::detail {

// Raw cumulative values read from /proc/stat. Linux reports CPU time in
// USER_HZ ticks; only differences between two samples are meaningful here.
struct CpuTimes {
    uint64_t idle = 0;
    uint64_t total = 0;
};

struct CpuSample {
    CpuTimes aggregate;
    std::vector<CpuTimes> cores;
};

struct DiskCounters {
    uint64_t read_sectors = 0;
    uint64_t write_sectors = 0;
};

struct NetCounters {
    uint64_t rx_bytes = 0;
    uint64_t tx_bytes = 0;
};

// Parsing is kept separate from file I/O so unit tests can use string streams
// instead of depending on the host machine's live /proc contents.
bool parse_proc_stat(std::istream& input, CpuSample& sample);
bool parse_meminfo(std::istream& input, MemStats& stats);
bool parse_diskstats(std::istream& input,
                     const std::unordered_set<std::string>& devices,
                     DiskCounters& counters);
bool parse_net_dev(std::istream& input, NetCounters& counters,
                   bool include_loopback = false);

CpuStats calculate_cpu_stats(const CpuSample& previous,
                             const CpuSample& current);
DiskStats calculate_disk_stats(const DiskCounters& previous,
                               const DiskCounters& current,
                               double elapsed_seconds);
NetIfStats calculate_net_stats(const NetCounters& previous,
                               const NetCounters& current,
                               double elapsed_seconds);

} // namespace ctopp::detail
