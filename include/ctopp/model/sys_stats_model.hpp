#pragma once

#include "ctopp/model/sys_stats_snapshot.hpp"
#include <functional>
#include <atomic>
#include <thread>
#include <string>

namespace ctopp {

class SysStatsModel {
public:
    using Callback = std::function<void(const SysStatsSnapshot&)>;

    SysStatsModel() = default;
    ~SysStatsModel();

    // Non-copyable, movable
    SysStatsModel(const SysStatsModel&) = delete;
    SysStatsModel& operator=(const SysStatsModel&) = delete;

    void set_callback(Callback cb) { callback_ = std::move(cb); }

    // Start polling /proc and /sys at ~1Hz. Must have a callback set first.
    void start();

    // Stop the polling thread.
    void stop();

    bool is_running() const { return running_.load(); }

private:
    void poll_loop();

    Callback callback_;
    std::atomic<bool> running_{false};
    std::thread worker_;
};

} // namespace ctopp
