#include "ctopp/model/sys_stats_model.hpp"
#include "sys_stats_reader.hpp"

#include <chrono>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <unordered_set>
#include <utility>

namespace ctopp {
namespace {

using Clock = std::chrono::steady_clock;

struct RawSystemSample {
    // Cumulative counters used to calculate rates between samples.
    detail::CpuSample cpu;
    MemStats mem;
    detail::DiskCounters disk;
    detail::NetCounters net;
    Clock::time_point captured_at;
};

bool is_ignored_block_device(const std::string& name) {
    // Ignore virtual memory-backed, loop and optical devices.
    constexpr const char* ignored_prefixes[] = {
        "loop", "ram", "zram", "fd", "sr"
    };
    for (const char* prefix : ignored_prefixes) {
        if (name.rfind(prefix, 0) == 0) return true;
    }
    return false;
}

std::unordered_set<std::string> enumerate_block_devices() {
    // /sys/block lists whole devices, avoiding partition double-counting.
    std::unordered_set<std::string> devices;
    std::error_code error;
    const std::filesystem::path sys_block{"/sys/block"};

    for (std::filesystem::directory_iterator it(sys_block, error), end;
         !error && it != end; it.increment(error)) {
        const std::string name = it->path().filename().string();
        if (!name.empty() && !is_ignored_block_device(name)) {
            devices.insert(name);
        }
    }

    if (error) {
        std::fprintf(stderr,
                     "[SysStatsModel] cannot enumerate /sys/block: %s\n",
                     error.message().c_str());
    }
    return devices;
}

bool collect_raw_sample(const std::unordered_set<std::string>& block_devices,
                        RawSystemSample& sample) {
    // Publish only complete samples from all four Linux data sources.
    RawSystemSample collected;

    std::ifstream proc_stat("/proc/stat");
    if (!proc_stat || !detail::parse_proc_stat(proc_stat, collected.cpu)) {
        std::fprintf(stderr, "[SysStatsModel] cannot parse /proc/stat\n");
        return false;
    }

    std::ifstream meminfo("/proc/meminfo");
    if (!meminfo || !detail::parse_meminfo(meminfo, collected.mem)) {
        std::fprintf(stderr, "[SysStatsModel] cannot parse /proc/meminfo\n");
        return false;
    }

    std::ifstream diskstats("/proc/diskstats");
    if (!diskstats
        || !detail::parse_diskstats(diskstats, block_devices, collected.disk)) {
        std::fprintf(stderr, "[SysStatsModel] cannot parse /proc/diskstats\n");
        return false;
    }

    std::ifstream net_dev("/proc/net/dev");
    if (!net_dev || !detail::parse_net_dev(net_dev, collected.net)) {
        std::fprintf(stderr, "[SysStatsModel] cannot parse /proc/net/dev\n");
        return false;
    }

    collected.captured_at = Clock::now();
    sample = std::move(collected);
    return true;
}

} // namespace

SysStatsModel::~SysStatsModel() {
    stop();
}

void SysStatsModel::set_callback(Callback cb) {
    std::lock_guard lock(callback_mutex_);
    callback_ = std::move(cb);
}

void SysStatsModel::start() {
    if (running_.exchange(true)) return;

    {
        std::lock_guard lock(callback_mutex_);
        if (!callback_) {
            std::fprintf(stderr,
                         "[SysStatsModel] start() requires a callback\n");
            running_ = false;
            return;
        }
    }

    try {
        worker_ = std::thread(&SysStatsModel::poll_loop, this);
    } catch (const std::exception& error) {
        running_ = false;
        std::fprintf(stderr, "[SysStatsModel] cannot start worker: %s\n",
                     error.what());
    }
}

void SysStatsModel::stop() {
    running_ = false;
    wakeup_.notify_all();
    if (worker_.joinable()) worker_.join();
}

void SysStatsModel::poll_loop() {
    const auto block_devices = enumerate_block_devices();
    if (block_devices.empty()) {
        std::fprintf(stderr,
                     "[SysStatsModel] no supported block devices found\n");
    }

    // First sample establishes the baseline for cumulative counters.
    RawSystemSample previous;
    bool have_previous = collect_raw_sample(block_devices, previous);
    auto next_sample = Clock::now();

    while (running_.load()) {
        next_sample += std::chrono::seconds(1);
        {
            std::unique_lock lock(wait_mutex_);
            if (wakeup_.wait_until(lock, next_sample,
                                   [this] { return !running_.load(); })) {
                break;
            }
        }

        RawSystemSample current;
        if (!collect_raw_sample(block_devices, current)) continue;
        if (!have_previous) {
            previous = std::move(current);
            have_previous = true;
            continue;
        }

        // Use actual elapsed time to account for scheduler delay.
        const double elapsed_seconds =
            std::chrono::duration<double>(current.captured_at
                                          - previous.captured_at).count();
        SysStatsSnapshot snapshot;
        snapshot.cpu = detail::calculate_cpu_stats(previous.cpu, current.cpu);
        snapshot.mem = current.mem;
        snapshot.disk = detail::calculate_disk_stats(
            previous.disk, current.disk, elapsed_seconds);
        snapshot.net_if = detail::calculate_net_stats(
            previous.net, current.net, elapsed_seconds);
        previous = std::move(current);

        // Copy under lock, invoke outside it to avoid callback deadlocks.
        Callback callback;
        {
            std::lock_guard lock(callback_mutex_);
            callback = callback_;
        }
        if (!callback) continue;

        try {
            callback(snapshot);
        } catch (const std::exception& error) {
            std::fprintf(stderr, "[SysStatsModel] callback failed: %s\n",
                         error.what());
        } catch (...) {
            std::fprintf(stderr,
                         "[SysStatsModel] callback failed with unknown error\n");
        }
    }
}

} // namespace ctopp
