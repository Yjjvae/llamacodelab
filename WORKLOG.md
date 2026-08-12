# LlamaCodeLab Worklog

本文记录实际完成的代码、验证证据、设计决策和未完成项。它不是计划文档；后续每次开发都应
在文件顶部的“当前状态”以及底部的日期记录中追加结果。

## 当前状态

- 日期：2026-08-03（Asia/Shanghai）
- 教程进度：第 10–12 章，即 M0、M1、M2
- 项目版本：`0.2.0`
- 结论：M0/M1/M2 完成；CPU、CUDA、真实 GGUF 和连续 20 次生成均已验收
- Git：基线 commit `489b2c9` 已推送 `origin/main`；本次硬件验收改动尚未提交

| 里程碑 | 状态 | 可验证结果 |
|---|---|---|
| M0 仓库约定 | 完成 | README、许可证、格式/静态检查配置、ignore 规则 |
| M1 工程骨架 | 完成 | CMake target、JSON 配置、日志、CLI、Fake、单元测试 |
| M2 llama.cpp | 完成 | 固定子模块、RAII、流式推理、取消、指标、设备枚举 |
| M2 CPU | 完成 | Debug、Release、ASan/UBSan 均编译；测试通过 |
| M2 真实模型 | 完成 | 官方 Qwen2.5-Coder 1.5B Q4_K_M；SHA-256 校验和 CPU/GPU 推理通过 |
| M2 CUDA | 完成 | CUDA 13.3、RTX 4060、29/29 层 offload、20 次连续生成通过 |

## 本次实现范围

### M0：仓库和开发约定

完成内容：

- 增加 `.gitignore`，排除构建目录、GGUF、索引、日志、IDE 文件和密钥文件。
- 增加 `.editorconfig`、`.clang-format` 和分阶段启用的 `.clang-tidy`。
- 增加 MIT `LICENSE`。
- 编写面向新开发者的 `README.md`。
- 增加 Pull Request 模板。
- 验证 `models/*.gguf`、`build/` 和 `.env` 均会被 Git 忽略。

### M1：工程骨架

完成内容：

- 根 CMake 只定义项目选项和子目录，各模块拥有独立 target。
- 项目代码开启严格 warning，第三方 target 不继承项目 warning/sanitizer。
- 同时提供标准 Ninja preset 和当前机器可用的 `*-make` 回退 preset。
- FetchContent 使用固定 commit 归档和 SHA-256，而不是跟随 branch/tag。
- 跨 preset 共享 `build/_downloads` 下载缓存，各配置仍拥有独立源码和二进制目录。
- 实现 JSON 配置加载、默认值、范围检查和带文件/字节位置的解析错误。
- 实现 spdlog 日志入口。
- 实现 CLI11 命令骨架。
- 实现 `ITextGenerator`、`ITokenCounter` 和无需模型的 `FakeGenerator`。
- 添加配置和 Fake 单元测试。

### M2：llama.cpp 单轮推理

完成内容：

- `third_party/llama.cpp` 以 Git 子模块接入并固定 commit。
- `LlamaRuntime` 管理进程级 backend 初始化/释放和设备枚举。
- `LlamaGenerator` 通过 PImpl 隔离 llama.cpp 类型，领域接口不依赖第三方头文件。
- 模型在 `LlamaGenerator` 构造时加载并常驻内存/显存，不在请求间反复加载。
- model、context、sampler 全部由自定义 deleter 和 `std::unique_ptr` 管理。
- 每次请求创建独立 context 和 sampler，避免残留上一次请求的 KV 状态。
- 长 prompt 按 `batch_size` 分批 prefill，而不是假设 prompt 永远小于 batch。
- 实现 greedy 或 `top-p -> temperature -> distribution` sampler chain。
- 实现 sample、decode、EOG、最大 token、上下文上限和 stop token 检查。
- 对跨 token 的 UTF-8 字节进行缓存，只向 callback 发送完整 UTF-8 序列。
- `generate` 用 mutex 串行化；M2 明确只支持单并发生成。
- 统计 prompt tokens、generated tokens、TTFT、prompt tokens/s、decode tokens/s。
- CLI 支持 `devices` 和 `generate`，Ctrl+C 通过 stop source 请求取消。
- 缺少模型、无效配置、超出上下文等错误会在边界转换为可读消息。
- 真实 GGUF 用例带 `model` label；没有 `LLCL_TEST_MODEL` 时跳过而不是失败。
- 增加带 SHA-256 校验、临时文件和原子改名的模型下载脚本。

## 固定依赖

