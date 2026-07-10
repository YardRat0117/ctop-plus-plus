#include "ctopp/model/network_model.hpp"
#include <cstdio>

namespace ctopp {

NetworkModel::~NetworkModel() {
    stop();
}

bool NetworkModel::init(const std::string& iface) {
    iface_ = iface;
    // TODO (同学 C): Load BPF skeleton, attach TC ingress/egress programs
    // to iface_.
    fprintf(stderr, "[NetworkModel] init(%s) stub\n", iface_.c_str());
    return true; // stub: pretend success
}

void NetworkModel::start() {
    if (running_.exchange(true)) return;
    worker_ = std::thread(&NetworkModel::poll_loop, this);
}

void NetworkModel::stop() {
    if (!running_.exchange(false)) return;
    if (worker_.joinable()) worker_.join();
    // TODO: detach BPF program, destroy skeleton
}

void NetworkModel::poll_loop() {
    // TODO (同学 C): ring_buffer__poll() loop, read PacketRecord from
    // BPF ring buffer, invoke callback_(pkt).
    fprintf(stderr, "[NetworkModel] poll_loop not implemented yet\n");
}

} // namespace ctopp
