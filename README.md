# dsh-windhawk-status

把 DSH 会话状态显示在 Windows 任务栏通知区域左侧。
<img width="1295" height="579" alt="2011437f134f98f24f04fcfb38c6e6b4" src="https://github.com/user-attachments/assets/1a728689-9182-4a66-8638-2c58324fd33a" />


由两半组成，各自独立运行：

| 部分 | 文件 | 作用 |
|---|---|---|
| DSH 插件 | `index.js` + `client.js` | 监听 DSH 事件，把会话状态发到本地 TCP，并接收跳转请求 |
| 任务栏模组 | `windhawk/dsh-taskbar-status.wh.cpp` | 把状态画成任务栏小窗，处理点击 |

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

## 功能

- **多会话**，排在托盘通知区域左侧。跑几个显示几个。
- **实时同步**。没跑过的会话根本不显示
- **超时消失**，默认 10 分钟，模组设置里可调，填 0 关闭。正在运行的会话不会因超时消失。
- **左键** → 详情面板：状态 / 模型 / 目录 / 工具 / 任务 / 进度 / Token / 上下文 / 运行时间 / PID，底部两个按钮：`打开会话`、`隐藏此实例`。
- **右键** → 直接跳到对应会话，不弹面板。
- 窗口区域没有实例时自动折叠，不占任务栏空间。
