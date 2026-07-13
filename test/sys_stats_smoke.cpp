// Live smoke test for SysStatsModel -> SysViewModel on Linux.
// Build with CMake, then run: ./build/sys_stats_smoke

#include "ctopp/model/sys_stats_model.hpp"
#include "ctopp/viewmodel/sys_view_model.hpp"

#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <mutex>

int main() {
    ctopp::SysStatsModel model;
    ctopp::SysViewModel view_model;
    std::mutex mutex;
    std::condition_variable updated;
    int sample_count = 0;

    model.set_callback([&](const ctopp::SysStatsSnapshot& snapshot) {
        view_model.on_snapshot(snapshot);
        {
            std::lock_guard lock(mutex);
            ++sample_count;
        }
        updated.notify_one();
    });

    model.start();
    {
        std::unique_lock lock(mutex);
        updated.wait_for(lock, std::chrono::seconds(4),
                         [&] { return sample_count >= 2; });
    }
    model.stop();

    const auto data = view_model.get_data();
    if (sample_count == 0 || data.cpu_per_core_pct.empty()
        || data.mem_total_gb == 0) {
        std::fprintf(stderr,
                     "sys_stats_smoke: no complete Linux sample received\n");
        return 1;
    }

    std::fprintf(stderr,
                 "sys_stats_smoke: samples=%d cpu=%.1f%% mem=%.1f%% "
                 "disk=%.2f/%.2f MiB/s net=%.2f/%.2f MiB/s cores=%zu\n",
                 sample_count, data.cpu_total_pct, data.mem_used_pct,
                 data.disk_read_mbps, data.disk_write_mbps,
                 data.net_rx_mbps, data.net_tx_mbps,
                 data.cpu_per_core_pct.size());
    return 0;
}
