# AI技术知识汇总

> This file is created by model **claude sonnet 4.6**

## 1. MCP, Skills, Function Calling 的区别

### MCP (Model Context Protocol)

- **定义**: Anthropic开发的开放协议，用于连接AI助手和外部数据源/工具
- **作用**: 让LLM能够安全、标准化地访问外部系统（数据库、API、文件系统等）
- **特点**:
  - 基于客户端-服务器架构
  - MCP服务器暴露资源（数据）、工具（操作）和提示（模板）
  - 客户端（如Claude）通过统一协议与多个MCP服务器通信
  - 类似于IDE的语言服务器协议（LSP），但用于AI上下文
- **例子**: 连接GitHub、Slack、数据库等外部服务

### Skills

- **定义**: 预定义的、可重用的任务模板或工作流
- **作用**: 封装常见任务的执行逻辑（如/commit、/review-pr等）
- **特点**:
  - 用户可通过 /skill-name 快速调用
  - 内部可能调用多个function calls
  - 是高层次的任务抽象
  - 可自定义和扩展
- **例子**: /commit（创建git提交）、/review-pr（审查PR）

### Function Calling (工具调用)

- **定义**: LLM直接调用预定义函数/工具的能力
- **作用**: 让LLM能够执行具体操作（读文件、搜索、运行命令等）
- **特点**:
  - 是最底层的能力
  - 每个function对应一个具体操作
  - LLM根据上下文决定调用哪个function
  - 参数由LLM生成
- **例子**: Read（读文件）、Bash（执行命令）、Grep（搜索）

### 三者关系

```text
Skills（高层任务）
  └─> Function Calling（具体操作）
        └─> MCP（外部系统连接）

层次关系：Skills > Function Calling > MCP
```

**举例说明**:

- 用户执行 `/commit`（skill）
- Skill内部调用多个Function Calls：`Bash('git status')`、`Read(file)`、`Bash('git commit')`
- 如果需要访问GitHub，Function可能通过MCP服务器连接GitHub API

---

## 2. Agent 设计的总体流程和框架

### 核心架构

```text
用户输入 → Agent调度器 → 任务规划 → 工具选择与执行 → 结果整合 → 输出
           ↑                                              ↓
           └──────────── 反馈循环/ReAct机制 ───────────────┘
```

### 详细流程

#### 2.1 输入处理阶段

- **意图识别**: 解析用户需求，识别任务类型
- **上下文加载**: 加载历史对话、记忆、项目上下文
- **任务分解**: 将复杂任务拆解为子任务

#### 2.2 计划与推理阶段（ReAct模式）

Pay Attention: **ReAct = Reasoning + Acting**

```shell
While 任务未完成:
    Thought: 思考当前状态和下一步
    Action: 选择并执行工具
    Observation: 观察结果
    Reflection: 反思是否需要调整策略
```

#### 2.3 工具选择与执行

- **工具库**: 维护可用工具清单（读文件、搜索、运行代码等）
- **参数生成**: 根据上下文生成工具调用参数
- **执行管理**:
  - 并行执行独立操作
  - 串行执行有依赖的操作
  - 错误处理与重试

#### 2.4 记忆系统

- **短期记忆**: 当前会话上下文
- **长期记忆**: 持久化的知识和偏好
- **工作记忆**: 任务执行过程中的临时状态

#### 2.5 核心组件设计

```java
// 伪代码示例
public class Agent {
    private LLM llm;                    // 大语言模型
    private ToolRegistry toolRegistry;   // 工具注册表
    private Memory memory;               // 记忆系统
    private TaskPlanner planner;         // 任务规划器
    private ExecutionEngine executor;    // 执行引擎

    public String run(String userInput) {
        // 1. 加载上下文
        Context context = memory.loadContext();

        // 2. 规划任务
        Plan plan = planner.createPlan(userInput, context);

        // 3. ReAct循环
        while (!plan.isComplete()) {
            // Thought: LLM推理
            Reasoning reasoning = llm.reason(plan.getCurrentState());

            // Action: 选择工具
            ToolCall toolCall = reasoning.getNextAction();

            // Execute: 执行工具
            Result result = executor.execute(toolCall);

            // Observation: 更新状态
            plan.updateState(result);

            // 检查是否需要调整计划
            if (result.requiresReplanning()) {
                plan = planner.replan(plan, result);
            }
        }

        // 4. 整合结果
        return plan.getFinalOutput();
    }
}
```

