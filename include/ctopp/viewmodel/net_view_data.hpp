#pragma once

#include <cstdint>
#include <deque>
#include <vector>
#include <string>

namespace ctopp {

struct NetViewData {
    // --- Throughput ---
    float download_kbps = 0.0f;
    float upload_kbps = 0.0f;
    uint64_t packet_count = 0;   // packets per second

    // --- Protocol distribution (percentage, 0–100) ---
    float tcp_pct = 0.0f;
    float udp_pct = 0.0f;
    float icmp_pct = 0.0f;

    // --- Top talkers ---
    struct IpRank {
        std::string ip;
        uint64_t bytes;
    };
    std::vector<IpRank> top_talkers;  // Top 10 by traffic volume

    // --- Recent packet table ---
    struct PacketRow {
        std::string time;
        std::string src;
        std::string dst;
        std::string protocol;
        uint32_t length;
    };
    static constexpr size_t kMaxRecentPackets = 1000;
    std::deque<PacketRow> recent_packets;

    // --- Speed history (for line charts, up to 60 data points) ---
    static constexpr size_t kMaxHistory = 60;
    std::deque<float> download_history;
    std::deque<float> upload_history;

    // --- Drop counter ---
    uint64_t dropped_count = 0;   // packets dropped due to ring buffer overflow
};

} // namespace ctopp
