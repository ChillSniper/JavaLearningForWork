# CAS 与 AQS

---

## 一、CAS（Compare And Swap）

### 核心概念

CAS 是一种**乐观锁**思想的底层实现，属于 CPU 层面的原子指令（`cmpxchg`）。

它包含三个操作数：

- **V**：内存中的当前值（Variable）
- **E**：期望值（Expected）
- **N**：要写入的新值（New）

**操作逻辑**：只有当 V == E 时，才将 V 更新为 N；否则什么都不做，返回失败。

整个操作由 CPU 保证原子性，不需要加锁。

### Java 中的实现

Java 通过 `sun.misc.Unsafe` 类调用底层 CAS 指令，`java.util.concurrent.atomic` 包下的类（如 `AtomicInteger`、`AtomicLong`、`AtomicReference`）都是基于 CAS 实现的。

```java
// AtomicInteger 自增的底层逻辑（简化）
public final int getAndIncrement() {
    return unsafe.getAndAddInt(this, valueOffset, 1);
}
// Unsafe 中：
public final int getAndAddInt(Object o, long offset, int delta) {
    int v;
    do {
        v = getIntVolatile(o, offset);  // 读取当前值
    } while (!compareAndSwapInt(o, offset, v, v + delta));  // CAS 失败就重试
    return v;
}
```

这里的循环重试就是**自旋（Spin）**，也叫自旋锁。

### CAS 的三大问题

| 问题                 | 描述                                             | 解决方案                                                              |
| -------------------- | ------------------------------------------------ | --------------------------------------------------------------------- |
| **ABA 问题**         | 值从 A 改为 B 再改回 A，CAS 无法感知中间变化     | 使用 `AtomicStampedReference`（带版本号）或 `AtomicMarkableReference` |
| **自旋开销**         | CAS 失败会一直循环重试，CPU 空转，高竞争下性能差 | 限制自旋次数，或升级为悲观锁（如 synchronized）                       |
| **只能保证单个变量** | CAS 只能对一个内存地址原子操作                   | 使用 `AtomicReference` 封装对象，或直接用锁                           |

### 面试如何回答 CAS

> "CAS 是 CPU 级别的原子指令，包含期望值、当前值、新值三个参数。只有当前值等于期望值时才写入新值，否则返回失败。Java 的 `Unsafe` 类对其进行了封装，`atomic` 包下的原子类都是基于 CAS 实现的无锁并发。它的优点是避免了线程切换和上下文开销，适合竞争不激烈的场景；缺点是存在 ABA 问题、自旋消耗 CPU、只能保证单变量原子性这三个问题。"

---

## 二、AQS（AbstractQueuedSynchronizer）

### 核心概念 AQS

`AbstractQueuedSynchronizer` 是 `java.util.concurrent.locks` 包中的**抽象基础框架类**，是 Java 中大多数同步器的骨架实现。

**基于 AQS 实现的类：**

- `ReentrantLock`（可重入锁）
- `ReentrantReadWriteLock`（读写锁）
- `Semaphore`（信号量）
- `CountDownLatch`（倒计时门栓）
- `CyclicBarrier`（循环屏障，间接依赖）
- `ThreadPoolExecutor` 中的 Worker

### AQS 的核心数据结构

AQS 内部维护两样东西：

**1. `state` 变量（volatile int）**

表示同步状态，不同子类语义不同：

- `ReentrantLock`：0 = 未锁，>0 = 被持有（值代表重入次数）
- `Semaphore`：state = 剩余许可数
- `CountDownLatch`：state = 待 countDown 的次数

**2. CLH 变体的双向等待队列（FIFO）**：

当线程获取锁失败时，会被封装成 `Node` 节点加入队列，挂起（`LockSupport.park()`）。锁释放时唤醒队首节点（`LockSupport.unpark()`）。

```java
Head <-> Node(Thread1) <-> Node(Thread2) <-> Node(Thread3) <-> Tail
         (等待中)          (等待中)          (等待中)
```

### AQS 的工作流程

```java
tryAcquire(state) ──成功──> 拿到锁，执行业务
        │
       失败
        │
   加入等待队列（Node 入队）
        │
   park() 挂起线程
        │
   前驱节点释放锁后 unpark() 唤醒
        │
   重新 tryAcquire(state)
```

### AQS 的两种模式

