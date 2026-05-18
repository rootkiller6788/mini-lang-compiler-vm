# Build System Architecture — 构建系统架构

> 对比分析 GNU Make / Ninja / Buck / Bazel 四种构建系统的架构设计、核心算法与适用场景

---

## 1. 构建系统分类

### 按图构建方式分类

```
构建系统
├── Task-based (任务驱动)
│   └── Make, Ant, Rake, Grunt
│       特点: 用户定义任务，系统推断依赖
│       优点: 灵活、直观
│       缺点: 依赖推断不完整
│
├── Graph-based (图驱动)
│   └── Ninja, MSBuild
│       特点: 完全预计算依赖图
│       优点: 极快、精确
│       缺点: 构建文件庞大
│
└── Artifact-based (产物驱动)
    └── Buck, Bazel, Pants, Please
        特点: 内容哈希、封闭构建
        优点: 正确性保证、远程缓存
        缺点: 复杂度高
```

### 按评估模型分类

| 模型 | 代表 | 特点 |
|------|------|------|
| Restarting | Make | 每次从零构建目标列表 |
| Incremental | Excel, Bazel Skyframe | 仅重新评估变化部分 |
| Suspending | Shake (Haskell) | 挂起/恢复计算状态 |

---

## 2. GNU Make: 基于文件的构建

### 架构

```
Makefile (文本)
    │
    ▼
┌─────────────┐
│ 解析器       │  → 规则 + 变量表
└──────┬──────┘
       │
       ▼
┌─────────────┐
│ 目标图构建   │  → 目标名 → 规则映射
└──────┬──────┘
       │
       ▼
┌─────────────┐
│ 时间戳检查   │  → 文件 stat()
└──────┬──────┘
       │
       ▼
┌─────────────┐
│ 递归执行     │  → system(shell_command)
└─────────────┘
```

### 核心算法: 基于时间戳的脏检查

```
函数: needs_rebuild(target)
  如果 目标文件不存在: 返回 true
  目标时间 = mtime(target)
  对于每个前置条件:
    如果 mtime(前置) > 目标时间: 返回 true
    如果 needs_rebuild(前置): 返回 true
  返回 false
```

### 优点
- 文件格式简单，手工编写友好
- 变量机制强大
- 模式规则减少重复
- 历史悠久，生态成熟

### 缺点
- 时间戳不精确（触碰到即重建）
- 隐式规则链可能导致意外行为
- 大项目性能差（O(n*m) 递归）
- 并行构建存在竞态条件风险
- 非封闭构建（依赖系统环境）

### 性能特征

| 规模 | 文件数 | 解析时间 | 启动时间 |
|------|--------|---------|---------|
| 小 | <100 | <10ms | <100ms |
| 中 | 100-1000 | 50-200ms | 200-500ms |
| 大 | 1000-10000 | 500ms-2s | 2-10s |

---

## 3. Ninja: 显式图 + 极简语法

### 架构

```
GN / CMake (生成器)
    │
    ▼
build.ninja (机器生成的文本)
    │
    ▼
┌──────────────────┐
│ 图解析器          │  → 少量 token 解析
└────────┬─────────┘
         │
         ▼
┌──────────────────┐
│ NinjaBuild 图    │  → 节点/边 DAG
└────────┬─────────┘
         │
         ▼
┌──────────────────┐
│ .ninja_log 加载  │  → 上次构建状态
└────────┬─────────┘
         │
         ▼
┌──────────────────┐
│ 脏标记传播        │  → 命令哈希比较
└────────┬─────────┘
         │
         ▼
┌──────────────────┐
│ 并行调度 & 执行   │  → 关键路径优先
└──────────────────┘
```

### 核心算法: 日志结构的脏检测

```
加载 .ninja_log:
  log[output] = (mtime, cmd_hash)

判断是否需要重建 output:
  如果 output 不在 log 中: 需要重建
  如果 mtime(output) != log[output].mtime: 需要重建
  如果 hash(cmd) != log[output].cmd_hash: 需要重建
  如果 任一输入 dirty: 需要重建
  否则: 不需要重建
```

### 优点
- 亚秒级冷启动
- 精确的变更检测（命令哈希）
- 原生高效并行
- 易于作为编译后端

