// SPDX-License-Identifier: GPL-2.0
#include <linux/bpf.h>
#include <linux/pkt_cls.h>
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
// ---------------------------------------------------------------------------
SEC("tc")
int traffic_monitor(struct __sk_buff *skb) {
    // 1. Allocate ring buffer slot
    struct packet_meta *meta = bpf_ringbuf_reserve(&packet_events,
                                                    sizeof(*meta), 0);
    if (!meta) {
        __u32 key = 0;
        __u64 *cnt = bpf_map_lookup_elem(&drop_count, &key);
        if (cnt) __sync_fetch_and_add(cnt, 1);
        return TC_ACT_OK;
    }

    // TC clsact: skb->data points to the Ethernet header (L2),
    // then IP header at offset 14 after the EtherType check.
    void *data     = (void *)(long)skb->data;
    void *data_end = (void *)(long)skb->data_end;

    // 2. Parse Ethernet header (14 bytes).
    if (data + 14 > data_end)
        goto drop;

    __u16 eth_proto = *(volatile __u16 *)(data + 12);
    if (eth_proto != __bpf_htons(0x0800))  // IPv4 only
        goto drop;

    int ip_off = 14;

    // 3. Parse IPv4 header (minimum 20 bytes, starting after Ethernet)
    if (data + ip_off + 20 > data_end)
        goto drop;

    __u8  ip_ver_ihl = *(volatile __u8 *)(data + ip_off);
    __u8  ip_ihl     = ip_ver_ihl & 0x0F;
    if (ip_ihl < 5)
        goto drop;

    int ip_hdr_len = ip_ihl * 4;

    // 4. Protocol (TCP=6, UDP=17, ICMP=1)
    __u8 protocol = *(volatile __u8 *)(data + ip_off + 9);
    if (protocol != 6 && protocol != 17 && protocol != 1)
        goto drop;

    // 5. Source / destination IP
    __u32 src_ip  = *(volatile __u32 *)(data + ip_off + 12);
    __u32 dest_ip = *(volatile __u32 *)(data + ip_off + 16);

    // 6. Transport-layer ports (TCP/UDP only)
    __u16 src_port  = 0;
    __u16 dest_port = 0;
    if (protocol == 6 || protocol == 17) {
        if (data + ip_off + ip_hdr_len + 4 > data_end)
            goto drop;
        src_port  = __bpf_htons(*(volatile __u16 *)(data + ip_off + ip_hdr_len));
        dest_port = __bpf_htons(*(volatile __u16 *)(data + ip_off + ip_hdr_len + 2));
    }

    // 7. Submit
    meta->src_ip       = src_ip;
    meta->dest_ip      = dest_ip;
    meta->src_port     = src_port;
    meta->dest_port    = dest_port;
    meta->protocol     = protocol;
    meta->length       = skb->len;
    meta->timestamp_ns = bpf_ktime_get_ns();

    bpf_ringbuf_submit(meta, 0);
    return TC_ACT_OK;

drop:
    if (meta)
        bpf_ringbuf_discard(meta, 0);
    return TC_ACT_OK;
}

char LICENSE[] SEC("license") = "GPL";
