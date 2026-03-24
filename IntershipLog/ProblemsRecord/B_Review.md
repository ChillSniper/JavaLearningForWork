# 简历经历复盘 - Apollo 配置中心集成

> 对应简历：负责接入并优化携程 Apollo 配置中心的集成方案，通过 gconf 组件封装了多 Namespace 的配置绑定逻辑，并实现了配置热更新监听机制。

---

## 面试官会问什么？

1. Apollo 配置中心是什么？你们为什么要用它？
2. `gconf` 是什么？你是自己写的还是封装好的？
3. 什么是 Namespace？你们项目里有哪几个 Namespace？
4. 热更新的原理是什么？`OnChange` 是怎么触发的？
5. 配置更新后，内存里的对象是怎么更新的？有没有并发安全问题？
6. 如果 Apollo 挂了，服务会怎样？你们有没有降级方案？

---

## 1. Apollo 配置中心是什么？

Apollo 是携程开源的分布式配置中心。核心价值在于：**把配置从代码和本地文件中剥离出来，统一管理，并支持实时推送变更**。

使用场景包括：

- **数据库连接**：MySQL host/port/密码，Redis 地址
- **第三方服务密钥**：SMS 密钥、微信 AppSecret、OBS 云存储 AK/SK
- **业务开关**：`WxOrderClose`（测试环境不执行定时任务）、`OldGmForbidden`
- **外部接口 URL**：游戏 GM 后台地址 `SlmGm`、`HlsgGm`，邮件服务接口路径
- **运行时参数**：分页大小、申诉限流配置、时区
- **Worker 并发参数**：消费者线程数、批量大小、轮询间隔

最大价值：**改配置不用重新发布服务**，运维友好且风险低。

---

## 2. gconf 是什么？

`gconf` 是公司内部基于 `agollo`（Apollo 官方 Go 客户端）封装的组件，包路径：`tygit.tuyoo.com/gocomponents/gconf`。

提供了 `BindConfig` API，通过 Option 模式将不同 Namespace 的配置绑定到对应的 Go 结构体，一行调用完成：连接 Apollo、拉取配置、反序列化到结构体、注册变更监听。

**代码示例（`internal/conf/init.go`）：**

```go
gconf.BindConfig(
    GlobalConf,                                // 把配置绑定到这个结构体
    gconf.WithEnableChangeListener(true),      // 开启热更新监听
    gconf.WithAppID("10067-customer-service"), // 服务在 Apollo 的唯一标识
    gconf.WithDevSecret("f7d0750..."),         // 不同环境的密钥
    gconf.WithNamespaceName("cfg.yaml"),       // 绑定哪个 Namespace
)
```

---

## 3. 什么是 Namespace？

**Apollo 里的 Namespace 就是一个配置文件。** 不同 Namespace 存放不同用途的配置，互相隔离、独立管理。

项目里共有 7 个 Namespace：

| Namespace 文件名          | 对应 Go 变量      | 存放内容                             |
| ------------------------- | ----------------- | ------------------------------------ |
| `cfg.yaml`                | `GlobalConf`      | 数据库、Redis、服务器基础配置        |
| `project.json`            | `ProjectConfig`   | 申诉项目、邮件、GM 域名等业务配置    |
| `project_v2.json`         | `ProjectConfigV2` | 开票配置、CloudId 配置等             |
| `user_manage_config.json` | `ManageConfig`    | 微信公众号、礼包码域名等管理平台配置 |
| `worker.json`             | `WorkerConfig`    | Worker 并发数、批量大小等运行参数    |
| `gift_code_config.json`   | `GiftCodeConfig`  | 礼包码相关配置                       |
| `crm_config.json`         | `CRMConfig`       | CRM 相关配置                         |
| `incentive_config.json`   | `IncentiveConfig` | 提现名单配置                         |

---

## 4. OnChange 回调原理 & "内存"指什么？

**触发流程：**

```go
Apollo 控制台修改配置
        ↓
Apollo 服务端推送变更通知（长连接）
        ↓
agollo 客户端（gconf内部）收到通知
        ↓
调用对应 Listener 的 OnChange(changeEvent)
        ↓
changeEvent.Changes 包含：key、oldValue、newValue
        ↓
JSON 反序列化 → 新的 Config 结构体
        ↓
全局变量指针替换（WorkerConfig = &newCfg）← 更新内存
        ↓
triggerWorkerReload() ← 通知其他模块热加载
```

**代码示例（`internal/conf/worker.go`）：**

```go
func (c *WorkerChangeListener) OnChange(changeEvent *storage.ChangeEvent) {
    for _, value := range changeEvent.Changes {
        var newCfg workerConfig
        err := jsoniter.UnmarshalFromString(fmt.Sprintf("%v", value.NewValue), &newCfg)
        if err != nil { continue }

        WorkerConfig = &newCfg  // 替换内存中的全局变量指针
        triggerWorkerReload()   // 通知其他模块
    }
}
```

**"内存"指的是**程序运行时 heap 上的全局变量，如 `conf.GlobalConf`、`conf.WorkerConfig` 等全局指针。替换指针后，所有引用该变量的业务代码自动读到新值，**无需重启服务**。

---

## 5. Apollo 总共管理哪些配置？

| 类别             | 具体例子                                             |
| ---------------- | ---------------------------------------------------- |
| **数据库连接**   | MySQL host/port/password，Redis 地址，ES 地址        |
| **第三方密钥**   | OBS AK/SK，易盾 SecretKey，微信 AppSecret，SMTP 密码 |
| **游戏 GM 域名** | `SlmGm`、`HlsgGm`（游戏组提供的 GM 后台接口地址）    |
| **业务开关**     | `WxOrderClose`（是否执行定时任务）、`OldGmForbidden` |
| **API URL 路径** | `SlmEmail.Product/Item/Task`（邮件服务接口路径）     |
| **运行时参数**   | Worker 并发数、批量大小、超时时间                    |
| **安全密钥**     | JWT Secret、AES Key、Sign Secret                     |

---

## 面试介绍话术

> "我负责把项目的 Apollo 配置中心做了梳理和优化。具体来说，我们用了公司内部的 `gconf` 组件，它封装了 Apollo 官方的 Go 客户端，提供了 `BindConfig` API。
>
> 整个项目按业务模块拆分了 7 个 Namespace，我把每个 Namespace 的配置分别绑定到对应的 Go 结构体上，比如 `GlobalConf` 绑定基础设施配置，`WorkerConfig` 绑定 Worker 并发参数等。
>
> 热更新这块，我给每个 Namespace 都实现了 `OnChange` 监听器，Apollo 配置一变更，监听器会拿到新的配置值，JSON 反序列化后直接替换内存中的全局变量指针，业务代码下次读取时就自动拿到新值，不需要重启服务。
>
> 实际价值体现在，比如 Worker 并发数、游戏 GM 域名这些参数，以前改一次要走发布流程，现在在 Apollo 控制台改一下，秒级生效。"