### 缺点
- 构建文件不适宜手写
- 需要上层生成器（增加工具链复杂度）
- 语言表达能力有限
- 无跨项目缓存共享

---

## 4. Buck / Bazel: 内容哈希 + 封闭构建

### Bazel 架构

```
WORKSPACE + BUILD files
    │
    ▼
┌─────────────────────┐
│ Skyframe 评估引擎    │
│ (增量依赖图评估)      │
└────────┬────────────┘
         │
    ┌────┴────┐
    ▼         ▼
 Action 图  Action 缓存
    │         │
    ▼         ▼
┌─────────────────────┐
│ 远程执行 (REAPI)     │
│ + 远程缓存            │
└─────────────────────┘
```

### 核心算法: 内容寻址缓存

```
RuleKey = hash(
  rule_type,
  hash(inputs_content),
  hash(command_line),
  hash(environment_vars),
  hash(toolchain)
)

查找缓存:
  if action_cache.contains(RuleKey):
    return action_cache[RuleKey]
  else:
    result = execute(action)
    action_cache[RuleKey] = result
    return result
```

### Skyframe: 增量评估框架

```
数据模型:
  SkyKey    → 唯一标识 (如: "//src:hello.o")
  SkyValue  → 评估结果 (如: 编译产出)
  SkyFunction → 输入→输出的纯函数

评估过程:
  1. 请求 SkyKey 的 SkyValue
  2. 检查 SkyValue 是否已缓存且干净
  3. 如果干净 → 返回缓存值
  4. 否则 → 调用 SkyFunction 重新计算
  5. SkyFunction 可能请求其他 SkyKey (递归)
  6. 记录每个 SkyFunction 的所有依赖
  7. 下次某个输入改变时，仅重新评估受影响节点
```

### 优点
- 内容哈希消除误判重建
- 封闭构建保证可重现性
- 远程缓存大幅加速 CI
- 跨项目增量

### 缺点
- 概念复杂度极高
- BUILD 文件迁移成本高
- 需要显式声明所有依赖
- 学习曲线陡峭
- I/O 沙箱有性能开销

---

## 5. 缓存策略比较

### 缓存层级

```
Level 1: 本地文件系统缓存
  ├── Make: 目标文件即缓存
  ├── Ninja: .ninja_log 记录上次状态
  └── Bazel: outputBase/ 下的 action cache

Level 2: 本地共享缓存
  └── Bazel: --disk_cache=/path/to/cache

Level 3: 远程缓存
  └── Bazel: --remote_cache=grpc://server

Level 4: 远程执行
  └── Bazel: --remote_executor=grpc://server
```

### 缓存键设计

| 系统 | 键组成 | 可重现性 |
|------|--------|---------|
| Make | target → mtime 隐式判断 | 低 |
| Ninja | output → cmd_hash + mtime | 中 |
| Buck | target → RuleKey(hash of inputs) | 高 |
| Bazel | Action → hash(inputs, cmd, env, tools) | 高 |

### 内容哈希优势

```
场景: 修改头文件注释
  Make:  检查 mtime → 头文件更新 → 重新编译所有依赖
  Ninja: 检查 mtime + cmd_hash → 头文件 mtime 更新
         但 cmd_hash 未变 → fork gcc → gcc 重新检查
         头文件 → gcc 重新读取 → 输出相同 → 不触发下游
  Bazel: RuleKey = hash(头文件内容) → 内容未变
         → RuleKey 相同 → 缓存命中 → 0 秒
```

---

## 6. 依赖图分析算法

### 拓扑排序

```
Kahn's Algorithm:
  1. 计算所有节点入度
  2. 入度为 0 的节点入队
  3. while 队列非空:
       a. 出队节点，输出到结果
       b. 对其每个后继: 入度-1
       c. 如果后继入度为 0: 入队
  4. 如果输出 < 总节点数: 存在环

DFS-Based (Tarjan 变体):
  1. 对每个未访问节点做 DFS
  2. 递归处理所有后继
  3. 节点及其子树完成后入栈
  4. 栈的逆序 = 拓扑序
```

### 环检测

