# Course Alignment — 课程概念映射

> 将 mini-build-system 各模块与经典构建系统文档、学术论文的核心概念进行映射

---

## 1. GNU Make Manual 映射 (make_engine)

### GNU Make Manual (v4.4) 核心章节

| 章节 | GNU Make 概念 | 对应模块 | 实现 |
|------|-------------|---------|------|
| Ch 2: An Introduction to Makefiles | 规则基本格式 | `make_engine.h` | `MakeRule`, `make_parse` |
| Ch 3: Writing Makefiles | Makefile 结构 | `make_engine.c` | 解析器主干 |
| Ch 4: Writing Rules | 规则语法、.PHONY | `make_parse` | 规则行/命令行解析 |
| Ch 4.6: Phony Targets | 伪目标 | `make_build` | `TARGET_PHONY` 处理 |
| Ch 6: How to Use Variables | 变量定义与引用 | `make_resolve_vars` | `$(VAR)` 展开 |
| Ch 10: Using Implicit Rules | 隐式规则 | `make_match_pattern` | `%` 模式匹配 |
| Ch 10.5: Automatic Variables | 自动变量 | `make_expand_auto_vars` | `$@`, `$<` 替换 |
| Ch 4.14: Double-Colon Rules | 多规则目标 | (未来扩展) | — |
| Ch 8: Functions for Transforming Text | 函数调用 | (未来扩展) | — |

### 关键差异

| 特性 | GNU Make | mini-make |
|------|----------|-----------|
| 变量风味 | Recursive (`=`) / Simple (`:=`) / Conditional (`?=`) | 仅 Recursive (`=`) |
| 自动变量全集 | `$@ $< $^ $? $* $% $+` | `$@ $<` |
| 隐式规则链 | 多步链式推理 | 单步模式匹配 |
| VPATH | 源文件目录搜索 | 不支持 |
| 条件指令 | `ifeq/ifneq/ifdef/ifndef` | 不支持 |
| include | Makefile 分片 | 不支持 |
| MAKEFLAGS | 运行时选项传递 | 不支持 |

---

## 2. Ninja Design Document 映射 (ninja_graph)

### Ninja 设计原则

| 原则 | 描述 | 对应实现 |
|------|------|---------|
| Minimalism | 最小语法集，易于生成 | `ninja_parse` — 仅 3 种语句 |
| Explicitness | 所有依赖显式声明 | `NinjaEdge` — 完全显式图 |
| Speed | 亚秒级启动 | 直接图结构，无解析开销 |
| Parallelism | 原生并行 | `ninja_schedule` + 关键路径 |
| Logging | .ninja_log 增量 | (计划: CacheEntry 可扩展) |

### 论文映射

Martin, Evan. "The Ninja build system." (2011)

| 论文概念 | 描述 | 代码位置 |
|---------|------|---------|
| "Build graph as DAG" | 构建图为有向无环图 | `NinjaBuild.nodes/edges` |
| "Dirty bit propagation" | 脏位自底向上传播 | `ninja_compute_dirty` |
| "Log-structured state" | 日志结构状态 | `.ninja_log` ↔ `CacheEntry` |
| "Restat optimization" | 输出未变则跳过下游 | `NINJA_NODE_RESTAT` 状态 |
| "Critical path scheduling" | 关键路径优先级调度 | `ninja_compute_critical_path` |
| "Depfile integration" | 编译器生成依赖 | `implicit_inputs` + depfile 机制 |
| "Console pool" | 串行化控制台输出 | (未来: pool 概念) |

### Ninja 文件格式规范映射

```
build.ninja ::= (declaration)*
declaration  ::= rule | build | default | variable
rule         ::= 'rule' NAME '\n' (INDENT binding)*
build        ::= 'build' outputs ':' rulename inputs ('|' implicit)? ('||' order_only)?
variable     ::= NAME '=' value
```

对应解析器状态机:
- `rule` → 记录当前 rule 名
- `build` → `parse_build_line`
- `default` → 记录 default_targets
- `name = value` → `set_build_var`
- `  name = value` → 若在 rule 内: `rule.name = value`

---

## 3. Buck / Bazel 概念映射

### Buck (Meta, 2013)

| Buck 概念 | 描述 | 对应模块/概念 |
|-----------|------|-------------|
| Build Rule | 声明式构建规则 | `MakeRule` / `NinjaEdge` |
| Build File (BUCK) | Python-like DSL | (可映射为 Makefile 解析) |
| Target Graph | 目标依赖图 | `DepGraph` |
| Action Graph | 构建动作图 | Task Scheduler 中 `BuildTask` |
| RuleKey | 内容哈希键 | `CacheEntry.key` (incremental.c) |
| Cache | 构建缓存 | `BuildCache` (incremental.c) |
| Buck-out | 输出隔离目录 | `cache_dir` |
| Supported Languages | 多语言支持 | 通过规则名区分 |

### Bazel (Google, 2015)

