#pragma once

#include "ctopp/model/packet_record.hpp"
#include <functional>
#include <atomic>
#include <thread>
#include <string>

namespace ctopp {

class NetworkModel {
public:
    using Callback = std::function<void(const PacketRecord&)>;

    NetworkModel() = default;
    ~NetworkModel();

    // Non-copyable, movable
    NetworkModel(const NetworkModel&) = delete;
    NetworkModel& operator=(const NetworkModel&) = delete;

    // Load the eBPF program and attach to the given network interface.
    // Must be called before start().
    bool init(const std::string& iface);

    void set_callback(Callback cb) { callback_ = std::move(cb); }

    // Start polling the BPF ring buffer. Requires a successful init() and a
    // callback set first.
    void start();

    // Stop the ring buffer poll thread and detach the BPF program.
    void stop();

    bool is_running() const { return running_.load(); }

private:
    void poll_loop();

    Callback callback_;
    std::atomic<bool> running_{false};
    std::thread worker_;
    std::string iface_;
    // BPF skeleton will be stored here (implementation detail).
};

} // namespace ctopp
