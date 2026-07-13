# ctop++ 系统可观测性工具 — 中期报告

> 构建 PDF：`cd docs && docker build -t ctopp-report -f Dockerfile.report . && docker run --rm -v $(pwd):/docs ctopp-report`

---

## 第一章 项目概述

### 1.1 项目需求

ctop++ 是一个基于 **eBPF 与 MVVM 架构** 的 Linux 系统可观测性工具，提供双面板实时监控：

| 面板 | 内容 | 数据源 |
|------|------|--------|
| **系统资源监控（System Stats）** | CPU 使用率、内存占用、磁盘 I/O | `/proc` + `/sys` 用户态轮询（1Hz） |
| **网络流量分析（Network Traffic）** | 包元数据捕获、吞吐量、Top IP、协议占比 | eBPF 内核探针 → RingBuffer → libbpf |

目标平台：Linux kernel 5.8+（BPF RingBuffer 支持）

### 1.2 技术栈选型

| 组件 | 选型理由 |
|------|---------|
| **eBPF（TC hook）** | 无需修改内核，安全高效地捕获网络包元数据；RingBuffer 零拷贝传递到用户态 |
| **libbpf** | BPF 程序加载与管理的标准库，支持 CO-RE（一次编译，到处运行） |
| **ImGui + ImPlot** | 轻量即时模式 GUI 框架，适合高频实时数据可视化；ImPlot 原生支持折线图、柱状图、饼图 |
| **CMake + vcpkg** | 跨平台 C++ 构建系统 + 声明式依赖管理 |
| **Clang** | 唯一支持将 C 代码编译为 eBPF 字节码的编译器 |

### 1.3 架构设计

采用 **MVVM 三层架构**，网络流量与系统资源各自独立的数据管道，在 View 层通过 Tab 页统一展示。

- **Model 层**：`SysStatsModel`（/proc 1Hz 轮询）+ `NetworkModel`（eBPF RingBuffer 事件驱动）
- **ViewModel 层**：`SysViewModel` + `NetViewModel`，各自独立线程，`std::shared_mutex` 线程安全
- **View 层**：ImGui 双 Tab 界面，只读访问 ViewModel 的 `get_data()`

**关键设计决策：**

| 决策 | 选择 | 理由 |
|------|------|------|
| Model 线程模型 | 各自独立线程 | 网络事件驱动（高频）与系统轮询（低频 1Hz）节奏不同，合并会互相阻塞 |
| 数据流向 | Model → ViewModel → View，单向 | ViewModel 持有最新聚合状态，View 只读 |
| 线程安全 | `std::shared_mutex` | ViewModel 被 Model 线程写、被 UI 线程读 |
| 模块解耦 | 接口/回调解耦 | Model 通过 callback 通知 ViewModel，View 通过 getter 读取 |

### 1.4 团队分工

| 角色 | 负责模块 | 交付物 |
|------|---------|--------|
| **同学 A** | `SysStatsModel` + `SysViewModel` | `/proc` 文件读取、CPU/内存/磁盘/网卡数据采集、聚合逻辑 |
| **同学 B** | View 层 UI | ImGui 双 Tab 界面、ImPlot 图表 |
| **同学 C** | eBPF 探针 + `NetworkModel` + `NetViewModel` + 工程配置 | BPF C 程序、libbpf 桥接、网络数据聚合、CMake/vcpkg |

**开发流程：** C 先行定义全部接口头文件（数据契约），三人并行开发各自模块，完成后通过 squash merge 合入 `main` 分支。

---

## 第二章 技术难点与克服

### 2.1 eBPF BPF 程序开发（同学 C）

**TC 钩子的选择**

最初考虑使用 XDP（eXpress Data Path），但 XDP 仅能钩住入站流量（ingress），无法捕获出站包。最终选择 TC clsact，同时挂载 ingress 和 egress 两个方向，实现对双向流量的完整监控。

**内核 Verifier 约束**

eBPF 程序需通过内核 verifier 的严格检查：
- **循环展开：** BPF 不支持循环，必须用 `#pragma unroll` 或完全展开的逻辑
- **栈大小：** 仅 512 字节，元数据结构需精打细算
- **指针运算：** 变量偏移的指针运算（如 `data + ip_off + ip_hdr_len`）被 verifier 禁止，需确保上下文可推导

