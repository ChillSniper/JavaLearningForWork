# Synchronized 知识点

---

## 一、基本用法

`synchronized` 是 JVM 内置的互斥锁关键字，有三种使用方式：

```java
// 1. 修饰实例方法 —— 锁对象是 this（当前实例）
public synchronized void method() { ... }

// 2. 修饰静态方法 —— 锁对象是 Class 对象（类锁）
public static synchronized void staticMethod() { ... }

// 3. 修饰代码块 —— 锁对象由自己指定
synchronized (lockObject) { ... }
```

**锁的本质：锁的是对象，不是代码块。**

- 同一个对象的所有 synchronized 方法共享同一把锁
- 不同实例之间互不影响

---

## 二、底层原理

### 2.1 对象头（Object Header）

每个 Java 对象在堆内存中由三部分组成：

```shell
┌─────────────────────────────────┐
│         Mark Word (64bit)        │  ← 存储锁状态、hashCode、GC分代年龄
├─────────────────────────────────┤
│         Klass Pointer            │  ← 指向类元数据
├─────────────────────────────────┤
│         Instance Data            │  ← 字段数据
└─────────────────────────────────┘
```

Mark Word 的内容随锁状态变化：

| 锁状态 | Mark Word 内容 |
| --- | --- |
| 无锁 | hashCode + 分代年龄 + 01 |
| 偏向锁 | 线程ID + Epoch + 分代年龄 + 101 |
| 轻量级锁 | 指向栈帧中 Lock Record 的指针 + 00 |
| 重量级锁 | 指向 Monitor 对象的指针 + 10 |
| GC标记 | 11 |

### 2.2 Monitor（监视器锁）

重量级锁依赖操作系统的 `Mutex Lock`，每个对象关联一个 Monitor：

```java
Monitor {
    _owner       // 持有锁的线程
    _EntryList   // 等待锁的线程队列（阻塞状态）
    _WaitSet     // 调用 wait() 后等待唤醒的线程队列
    _count       // 重入次数
}
```

字节码层面：

- `synchronized` 代码块 → `monitorenter` / `monitorexit` 指令
- `synchronized` 方法 → 方法标志位 `ACC_SYNCHRONIZED`

### 2.3 锁升级过程（单向，不可降级）

```text
无锁 → 偏向锁 → 轻量级锁 → 重量级锁
```

**偏向锁**:

- 场景：锁几乎总是被同一个线程获取
- 原理：将线程 ID 记录在 Mark Word，下次同一线程进入无需 CAS
- 撤销代价高（需 STW），JDK 15 起默认禁用

**轻量级锁**:

- 场景：多线程交替访问，无实际竞争
- 原理：CAS 将 Mark Word 替换为指向 Lock Record 的指针
- 竞争失败 → 自旋 → 仍失败 → 膨胀为重量级锁

**重量级锁**:

- 场景：多线程真实竞争
- 原理：依赖 OS Mutex，线程挂起进入内核态
- 代价：用户态/内核态切换，性能开销大

---

## 三、可重入性

`synchronized` 天然支持**可重入**（同一线程可以重复获取同一把锁）：

```java
public synchronized void outer() {
    inner(); // 不会死锁，同一线程重入
}
public synchronized void inner() { ... }
```

实现原理：Monitor 的 `_count` 计数器，每次重入 +1，每次退出 -1，归零时释放锁。

---

## 四、wait / notify 与 synchronized

`wait()`、`notify()`、`notifyAll()` 必须在 `synchronized` 块内调用，否则抛 `IllegalMonitorStateException`。

```java
synchronized (lock) {
    while (!condition) {   // 必须用 while，不能用 if（防止虚假唤醒）
        lock.wait();       // 释放锁，线程进入 WaitSet
    }
    // ... 执行逻辑
    lock.notifyAll();      // 唤醒 WaitSet 中所有线程
}
```

| 方法 | 说明 |
| --- | --- |
| `wait()` | 释放锁，线程挂起，进入 WaitSet |
| `notify()` | 随机唤醒 WaitSet 中一个线程，移入 EntryList |
| `notifyAll()` | 唤醒 WaitSet 中所有线程 |
| `wait(timeout)` | 超时自动唤醒 |

**与 `sleep()` 的区别：**

- `wait()` 释放锁；`sleep()` 不释放锁
- `wait()` 在 Object 上；`sleep()` 在 Thread 上
- `wait()` 需要在 synchronized 块中；`sleep()` 无限制

---

## 五、JVM 对 synchronized 的优化

### 5.1 自旋锁 / 自适应自旋

- 轻量级锁竞争失败时，不立即阻塞，而是 CPU 空转一段时间（默认10次）
- JDK 6+ 引入自适应自旋：根据上次自旋是否成功动态调整次数

