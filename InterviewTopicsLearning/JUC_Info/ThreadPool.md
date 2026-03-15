# Java 线程池知识点

> 线程池属于 JUC Executor 框架，核心类 `ThreadPoolExecutor` 在 `java.util.concurrent` 包下。
> 核心价值：**复用线程**（避免频繁创建销毁的开销）、**控制并发量**（防止资源耗尽）、**管理任务队列**。

---

## 一、Java 创建线程的方式（前置知识）

```java
// 方式1：继承 Thread 类
class MyThread extends Thread {
    @Override public void run() { /* 任务逻辑 */ }
}
new MyThread().start();

// 方式2：实现 Runnable 接口（推荐，解耦任务与线程）
Thread t = new Thread(() -> System.out.println("task"));
t.start();

// 方式3：实现 Callable + FutureTask（有返回值 / 可抛异常）
FutureTask<Integer> ft = new FutureTask<>(() -> 42);
new Thread(ft).start();
Integer result = ft.get(); // 阻塞直到任务完成

// 方式4：线程池提交（生产环境唯一推荐方式）
ExecutorService pool = Executors.newFixedThreadPool(4);
pool.execute(() -> { /* Runnable，无返回值 */ });
Future<Integer> f = pool.submit(() -> 42); // Callable，有返回值
```

**本质只有一种：`new Thread().start()`**，其余是任务封装方式。
优先用 `Runnable` 而非继承 `Thread`，因为 Java 单继承，且任务与线程解耦。

---

## 二、Executor 框架层次结构

```java
Executor                          ← 顶层接口，只有 execute(Runnable)
  └── ExecutorService             ← 扩展：submit / shutdown / invokeAll 等
        ├── AbstractExecutorService   ← 实现了 submit / invokeAll / invokeAny
        │     └── ThreadPoolExecutor ← 核心实现类
        │           └── ScheduledThreadPoolExecutor ← 支持定时/周期任务
        └── ForkJoinPool          ← 工作窃取，用于分治任务

Executors                        ← 工具类，提供快捷创建线程池的静态方法
```

---

## 三、ThreadPoolExecutor 七大核心参数

```java
public ThreadPoolExecutor(
    int corePoolSize,        // 核心线程数（常驻，不会被回收）
    int maximumPoolSize,     // 最大线程数
    long keepAliveTime,      // 非核心线程的空闲存活时间
    TimeUnit unit,           // 存活时间单位
    BlockingQueue<Runnable> workQueue,   // 任务等待队列
    ThreadFactory threadFactory,         // 线程创建工厂
    RejectedExecutionHandler handler     // 拒绝策略
)
```

### 参数详解

| 参数 | 说明 |
| --- | --- |
| `corePoolSize` | 核心线程数。即使空闲也不销毁（除非设置 `allowCoreThreadTimeOut(true)`） |
| `maximumPoolSize` | 线程池最多能创建的线程数，必须 >= corePoolSize |
| `keepAliveTime` + `unit` | 超出 core 数量的线程，空闲超过此时间后被销毁 |
| `workQueue` | 当核心线程全忙时，新任务进入队列等待 |
| `threadFactory` | 可自定义线程名、优先级、是否守护线程，便于排查问题 |
| `handler` | 队列满 + 线程数达上限时的拒绝策略 |

---

## 四、任务提交流程（核心逻辑）

```java
提交任务
    │
    ├─ 当前线程数 < corePoolSize？
    │       YES → 创建新核心线程执行任务（即使有空闲线程）
    │
    ├─ 队列未满？
    │       YES → 任务入队列等待
    │
    ├─ 当前线程数 < maximumPoolSize？
    │       YES → 创建新非核心线程执行任务
    │
    └─ 以上都不满足 → 触发拒绝策略
```

**关键点**：任务不是第一时间入队，而是优先开满核心线程；队列满了才创建非核心线程。

---

## 五、四种阻塞队列选型

| 队列类型 | 特点 | 适用场景 |
| --- | --- | --- |
| `LinkedBlockingQueue` | 默认无界（Integer.MAX_VALUE），链表实现 | `FixedThreadPool` / `SingleThreadPool` 使用，**生产慎用**（OOM 风险） |
| `ArrayBlockingQueue` | 有界，数组实现，必须指定容量 | 生产环境推荐，明确限制待处理任务数 |
| `SynchronousQueue` | 容量为 0，直接移交给线程 | `CachedThreadPool` 使用，来一个任务就要有线程接，不存储 |
| `PriorityBlockingQueue` | 无界，按优先级排序 | 任务有优先级区分的场景 |