| Bazel 概念 | 描述 | 映射 |
|------------|------|------|
| Workspace | 项目根 | — |
| BUILD file | 构建描述（Starlark） | `MakeFile` 数据结构 |
| Target | 构建目标 | `MakeTarget` |
| Action | 最小构建单元 | `BuildTask` |
| Skyframe | 增量评估框架 | `DepGraph` 拓扑 + 缓存 |
| Action Cache | 动作缓存 | `BuildCache` |
| Remote Execution | 远程构建执行 | (未来: 分布式调度) |
| Hermetic Builds | 封闭构建 | `CacheEntry.key` 内容哈希 |
| Input Root | 输入根 | — |
| Output Base | 输出基目录 | `cache_dir` |

### Skyframe 映射

Bazel 的核心增量引擎 Skyframe 与我们实现的对应关系：

```
Skyframe Component        mini-build-system Component
─────────────────────────────────────────────────────
SkyKey                   → CacheEntry.key (哈希)
SkyValue                 → CacheEntry (缓存结果)
SkyFunction              → Build Rule (MakeRule/NinjaEdge)
NodeEntry                → DepNode 或 MakeTarget
Dirty Tracking           → tracker_detect_changes
Invalidation             → cache_invalidate
Incremental Evaluation   → make_build (递归重新构建)
```

### 内容哈希 vs 时间戳

```
Make (传统):
  修改时间戳比较 → 可能误判（无关修改触发重建）
  
Ninja (混合):
  时间戳 + 命令哈希 → 减少误判
  
Buck/Bazel (内容哈希):
  RuleKey = hash(inputs + command + env) → 最精确
  实现位置: cache_compute_key() in incremental.c
```

---

## 4. 增量构建理论

### 核心问题

给定构建图 G = (V, E) 和修改的文件集合 F，找出需要重新执行的最小构建任务集合 T ⊆ V。

### 算法分类

| 算法 | 适用场景 | 实现 |
|------|---------|------|
| Make-up-to-date | 基于时间戳 | `make_target_is_dirty` |
| Ninja-dirty-prop | 脏标记传播 | `ninja_compute_dirty` |
| Bazel-Skyframe | 增量评估 | (设计参考 dep_graph + incremental) |

### 正确性条件

一个增量构建算法正确，当且仅当：
1. 所有输入改变的任务都被重建（完备性）
2. 没有不必要的任务被重建（最小性）
3. 构建结果与全量构建一致（正确性）

---

## 5. 调度理论

### 问题定义

给定一组任务，每个任务有依赖约束和预估执行时间，在 p 个并行处理器上调度任务，最小化总完工时间（makespan）。

### 关键概念

| 概念 | 公式 | 实现 |
|------|------|------|
| Critical Path | max(路径执行时间和) | `dep_compute_critical_path` |
| Level Width | 每层任务数 | `dep_parallel_schedule` |
| Makespan | max(完成时间) | `ts->current_time` |
| Speedup | T_serial / T_parallel | 调度器统计 |
| Efficiency | Speedup / p | 并行效率 |

### 调度策略对比

| 策略 | 描述 | 位置 |
|------|------|------|
| Serial (串行) | 任一顺序逐个执行 | `sched_run_serial` |
| Round-Robin (轮转) | 每轮取 max_parallel 个就绪任务 | `sched_run_parallel` |
| CP-priority (关键路径优先) | 按关键路径降序 | `ninja_schedule` |
| Level-by-Level (分层) | 按拓扑层并行 | `dep_parallel_schedule` |

---

## 6. 课程知识交叉矩阵

```
            ┌──────────────┐
            │  make_engine  │  规则解析、变量展开
            └──────┬───────┘
                   │ 生成构建任务
                   ▼
    ┌──────────────────────────────┐
    │         dep_graph            │  依赖解析、拓扑排序、并行分层
    └──────┬───────────────────────┘
           │          │
           ▼          ▼
    ┌──────────┐  ┌──────────────┐
    │ ninja_graph│  │incremental   │  图表示、脏标记、关键路径
    └──────┬───┘  └──────┬───────┘
           │              │
           ▼              ▼
    ┌──────────────────────────┐
    │    task_scheduler        │  任务调度、并行执行
    └──────────────────────────┘
```

---

## 7. 推荐阅读

1. **GNU Make Manual** — Richard Stallman, Roland McGrath — 第 2-4, 6, 10 章
2. **The Ninja Build System** — Evan Martin, 2011 — 设计原则
3. **Build Systems a la Carte** — Mokejima et al., 2018 — 构建系统理论框架
4. **Bazel: Correct, Reproducible, Fast Builds** — Google, 2015+ — Skyframe 增量评估
5. **Recursive Make Considered Harmful** — Peter Miller, 1997 — 单一 Makefile 优于递归 Make
6. **A Formal Framework for Build Systems** — ICSME 2022 — 形式化模型
7. **Algorithms for Scheduling Tasks with Precedence Constraints** — Introduction to Algorithms, Ch 27 — 调度理论基础

---

> 本课程设计覆盖构建系统从解析到执行的完整链路，可独立运行每个模块进行实验