通过增量测试、逐步添加解析逻辑的方式定位到每个 verifier 报错点。

**字节序陷阱**

IP 地址从内核 BPF RingBuffer 以原始字节传递到用户态。在 x86（小端）上，4 字节 `[0x0A, 0xC4, 0xBD, 0x2A]`（即 `10.196.189.42`）作为 `uint32_t` 读出的值是 `0x2ABDC40A`，首字节 `10` 位于最低位。方向判定函数 `is_private_ip` 需用 `ip & 0xFF` 而非 `(ip >> 24)` 取首字节——后者恰好相反，导致所有流量被误判为下行，排查过程涉及逐包打印比对两端字节序。

### 2.2 系统资源采集（同学 A）

**`/proc` 文件解析的兼容性**

Linux 内核的 `/proc/stat` 在不同版本间 CPU 列数存在差异，`/proc/diskstats` 的设备命名规则也不一致。通过白名单过滤 loop/ram/zram 等虚拟设备，排除 `guest`/`guest_nice` 列避免重复计数。

**计数器复位检测**

网卡重启或磁盘重置会导致采样差值为负，产生虚假的瞬时高峰。实现保护逻辑：当差值小于零时跳过当前采样点，等待下一个稳定样本。

**单元测试策略**

所有 `/proc` 解析函数使用 `std::istringstream` 注入固定文本进行测试，不依赖真实 `/proc`，使测试可在任何环境（包括 CI）运行。

### 2.3 View 层 ImGui 集成（同学 C + B）

**高频更新不阻塞 UI**

每秒上千个包的场景下，UI 不能卡顿。NetViewModel 使用 `std::atomic` 计数器在无锁路径累加包数据，`tick()` 每秒仅一次结算；ImGui 表格配合 `ImGuiListClipper` 仅渲染可视行。

**ImPlot API 版本兼容**

vcpkg 提供的 ImPlot 1.0 API 与在线文档有差异：`ImPlotSpec` 变为了属性对构造方式、`PlotBars` 的水平/垂直通过 `ImPlotProp_Flags` 传入、`Annotation` 签名要求额外偏移参数。首次编写的代码编译失败后逐项查头文件适配。

---

## 第三章 阶段性成果展示

### 3.1 代码仓库结构

```
├── bpf/traffic_monitor.bpf.c      # eBPF 内核探针（116 行）
├── include/ctopp/
│   ├── model/                      # 数据契约头文件
│   └── viewmodel/                  # ViewModel 接口
├── src/
│   ├── main.cpp                    # 主界面（455 行）
│   ├── model/
│   │   ├── network_model.cpp       # BPF 用户态桥接
│   │   ├── sys_stats_model.cpp     # 系统资源轮询
│   │   └── sys_stats_reader.cpp    # /proc 解析（250 行）
│   └── viewmodel/
│       ├── net_view_model.cpp      # 网络数据聚合
│       └── sys_view_model.cpp      # 系统数据转换
├── test/                           # 单元测试 + 冒烟测试
├── CMakeLists.txt                  # 构建系统（166 行）
└── Dockerfile                      # Docker 构建
```

### 3.2 构建与运行

```bash
# 构建
cmake --preset vcpkg -B build
cmake --build build

# 运行（需 root 加载 eBPF）
xhost +local:
sudo ./build/ctopp wlp2s0
```

### 3.3 网络流量面板

### 3.4 测试覆盖

| 测试 | 文件 | 内容 |
|------|------|------|
| `pipe_test` | `test/pipeline_smoke.cpp` | BPF → NetworkModel → NetViewModel 端到端冒烟 |
| `sys_stats_test` | `test/sys_stats_test.cpp` | `/proc` 解析、速率计算、ViewModel 转换的单元测试（7 个用例） |
| `sys_stats_smoke` | `test/sys_stats_smoke.cpp` | 真实 /proc 采集链路冒烟 |

```bash
# 运行全部测试
ctest --test-dir build --output-on-failure
```

---

## 第四章 团队协作情况

