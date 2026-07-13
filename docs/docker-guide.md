# ctop++ Docker 构建与运行

## 构建

项目根目录下已提供 `Dockerfile`，一条命令构建：

```bash
docker build -t ctopp .
```

构建过程：
1. 安装系统依赖（clang、libbpf、OpenGL、X11 等）
2. 安装 [vcpkg](https://github.com/microsoft/vcpkg) 并编译 C++ 依赖
3. 编译 ctopp（含 eBPF 程序）

首次构建约需 **10-15 分钟**（主要耗时在 vcpkg 编译）。后续修改 C++ 代码后再次构建利用 Docker 缓存只需 **30 秒-2 分钟**。

## 运行

加载 eBPF 程序需要特权，同时需要将 X11 显示共享给容器：

```bash
xhost +local:
docker run --rm -it --privileged \
    -e DISPLAY=$DISPLAY \
    -v /tmp/.X11-unix:/tmp/.X11-unix \
    -v /sys/kernel/btf:/sys/kernel/btf:ro \
    ctopp ./ctopp wlp2s0
```

参数说明：

| 参数 | 作用 |
|------|------|
| `--privileged` | 赋予 `CAP_BPF`、`CAP_NET_ADMIN` 等内核能力 |
| `-e DISPLAY=$DISPLAY` | 传递 X11 显示环境变量 |
| `-v /tmp/.X11-unix` | 共享 X11 socket，使 GUI 能显示到宿主机 |
| `-v /sys/kernel/btf:ro` | 挂载 BTF 信息，供 eBPF CO-RE 使用 |
| `wlp2s0` | 被监控网卡名，按实际环境替换（如 `eth0`、`lo`） |

## 仅编译，不运行

若只想编译验证，无需显示：

```bash
docker build -t ctopp .
```

编译产物在容器内的 `/ctopp/build/ctopp`，可通过以下方式提取：

```bash
# 方法一：从临时容器复制
docker create --name ctopp-tmp ctopp
docker cp ctopp-tmp:/ctopp/build/ctopp .
docker rm ctopp-tmp

# 方法二：挂载 volume
docker build -t ctopp .
docker run --rm -v $(pwd):/out ctopp cp /ctopp/build/ctopp /out/
```
