# 实习经历复盘：红包激励黑白名单管理系统

> 对应简历描述：设计并实现了红包激励黑白名单管理系统，封装激励服务 HTTP 客户端 SDK，实现 SHA-256 多字段组合签名认证，保障跨服务调用安全性。采用 Functional Options 模式支持 Endpoint 灵活配置，提升 SDK 的可扩展性。通过异步 Goroutine 记录黑白名单的全生命周期操作，支持按操作人、操作时间等多维度查询。

---

## 一、面试时的完整表述模板（STAR 法则）

> "我负责设计并实现了红包激励系统的黑白名单管理模块。背景是红包场景存在风控需求，需要拦截违规用户、并对特定用户开放权限，同时这个能力要暴露给多个内部服务调用。
>
> 我封装了一个 HTTP 客户端 SDK，屏蔽了底层的签名、请求、错误处理细节，让调用方可以直接调用语义清晰的接口。安全性上，我用 SHA-256 对请求的多个关键字段做组合签名，服务端验签，防止未授权调用和请求篡改。SDK 的配置上，采用 Functional Options 模式，让调用方可以按需传入 Endpoint 等参数，后续扩展新配置项也不会影响已有调用方。此外，黑白名单的增删改都需要记录全生命周期的操作日志，用于审计和查询，这部分我用异步 Goroutine 实现，避免日志写入影响主流程的响应延迟。"

---

## 二、业务需求背景

### 本质：风控系统

红包激励系统（发红包、现金奖励、积分等）是容易被薅羊毛的场景。黑白名单的存在是为了：

| 名单类型   | 作用                                                |
| ---------- | --------------------------------------------------- |
| **黑名单** | 封禁违规用户、刷单用户、高风险账号，不让他们领红包  |
| **白名单** | 给特定用户（如内测用户、KOL、合作商户）开放特殊权限 |

### 面试表述

> "业务背景是红包激励场景下的风控需求。为了防止用户刷单套现或屏蔽高风险用户，同时也需要对特定渠道用户开放权限，所以需要一套黑白名单管理机制，并且这个服务会被多个下游服务调用。"

### 延伸：风控系统常见手段

除了黑白名单，实际风控系统还会组合使用：

- **频率限制（Rate Limiting）**：同一用户/IP 单位时间内请求次数上限，防止暴力刷单
- **设备指纹**：通过设备特征识别同一个人注册多账号
- **行为分析**：异常的操作序列（如领完立即提现）触发风控
- **规则引擎**：可配置化规则，如"新注册 24 小时内不能领红包"
- **实时黑名单下推**：黑名单变更后，推送到各下游服务的本地缓存，减少远程查询延迟

---

## 三、SHA-256 多字段组合签名

### 这不是加密，是哈希签名（数字摘要认证）

**SHA-256 是哈希算法**，特点：

- **单向不可逆**：只能正向哈希，无法从哈希值还原原文
- **雪崩效应**：输入变一个字节，输出完全不同
- **固定长度输出**：无论输入多大，输出都是 256 bit（64位十六进制字符）

### 多字段组合签名的工作流程

```go
调用方（客户端）：
sign = SHA256(timestamp + user_id + request_body + secret_key)
→ 把 sign 放进请求头发给服务端

服务端：
用同样规则重新计算 sign，比对是否一致
→ 一致：请求合法，放行
→ 不一致：请求被篡改或伪造，拒绝（返回 401/403）
```

### 为什么要"多字段组合"，而不是只签一个字段？

- 只签 `body`：攻击者可以重放请求（Replay Attack），把同一个合法请求发多次
- 加入 `timestamp`：请求超过一定时间窗口（如 5 分钟）直接拒绝，防重放
- 加入 `user_id` / `app_id`：防止 A 服务的合法签名被 B 服务冒用

### 进阶：HMAC-SHA256

实际工程中更常用 **HMAC-SHA256**（Hash-based Message Authentication Code）：

```go
sign = HMAC-SHA256(secret_key, message)
```

相比普通 SHA256，HMAC 把密钥融入哈希计算过程，安全性更高，是微信支付、支付宝等开放平台的标准签名方案。

---

## 四、加密算法体系梳理

### 对称加密

加解密使用**同一个密钥**。