### 关键设计模式

1. **责任链模式**: 工具调用链
2. **策略模式**: 不同任务类型使用不同策略
3. **观察者模式**: 状态变化通知
4. **工厂模式**: 动态创建子Agent

### 多Agent协作

```text
主Agent
  ├─> 代码探索Agent（Explore）
  ├─> 计划Agent（Plan）
  ├─> 测试运行Agent（Test Runner）
  └─> 通用Agent（General Purpose）
```

---

## 3. RAG（检索增强生成）详解

### 3.1 什么是RAG

Pay Attention: **RAG = Retrieval-Augmented Generation**

- 结合检索系统和生成模型
- 解决LLM知识过时、幻觉、领域知识不足问题

### 3.2 RAG核心流程

```text
用户查询
  ↓
1. 查询预处理（Query Preprocessing）
  - 查询重写/扩展
  - 关键词提取
  ↓
2. 向量化（Embedding）
  - 将查询转换为向量
  ↓
3. 检索召回（Retrieval）
  - 向量数据库检索（相似度搜索）
  - 关键词检索（BM25等）
  - 混合检索（Hybrid Search）
  ↓
4. 重排序（Reranking）
  - 使用更精确的模型重新排序
  - 计算相关性分数
  ↓
5. 上下文构建（Context Building）
  - 选择Top-K结果
  - 格式化为Prompt
  ↓
6. LLM生成（Generation）
  - 基于检索内容生成答案
  ↓
7. 后处理（Post-processing）
  - 引用标注
  - 答案验证
  ↓
输出结果
```

### 3.3 详细实现

#### 数据准备阶段

```java
// 1. 文档切片（Chunking）
public class DocumentChunker {
    public List<Chunk> chunk(Document doc, int chunkSize, int overlap) {
        // 策略：
        // - 固定大小切分（每500 tokens）
        // - 句子边界切分（保持语义完整）
        // - 段落切分
        // - 语义切分（基于主题）

        List<Chunk> chunks = new ArrayList<>();
        // 滑动窗口切分，overlap防止信息断裂
        for (int i = 0; i < doc.length(); i += chunkSize - overlap) {
            chunks.add(new Chunk(doc.substring(i, i + chunkSize)));
        }
        return chunks;
    }
}

// 2. 向量化
public class EmbeddingService {
    public float[] embed(String text) {
        // 调用Embedding模型（如OpenAI text-embedding-3、
        // BGE、M3E等）将文本转换为向量
        return embeddingModel.encode(text); // 返回768/1536维向量
    }
}

// 3. 存储到向量数据库
public class VectorStore {
    private ElasticsearchClient esClient;

    public void store(String id, String text, float[] vector, Map<String, Object> metadata) {
        IndexRequest request = new IndexRequest.Builder()
            .index("documents")
            .id(id)
            .document(Map.of(
                "text", text,
                "vector", vector,
                "metadata", metadata
            ))
            .build();
        esClient.index(request);
    }
}
```

#### 查询阶段

```java
@Service
public class RAGService {
    @Autowired
    private EmbeddingService embeddingService;

    @Autowired
    private VectorSearchService vectorSearch;

    @Autowired
    private RerankerService reranker;

    @Autowired
    private LLMService llmService;

    public Mono<String> query(String userQuery) {
        return Mono.just(userQuery)
            // 1. 查询向量化
            .map(query -> embeddingService.embed(query))

            // 2. 向量检索（召回）
            .flatMap(queryVector -> vectorSearch.search(queryVector, 20))
            // 返回Top-20候选

            // 3. 重排序（精排）
            .flatMap(candidates -> reranker.rerank(userQuery, candidates, 5))
            // 选出Top-5最相关

            // 4. 构建Prompt
            .map(topResults -> buildPrompt(userQuery, topResults))

            // 5. LLM生成答案
            .flatMap(prompt -> llmService.generate(prompt))

            // 6. 后处理
            .map(response -> addCitations(response, topResults));
    }

    private String buildPrompt(String query, List<Document> docs) {
        StringBuilder sb = new StringBuilder();
        sb.append("基于以下参考资料回答问题：\n\n");
        for (int i = 0; i < docs.size(); i++) {
            sb.append(String.format("[%d] %s\n\n", i+1, docs.get(i).getText()));
        }
        sb.append("问题：").append(query).append("\n");
        sb.append("请基于上述资料回答，并标注引用来源。");
        return sb.toString();
    }
}
```

