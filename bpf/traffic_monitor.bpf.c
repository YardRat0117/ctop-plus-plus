// SPDX-License-Identifier: GPL-2.0
#include <linux/bpf.h>
#include <bpf/bpf_helpers.h>

// ---------------------------------------------------------------------------
// RingBuffer — carries packet metadata from kernel to userspace.
// ---------------------------------------------------------------------------
struct {
    __uint(type, BPF_MAP_TYPE_RINGBUF);
    __uint(max_entries, 256 * 1024); // 256 KB
} packet_events SEC(".maps");

// ---------------------------------------------------------------------------
// Packet metadata (must match userspace PacketRecord layout).
// ---------------------------------------------------------------------------
struct packet_meta {
    __u32 src_ip;
    __u32 dest_ip;
    __u16 src_port;
    __u16 dest_port;
    __u8  protocol;
    __u32 length;
    __u64 timestamp_ns;
};

// ---------------------------------------------------------------------------
// TC egress / ingress program.
//
// Attach with:
//   tc filter add dev <iface> egress bpf obj traffic_monitor.bpf.o sec tc
//   tc filter add dev <iface> ingress bpf obj traffic_monitor.bpf.o sec tc
// ---------------------------------------------------------------------------
SEC("tc")
int traffic_monitor(struct __sk_buff *skb) {
    // --- Allocate ring buffer slot ---
    struct packet_meta *meta = bpf_ringbuf_reserve(&packet_events,
                                                    sizeof(*meta), 0);
    if (!meta) {
        // Ring buffer full — drop this packet (count could be exposed via
        // a perf_event_array or per-CPU map in the future).
        return TC_ACT_OK;
    }

    // --- Parse Ethernet header (14 bytes) ---
    // skb->data / skb->data_end bounds checking is required by the verifier.
    void *data     = (void *)(long)skb->data;
    void *data_end = (void *)(long)skb->data_end;

    // Ethernet header: 6 (dst MAC) + 6 (src MAC) + 2 (ethertype)
    if (data + 14 > data_end)
        goto drop;

    __u16 eth_proto = *(volatile __u16 *)(data + 12);
    __be16 eth_proto_be = __bpf_htons(eth_proto); // htons for verifier-friendly comparison
    int ip_offset = 14;

    // --- Handle VLAN (802.1Q) — optional, 4 extra bytes ---
    if (eth_proto_be == 0x8100) {
        if (data + 18 > data_end)
            goto drop;
        eth_proto_be = __bpf_htons(*(volatile __u16 *)(data + 16));
        ip_offset = 18;
    }

    // --- Only process IPv4 ---
    if (eth_proto_be != 0x0800)
        goto drop;

    // --- Parse IPv4 header (minimum 20 bytes) ---
    if (data + ip_offset + 20 > data_end)
        goto drop;

    __u8  ip_ver_ihl = *(volatile __u8 *)(data + ip_offset);
    __u8  ip_ihl     = ip_ver_ihl & 0x0F;          // header length in 32-bit words
    if (ip_ihl < 5)
        goto drop;

    int ip_hdr_len = ip_ihl * 4;

    // Protocol field (TCP=6, UDP=17, ICMP=1)
    __u8 protocol = *(volatile __u8 *)(data + ip_offset + 9);
    if (protocol != 6 && protocol != 17 && protocol != 1)
        goto drop;

    // Source / destination IP
    __u32 src_ip  = *(volatile __u32 *)(data + ip_offset + 12);
    __u32 dest_ip = *(volatile __u32 *)(data + ip_offset + 16);

    // --- Parse transport layer ports (TCP/UDP only) ---
    __u16 src_port  = 0;
    __u16 dest_port = 0;

    if (protocol == 6 || protocol == 17) {
        // TCP/UDP header starts right after IP header
        if (data + ip_offset + ip_hdr_len + 4 > data_end)
            goto drop;

        src_port  = __bpf_htons(*(volatile __u16 *)(data + ip_offset + ip_hdr_len));
        dest_port = __bpf_htons(*(volatile __u16 *)(data + ip_offset + ip_hdr_len + 2));
    }

    // --- Fill metadata ---
    meta->src_ip      = src_ip;
    meta->dest_ip     = dest_ip;
    meta->src_port    = src_port;
    meta->dest_port   = dest_port;
    meta->protocol    = protocol;
    meta->length      = skb->len;  // full packet length
    meta->timestamp_ns = bpf_ktime_get_ns();

    bpf_ringbuf_submit(meta, 0);
    return TC_ACT_OK;

drop:
    if (meta)
        bpf_ringbuf_discard(meta, 0);
    return TC_ACT_OK;
}

char LICENSE[] SEC("license") = "GPL";
