# Overview Of DDD

## DDD 架构与 MVC 架构的对比分析

### 初步理解

对于传统的单体项目，如果采用 MVC 架构设计，无非是 `Controller → Service → DAO`。

对于 DDD 架构：

- 将 Controller 抽离出来，形成单独的代码模块 → **trigger** 层
- Model + Service 形成 → **domain** 层
- DAO 层形成 → **infrastructure** 层

各个模块之间相互独立，减少相互依赖和耦合设计。

---

### 关键纠正：依赖方向

#### ❌ 错误理解

各模块相互独立，互不依赖

#### ✅ 正确理解

**外层依赖内层，内层不依赖外层**：

**依赖方向：** `trigger → domain → infrastructure`

```java
trigger 依赖 domain（调用 Service）
domain 依赖 infrastructure（调用 Repository）
但 domain 不能依赖 trigger！
但 infrastructure 不能依赖 domain！
```

**为什么这样设计？**

- **domain 是核心业务**，应该纯粹，不关心外部怎么调用（HTTP/RPC/MQ）
- **infrastructure 只负责数据存取**，不关心业务规则

---

### DDD 和 MVC 的本质区别

| 对比维度       | 传统 MVC                   | DDD 架构                         |
| -------------- | -------------------------- | -------------------------------- |
| **设计思路**   | 以技术分层为中心           | **以业务领域为中心**             |
| **Service 层** | 简单调用 DAO，贫血模型     | 包含业务规则、领域逻辑，充血模型 |
| **复用性**     | Controller 和 Service 耦合 | domain 可被多种 trigger 复用     |
| **扩展性**     | 加功能改 Service           | 加功能在 domain 聚合根里扩展     |

---

### 贫血模型 vs 充血模型

#### 传统 MVC（贫血模型）

```java
// User.java - 只有 getter/setter
public class User {
    private String name;
    private Integer age;
    // 只有 getter/setter，没有业务逻辑
}

// UserService.java
public void updateUser(User user) {
    // 业务逻辑散落在 Service 里
    if (user.getAge() < 0 || user.getAge() > 150) {
        throw new RuntimeException("年龄不合法");
    }
    userDao.update(user);
}
```

**问题：**

- 实体只是数据容器，没有行为
- 业务逻辑全在 Service 里，容易变成事务脚本
- 相同的校验逻辑可能散落各处

---

#### DDD（充血模型）

```java
// UserEntity.java - 包含业务逻辑
public class UserEntity {
    private String name;
    private Integer age;

    // 领域逻辑封装在实体内
    public void changeAge(Integer newAge) {
        if (newAge < 0 || newAge > 150) {
            throw new AppException("年龄不合法");
        }
        this.age = newAge;
    }

    public void changeName(String newName) {
        if (StringUtils.isBlank(newName)) {
            throw new AppException("名称不能为空");
        }
        this.name = newName;
    }
}

// UserService.java - 只负责编排
public void updateUser(UserEntity entity) {
    entity.changeAge(25);  // 业务规则在 entity 里
    entity.changeName("张三");
    repository.save(entity);
}
```

**优势：**

- 业务逻辑内聚在领域对象中
- Service 只负责编排，不包含业务规则
- 更符合面向对象设计原则

---

### DDD 的核心价值

#### 不仅仅是"拆分模块"

```java
传统 MVC 问题：
✗ Service 层变成事务脚本（一堆 CRUD）
✗ 业务逻辑散落各处，难维护
✗ 实体只是数据容器（贫血模型）

DDD 解决方案：
✓ domain 层用领域模型承载业务规则
✓ Entity、ValueObject、Aggregate 封装领域逻辑
✓ Service 只负责编排，不包含业务逻辑
✓ 以业务领域为中心建模，而不是以数据库表为中心
```

---

### 在 ai-agent-station 项目中的体现

#### trigger 层（AiAgentController）

```java
@RestController
@RequestMapping("/api/v1/ai/agent/")
public class AiAgentController implements IAiAgentService {
    @Resource
    private IAiAgentChatService aiAgentChatService;

    @RequestMapping(value = "chat_agent", method = RequestMethod.GET)
    public Response<String> chatAgent(@RequestParam("aiAgentId") Long aiAgentId,
                                      @RequestParam("message") String message) {
        // 只负责接收请求，转发给 domain
        String content = aiAgentChatService.aiAgentChat(aiAgentId, message);
        return Response.success(content);
    }
}
```

