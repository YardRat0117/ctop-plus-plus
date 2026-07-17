#pragma once

#include "ctopp/model/sys_stats_snapshot.hpp"
#include "ctopp/viewmodel/sys_view_data.hpp"
#include <QObject>
#include <QVariantList>
#include <shared_mutex>
#include <deque>

namespace ctopp {

class SysViewModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(float cpuTotalPct READ cpuTotalPct NOTIFY dataChanged)
    Q_PROPERTY(float memUsedPct READ memUsedPct NOTIFY dataChanged)
    Q_PROPERTY(quint64 memTotalGb READ memTotalGb NOTIFY dataChanged)
    Q_PROPERTY(quint64 memAvailGb READ memAvailGb NOTIFY dataChanged)
    Q_PROPERTY(quint64 memTotalMb READ memTotalMb NOTIFY dataChanged)
    Q_PROPERTY(quint64 memUsedMb READ memUsedMb NOTIFY dataChanged)
    Q_PROPERTY(float diskReadMbps READ diskReadMbps NOTIFY dataChanged)
    Q_PROPERTY(float diskWriteMbps READ diskWriteMbps NOTIFY dataChanged)
    Q_PROPERTY(float netRxMbps READ netRxMbps NOTIFY dataChanged)
    Q_PROPERTY(float netTxMbps READ netTxMbps NOTIFY dataChanged)
    Q_PROPERTY(QVariantList cpuHistory READ cpuHistory NOTIFY dataChanged)
    Q_PROPERTY(QVariantList memHistory READ memHistory NOTIFY dataChanged)

public:
    explicit SysViewModel(QObject* parent = nullptr);

    // Called by SysStatsModel on each polling tick (Model thread).
    void on_snapshot(const SysStatsSnapshot& snap);

    // For tests and direct data access (thread-safe value copy)
    SysViewData get_data() const;

    // Property getters (thread-safe)
    float cpuTotalPct() const;
    float memUsedPct() const;
    quint64 memTotalGb() const;
    quint64 memAvailGb() const;
    quint64 memTotalMb() const;
    quint64 memUsedMb() const;
    float diskReadMbps() const;
    float diskWriteMbps() const;
    float netRxMbps() const;
    float netTxMbps() const;
    QVariantList cpuHistory() const;
    QVariantList memHistory() const;

signals:
    void dataChanged();

private:
    void push_history(std::deque<float>& hist, float value);

    mutable std::shared_mutex mutex_;
    SysViewData data_;
};

} // namespace ctopp
