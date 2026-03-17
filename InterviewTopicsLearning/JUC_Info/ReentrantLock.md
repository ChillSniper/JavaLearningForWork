# ReentrantLock（可重入锁）

---

## 一、核心概念

`ReentrantLock` 是 `java.util.concurrent.locks` 包下的显式锁，基于 **AQS（AbstractQueuedSynchronizer）** 实现，是 `synchronized` 的功能增强替代品。

**"可重入"** 的含义：同一个线程可以多次 `lock()`，不会自己死锁。每次 `lock()` state+1，每次 `unlock()` state-1，归零时才真正释放。

---

## 二、ReentrantLock vs synchronized

| 维度           | ReentrantLock                 | synchronized        |
| -------------- | ----------------------------- | ------------------- |
| **实现层面**   | Java 代码（AQS）              | JVM 内置（Monitor） |
| **锁获取**     | 显式 `lock()` / `unlock()`    | 自动加锁释放        |
| **可中断**     | 支持（`lockInterruptibly()`） | 不支持              |
| **公平锁**     | 支持（构造参数指定）          | 不支持（非公平）    |
| **超时获取**   | 支持（`tryLock(timeout)`）    | 不支持              |
| **多条件变量** | 支持多个 `Condition`          | 只有一个等待队列    |
| **锁绑定条件** | 可绑定多个 Condition          | 一个 Monitor        |
| **性能**       | 高并发下略优                  | 低竞争下 JVM 优化好 |
| **死锁风险**   | 需手动 unlock，忘了会死锁     | 自动释放，更安全    |

**结论**：优先用 `synchronized`（简单安全），需要高级功能（可中断、公平锁、多条件）时用 `ReentrantLock`。

---

## 三、基本使用

```java
ReentrantLock lock = new ReentrantLock();

lock.lock();           // 加锁（阻塞直到获取）
try {
    // 临界区代码
} finally {
    lock.unlock();     // 必须在 finally 中释放，防止异常导致死锁
}
```

> **重点**：`unlock()` 必须放在 `finally` 块中，否则如果临界区抛异常，锁永远不会释放。

### 其他获取锁的方式

```java
// 1. 可中断获取（等待过程中可被 interrupt()）
lock.lockInterruptibly();

// 2. 尝试获取（立即返回，不阻塞）
boolean acquired = lock.tryLock();

// 3. 超时获取（等待最多 3 秒）
boolean acquired = lock.tryLock(3, TimeUnit.SECONDS);
```

---

## 四、公平锁 vs 非公平锁

```java
ReentrantLock fairLock    = new ReentrantLock(true);   // 公平锁
ReentrantLock unfairLock  = new ReentrantLock(false);  // 非公平锁（默认）
ReentrantLock defaultLock = new ReentrantLock();       // 非公平锁（默认）
```

### 公平锁（Fair）

- 严格按照 AQS 等待队列的 FIFO 顺序分配锁
- 每次 `tryAcquire` 先检查队列中是否有等待的线程，有则入队排队
- **优点**：无线程饥饿
- **缺点**：吞吐量低，线程切换频繁（每次都要挂起/唤醒）

### 非公平锁（Unfair，默认）

- 新来的线程先 CAS 抢一次，抢不到再入队
- **优点**：吞吐量高，减少线程切换（刚运行的线程大概率能再次抢到）
- **缺点**：可能造成线程饥饿（某个线程一直抢不到）

### 源码对比（核心差异）

```java
// 非公平锁 NonfairSync.tryAcquire
final boolean nonfairTryAcquire(int acquires) {
    Thread current = Thread.currentThread();
    int c = getState();
    if (c == 0) {
        if (compareAndSetState(0, acquires)) {   // 直接 CAS 抢，不管队列
            setExclusiveOwnerThread(current);
            return true;
        }
    }
    // ... 可重入逻辑
}

// 公平锁 FairSync.tryAcquire
protected final boolean tryAcquire(int acquires) {
    Thread current = Thread.currentThread();
    int c = getState();
    if (c == 0) {
        if (!hasQueuedPredecessors()             // 先检查队列有没有等待者
                && compareAndSetState(0, acquires)) {
            setExclusiveOwnerThread(current);
            return true;
        }
    }
    // ... 可重入逻辑
}
```

---

## 五、可重入原理

```java
// AQS Sync 中的可重入逻辑（公平/非公平共用）
if (current == getExclusiveOwnerThread()) {  // 判断是否是当前持有锁的线程
    int nextc = c + acquires;                // state + 1（重入计数）
    setState(nextc);
    return true;
}

// unlock 时
protected final boolean tryRelease(int releases) {
    int c = getState() - releases;           // state - 1
    if (c == 0) {                            // 归零才真正释放
        setExclusiveOwnerThread(null);
        setState(0);
        return true;
    }
    setState(c);
    return false;
}
```

**重入次数必须与释放次数匹配**，否则锁不会释放。

---

## 六、Condition（条件变量）

`Condition` 是 `ReentrantLock` 的等待/通知机制，相当于 `Object.wait()/notify()` 的增强版。

