# 0005: 使用可选的本地生成式重排并保留检索回退

## 决定

M9 的服务端索引可由 `index.hnsw_enabled` 在暴力检索与 HNSW 间选择，默认保持暴力检索，便于小型
仓库调试和结果对照。HNSW 的 `ef_search` 默认值为 256。

关键词检索和向量检索先通过 RRF 合并。若 `index.reranker_enabled=true`，`AskService` 会取得前
`rerank_candidates`（限定为 20–50，默认 30）个混合候选，再由 `LlamaReranker` 使用已有的本地
生成模型逐条输出 0–3 的确定性相关性评级，最后保留请求的 `top_k` 条。候选内容被显式标为不可信
文档，不能改变评分指令。

## 后果

该方案不引入第二个 cross-encoder GGUF，因而适合 8 GiB 显存机器，也避免额外下载和常驻模型的显存
成本；代价是每个候选都要一次生成，延迟明显高于只做 RRF。它默认关闭，关闭时 AskService 直接保留
混合检索顺序；关闭 HNSW 时则使用 BruteForceIndex。两种回退均有自动化测试。

## 评测证据

`LlamaRerankerTest.SortsCandidatesByDeterministicModelRating` 使用两条固定候选：向量排序把无关
候选排第一，而评分器把包含直接答案的候选评为 3、无关候选评为 0。重排后的 Recall@1 从 0/1 变为
1/1。此项是确定性的管道契约评测，不宣称真实 GGUF 在任意代码库上的质量提升；启用真实模型前应以
目标仓库的标注查询集重新测量质量和端到端延迟。
