# dsh-windhawk-status

把 DSH 会话状态显示在 Windows 任务栏通知区域左侧。

由两半组成，各自独立运行：

| 部分 | 文件 | 作用 |
|---|---|---|
| DSH 插件 | `index.js` + `client.js` | 监听 DSH 事件，把会话状态发到本地 TCP，并接收跳转请求 |
| 任务栏模组 | `windhawk/dsh-taskbar-status.wh.cpp` | 把状态画成任务栏小窗，处理点击 |

模组 fork 自 `OpenCode_Windhawk_Status_Plugin`。XAML 任务栏注入、单行/双行布局、占位符格式、详情 Flyout **全部原样保留**，只加了默认端口、右键跳转会话、无动作超时三件事。原仓库文件未做任何修改。

---

## 目录结构

```
dsh-windhawk-status/
├── index.js                          Host 半边：事件监听 + TCP 发布 + 控制端口
├── client.js                         Client 半边：轮询跳转请求并导航 UI
├── package.json                      Cordis bundle 声明
├── cordis.patch.yml                  插入到 profile 的补丁
├── windhawk/
│   └── dsh-taskbar-status.wh.cpp     任务栏模组（单文件，可直接贴进 Windhawk）
└── tools/
    ├── check-plugin.mjs              Host 半边测试（假 Cordis ctx）
    ├── check-client.mjs              Client 半边测试（shim ModuleLoader）
    ├── check-mod-settings.mjs        模组设置块与 C++ 读取端一致性检查
    └── mock-producer.mjs             假会话生产者，用于单独调试模组
```

---

## 安装

### 1. 任务栏模组

Windhawk → **新建本地模组** → 把 `windhawk/dsh-taskbar-status.wh.cpp` 整个贴进去 → 编译 → 对 `explorer.exe` 启用。

和原版 `opencode-taskbar-status` 可以同时装着，互不干扰：

- 原版绑 **19288** 收 OpenCode
- 本模组绑 **19290** 收 DSH

也可以只装本模组，把 OpenCode 插件的 `OPENCODE_STATUS_PORT` 也设成 19290 —— 本模组接受两种载荷，两者会显示在同一排小窗里。

### 2. DSH 插件

已经挂进 `desktop` profile 的 `dsh.profile.bundles`，`package.json` 里以 `link:` 指向本目录。**重启 DSH Desktop 生效。**

---

## 端口

| 端口 | 方向 | 默认 | 环境变量 |
|---|---|---|---|
| 状态端口 | 插件 → 模组 | 19290 | `DSH_STATUS_PORT` |
| 控制端口 | 模组 → 插件 | 19289 | `DSH_STATUS_CONTROL_PORT` |

> 19288 被原版 OpenCode 任务栏模组以 `SO_EXCLUSIVEADDRUSE` 独占，所以这里默认走 19290。

心跳间隔 `DSH_STATUS_HEARTBEAT_MS`（默认 5000），跳转请求存活时间 `DSH_STATUS_FOCUS_TTL_MS`（默认 15000）。

---

## 行为

- **一个会话一个小窗**，排在托盘通知区域左侧。跑几个显示几个。
- **只在运行后出现**。没跑过的会话根本不发布 —— 模组会渲染收到的每一个实例，所以"只在运行后显示"必须在插件侧拦住。会话第一次开始干活的那一刻才开始发布。
- **无动作超时消失**，默认 10 分钟，模组设置里可调，填 0 关闭。正在运行的会话不会因超时消失。
- **左键** → 详情面板：状态 / 模型 / 目录 / 工具 / 任务 / 进度 / Token / 上下文 / 运行时间 / PID，底部两个按钮：`打开会话`、`隐藏此实例`。
- **右键** → 直接跳到对应会话，不弹面板。
- 窗口区域没有实例时自动折叠，不占任务栏空间。

### 超时为什么要记一笔

插件每 5 秒发一次心跳。如果模组只是"超时就把小窗删掉"，下一次心跳会立刻把它放回来 —— 每 5 秒闪一次。

所以模组记下超时那一刻的 `lastActivityAt`。心跳重复同一个值，继续压着不显示；一旦 `lastActivityAt` 变大，说明会话重新干活了，小窗自动回来。填 0 关闭时这份记录会清空，所有会话在下一条消息时回来。

---

## 设置

模组设置（Windhawk UI，16 项）：