### 3.4 为什么选用WebFlux？

**WebFlux优势**:

1. **非阻塞I/O**: RAG涉及多次网络调用（向量DB、LLM API），异步处理提高吞吐量
2. **背压处理**: 控制数据流速，防止下游过载
3. **资源高效**: 少量线程处理大量并发请求
4. **响应式编程**: 管道式处理流（查询→检索→重排→生成）非常自然

**典型场景**:

```java
// 场景：同时查询多个数据源
public Mono<List<Document>> hybridSearch(String query) {
    Mono<List<Document>> vectorResults = vectorDB.search(query);
    Mono<List<Document>> keywordResults = esKeywordSearch(query);
    Mono<List<Document>> graphResults = knowledgeGraph.search(query);

    // 并行执行，合并结果
    return Mono.zip(vectorResults, keywordResults, graphResults)
        .map(tuple -> mergeAndDeduplicate(tuple.getT1(), tuple.getT2(), tuple.getT3()));
}
```

**替代方案**:

- 简单场景可用同步Spring MVC + 多线程
- 但RAG的多步异步调用链更适合响应式

### 3.5 提升召回精度的方法

#### 问题诊断

召回率低的原因：

1. 向量语义失配
2. 文档切分不当
3. 检索策略单一
4. Query表达不清晰

#### 解决方案

**A. 优化Embedding模型**:

```java
// 1. 使用领域特化模型
// 通用：OpenAI text-embedding-3, BGE
// 中文：M3E, text2vec
// 代码：CodeBERT, GraphCodeBERT

// 2. Fine-tune Embedding模型
// 使用自己的数据微调，提升领域适配性
```

**B. 混合检索（Hybrid Search）**:

```java
public class HybridRetriever {
    public List<Document> retrieve(String query) {
        // 向量检索（语义相似）
        List<Document> vectorResults = vectorSearch(query, 50);

        // 关键词检索（精确匹配）
        List<Document> keywordResults = bm25Search(query, 50);

        // 融合：RRF (Reciprocal Rank Fusion)
        return fuseResults(vectorResults, keywordResults);
    }

    private List<Document> fuseResults(List<Document> list1, List<Document> list2) {
        // RRF算法
        Map<String, Double> scores = new HashMap<>();
        int k = 60; // 常量

        for (int i = 0; i < list1.size(); i++) {
            String docId = list1.get(i).getId();
            scores.merge(docId, 1.0 / (k + i + 1), Double::sum);
        }

        for (int i = 0; i < list2.size(); i++) {
            String docId = list2.get(i).getId();
            scores.merge(docId, 1.0 / (k + i + 1), Double::sum);
        }

        // 按分数排序并返回
        return scores.entrySet().stream()
            .sorted(Map.Entry.<String, Double>comparingByValue().reversed())
            .map(entry -> getDocById(entry.getKey()))
            .collect(Collectors.toList());
    }
}
```

**C. Query优化**:

```java
// 查询重写
public class QueryRewriter {
    public List<String> rewrite(String query) {
        List<String> queries = new ArrayList<>();
        queries.add(query); // 原始查询

        // 1. 查询扩展（添加同义词）
        queries.add(expandWithSynonyms(query));

        // 2. 子查询拆分
        queries.addAll(splitIntoSubQueries(query));

        // 3. HyDE (Hypothetical Document Embeddings)
        // 让LLM生成假设性答案，用该答案去检索
        String hypotheticalAnswer = llm.generate("请回答：" + query);
        queries.add(hypotheticalAnswer);

        return queries;
    }
}
```

**D. 文档切分优化**:

```java
// 智能切分策略
public class SmartChunker {
    public List<Chunk> chunk(Document doc) {
        // 1. 语义切分（而非固定大小）
        List<Section> sections = detectSections(doc); // 按主题分段

        // 2. 保留结构信息
        for (Section section : sections) {
            section.setMetadata(Map.of(
                "title", extractTitle(section),
                "parent", doc.getTitle(),
                "level", section.getLevel()
            ));
        }

        // 3. Overlap处理
        // 确保前后chunk有上下文连续性
        return addOverlap(sections, 50); // 50 tokens overlap
    }
}
```

**E. 重排序（Reranking）**:

