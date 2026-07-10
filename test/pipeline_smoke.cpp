// pipeline_smoke.cpp — smoke test for BPF → NetworkModel → NetViewModel
//
// Build: cmake --build build --target pipe_test
// Run:   sudo ./build/pipe_test
// Then:  ping 127.0.0.1  (or any traffic through the monitored iface)
//
// Does not depend on ImGui, GLFW, or main.cpp — exercises C's pipeline only.

#include "ctopp/model/network_model.hpp"
#include "ctopp/viewmodel/net_view_model.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <csignal>
#include <unistd.h>
#include <thread>
#include <chrono>

static volatile sig_atomic_t g_running = 1;

static void sigint_handler(int) { g_running = 0; }

// ---------------------------------------------------------------
static void print_header() {
    fprintf(stderr, "\n=== ctopp pipeline smoke test ===\n");
    fprintf(stderr, "Monitors traffic through the loopback interface.\n");
    fprintf(stderr, "Run 'ping 127.0.0.1' in another terminal.\n\n");
}

static void print_tick(int sec, const ctopp::NetViewData& d) {
    fprintf(stderr,
            "[pipe_test] T+%ds  pkts=%-6lu  dl=%-8.1fKB/s  ul=%-8.1fKB/s  "
            "tcp=%.0f%%  udp=%.0f%%  icmp=%.0f%%  dropped=%lu\n",
            sec,
            (unsigned long)d.packet_count,
            d.download_kbps,
            d.upload_kbps,
            d.tcp_pct, d.udp_pct, d.icmp_pct,
            (unsigned long)d.dropped_count);
}

static void print_top_talkers(const ctopp::NetViewData& d) {
    fprintf(stderr, "\n--- top talkers (cumulative bytes) ---\n");
    if (d.top_talkers.empty()) {
        fprintf(stderr, "  (no traffic recorded)\n");
        return;
    }
    for (const auto& t : d.top_talkers) {
        fprintf(stderr, "  %-20s  %10lu bytes\n",
                t.ip.c_str(), (unsigned long)t.bytes);
    }
}

static void print_recent(const ctopp::NetViewData& d) {
    fprintf(stderr, "\n--- recent packets (last %zu) ---\n",
            d.recent_packets.size());
    size_t show = d.recent_packets.size() > 15
                      ? 15 : d.recent_packets.size();
    for (size_t i = d.recent_packets.size() - show;
         i < d.recent_packets.size(); ++i) {
        const auto& r = d.recent_packets[i];
        fprintf(stderr, "  %s  %-30s  %-5s  %u\n",
                r.time.c_str(), r.src.c_str(), r.protocol.c_str(), r.length);
    }
    if (d.recent_packets.size() > 15)
        fprintf(stderr, "  ... (%zu more)\n", d.recent_packets.size() - 15);
}

// ---------------------------------------------------------------
int main(int argc, char** argv) {
    signal(SIGINT, sigint_handler);
    print_header();

    const char* iface = (argc > 1) ? argv[1] : "lo";
    const char* bpf_obj =
        (argc > 2) ? argv[2] : "traffic_monitor.bpf.o";

    ctopp::NetworkModel net;
    ctopp::NetViewModel  vm;

    // Wire Model → ViewModel
    net.set_callback([&vm](const ctopp::PacketRecord& pkt) {
        vm.on_packet(pkt);
    });

    // Init
    fprintf(stderr, "[pipe_test] init %s  (bpf_obj=%s)...\n", iface, bpf_obj);
    if (!net.init(iface, bpf_obj)) {
        fprintf(stderr, "[pipe_test] ERROR: init failed (need root? wrong iface?)\n");
        return 1;
    }
    fprintf(stderr, "[pipe_test] init OK\n");

    // Start polling
    net.start();

    // Main tick loop
    fprintf(stderr, "[pipe_test] listening on %s — Ctrl+C to stop\n\n", iface);

    int sec = 0;
    while (g_running && sec < 60) {  // max 60 s
        std::this_thread::sleep_for(std::chrono::seconds(1));
        ++sec;

        vm.tick();
        vm.set_dropped(net.dropped_packets());

        auto data = vm.get_data();
        print_tick(sec, data);
    }

    // Stop
    net.stop();
    fprintf(stderr, "\n[pipe_test] stopped\n");

    // Final report
    auto final_data = vm.get_data();
    print_top_talkers(final_data);
    print_recent(final_data);

    fprintf(stderr, "\n[pipe_test] done.\n");
    return 0;
}