| 设置 | 默认 | 说明 |
|---|---|---|
| `port` | 19290 | 状态端口，必须与 `DSH_STATUS_PORT` 一致 |
| `controlPort` | 19289 | 控制端口，必须与 `DSH_STATUS_CONTROL_PORT` 一致 |
| `idleTimeoutMinutes` | 10 | 无动作超时，0 关闭 |
| `layout` | twoLines | `singleLine` / `twoLines` |
| `displayLanguage` | auto | `auto` / `zh-CN` / `en`。只影响 `{approval}` —— 工具名和状态文案跟 DSH 自己的语言走 |
| `singleLineFormat` | `[{progress} ] {status_text}` | 单行格式 |
| `firstLineFormat` | `[{task}] {name}` | 第一行格式 |
| `secondLineFormat` | `[进度: {progress}  ] {status_text} [{runtime}]` | 第二行格式 |
| `width` | 230 | 最大宽度（逻辑像素），超出省略号截断 |
| `lineSpacing` | -6 | 双行上下间距，负值靠拢 |
| `fontSize` | 11 | 字号 |
| `fontFamily` | Microsoft YaHei UI | 字体 |
| `textColor` | auto | `auto` 或 `FFFFFF` 这样的十六进制 RGB |
| `openConversationOnClick` | true | 右键是否直接跳转会话，详情面板的按钮也受它控制 |
| `updateIntervalMilliseconds` | 250 | 位置与显示刷新间隔 |
| `debugLogging` | false | 把 Socket、解析、渲染摘要写进 Windhawk 日志 |

### 工具名和状态文案：模组里没有翻译表

`{status_text}` 和 `{activity}` 直接显示**插件给过来的成品文案**，模组不做任何翻译。语言由 DSH 决定：

```
客户端半边（浏览器里）
  └─ ctx.get('locale').bind('chat')        ← 读 DSH 自己的 locale 服务
  └─ t('message.stepProcess.commands')     ← "正在运行命令"，跟随 DSH 当前语言
  └─ POST /api-dsh-status/labels           ← 把 14 个活动分类的文案报给 Host

Host 半边
  └─ activityCategory(toolId)              ← 只做分类，不存任何翻译
  └─ activity     = 文案（没有就退回 toolId）
  └─ activityId   = 原始 toolId，用于匹配

模组
  └─ 原样打印
```

所以：

- **不会漏**。DSH 出新工具、或你接了 MCP 工具，只要它归到某个分类就有文案；归不到就显示原始 id（`mcp__thing__do_it`），**永远不会空着**。
- **不用手工维护**。之前模组里那张 OpenCode 工具名表已经删掉了（`edit`/`pwsh`/`todo_write` 这些当时全都漏了）。
- **跟随 DSH 语言**。你在 DSH 里切语言，任务栏文案跟着变；模组的 `displayLanguage` 现在只管 `{approval}` 那两句。

分类用的是 DSH 聊天 UI 自己那套（`dsh-client-ui-chat` 的 `activity()`），共 14 类：
`thinking` `read` `readImage` `write` `search` `edit` `commands` `code`
`webSearch` `webFetch` `subagents` `plan` `questions` `tools`。

### 占位符

`{app}`, `{name}`, `{status}`, `{status_text}`, `{activity}`, `{progress}`,
`{current}`, `{completed}`, `{total}`, `{task}`, `{tokens}`, `{input_tokens}`,
`{output_tokens}`, `{reasoning_tokens}`, `{cache_read}`, `{cache_write}`,
`{context_used}`, `{context_limit}`, `{context_percent}`, `{runtime}`, `{provider}`,
`{model}`, `{approval}`, `{indicator}`, `{pid}`, `{instance_id}`, `{instance_count}`。

`{task}` / `{progress}` / `{current}` / `{completed}` / `{total}` 都来自**会话的任务列表（`todo_write`）**：

- `{progress}` = `已完成/总数`（如 `2/5`）
- `{task}` = 正在做的那一条（`in_progress`）；没有标 `in_progress` 的，就用第一条未完成的
- `{current}` = `{completed}` = 已完成条数，`{total}` = 总条数

**没有任务列表时**退回**目标（goal）**的轮数：`{progress}` = `已跑轮数/上限`，`{task}` = 目标正文。
两者都没有就是空的，`[{progress} ]` 这种可选块会整块省略。

`node tools/probe.mjs` 会打出每个会话的 `todos` 条数、`hasGoal` 和算好的 `progress`，
一眼能分清"读不到"还是"本来就没有"。

> 任务列表有两个来源：`todo_write` 的 **block-end**（完整 `arguments` 只在这里出现，delta 里没有），
> 以及**会话日志**里最后一条 `tool/call`——后者用来收养已存在的会话，否则重启后要等
> 下一次 `todo_write` 才会显示。
>
> 解析失败一律忽略而不是清空，免得一个坏载荷把已经显示出来的任务行抹掉。
>
> `goals.get()` 会校验传入的必须**就是**注册表里的那个活 Agent，传个 `{ id }` 形状的替身会抛错。
> 插件必须先用 `agents.get(sessionId)` 拿到真身再查。

空值占位符可以包在可选块里，块内任一占位符为空时整块省略：`[{progress} ]{task}`

`{app}` 取载荷里的 `app` 字段，DSH 会话显示 `DSH`；老载荷没有该字段时退回 `OpenCode`。

---

## 跳转会话：三跳桥

模组跑在 `explorer.exe` 里，碰不到 DSH 的页面，**没法自己选中某个会话**。DSH Desktop 也没有会话级深链（`--dsh-desktop-workspace` 只能打开 workspace，选不到对话）。所以走三跳：

```
右键小窗
  └─ 模组  ActivateDshDesktop()  抬起 DSH 窗口
  └─ 模组  → 19289  {"action":"focus","sessionId":"session-…"}
       └─ Host  存下 pendingFocus = {sessionId, at}
            └─ Client 轮询 GET /api-dsh-status/pending
                 └─ Client  uiWorkspace.openSession(sessionId)
                      └─ Client  POST /api-dsh-status/focus-done
```

