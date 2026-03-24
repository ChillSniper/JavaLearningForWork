# 简历第四点：CRM 流失召回功能——架构优化深度梳理

> 涉及代码路径（customer-service-master）：
>
> - `internal/service/vip_scrm/churn_job.go` — Pipeline 加锁/释放锁核心逻辑
> - `internal/dao/dao_vip_scrm/vip_scrm_user_tag_dao.go` — GetHiddenTagUIDs 实现
> - `internal/dao/dao_vip_scrm/vip_scrm_churn_dao.go` — WithExcludeUIDs 查询选项
> - `internal/service/vip_scrm/churn_core.go` — 两者的调用现场

---

## 一、Redis Pipeline + Lua 脚本：分布式锁机制

### 1.1 背景：为什么需要分布式锁

流失判定是**定时任务（每天 9:30）+ 并发 goroutine**的架构：

- 多个 goroutine 同时处理不同 projectID。
- 每个项目会分批（100 条/批）扫描待判定用户。
- 极端情况下（重跑任务、多实例部署）同一用户可能被两个 goroutine 同时拿到并处理，造成重复流失记录或数据竞争。

**分布式锁的作用**：保证同一用户在同一时刻只被一个 goroutine 处理，即**幂等性**。

---

### 1.2 Redis Pipeline 是什么

Redis Pipeline（管道）是一种**客户端批量发送命令**的技术。

**普通模式**：每条命令 = 1 次网络往返（RTT）：

```go
Client → [SET key1] → Server
Client ← [OK]        ← Server
Client → [SET key2] → Server
Client ← [OK]        ← Server
... （N条命令 = N次RTT）
```

**Pipeline 模式**：N 条命令打包一次性发送，只需 1 次网络往返：

```go
Client → [SET key1, SET key2, SET key3, ...N条] → Server
Client ←         [OK, OK, OK, ...N个响应]        ← Server
```

**性能收益**：100 个用户加锁，从 100 次 RTT 降为 1 次 RTT，在网络延迟 1ms 的场景下节省约 100ms。

**代码实现**（churn_job.go:71-93）：

```go
pipe := db.Rdb().Pipeline()
lockCmds := make([]*redis.BoolCmd, len(users))
for i, u := range users {
    lockCmds[i] = pipe.SetNX(ctx,
        fmt.Sprintf(rdb.ScrmChurnUserLockKey, projectID, userUIDs[i]),
        lockToken,       // value = 随机 token，防止误删
        15*time.Minute,  // TTL 兜底，防止死锁
    )
}
pipe.Exec(ctx)  // 一次性发送所有 SetNX
```

**SetNX（SET if Not eXists）**：键不存在时才设置，返回 true 表示加锁成功；键已存在则返回 false，表示该用户已被其他实例锁定，跳过处理。这就是**乐观锁**的核心语义。

**注意**：Pipeline 不保证原子性，多条命令可能被其他客户端命令穿插（但对于 SetNX 加锁场景，每条命令本身是原子的，整体并发安全）。

---

### 1.3 为什么释放锁要用 Lua 脚本

**问题：直接 DEL 会误删别人的锁**：

考虑如下时序：

```go
实例A 加锁 key=user:123，value=tokenA，TTL=15min
实例A 处理耗时过长，TTL 到期，锁自动过期
实例B 加锁 key=user:123，value=tokenB，加锁成功
实例A 处理完成，执行 DEL key=user:123 ← 误删了实例B的锁！
实例C 此时也能加锁，并发产生
```

**解决方案**：释放锁时必须先校验 value 是否是自己的 token，"检查 + 删除"必须是**原子操作**。

**为什么需要原子性**：

```go
// 非原子的伪代码（有问题）：
if GET(key) == myToken {   // 检查
    // 在这个空隙，锁可能过期，被别人重新加上
    DEL(key)               // 删除 → 误删了别人的锁
}
```

**Lua 脚本保证原子性**：Redis 单线程执行 Lua 脚本时，脚本中所有命令不可被打断。

```lua
-- churn_job.go:22
if redis.call("GET", KEYS[1]) == ARGV[1] then
    return redis.call("DEL", KEYS[1])
else
    return 0
end
```

执行时：

```go
// churn_job.go:102
delPipe.Eval(ctx, churnUserUnlockLua,
    []string{fmt.Sprintf(rdb.ScrmChurnUserLockKey, projectID, uid)},
    lockToken,  // ARGV[1] = 加锁时存入的 token
)
```

释放锁也用 Pipeline 批量发送：100 个用户的 Eval 命令打包一次性发送，同样节省网络往返。

---

### 1.4 Lua 脚本在 Redis 中的底层原理