| 模式                     | 说明                     | 代表实现                                    |
| ------------------------ | ------------------------ | ------------------------------------------- |
| **独占模式 (Exclusive)** | 同一时刻只有一个线程持有 | `ReentrantLock`                             |
| **共享模式 (Shared)**    | 多个线程可同时持有       | `Semaphore`、`CountDownLatch`、读写锁的读锁 |

### AQS 的模板方法设计

AQS 使用**模板方法模式**，子类只需重写以下方法：

```java
// 独占模式 - 子类实现具体的获取/释放逻辑
protected boolean tryAcquire(int arg)   // 尝试获取锁
protected boolean tryRelease(int arg)   // 尝试释放锁

// 共享模式
protected int tryAcquireShared(int arg)   // >=0 表示成功
protected boolean tryReleaseShared(int arg)

// 是否持有独占锁（Condition 需要）
protected boolean isHeldExclusively()
```

AQS 负责**队列管理、线程挂起/唤醒**，子类只负责**state 的语义和修改逻辑**。

### 面试如何回答 AQS

> "AQS 是 JUC 中同步器的抽象基类，核心是一个 volatile 的 state 变量和一个 CLH 变体双向等待队列。线程获取锁本质是 CAS 修改 state；失败则入队并 park 挂起；释放锁时 unpark 唤醒队首线程。AQS 用模板方法模式，把队列管理和线程调度封装好，子类只需实现 tryAcquire/tryRelease 等方法。`ReentrantLock`、`Semaphore`、`CountDownLatch` 都是基于 AQS 实现的。"

---

## 三、CAS 与 AQS 的关系与区别

### 关系

**AQS 内部使用 CAS**。AQS 用 CAS 来原子地修改 `state` 和操作等待队列中的节点，这是它实现无锁入队的基础。

```java
AQS（框架层）
 ├── 用 CAS 修改 state（获取/释放锁的原子操作）
 ├── 用 CAS 操作队列节点（compareAndSetTail 等）
 └── 用 LockSupport.park/unpark 挂起/唤醒线程
```

### 区别对比

| 维度         | CAS                            | AQS                             |
| ------------ | ------------------------------ | ------------------------------- |
| **本质**     | CPU 原子指令（硬件级别）       | Java 抽象类（软件框架）         |
| **层次**     | 底层实现机制                   | 上层同步框架                    |
| **思想**     | 乐观锁，无锁竞争               | 悲观锁思想，获取失败则阻塞入队  |
| **适用场景** | 竞争不激烈，操作简单（单变量） | 复杂同步场景，需要等待/唤醒机制 |
| **CPU 开销** | 失败时自旋，消耗 CPU           | 失败时 park 阻塞，让出 CPU      |
| **代表实现** | `AtomicInteger` 等原子类       | `ReentrantLock`、`Semaphore` 等 |

### 一句话总结

> **CAS 是 AQS 的底层工具，AQS 是基于 CAS 构建的同步框架。**
> 两者共同构成了 Java 并发包（JUC）的核心基础。

---

## 四、高频面试题汇总

**Q1：什么是 CAS，有什么问题？**
> CAS 是原子性的比较交换操作，Java 通过 Unsafe 封装。三大问题：ABA、自旋开销、单变量限制。

**Q2：什么是 AQS？说说它的实现原理。**
> AQS 是同步器抽象基类，核心是 volatile state + CLH 队列。获取锁用 CAS 改 state，失败入队 park；释放锁改 state 并 unpark 队首。子类用模板方法实现具体语义。

**Q3：ReentrantLock 是如何基于 AQS 实现的？**
> `ReentrantLock` 内部有 `Sync` 类继承 AQS。state=0 表示未锁，state>0 表示重入次数。`lock()` 调用 `acquire(1)`，最终调用子类的 `tryAcquire`，用 CAS 尝试 state 从 0 改为 1；失败则入队等待。

**Q4：CountDownLatch 是如何基于 AQS 实现的？**
> `CountDownLatch` 用共享模式。初始 state=count。`countDown()` 调用 `releaseShared`，CAS 将 state-1；`await()` 调用 `acquireSharedInterruptibly`，当 state>0 时入队阻塞，直到 state=0 时唤醒所有等待线程。

**Q5：CAS 和 synchronized 如何选择？**
> 竞争不激烈、操作简单 → 用 CAS（原子类）；竞争激烈、操作复杂、需要条件等待 → 用 synchronized 或 ReentrantLock（基于 AQS）。
