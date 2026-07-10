#pragma once

#include <cstdint>

namespace ctopp {

struct PacketRecord {
    uint32_t src_ip;
    uint32_t dest_ip;
    uint16_t src_port;
    uint16_t dest_port;
    uint8_t  protocol;       // 6=TCP, 17=UDP, 1=ICMP
    uint32_t length;         // packet length in bytes
    uint64_t timestamp_ns;   // kernel timestamp in nanoseconds
};

} // namespace ctopp
