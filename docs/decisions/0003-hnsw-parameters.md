# 0003: 使用 HNSW 作为可选向量检索后端

## 决定

M9 保留 `IVectorIndex` 接口和 `BruteForceIndex` fallback，并在 `src/adapters/vector/` 中增加基于
hnswlib v0.8.0 的 HNSW 适配器。初始构图参数为 `M=16`、`ef_construction=200`；对当前 10,000 条、
64 维的确定性基准，选择 `ef_search=256` 作为达到召回门槛的评测配置。

## 证据

在本机 Release benchmark（10,000 条、64 维、200 个查询）中：

| `ef_search` | Recall@10 | HNSW p95 |
| --- | ---: | ---: |
| 64 | 0.8275 | 52 µs |
| 128 | 0.9535 | 79 µs |
| 256 | 0.9900 | 198 µs |

暴力检索的 p95 为 1,631 µs。`ef_search=256` 在达到更高召回的同时仍显著快于暴力检索，因此作为评测
配置；实际仓库规模与 embedding 模型变化后必须重新测量。

## 约束

HNSW 文件旁必须保存并校验格式版本、向量维数、最大元素数和 embedding 模型 hash。加载不匹配时失败，
不会静默回退到不兼容的向量空间。暴力检索继续保留，供关闭 HNSW、测试 Recall 和故障诊断使用。
