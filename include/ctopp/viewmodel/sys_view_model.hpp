#pragma once

#include "ctopp/model/sys_stats_snapshot.hpp"
#include "ctopp/viewmodel/sys_view_data.hpp"
#include <shared_mutex>

namespace ctopp {

class SysViewModel {
public:
    SysViewModel() = default;

    // Called by SysStatsModel on each polling tick (Model thread).
    void on_snapshot(const SysStatsSnapshot& snap);

    // Called by View (UI thread). Thread-safe read access.
    SysViewData get_data() const;

private:
    void push_history(std::deque<float>& hist, float value);

    mutable std::shared_mutex mutex_;
    SysViewData data_;
};

} // namespace ctopp