| 依赖 | 版本/commit | 归档 SHA-256 |
|---|---|---|
| llama.cpp | `3581ba0cf591b3f772fbb002de0f70e294bc0396` | Git 子模块 |
| nlohmann/json | `v3.12.0` / `65ee68451d8eb2b5f3a30b410476ab83deb3289b` | `13ef31d691947940a08909f8e0772f1d7d68e5da1678ee812a49c4bb0c996b2f` |
| spdlog | `v1.15.3` / `6fa36017cfd5731d617e1a934f0e5ea9c4445b13` | `5097fb362e79a2bd7247beaf1f8377ed60e274fbe83a4b33e7b73383f0279022` |
| CLI11 | `v2.5.0` / `4160d259d961cd393fd8d67590a8c7d210207348` | `c91e8768600e61be11f7250e3cf3e71afd9d0f18f9c9e9e209a8e084ca08cd85` |
| GoogleTest | `v1.17.0` / `52eb8108c5bdec04579160ae17225d66034bd723` | `745c55415660044610f7fcd3af7a6420d5de16a7dbb9ebfe2e131275676232be` |

## 实际环境

| 项目 | 实际值 |
|---|---|
| OS | WSL2 / Ubuntu 26.04 |
| CPU | AMD Ryzen 7 8845H，16 logical CPUs |
| RAM | 约 11 GiB 可用 |
| GPU | NVIDIA GeForce RTX 4060 Laptop GPU |
| VRAM | 8188 MiB |
| Windows NVIDIA Driver | 591.44 |
| CMake | 4.2.3 |
| C++ compiler | GCC 15.2.0 |
| llama.cpp / ggml | commit `3581ba0` / ggml `0.18.0` |
| Ninja | 1.13.2 |
| ccache | 4.12.3 |
| clang-format / clang-tidy | 21.1.8 |
| CUDA Toolkit / nvcc | 13.3 / 13.3.73 |

说明：普通 WSL 环境能通过 `/usr/lib/wsl/lib/nvidia-smi` 看到 GPU。CPU build 的
`llcl-cli devices` 只列出 CPU 是预期行为，因为该 build 没有编译 `GGML_CUDA`。

## 验证记录

### Debug CPU

```bash
cmake --preset dev-make
cmake --build --preset dev-make
ctest --test-dir build/dev-make --output-on-failure
```

结果：

```text
100% tests passed, 0 tests failed out of 8
7 unit tests passed
1 model integration test skipped (LLCL_TEST_MODEL 未设置)
```

CLI 验证：

```bash
./build/dev-make/apps/cli/llcl-cli --help
./build/dev-make/apps/cli/llcl-cli devices
./build/dev-make/apps/cli/llcl-cli generate \
  --config configs/cpu.example.json --prompt test
```

结果：help 正常；设备枚举显示 Ryzen CPU；缺少 GGUF 时返回：

```text
error: GGUF model does not exist or is not a file: models/generation-q4_k_m.gguf
```

### ASan/UBSan

```bash
cmake --workflow --preset asan-make
```

结果：配置、构建、测试三步全部成功；7 个实际执行的测试没有 sanitizer 报告。

### Release CPU

```bash
cmake --preset release-cpu-make
cmake --build --preset release-cpu-make
ctest --test-dir build/release-cpu-make --output-on-failure
./build/release-cpu-make/apps/cli/llcl-cli devices
```

结果：Release 编译成功，8/8 测试通过（其中 model 测试按条件跳过），设备输出：

```text
gpu_offload_supported=false
name=CPU type=CPU description="AMD Ryzen 7 8845H w/ Radeon 780M Graphics"
```

### CUDA 与真实模型

```bash
/usr/lib/wsl/lib/nvidia-smi \
  --query-gpu=name,memory.total,driver_version --format=csv,noheader
cmake --preset release-cuda-make
```

初始探测结果：

```text
NVIDIA GeForce RTX 4060 Laptop GPU, 8188 MiB, 591.44
Could not find `nvcc` executable in any searched paths
CUDA Toolkit not found
```

这证明 Windows 驱动和 WSL GPU 转发正常。最初安装 CUDA 13.1 后，Ubuntu 26.04 的 glibc
`rsqrt/rsqrtf noexcept` 声明与 Toolkit 头文件冲突。升级到官方支持 Ubuntu 26.04 的 CUDA
13.3 后配置成功。Windows 驱动 591.44 满足 CUDA 13.x minor-version compatibility 的最低
580 要求；WSL 内未安装 Linux NVIDIA driver。

CUDA Release 构建和项目设备枚举通过：

```text
gpu_offload_supported=true
name=CUDA0 type=GPU description="NVIDIA GeForce RTX 4060 Laptop GPU" memory_mib=8188
name=CPU type=CPU description="AMD Ryzen 7 8845H w/ Radeon 780M Graphics"
```

