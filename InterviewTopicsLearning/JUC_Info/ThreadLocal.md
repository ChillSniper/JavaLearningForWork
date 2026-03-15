# ThreadLocal 知识点

> ThreadLocal 属于 `java.lang` 包，不是 JUC 的一部分，但常与并发知识一起考察。
> 核心思想：**空间换时间**，每个线程持有变量的独立副本，彻底消除共享，无需加锁。

---

## 一、基本用法

```java
// 创建
ThreadLocal<String> tl = new ThreadLocal<>();

// 设置当前线程的值
tl.set("hello");

// 获取当前线程的值
String val = tl.get();  // "hello"

// 删除当前线程的值（重要！防止内存泄漏）
tl.remove();

// 设置初始值（推荐用 withInitial 工厂方法）
ThreadLocal<List<String>> tl2 = ThreadLocal.withInitial(ArrayList::new);
```

典型场景：

- 数据库连接 / Session 绑定到当前线程（Spring 的 `TransactionSynchronizationManager`）
- 用户身份信息透传（登录用户 ID 在请求链路中传递）
- `SimpleDateFormat` 线程安全包装（它本身非线程安全）
- MDC 日志追踪（如 Slf4j 的 `MDC`）

---

## 二、底层数据结构

### 2.1 整体关系

ThreadLocal **本身不存储数据**，数据存在 Thread 对象内部：

```java
Thread
 └── ThreadLocal.ThreadLocalMap threadLocals   ← 每个线程持有自己的 Map
       └── Entry[]  table                       ← 开放地址法的哈希表
             └── Entry extends WeakReference<ThreadLocal<?>>
                   key   = WeakReference(ThreadLocal实例)  ← 弱引用
                   value = Object（实际存储的值）            ← 强引用
```

### 2.2 ThreadLocalMap 内部结构

```java
// Thread 类中
ThreadLocal.ThreadLocalMap threadLocals = null;

// ThreadLocalMap 是 ThreadLocal 的静态内部类
static class ThreadLocalMap {
    // Entry 继承 WeakReference，key 是 ThreadLocal 实例（弱引用）
    static class Entry extends WeakReference<ThreadLocal<?>> {
        Object value;
        Entry(ThreadLocal<?> k, Object v) {
            super(k);       // key 以弱引用形式保存
            value = v;
        }
    }
    private Entry[] table;  // 哈希表，初始容量 16
    private int size;
    private int threshold;  // 扩容阈值（默认 2/3）
}
```

### 2.3 哈希定位

```java
// ThreadLocal 的 threadLocalHashCode，使用魔数 0x61c88647（黄金分割比）
// 使哈希值均匀分布，减少碰撞
private final int threadLocalHashCode = nextHashCode();
private static final int HASH_INCREMENT = 0x61c88647;

// 定位桶：
int i = key.threadLocalHashCode & (len - 1);
```

**冲突解决：开放地址法（线性探测）**，与 HashMap 的链表/红黑树不同。

---

## 三、set / get / remove 流程

### 3.1 set(value)

```java
1. 获取当前线程 t = Thread.currentThread()
2. 获取 t.threadLocals（ThreadLocalMap）
3. 如果 map 不为空：
     map.set(this, value)
       → 计算 hash 定位桶 i
       → 若 table[i] 为空，直接放入
       → 若 key 相同，更新 value
       → 若 key 为 null（弱引用已被 GC），替换该过期 Entry（顺便清理）
       → 否则线性探测下一个槽位
       → 检查是否需要扩容（size >= threshold 时 rehash + resize）
4. 如果 map 为空：createMap(t, value)（初始化 ThreadLocalMap）
```

### 3.2 get()

```java
1. 获取当前线程的 ThreadLocalMap
2. 以 this（ThreadLocal实例）为 key 查找 Entry
3. 找到且 key 不为 null → 返回 value
4. 找不到 → 调用 setInitialValue()，返回初始值（默认 null）
```

### 3.3 remove()

```java
1. 获取当前线程的 ThreadLocalMap
2. 找到对应 Entry，将 key 置为 null，value 置为 null
3. 执行 expungeStaleEntry() 清理后续过期的 Entry
```

---

## 四、内存泄漏问题（重点）