| 算法     | 密钥长度        | 特点                              |
| -------- | --------------- | --------------------------------- |
| **AES**  | 128/192/256 bit | 目前最主流，速度快，安全          |
| DES      | 56 bit          | 已被淘汰，密钥太短                |
| 3DES     | 112/168 bit     | DES 的改进版，现在也在被 AES 替代 |
| RC4      | 可变            | 流加密，有已知安全漏洞，不推荐    |
| ChaCha20 | 256 bit         | Google 推广，移动端性能优于 AES   |

**AES 常见模式：**

- `ECB`：最简单但最不安全（相同明文产生相同密文，有规律）
- `CBC`：引入初始化向量（IV），安全性好，是主流选择
- `GCM`：在 CBC 基础上加认证，同时提供加密和完整性校验，HTTPS/TLS 常用

### 非对称加密

**公钥加密，私钥解密**（或私钥签名，公钥验签）。

| 算法                | 特点                                                             |
| ------------------- | ---------------------------------------------------------------- |
| **RSA**             | 最经典，密钥长度通常 2048/4096 bit，速度慢                       |
| **ECC（椭圆曲线）** | 更短的密钥达到同等安全性，性能更好（256 bit ECC ≈ 3072 bit RSA） |
| DSA                 | 专门用于数字签名，不用于加密                                     |
| ECDSA               | ECC + DSA，以太坊区块链用这个                                    |

### 哈希算法（不是加密）

| 算法            | 输出长度 | 状态                                   |
| --------------- | -------- | -------------------------------------- |
| MD5             | 128 bit  | 已有碰撞，不推荐用于安全场景           |
| SHA-1           | 160 bit  | 有碰撞风险，不推荐                     |
| **SHA-256**     | 256 bit  | 主流，安全                             |
| SHA-512         | 512 bit  | 更强，用于高安全场景                   |
| bcrypt / argon2 | 可变     | 专门用于密码存储，有内置防暴力破解机制 |

### 面试常问：HTTPS 用的什么加密？

HTTPS（TLS）是**混合加密**：

1. **握手阶段**：用非对称加密（RSA 或 ECDHE）协商出一个对称密钥
2. **传输阶段**：用对称加密（AES-GCM）加密实际数据

原因：非对称加密安全但慢，对称加密快但密钥分发难，两者结合取长补短。

---

## 五、Functional Options 设计模式

### 是什么？

Go 语言中的经典配置模式，用**函数**代替参数列表来传配置项。

### 对比演示

```go
// 方式一：普通参数（参数爆炸，后期难以扩展）
// 新增一个参数 → 所有调用方都要修改 → 破坏性变更
func NewClient(endpoint string, timeout int, retryCount int, enableLog bool) *Client

// 方式二：Config 结构体（稍好，但调用方必须了解所有字段）
func NewClient(cfg Config) *Client

// 方式三：Functional Options（最优雅）
func NewClient(opts ...Option) *Client
```

### 完整实现示例

```go
type Client struct {
    endpoint   string
    timeout    time.Duration
    retryCount int
    enableLog  bool
}

// Option 本质是一个"修改 Client 的函数"
type Option func(*Client)

// 每个 WithXxx 返回一个 Option
func WithEndpoint(url string) Option {
    return func(c *Client) {
        c.endpoint = url
    }
}

func WithTimeout(d time.Duration) Option {
    return func(c *Client) {
        c.timeout = d
    }
}

func WithRetry(count int) Option {
    return func(c *Client) {
        c.retryCount = count
    }
}

func WithLog() Option {
    return func(c *Client) {
        c.enableLog = true
    }
}

// 构造函数：先设默认值，再依次应用 Option
func NewClient(opts ...Option) *Client {
    c := &Client{
        endpoint:   "http://default-incentive-service.com",
        timeout:    5 * time.Second,
        retryCount: 3,
        enableLog:  false,
    }
    for _, opt := range opts {
        opt(c)
    }
    return c
}

// 调用方：只传自己关心的参数
client := NewClient(
    WithEndpoint("http://incentive-service.prod.com"),
    WithTimeout(10 * time.Second),
    WithLog(),
)
```

### 为什么说提升了可扩展性？

**开闭原则（Open/Closed Principle）：对扩展开放，对修改关闭。**

- 新增 `WithProxy()` 配置项 → 只需新增一个函数，不改 `NewClient` 签名
- 已有调用方代码**零改动**，不受影响
- 对比普通参数：加一个参数 = 所有调用方都要改 = 牵一发动全身

### 延伸：其他语言的类似模式

