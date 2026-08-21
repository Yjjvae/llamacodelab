# LlamaCodeLab

一个基于 llama.cpp 的本地 C++ 代码库智能助手。目前完成到 M7：
工程骨架、真实 GGUF 的 CPU/CUDA 流式推理、多轮消息、安全的仓库扫描、稳定代码切块和可持久化的本地向量检索。

## Status

M0–M7 已完成。`ask` 会临时扫描仓库、检索相关 Chunk，以真实 tokenizer 预算构造防注入 RAG
提示词，并输出带源文件和行号的引用；`index` 则构建可增量更新的持久化索引。

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

./build/release-cuda/apps/cli/llcl-cli index \
  --config configs/default.json --repo .

./build/release-cuda/apps/cli/llcl-cli ask \
  --config configs/default.json --repo . \
  "Where is the vector index implemented?"
```

`chat` 会从 GGUF 读取模板，预留生成 token 后按 token 预算丢弃最早的非 system 历史消息。
简单输入可用 `--system` 和重复的 `--message`；带 assistant 历史或需要严格保序时使用重复的
`--turn role:content`（两种写法不能混用）。
若模型不含模板，可在对应 `generation_model.chat_template` 中配置 llama.cpp 支持的模板名称
（例如 `chatml`）。

`scan` 不加载模型；它按 `index.chunk_lines`、`index.overlap_lines` 和
`index.max_file_bytes` 扫描仓库并报告文件/Chunk 统计。可重复使用 `--include` 与 `--exclude`
添加或排除相对路径 glob。

`index` 使用 embedding 模型把索引写入 `index.data_dir`（默认 `var/index`）。元数据保存在
SQLite，向量保存在带版本号的二进制快照中；重复执行只会重新 embedding 新增或内容发生改变的文件。
模型标识或向量维度不一致时命令会拒绝加载旧索引，避免静默混用向量空间。

`ask` 当前是内存式 RAG：每次调用都会重新扫描、切块和 embedding，因此结果不依赖旧索引；回答
必须使用 `[S1]` 等引用，命令末尾会列出对应的文件和行号。

配置示例使用 Qwen2.5-Coder-1.5B-Instruct Q4_K_M。模型下载和校验方式见
[models/README.md](models/README.md)。

## Verification

```bash
./scripts/quality.sh --preset dev
cmake --workflow --preset asan-make
```

The quality script verifies format, build, unit tests, and `clang-tidy`. GitHub Actions repeats
the non-GPU checks for pull requests and `main`; GPU/model validation remains an explicit local
or trusted self-hosted-runner step.

设置真实模型后运行集成测试：

```bash
LLCL_TEST_MODEL=/absolute/path/to/model.gguf \
  ctest --test-dir build/release-cpu-make -L model --output-on-failure

LLCL_TEST_MODEL="$PWD/models/qwen2.5-coder-1.5b-instruct-q4_k_m.gguf" \
LLCL_TEST_GPU_LAYERS=-1 LLCL_TEST_REPEAT=20 \
  ctest --test-dir build/release-cuda -L model --output-on-failure

LLCL_TEST_EMBEDDING_MODEL="$PWD/models/nomic-embed-text-v1.5-q4_k_m.gguf" \
  ctest --test-dir build/dev -R EmbeddingSmokeTest --output-on-failure
```

可选的检索微基准：

```bash
cmake -S . -B build/bench -G Ninja -DLLCL_BUILD_BENCHMARKS=ON
cmake --build build/bench --target llcl_retrieval_benchmark
./build/bench/benchmarks/llcl_retrieval_benchmark
```

## Documentation

- [完整实现教程](docs/IMPLEMENTATION_GUIDE.md)
- [开发工作日志](WORKLOG.md)
