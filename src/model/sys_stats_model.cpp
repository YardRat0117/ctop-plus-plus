#include "ctopp/model/sys_stats_model.hpp"
#include <cstdio>

namespace ctopp {

SysStatsModel::~SysStatsModel() {
    stop();
}

void SysStatsModel::start() {
    if (running_.exchange(true)) return;
    worker_ = std::thread(&SysStatsModel::poll_loop, this);
}

void SysStatsModel::stop() {
    if (!running_.exchange(false)) return;
    if (worker_.joinable()) worker_.join();
}

void SysStatsModel::poll_loop() {
    // TODO (同学 A): Read /proc/stat, /proc/meminfo, /proc/diskstats,
    // /proc/net/dev every second, compute deltas, build SysStatsSnapshot,
    // and invoke callback_(snap).
    fprintf(stderr, "[SysStatsModel] poll_loop not implemented yet\n");
}

} // namespace ctopp