---

## 六、四种拒绝策略

| 策略 | 类名 | 行为 |
| --- | --- | --- |
| 抛异常（默认） | `AbortPolicy` | 抛出 `RejectedExecutionException`，调用方感知到任务被拒 |
| 调用者执行 | `CallerRunsPolicy` | 由提交任务的线程自己执行，起到负反馈限速作用 |
| 静默丢弃 | `DiscardPolicy` | 直接丢弃新任务，无任何提示 |
| 丢弃最老任务 | `DiscardOldestPolicy` | 丢弃队列头部最老的任务，然后重新提交当前任务 |

**生产实践**：通常自定义拒绝策略，记录日志 + 告警，或将被拒绝的任务持久化到 MQ/DB 中重试。

```java
// 自定义拒绝策略示例
executor.setRejectedExecutionHandler((r, pool) -> {
    log.error("任务被拒绝，当前队列大小: {}", pool.getQueue().size());
    // 可选：存入数据库 / 发送告警 / 降级处理
});
```

---

## 七、线程池的五种状态

```java
RUNNING  →（调用 shutdown()）→  SHUTDOWN  →（队列空+线程空）→  TIDYING  →  TERMINATED
         →（调用 shutdownNow()）→  STOP     →（线程空）      →  TIDYING  →  TERMINATED
```

| 状态 | 说明 |
| --- | --- |
| `RUNNING` | 正常运行，接受新任务，处理队列任务 |
| `SHUTDOWN` | 不接受新任务，继续处理队列中已有任务 |
| `STOP` | 不接受新任务，不处理队列任务，中断正在执行的线程 |
| `TIDYING` | 所有任务终止，线程数为0，准备调用 `terminated()` |
| `TERMINATED` | `terminated()` 执行完毕，彻底终止 |

状态和线程数被合并存储在一个 `AtomicInteger ctl` 中（高3位存状态，低29位存线程数）。

---

## 八、Executors 快捷方法（了解原理，生产慎用）

```java
// 固定大小线程池，队列无界（LinkedBlockingQueue）→ OOM 风险
ExecutorService fixed = Executors.newFixedThreadPool(4);

// 单线程池，保证任务顺序执行，队列无界 → OOM 风险
ExecutorService single = Executors.newSingleThreadExecutor();

// 弹性线程池，无 core 线程，max=Integer.MAX_VALUE，SynchronousQueue → 线程数失控
ExecutorService cached = Executors.newCachedThreadPool();

// 定时 / 周期任务线程池
ScheduledExecutorService scheduled = Executors.newScheduledThreadPool(4);
scheduled.scheduleAtFixedRate(task, 0, 5, TimeUnit.SECONDS); // 固定速率
scheduled.scheduleWithFixedDelay(task, 0, 5, TimeUnit.SECONDS); // 固定延迟
```

**阿里规范明确禁止使用 Executors**，原因：隐藏了队列大小和最大线程数，容易导致 OOM 或线程数失控。

---

## 九、Future / Callable / CompletableFuture

### 9.1 Future + Callable

```java
ExecutorService pool = Executors.newFixedThreadPool(4);

// 提交有返回值的任务
Future<Integer> future = pool.submit(() -> {
    Thread.sleep(1000);
    return 42;
});

// get() 阻塞等待，可设置超时
Integer result = future.get(2, TimeUnit.SECONDS);

// isDone() 非阻塞检查
if (future.isDone()) { ... }

// cancel() 尝试取消
future.cancel(true); // true = 中断正在执行的线程
```

**Future 的局限**：`get()` 会阻塞，多个 Future 难以编排（无法方便地实现"A完成后执行B"）。

### 9.2 CompletableFuture（JDK 8+，重点）

```java
// 异步执行，默认使用 ForkJoinPool.commonPool()（生产建议传入自定义线程池）
CompletableFuture<Integer> cf = CompletableFuture.supplyAsync(() -> 42, customPool);

// 链式回调：上一步结果作为下一步输入
cf.thenApply(x -> x * 2)          // 同步转换，有返回值
  .thenAccept(x -> log(x))         // 消费结果，无返回值
  .thenRun(() -> cleanup());       // 不关心结果，执行动作

// 组合两个 CF（都完成后执行）
CompletableFuture.allOf(cf1, cf2).thenRun(() -> System.out.println("all done"));

// 任一完成就执行
CompletableFuture.anyOf(cf1, cf2).thenAccept(result -> System.out.println(result));

// 异常处理
cf.exceptionally(e -> { log(e); return -1; })
  .handle((result, e) -> e != null ? -1 : result); // 统一处理正常和异常

// 两个 CF 都完成后合并结果
cf1.thenCombine(cf2, (r1, r2) -> r1 + r2);
```