```java
// 使用Cross-Encoder模型精排
public class Reranker {
    private CrossEncoderModel model; // 如bge-reranker-large

    public List<Document> rerank(String query, List<Document> candidates, int topK) {
        // Cross-Encoder同时编码query和doc
        // 比向量点积更准确，但更慢
        List<ScoredDocument> scored = candidates.stream()
            .map(doc -> new ScoredDocument(
                doc,
                model.score(query, doc.getText())
            ))
            .sorted(Comparator.comparingDouble(ScoredDocument::getScore).reversed())
            .limit(topK)
            .collect(Collectors.toList());

        return scored.stream()
            .map(ScoredDocument::getDocument)
            .collect(Collectors.toList());
    }
}
```

**F. 多路召回**:

```java
public class MultiRecallStrategy {
    public List<Document> recall(String query) {
        // 路径1: 密集向量检索
        List<Document> denseResults = denseVectorSearch(query);

        // 路径2: 稀疏向量检索（SPLADE）
        List<Document> sparseResults = sparseVectorSearch(query);

        // 路径3: BM25关键词
        List<Document> bm25Results = bm25Search(query);

        // 路径4: 知识图谱
        List<Document> graphResults = knowledgeGraphSearch(query);

        // 融合
        return ensembleFusion(denseResults, sparseResults, bm25Results, graphResults);
    }
}
```

**G. 评估与迭代**:

```python
# 评估指标
def evaluate_rag(test_set):
    metrics = {
        'recall@k': [],      # 召回率：Top-K中包含正确答案的比例
        'precision@k': [],   # 精确率
        'mrr': [],           # Mean Reciprocal Rank
        'ndcg': [],          # Normalized Discounted Cumulative Gain
        'answer_quality': [] # 生成答案质量（人工评估）
    }

    for query, ground_truth in test_set:
        results = rag_system.retrieve(query, k=10)
        metrics['recall@k'].append(calculate_recall(results, ground_truth))
        # ... 计算其他指标

    return metrics
```

---

## 4. Elasticsearch 与向量存储

### 4.1 Elasticsearch 是什么？

**定义**: 分布式搜索和分析引擎

- 基于Apache Lucene构建
- 擅长全文检索、日志分析、实时搜索
- 支持RESTful API

**核心概念**:

- **Index（索引）**: 类似数据库的表
- **Document（文档）**: JSON格式的数据记录
- **Inverted Index（倒排索引）**: 关键词到文档的映射

### 4.2 ES与RAG的关系

**ES在RAG中的角色**:

1. **关键词检索**: BM25算法进行精确匹配
2. **向量存储**: ES 8.0+支持dense_vector类型
3. **混合检索**: 同时支持关键词+向量检索

### 4.3 ES向量存储实现

#### 创建索引

```json
// 定义索引mapping
PUT /knowledge_base
{
  "mappings": {
    "properties": {
      "text": {
        "type": "text",
        "analyzer": "ik_max_word"  // 中文分词器
      },
      "text_vector": {
        "type": "dense_vector",
        "dims": 768,               // 向量维度（根据Embedding模型）
        "index": true,
        "similarity": "cosine"     // 相似度算法：cosine/dot_product/l2_norm
      },
      "metadata": {
        "properties": {
          "source": { "type": "keyword" },
          "timestamp": { "type": "date" },
          "category": { "type": "keyword" }
        }
      }
    }
  },
  "settings": {
    "index": {
      "knn": true,  // 启用k-NN搜索
      "knn.algo_param.ef_construction": 100  // HNSW参数
    }
  }
}
```

#### Java代码实现