| 特性              | 原理                                                                                                     |
| ----------------- | -------------------------------------------------------------------------------------------------------- |
| **原子性**        | Redis 是单线程事件循环，执行 Lua 脚本期间不处理其他命令，脚本整体作为一个不可分割的操作执行              |
| **执行引擎**      | Redis 内嵌了 LuaJIT（或 Lua 5.1），通过 `luaL_newstate()` 初始化 Lua 环境                                |
| **redis.call()**  | Lua 调用此函数时，会直接调用 Redis 内部命令处理函数，就像本地函数调用，无网络开销                        |
| **KEYS / ARGV**   | 参数通过 Lua 全局变量传入，KEYS 是键列表，ARGV 是参数列表。Redis Cluster 模式下要求所有 KEYS 在同一 slot |
| **脚本缓存**      | EVAL 每次发送脚本体；EVALSHA 发送脚本 SHA1，Redis 从缓存中取脚本执行，节省带宽                           |
| **无副作用约束**  | Lua 脚本不能执行耗时操作（sleep、I/O），否则会阻塞整个 Redis 实例                                        |
| **vs MULTI/EXEC** | MULTI/EXEC 事务不能根据中间结果做条件判断（WATCH 除外）；Lua 可以写 if/else，逻辑更灵活                  |

**与 MULTI/EXEC 的本质区别**：

- MULTI/EXEC：乐观锁，提交时发现 WATCH 的键被修改则整个事务回滚
- Lua 脚本：悲观执行，整个脚本运行期间 Redis 不响应其他命令，适合"读-判断-写"三步必须原子的场景

---

## 二、GetHiddenTagUIDs + WithExcludeUIDs：标签系统过滤链路

### 2.1 业务背景：从"免打扰"到"标签系统"

**旧设计**：流失列表有一个硬编码的"免打扰"标志位（`is_disturbfree` 字段），用于屏蔽不想被联系的用户。

**问题**：只有一种过滤维度，无法扩展（比如：高价值用户不在流失列表展示、异常账号过滤等）。

**新设计**：抽象出通用的"标签系统"：

- `vip_scrm_tag_types` 表：定义标签类型，每种标签有 `is_hidden`（是否从流失列表隐藏）、`count_in_churn_rate`（是否计入流失率）等属性
- `vip_scrm_user_tags` 表：记录每个用户被打了哪些标签
- 只要给用户打上 `is_hidden=true` 类型的标签，该用户就会自动从流失列表消失

---

### 2.2 GetHiddenTagUIDs 实现解析

```go
// vip_scrm_user_tag_dao.go:91
func (d *userTagDao) GetHiddenTagUIDs(ctx context.Context,
    projectID, cloudID, gameID, appID string) ([]int64, error) {

    var uids []int64
    err := d.db.WithContext(ctx).
        Model(&model_vip_scrm.UserTag{}).
        // JOIN 标签类型表，获取标签的属性
        Joins("INNER JOIN vip_scrm_tag_types tt ON tt.project_id = ... AND tt.tag_type = vip_scrm_user_tags.tag_type").
        Where("vip_scrm_user_tags.project_id = ? AND ...", projectID, cloudID, gameID, appID).
        Where("tt.is_hidden = ?", true).   // 只取"隐藏"类型的标签
        Distinct("vip_scrm_user_tags.uid"). // 去重，同一用户可能有多个隐藏标签
        Pluck("vip_scrm_user_tags.uid", &uids).Error
    return uids, err
}
```

等价 SQL：

```sql
SELECT DISTINCT vut.uid
FROM vip_scrm_user_tags vut
INNER JOIN vip_scrm_tag_types tt
    ON tt.project_id = vut.project_id AND ... AND tt.tag_type = vut.tag_type
WHERE vut.project_id = ? AND vut.cloud_id = ? AND vut.game_id = ? AND vut.app_id = ?
  AND tt.is_hidden = true;
```

---

### 2.3 WithExcludeUIDs 实现解析

`WithExcludeUIDs` 是一个**函数式选项（Functional Options Pattern）**，用于构建查询条件：

```go
// vip_scrm_churn_dao.go:739
func WithExcludeUIDs(uids []int64) QueryOption {
    return func(o *queryOptions) {
        o.ExcludeUIDs = uids  // 注入到查询配置
    }
}

// 在查询构建阶段转义为 SQL：
if len(options.ExcludeUIDs) > 0 {
    uidSub = uidSub.Where("vip_scrm_churn_record.uid NOT IN ?", options.ExcludeUIDs)
}
```

**函数式选项模式的优势**：

- 调用方按需传入选项，不传则使用默认值，避免大量 nil 参数
- 新增过滤维度只需加一个 `WithXxx` 函数，不改接口签名，符合开闭原则
- 多个选项之间相互独立，可任意组合

---

### 2.4 完整调用链（churn_core.go）

