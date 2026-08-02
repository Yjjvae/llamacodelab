# LlamaCodeLab

一个基于 llama.cpp 的本地 C++ 代码库智能助手。目前完成到实现教程第 12 章（M2）：
工程骨架、配置与日志、测试替身，以及 GGUF 模型的 CPU/CUDA 单轮流式推理。

## Status

M0、M1、M2 已实现。M2 的真实模型集成测试需要本地 GGUF 文件；CUDA 构建需要
WSL CUDA Toolkit。

## Requirements

- Linux 或 WSL2
- CMake 3.28+
- 支持 C++20 的 GCC 或 Clang
- Git（需初始化 `third_party/llama.cpp` 子模块）
- 可选：Ninja、ccache、CUDA Toolkit

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
```

配置示例中的模型路径只是占位符。模型下载和校验方式见 [models/README.md](models/README.md)。

## Verification

```bash
cmake --workflow --preset dev-make
cmake --workflow --preset asan-make
```

设置真实模型后运行集成测试：

```bash
LLCL_TEST_MODEL=/absolute/path/to/model.gguf \
  ctest --test-dir build/release-cpu-make -L model --output-on-failure
```

## Documentation

- [完整实现教程](docs/IMPLEMENTATION_GUIDE.md)
- [开发工作日志](WORKLOG.md)