```java
@Service
public class ElasticsearchVectorStore {

    @Autowired
    private ElasticsearchClient esClient;

    @Autowired
    private EmbeddingService embeddingService;

    // 存储文档及其向量
    public void indexDocument(String id, String text, Map<String, Object> metadata)
            throws IOException {
        // 1. 生成向量
        float[] vector = embeddingService.embed(text);

        // 2. 构建文档
        Map<String, Object> document = new HashMap<>();
        document.put("text", text);
        document.put("text_vector", vector);
        document.put("metadata", metadata);

        // 3. 索引到ES
        IndexRequest<Map<String, Object>> request = IndexRequest.of(i -> i
            .index("knowledge_base")
            .id(id)
            .document(document)
        );

        esClient.index(request);
    }

    // 向量检索
    public Mono<List<SearchResult>> vectorSearch(String query, int topK) {
        return Mono.fromCallable(() -> {
            // 1. 查询向量化
            float[] queryVector = embeddingService.embed(query);

            // 2. k-NN搜索
            SearchRequest request = SearchRequest.of(s -> s
                .index("knowledge_base")
                .knn(k -> k
                    .field("text_vector")
                    .queryVector(queryVector)
                    .k(topK)
                    .numCandidates(100)  // 候选数量
                )
                .source(src -> src.filter(f -> f
                    .includes("text", "metadata")
                ))
            );

            SearchResponse<Document> response = esClient.search(request, Document.class);

            // 3. 解析结果
            return response.hits().hits().stream()
                .map(hit -> new SearchResult(
                    hit.source().getText(),
                    hit.score(),
                    hit.source().getMetadata()
                ))
                .collect(Collectors.toList());
        }).subscribeOn(Schedulers.boundedElastic()); // WebFlux异步执行
    }

    // 混合检索（关键词 + 向量）
    public Mono<List<SearchResult>> hybridSearch(String query, int topK) {
        return Mono.fromCallable(() -> {
            float[] queryVector = embeddingService.embed(query);

            SearchRequest request = SearchRequest.of(s -> s
                .index("knowledge_base")
                .query(q -> q
                    .bool(b -> b
                        // 关键词匹配（BM25）
                        .should(sh -> sh
                            .match(m -> m
                                .field("text")
                                .query(query)
                            )
                        )
                    )
                )
                // 向量匹配（k-NN）
                .knn(k -> k
                    .field("text_vector")
                    .queryVector(queryVector)
                    .k(topK)
                    .numCandidates(100)
                )
                .size(topK)
            );

            SearchResponse<Document> response = esClient.search(request, Document.class);

            return response.hits().hits().stream()
                .map(hit -> new SearchResult(
                    hit.source().getText(),
                    hit.score(),  // ES自动融合BM25和向量分数
                    hit.source().getMetadata()
                ))
                .collect(Collectors.toList());
        }).subscribeOn(Schedulers.boundedElastic());
    }
}
```

### 4.4 ES向量存储的底层原理

**HNSW算法 (Hierarchical Navigable Small World)**:

- ES使用HNSW构建近似最近邻（ANN）索引
- 分层图结构，查询复杂度O(log N)
- Trade-off: 精确度 vs 速度

**参数调优**:

```json
{
  "settings": {
    "index.knn": true,
    "index.knn.algo_param.ef_construction": 200,  // 构建时精度（越大越准，越慢）
    "index.knn.algo_param.m": 16                 // 每层连接数
  }
}
```

查询时参数:

```java
.knn(k -> k
    .numCandidates(100)  // 候选数量（越大召回越高，但越慢）
    .k(10)               // 返回Top-K
)
```

### 4.5 ES vs 专用向量数据库

| 特性 | Elasticsearch | Pinecone/Milvus/Weaviate |
| ------ | -------------- | -------------------------- |
| 向量检索 | 支持（8.0+） | 原生支持，性能更优 |
| 关键词检索 | 强大（BM25） | 较弱或不支持 |
| 混合检索 | 原生支持 | 需额外集成 |
| 规模 | 数百万-千万级 | 亿级+ |
| 运维 | 生态成熟 | 相对较新 |
| 适用场景 | 中小规模，需混合检索 | 大规模纯向量检索 |

**选择建议**:

- **已有ES集群** → 直接用ES（降低运维成本）
- **大规模纯向量检索** → 专用向量DB（Milvus/Qdrant）
- **需要复杂过滤** → ES的filter能力更强

### 4.6 完整RAG架构示例

