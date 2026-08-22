# TUI 工作台规划

## 目标

新增独立的 `llcl-tui` C++20 终端客户端，连接已运行的本地 `llcl-server`，提供索引控制、SSE
流式问答、引用源码预览和检索调试。首版不加载模型、不管理 server 进程、不持久化会话，也不读取仓库
完整文件。

## 架构与依赖

- 以 FTXUI `v7.0.2` 的固定版本接入 CMake FetchContent，新增 `apps/tui/` target。
- 复用现有 `cpp-httplib` 作为 HTTP 客户端，避免引入第二个网络库。
- TUI 通过既有 HTTP/SSE API 访问 server；模型、索引、生成队列和语义检索逻辑仍归属
  `llcl-server`。
- 不需要浏览器 CORS；取消通过关闭 SSE 请求实现，沿用服务端的客户端断连取消机制。

## 实施前置条件

TUI 开发前先完成以下高优先级后端加固，避免把不稳定契约固化到客户端：

1. 增加仓库级 `IndexCoordinator`，拒绝并发索引；查询和索引至少先用读写锁保证一致性，随后升级为一次
   原子发布向量、Chunk、FTS 和符号数据的 `RetrievalSnapshot`。
2. 使用模型文件真实 SHA-256 作为 embedding 索引指纹，不能继续以文件名的 FNV hash 代替。
3. 增加类型化 API DTO 和 `GET /v1/status`，返回 API 版本、服务状态、索引状态、generation、Chunk 数、
   默认 `top_k`、队列深度和最近错误。
4. citation 增加 `chunk_id`、`language` 和 `content`，使源码预览与实际送入模型的 Chunk 完全一致。
5. SSE 改为“生成线程 -> 有界 token channel -> HTTP provider 线程 -> socket”，不跨线程直接操作
   `httplib::DataSink`；补齐断连、背压和关闭时的 promise/task 收尾。
6. 明确聊天语义：首版 TUI 的每次代码问答相互独立；在后端正式支持完整 `messages` 历史前，界面不能暗示
   追问会自动继承前文。

## 命令与布局

提供 `llcl-tui`：

```text
llcl-tui --server http://127.0.0.1:8080 --top-k 8
```

- `--server` 默认 `http://127.0.0.1:8080`。
- 未传 `--top-k` 时使用 `/v1/status` 返回的服务端默认值。
- 启动时调用 `/healthz` 和 `/v1/status`，在状态栏显示连接状态、加载/索引/就绪状态和模型名。

界面采用三栏工作台：

```text
┌ 服务/索引 ─────────┬ 对话 ─────────────────────┬ 引用与源码预览 ───────┐
│ model / readiness  │ 问题输入、流式回答        │ 路径、行号、分数        │
│ index 统计         │ Enter 发送，Esc 取消      │ 上下选择，预览 Chunk    │
│ r: 重新索引        │ d: 检索调试               │                         │
└────────────────────┴───────────────────────────┴─────────────────────────┘
```

### 交互

- 左栏：服务状态、模型名、索引 generation 和 Chunk 数；`r` 调用 `POST /v1/index`。索引进行中禁止重复
  提交，并以 `/v1/status` 刷新成功统计或失败信息。
- 中栏：运行期聊天历史和问题输入；`Enter` 发送，`Esc` 取消当前流式请求，`Ctrl+C` 退出。
- 右栏：引用或检索结果列表；上下键选择项，显示 citation 返回的路径、行号、语言、相关性分数和 Chunk
  内容。
- `d` 打开/关闭检索调试面板：调用 `POST /v1/search`，展示命中顺序、分数、文件和 Chunk 内容，不生成
  回答。

## 并发与错误处理

- FTXUI 主线程只负责渲染和键盘事件。
- 索引、检索和 SSE 读取在工作线程执行；结果通过线程安全事件队列回传主线程，避免网络阻塞终端刷新。
- SSE 解析器按事件边界处理 `token`、`citations`、`metrics`、`done`、`error`。
- 断线、`429 queue_full`、`503 not_ready`、HTTP/JSON 错误映射为状态栏提示，保留已有会话内容。
- 会话只在当前 TUI 进程中存在；退出时停止后台任务，不写本地历史文件。
- HTTP/SSE、DTO 和状态转换放入独立的 `llcl_tui_client`/view-model target；FTXUI target 只负责渲染与
  键盘映射，便于无终端单测。

## 测试与验收

- 单元测试：SSE 分帧与跨 chunk JSON 解析、事件状态转移、引用选择、错误映射、URL 与输入校验。
- 集成测试：基于 HTTP 服务 fixture 验证状态探测、索引请求、检索结果转换、完整 SSE 生命周期及
  429/503/断线场景。
- 并发验收：重复触发索引只接受一个任务；索引更新期间 search/ask 不出现向量与 Chunk 代际错配；关闭
  server 或 TUI 时所有等待任务均能结束。
- 手工验收：最小 80×24 终端可用；窗口缩放不崩溃；长回答和长 Chunk 可滚动；中文 token 增量显示正确；
  退出后无终端残留。
- 文档：更新 README、CONTRIBUTING、WORKLOG 和实现教程，记录 server 启动方式、TUI 命令、依赖与快捷键。