```go
GetChurnList（查询流失列表接口）
  ├─ userTagDao.GetHiddenTagUIDs(...)     → []int64（隐藏用户的 UID 列表）
  ├─ 构建 options []QueryOption
  │    ├─ WithProjectIDs(...)
  │    ├─ WithExcludeUIDs(hiddenUIDs)    ← 将隐藏 UID 注入过滤条件
  │    └─ ...其他过滤条件
  └─ churnDao.QueryChurnRecords(ctx, options...)
       └─ SQL: ... WHERE uid NOT IN (隐藏UID列表) ...
```

**动态性体现**：标签是实时打的，每次查询流失列表都会重新查一遍隐藏标签，无需重新跑判定任务，过滤结果实时生效。

---

## 三、面试官最可能问的问题

### 3.1 Redis Pipeline 相关

**Q1：Pipeline 和普通命令有什么区别？为什么用 Pipeline？**
> 答：Pipeline 将多条命令批量发送，只需一次网络往返，显著降低 RTT 累积延迟。100 个用户加锁从 100 次 RTT 降为 1 次，在高并发下性能提升显著。

**Q2：Pipeline 是原子的吗？**
> 答：不是。Pipeline 只是批量发送，服务端依然逐条执行，中间可能被其他客户端命令插入。与 MULTI/EXEC 事务不同，Pipeline 不提供原子性保证。对本场景无影响，因为每条 SetNX 本身是原子的。

**Q3：SetNX 加锁的 TTL 设置为 15 分钟，这个值怎么评估的？**
> 答：TTL 是死锁兜底设计——如果实例宕机来不及释放锁，TTL 到期后锁自动释放，避免永久死锁。15 分钟 > 单批次处理预期耗时，保证正常情况下任务完成前锁不会提前过期。

**Q4：如果 Pipeline 执行失败（Redis 宕机等），你的代码如何处理的？**
> 答：代码有降级逻辑（churn_job.go:82）：`if pipeErr != nil { lockedUsers = users }`，即加锁失败时降级为处理全部用户，不因 Redis 故障导致流失判定完全停摆。代价是可能有短暂的并发重复处理，但流失记录最终由 DB 的 Upsert 保证幂等。

---

### 3.2 Lua 脚本相关

**Q5：为什么释放锁要用 Lua 脚本？直接 DEL 不行吗？**
> 答：直接 DEL 会有"误删他人锁"的问题。场景：实例A的锁 TTL 到期 → 实例B重新加锁 → 实例A执行 DEL，把实例B的锁删了。Lua 脚本将"GET 校验 token + DEL"合并为一个原子操作，保证只删自己的锁。

**Q6：Lua 脚本为什么能保证原子性？**
> 答：Redis 是单线程事件循环，执行 Lua 脚本期间不会处理任何其他命令，脚本整体不可被中断，从而保证原子性。这与 MULTI/EXEC 不同，MULTI/EXEC 无法在事务内做条件判断，而 Lua 可以写 if/else 逻辑。

**Q7：Lua 脚本有什么使用注意事项？**
> 答：① 不能执行耗时操作（sleep/I/O），会阻塞全局；② Redis Cluster 下所有 KEYS 必须在同一 slot；③ 生产环境建议用 EVALSHA + SCRIPT LOAD 缓存脚本，减少每次传输完整脚本体的带宽开销。

---

### 3.3 整体架构相关

**Q8：分布式锁 + DB Upsert，这两层幂等保障是否重复？**
> 答：不重复，各解决不同层面的问题。分布式锁在**内存计算阶段**防止并发，避免同一用户的流失判定逻辑被执行两次（节省计算资源、避免中间状态竞争）。DB Upsert 是**持久化阶段**的最终兜底，应对极端情况（锁降级、Redis 故障）下可能产生的重复写入。

**Q9：GetHiddenTagUIDs 每次查询都打一次 DB，有性能问题吗？**
> 答：当前是每次请求查一次。如果标签数量很大，可以加 Redis 缓存（短 TTL 如 5 分钟），标签变更时主动失效。现有设计是"正确性优先"，缓存是后续优化方向。

**Q10：为什么用函数式选项（Functional Options）而不是直接传结构体参数？**
> 答：查询条件组合方式多，不同调用方需要的过滤维度不同。函数式选项让每个选项独立、可选、可组合，新增过滤维度只加一个 WithXxx 函数，不会破坏已有调用方的代码（开闭原则）。

**Q11：整个流失判定的幂等性是如何保证的？**
> 答：三层保障：① `vipDao.QueryAndUpdateChurnCheckTime` 使用原子查询+更新，同一用户当天只被查出一次（DB 层）；② Redis SetNX 分布式锁，同一用户同一时刻只有一个实例处理（Redis 层）；③ DB Upsert，最终写入时按唯一键冲突更新而非插入，保证数据最终一致（持久化层）。
