#pragma once

#include "ctopp/model/packet_record.hpp"
#include "ctopp/viewmodel/net_view_data.hpp"
#include <QObject>
#include <QVariantList>
#include <shared_mutex>
#include <atomic>
#include <cstdint>
#include <unordered_map>

namespace ctopp {

class NetViewModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(float downloadKbps READ downloadKbps NOTIFY dataChanged)
    Q_PROPERTY(float uploadKbps READ uploadKbps NOTIFY dataChanged)
    Q_PROPERTY(quint64 packetCount READ packetCount NOTIFY dataChanged)
    Q_PROPERTY(float tcpPct READ tcpPct NOTIFY dataChanged)
    Q_PROPERTY(float udpPct READ udpPct NOTIFY dataChanged)
    Q_PROPERTY(float icmpPct READ icmpPct NOTIFY dataChanged)
    Q_PROPERTY(QVariantList topTalkers READ topTalkers NOTIFY dataChanged)
    Q_PROPERTY(QVariantList recentPackets READ recentPackets NOTIFY dataChanged)
    Q_PROPERTY(QVariantList downloadHistory READ downloadHistory NOTIFY dataChanged)
    Q_PROPERTY(QVariantList uploadHistory READ uploadHistory NOTIFY dataChanged)
    Q_PROPERTY(quint64 droppedCount READ droppedCount NOTIFY dataChanged)

public:
    explicit NetViewModel(QObject* parent = nullptr);

    // Called by NetworkModel for each packet (Model thread, high frequency).
    void on_packet(const PacketRecord& pkt);

    // Called once per second from the main loop to settle statistics.
    void tick();

    // Called from the main loop to update the dropped-packet counter.
    Q_INVOKABLE void set_dropped(uint64_t count);

    // For tests and direct data access (thread-safe value copy)
    NetViewData get_data() const;

    // --- Property getters (thread-safe) ---
    float downloadKbps() const;
    float uploadKbps() const;
    quint64 packetCount() const;
    float tcpPct() const;
    float udpPct() const;
    float icmpPct() const;
    QVariantList topTalkers() const;
    QVariantList recentPackets() const;
    QVariantList downloadHistory() const;
    QVariantList uploadHistory() const;
    quint64 droppedCount() const;

signals:
    void dataChanged();

private:
    mutable std::shared_mutex mutex_;
    NetViewData data_;

    // Accumulators for the current second (atomic — Model thread writes,
    // tick may be called from UI thread).
    std::atomic<uint64_t> bytes_in_  = 0;
    std::atomic<uint64_t> bytes_out_ = 0;
    std::atomic<uint64_t> pkt_count_ = 0;
    std::atomic<uint64_t> tcp_count_ = 0;
    std::atomic<uint64_t> udp_count_ = 0;
    std::atomic<uint64_t> icmp_count_ = 0;

    // Cumulative per-IP byte counters for Top IP ranking.
    // Accessed under mutex_ in on_packet() and tick().
    std::unordered_map<uint32_t, uint64_t> ip_bytes_;
};

} // namespace ctopp