### 4.1 为什么会泄漏？

```java
强引用链：Thread → ThreadLocalMap → Entry → value
弱引用：Entry.key → ThreadLocal 实例
```

当 ThreadLocal 对象没有外部强引用时：

- **key（ThreadLocal 实例）**：弱引用，下次 GC 时被回收，key 变为 null
- **value（实际数据）**：强引用仍在，GC 无法回收

此时 Entry 变为 `{key=null, value=存活的对象}`，**value 永远无法被访问，也无法被 GC**。

如果线程长期存活（如线程池中的线程），这些 value 会一直堆积，造成**内存泄漏**。

### 4.2 为什么 key 设计为弱引用？

如果 key 是强引用：ThreadLocal 实例在业务代码中赋值为 null 后，仍因 Entry.key 的强引用而无法被 GC，泄漏更严重。

弱引用是一种**权衡**：让 ThreadLocal 实例本身可以被 GC，同时通过探测清理机制（set/get/remove 时的 `expungeStaleEntry`）顺带清理 value，只要使用者配合调用 `remove()`。

### 4.3 ThreadLocalMap 的自救机制

在 `set()` / `get()` / `remove()` 时，ThreadLocalMap 会触发**过期 Entry 清理**：

- `expungeStaleEntry(int staleSlot)`：清理指定位置及其之后连续的过期 Entry
- `cleanSomeSlots()`：启发式扫描清理

但这是**被动触发**的，不能完全依赖它。

### 4.4 正确使用姿势

```java
ThreadLocal<UserInfo> userLocal = new ThreadLocal<>();

try {
    userLocal.set(currentUser);
    // ... 业务逻辑
} finally {
    userLocal.remove();  // 必须在 finally 中调用！
}
```

**线程池场景下尤其重要**：线程池的线程会被复用，如果不 remove，上一个任务遗留的数据会被下一个任务读到（数据污染 + 内存泄漏双重问题）。

---

## 五、InheritableThreadLocal

### 5.1 作用

`ThreadLocal` 中，子线程**无法**获取父线程设置的值。`InheritableThreadLocal` 解决了这个问题：

```java
InheritableThreadLocal<String> itl = new InheritableThreadLocal<>();
itl.set("parent-value");

new Thread(() -> {
    System.out.println(itl.get()); // "parent-value"，子线程继承了父线程的值
}).start();
```

### 5.2 实现原理

Thread 类中有两个 Map：

```java
ThreadLocal.ThreadLocalMap threadLocals = null;          // 普通
ThreadLocal.ThreadLocalMap inheritableThreadLocals = null; // 可继承
```

创建子线程时（`Thread` 构造器中调用 `init()`）：

```java
if (parent.inheritableThreadLocals != null) {
    this.inheritableThreadLocals =
        ThreadLocal.createInheritedMap(parent.inheritableThreadLocals);
    // 浅拷贝父线程的 inheritableThreadLocals 到子线程
}
```

### 5.3 局限性

- **只在线程创建时拷贝一次**，之后父线程修改值，子线程感知不到
- **线程池场景下失效**：线程池中的线程是提前创建的，不是任务提交时创建，拷贝时机不对
- 解决方案：阿里开源的 **TransmittableThreadLocal (TTL)**，专门解决线程池场景下的上下文传递

---

## 六、TransmittableThreadLocal（TTL）简介

> 阿里巴巴开源：`com.alibaba:transmittable-thread-local`

解决问题：线程池中，父线程提交任务时，将当前线程的 ThreadLocal 值**主动传递**给执行任务的子线程。

核心机制：

1. 提交任务时，**快照**（capture）当前线程的 TTL 值
2. 任务执行前，将快照**回放**（replay）到执行线程
3. 任务执行后，**恢复**（restore）执行线程原有的值

```java
// 使用 TtlRunnable / TtlCallable 包装任务
ExecutorService pool = TtlExecutors.getTtlExecutorService(Executors.newFixedThreadPool(4));
TransmittableThreadLocal<String> ttl = new TransmittableThreadLocal<>();

ttl.set("request-id-123");
pool.submit(TtlRunnable.get(() -> {
    System.out.println(ttl.get()); // "request-id-123"，跨线程池正确传递
}));
```

