# How to use codex cli

> 下面的回答都是ChatGPT plus 的回答，本来我一直用Claude code的，后来离职了没令牌了，那得用codex了哈哈哈

Codex CLI 的命令体系可以分成三类：

**① Slash 命令（交互模式最常用）**
**② CLI 命令（终端直接执行）**
**③ Shell增强（! 执行系统命令）**

下面按实际开发使用频率整理一套“够用 + 常用”的命令清单。

---

## 一、Slash 命令（核心）

在 Codex 里输入 `/` 就能看到所有命令列表 ([OpenAI Developers][1])

## 🔧 会话 / 控制类

```bash
/new        # 新建对话（清空上下文）
/quit       # 退出 codex
/exit       # 同上（别名）
/status     # 查看当前状态（模型、token、目录等）
```

👉 `/status` 很关键，可以看：

* 当前模型
* token 使用
* 工作目录 ([Amanhimself][2])

---

## 🤖 模型 / 推理控制

```bash
/model      # 切换模型（如 gpt-5 / gpt-5.4）
/fast       # 开启更快推理（消耗更高）
```

---

## 🧠 上下文优化（非常实用）

```bash
/compact    # 压缩上下文（减少 token 占用）
/clear      # 清空对话（有些版本叫 /new）
```

👉 当你看到 token 快用完时必须用 `/compact`

---

## 📁 项目初始化 / 配置

```bash
/init       # 初始化项目（生成 AGENTS.md 等）
```

👉 用于定义：

* 代码风格
* 项目规则
* Codex 行为

---

## 🔍 代码 /变更相关（部分版本支持）

```bash
/diff       # 查看修改差异
/approvals  # 查看/管理执行权限
```

---

## 🧩 自定义命令（高级玩法）

```bash
/prompts:xxx
```

👉 来自：

```shell
~/.codex/prompts/*.md
```

可以做：

* 一键生成代码模板
* 一键部署脚本

---

## 二、CLI 命令（终端执行）

这些是在 codex 外用的：

## ▶️ 启动

```bash
codex
```

---

## 🔄 会话恢复

```bash
codex resume --last
codex resume <session_id>
```

👉 会从本地 session 文件恢复上下文 ([Amanhimself][2])

---

## 🔐 登录相关

```bash
codex login
codex logout
```

---

## ⚙️ 指定参数运行

```bash
codex --model gpt-5
codex --approval-mode suggest
```

---

## 三、Shell 增强（很多人不知道的高效点）

在 Codex 内直接执行系统命令：

```bash
!ls
!pwd
!git status
```

👉 特点：

* 输出会进入上下文
* Codex 可以基于结果继续推理 ([Amanhimself][2])

---

## 四、快捷键（提升效率）

```bash
Ctrl + C   # 中断 / 退出
Ctrl + K   # 清屏（部分终端）
↑ ↓        # 历史命令
```

---

## 五、最常用命令组合（实际开发场景）

## 场景1：项目分析

```bash
/init
Explain this codebase
```

---

## 场景2：token 快用完

```bash
/compact
/status
```

---

## 场景3：卡住 or 出错

```bash
Ctrl + C
/new
```

---

## 场景4：继续上次工作

```bash
codex resume --last
```

---

## 六、一个核心认知

Codex CLI 本质是：

> **对话驱动 + 命令增强的开发代理**

所以命令不是重点，重点是：

* `/` 控制状态
* 自然语言做任务

---

## 总结（最小必记集）

只记这几个就够用了：

```bash
/new
/status
/model
/compact
/quit
!git status
codex resume --last
```

---

如果后面要深入（比如 approvals / agent 自动执行 / CI 集成），可以再展开，那一层就已经是“把 Codex 当自动化开发 agent 用”了。

[1]: https://developers.openai.com/codex/cli/slash-commands/?utm_source=chatgpt.com "Slash commands in Codex CLI"
[2]: https://amanhimself.dev/blog/first-few-days-with-codex-cli/?utm_source=chatgpt.com "First few days with Codex CLI | amanhimself.dev"
