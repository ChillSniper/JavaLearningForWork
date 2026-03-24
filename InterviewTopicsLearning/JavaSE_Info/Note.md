# Note of Essential part of JavaSE

## About JDK

### 版本命名说明

Java 的版本命名历史上发生过一次变化：

| 旧叫法（JDK 1.x） | 实际版本                         |
| ----------------- | -------------------------------- |
| JDK 1.0 ~ 1.4     | Java 1 ~ Java 4                  |
| JDK 1.5           | Java 5                           |
| JDK 1.6           | Java 6                           |
| JDK 1.7           | **Java 7**                       |
| JDK 1.8           | **Java 8**                       |
| Java 9 起         | 直接用数字，9、10、11、17、21... |

> **常见误区**：JDK 1.7 ≠ Java 17，JDK 1.8 ≠ Java 18。1.x 命名体系在 Java 8 后被废弃。

---

### 各版本核心新特性

#### Java 5（JDK 1.5）— 2004

- **泛型（Generics）**：`List<String>`，编译期类型检查
- **注解（Annotations）**：`@Override`、`@Deprecated`、自定义注解
- **自动装箱/拆箱**：`int` ↔ `Integer` 自动转换
- **枚举（Enum）**：`enum Color { RED, GREEN, BLUE }`
- **可变参数（Varargs）**：`void foo(String... args)`
- **增强 for 循环**：`for (String s : list)`
- **静态导入**：`import static java.lang.Math.PI`

#### Java 6（JDK 1.6）— 2006

- 性能优化（JIT 编译器改进）
- `JAXB`、`JAX-WS` 内置支持
- `Compiler API`（`javax.tools`）
- `HttpServer` 内置轻量级 HTTP 服务器

#### Java 7（JDK 1.7）— 2011

- **钻石操作符**：`List<String> list = new ArrayList<>()`
- **switch 支持 String**
- **try-with-resources**：自动关闭资源（实现 `AutoCloseable`）
- **多异常捕获**：`catch (IOException | SQLException e)`
- **NIO 2（java.nio.file）**：`Path`、`Files`、`WatchService`
- 数字字面量可加下划线：`1_000_000`
- 二进制字面量：`0b1010`

#### Java 8（JDK 1.8）— 2014 ⭐ 最重要的版本之一

- **Lambda 表达式**：`(x, y) -> x + y`
- **函数式接口**：`@FunctionalInterface`，`Function`、`Predicate`、`Consumer`、`Supplier`
- **Stream API**：链式操作集合，`filter`、`map`、`reduce`、`collect`
- **Optional**：优雅处理 null，避免 NPE
- **方法引用**：`String::toUpperCase`
- **默认方法（default method）**：接口可以有默认实现
- **新日期时间 API**：`LocalDate`、`LocalDateTime`、`ZonedDateTime`（替代 `Date`/`Calendar`）
- **CompletableFuture**：更强大的异步编程
- **Nashorn JS 引擎**（Java 15 移除）

#### Java 9 — 2017

- **模块系统（JPMS）**：`module-info.java`，模块化 JAR
- **JShell**：交互式 REPL
- **接口私有方法**：接口中可以定义 `private` 方法
- **集合工厂方法**：`List.of()`、`Set.of()`、`Map.of()`（不可变）
- **改进的 Stream**：`takeWhile`、`dropWhile`、`iterate`
- **改进的 Optional**：`ifPresentOrElse`、`stream()`
- HTTP/2 Client（孵化阶段）

#### Java 10 — 2018

- **局部变量类型推断**：`var list = new ArrayList<String>()`
- G1 GC 并行 Full GC
- `List.copyOf()`、`Map.copyOf()`

#### Java 11（LTS）— 2018 ⭐ 长期支持版本

- **`var` 用于 Lambda 参数**：`(var x, var y) -> x + y`
- **字符串新方法**：`strip()`、`isBlank()`、`lines()`、`repeat()`
- **HTTP Client 正式版**：支持 HTTP/1.1 和 HTTP/2
- 直接运行单文件：`java Hello.java`
- `Files.readString()`、`Files.writeString()`
- 移除 Java EE 和 CORBA 模块

#### Java 12 — 2019

- **Switch 表达式（预览）**：`switch` 可以作为表达式返回值
- `String.indent()`、`String.transform()`

#### Java 13 — 2019

- **文本块（预览）**：三引号多行字符串 `""" ... """`

#### Java 14 — 2020

- **Switch 表达式（正式）**
- **`instanceof` 模式匹配（预览）**：`if (obj instanceof String s)`
- **Record（预览）**
- **NullPointerException 精确信息**：明确告诉你哪个变量是 null

#### Java 15 — 2020

- **文本块（正式）**
- **密封类（预览）**：`sealed class`

#### Java 16 — 2021

- **`instanceof` 模式匹配（正式）**
- **Record（正式）**：`record Point(int x, int y) {}`，不可变数据类

#### Java 17（LTS）— 2021 ⭐ 当前主流 LTS 版本

- **密封类（正式，Sealed Classes）**：`sealed interface Shape permits Circle, Rectangle`
- **模式匹配增强**
- 移除实验性 AOT/JIT 编译器
- 强封装 JDK 内部 API

#### Java 18 — 2022

- **默认 UTF-8 编码**
- Simple Web Server（命令行快速启动 HTTP 服务器）
- `@snippet` Javadoc 标签

#### Java 19 — 2022

- **虚拟线程（预览，Project Loom）**
- **结构化并发（孵化）**

#### Java 21（LTS）— 2023 ⭐ 最新 LTS 版本

- **虚拟线程（正式，Virtual Threads）**：轻量级线程，百万级并发，替代线程池模型
- **结构化并发（预览）**
- **序列集合（Sequenced Collections）**：`SequencedCollection`、`getFirst()`、`getLast()`
- **Record 模式（正式）**：`case Point(int x, int y) ->`
- **Switch 模式匹配（正式）**：switch 支持类型模式、null 分支
- **字符串模板（预览）**

---

### LTS 版本（重点关注）

| 版本    | 发布年份 | 支持至              |
| ------- | -------- | ------------------- |
| Java 8  | 2014     | 2030（Oracle 付费） |
| Java 11 | 2018     | 2026                |
| Java 17 | 2021     | 2029                |
| Java 21 | 2023     | 2031                |

> 生产环境推荐使用 LTS 版本。当前新项目建议 **Java 17** 或 **Java 21**。
