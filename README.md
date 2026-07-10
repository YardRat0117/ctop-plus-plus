# ctop++

基于 **eBPF 与 MVVM 架构** 的 Linux 系统可观测性工具，提供系统资源监控与网络流量分析双面板实时可视化。

## 环境要求

| 组件 | 用途 | 安装方式 |
|------|------|---------|
| Linux kernel 5.8+ | BPF RingBuffer 支持 | — |
| CMake 3.20+ | 构建系统 | 系统包管理器 |
| vcpkg | C++ 依赖管理 | [vcpkg.io](https://vcpkg.io) |
| clang | 编译 BPF C 程序 | 系统包管理器 |
| libbpf | BPF 用户态加载 | vcpkg 或 `libbpf-dev` |
| ImGui + ImPlot + GLFW | UI / 图表 | vcpkg (见 vcpkg.json) |

## 构建

```bash
# 安装 vcpkg 依赖并配置
cmake --preset vcpkg -B build

# 编译（含 eBPF 程序）
cmake --build build
```

## 运行

```bash
# eBPF 加载需要特权，开发阶段用 sudo
sudo ./build/ctopp
```

## 开发注意事项

### C++ 头文件

- **`std::unique_lock` 需要显式 `#include <mutex>`。**  
  即便通过 `<shared_mutex>` 已经拿到了 `std::shared_lock`，`unique_lock` 的定义位于独立的 `<mutex>` 头文件中。某些工具链下 `<shared_mutex>` 不会间接引入 `<mutex>`，编译会报 `no member named 'unique_lock' in namespace 'std'`。

### BPF 程序编译

- **必须包含 `<linux/pkt_cls.h>`** — 提供 `TC_ACT_OK` 宏。
- **必须包含 `<bpf/bpf_endian.h>`** — 提供 `__bpf_htons` 等字节序转换函数。
- **需要架构头文件路径** — `<asm/types.h>` 位于 `/usr/include/<arch>-linux-gnu/` 下。  
  手动编译时需加上 `-I/usr/include/x86_64-linux-gnu`（替换为对应架构）。  
  CMakeLists.txt 已自动检测该路径，无需手动指定。

## 架构

详见 [design.md](design.md)。
