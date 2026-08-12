# LlamaCodeLab

一个基于 llama.cpp 的本地 C++ 代码库智能助手。目前完成到实现教程第 14 章（M4）：
工程骨架、真实 GGUF 的 CPU/CUDA 流式推理、多轮消息，以及安全的仓库扫描和稳定代码切块。

## Status

M0、M1、M2、M3、M4 已完成。M4 默认只扫描 C/C++/CMake 文件，遵循 `.gitignore`，
排除构建和第三方目录，并产生稳定的路径、行号、内容 hash 与 Chunk ID。

## Requirements

- Linux 或 WSL2
- CMake 3.28+
- 支持 C++20 的 GCC 或 Clang
- Git（需初始化 `third_party/llama.cpp` 子模块）
- Ninja、ccache
- GPU 构建：CUDA Toolkit 13.3（Ubuntu 26.04）

## Bootstrap

```bash
git submodule update --init --recursive
cmake --workflow --preset dev-make
```

安装 Ninja 和 ccache 后，也可以使用教程中的标准 preset：

```bash
cmake --workflow --preset dev
cmake --workflow --preset asan
```

## CLI

```bash
./build/dev-make/apps/cli/llcl-cli --help
./build/dev-make/apps/cli/llcl-cli devices

./build/release-cpu-make/apps/cli/llcl-cli generate \
  --config configs/cpu.example.json \
  --prompt "Explain RAII in C++ with a short example."

./build/release-cuda/apps/cli/llcl-cli chat \
  --config configs/cuda-8gb.example.json \
  --turn "system:You are a concise C++ tutor." \
  --turn "user:What does RAII mean?" \
  --turn "assistant:RAII binds resource lifetime to object lifetime." \
  --turn "user:Give a minimal example." \
  --temperature 0.2 --seed 42

./build/dev/apps/cli/llcl-cli scan \
  --repo . \
  --dry-run
```

`chat` 会从 GGUF 读取模板，预留生成 token 后按 token 预算丢弃最早的非 system 历史消息。
简单输入可用 `--system` 和重复的 `--message`；带 assistant 历史或需要严格保序时使用重复的
`--turn role:content`（两种写法不能混用）。
若模型不含模板，可在对应 `generation_model.chat_template` 中配置 llama.cpp 支持的模板名称
（例如 `chatml`）。

`scan` 不加载模型；它按 `index.chunk_lines`、`index.overlap_lines` 和
`index.max_file_bytes` 扫描仓库并报告文件/Chunk 统计。可重复使用 `--include` 与 `--exclude`
添加或排除相对路径 glob。

配置示例使用 Qwen2.5-Coder-1.5B-Instruct Q4_K_M。模型下载和校验方式见
[models/README.md](models/README.md)。

## Verification

```bash
cmake --workflow --preset dev-make
cmake --workflow --preset asan-make
```

设置真实模型后运行集成测试：

```bash
LLCL_TEST_MODEL=/absolute/path/to/model.gguf \
  ctest --test-dir build/release-cpu-make -L model --output-on-failure

LLCL_TEST_MODEL="$PWD/models/qwen2.5-coder-1.5b-instruct-q4_k_m.gguf" \
LLCL_TEST_GPU_LAYERS=-1 LLCL_TEST_REPEAT=20 \
  ctest --test-dir build/release-cuda -L model --output-on-failure
```

## Documentation

- [完整实现教程](docs/IMPLEMENTATION_GUIDE.md)
- [开发工作日志](WORKLOG.md)