| 语言       | 类似方案                                                                        |
| ---------- | ------------------------------------------------------------------------------- |
| Java       | Builder 模式（`Client.builder().endpoint(...).timeout(...).build()`）           |
| Python     | `**kwargs` + 默认参数                                                           |
| JavaScript | 解构赋值默认参数 `function init({ endpoint = 'default', timeout = 5000 } = {})` |

---

## 六、Endpoint 与灵活配置

### Endpoint 是什么？

服务的**访问入口地址**，通常是一个 URL：

```go
http://incentive-service.prod.company.com/api/v1   # 生产环境
http://incentive-service.test.company.com/api/v1   # 测试环境
http://127.0.0.1:8080/api/v1                        # 本地开发
```

### 为什么需要灵活配置？

你封装的 SDK 会被多个团队在多个环境使用：

```go
// 测试环境团队
client := NewClient(WithEndpoint("http://incentive-service.test.com"))

// 生产环境团队
client := NewClient(WithEndpoint("http://incentive-service.prod.com"))

// 私有化部署（比如给政务客户）
client := NewClient(WithEndpoint("http://10.0.1.100:8080"))
```

如果 Endpoint **写死（hardcode）** 在 SDK 代码里，其他人就没法用，SDK 就失去了复用价值。

### 延伸：实际工程中 Endpoint 怎么管理？

1. **环境变量**：`INCENTIVE_ENDPOINT=http://xxx.com` ，程序启动时读取
2. **配置中心**（如 Apollo、Nacos）：动态下发，不重启服务即可修改
3. **服务发现**（如 Consul、etcd）：微服务场景下自动找到服务地址，无需硬配置
4. **DNS 解析**：不同环境的域名解析到不同 IP，代码里只写域名

---

## 七、SDK 与可扩展性

### SDK 是什么？

**Software Development Kit，软件开发工具包。** 在这个场景里，SDK 是你封装的一个 Go package（库），让其他服务可以方便地调用激励服务，而不需要每次自己写 HTTP 请求、签名、错误处理等模板代码。

```go
// 没有 SDK，每个调用方都要自己写（重复劳动，且容易出错）：
body, _ := json.Marshal(req)
sign := sha256(timestamp + userID + string(body) + secretKey)
httpReq, _ := http.NewRequest("POST", "http://incentive.com/blacklist/add", bytes.NewReader(body))
httpReq.Header.Set("X-Sign", sign)
httpReq.Header.Set("X-Timestamp", timestamp)
resp, err := http.DefaultClient.Do(httpReq)
// ... 处理响应，处理错误，重试逻辑 ...

// 有了 SDK，调用方只需要：
err := client.AddToBlacklist(ctx, userID)
```

### SDK 设计的核心原则

1. **接口语义化**：方法名要表达业务含义，而非 HTTP 细节（`AddToBlacklist` 而非 `PostV1BlacklistAdd`）
2. **隐藏复杂性**：签名、重试、超时、错误码转换都在 SDK 内部处理
3. **调用方友好**：尽量减少调用方需要了解的概念
4. **可观测性**：SDK 内部要有日志、metrics，方便排查问题

### 什么是可扩展性？

> 系统在需求变化时，新增功能的成本低、改动范围小、不破坏现有功能。

通过 Functional Options 实现可扩展性的体现：

- 新增 `WithVersion("v2")` → 只加一个函数，所有现有调用方无感知
- 新增 `WithCircuitBreaker()` 熔断配置 → 同上，零破坏
- 修改默认超时时间 → 改构造函数里的默认值，调用方不需要改代码

---

## 八、异步 Goroutine 记录操作日志

### 为什么不能同步写？

```go
【同步写日志的问题】
用户请求 → 执行黑名单操作（快）→ 写审计日志到 DB（慢，可能超时）→ 返回结果
                                              ↑
                              日志写入失败 → 整个业务请求失败（不合理！）
                              日志写入慢  → 用户等待时间增加（体验差！）
```

### 异步 Goroutine 的方案

```go
func (s *BlacklistService) AddToBlacklist(ctx context.Context, userID string) error {
    // 主流程：执行业务操作
    err := s.repo.AddBlacklist(ctx, userID)
    if err != nil {
        return err
    }

    // 异步记录操作日志，不阻塞主流程
    go func() {
        // 注意：这里不能用外层的 ctx，因为请求结束后 ctx 会被取消
        logCtx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
        defer cancel()

        s.auditLog.Record(logCtx, AuditLog{
            Operator:  operatorFromCtx(ctx),
            Action:    "ADD_BLACKLIST",
            TargetID:  userID,
            CreatedAt: time.Now(),
        })
    }()

    return nil
}
```