### 4.1 开发流程

项目采用 **Feature Branch Workflow**：

1. 从 `main` 创建 `feat/<module>` 功能分支
2. 在各自分支上独立开发、commit
3. 完成后 squash merge 回 `main`
4. 功能分支保留不删，方便回溯

接口提交流程：C 先行定义所有公共头文件（数据契约），A/B/C 确认后各自开发。如需修改接口，先在群内达成一致。

### 4.2 协作亮点

- **接口契约先行**：通过预定义的数据结构避免了模块间的耦合问题，三人并行开发时互不阻塞
- **分支隔离**：git 分支策略有效隔离了各自的工作区，编译验证各自独立
- **及时沟通**：字节序 bug 等跨模块问题在群内同步后快速定位

### 4.3 可改进之处

- **集成联调滞后**：sys stats 的 View 层尚未接入 UI，集成工作留到了 Phase 3
- **代码审查**：项目未建立正式的 Code Review 流程

---

## 第五章 智能体的使用

### 5.1 使用方式

本项目中，同学 C 使用 **OpenCode（AI coding agent）** 完成大部分编码工作。工作流程如下：

1. **需求描述：** 通过自然语言向 agent 描述需要实现的功能
2. **代码库分析：** agent 自动读取文件结构、头文件定义、现有实现
3. **代码生成：** agent 生成符合项目风格的 C++ 代码，遵循 MVVM 架构
4. **编译验证：** agent 自动运行 `cmake --build build` 验证编译
5. **迭代修正：** 编译错误或 bug 通过对话驱动 agent 修复

### 5.2 实际效果

- **效率提升显著**：网络流量面板的 ImGui/ImPlot UI 集成在约 3 小时内完成，包括包表、吞吐量折线图、Top IP 表格、协议饼图的完整实现
- **代码质量可靠**：agent 对 C++17、MVVM 模式、`shared_mutex` 线程安全等有良好的理解，生成的代码风格与项目一致
- **学习辅助**：agent 对 eBPF 的 RingBuffer API、libbpf 的 skeleton 加载流程的解释帮助团队成员快速上手

### 5.3 存在问题

- **API 版本感知不足**：agent 基于通用知识生成代码，但 vcpkg 提供的 ImPlot 1.0 与最新版 API 有差异，首次生成的 UI 代码编译失败率约 60%。解决方法是将 ImPlot 头文件内容提供给 agent 作为参考
- **多文件依赖管理**：当修改涉及多个文件（如新增全局变量、修改头文件），agent 有时会遗漏某些调用点的适配
- **环境问题无法处理**：SSH key 认证、缺少系统依赖包（如 OpenGL dev）、`sudo` 权限等非代码问题需要人工介入，这些在整体耗时中占比不小

### 5.4 总体评价

AI coding agent 在**已知模式、明确接口**的编码任务上表现出色（如 MVVM View 层的 ImGui 集成）。但在**需要端到端理解数据流、定位字节序 bug** 等调试场景中，agent 提供的"推测性修复"反而增加了问题排查的复杂度。合理的分工是：agent 负责生成骨架和常规代码，人工负责架构决策和疑难调试。

---

## 第六章 总体心得与个人感悟

### 6.1 项目总体进展

项目完成了 Phase 1（接口定义 + 工程搭建）和 Phase 2（各模块独立实现）的全部编码工作：

- 后端管线（SysStats + Network）：100% 完成，通过单元测试和冒烟测试
- 前端 UI（网络流量面板）：100% 完成，可实时展示网络数据
- 前端 UI（系统资源面板）：待同学 B 集成

项目的 MVVM 架构设计和接口契约先行策略有效支撑了三人的并行开发，代码仓库目前整洁、可编译、可运行。

### 6.2 同学 A 的个人感悟

作为负责 `SysStatsModel` 与 `SysViewModel` 的成员，本阶段的开发让我对 Linux 系统资源监控有了更具体的认识。`/proc/stat`、`/proc/diskstats` 提供的是持续累积的底层计数，并不能直接作为百分比或速率展示，因此需要正确处理采样差值、实际时间间隔、设备过滤和计数器复位。将文件解析、指标计算与线程轮询拆分后，我也体会到了可测试设计的价值：解析逻辑可以通过固定文本进行确定性验证，Model 则专注于采集线程、生命周期和回调通知，使问题更容易定位。