模型：`Qwen/Qwen2.5-Coder-1.5B-Instruct-GGUF`，revision `f86cb2c1`，Q4_K_M，
1,117,320,768 bytes，Apache-2.0。下载后 SHA-256 与发布方 LFS oid 一致：

```text
cc324af070c2ecbfd324a30884d2f951a7ff756aba85cb811a6ec436933bb046
```

CPU 与 GPU 使用相同的 16-token 原始 prompt；GPU 运行 256 tokens，CPU 基线运行 64 tokens，
因此主要比较稳态吞吐：

| 指标 | Release CPU | Release CUDA |
|---|---:|---:|
| 模型加载 | 998 ms | 803 ms |
| TTFT | 230 ms | 85 ms |
| Prompt throughput | 118.16 tok/s | 279.59 tok/s |
| Decode throughput | 36.67 tok/s | 123.29 tok/s |

CUDA 日志确认 `offloaded 29/29 layers to GPU`，模型 buffer 934.70 MiB、KV buffer
112.00 MiB、compute buffer 299.75 MiB。ChatML 语义检查正确解释 RAII，decode 119.12 tok/s。

连续运行命令：

```bash
LLCL_TEST_MODEL="$PWD/models/qwen2.5-coder-1.5b-instruct-q4_k_m.gguf" \
LLCL_TEST_GPU_LAYERS=-1 LLCL_TEST_REPEAT=20 \
  ctest --test-dir build/release-cuda -L model --output-on-failure
```

结果：20 次同进程生成通过，用时 3.02 秒。`nvidia-smi` 的整卡已用显存在测试前约
1599 MiB、运行期间 1596–1609 MiB、结束后 1607 MiB；未观察到随次数递增的显存增长。

## 第 12 章验收清单

- [x] 同一套源码提供 CPU/CUDA CMake 选项和 preset。
- [x] CPU Debug 和 Release 编译成功。
- [x] 流式 callback、取消入口和性能统计已实现。
- [x] 错误模型路径输出可理解信息。
- [x] 无模型测试不依赖真实权重。
- [x] ASan/UBSan 工作流通过。
- [x] CUDA build 显示 NVIDIA backend，并识别 compute capability 8.9。
- [x] 使用真实 GGUF 观察逐 token 输出，并记录 CPU/GPU 指标。
- [x] 连续生成 20 次并检查显存曲线。

## 已知限制

1. M2 只实现单轮原始 prompt；聊天模板和多轮消息属于第 13 章 M3。
2. `LlamaGenerator` 用 mutex 将生成串行化；并发队列和 context pool 属于后续里程碑。
3. stop token 在每次 decode 之间检查；一次正在执行的 GPU kernel 不会被强行中断。
4. M2 拒绝 encoder-decoder 模型，当前目标是 decoder-only 代码模型。
5. M2 CLI 不自动套用 GGUF chat template；语义验收使用显式 ChatML，自动模板属于 M3。
6. `nvidia-smi` 在 WSL 中报告整卡占用，包含 Windows 桌面和其他进程，不能当作项目独占显存。

## 下一步操作

### 1. 进入 M3

实现 GGUF chat template、多轮消息结构和会话状态，让使用者不需要手写 ChatML。随后再进入
检索、embedding 和索引模块。保留当前 Qwen 1.5B 作为快速回归模型；更大模型只作为质量基线。

## 日期记录

### 2026-08-03 — M2 GPU 与真实推理验收

- 安装 CUDA 13.3 最小开发组件和 Ninja/ccache/Clang 工具，不安装 WSL Linux 驱动。
- 记录并修复 CUDA 13.1 与 Ubuntu 26.04 glibc 的头文件兼容问题。
- 选择、下载并校验官方 Qwen2.5-Coder-1.5B-Instruct Q4_K_M。
- CUDA Release 构建成功，设备枚举、29/29 层 offload 和真实生成通过。
- CPU/GPU 性能对照、ChatML 语义检查和同进程连续 20 次生成通过。
- 测试增加 `LLCL_TEST_GPU_LAYERS` 与 `LLCL_TEST_REPEAT`，便于 CI/本机复用。

### 2026-08-02 — M0/M1/M2 初始实现

- 从仅有教程文档的空仓库建立完整 C++ 工程。
- 固定 llama.cpp 和四个小型依赖。
- 因 GitHub Git pack 多次 `early EOF`，把小型 FetchContent 依赖改为 SHA-256 归档；保持
  llama.cpp 为子模块。
- 修正 spdlog 1.15 的 sink 头文件变化。
- 将项目库明确设为 static，避免第三方 static library 被错误链接进隐式 shared target。
- 通过 Debug、Release CPU 和 ASan/UBSan 验证。
- 确认 CUDA 未通过的唯一当前环境阻塞是 Toolkit/nvcc 缺失。