客户端按 Host 下发的时间戳去重，所以同一个会话连点两次也会重新导航。

---

## 协议

本地 TCP，NDJSON，一行一条：

```json
{
  "protocol": "opencode-status",
  "version": 2,
  "type": "status",
  "source": "dsh-status",
  "app": "DSH",
  "openable": true,
  "instanceId": "Desktop-…:session:session-…",
  "pid": 12345,
  "session": {
    "id": "session-…", "name": "…", "directory": "C:\\…",
    "status": "working",
    "statusId": "commands", "statusText": "正在运行命令",
    "activityId": "pwsh", "activity": "正在运行命令",
    "indicator": "working", "approval": "", "hasRun": true,
    "runStartedAt": 1791042750879, "lastActivityAt": 1791042751931,
    "progress": { "current": 0, "completed": 0, "total": 0, "label": "", "task": "" },
    "model": { "providerId": "…", "modelId": "…" },
    "tokens": { "input": 0, "output": 0, "reasoning": 0, "cacheRead": 0,
                "cacheWrite": 0, "total": 0, "contextUsed": 0,
                "contextLimit": 200000, "contextPercent": 0 }
  }
}
```

移除会话：`{"type":"remove","instanceId":"…","session":null}`。

**`…Id` 用于匹配，`…Text`/`activity` 用于显示。** `activityId` 是原始工具名（`pwsh`、`edit`、`mcp__thing__do_it`），`statusId` 是活动分类（`commands`、`edit`、`tools`…）。显示文案由 Host 从客户端上报的 DSH 文案里取，取不到就退回 id 本身。

**`openable` 必须在顶层。** 模组的 JSON 查找是扁平子串匹配，写在 `session` 里会被顺带找到 —— 能跑，但靠的是运气。顶层才是它真正读取的位置，`check-plugin.mjs` 里有断言守着这条。

---

## 调试

```powershell
# 不装模组也能看插件发了什么
node tools/mock-producer.mjs --count 3        # 3 个假会话
node tools/mock-producer.mjs --scripted       # 走完整生命周期
node tools/mock-producer.mjs --control        # 顺便监听控制端口，打印跳转请求
```

```powershell
node tools/check-plugin.mjs        # Host 半边：收养、自愈、线协议、生命周期、审批放行
node tools/check-client.mjs        # Client 半边：openSession 真被调用、回执真发出
node tools/check-mod-settings.mjs  # 模组设置块与 C++ 读取端一致性
```

模组日志在 Windhawk 的 **日志** 标签页，搜 `dsh-taskbar-status`。把 `debugLogging` 打开会看到每次渲染摘要和超时记录。

---

## 已知限制

- **跳转需要插件可达**。控制端口连不上时，窗口照样抬起，但会话不会被选中。
- **会话未启动时点右键** 只会抬起 DSH 窗口，因为没有会话可跳。
- 详情面板只有两个动作：`打开会话` 和 `隐藏此实例`。原版的 `打开终端` 按钮已删掉 —— DSH 没有终端窗口，那个按钮对本插件永远无事可做。`terminalWindow` 字段仍然保留，因为接收路径用它做实例去重。
- 模组注入 `explorer.exe`；任务栏重启后会自动重新注入。
- 超时是显示层的，不影响会话本身 —— 会话还在跑，只是不再占着任务栏。
- **模组只信任两个进程镜像**：`opencode.exe` 和 `DSH Desktop.exe`。插件上报的 `pid` 不在这个名单里时，模组会判定"进程已退出"，每 250 ms 把小窗删一次 —— 表现就是任务栏**完全空白**，而插件日志里 `sendOk` 一路增长。换宿主（改了 exe 名字的分支）要在 `IsTrackedAppImage()` 里补一行。
- **widget 名字改成了 `DshStatusWidget`**。原版模组用 `OpenCodeStatusWidget`，两个模组都开着时会往同一棵任务栏 XAML 树注入，重名会互相找到对方的控件。
- **模组不再翻译工具名**。`edit`/`pwsh`/`todo_write` 这些以前全漏，因为那张表是照 OpenCode 的词表写的。现在文案由 DSH 提供，模组只负责打印。代价是：**如果给模组喂 OpenCode 的载荷**（没带 `activityId`），工具名会原样显示英文，不再翻译。原版模组还在，需要 OpenCode 翻译就用它。
- **文案要等客户端上报**。DSH 页面加载后第一次轮询才把文案 POST 给 Host，在那之前小窗显示的是原始 id（`pwsh`），一秒内会被替换成正式文案。控制台日志里有 `labels updated (14 entries)`。
- **完成提示音整个功能已移除**：设置项、`ExpandSoundPath`/`PlayCompletionSound`、MCI 调用、`-lwinmm` 和 `<mmsystem.h>` 都没了，模组因此小了约 45 KB。原来那套 `completionArmed` / `completionPendingAt` 只服务于提示音，一并删掉。
