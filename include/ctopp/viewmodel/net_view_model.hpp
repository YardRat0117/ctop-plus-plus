#pragma once

#include "ctopp/model/packet_record.hpp"
#include "ctopp/viewmodel/net_view_data.hpp"
#include <shared_mutex>
#include <atomic>
#include <cstdint>
#include <unordered_map>

namespace ctopp {

class NetViewModel {
public:
    NetViewModel() = default;

    // Called by NetworkModel for each packet (Model thread, high frequency).
    void on_packet(const PacketRecord& pkt);

    // Called once per second (e.g. from a timer or the main loop) to
    // finalise the current second's statistics and reset accumulators.
    void tick();

    // Called by View (UI thread). Thread-safe read access.
    NetViewData get_data() const;

    // Update the dropped-packet counter from NetworkModel.
    void set_dropped(uint64_t count);

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
