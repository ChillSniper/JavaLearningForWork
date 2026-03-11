# Overview

## 1. 分表架构 + 函数选项模式

- 基于 ProjectID/GameID/AppID 的分表设计
- Functional Options Pattern（函数选项模式）
- SELECT FOR UPDATE SKIP LOCKED 乐观并发控制

### 函数选项模式 (Functional Options Pattern)

是什么：

- 一种 Go 语言中常用的设计模式，用于优雅地处理可选参数

核心代码示例：
// 定义选项函数类型
type AuditPunishmentDaoOption func(*AuditPunishmentDao) error

// 定义具体的选项函数
func WithPunishmentShard(key model.AuditPunishmentTableKey) AuditPunishmentDaoOption {
    return func(dao *AuditPunishmentDao) error {
        dao.table = model.AuditPunishment{}.ShardTableName(key)
        return nil
    }
}

// 使用选项模式
dao, err := NewAuditPunishmentDao(
    WithPunishmentShard(key),
    WithPunishmentDB(tx),
)

优点：

- 可扩展性强：添加新选项不影响已有代码
- 可读性好：每个选项的作用一目了然
- 灵活性高：可以任意组合选项

面试可能问到：

- 为什么使用函数选项模式而不是直接传参？
- 和Builder模式的区别是什么？

## 2. Redis ZSet + 分布式锁

- ZSet 实现定时黑名单解封
- ZRANGEBYSCORE 按时间范围查询
- SetNX 实现分布式锁

### Casbin RBAC 模型

是什么：

- Casbin 是一个权限管理库
- RBAC = Role-Based Access Control（基于角色的访问控制）

核心概念：
Subject (主体)：谁？ → User
Object (客体)：什么资源？ → API路径
Action (动作)：做什么？ → GET/POST/DELETE
Role (角色)：权限集合 → Admin/Editor/Viewer

模型文件示例 (model.conf)：

```conf

[request_definition]
r = sub, obj, act

[policy_definition]
p = sub, obj, act

[role_definition]
g = _, _

[policy_effect]
e = some(where (p.eft == allow))

[matchers]
m = g(r.sub, p.sub) && r.obj == p.obj && r.act == p.act

```

面试可能问到：

- RBAC 和 ABAC（基于属性的访问控制）的区别？
- Casbin 的 Enforcer 是如何工作的？
- 如何实现动态权限更新？

## 3. Casbin RBAC + JWT

- Casbin 框架 + RBAC 模型
- Casbin Adapter 持久化
- JWT Token 认证中间件

### JWT Token 认证

是什么：

- JSON Web Token，一种无状态的认证方案
- 结构：Header.Payload.Signature

工作流程：

1. 用户登录 → 服务器验证
2. 生成 JWT (包含用户信息)
3. 返回给客户端
4. 客户端每次请求携带 JWT (Header: Authorization: Bearer `token`)
5. 服务器验证 JWT 签名 → 提取用户信息

和 Casbin 的配合：
// 1. JWT 中间件解析 Token
payload, err := util2.ParseToken(token)

// 2. 设置用户信息到 Context
c.Set(constants.UserSub, permission.GetUserSub(payload.Username))

// 3. Casbin 进行权限校验
enforcer.Enforce(userSub, resource, action)

面试可能问到：

- JWT 和 Session 的区别？
- JWT 过期了怎么办？（Refresh Token）
- 如何防止 JWT 被盗用？

## 4. Apollo 配置中心

### Apollo 配置中心

是什么：

- 携程开源的分布式配置中心
- 支持配置的集中管理、实时推送、灰度发布

核心功能：

```go

// 1. 绑定配置
gconf.BindConfig(
    GlobalConf,
    gconf.WithAppID("10067-customer-service"),
    gconf.WithNamespaceName("cfg.yaml"),
    gconf.WithChangeListener(&GlobalConfChangeListener{}),
)

// 2. 监听配置变更
func (c *GlobalConfChangeListener) OnChange(changeEvent *storage.ChangeEvent) {
    // 配置已经被自动更新到 GlobalConf
    // 可以在这里做一些额外的处理
}
```

优势：

- 配置热更新：无需重启服务
- 版本管理：可回滚
- 权限控制：不同环境不同权限
- 灰度发布：逐步推送配置

面试可能问到：

- Apollo 的配置是如何实时推送到客户端的？（长轮询 HTTP Long Polling）
- 如果 Apollo 挂了，服务还能正常运行吗？（本地缓存）
- 配置变更后，如何保证所有实例都生效？

- 多 Namespace 配置绑定
- ChangeListener 配置热更新
- 无需重启服务即可生效

## 5. 通知系统 - 长轮询

- Long Polling 方案
- Redis 存储通知标记
- MySQL + Redis 结合降低数据库压力

### Redis ZSet 定时任务

核心代码：

```go

// 1. 添加定时任务（score 是时间戳）
db.Rdb().ZAdd(ctx, rdb.PayRiskBlackLimitKey, redis.Z{
    Score:  float64(解封时间戳),
    Member: "userId_projectKey",
})

// 2. 定时扫描到期任务
redisStringList, err := db.Rdb().ZRangeByScoreWithScores(ctx,
    rdb.PayRiskBlackLimitKey,
    &redis.ZRangeBy{
        Min:    "-inf",
        Max:    currentTime, // 当前时间
        Offset: 0,
        Count:  100,
    })

// 3. 分布式锁防止重复执行
ok := db.Rdb().SetNX(ctx, taskKey, 1, time.Second*5).Val()
if !ok {
    continue  // 其他实例已经在处理
}

```

为什么用 ZSet：

- 自动按时间排序
- 范围查询高效（O(log N)）
- 支持原子操作

面试可能问到：

- 为什么不用延时队列？
- SetNX 分布式锁的缺陷是什么？（主从切换可能丢失）
- 如何保证定时任务不被重复执行？

---

### SELECT FOR UPDATE SKIP LOCKED

是什么：

- 数据库悲观锁的一种优化手段
- 跳过已被锁定的行，避免等待

使用场景：

```go
// 批量认领任务，并发场景下避免冲突
tx := d.db.WithContext(ctx).Begin()

lockClause := clause.Locking{Strength: "UPDATE", Options: "SKIP LOCKED"}
tx.Table(d.table).
    Clauses(lockClause).
    Where("punishment_status = ?", constants.PunishmentStatusPending).
    Limit(limit).
    Find(&tasks)

// 更新状态为执行中
tx.Table(d.table).
    Where("id IN ?", ids).
    Update("punishment_status", constants.PunishmentStatusRunning)

tx.Commit()
```

对比：

- SELECT FOR UPDATE：等待锁释放
- SELECT FOR UPDATE NOWAIT：立即报错
- SELECT FOR UPDATE SKIP LOCKED：跳过被锁的行（最适合抢任务场景）

面试可能问到：

- 为什么不用乐观锁？
- 如何处理死锁问题？
