# Redis分布式锁实现 - Redisson风格

## 📚 核心概念

### 什么是Redisson？

**Redisson** 是一个Java的Redis客户端库，提供了比原生Redis客户端更强大的分布式锁实现。

### 为什么Redisson比RedLock好？

| 特性 | SETNX简单锁 | RedLock | Redisson锁 |
| ------ | ------------ | --------- | ----------- |
| **性能** | ⭐⭐⭐⭐⭐ 最快 | ⭐⭐ 很慢（需要多个Redis） | ⭐⭐⭐⭐ 快 |
| **可靠性** | ⭐⭐ 主从复制有风险 | ⭐⭐⭐⭐ 较可靠 | ⭐⭐⭐⭐ 可靠 |
| **自动续期** | ❌ 需手动实现 | ❌ 需手动实现 | ✅ WatchDog机制 |
| **原子性** | ⚠️ 可能有竞态 | ✅ Lua脚本 | ✅ Lua脚本 |
| **防误删** | ❌ 容易误删 | ✅ 有防护 | ✅ UUID防护 |
| **部署成本** | 单Redis | 需要5个独立Redis | 单Redis即可 |

**结论**：Redisson在单Redis环境下是最佳选择，性能好、可靠性高、使用简单。

---

## 🔍 看门狗（WatchDog）机制详解

### 原理图

```go
时间轴：
T0s    T10s   T20s   T30s   T40s   T50s   T60s
│      │      │      │      │      │      │
├──────┼──────┼──────┼──────┼──────┼──────┤
│ Lock │      │      │      │      │      │ Unlock
│ TTL=30s    │      │      │      │      │
│      │     │      │      │      │      │
│      └─Renew(续期30s)   │      │      │
│             │      │      │      │      │
│             └─────Renew(续期30s) │      │
│                    │      │      │      │
│                    └─────Renew(续期30s) │
│                           │      │      │
│                           └─────Renew─  │
                                     业务完成，主动释放
```

### 核心逻辑

```go
// 1. 加锁时设置TTL=30秒
Lock(TTL=30s)

// 2. 启动看门狗goroutine
go watchdog() {
    ticker := time.NewTicker(30/3秒) // 每10秒
    for {
        select {
        case <-ticker.C:
            Renew(TTL=30s) // 续期到30秒
        case <-stop:
            return // 解锁时停止
        }
    }
}

// 3. 业务逻辑执行（即使超过30秒也不会掉锁）
doLongRunningTask()

// 4. 解锁时停止看门狗
Unlock()
close(stop)
```

### 为什么是 1/3 时间？

- **太频繁**（如1/10）：网络开销大，Redis压力大
- **太稀疏**（如1/2）：网络抖动可能导致锁过期
- **1/3是甜蜜点**：
  - 第一次续期在10秒（有20秒buffer）
  - 第二次续期在20秒（有10秒buffer）
  - 即使一次续期失败，还有时间补救

---

## 🛠️ 使用指南

### 场景1：定时任务防重复执行

```go
// 原有代码（lock.go）
func MyCronJob() {
    if !LOCK {
        return // 不是master，跳过
    }
    doJob()
}

// 新方案（使用Redisson风格锁）
func MyCronJob() {
    ctx := context.Background()

    err := WithLock(ctx, "my-cron-job", 30*time.Second, func() error {
        return doJob()
    })

    if err == ErrLockFailed {
        // 其他实例正在执行，跳过
        return
    }
    if err != nil {
        glog.Error(ctx, "job failed:", err)
    }
}
```

### 场景2：手动控制锁

```go
lock := NewRedisLock("my-lock", 30*time.Second)

// 尝试获取锁，最多等待10秒
err := lock.TryLock(10*time.Second, 100*time.Millisecond)
if err != nil {
    return err
}
defer lock.Unlock()

// 执行业务逻辑
// 看门狗会自动续期，不用担心超时
doLongTask() // 即使执行60秒也没问题
```

### 场景3：长时间任务

```go
lock := NewRedisLock("long-task", 10*time.Second)
lock.Lock()
defer lock.Unlock()

// 模拟60秒的任务，锁只有10秒TTL
for i := 0; i < 60; i++ {
    time.Sleep(1 * time.Second)
    // 看门狗每3.3秒自动续期一次
}
// 任务完成，看门狗自动停止
```

---

## ⚙️ 实现细节

### Lua脚本保证原子性

#### 加锁脚本

