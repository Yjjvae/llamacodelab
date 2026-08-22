# Future Plan：前后端架构演进

本文记录 M0–M12 既有阶段之外的后续工程方向，不新增里程碑编号。实施时按优先级逐项建立独立分支和 PR；
每个 PR 都必须包含可复现测试、迁移说明和对应 worklog。

## 目标架构

```text
CLI             TUI             VS Code extension
 │               │                       │
 └───────────────┴────── HTTP/SSE v1 ────┘
                              │
                       Typed API Adapter
                              │
                       AssistantRuntime
                 ┌────────────┼──────────────┐
                 │            │              │
          StatusService  IndexCoordinator  QueryService
                              │              │
                              └──────┬───────┘
                                     │
                         Atomic RetrievalSnapshot
                     vectors + chunks/FTS + symbols
                                     │
                         GenerationScheduler / llama.cpp
```

`apps/server/main.cpp` 最终只负责依赖装配；仓库状态、索引协调、检索和问答编排进入可独立测试的应用服务。
所有交互客户端只依赖版本化 HTTP/SSE 契约。

## P0：正确性与一致性

这些工作在 TUI 实现前完成。

### 索引代际一致性

- 增加 `IndexCoordinator`，同一仓库同时只允许一个索引任务；重复请求返回明确的 `409 index_busy`。
- 第一阶段用读写锁让 search/ask 与索引提交互斥，立即消除“旧向量 + 新 SQLite Chunk”的窗口。
- 第二阶段把 vector index、Chunk repository、FTS searcher、symbol repository 和 generation 打包为不可变
  `RetrievalSnapshot`，构建完成后一次原子发布；旧查询继续持有旧 snapshot。
- 将 Chunk/FTS 与 symbol 更新纳入同一 generation 语义；进程崩溃后只加载 manifest 指向的完整 generation，
  并回收未引用的向量文件和临时产物。

验收：并发 index/search/ask 压力测试不出现缺失 Chunk、错误引用、SQLite busy 或重复 generation；在向量写入、
SQLite 提交、符号提交和 snapshot 发布处注入失败，重启后只能看到完整旧代或完整新代。

### 模型和索引身份

- 对 embedding GGUF 计算真实 SHA-256；metadata 字段名、二进制 header 和日志统一使用该值。
- 索引身份同时包含 embedding SHA-256、维度、归一化、Chunk/Parser 版本和关键检索配置。
- 同名模型内容变化、语义切块开关变化或不兼容 schema 变化时明确要求重建，不静默复用旧向量。

验收：替换同名 GGUF、修改 parser/chunker 版本和伪造 metadata 都能被自动测试识别并拒绝加载。

### 流式并发与关闭

- generation worker 只向有界 `TokenChannel` 写事件，HTTP provider 线程独占 `DataSink`。
- channel 支持背压、客户端断连、首 token 前取消和 server shutdown；每个任务只能完成 promise 一次。
- 队列容量定义包含运行中还是仅等待中必须明确，并在 status/metrics 中使用相同口径。

验收：SSE 任意字节分帧、慢客户端、断连、队列满和带等待任务关闭均无悬挂线程、未完成 future 或重复事件。

## P1：稳定的客户端契约

### 类型化 API v1

- 定义共享 DTO：`ServiceStatus`、`IndexStatus`、`SearchItem`、`CitationItem`、`ApiError` 和 SSE event variant；
  HTTP adapter 只负责 JSON 映射。
- 增加 `GET /v1/status`：返回 API 版本、模型/索引状态、generation、Chunk 数、默认 `top_k`、队列深度和
  最近错误；保留 `/healthz`、`/readyz` 供探针使用。
- search 返回单个 `SearchItem` 数组，不再用位置对应的 `hits`/`chunks` 平行数组。
- citation 返回 `chunk_id`、source id、路径、行号、语言、分数和实际进入 prompt 的 Chunk 内容。
- 为 JSON 和 SSE 建立 golden contract 测试；兼容字段只能新增，破坏性修改必须升级 API 版本。

### 应用编排

- 引入 `AssistantRuntime`，统一持有状态、当前 retrieval snapshot、IndexCoordinator、QueryService 和生成调度器。
- CLI 的持久化问答与 server 使用同一 QueryService，消除 CLI 临时内存索引和 server 持久化检索的行为分叉。
- model load、首次索引改为后台状态机；HTTP server 先监听，客户端能观察 `loading_model -> indexing -> ready`
  或 `failed`。
- 将占位的 `/metrics` 替换为真实计数与延迟；记录请求量、错误、索引耗时、检索 p50/p95、TTFT、decode、
  队列等待和取消次数。

## P1：TUI 工作台

后端 P0 和 API v1 完成后，按 [TUI 工作台规划](tui-plan.md) 实施。

- `llcl_tui_client` 负责 URL、HTTP、SSE 增量解析和 DTO；不依赖 FTXUI。
- `TuiViewModel` 以事件驱动状态机管理连接、索引、提问、取消、引用选择和错误；可完全无终端单测。
- `llcl-tui` 只实现三栏布局、滚动、焦点和快捷键；不加载模型、不读取完整仓库文件、不管理 server 进程。
- 第一版问题相互独立；界面明确提示当前后端不使用视觉聊天历史进行追问。

验收：80x24 与窗口缩放可用；中文流式输出无破损；引用源码与 prompt Chunk 一致；429、503、断线和取消有
稳定状态；退出后恢复终端且没有后台线程残留。

## P2：体验与检索演进

- 多轮 RAG：让 QueryService 接收完整 messages，经 tokenizer budget 裁剪后将历史与检索上下文共同构造
  prompt；增加代词追问和历史裁剪评测。
- 文件监听：inotify + debounce 处理编辑器原子替换，后台增量索引；查询始终使用上一完整 generation。
- 多仓库 workspace：每个仓库独立配置、索引目录、generation 和状态；API 通过稳定 workspace id 选择目标。
- 检索解释：返回 vector/BM25/symbol 各自名次和融合理由，TUI 可用于诊断召回失败，不把内部量纲直接混加。
- 真实评测闭环：扩充 ground truth，持续记录 Recall/MRR/nDCG、回答正确性、引用有效率及 CPU/CUDA 性能回归。

## P3：IDE、工具调用与交付

- VS Code 扩展复用 HTTP/SSE v1：选中代码提问、引用跳转、索引状态、取消生成；扩展不承载模型或索引逻辑。
- 只读工具调用先开放 `find_symbol`、`get_callers`、`read_source_range`、`search_text`，以 JSON Schema 和
  服务端白名单校验；默认不开放 shell。
- 补丁生成采用“生成 diff -> 用户审阅 -> 显式应用”，记录目标文件 hash，拒绝覆盖已经变化的内容。
- 非 loopback 部署前加入鉴权、TLS、仓库访问隔离、请求大小/速率限制和安全审计；默认仍只监听本机。
- 提供可复现 Docker/本机安装包、配置迁移工具和索引 schema 升级/回滚说明。

## 实施顺序

1. 模型 SHA-256 与索引身份。
2. IndexCoordinator、读写一致性和故障注入测试。
3. TokenChannel、队列关闭语义和 SSE 压力测试。
4. 类型化 API v1、status、citation content 和契约测试。
5. AssistantRuntime、后台启动和真实 metrics。
6. TUI client/view-model/FTXUI 三层实现。
7. 多轮、文件监听、多仓库和检索解释。
8. VS Code、受控工具调用与远程部署能力。

任何步骤若没有对应的可测量问题、回归测试和验收证据，不进入下一项。