---

## 十、线程池调参最佳实践

### 10.1 线程数如何设置？

| 任务类型 | 公式 | 说明 |
| --- | --- | --- |
| **CPU 密集型** | `核心数 + 1` | 充分利用 CPU，+1 是为了防止偶发中断导致 CPU 空闲 |
| **IO 密集型** | `核心数 × (1 + 等待时间/计算时间)` | IO 等待期间 CPU 空闲，可多开线程 |
| **经验值（IO密集）** | `核心数 × 2` | 粗略估算，实际需压测 |

> 例：8 核机器，纯 IO 任务（等待/计算 ≈ 9:1 → 等待比0.9），线程数 = 8 × (1 + 9) = **80**

**最终方案：压测 + 监控，动态调整。** 公式只是起点。

### 10.2 生产环境推荐配置

```java
// 推荐写法：手动创建 ThreadPoolExecutor，参数明确
ThreadPoolExecutor executor = new ThreadPoolExecutor(
    8,                              // corePoolSize
    16,                             // maximumPoolSize
    60L, TimeUnit.SECONDS,          // keepAliveTime
    new ArrayBlockingQueue<>(1000), // 有界队列，防止 OOM
    new ThreadFactoryBuilder()      // 自定义线程名（Guava / 手写）
        .setNameFormat("order-pool-%d")
        .build(),
    new ThreadPoolExecutor.CallerRunsPolicy() // 拒绝时由调用方执行，起限速作用
);

// 允许核心线程也超时回收（可选，节省资源）
executor.allowCoreThreadTimeOut(true);
```

### 10.3 优雅关闭

```java
// 优雅关闭：不接受新任务，等待已有任务全部完成
executor.shutdown();
if (!executor.awaitTermination(60, TimeUnit.SECONDS)) {
    // 超时仍未结束，强制关闭
    executor.shutdownNow();
}
```

---

## 十一、线程池监控

`ThreadPoolExecutor` 提供多个状态查询方法，可埋点上报 Prometheus / 打印日志：

```java
// 获取监控指标
executor.getCorePoolSize()        // 核心线程数
executor.getMaximumPoolSize()     // 最大线程数
executor.getPoolSize()            // 当前线程总数
executor.getActiveCount()         // 正在执行任务的线程数
executor.getQueue().size()        // 队列中等待的任务数
executor.getQueue().remainingCapacity() // 队列剩余容量
executor.getTaskCount()           // 历史提交总任务数
executor.getCompletedTaskCount()  // 已完成任务数

// 告警规则（参考）：
// 队列使用率 > 80% → 预警
// 活跃线程数 / 最大线程数 > 90% → 预警
// 拒绝策略触发次数 > 0 → 立即告警
```

### 动态线程池（美团方案）

生产中线程池参数往往需要在不重启的情况下调整：

```java
// ThreadPoolExecutor 支持运行时修改参数
executor.setCorePoolSize(16);
executor.setMaximumPoolSize(32);
// 队列大小不能直接修改，需自定义 ResizableBlockingQueue
```

---

## 十二、ScheduledThreadPoolExecutor

```java
ScheduledExecutorService scheduler = new ScheduledThreadPoolExecutor(4);

// 延迟执行（只执行一次）
scheduler.schedule(task, 5, TimeUnit.SECONDS);

// 固定速率（不管上次是否完成，每隔 period 触发一次）
// 若任务执行时间 > period，则立即执行下一次（不会并行，会延后）
scheduler.scheduleAtFixedRate(task, 0, 10, TimeUnit.SECONDS);

// 固定延迟（上次完成后，再等 delay 才触发）
scheduler.scheduleWithFixedDelay(task, 0, 10, TimeUnit.SECONDS);
```

**与 Timer 的区别**：`Timer` 单线程，一个任务异常会杀死所有定时任务；`ScheduledThreadPoolExecutor` 多线程，健壮性更好。

---

## 十三、面试高频问题

