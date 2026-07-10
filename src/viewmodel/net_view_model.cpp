#include "ctopp/viewmodel/net_view_model.hpp"
#include <mutex>
#include <unordered_map>
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <arpa/inet.h>

namespace ctopp {

void NetViewModel::on_packet(const PacketRecord& pkt) {
    // Accumulate into current-second counters (no lock — single writer).
    pkt_count_++;

    switch (pkt.protocol) {
        case 6:  tcp_count_++;  break;
        case 17: udp_count_++;  break;
        case 1:  icmp_count_++; break;
    }

    // Direction heuristic: if src is a private IP and dst is public → upload.
    // Simplified for now — just add to bytes_in_ (will be refined in tick()).
    bytes_in_ += pkt.length;

    // Build a PacketRow for the recent-packets table (under lock).
    {
        std::unique_lock lock(mutex_);

        char src_buf[INET_ADDRSTRLEN], dst_buf[INET_ADDRSTRLEN];
        struct in_addr s = { .s_addr = pkt.src_ip };
        struct in_addr d = { .s_addr = pkt.dest_ip };
        inet_ntop(AF_INET, &s, src_buf, sizeof(src_buf));
        inet_ntop(AF_INET, &d, dst_buf, sizeof(dst_buf));

        const char* proto_str = "???";
        switch (pkt.protocol) {
            case 6:  proto_str = "TCP";  break;
            case 17: proto_str = "UDP";  break;
            case 1:  proto_str = "ICMP"; break;
        }

        char time_buf[32];
        snprintf(time_buf, sizeof(time_buf), "%llu",
                 (unsigned long long)(pkt.timestamp_ns / 1000000));

        data_.recent_packets.push_back({
            time_buf,
            std::string(src_buf) + ":" + std::to_string(pkt.src_port),
            std::string(dst_buf) + ":" + std::to_string(pkt.dest_port),
            proto_str,
            pkt.length
        });

        if (data_.recent_packets.size() > NetViewData::kMaxRecentPackets) {
            data_.recent_packets.pop_front();
        }
    }
}

void NetViewModel::tick() {
    // Called once per second — finalise stats for the previous second.
    std::unique_lock lock(mutex_);

    if (pkt_count_ > 0) {
        uint64_t tcp  = tcp_count_;
        uint64_t udp  = udp_count_;
        uint64_t icmp = icmp_count_;
        uint64_t total_proto = tcp + udp + icmp;
        data_.tcp_pct   = total_proto > 0 ? (100.0f * tcp  / total_proto) : 0.0f;
        data_.udp_pct   = total_proto > 0 ? (100.0f * udp  / total_proto) : 0.0f;
        data_.icmp_pct  = total_proto > 0 ? (100.0f * icmp / total_proto) : 0.0f;

        data_.download_kbps = static_cast<float>(bytes_in_)  / 1024.0f;
        data_.upload_kbps   = static_cast<float>(bytes_out_) / 1024.0f;

        data_.packet_count = pkt_count_;
    }

    // Maintain history
    data_.download_history.push_back(data_.download_kbps);
    if (data_.download_history.size() > NetViewData::kMaxHistory)
        data_.download_history.pop_front();
    data_.upload_history.push_back(data_.upload_kbps);
    if (data_.upload_history.size() > NetViewData::kMaxHistory)
        data_.upload_history.pop_front();

    // Reset accumulators
    bytes_in_  = 0;
    bytes_out_ = 0;
    pkt_count_ = 0;
    tcp_count_ = 0;
    udp_count_ = 0;
    icmp_count_ = 0;
}

NetViewData NetViewModel::get_data() const {
    std::shared_lock lock(mutex_);
    return data_;
}

} // namespace ctopp