```lua
-- KEYS[1] = 锁的key
-- ARGV[1] = 锁的value（UUID）
-- ARGV[2] = 过期时间（毫秒）
if redis.call("exists", KEYS[1]) == 0 then
    redis.call("set", KEYS[1], ARGV[1], "PX", ARGV[2])
    return 1
end
return 0
```

**为什么要Lua？**

- ❌ 错误做法：`GET -> 判断 -> SET`（三步，有竞态）
- ✅ 正确做法：Lua脚本原子执行（一步完成）

#### 解锁脚本

```lua
-- KEYS[1] = 锁的key
-- ARGV[1] = 锁的value（UUID）
if redis.call("get", KEYS[1]) == ARGV[1] then
    return redis.call("del", KEYS[1])
end
return 0
```

**防误删**：只有value匹配才能删除（防止删除别人的锁）

#### 续期脚本

```lua
-- KEYS[1] = 锁的key
-- ARGV[1] = 锁的value（UUID）
-- ARGV[2] = 新的过期时间（毫秒）
if redis.call("get", KEYS[1]) == ARGV[1] then
    return redis.call("pexpire", KEYS[1], ARGV[2])
end
return 0
```

---

## 🆚 原lock.go的问题

### 原代码的竞态条件

```go
// lock.go:62-78
val := db.Rdb().Get(ctx, RedisLockKey).Val()  // ← 第一步
if val == version {
    err = db.Rdb().Expire(ctx, RedisLockKey, time.Second*5).Err()  // ← 第二步
} else {
    vali := util.GenVersionNumber(val)
    vi := util.GenVersionNumber(version)
    if vi > vali {
        err = db.Rdb().Set(ctx, RedisLockKey, version, time.Second*5).Err()  // ← 第三步
    }
}
```

**问题**：

1. **GET和SET不是原子操作**，中间可能被其他实例抢占
2. **force模式的Del+Set**（行52-58）也不是原子操作
3. **没有防误删**，可能删除其他版本的锁

### 新实现的优势

```go
// redis_lock.go: 全部用Lua脚本
result, err := luaLock.Run(ctx, client, []string{key}, value, expiration).Int()
```

**优势**：

1. ✅ 单个Lua脚本，原子执行
2. ✅ UUID防误删
3. ✅ 看门狗自动续期
4. ✅ 代码更简洁

---

## 🚨 常见问题

### Q1: 看门狗goroutine会泄漏吗？

**答**：不会。解锁时会 `close(watchdogStop)`，goroutine会退出。

### Q2: Redis挂了怎么办？

**答**：

- 单机Redis挂了：所有锁失效，需要Redis高可用方案（哨兵/集群）
- 主从切换：可能有短暂的锁丢失（见前面的主从异步复制问题）
- 如果要求极高可靠性：用etcd/Zookeeper

### Q3: 为什么不用RedLock？

**答**：

- RedLock需要5个独立Redis（成本高）
- 网络延迟影响性能
- 依赖时钟同步（NTP漂移可能出问题）
- Redisson单机方案已经足够可靠

### Q4: 续期失败了怎么办？

**答**：看门狗检测到续期失败会自动停止，业务代码会在下次操作时发现锁丢失。

---

## 📝 迁移建议

### 方案A：渐进式迁移

```go
// 保留原lock.go的LOCK变量，但改用新锁
func Setup() {
    version := util.Version()
    lock := NewRedisLock(RedisLockKey, 5*time.Second)

    ticker := time.NewTicker(time.Second)
    go func() {
        for {
            select {
            case <-ticker.C:
                if shouldHold := checkVersion(version); shouldHold {
                    if !lock.IsLocked() {
                        lock.Lock()
                    }
                } else {
                    if lock.IsLocked() {
                        lock.Unlock()
                    }
                }
                LOCK = lock.IsLocked()
            }
        }
    }()
}
```

### 方案B：直接替换

```go
// 在每个定时任务中直接使用
func CronJob() {
    WithLock(context.Background(), "job-name", 30*time.Second, func() error {
        return doJob()
    })
}
```

---

## 🎯 总结

| 特性 | 原lock.go | 新redis_lock.go |
| ------ | ----------- | ---------------- |
| 原子性 | ❌ 多步操作 | ✅ Lua脚本 |
| 防误删 | ❌ 无防护 | ✅ UUID防护 |
| 自动续期 | ⚠️ 手动ticker | ✅ WatchDog |
| 长时间任务 | ❌ 可能超时 | ✅ 自动续期 |
| 代码复杂度 | ⚠️ 较复杂 | ✅ 简洁 |

**推荐**：新项目直接用 `redis_lock.go`，老项目可以渐进式迁移。