**Q1：线程池的核心参数有哪些？任务提交后的执行流程是什么？**
> 七大参数：corePoolSize、maximumPoolSize、keepAliveTime、unit、workQueue、threadFactory、handler。
>
> 流程：线程数 < core → 创建核心线程；线程数 >= core → 入队；队列满 + 线程数 < max → 创建非核心线程；均满 → 拒绝策略。

**Q2：为什么不推荐用 Executors 创建线程池？**
> `FixedThreadPool` / `SingleThreadPool` 使用无界队列（`LinkedBlockingQueue`），任务堆积会导致 OOM；`CachedThreadPool` 最大线程数为 `Integer.MAX_VALUE`，可创建大量线程导致 OOM 或 CPU 耗尽。应手动创建 `ThreadPoolExecutor` 并明确所有参数。

**Q3：核心线程数怎么设置？**
> CPU 密集型：核心数 + 1；IO 密集型：核心数 × 2（经验值）或核心数 × (1 + 等待时间/计算时间)。最终需通过压测确定，没有万能公式。

**Q4：线程池的拒绝策略有哪些？生产中用哪个？**
> AbortPolicy（抛异常）、CallerRunsPolicy（调用者执行）、DiscardPolicy（静默丢弃）、DiscardOldestPolicy（丢弃最老）。
>
> 生产中通常**自定义**：记录告警日志 + 将任务持久化到 MQ / DB，或使用 `CallerRunsPolicy` 做限速。

**Q5：线程池怎么做到线程复用的？**
> 核心线程调用 `workQueue.take()` 阻塞等待任务（永不退出）；非核心线程调用 `workQueue.poll(keepAliveTime)` 超时等待，超时返回 null 后线程退出。本质是线程在 `Worker.run()` 里死循环从队列取任务，而不是任务执行完就退出。

**Q6：execute() 和 submit() 的区别？**
> `execute(Runnable)`：无返回值，异常会直接抛出并传播到 UncaughtExceptionHandler。
> `submit(Callable/Runnable)`：返回 `Future`，异常被封装在 Future 中，调用 `get()` 时才抛出，如果不调用 `get()` 异常会被吞掉。

**Q7：shutdown() 和 shutdownNow() 的区别？**
> `shutdown()`：平滑关闭，不接受新任务，等待队列中的任务和正在执行的任务全部完成。
> `shutdownNow()`：立即关闭，向正在执行的线程发送中断信号，返回队列中未执行的任务列表（不保证正在执行的任务能被中断）。

**Q8：CompletableFuture 和 Future 的区别？**
> `Future`只能阻塞 `get()` 等结果，无法链式编排，多个 Future 组合复杂。
> `CompletableFuture` 支持回调（thenApply/thenAccept）、组合（allOf/anyOf/thenCombine）、异常处理（exceptionally/handle），可实现非阻塞的异步流水线。

**Q9：线程池中线程抛出异常怎么处理？**

> - `execute()` 提交：异常被 `Worker` 捕获后，交给线程的 `UncaughtExceptionHandler`，然后该 Worker 线程终止，线程池补充新线程。
> - `submit()` 提交：异常被封装进 `FutureTask`，线程不会终止，但不调用 `get()` 会导致异常被**静默吞掉**。
>
> 最佳实践：统一在 `ThreadFactory` 中设置 `UncaughtExceptionHandler` 记录日志；`submit()` 任务内部 try-catch。

**Q10：如何实现一个简单的线程池？（手写思路）**：

> 1. 维护一个 `BlockingQueue` 存任务
> 2. 创建固定数量的 Worker 线程，每个线程循环从队列 `take()` 任务执行
> 3. `execute()` 方法将任务放入队列
> 4. `shutdown()` 时清空队列 + 中断所有 Worker 线程

```java
public class SimpleThreadPool {
    private final BlockingQueue<Runnable> queue = new LinkedBlockingQueue<>(100);
    private final List<Thread> workers = new ArrayList<>();

    public SimpleThreadPool(int nThreads) {
        for (int i = 0; i < nThreads; i++) {
            Thread t = new Thread(() -> {
                while (!Thread.currentThread().isInterrupted()) {
                    try {
                        Runnable task = queue.take(); // 阻塞等待
                        task.run();
                    } catch (InterruptedException e) {
                        Thread.currentThread().interrupt(); // 恢复中断标志
                    }
                }
            }, "worker-" + i);
            workers.add(t);
            t.start();
        }
    }

    public void execute(Runnable task) {
        queue.offer(task); // 队列满时返回 false（可扩展拒绝策略）
    }

    public void shutdown() {
        workers.forEach(Thread::interrupt);
    }
}
```