### 关键细节：为什么要新建 Context？

这是一个常见的坑：

- 请求的 `ctx` 在请求返回后会被框架**取消（Cancel）**
- 如果 goroutine 里用原来的 `ctx`，请求一返回，日志写入就会立刻收到取消信号，日志可能写不进去
- 正确做法：在 goroutine 里用 `context.Background()` 新建一个独立的超时 Context

### 延伸：异步日志的可靠性问题

纯 Goroutine 异步有一个问题：**程序崩溃或重启时，Goroutine 里没写完的日志会丢失。**

更可靠的方案：

| 方案                                      | 可靠性               | 复杂度 |
| ----------------------------------------- | -------------------- | ------ |
| 直接 `go func()`                          | 低（进程崩溃丢失）   | 低     |
| 带 recover 的 goroutine + 重试            | 中                   | 中     |
| 写入本地消息队列（channel + worker pool） | 中高                 | 中     |
| 写入 Kafka/RocketMQ 等消息队列            | 高（持久化，可重试） | 高     |

**面试时可以说的亮点：**
> "当前用异步 Goroutine 实现，能满足审计日志的需求。如果对日志可靠性有更高要求，可以引入消息队列（如 Kafka）做异步削峰和持久化，确保日志不丢失。"

### 延伸：Goroutine 的注意事项

1. **防止 Goroutine 泄漏**：goroutine 如果没有退出条件，会一直占用内存

   ```go
   // 错误：没有超时控制，可能永远阻塞
   go func() {
       s.auditLog.Record(context.Background(), log) // 如果 DB 挂了，永远阻塞
   }()

   // 正确：设置超时
   go func() {
       ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
       defer cancel()
       s.auditLog.Record(ctx, log)
   }()
   ```

2. **panic 隔离**：goroutine 里的 panic 不会被主 goroutine 捕获，会直接崩溃整个程序

   ```go
   go func() {
       defer func() {
           if r := recover(); r != nil {
               log.Errorf("audit log goroutine panic: %v", r)
           }
       }()
       s.auditLog.Record(ctx, log)
   }()
   ```

3. **并发量控制**：大量请求同时来，goroutine 数量可能爆炸，用 **Worker Pool** 限制并发数

---

## 九、该模块涉及的设计模式总结

| 设计模式               | 应用场景 | 解决问题                                  |
| ---------------------- | -------- | ----------------------------------------- |
| **Functional Options** | SDK 配置 | 可扩展的可选参数，避免破坏性变更          |
| **Facade 外观模式**    | 封装 SDK | 对外提供简洁接口，隐藏内部复杂性          |
| **策略模式**           | 签名算法 | 可以替换不同的签名算法实现（SHA256/HMAC） |
| **观察者/异步事件**    | 操作日志 | 解耦业务逻辑和审计日志记录                |

---

## 十、可能的面试追问与参考回答

**Q：黑名单存在哪里？Redis 还是 DB？**
> 一般两层：DB 持久化存储，Redis 缓存加速读取。黑名单变更时同步更新 Redis，查询时先走 Redis，Redis miss 再查 DB 并回写缓存。

**Q：黑名单变更后，如何让调用方立刻生效？**
> 可以通过消息队列（Kafka/MQ）发布黑名单变更事件，各下游服务订阅后更新本地缓存，实现准实时生效。

**Q：签名里的 secret_key 怎么管理？**
> 不能写在代码里，应该放在配置中心或密钥管理系统（KMS）里，线上环境通过环境变量或运行时注入，定期轮换。

**Q：如果黑名单接口被高并发请求打爆怎么办？**

> 1. Redis 缓存扛读请求；2. 写操作异步化，用消息队列削峰；3. 接口限流（令牌桶/漏桶算法）；4. 熔断降级（如 Hystrix/sentinel）。

**Q：Goroutine 会不会太多，资源耗尽？**
> 每个请求起一个 Goroutine 写日志，在高并发时确实有风险。更优的做法是实现一个 Worker Pool（固定数量的消费者 Goroutine），日志任务投入 channel，由 Worker Pool 消费，控制并发度。
