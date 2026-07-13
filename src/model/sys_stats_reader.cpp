#include "sys_stats_reader.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <sstream>
#include <string>

namespace ctopp::detail {
namespace {

constexpr double kBytesPerKiB = 1024.0;
constexpr double kBytesPerDiskSector = 512.0;

bool is_cpu_label(const std::string& label) {
    if (label == "cpu") return true;
    if (label.size() <= 3 || label.compare(0, 3, "cpu") != 0) return false;
    return std::all_of(label.begin() + 3, label.end(), [](unsigned char ch) {
        return std::isdigit(ch) != 0;
    });
}

bool parse_cpu_times(std::istringstream& line, CpuTimes& times) {
    // user, nice, system, idle, iowait, irq, softirq, steal, guest,
    // guest_nice. guest values are already included in user/nice and must not
    // be counted twice.
    std::array<uint64_t, 10> fields{};
    std::size_t count = 0;
    while (count < fields.size() && line >> fields[count]) ++count;
    if (count < 4) return false;

    times.idle = fields[3] + (count > 4 ? fields[4] : 0);
    const std::size_t total_fields = std::min<std::size_t>(count, 8);
    for (std::size_t i = 0; i < total_fields; ++i) {
        times.total += fields[i];
    }
    return true;
}

std::string trim(std::string value) {
    const auto first = value.find_first_not_of(" \t");
    if (first == std::string::npos) return {};
    const auto last = value.find_last_not_of(" \t");
    return value.substr(first, last - first + 1);
}

uint64_t counter_delta(uint64_t previous, uint64_t current) {
    // /proc counters are monotonic in normal operation. A smaller current
    // value means the device was reset/replaced or a counter wrapped; treating
    // that sample as zero avoids a huge false rate.
    return current >= previous ? current - previous : 0;
}

float percentage(const CpuTimes& previous, const CpuTimes& current) {
    const uint64_t total_delta = counter_delta(previous.total, current.total);
    if (total_delta == 0 || current.idle < previous.idle) return 0.0f;

    const uint64_t idle_delta = current.idle - previous.idle;
    const uint64_t busy_delta = total_delta > idle_delta
        ? total_delta - idle_delta
        : 0;
    const double value = 100.0 * static_cast<double>(busy_delta)
        / static_cast<double>(total_delta);
    return static_cast<float>(std::clamp(value, 0.0, 100.0));
}

} // namespace

bool parse_proc_stat(std::istream& input, CpuSample& sample) {
    CpuSample parsed;
    bool found_aggregate = false;
    std::string text;

    while (std::getline(input, text)) {
        std::istringstream line(text);
        std::string label;
        if (!(line >> label) || !is_cpu_label(label)) continue;

        CpuTimes times;
        if (!parse_cpu_times(line, times)) return false;

        if (label == "cpu") {
            parsed.aggregate = times;
            found_aggregate = true;
        } else {
            parsed.cores.push_back(times);
        }
    }

    if (!found_aggregate) return false;
    sample = std::move(parsed);
    return true;
}

bool parse_meminfo(std::istream& input, MemStats& stats) {
    MemStats parsed;
    uint64_t swap_total_kb = 0;
    uint64_t swap_free_kb = 0;
    bool found_total = false;
    bool found_available = false;
    std::string key;
    uint64_t value = 0;

    while (input >> key >> value) {
        std::string unit;
        std::getline(input, unit);
        if (!key.empty() && key.back() == ':') key.pop_back();

        if (key == "MemTotal") {
            parsed.total_kb = value;
            found_total = true;
        } else if (key == "MemAvailable") {
            parsed.avail_kb = value;
            found_available = true;
        } else if (key == "SwapTotal") {
            swap_total_kb = value;
        } else if (key == "SwapFree") {
            swap_free_kb = value;
        }
    }

    if (!found_total || !found_available) return false;
    parsed.swap_used_kb = swap_total_kb >= swap_free_kb
        ? swap_total_kb - swap_free_kb
        : 0;
    stats = parsed;
    return true;
}

bool parse_diskstats(std::istream& input,
                     const std::unordered_set<std::string>& devices,
                     DiskCounters& counters) {
    DiskCounters parsed;
    bool found_device = false;
    std::string text;

    while (std::getline(input, text)) {
        std::istringstream line(text);
        unsigned int major = 0;
        unsigned int minor = 0;
        std::string name;
        if (!(line >> major >> minor >> name)) continue;
        if (devices.find(name) == devices.end()) continue;

        uint64_t reads_completed = 0;
        uint64_t reads_merged = 0;
        uint64_t sectors_read = 0;
        uint64_t read_ms = 0;
        uint64_t writes_completed = 0;
        uint64_t writes_merged = 0;
        uint64_t sectors_written = 0;
        if (!(line >> reads_completed >> reads_merged >> sectors_read >> read_ms
                  >> writes_completed >> writes_merged >> sectors_written)) {
            return false;
        }

        parsed.read_sectors += sectors_read;
        parsed.write_sectors += sectors_written;
        found_device = true;
    }

    if (!found_device) return false;
    counters = parsed;
    return true;
}

bool parse_net_dev(std::istream& input, NetCounters& counters,
                   bool include_loopback) {
    NetCounters parsed;
    bool found_interface = false;
    std::string text;

    while (std::getline(input, text)) {
        const auto separator = text.find(':');
        if (separator == std::string::npos) continue;

        const std::string interface_name = trim(text.substr(0, separator));
        if (interface_name.empty()) continue;
        if (!include_loopback && interface_name == "lo") continue;

        std::istringstream fields(text.substr(separator + 1));
        uint64_t rx_bytes = 0;
        uint64_t ignored = 0;
        uint64_t tx_bytes = 0;
        if (!(fields >> rx_bytes)) return false;
        for (int i = 0; i < 7; ++i) {
            if (!(fields >> ignored)) return false;
        }
        if (!(fields >> tx_bytes)) return false;

        parsed.rx_bytes += rx_bytes;
        parsed.tx_bytes += tx_bytes;
        found_interface = true;
    }

    if (!found_interface) return false;
    counters = parsed;
    return true;
}

CpuStats calculate_cpu_stats(const CpuSample& previous,
                             const CpuSample& current) {
    CpuStats stats;
    stats.total_pct = percentage(previous.aggregate, current.aggregate);
    stats.per_core_pct.reserve(current.cores.size());

    for (std::size_t i = 0; i < current.cores.size(); ++i) {
        stats.per_core_pct.push_back(i < previous.cores.size()
            ? percentage(previous.cores[i], current.cores[i])
            : 0.0f);
    }
    return stats;
}

DiskStats calculate_disk_stats(const DiskCounters& previous,
                               const DiskCounters& current,
                               double elapsed_seconds) {
    DiskStats stats;
    if (elapsed_seconds <= 0.0) return stats;

    const double read_bytes = static_cast<double>(
        counter_delta(previous.read_sectors, current.read_sectors))
        * kBytesPerDiskSector;
    const double write_bytes = static_cast<double>(
        counter_delta(previous.write_sectors, current.write_sectors))
        * kBytesPerDiskSector;
    stats.read_kbps = static_cast<float>(
        read_bytes / kBytesPerKiB / elapsed_seconds);
    stats.write_kbps = static_cast<float>(
        write_bytes / kBytesPerKiB / elapsed_seconds);
    return stats;
}

NetIfStats calculate_net_stats(const NetCounters& previous,
                               const NetCounters& current,
                               double elapsed_seconds) {
    NetIfStats stats;
    if (elapsed_seconds <= 0.0) return stats;

    stats.rx_kbps = static_cast<float>(static_cast<double>(
        counter_delta(previous.rx_bytes, current.rx_bytes))
        / kBytesPerKiB / elapsed_seconds);
    stats.tx_kbps = static_cast<float>(static_cast<double>(
        counter_delta(previous.tx_bytes, current.tx_bytes))
        / kBytesPerKiB / elapsed_seconds);
    return stats;
}

} // namespace ctopp::detail
