# JUC 学习目录（java.util.concurrent）

> JUC 是 Java 并发编程的核心包，由 Doug Lea 主导设计。
> 学习路径建议：底层原理 → 锁机制 → 线程协作 → 并发容器 → 线程池 → 高级工具

---

## 一、前置基础（不在 JUC 包内，但必须掌握）

- `synchronized` 关键字（JVM 内置，偏向锁/轻量锁/重量锁升级）
- `volatile` 关键字（可见性 + 禁止指令重排）
- Java 内存模型（JMM）：主内存 / 工作内存 / happens-before 规则
- `ThreadLocal`（`java.lang` 包，线程隔离，InheritableThreadLocal）

---

## 二、底层原理

### 2.1 CAS（Compare And Swap）

- 原理：乐观锁的硬件实现（CPU 原子指令）
- `sun.misc.Unsafe` 类
- ABA 问题及解决（`AtomicStampedReference`）

### 2.2 原子类（`java.util.concurrent.atomic`）

- 基本类型：`AtomicInteger`、`AtomicLong`、`AtomicBoolean`
- 引用类型：`AtomicReference`、`AtomicStampedReference`、`AtomicMarkableReference`
- 数组类型：`AtomicIntegerArray`、`AtomicLongArray`
- 字段更新器：`AtomicIntegerFieldUpdater`、`AtomicReferenceFieldUpdater`
- 高性能累加：`LongAdder`、`LongAccumulator`（分段 + 伪共享问题）

---

## 三、锁机制（`java.util.concurrent.locks`）

### 3.1 AQS（AbstractQueuedSynchronizer）

- 核心数据结构：CLH 变体双向队列 + state 变量
- 独占模式 vs 共享模式
- 条件队列：`ConditionObject`
- 是 ReentrantLock / Semaphore / CountDownLatch 等的底层实现

### 3.2 ReentrantLock（可重入锁）

- 公平锁 vs 非公平锁
- 与 `synchronized` 的对比（超时、可中断、多条件变量）
- `tryLock()` / `lockInterruptibly()` 用法

### 3.3 ReadWriteLock（读写锁）

- `ReentrantReadWriteLock`：读共享、写独占
- 锁降级（写锁降为读锁）
- 适用场景：读多写少

### 3.4 StampedLock（JDK 8+）

- 乐观读、悲观读、写三种模式
- 不支持重入、不支持条件变量
- 性能优于 ReadWriteLock，但使用复杂

### 3.5 LockSupport

- `park()` / `unpark()` —— AQS 阻塞线程的底层工具
- 与 `wait/notify` 的区别

---

## 四、线程协作工具类

### 4.1 CountDownLatch（倒计时门闩）

- 一次性，不可重置
- 场景：主线程等待多个子任务完成

### 4.2 CyclicBarrier（循环屏障）

- 可重置复用
- 场景：多线程相互等待，到达屏障点后一起继续

### 4.3 Semaphore（信号量）

- 控制并发访问数量
- 场景：限流、连接池

### 4.4 Exchanger

- 两线程之间交换数据的同步点
- 场景：流水线数据交换

### 4.5 Phaser（JDK 7+）

- CountDownLatch + CyclicBarrier 的增强版
- 支持动态注册参与者

---

## 五、并发容器

### 5.1 并发 Map

- `ConcurrentHashMap`：JDK 7（分段锁） vs JDK 8（CAS + synchronized + 红黑树）
- `ConcurrentSkipListMap`：基于跳表，有序

### 5.2 并发 List / Set

- `CopyOnWriteArrayList`：写时复制，适合读多写少
- `CopyOnWriteArraySet`
- `ConcurrentSkipListSet`

### 5.3 阻塞队列（`BlockingQueue`）

- `ArrayBlockingQueue`：有界，数组实现
- `LinkedBlockingQueue`：可选有界，链表实现
- `PriorityBlockingQueue`：无界，优先级排序
- `DelayQueue`：延迟取出（元素实现 `Delayed` 接口）
- `SynchronousQueue`：无容量，直接移交
- `LinkedTransferQueue`：`transfer()` 方法，生产者等待消费者接收
- `LinkedBlockingDeque`：双端阻塞队列

### 5.4 非阻塞队列

- `ConcurrentLinkedQueue`：CAS 实现，无界，高并发
- `ConcurrentLinkedDeque`：双端

---

## 六、线程池（Executor 框架）

### 6.1 核心接口与类

- `Executor` → `ExecutorService` → `AbstractExecutorService` → `ThreadPoolExecutor`
- `ScheduledExecutorService` → `ScheduledThreadPoolExecutor`

### 6.2 ThreadPoolExecutor 七大参数

- `corePoolSize`、`maximumPoolSize`、`keepAliveTime`、`unit`
- `workQueue`（阻塞队列）
- `threadFactory`
- `handler`（拒绝策略）

### 6.3 四种拒绝策略

- `AbortPolicy`（默认，抛异常）
- `CallerRunsPolicy`（调用者线程执行）
- `DiscardPolicy`（静默丢弃）
- `DiscardOldestPolicy`（丢弃最老任务）

### 6.4 Executors 工具类（了解原理，生产慎用）

- `newFixedThreadPool`、`newCachedThreadPool`、`newSingleThreadExecutor`、`newScheduledThreadPool`

### 6.5 Future / Callable

- `Callable` vs `Runnable`
- `Future` / `FutureTask`
- `CompletableFuture`（JDK 8+，异步编排）

### 6.6 线程池调参与监控

- 如何合理设置线程数（CPU 密集型 vs IO 密集型）
- 线程池状态：RUNNING / SHUTDOWN / STOP / TIDYING / TERMINATED

---

## 七、Fork/Join 框架（JDK 7+）

- `ForkJoinPool`：工作窃取算法
- `ForkJoinTask`：`RecursiveTask`（有返回值） / `RecursiveAction`（无返回值）
- 适用场景：分治算法、大任务拆解

---

## 八、面试高频对比总结

| 对比项 | 结论 |
| --- | --- |
| `synchronized` vs `ReentrantLock` | Lock 更灵活，支持超时/中断/多条件；synchronized 更简单，JVM 自动释放 |
| `HashMap` vs `ConcurrentHashMap` | CHM 线程安全，JDK8 细化到桶级别加锁 |
| `ArrayList` vs `CopyOnWriteArrayList` | COW 读不加锁，写时复制；适合读多写少 |
| `CountDownLatch` vs `CyclicBarrier` | CDL 一次性，主等子；CB 可重置，互相等待 |
| `ThreadLocal` vs 锁 | ThreadLocal 空间换时间（线程隔离），锁是时间换空间（互斥） |
| `volatile` vs `synchronized` | volatile 只保证可见性+有序性，不保证原子性 |
| `LongAdder` vs `AtomicLong` | LongAdder 高并发下性能更好（分段累加），但实时精度略低 |
