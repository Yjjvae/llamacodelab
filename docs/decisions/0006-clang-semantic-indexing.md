# 0006: 以可选 Clang AST 索引补充文本检索

## 决定

M10 保留既有文本切块作为默认行为，并将 Clang LibTooling 置于 `LLCL_ENABLE_CLANG` 可选构建开关之后。
开启后，索引器依据目标仓库的 `compile_commands.json` 解析每个 C++ 翻译单元，生成函数和类的语义 Chunk，
并把符号、调用、继承和 override 关系写入独立的 `symbols.sqlite3`。

查询包含限定名（例如 `demo::Widget::run`）时，符号图先给出精确命中并扩展一跳关系；该列表与向量和
FTS5/BM25 候选通过 RRF 融合。“谁调用”类查询沿 `calls` 入边扩展。缺少编译数据库、诊断错误或未生成
语义 Chunk 时，当前文件记录告警并使用稳定的文本 Chunk，不终止整个索引任务。

## 后果

默认构建不需要 LLVM/Clang，现有 CPU、CUDA、Docker 和 CI 路径保持可用。开启语义模式需要目标仓库提供
正确的编译数据库，因此生成文件、平台宏或外部 SDK 缺失仍可能触发单文件 fallback。符号 Chunk id 与符号 id
一致，使图检索结果可直接由既有 Chunk repository 获取源码和引用。

初版覆盖函数、方法、类/结构、枚举、调用、继承、override、模板实体和宏展开后的函数体；跨翻译单元的
声明—定义归并、宏定义本身和成员变量摘要留作后续迭代。
