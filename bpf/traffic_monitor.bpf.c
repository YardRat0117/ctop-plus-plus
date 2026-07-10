// SPDX-License-Identifier: GPL-2.0
#include <linux/bpf.h>
#include <linux/pkt_cls.h>
#include <linux/if_ether.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_endian.h>

// ---------------------------------------------------------------------------
// RingBuffer — carries packet metadata from kernel to userspace.
// ---------------------------------------------------------------------------
struct {
    __uint(type, BPF_MAP_TYPE_RINGBUF);
    __uint(max_entries, 256 * 1024); // 256 KB
} packet_events SEC(".maps");

// ---------------------------------------------------------------------------
// Drop counter — incremented when the ring buffer is full.
// ---------------------------------------------------------------------------
struct {
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __uint(max_entries, 1);
    __type(key, __u32);
    __type(value, __u64);
} drop_count SEC(".maps");

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
        // Ring buffer full — increment drop counter.
        __u32 key = 0;
        __u64 *cnt = bpf_map_lookup_elem(&drop_count, &key);
        if (cnt) __sync_fetch_and_add(cnt, 1);
        return TC_ACT_OK;
    }

    // TC clsact: skb->data points directly to the IP header (L3).
    // skb->data / skb->data_end bounds checking is required by the verifier.
    void *data     = (void *)(long)skb->data;
    void *data_end = (void *)(long)skb->data_end;

    // Only process IPv4
    if (skb->protocol != bpf_htons(ETH_P_IP))
        goto drop;

    // --- Parse IPv4 header (minimum 20 bytes) ---
    if (data + 20 > data_end)
        goto drop;

    __u8  ip_ver_ihl = *(volatile __u8 *)(data);
    __u8  ip_ihl     = ip_ver_ihl & 0x0F;
    if (ip_ihl < 5)
        goto drop;

    int ip_hdr_len = ip_ihl * 4;

    // Protocol field (TCP=6, UDP=17, ICMP=1)
    __u8 protocol = *(volatile __u8 *)(data + 9);
    if (protocol != 6 && protocol != 17 && protocol != 1)
        goto drop;

    // Source / destination IP
    __u32 src_ip  = *(volatile __u32 *)(data + 12);
    __u32 dest_ip = *(volatile __u32 *)(data + 16);

    // --- Parse transport layer ports (TCP/UDP only) ---
    __u16 src_port  = 0;
    __u16 dest_port = 0;

    if (protocol == 6 || protocol == 17) {
        if (data + ip_hdr_len + 4 > data_end)
            goto drop;

        src_port  = __bpf_htons(*(volatile __u16 *)(data + ip_hdr_len));
        dest_port = __bpf_htons(*(volatile __u16 *)(data + ip_hdr_len + 2));
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
