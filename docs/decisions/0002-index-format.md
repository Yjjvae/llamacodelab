# 0002: 使用 SQLite 元数据与版本化二进制向量文件

## 决定

索引元数据和 Chunk 内容存入 SQLite（WAL 模式），float32 embedding 使用独立的 `LLCLVEC1`
二进制文件。SQLite 的 `active_vector_generation` 是唯一生效指针；向量文件使用
`vectors.<generation>.bin` 并先写 `.tmp`、fsync、原子 rename。

## 原因

SQLite 擅长事务、删除与内容哈希查找；向量不应以 JSON 形式膨胀存储。不可变 generation 可让
读请求持有旧快照，更新完成后再切换到新快照。

## 约束

加载时必须验证 magic、版本、维数、记录数、文件大小和 embedding model hash。模型 hash 或维数变化
会拒绝加载旧索引，要求显式重建，避免混用不兼容 embedding。