```java
@RestController
@RequestMapping("/api/rag")
public class RAGController {

    @Autowired
    private RAGService ragService;

    @PostMapping(value = "/query", produces = MediaType.TEXT_EVENT_STREAM_VALUE)
    public Flux<String> query(@RequestBody QueryRequest request) {
        return ragService.query(request.getQuery())
            // 流式返回（类似ChatGPT打字效果）
            .flatMapMany(answer -> Flux.fromArray(answer.split(" ")))
            .delayElements(Duration.ofMillis(50)); // 模拟流式输出
    }
}

@Service
public class AdvancedRAGService {

    public Mono<String> query(String userQuery) {
        return Mono.just(userQuery)
            // Stage 1: 查询理解
            .flatMap(this::queryUnderstanding)

            // Stage 2: 多路召回
            .flatMap(this::multiRecall)

            // Stage 3: 重排序
            .flatMap(docs -> rerank(userQuery, docs))

            // Stage 4: 上下文压缩（去冗余）
            .flatMap(this::contextCompression)

            // Stage 5: 生成答案
            .flatMap(context -> generateAnswer(userQuery, context))

            // Stage 6: 答案自我验证
            .flatMap(this::selfVerification);
    }

    // 查询理解：意图识别、实体提取、查询重写
    private Mono<EnhancedQuery> queryUnderstanding(String query) {
        return llmService.analyze(query)
            .map(analysis -> new EnhancedQuery(
                query,
                analysis.getIntent(),
                analysis.getEntities(),
                analysis.getRewrittenQueries()
            ));
    }

    // 多路召回
    private Mono<List<Document>> multiRecall(EnhancedQuery query) {
        Mono<List<Document>> dense = denseVectorSearch(query.getOriginal());
        Mono<List<Document>> sparse = sparseVectorSearch(query.getOriginal());
        Mono<List<Document>> keyword = bm25Search(query.getOriginal());

        // 对重写的查询也进行检索
        List<Mono<List<Document>>> rewrittenSearches = query.getRewrittenQueries()
            .stream()
            .map(this::denseVectorSearch)
            .collect(Collectors.toList());

        // 合并所有结果
        return Flux.merge(dense, sparse, keyword)
            .concatWith(Flux.merge(rewrittenSearches))
            .collectList()
            .map(this::deduplicateAndMerge);
    }

    // 上下文压缩
    private Mono<String> contextCompression(List<Document> docs) {
        // 使用小模型提取每个doc中与query相关的句子
        // 减少无关信息，降低LLM输入token数
        return Flux.fromIterable(docs)
            .flatMap(doc -> extractRelevantSentences(doc))
            .collectList()
            .map(sentences -> String.join("\n", sentences));
    }

    // 答案自我验证
    private Mono<String> selfVerification(String answer) {
        // 让LLM检查答案是否基于检索内容，是否有幻觉
        return llmService.verify(answer)
            .map(verification -> {
                if (verification.hasHallucination()) {
                    return regenerateAnswer(); // 重新生成
                }
                return answer;
            });
    }
}
```

---

## 5. 高级RAG技术

### 5.1 GraphRAG

结合知识图谱：

```text
文档 → 实体提取 → 关系抽取 → 知识图谱
                                    ↓
用户查询 → 图谱查询 + 向量检索 → 综合结果 → LLM生成
```

### 5.2 Adaptive RAG

根据查询复杂度动态选择策略：

- 简单查询 → 直接向量检索
- 中等复杂 → 混合检索 + 重排序
- 高复杂度 → 多跳推理（迭代检索）

### 5.3 Self-RAG

让LLM自己决定：

- 是否需要检索？
- 检索结果是否相关？
- 答案是否支持？

---

## 6. 总结对比

### MCP vs Skills vs Function Calling

| 维度 | MCP | Skills | Function Calling |
| ------ | ----- | -------- | ------------------ |
| 层次 | 基础设施层 | 应用层 | 能力层 |
| 作用 | 连接外部系统 | 封装任务工作流 | 执行具体操作 |
| 例子 | GitHub连接器 | /commit | Bash('ls') |
| 扩展性 | 协议标准化 | 用户可自定义 | 开发者预定义 |

### RAG关键技术栈总结

```go
数据层:      Elasticsearch/Milvus/Pinecone (向量存储)
            Postgres/MySQL (结构化数据)

检索层:      Embedding模型 (OpenAI/BGE/M3E)
            Reranker模型 (bge-reranker)

生成层:      LLM (GPT-4/Claude/文心一言)

框架层:      LangChain/LlamaIndex (RAG框架)
            Spring WebFlux (响应式后端)

评估层:      RAGAS (RAG评估框架)
```

### 提升召回精度的优先级

1. **混合检索** (快速提升20-30%) ← 最优先
2. **重排序** (提升10-20%)
3. **Query优化** (提升10-15%)
4. **文档切分优化** (提升5-10%)
5. **Fine-tune Embedding** (提升5-15%，成本高)

---

## 参考资源

- **MCP官方文档**: <https://modelcontextprotocol.io/>
- **RAG论文**: "Retrieval-Augmented Generation for Knowledge-Intensive NLP Tasks"
- **Elasticsearch向量搜索**: <https://www.elastic.co/guide/en/elasticsearch/reference/current/dense-vector.html>
- **LangChain RAG指南**: <https://python.langchain.com/docs/use_cases/question_answering/>
- **RAGAS评估框架**: <https://github.com/explodinggradients/ragas>