**职责：**

- 接收 HTTP 请求
- 转发给 domain 层
- 返回结果

---

#### domain 层（AiAgentChatService）

```java
@Service
public class AiAgentChatService implements IAiAgentChatService {

    public String aiAgentChat(Long aiAgentId, String message) {
        // 复杂的业务编排：
        // 1. 加载 Agent 配置
        AiClientModelVO modelConfig = repository.queryModelConfig(aiAgentId);

        // 2. 组装提示词
        String systemPrompt = buildSystemPrompt(aiAgentId);

        // 3. 整合 MCP 工具
        List<Tool> mcpTools = loadMcpTools(aiAgentId);

        // 4. 调用 OpenAI API
        ChatResponse response = chatClient.call(
            new Prompt(message, systemPrompt, mcpTools)
        );

        // 5. 处理结果
        return response.getResult().getOutput().getContent();
    }
}
```

**职责：**

- 包含复杂的业务编排
- 调用 AI API
- 整合各种配置和工具

---

#### infrastructure 层（DAO）

```java
@Mapper
public interface IAiClientModelDao {
    // 只负责数据访问
    AiClientModel queryModelById(Long id);
    int insert(AiClientModel model);
    int update(AiClientModel model);
}
```

**职责：**

- 数据的增删改查
- 不包含任何业务逻辑

---

### 依赖关系可视化

```java
┌─────────────────────────────────────┐
│         trigger 层                   │
│   (Controller/Job/Listener)         │
│   - 接收外部请求                     │
│   - 参数校验、格式转换               │
└──────────────┬──────────────────────┘
               │ 依赖（调用 Service）
               ↓
┌─────────────────────────────────────┐
│         domain 层                    │
│   (Service/Entity/ValueObject)      │
│   - 核心业务逻辑                     │
│   - 领域模型、业务规则               │
└──────────────┬──────────────────────┘
               │ 依赖（调用 Repository）
               ↓
┌─────────────────────────────────────┐
│      infrastructure 层               │
│   (DAO/PO/Redis)                    │
│   - 数据访问                         │
│   - 缓存操作                         │
└─────────────────────────────────────┘
```

**反向依赖是被禁止的：**

- ❌ domain 不能 `import` trigger 包的类
- ❌ infrastructure 不能 `import` domain 包的类

---

### 特殊情况：Admin Controller 为什么直接调用 DAO？

```java
@RestController
@RequestMapping("/api/v1/ai/admin/client/tool/mcp/")
public class AiAdminClientToolMcpController {
    @Resource
    private IAiClientToolMcpDao aiClientToolMcpDao; // 直接注入 DAO！

    public ResponseEntity<List<AiClientToolMcp>> queryMcpList(...) {
        List<AiClientToolMcp> mcpList = aiClientToolMcpDao.queryMcpList(...);
        return ResponseEntity.ok(mcpList);
    }
}
```

**为什么可以这样？**

- 后台管理接口（Admin）是**简单的 CRUD 操作**
- 不需要复杂的业务逻辑
- 遵循"够用就好"的原则，避免过度设计
- 如果为每个 CRUD 都创建一个 Service，反而增加维护成本

**这是 DDD 的灵活性体现：不是教条式套用，而是根据实际需求调整。**

---

### 总结：DDD ≠ 简单的模块拆分

| 维度         | 你的理解  | 完整理解                                                     |
| ------------ | --------- | ------------------------------------------------------------ |
| **模块拆分** | ✅ 正确   | Controller → trigger，Service → domain，DAO → infrastructure |
| **依赖关系** | ❌ 不准确 | 不是"相互独立"，而是"单向依赖"（外层依赖内层）               |
| **核心价值** | 📌 未提及 | 以**领域为中心建模**，充血模型，业务逻辑内聚                 |
| **设计思想** | 📌 未提及 | 不是以数据库表为中心，而是以业务领域为中心                   |

**DDD 三大支柱：**

1. **分层隔离** - 你理解到了 ✅
2. **单向依赖** - 需要补充 ⚠️
3. **领域建模** - 这是核心 ⚠️

---

### 快速判断是否需要 DDD

**适合 DDD：**

- 业务逻辑复杂
- 需要多种触发方式（HTTP/MQ/RPC）
- 团队大，需要明确边界

**不适合 DDD：**

- 简单的 CRUD 项目
- 业务逻辑少
- 快速原型开发
