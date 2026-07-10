#pragma once

#include "ctopp/model/packet_record.hpp"
#include <functional>
#include <atomic>
#include <thread>
#include <string>
#include <memory>

namespace ctopp {

class NetworkModel {
public:
    using Callback = std::function<void(const PacketRecord&)>;

    NetworkModel();
    ~NetworkModel();

    // Non-copyable
    NetworkModel(const NetworkModel&) = delete;
    NetworkModel& operator=(const NetworkModel&) = delete;

    // Load the eBPF program and attach to the given network interface.
    // bpf_obj_path should point to the compiled traffic_monitor.bpf.o
    // (default assumes running from the build directory).
    // Must be called before start().
    bool init(const std::string& iface,
              const std::string& bpf_obj_path = "traffic_monitor.bpf.o");

    void set_callback(Callback cb) { callback_ = std::move(cb); }

    // Called from the static ring buffer callback (implementation detail).
    void invoke_callback(const PacketRecord& pkt);

    // Start polling the BPF ring buffer. Requires a successful init() and a
    // callback set first.
    void start();

    // Stop the ring buffer poll thread and detach the BPF program.
    void stop();

    bool is_running() const { return running_.load(); }

    // Returns the number of packets dropped due to ring buffer overflow.
    // Only meaningful when BPF is active; returns 0 otherwise.
    uint64_t dropped_packets() const;

private:
    void poll_loop();

    Callback callback_;
    std::atomic<bool> running_{false};
    std::thread worker_;
    std::string iface_;

    // PIMPL — hides BPF skeleton and ring buffer from the public header.
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ctopp
