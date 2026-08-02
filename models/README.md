# Local models

模型权重不会提交到 Git。把 GGUF 文件下载到本目录，并在配置文件中填写实际路径。

| Role | Model id | File | Quantization | SHA-256 | License |
|---|---|---|---|---|---|
| generation-dev | 待选择 | `generation-q4_k_m.gguf` | Q4_K_M | 待填写 | 待核对 |
| generation-demo | 待选择 | 待填写 | Q4_K_M | 待填写 | 待核对 |
| embedding | 待选择 | 待填写 | 待填写 | 待填写 | 待核对 |

## 下载并校验

只从你有权访问的模型仓库下载，并使用模型发布方给出的 SHA-256：

```bash
MODEL_URL='https://example.invalid/model.gguf' \
MODEL_FILE='generation-q4_k_m.gguf' \
MODEL_SHA256='<EXPECTED_SHA256>' \
bash scripts/download_model.sh
```

下载完成后，应在上表记录模型仓库及 revision、文件名、量化类型、SHA-256 和许可证。
当前工程固定的 llama.cpp revision 记录在 Git 子模块和 `WORKLOG.md` 中。
