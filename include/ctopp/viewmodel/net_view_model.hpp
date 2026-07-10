#pragma once

#include "ctopp/model/packet_record.hpp"
#include "ctopp/viewmodel/net_view_data.hpp"
#include <shared_mutex>
#include <cstdint>

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

private:
    mutable std::shared_mutex mutex_;
    NetViewData data_;

    // Accumulators for the current second (no lock needed — only written
    // by on_packet / tick from the Model thread).
    uint64_t bytes_in_     = 0;
    uint64_t bytes_out_    = 0;
    uint64_t pkt_count_    = 0;
    uint64_t tcp_count_    = 0;
    uint64_t udp_count_    = 0;
    uint64_t icmp_count_   = 0;
};

} // namespace ctopp
