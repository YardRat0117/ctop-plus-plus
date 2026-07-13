#pragma once

#include "ctopp/model/sys_stats_snapshot.hpp"
#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

namespace ctopp {

class SysStatsModel {
public:
    using Callback = std::function<void(const SysStatsSnapshot&)>;

    SysStatsModel() = default;
    ~SysStatsModel();

    // Non-copyable — owns a polling thread and synchronization state.
    SysStatsModel(const SysStatsModel&) = delete;
    SysStatsModel& operator=(const SysStatsModel&) = delete;

    // Invoked on the polling thread; the receiver must outlive the running model.
    void set_callback(Callback cb);

    // Start polling /proc and /sys at ~1Hz. Must have a callback set first.
    void start();

    // Stop the polling thread. Safe to call repeatedly.
    void stop();

    bool is_running() const { return running_.load(); }

private:
    void poll_loop();

    Callback callback_;
    mutable std::mutex callback_mutex_;
    std::atomic<bool> running_{false};
    std::thread worker_;
    // Interruptible 1 Hz polling wait.
    std::mutex wait_mutex_;
    std::condition_variable wakeup_;
};

} // namespace ctopp