实现轮询线程的过程中，我进一步熟悉了 `std::thread`、`std::mutex`、`std::condition_variable` 和 RAII 的使用，也更加重视对象生命周期与跨线程数据访问。MVVM 架构和接口契约使三名成员能够并行开发，Feature Branch 与 PR 流程则让我熟悉了较规范的协作方式。

本次开发中，我使用 Codex 辅助阅读现有代码库、梳理任务、分析 `/proc` 数据语义、补充单元测试，并在 Windows 与 WSL2 环境之间完成编译和运行验证。相比只针对单个代码片段提问，让 Codex 结合设计文档、Git 历史和跨层接口进行分析，更容易得到符合项目现状的建议。它还帮助我把实现拆分成多次职责清晰的提交，检查公共接口和测试覆盖，提高了开发过程的完整性。

不过，Codex 给出的初始方案有时会偏详细或存在过度设计倾向，对本地依赖、目录规范和团队分工的判断也需要根据实际情况调整。例如，是否修改公共头文件、哪些测试配置可以提交、注释应写到什么程度，都需要我结合组内约定作出最终决定。这次实践让我认识到，智能体能够提高编码、检索和验证效率，却不能代替开发者对数据语义、线程安全和整体架构的理解；只有经过编译、测试和人工审查，生成的方案才能真正成为可靠的工程成果。

### 6.3 同学 B 的个人感悟

> TODO

### 6.4 同学 C 的个人感悟

作为负责网络流量管线与工程配置的成员，这次项目给我最大感触的是 **eBPF 从理论到实战的完整链路**。

**eBPF 的学习曲线**

虽然课堂上讲解过 eBPF 的基本原理，但实际动手写 BPF 程序时遇到的第一个问题是内核 verifier 的各种限制。一个看似简单的"解析 IP 头 + 提取元数据"功能，因为不支持循环、栈空间有限、指针运算受限，需要反复调整写法才能通过 verifier 检查。debug 信息中逐条指令的 verifier 输出（"R1 has pointer with unsupported alu operation"）让我深刻理解了 BPF 程序的底层约束。

**AI agent 辅助开发的体验**

这是我第一次在项目中重度使用 AI coding agent。最有价值的能力是 agent 能快速读取整个代码库，理解现有的架构模式和接口定义，然后生成风格一致的代码。例如，NetViewModel 的接入完全由 agent 完成：它读懂了 `design.md` 里的 MVVM 分层，查看了 `NetViewData` 的数据结构，然后直接在 `main.cpp` 中创建全局实例、连接 callback、驱动 tick()、渲染 UI——全程不需要我手动查 API 文档。

但 agent 并非万能。ImPlot 的 API 版本差异导致的编译错误花了近一个小时才逐一修正。更关键的是，**agent 不理解数据流的端到端含义**——字节序 bug 的排查中，agent 的多次"修复"都是基于局部猜测而非全局理解，最终是我人工逐条追踪了 IP 从 BPF 程序到用户态的完整字节流才找到根源。这让我认识到：agent 适合执行已知模式的编码任务，但系统性的问题诊断仍然需要人的判断。

**MVVM 架构的实践收获**

三组人员并行开发的场景下，接口契约先行被证明是高效的策略。C 先行定义的 8 个头文件（4 个数据契约 + 4 个类接口）成为了三个人之间的"通信协议"，A 和 C 各自完成后端后，View 层的集成只需要调用 `get_data()` 读取快照——没有全局变量耦合，没有跨线程的数据竞争。`shared_mutex` 的读写锁策略在工作线程序号 UI 线程读的场景下工作良好，从未出现数据竞争问题。

**对项目的期望**

目前前端还有一个缺口：sys stats 的 View 层尚未接入。完成这个缺口后，ctop++ 将成为一套完整的、可在真实 Linux 环境下运行的系统可观测性工具。后续可以考虑添加 HTTP 方法统计、进程级网络流量关联、以及基于历史数据的异常检测等进阶功能。
