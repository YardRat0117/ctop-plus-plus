#include "ctopp/viewmodel/sys_view_model.hpp"
#include <mutex>

namespace ctopp {

void SysViewModel::on_snapshot(const SysStatsSnapshot& snap) {
    // TODO (同学 A): Convert snapshot → SysViewData, maintain history deque,
    // perform CPU percentage calculation (via two-point diff).
    std::unique_lock lock(mutex_);

    data_.cpu_total_pct = snap.cpu.total_pct;
    data_.mem_used_pct = snap.mem.total_kb > 0
        ? (1.0f - static_cast<float>(snap.mem.avail_kb) / snap.mem.total_kb) * 100.0f
        : 0.0f;
    data_.mem_total_gb = snap.mem.total_kb / (1024 * 1024);
    data_.mem_avail_gb = snap.mem.avail_kb / (1024 * 1024);
    data_.disk_read_mbps = snap.disk.read_kbps / 1000.0f;
    data_.disk_write_mbps = snap.disk.write_kbps / 1000.0f;

    push_history(data_.cpu_history, snap.cpu.total_pct);
    push_history(data_.mem_history, data_.mem_used_pct);
}

SysViewData SysViewModel::get_data() const {
    std::shared_lock lock(mutex_);
    return data_;
}

void SysViewModel::push_history(std::deque<float>& hist, float value) {
    hist.push_back(value);
    if (hist.size() > SysViewData::kMaxHistory) {
        hist.pop_front();
    }
}

} // namespace ctopp
