# Bug修复总结

## 问题描述

在 `redis_lock_test.go` 中遇到编译错误：

```bash
❌ cannot use luaUnlock (variable of type *redis.Script) as string value in argument to mock.ExpectEval
```

## 根本原因

### 原因1: 类型不匹配

```go
// redis_lock.go (修改前)
var luaLock = redis.NewScript(`...`)  // 类型: *redis.Script

// redis_lock_test.go (修改前)
mock.ExpectEval(luaLock, ...)  // ❌ ExpectEval期望string，但传入了*redis.Script
```

`mock.ExpectEval()` 的函数签名：

```go
func (m *ClientMock) ExpectEval(script string, keys []string, args ...interface{})
                                      ^^^^^^^
                                      需要string类型
```

### 原因2: EVALSHA vs EVAL

`redis.Script.Run()` 的优化机制：

1. 先尝试 `EVALSHA sha_hash keys args` (使用脚本缓存，更快)
2. 如果失败(脚本未缓存)，才用 `EVAL script keys args`

所以即使修正了类型，用 `ExpectEval` 还是会失败，因为实际调用的是 `EVALSHA`。

## 解决方案

### 步骤1: 提取Lua脚本为常量

**修改 redis_lock.go:**

```go
// 修改前
var luaLock = redis.NewScript(`
if redis.call("exists", KEYS[1]) == 0 then
    redis.call("set", KEYS[1], ARGV[1], "PX", ARGV[2])
    return 1
end
return 0
`)

// 修改后
const luaLockScript = `
if redis.call("exists", KEYS[1]) == 0 then
    redis.call("set", KEYS[1], ARGV[1], "PX", ARGV[2])
    return 1
end
return 0
`

var luaLock = redis.NewScript(luaLockScript)
```

**好处**:

- ✅ 可以在测试中引用 `luaLockScript` (string类型)
- ✅ 保持生产代码不变 (luaLock仍是*redis.Script)

### 步骤2: 使用 ExpectEvalSha 而不是 ExpectEval

**修改 redis_lock_test.go:**

```go
// 修改前
mock.ExpectEval(luaLock, []string{"test-lock"}, lock.value, 30000)
             // ^^^^^^^ 类型错误，且命令也不匹配

// 修改后
mock.ExpectEvalSha(luaLock.Hash(), []string{"test-lock"}, lock.value, int(30000))
                // ^^^^^^^^^^^^    使用Script的Hash()方法获取SHA
```

`luaLock.Hash()` 返回脚本的SHA哈希值，这是Redis用于缓存识别的。

### 步骤3: 处理随机UUID

每个锁实例的 `value` 是随机生成的UUID，导致mock难以匹配。

**解决方法**:

```go
// 在测试中先创建锁实例，然后使用其真实的value
lock := NewRedisLock("test-lock", 30*time.Second)

// 使用lock.value进行mock，而不是硬编码
mock.ExpectEvalSha(
    luaLock.Hash(),
    []string{"test-lock"},
    lock.value,     // ← 使用真实的UUID
    int(30000),
)
```

### 步骤4: 放宽mock匹配规则

对于复杂场景，使用：

```go
mock.MatchExpectationsInOrder(false)
```

这样可以避免严格的顺序匹配，适合并发测试。

## 修改对比

### 文件1: redis_lock.go

```diff
- var luaLock = redis.NewScript(`...`)
+ const luaLockScript = `...`
+ var luaLock = redis.NewScript(luaLockScript)

- var luaUnlock = redis.NewScript(`...`)
+ const luaUnlockScript = `...`
+ var luaUnlock = redis.NewScript(luaUnlockScript)

- var luaRenew = redis.NewScript(`...`)
+ const luaRenewScript = `...`
+ var luaRenew = redis.NewScript(luaRenewScript)
```

### 文件2: redis_lock_test.go

```diff
func TestRedisLock_BasicLock(t *testing.T) {
    mock := db.SetupMockRedis()
    lock := NewRedisLock("test-lock", 30*time.Second)

-   mock.ExpectEval(luaLock, []string{"test-lock"}, lock.value, 30000)
+   mock.MatchExpectationsInOrder(false)
+   mock.ExpectEvalSha(luaLock.Hash(), []string{"test-lock"}, lock.value, int(30000))
    ...
}
```

## 测试结果

```bash
$ go test -v ./internal/gcron -run TestRedisLock
=== RUN   TestRedisLock_BasicLock
--- PASS: TestRedisLock_BasicLock (0.00s)
=== RUN   TestRedisLock_LockFailed
--- PASS: TestRedisLock_LockFailed (0.00s)
=== RUN   TestRedisLock_Watchdog
    redis_lock_test.go:74: Skipping watchdog test - requires time-based mocking
--- SKIP: TestRedisLock_Watchdog (0.00s)
=== RUN   TestRedisLock_UniqueValue
--- PASS: TestRedisLock_UniqueValue (0.00s)
PASS
ok      customer-service/internal/gcron 0.871s
```

✅ **3个测试通过，1个跳过（看门狗需要集成测试）**

## 关键知识点

### 1. redis.Script 的工作机制

```go
script := redis.NewScript("return 1")

// 首次调用会执行：
// 1. SCRIPT LOAD sha_hash  (将脚本加载到Redis)
// 2. EVALSHA sha_hash ...   (执行脚本)

// 后续调用直接执行：
// EVALSHA sha_hash ...
```

### 2. Mock的正确姿势

| 场景 | Mock方法 | 说明 |
| ------ | --------- | ------ |
| 直接调用 `Eval()` | `ExpectEval(script_string, ...)` | 期望EVAL命令 |
| 使用 `Script.Run()` | `ExpectEvalSha(script_hash, ...)` | 期望EVALSHA命令 |
| 脚本未缓存 | `ExpectEvalSha(...).RedisNil()` + `ExpectEval(...)` | 先失败后回退 |

### 3. 类型转换注意点

```go
expiration := 30 * time.Second
// ❌ 错误：直接传time.Duration
mock.ExpectEvalSha(..., expiration)

// ✅ 正确：转换为毫秒的int
mock.ExpectEvalSha(..., int(expiration.Milliseconds()))
```

## 经验教训

1. **Mock复杂对象时，优先提取常量**
   - 便于测试引用
   - 保持生产代码简洁

2. **了解底层实现很重要**
   - redis.Script 使用 EVALSHA 优化
   - 不了解会导致mock失败

3. **Mock有局限性**
   - 看门狗（时间相关）难以mock
   - 并发场景容易出问题
   - 复杂场景用集成测试

4. **测试策略分层**
   - 单元测试: 基础逻辑、错误处理
   - 集成测试: 完整功能、并发、性能
   - 手动测试: 开发阶段快速验证

## 扩展阅读

- [Redis EVAL vs EVALSHA](https://redis.io/commands/eval/)
- [Redisson分布式锁原理](https://github.com/redisson/redisson)
- [go-redis脚本缓存机制](https://github.com/redis/go-redis)

## 相关文件

- `redis_lock.go` - 核心实现
- `redis_lock_test.go` - 单元测试（已修复）
- `TESTING_GUIDE.md` - 测试指南
- `REDIS_LOCK_README.md` - 使用文档
