# 0004: 用 SQLite FTS5 和 RRF 实现混合检索

## 决定

M9 的关键词后端使用 SQLite FTS5，索引 `path`、`symbol` 和 `content` 三个字段。`chunks` 的新增、删除和更新
由 SQLite 触发器同步到 FTS 表，打开已有索引时会补齐 FTS 记录。

应用层的 `HybridRetriever` 分别取得向量候选与关键词候选，再用 Reciprocal Rank Fusion (RRF) 合并。初始值为
向量候选 30、关键词候选 30、RRF `k=60`。RRF 只依赖各后端的名次，因此不会把 BM25 与余弦相似度这两个
不同量纲的分数直接相加。

## 后果

服务端的搜索和问答路径使用混合检索；CLI 的一次性内存问答没有 SQLite 持久化索引，保留向量检索 fallback。
精确类名、函数名、宏和文件名查询会从 FTS5 获益，Reranker 则留给下一阶段处理最终候选的相关性排序。
