#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
BUILD="$ROOT/build"

# ---------- colors ----------
RED='\033[0;31m'; GREEN='\033[0;32m'; CYAN='\033[0;36m'; NC='\033[0m'
info()  { echo -e "${CYAN}[demo]${NC} $*"; }
ok()    { echo -e "${GREEN}[demo]${NC} $*"; }
err()   { echo -e "${RED}[demo]${NC} $*"; exit 1; }

# ---------- step 1: configure ----------
info "Configuring with vcpkg…"
cmake --preset vcpkg -B "$BUILD" -DCMAKE_BUILD_TYPE=Release &>/dev/null

# ---------- step 2: build ----------
info "Building…"
cmake --build "$BUILD" -j"$(nproc)" 2>&1 | tail -3
ok "Build complete."

# ---------- step 3: eBPF capability ----------
CTOPP="$BUILD/ctopp"
BPF_DEPS="cap_bpf,cap_net_admin,cap_perfmon"

if capsh --has-cap="$BPF_DEPS" 2>/dev/null || setcap -v "$BPF_DEPS+ep" "$CTOPP" &>/dev/null; then
    info "eBPF capabilities already set."
else
    info "Setting eBPF capabilities (requires sudo once)…"
    sudo setcap "$BPF_DEPS+ep" "$CTOPP" || info "Could not setcap; BPF will need sudo."
fi

# ---------- step 4: pick interface ----------
# Dry-run flag for CI: just build, skip running
if [ "${1:-}" = "--dry-run" ]; then
    info "Dry-run mode — build only, skipping execution."
    exit 0
fi

IFACE="${1:-}"
if [ -z "$IFACE" ]; then
    # pick first non-loopback, non-docker interface
    IFACE=$(ip -json link show | python3 -c "
import json, sys
for iface in json.load(sys.stdin):
    name = iface['ifname']
    if name not in ('lo',) and not name.startswith(('docker', 'br-', 'veth', 'cali')):
        print(name)
        break
" 2>/dev/null || echo "lo")
fi
info "Using interface: ${IFACE:-lo}"

# ---------- step 5: run ----------
echo ""
info "${GREEN}Starting ctop++${NC}"
info "  System stats:  CPU · Memory · Disk I/O · Network (from /proc)"
info "  Network flow:  Packet table · Throughput · Top IP · Protocol pie"
echo ""

# Try to resolve display for sudo / setcap scenario
if [ "$EUID" -eq 0 ] && [ -n "${DISPLAY:-}" ]; then
    # running as root with a display — likely need xhost
    xhost +SI:localuser:root &>/dev/null || true
elif [ -z "${DISPLAY:-}" ] && [ -z "${WAYLAND_DISPLAY:-}" ]; then
    info "No display detected — running in offscreen mode (log only)."
fi

exec "$CTOPP" "$IFACE"