### 5.2 锁消除

- JIT 编译器通过逃逸分析，发现锁对象不会被外部访问时，直接去掉锁

```java
// StringBuffer 的 append 是 synchronized 的，但 sb 不会逃逸，锁被消除
public String concat(String a, String b) {
    StringBuffer sb = new StringBuffer();
    sb.append(a);
    sb.append(b);
    return sb.toString();
}
```

### 5.3 锁粗化

- 多个连续的 synchronized 块对同一对象加锁，JIT 会合并为一个大锁

```java
// 优化前
synchronized(lock) { ... }
synchronized(lock) { ... }
synchronized(lock) { ... }

// 优化后（JIT 自动合并）
synchronized(lock) { ... ... ... }
```

---

## 六、synchronized vs ReentrantLock

| 特性 | `synchronized` | `ReentrantLock` |
| --- | --- | --- |
| 实现层面 | JVM 内置，关键字 | JDK 类库，`java.util.concurrent.locks` |
| 锁释放 | 自动（退出代码块/方法） | 必须手动 `unlock()`，需配合 `finally` |
| 可中断 | 不支持 | `lockInterruptibly()` 支持 |
| 超时获取 | 不支持 | `tryLock(timeout)` 支持 |
| 公平锁 | 不支持 | 构造器传 `true` 支持 |
| 多条件变量 | 只有一个 WaitSet | 可创建多个 `Condition` |
| 可重入 | 支持 | 支持 |
| 性能（JDK6+） | 优化后接近 | 接近 |
| 适用场景 | 简单同步，代码简洁 | 需要高级特性时 |

**结论：优先用 `synchronized`，需要高级特性（超时、中断、公平、多条件）时用 `ReentrantLock`。**

---

## 七、常见问题与陷阱

### 7.1 锁对象变化导致锁失效

```java
// 错误：String 是不可变对象，赋值后 lock 指向新对象，旧锁失效
private String lock = "lock";
synchronized (lock) {
    lock = "newLock"; // 危险！
}
```

### 7.2 锁 Integer / Long 等包装类

```java
// 危险：Integer 缓存范围 -128~127，超出范围每次 new 新对象
private Integer count = 0;
synchronized (count) { // 不同线程拿到不同对象！
    count++;
}
```

### 7.3 死锁

```java
// 线程1持有A，等待B；线程2持有B，等待A
synchronized (A) {
    synchronized (B) { ... }
}
// 预防：固定加锁顺序，使用 tryLock 超时
```

---

## 八、面试高频问题

**Q1：synchronized 锁的是什么？**
> 锁的是对象（实例方法锁 this，静态方法锁 Class 对象）。本质是 Mark Word 中记录的 Monitor 地址。

**Q2：synchronized 的锁升级过程？能降级吗？**
> 无锁 → 偏向锁 → 轻量级锁 → 重量级锁，单向升级，**正常情况下不可降级**（仅 GC STW 时特殊处理）。

**Q3：为什么 JDK 15 废弃偏向锁？**
> 偏向锁的撤销需要 STW，在多线程竞争普遍的现代应用中，撤销成本远大于收益，维护复杂度高。

**Q4：synchronized 和 volatile 的区别？**

> - `synchronized`：保证原子性、可见性、有序性；有阻塞
> - `volatile`：只保证可见性和有序性，**不保证原子性**；无阻塞

**Q5：wait() 为什么必须在 synchronized 中？**
> 释放锁和进入等待必须是原子操作。如果不在 synchronized 中，调用 wait() 前锁已释放，notify() 可能先于 wait() 执行，导致永久等待（lost wakeup 问题）。

**Q6：wait() 为什么用 while 而不是 if？**
> 防止**虚假唤醒（spurious wakeup）**。操作系统层面，线程可能在没有 notify() 的情况下被唤醒。用 while 确保醒来后重新检查条件。

**Q7：notify() 和 notifyAll() 怎么选？**
> 通常用 `notifyAll()` 更安全。`notify()` 只唤醒一个线程，如果唤醒的线程条件不满足，会重新 wait，其他线程永远无法被唤醒，造成"信号丢失"。

**Q8：synchronized 方法和 synchronized(this) 代码块有何区别？**
> 锁对象相同（都是 this），区别在于**粒度**。synchronized 方法整个方法体加锁；代码块可以只锁关键部分，减少锁的持有时间，提升并发度。

**Q9：一个类的两个 synchronized 实例方法能同时被两个线程执行吗？**
> 不能。两个方法锁的都是同一个 this 对象，同一时刻只有一个线程能持有该锁。

**Q10：synchronized 是公平锁还是非公平锁？**
> **非公平锁**。新来的线程在 EntryList 排队前，会先尝试 CAS 抢锁，抢到直接执行，不按等待顺序。