```
DFS 三色标记:
  WHITE: 未访问
  GRAY:  正在访问 (在递归栈中)
  BLACK: 访问完成

  如果在 DFS 中遇到 GRAY 节点 → 检测到环
```

### 关键路径算法

```
定义: CP(v) = 如果 v 无后继: 1
             否则: 1 + max(CP(u) for u in successors(v))

应用: 按 CP 降序调度任务 → 理论最优并行效率
```

### 并行调度

```
分层算法:
  1. BFS 计算每个节点的层级
     level(root) = 0
     level(v) = max(level(前驱)) + 1
  2. 同一层级的任务可并行执行
  3. 层级间必须串行

优点: 简单
缺点: 未考虑任务执行时间差异
```

---

## 7. 性能模型

### 构建时间组成

```
T_total = T_parse + T_dirty_check + T_execute

T_parse:
  - Make: O(n) 行数
  - Ninja: O(n) 行数 (更轻量)
  - Bazel: O(n) 目标数 (需加载传递依赖)

T_dirty_check:
  - Make: O(D) 每个目标的依赖数，最坏 O(V*E)
  - Ninja: O(V+E) 一次遍历
  - Bazel: O(Δ) 仅变化部分 (Skyframe 增量)

T_execute:
  - 受关键路径限制: T_execute ≥ critical_path_length
  - 受并行度限制: T_execute ≥ total_work / parallelism
```

### 加速比分析

```
Speedup = T_1 / T_p

理想: Speedup = P (线性加速)
实际限制:
  - 关键路径: Speedup ≤ T_1 / CP
  - 负载不均衡
  - I/O 竞争
  - 依赖解析开销

典型值:
  - 编译任务: 2-8x (CPU 密集 + 大量 I/O)
  - 测试任务: 1.5-3x (I/O 为主)
```

---

## 8. 设计模式

### 访问者模式 — 图遍历

```c
typedef void (*GraphVisitor)(DepNode *node, void *ctx);

void dep_visit_all(DepGraph *dg, GraphVisitor visitor, void *ctx) {
    for (int i = 0; i < dg->num_nodes; i++) {
        visitor(&dg->nodes[i], ctx);
    }
}
```

### 策略模式 — 调度策略

```c
typedef void (*ScheduleStrategy)(TaskScheduler *ts);

// 可在运行时选择不同调度策略
ScheduleStrategy strategies[] = {
    sched_run_serial,
    sched_run_parallel,
    NULL  // 可扩展
};
```

### 观察者模式 — 文件变更跟踪

```c
typedef void (*ChangeCallback)(const char *filepath, time_t mtime, void *ctx);

void tracker_watch(BuildTracker *bt, ChangeCallback cb, void *ctx);
```

### 命令模式 — 构建动作

```c
typedef struct {
    char *command;
    int (*execute)(void *ctx);
} BuildAction;
```

---

## 9. 未来方向

### 分布式构建
- 远程执行服务 (REAPI/gRPC)
- 内容寻址存储 (CAS)
- 构建任务分片

### 增量解析
- 部分重新评估 (Skyframe-like)
- 变化影响分析
- 依赖图差分

### 高级缓存
- 分布式内容缓存
- 构建结果共享
- CI/CD 流水线加速

### 工具链集成
- 编译器包装器
- 自动依赖提取
- 交叉编译支持

---

## 10. 模块关系图

```
         ┌──────────────┐
         │  make_engine  │   规则解析与变量展开
         └──────┬───────┘
                │ 提供 MakeRule/Target
                ▼
      ┌──────────────────┐
      │    dep_graph     │   依赖解决、拓扑排序、环检测
      └────────┬─────────┘
               │ 提供排序后的执行顺序
               ▼
   ┌───────────────────────┐
   │   task_scheduler      │   任务调度与并行执行
   └───────┬───────────────┘
           │
     ┌─────┴─────┐
     ▼           ▼
┌─────────┐ ┌────────────┐
│ ninja_  │ │incremental │  Ninja 图模型 │ 增量缓存
│ graph   │ │            │
└─────────┘ └────────────┘
```

---

> 构建系统设计是系统工程、图论、缓存策略与并行计算的交叉领域，本项目提供了从理论到实践的完整实验平台
