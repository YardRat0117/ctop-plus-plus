#include "ctopp/viewmodel/net_view_model.hpp"
#include <mutex>
#include <unordered_map>
#include <algorithm>
#include <cstdio>
#include <ctime>
#include <arpa/inet.h>

namespace ctopp {

// rfc 1918 private IP ranges
static bool is_private_ip(uint32_t ip) {
    uint8_t b0 = ip & 0xFF;
    uint8_t b1 = (ip >> 8) & 0xFF;
    return (b0 == 10)
        || (b0 == 172 && b1 >= 16 && b1 <= 31)
        || (b0 == 192 && b1 == 168);
}

void NetViewModel::on_packet(const PacketRecord& pkt) {
    // Accumulate into current-second counters (atomic — Model thread).
    pkt_count_++;

    switch (pkt.protocol) {
        case 6:  tcp_count_++;  break;
        case 17: udp_count_++;  break;
        case 1:  icmp_count_++; break;
    }

    // Direction heuristic: private src + public dst → upload, else download
    if (is_private_ip(pkt.src_ip) && !is_private_ip(pkt.dest_ip))
        bytes_out_ += pkt.length;
    else
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

        // Human-readable timestamp: HH:MM:SS.mmm
        char time_buf[16];
        time_t sec  = pkt.timestamp_ns / 1000000000ULL;
        int    ms   = (pkt.timestamp_ns / 1000000ULL) % 1000;
        struct tm tm_buf;
        localtime_r(&sec, &tm_buf);
        snprintf(time_buf, sizeof(time_buf), "%02d:%02d:%02d.%03d",
                 tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec, ms);

        data_.recent_packets.push_back({
            time_buf,
            std::string(src_buf) + ":" + std::to_string(pkt.src_port),
            std::string(dst_buf) + ":" + std::to_string(pkt.dest_port),
            proto_str,
            pkt.length
        });

        if (data_.recent_packets.size() > NetViewData::kMaxRecentPackets)
            data_.recent_packets.pop_front();

        // Accumulate per-IP byte counters for Top IP ranking.
        ip_bytes_[pkt.src_ip]  += pkt.length;
        ip_bytes_[pkt.dest_ip] += pkt.length;
    }
}

void NetViewModel::tick() {
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

    // Top IP ranking (cumulative, updated every tick)
    {
        std::vector<std::pair<uint32_t, uint64_t>> ranked(
            ip_bytes_.begin(), ip_bytes_.end());
        size_t top_n = std::min<size_t>(ranked.size(), 10);
        std::partial_sort(ranked.begin(), ranked.begin() + top_n,
                          ranked.end(),
                          [](const auto& a, const auto& b) {
                              return a.second > b.second;
                          });
        data_.top_talkers.clear();
        for (size_t i = 0; i < top_n; ++i) {
            struct in_addr addr = { .s_addr = ranked[i].first };
            char buf[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &addr, buf, sizeof(buf));
            data_.top_talkers.push_back({buf, ranked[i].second});
        }
    }

    // Maintain history
    data_.download_history.push_back(data_.download_kbps);
    if (data_.download_history.size() > NetViewData::kMaxHistory)
        data_.download_history.pop_front();
    data_.upload_history.push_back(data_.upload_kbps);
    if (data_.upload_history.size() > NetViewData::kMaxHistory)
        data_.upload_history.pop_front();

    // Reset per-second accumulators
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

void NetViewModel::set_dropped(uint64_t count) {
    std::unique_lock lock(mutex_);
    data_.dropped_count = count;
}

} // namespace ctopp
