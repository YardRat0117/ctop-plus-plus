// Unit tests for SysStatsModel parsing, rate calculation and SysViewModel.

#include "sys_stats_reader.hpp"
#include "ctopp/viewmodel/sys_view_model.hpp"

#include <cmath>
#include <cstdio>
#include <sstream>
#include <string>
#include <unordered_set>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "[FAIL] %s\n", message);
    ++failures;
}

void expect_near(float actual, float expected, float tolerance,
                 const char* message) {
    expect(std::fabs(actual - expected) <= tolerance, message);
}

// ---------------------------------------------------------------------------
// /proc parsers
// ---------------------------------------------------------------------------
void test_proc_stat_parser() {
    std::istringstream input(
        "cpu  100 10 20 800 30 4 6 2 50 5\n"
        "cpu0 50 5 10 400 15 2 3 1 25 2\n"
        "cpu1 50 5 10 400 15 2 3 1 25 3\n"
        "intr 12345\n");

    ctopp::detail::CpuSample sample;
    expect(ctopp::detail::parse_proc_stat(input, sample),
           "parse /proc/stat");
    expect(sample.aggregate.idle == 830, "aggregate idle includes iowait");
    expect(sample.aggregate.total == 972,
           "aggregate total excludes guest fields");
    expect(sample.cores.size() == 2, "parse per-core CPU rows");
}

void test_meminfo_parser() {
    std::istringstream input(
        "MemTotal:       8388608 kB\n"
        "MemFree:        1048576 kB\n"
        "MemAvailable:   2097152 kB\n"
        "SwapTotal:      4194304 kB\n"
        "SwapFree:       3145728 kB\n");

    ctopp::MemStats stats;
    expect(ctopp::detail::parse_meminfo(input, stats),
           "parse /proc/meminfo");
    expect(stats.total_kb == 8388608, "read MemTotal");
    expect(stats.avail_kb == 2097152, "read MemAvailable");
    expect(stats.swap_used_kb == 1048576, "calculate used swap");
}

void test_diskstats_parser() {
    std::istringstream input(
        "8 0 sda 10 0 100 0 20 0 200 0 0 0 0 0\n"
        "8 1 sda1 10 0 90 0 20 0 180 0 0 0 0 0\n"
        "7 0 loop0 10 0 500 0 20 0 600 0 0 0 0 0\n"
        "259 0 nvme0n1 10 0 300 0 20 0 400 0 0 0 0 0\n");
    const std::unordered_set<std::string> devices{"sda", "nvme0n1"};

    ctopp::detail::DiskCounters counters;
    expect(ctopp::detail::parse_diskstats(input, devices, counters),
           "parse selected whole-disk rows");
    expect(counters.read_sectors == 400, "sum disk read sectors");
    expect(counters.write_sectors == 600, "sum disk write sectors");
}

void test_net_dev_parser() {
    std::istringstream input(
        "Inter-| Receive | Transmit\n"
        " face |bytes packets errs drop fifo frame compressed multicast|"
        "bytes packets errs drop fifo colls carrier compressed\n"
        "lo: 1000 1 0 0 0 0 0 0 1000 1 0 0 0 0 0 0\n"
        "eth0: 2048 2 0 0 0 0 0 0 4096 3 0 0 0 0 0 0\n"
        "wlan0: 1024 1 0 0 0 0 0 0 2048 2 0 0 0 0 0 0\n");

    ctopp::detail::NetCounters counters;
    expect(ctopp::detail::parse_net_dev(input, counters),
           "parse /proc/net/dev");
    expect(counters.rx_bytes == 3072, "sum non-loopback RX bytes");
    expect(counters.tx_bytes == 6144, "sum non-loopback TX bytes");
}