---

## 七、面试高频问题

**Q1：ThreadLocal 是什么？解决什么问题？**
> ThreadLocal 为每个线程提供独立的变量副本，线程间互不影响，实现线程隔离。解决的是多线程共享变量的线程安全问题，以空间换时间，无需加锁。

**Q2：ThreadLocal 的底层数据结构是什么？**
> 每个 Thread 对象内部有一个 `ThreadLocalMap`，它是一个以 **ThreadLocal 实例为 key、实际值为 value** 的哈希表。哈希冲突用**开放地址法（线性探测）**解决，而非链表。ThreadLocal 实例本身不存数据。

**Q3：ThreadLocal 为什么会内存泄漏？如何避免？**
> ThreadLocalMap 的 Entry 中，key（ThreadLocal）是**弱引用**，value 是**强引用**。当 ThreadLocal 实例没有外部强引用时，key 被 GC 回收变为 null，但 value 由于强引用链（Thread → Map → Entry → value）无法被回收，造成内存泄漏。在线程池场景下，线程长期存活，泄漏更严重。
>
> **避免方式：使用完后必须调用 `remove()`，推荐放在 `finally` 块中。**

**Q4：ThreadLocalMap 为什么用弱引用而不是强引用作为 key？**
> 如果 key 是强引用，当业务代码中 ThreadLocal 变量赋值为 null 时，ThreadLocalMap 的 Entry 仍持有其强引用，导致 ThreadLocal 实例无法被 GC，泄漏更严重。弱引用让 ThreadLocal 实例在没有外部引用时可以被 GC，只剩 value 的泄漏问题，配合 `remove()` 即可彻底解决。

**Q5：ThreadLocalMap 用什么解决哈希冲突？和 HashMap 有什么区别？**
> ThreadLocalMap 使用**开放地址法（线性探测）**：冲突时向后探测下一个空槽。
> HashMap 使用**链地址法**：冲突时形成链表，达到阈值转为红黑树。
>
> ThreadLocalMap 选择线性探测的原因：ThreadLocal 数量通常很少，线性探测在小容量下性能更好，且便于在探测过程中顺带清理过期 Entry。

**Q6：ThreadLocal 和 synchronized 的区别？**

> - `synchronized`：多线程**竞争同一个资源**，通过互斥保证同一时刻只有一个线程访问（时间换空间）
> - `ThreadLocal`：每个线程**拥有自己的副本**，根本不共享，无需竞争（空间换时间）
>
> 适用场景不同：synchronized 用于多线程需要协作修改共享数据；ThreadLocal 用于线程间数据完全隔离。

**Q7：子线程能访问父线程的 ThreadLocal 值吗？**
> 普通 `ThreadLocal` 不能。`InheritableThreadLocal` 可以，在子线程创建时会**浅拷贝**父线程的值。但在线程池场景下，线程是预先创建的，`InheritableThreadLocal` 也会失效，需要使用阿里的 **TransmittableThreadLocal（TTL）**。

**Q8：线程池中使用 ThreadLocal 有什么问题？**
> 两个问题：
>
>1. **数据污染**：线程池中的线程被复用，上个任务 set 的值如果没有 remove，下个任务的 get 会读到脏数据
>2. **内存泄漏**：线程长期存活，Entry 中的 value 无法被 GC
>
> 解决：任务结束时**必须 remove()**；如需父子线程传值使用 TTL。

**Q9：ThreadLocal 的 set() 流程中是如何处理 key 为 null 的 Entry 的？**
> 在线性探测过程中，如果遇到 key 为 null（弱引用已被 GC）的 Entry，会触发 `replaceStaleEntry()`：将新值放入该位置，并以该位置为起点向前向后扫描，清理所有 key 为 null 的过期 Entry（`expungeStaleEntry`），同时对非过期 Entry 重新 rehash 到正确位置。

**Q10：ThreadLocal 适合存什么？不适合存什么？**
> **适合**：请求级别的上下文（用户信息、traceId、数据库连接、事务对象）；线程安全包装（SimpleDateFormat）
>
> **不适合**：需要跨线程共享的数据；生命周期长于线程的大对象（泄漏风险）；需要子线程继承但使用线程池的场景（用 TTL 替代）