```java
ReentrantLock lock = new ReentrantLock();
Condition notFull  = lock.newCondition();  // 条件：未满
Condition notEmpty = lock.newCondition();  // 条件：非空

// 生产者
lock.lock();
try {
    while (队列满) notFull.await();    // 等待"未满"条件（释放锁并挂起）
    // 生产...
    notEmpty.signal();                 // 通知消费者"非空"了
} finally {
    lock.unlock();
}

// 消费者
lock.lock();
try {
    while (队列空) notEmpty.await();   // 等待"非空"条件
    // 消费...
    notFull.signal();
} finally {
    lock.unlock();
}
```

### Condition vs Object.wait/notify

| 维度         | Condition                           | Object.wait/notify  |
| ------------ | ----------------------------------- | ------------------- |
| **条件数量** | 一个锁可多个 Condition              | 只有一个等待队列    |
| **精准通知** | `signal()` 唤醒特定条件的线程       | `notify()` 随机唤醒 |
| **超时等待** | `await(time, unit)`                 | `wait(millis)`      |
| **可中断**   | `awaitUninterruptibly()` 不响应中断 | 必须响应中断        |

---

## 七、ReentrantLock 的 AQS 实现原理

### 内部结构

```java
ReentrantLock
 └── Sync（继承 AQS）
      ├── NonfairSync（非公平锁实现）
      └── FairSync（公平锁实现）
```

### 加锁流程（以非公平锁为例）

```java
lock()
  │
  ├─ CAS(state: 0→1) 成功 ──> 设置持有线程，获取锁
  │
  └─ 失败 → acquire(1)
               │
               ├─ tryAcquire() ──成功──> 获取锁（可重入 or 再次CAS）
               │
               └─ 失败 → addWaiter(Node.EXCLUSIVE) 入队
                           │
                           └─ acquireQueued() 自旋 + park() 挂起
```

### 释放锁流程

```java
unlock() → release(1)
              │
              └─ tryRelease(): state-1
                    │
                    ├─ state > 0 → 还在重入，不释放
                    │
                    └─ state == 0 → 真正释放，unpark(队首等待线程)
```

---

## 八、高频面试题汇总

**Q1：ReentrantLock 和 synchronized 的区别？**
> ReentrantLock 是 Java 代码层面基于 AQS 实现的显式锁，synchronized 是 JVM 内置的 Monitor 锁。ReentrantLock 额外支持：可中断获取（`lockInterruptibly`）、超时获取（`tryLock`）、公平锁、多个 Condition 条件变量。synchronized 更简洁安全，低竞争下 JVM 有偏向锁等优化。优先用 synchronized，需要高级功能时用 ReentrantLock。

**Q2：什么是可重入锁？为什么需要可重入？**
> 可重入指同一线程可以多次获取同一把锁而不死锁。AQS 的 state 记录重入次数，每次 lock state+1，每次 unlock state-1，归零时才真正释放。需要可重入是因为同一线程中可能存在递归调用或方法嵌套调用加锁的场景，没有可重入会死锁。

**Q3：公平锁和非公平锁的区别？默认是哪种？**
> 默认非公平锁。公平锁严格 FIFO，每次获取锁先检查等待队列（`hasQueuedPredecessors()`），无饥饿但吞吐量低。非公平锁新来的线程直接 CAS 抢，吞吐量高但可能饥饿。非公平锁之所以性能高，是因为减少了线程上下文切换：刚被唤醒的线程不必然能抢到锁，可能刚运行的线程直接抢到，避免了一次挂起/唤醒的开销。

**Q4：ReentrantLock 如何实现的（底层原理）？**
> 内部维护 `Sync` 类继承 AQS，state=0 表示未锁，state>0 表示持有并记录重入次数。`lock()` 本质是 CAS 将 state 从 0 改为 1；失败则判断是否重入（当前线程持有则 state+1）；再失败则封装成 Node 入 AQS 等待队列，调用 `LockSupport.park()` 挂起。`unlock()` 将 state-1，归零时 `LockSupport.unpark()` 唤醒队首线程。

**Q5：tryLock 和 lock 的区别？**
> `lock()` 会一直阻塞到获取锁为止；`tryLock()` 立即返回 boolean，不阻塞；`tryLock(time, unit)` 等待指定时间后返回。`tryLock` 内部走非公平逻辑，即使是公平锁实例，`tryLock()` 也不排队。

**Q6：ReentrantLock 会死锁吗？如何避免？**
> 会。常见原因：①忘记在 finally 中 `unlock()`；②两个线程互相等待对方持有的锁（循环等待）。避免方式：①`unlock()` 必须写在 finally；②使用 `tryLock(timeout)` 设超时避免永久等待；③固定加锁顺序避免循环等待。

**Q7：Condition 的 await 和 Object 的 wait 区别？**
> 两者都是释放锁并挂起线程。区别：一个 ReentrantLock 可以创建多个 Condition，实现精准的分组通知（比如生产者/消费者用两个 Condition 互相通知），而 Object 只有一个等待队列，`notify()` 只能随机唤醒，可能唤醒同类线程造成效率低。