// ---------------------------------------------------------------------------
// Delta calculations
// ---------------------------------------------------------------------------
void test_rate_calculations() {
    ctopp::detail::CpuSample previous;
    previous.aggregate = {800, 1000};
    previous.cores = {{400, 500}, {400, 500}};
    ctopp::detail::CpuSample current;
    current.aggregate = {850, 1200};
    current.cores = {{425, 600}, {425, 600}};

    const auto cpu = ctopp::detail::calculate_cpu_stats(previous, current);
    expect_near(cpu.total_pct, 75.0f, 0.001f, "calculate total CPU usage");
    expect(cpu.per_core_pct.size() == 2, "calculate every CPU core");
    expect_near(cpu.per_core_pct[0], 75.0f, 0.001f,
                "calculate per-core CPU usage");

    const ctopp::detail::DiskCounters previous_disk{100, 200};
    const ctopp::detail::DiskCounters current_disk{1124, 2248};
    const auto disk = ctopp::detail::calculate_disk_stats(
        previous_disk, current_disk, 2.0);
    expect_near(disk.read_kbps, 256.0f, 0.001f, "calculate disk read KiB/s");
    expect_near(disk.write_kbps, 512.0f, 0.001f,
                "calculate disk write KiB/s");

    const ctopp::detail::NetCounters previous_net{1024, 2048};
    const ctopp::detail::NetCounters current_net{5120, 10240};
    const auto net = ctopp::detail::calculate_net_stats(
        previous_net, current_net, 2.0);
    expect_near(net.rx_kbps, 2.0f, 0.001f, "calculate network RX KiB/s");
    expect_near(net.tx_kbps, 4.0f, 0.001f, "calculate network TX KiB/s");
}

void test_counter_reset() {
    const ctopp::detail::DiskCounters previous{1000, 1000};
    const ctopp::detail::DiskCounters current{10, 20};
    const auto disk = ctopp::detail::calculate_disk_stats(
        previous, current, 1.0);
    expect_near(disk.read_kbps, 0.0f, 0.001f,
                "disk counter reset does not create a spike");
    expect_near(disk.write_kbps, 0.0f, 0.001f,
                "disk write reset does not create a spike");
}

// ---------------------------------------------------------------------------
// SysViewModel conversion and history
// ---------------------------------------------------------------------------
void test_sys_view_model() {
    ctopp::SysViewModel view_model;
    ctopp::SysStatsSnapshot snapshot;
    snapshot.cpu.total_pct = 25.0f;
    snapshot.cpu.per_core_pct = {20.0f, 30.0f};
    snapshot.mem.total_kb = 8ULL * 1024 * 1024;
    snapshot.mem.avail_kb = 2ULL * 1024 * 1024;
    snapshot.disk.read_kbps = 2048.0f;
    snapshot.disk.write_kbps = 1024.0f;
    snapshot.net_if.rx_kbps = 4096.0f;
    snapshot.net_if.tx_kbps = 512.0f;

    for (std::size_t i = 0; i < ctopp::SysViewData::kMaxHistory + 5; ++i) {
        snapshot.cpu.total_pct = static_cast<float>(i);
        view_model.on_snapshot(snapshot);
    }

    const auto data = view_model.get_data();
    expect(data.cpu_per_core_pct.size() == 2, "copy per-core CPU data");
    expect_near(data.mem_used_pct, 75.0f, 0.001f,
                "convert available memory to used percentage");
    expect(data.mem_total_gb == 8, "convert total memory to GiB");
    expect(data.mem_avail_gb == 2, "convert available memory to GiB");
    expect_near(data.disk_read_mbps, 2.0f, 0.001f,
                "convert disk read rate to MiB/s");
    expect_near(data.net_rx_mbps, 4.0f, 0.001f,
                "convert network rate to MiB/s");
    expect(data.cpu_history.size() == ctopp::SysViewData::kMaxHistory,
           "limit CPU history to 60 samples");
    expect(data.mem_history.size() == ctopp::SysViewData::kMaxHistory,
           "limit memory history to 60 samples");
}

} // namespace

int main() {
    test_proc_stat_parser();
    test_meminfo_parser();
    test_diskstats_parser();
    test_net_dev_parser();
    test_rate_calculations();
    test_counter_reset();
    test_sys_view_model();

    if (failures != 0) {
        std::fprintf(stderr, "sys_stats_test: %d failure(s)\n", failures);
        return 1;
    }
    std::fprintf(stderr, "sys_stats_test: all checks passed\n");
    return 0;
}
