# LlamaCodeLab Worklog

本文记录实际完成的代码、验证证据、设计决策和未完成项。它不是计划文档；后续每次开发都应
在文件顶部的“当前状态”以及底部的日期记录中追加结果。

## 当前状态

- 日期：2026-08-02（Asia/Shanghai）
- 教程进度：第 10–12 章，即 M0、M1、M2
- 项目版本：`0.2.0`
- 结论：M0/M1 完成；M2 代码和 CPU 工程验收完成，真实 GGUF 推理及 CUDA 编译等待外部环境
- Git：没有创建 commit，也没有推送远程；GitHub 认证问题按要求暂不处理

| 里程碑 | 状态 | 可验证结果 |
|---|---|---|
| M0 仓库约定 | 完成 | README、许可证、格式/静态检查配置、ignore 规则 |
| M1 工程骨架 | 完成 | CMake target、JSON 配置、日志、CLI、Fake、单元测试 |
| M2 llama.cpp | 代码完成 | 固定子模块、RAII、流式推理、取消、指标、设备枚举 |
| M2 CPU | 完成 | Debug、Release、ASan/UBSan 均编译；测试通过 |
| M2 真实模型 | 待模型 | 未下载 GGUF；model 测试按设计跳过 |
| M2 CUDA | 待 Toolkit | GPU/驱动可见，但 WSL 内没有 `nvcc` 和 CUDA Toolkit |

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
| Ninja | 未安装 |
| ccache | 未安装 |
| clang-format / clang-tidy | 未安装 |
| CUDA Toolkit / nvcc | 未安装 |

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

### CUDA 配置探测

```bash
/usr/lib/wsl/lib/nvidia-smi \
  --query-gpu=name,memory.total,driver_version --format=csv,noheader
cmake --preset release-cuda-make
```

结果：

```text
NVIDIA GeForce RTX 4060 Laptop GPU, 8188 MiB, 591.44
Could not find `nvcc` executable in any searched paths
CUDA Toolkit not found
```

这证明 Windows 驱动和 WSL GPU 转发正常；当前 CUDA 阻塞点是用户态 Toolkit，而不是工程代码
或显卡驱动。不要在 WSL 内安装 Linux NVIDIA kernel driver。

## 第 12 章验收清单

- [x] 同一套源码提供 CPU/CUDA CMake 选项和 preset。
- [x] CPU Debug 和 Release 编译成功。
- [x] 流式 callback、取消入口和性能统计已实现。
- [x] 错误模型路径输出可理解信息。
- [x] 无模型测试不依赖真实权重。
- [x] ASan/UBSan 工作流通过。
- [ ] CUDA build 显示 NVIDIA backend：等待安装 CUDA Toolkit。
- [ ] 使用真实 GGUF 观察逐 token 输出：等待选定并下载模型。
- [ ] 连续生成 20 次并检查显存曲线：依赖前两项。

## 已知限制

1. M2 只实现单轮原始 prompt；聊天模板和多轮消息属于第 13 章 M3。
2. `LlamaGenerator` 用 mutex 将生成串行化；并发队列和 context pool 属于后续里程碑。
3. stop token 在每次 decode 之间检查；一次正在执行的 GPU kernel 不会被强行中断。
4. M2 拒绝 encoder-decoder 模型，当前目标是 decoder-only 代码模型。
5. 没有真实 GGUF，因此尚未获得可信的 TTFT/tokens/s 数据。
6. 没有 clang-format/clang-tidy 可执行文件，因此本次依靠编译器 warning 和人工风格检查。
7. `git submodule add` 按 Git 行为把 `.gitmodules` 和 gitlink 放入暂存区；其他文件未暂存。

## 下一步操作

### 1. 补齐本机工具

在普通 WSL 终端完成 NVIDIA CUDA 软件源配置后，安装与 Windows 驱动兼容的 Toolkit。当前
preset 目标路径是 CUDA 13.1，至少需要 `nvcc`、CUDA runtime headers 和 cuBLAS development
files。安装完成后先确认：

```bash
/usr/local/cuda-13.1/bin/nvcc --version
```

基础工程工具建议安装：

```bash
sudo apt-get install -y ninja-build ccache clang-format clang-tidy
```

之后可改用标准 `dev`、`asan`、`release-cpu` 和 `release-cuda` preset。

### 2. 选择并记录 GGUF

为开发期选择 1.5B–3B coder instruct GGUF，为演示选择能放入 8GB 显存的 7B/8B
`Q4_K_M`。确认许可证和 SHA-256 后更新 `models/README.md`，再运行下载脚本。

### 3. 完成 M2 的硬件验收

```bash
cmake --preset release-cuda-make
cmake --build --preset release-cuda-make
./build/release-cuda-make/apps/cli/llcl-cli devices

./build/release-cuda-make/apps/cli/llcl-cli generate \
  --config configs/cuda-8gb.example.json \
  --prompt "Explain RAII in C++ with a short example."

LLCL_TEST_MODEL=/absolute/path/to/model.gguf \
  ctest --test-dir build/release-cuda-make -L model --output-on-failure
```

同时在另一个普通 WSL 终端观察：

```bash
watch -n 0.5 /usr/lib/wsl/lib/nvidia-smi
```

记录模型、上下文、量化、GPU layers、TTFT、prompt t/s、decode t/s 和峰值显存。

## 日期记录

### 2026-08-02 — M0/M1/M2 初始实现

- 从仅有教程文档的空仓库建立完整 C++ 工程。
- 固定 llama.cpp 和四个小型依赖。
- 因 GitHub Git pack 多次 `early EOF`，把小型 FetchContent 依赖改为 SHA-256 归档；保持
  llama.cpp 为子模块。
- 修正 spdlog 1.15 的 sink 头文件变化。
- 将项目库明确设为 static，避免第三方 static library 被错误链接进隐式 shared target。
- 通过 Debug、Release CPU 和 ASan/UBSan 验证。
- 确认 CUDA 未通过的唯一当前环境阻塞是 Toolkit/nvcc 缺失。
