# Local models

模型权重不会提交到 Git。把 GGUF 文件下载到本目录，并在配置文件中填写实际路径。

| Role | Model id | File | Quantization | SHA-256 | License |
|---|---|---|---|---|---|
| generation-dev | `Qwen/Qwen2.5-Coder-1.5B-Instruct-GGUF` @ `f86cb2c1fa58255f8052cc32aeede1b7482d4361` | `qwen2.5-coder-1.5b-instruct-q4_k_m.gguf` | Q4_K_M | `cc324af070c2ecbfd324a30884d2f951a7ff756aba85cb811a6ec436933bb046` | Apache-2.0 |
| generation-demo | 待选择 | 待填写 | Q4_K_M | 待填写 | 待核对 |
| embedding | `nomic-ai/nomic-embed-text-v1.5-GGUF` @ `4ef6244e6d94b30c009d2388c001faf181c3e237` | `nomic-embed-text-v1.5-q4_k_m.gguf` | Q4_K_M | `d4e388894e09cf3816e8b0896d81d265b55e7a9fff9ab03fe8bf4ef5e11295ac` | Apache-2.0 |

## 下载并校验

只从你有权访问的模型仓库下载，并使用模型发布方给出的 SHA-256：

```bash
MODEL_URL='https://huggingface.co/Qwen/Qwen2.5-Coder-1.5B-Instruct-GGUF/resolve/f86cb2c1fa58255f8052cc32aeede1b7482d4361/qwen2.5-coder-1.5b-instruct-q4_k_m.gguf' \
MODEL_FILE='qwen2.5-coder-1.5b-instruct-q4_k_m.gguf' \
MODEL_SHA256='cc324af070c2ecbfd324a30884d2f951a7ff756aba85cb811a6ec436933bb046' \
bash scripts/download_model.sh
```

下载完成后，应在上表记录模型仓库及 revision、文件名、量化类型、SHA-256 和许可证。
当前工程固定的 llama.cpp revision 记录在 Git 子模块和 `WORKLOG.md` 中。

M5 的 Nomic 模型通过 `search_query: ` 和 `search_document: ` 前缀区分问题与代码 Chunk；适配器会
在调用前添加它们。下载命令：

```bash
MODEL_URL='https://huggingface.co/nomic-ai/nomic-embed-text-v1.5-GGUF/resolve/4ef6244e6d94b30c009d2388c001faf181c3e237/nomic-embed-text-v1.5.Q4_K_M.gguf' \
MODEL_FILE='nomic-embed-text-v1.5-q4_k_m.gguf' \
MODEL_SHA256='d4e388894e09cf3816e8b0896d81d265b55e7a9fff9ab03fe8bf4ef5e11295ac' \
bash scripts/download_model.sh
```
