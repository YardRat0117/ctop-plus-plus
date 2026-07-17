#include "ctopp/viewmodel/sys_view_model.hpp"
#include <QMetaObject>
#include <algorithm>
#include <mutex>

namespace ctopp {

SysViewModel::SysViewModel(QObject* parent)
    : QObject(parent)
{
}

void SysViewModel::on_snapshot(const SysStatsSnapshot& snap) {
    {
        std::unique_lock lock(mutex_);

        data_.cpu_total_pct = snap.cpu.total_pct;
        data_.cpu_per_core_pct = snap.cpu.per_core_pct;

        const float mem_used_pct = snap.mem.total_kb > 0
            ? (1.0f - static_cast<float>(snap.mem.avail_kb)
                         / static_cast<float>(snap.mem.total_kb)) * 100.0f
            : 0.0f;
        data_.mem_used_pct = std::clamp(mem_used_pct, 0.0f, 100.0f);
        data_.mem_total_gb = snap.mem.total_kb / (1024 * 1024);
        data_.mem_avail_gb = snap.mem.avail_kb / (1024 * 1024);
        data_.mem_total_mb = snap.mem.total_kb / 1024;       // KB → MB
        data_.mem_used_mb  = (snap.mem.total_kb > snap.mem.avail_kb
                            ? snap.mem.total_kb - snap.mem.avail_kb
                            : 0ULL) / 1024;
        data_.disk_read_mbps = snap.disk.read_kbps / 1024.0f;
        data_.disk_write_mbps = snap.disk.write_kbps / 1024.0f;
        data_.net_rx_mbps = snap.net_if.rx_kbps / 1024.0f;
        data_.net_tx_mbps = snap.net_if.tx_kbps / 1024.0f;

        push_history(data_.cpu_history, data_.cpu_total_pct);
        push_history(data_.mem_history, data_.mem_used_pct);
    }

    // Notify QML bindings on the main thread
    QMetaObject::invokeMethod(this, "dataChanged", Qt::QueuedConnection);
}

SysViewData SysViewModel::get_data() const {
    std::shared_lock lock(mutex_);
    return data_;
}

float SysViewModel::cpuTotalPct() const {
    std::shared_lock lock(mutex_);
    return data_.cpu_total_pct;
}

float SysViewModel::memUsedPct() const {
    std::shared_lock lock(mutex_);
    return data_.mem_used_pct;
}

quint64 SysViewModel::memTotalGb() const {
    std::shared_lock lock(mutex_);
    return data_.mem_total_gb;
}

quint64 SysViewModel::memAvailGb() const {
    std::shared_lock lock(mutex_);
    return data_.mem_avail_gb;
}

quint64 SysViewModel::memTotalMb() const {
    std::shared_lock lock(mutex_);
    return data_.mem_total_mb;
}

quint64 SysViewModel::memUsedMb() const {
    std::shared_lock lock(mutex_);
    return data_.mem_used_mb;
}

float SysViewModel::diskReadMbps() const {
    std::shared_lock lock(mutex_);
    return data_.disk_read_mbps;
}

float SysViewModel::diskWriteMbps() const {
    std::shared_lock lock(mutex_);
    return data_.disk_write_mbps;
}

float SysViewModel::netRxMbps() const {
    std::shared_lock lock(mutex_);
    return data_.net_rx_mbps;
}

float SysViewModel::netTxMbps() const {
    std::shared_lock lock(mutex_);
    return data_.net_tx_mbps;
}

QVariantList SysViewModel::cpuHistory() const {
    std::shared_lock lock(mutex_);
    QVariantList list;
    list.reserve(static_cast<int>(data_.cpu_history.size()));
    for (float v : data_.cpu_history)
        list.append(v);
    return list;
}

QVariantList SysViewModel::memHistory() const {
    std::shared_lock lock(mutex_);
    QVariantList list;
    list.reserve(static_cast<int>(data_.mem_history.size()));
    for (float v : data_.mem_history)
        list.append(v);
    return list;
}

void SysViewModel::push_history(std::deque<float>& hist, float value) {
    hist.push_back(value);
    if (hist.size() > SysViewData::kMaxHistory)
        hist.pop_front();
}

} // namespace ctopp
