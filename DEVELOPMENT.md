# 开发流程

## 分支策略

- `main` — 稳定分支，只接受 **squash merge**。每次合入对应一个功能模块的完整交付。
- `feat/<module>` — 功能分支，从 `main` 创建，完成后 squash merge 回 `main`。
  分支上的详细 commit 历史**保留不删**，方便溯源。

**当前分支：**

| 分支 | 负责人 | 内容 |
|------|--------|------|
| `feat/sys-stats` | 同学 A | `SysStatsModel` + `SysViewModel` 实现 |
| `feat/view` | 同学 B | ImGui + ImPlot 双 Tab 界面（系统 Stats / 网络流量） |
| `feat/network-pipeline` | 同学 C | eBPF 探针 + `NetworkModel` + `NetViewModel` + 集成 |

## Commit 规范

统一使用 `<type>: <简短描述>` 格式，英文提交信息。

| 前缀 | 用途 |
|------|------|
| `feat` | 新功能 |
| `fix` | 修复 bug |
| `docs` | 文档变更（README、设计文档等） |
| `refactor` | 重构，不改变功能 |

**原则：** 一个 commit 只做一件事。不要在一个 commit 里混入无关改动。

## 文件归属

以下为各成员负责的目录和文件。修改不属于自己的文件前，先在群内沟通确认。

### 同学 A — 系统资源监控管线

| 文件 | 说明 |
|------|------|
| `include/ctopp/model/sys_stats_snapshot.hpp` | 数据契约，不可随意修改字段（C 定义） |
| `include/ctopp/model/sys_stats_model.hpp` | 类接口声明（C 定义） |
| `include/ctopp/viewmodel/sys_view_data.hpp` | 数据契约（C 定义） |
| `include/ctopp/viewmodel/sys_view_model.hpp` | 类接口声明（C 定义） |
| `src/model/sys_stats_reader.hpp/.cpp` | **A 实现**：`/proc` 解析和采样差分计算 |
| `src/model/sys_stats_model.cpp` | **A 实现**：1 Hz 轮询、数据采集和回调通知 |
| `src/viewmodel/sys_view_model.cpp` | **A 实现**：显示单位转换和历史队列 |
| `test/sys_stats_test.cpp` | **A 实现**：解析、差分和 ViewModel 单元测试 |
| `test/sys_stats_smoke.cpp` | **A 实现**：Linux 真实采集链路测试 |

### 同学 B — View 层

| 文件 | 说明 |
|------|------|
| `src/main.cpp` | **B 实现**：全部 UI 内容。当前为骨架，直接在此基础上开发 |

### 同学 C — 网络流量管线 + 工程配置

| 文件 | 说明 |
|------|------|
| `include/ctopp/model/packet_record.hpp` | 包元数据结构 |
| `include/ctopp/model/network_model.hpp` | NetworkModel 接口 |
| `include/ctopp/viewmodel/net_view_data.hpp` | NetViewData 数据契约 |
| `include/ctopp/viewmodel/net_view_model.hpp` | NetViewModel 接口 |
| `src/model/network_model.cpp` | libbpf 用户态桥接 |
| `src/viewmodel/net_view_model.cpp` | 网络数据聚合 |
| `bpf/traffic_monitor.bpf.c` | eBPF 内核探针 |
| `CMakeLists.txt` / `vcpkg.json` / `CMakePresets.json` | 工程配置 |

## 接口契约

`include/ctopp/` 下的头文件是三人之间的**公共接口契约**。

- 任何人不能随意修改不属于自己负责的接口头文件。
- 如果确实需要改接口（比如 View 层需要新的数据字段），先在群内提出、三人达成一致后再改。
- 改接口后，负责变更的人也负责通知另外两人同步拉取最新代码。

## 开发流程

1. 从 `main` 创建自己的 `feat/<module>` 分支
2. 在本分支上自由开发、commit
3. 完成后自测：**代码必须能编译通过**（`cmake --build build`）
4. 在群内知会，准备合并
5. 由 C 统一执行 squash merge 到 `main`

## 合并到 main（由 C 执行）

```bash
# 1. 确保 main 是最新的
git checkout main
git pull origin main

# 2. squash merge 功能分支
git merge --squash feat/<module>
git commit -m "feat: <模块描述>"

# 3. 推送
git push origin main
```

功能分支**保留不删**，详细 commit 历史可以随时回溯。

## 常见问题

### 编译环境

先安装 vcpkg 和依赖，详见 [README.md](README.md)。

### 编译 BPF 程序需要 clang

```bash
# 仅编译 C++ 部分（跳过 BPF）
cmake --preset vcpkg -B build
cmake --build build --target ctopp

# 编译 BPF 程序
cmake --build build --target bpf
```

### 修改 `include/` 下的头文件后记得重新编译

头文件变更可能影响多个 .cpp 文件。建议每次都 `cmake --build build` 全量重编，避免遗漏。
