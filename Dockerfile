# =============================================================================
# ctop++ — Docker 构建镜像
# =============================================================================
# 构建（单行命令）：
#   docker build -t ctopp .
#
# 运行（需特权模式才能加载 eBPF + 访问 X11）：
#   docker run --rm -it --privileged \
#       -e DISPLAY=$DISPLAY \
#       -v /tmp/.X11-unix:/tmp/.X11-unix \
#       -v /sys/kernel/btf:/sys/kernel/btf:ro \
#       ctopp ./ctopp wlp2s0
# =============================================================================

FROM debian:stable-slim AS builder

SHELL ["/bin/bash", "-c"]

# ---------------------------------------------------------------------------
# 系统依赖
# ---------------------------------------------------------------------------
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    # 编译工具
    clang cmake curl git make \
    # eBPF
    libbpf-dev bpftool \
    # OpenGL / GLFW
    libgl1-mesa-dev libglu1-mesa-dev \
    libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev \
    # 构建工具
    pkg-config ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# ---------------------------------------------------------------------------
# vcpkg（C++ 依赖管理）
# ---------------------------------------------------------------------------
ENV VCPKG_ROOT=/opt/vcpkg
RUN git clone --depth=1 https://github.com/microsoft/vcpkg $VCPKG_ROOT \
    && $VCPKG_ROOT/bootstrap-vcpkg.sh -disableMetrics

# ---------------------------------------------------------------------------
# 构建 ctopp
# ---------------------------------------------------------------------------
WORKDIR /ctopp
COPY . .

# vcpkg manifest 模式自动安装依赖（imgui、implot、glfw3）
RUN cmake --preset vcpkg -B build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build --parallel $(nproc)
