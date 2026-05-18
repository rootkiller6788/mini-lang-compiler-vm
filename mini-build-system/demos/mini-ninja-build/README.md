# Ninja Build — 显示依赖图构建系统

> 参考 Ninja 设计文档，实现 .ninja 格式解析、依赖图维护、脏标记传播与关键路径调度

---

## 1. 概述

Ninja 是 Google 设计的专注于构建速度的构建系统。与 Make 不同，Ninja 的构建文件（build.ninja）由更高级的构建系统（如 CMake、GN）生成，而非手工编写。

`ninja_graph` 模块实现了 Ninja 风格的构建图核心：

- `.ninja` 格式解析
- 节点/边图模型
- 脏标记传播算法
- 关键路径分析与调度
- 模拟构建执行

### Ninja 哲学

> Make 设计哲学：用户编写构建规则，Make 推断依赖关系
> Ninja 设计哲学：依赖关系是显式的，构建文件是机器生成的

---

## 2. 核心数据结构

### NinjaNode

```c
typedef struct {
    char           path[256];          // 文件路径
    NinjaNodeType  type;               // FILE / RULE / PHONY
    time_t         mtime;              // 修改时间
    NinjaDirtyState dirty;             // clean / dirty / restat
    bool           exists;             // 文件是否存在
    int            critical_path;      // 关键路径长度
} NinjaNode;
```

### NinjaEdge

```c
typedef struct {
    char  rule_name[128];              // 构建规则名
    char  inputs[16][256];             // 显式输入
    int   num_inputs;
    char  implicit_inputs[8][256];     // 隐式依赖
    int   num_implicit;
    char  order_only_deps[8][256];     // 仅顺序依赖
    int   num_order_only;
    char  outputs[1][256];             // 构建输出
    char  variables[128][2][256];      // 规则变量
    int   num_vars;
} NinjaEdge;
```

### NinjaBuild

```c
typedef struct {
    NinjaNode nodes[256];              // 所有节点
    int       num_nodes;
    NinjaEdge edges[256];              // 所有边
    int       num_edges;
    char      default_targets[8][256]; // 默认目标
    int       num_default_targets;
} NinjaBuild;
```

---

## 3. .ninja 文件语法

### 变量声明

```ninja
# 顶层变量
cxx = g++
cflags = -Wall -O2
outdir = build
```

### Rule 声明

```ninja
rule cxx
  command = $cxx -c $cflags $in -o $out
  description = CXX $out
  depfile = $out.d

rule link
  command = $cxx $ldflags -o $out $in $libs
  description = LINK $out
```

### Build 声明

```ninja
build hello.obj: cxx hello.cpp | hello.h
build main.obj: cxx main.cpp
build hello: link hello.obj main.obj

default hello
```

### 语法要素

| 元素 | 含义 | 示例 |
|------|------|------|
| `rule name` | 定义构建规则 | `rule cxx` |
| `build output: rule inputs` | 定义构建边 | `build a.o: cxx a.cpp` |
| `\| implicit_inputs` | 隐式依赖 | `\| config.h` |
| `\|\| order_only_inputs` | 仅顺序依赖 | `\|\| dir/` |
| `default target` | 默认构建目标 | `default hello` |
| `$var` | 变量引用 | `$cxx $in -o $out` |

---

## 4. 依赖类型详解

### 显式依赖 (explicit inputs)

```ninja
build foo.o: cxx foo.cpp
```

`foo.cpp` 是显式输入。如果 `foo.cpp` 修改了，`foo.o` 必须重新编译。这些输入决定构建命令的 `$in` 变量。

### 隐式依赖 (implicit inputs)

```ninja
build foo.o: cxx foo.cpp | foo.h bar.h
```

`foo.h` 和 `bar.h` 是隐式输入。它们也触发重新构建，但不出现在 `$in` 变量中。适用于头文件依赖。

### 顺序依赖 (order-only dependencies)

```ninja
build out: rule in || dir/
```

`dir/` 是仅顺序依赖。它确保目标目录先创建，但不会因为目录修改而重新构建输出。

### 语义对比

| 类型 | 触发重新构建 | 出现在 $in | 生成依赖文件 |
|------|-------------|------------|-------------|
| 显式 | yes | yes | yes |
| 隐式 | yes | no | yes |
| 顺序 | no | no | no |

---

## 5. 脏标记传播

`ninja_compute_dirty()` 实现自底向上的脏标记传播：

```
算法: Compute Dirty

1. 重置所有节点为 CLEAN
2. 检查每个节点的文件存在性
3. 对于每条边:
   a. 如果输出文件不存在 → DIRTY
   b. 如果任一显式输入是 DIRTY → 输出标记 DIRTY
   c. 如果任一隐式输入是 DIRTY → 输出标记 DIRTY
   d. 顺序依赖不参与传播
4. 脏标记沿依赖边向上传播（迭代直到不动点）
```

### 日志式构建 (Log-structured)

Ninja 使用 `.ninja_log` 记录上次构建的命令行与输出：

```
# ninja_log format
start_time end_time mtime output_hash command_hash
```

通过比较命令哈希（而非文件时间戳）判断是否需要重新构建，比 Make 的基于时间戳更精确：

- Make 问题：头文件注释修改 → 时间戳更新 → 不必要的重新编译
- Ninja 方案：命令未变 → 即使输入时间戳变化也不重新构建

---

## 6. 关键路径调度

### 关键路径定义

在依赖图中，从源文件到最终目标的最长路径（按时间权重）即为关键路径。关键路径决定了理论最短构建时间。

### 计算算法

```
compute_critical_path(node):
  if visited[node]: return node.critical_path
  if visiting[node]: return 0  (cycle detected)
  
  marking visiting
  max_cp = 1
  for each edge where node is an input:
    out = edge.output
    cp = compute_critical_path(out) + 1
    max_cp = max(max_cp, cp)
  mark visited
  node.critical_path = max_cp
  return max_cp
```

### 应用

- 关键路径长度 = 最小并行构建时间（理论下限）
- 按关键路径降序调度可最大化并行效率
- 识别构建瓶颈

---

## 7. 与 Make 的对比

| 维度 | GNU Make | Ninja |
|------|----------|-------|
| **文件格式** | Makefile (手工编写友好) | build.ninja (机器生成) |
| **依赖模型** | 隐式推断 + 显式规则 | 完全显式 |
| **构建图** | 按规则匹配动态构建 | 完全预计算图 |
| **并行构建** | 通过 -j 参数 | 原生并行支持 |
| **变更检测** | 文件时间戳 | 命令哈希 + 时间戳 |
| **语言特性** | 变量、函数、条件 | 最小化语法 |
| **使用场景** | 手工项目 | 生成器后端 |
| **启动速度** | 中等（需解析 Makefile） | 极快 |
| **构建文件** | ~100-1000 行（手工） | ~10000+ 行（生成） |
| **增量构建** | 基于时间戳 | 基于内容哈希 |

---

## 8. 工作流程

### 完整构建流程

```
1. 上层工具 (CMake/GN) 生成 build.ninja
2. ninja 解析 build.ninja → NinjaBuild 图
3. ninja 从磁盘读取 .ninja_log
4. 计算脏节点 (ninja_compute_dirty)
5. 计算关键路径 (ninja_compute_critical_path)
6. 按优先级构建脏节点
7. 更新 .ninja_log
8. 输出构建结果
```

### 增量构建流程

```
1. 用户修改源文件
2. 运行 ninja (无参数)
3. 读取 .ninja_log 上次命令
4. 比较当前命令哈希与日志
5. 仅重新编译受影响的文件
6. 链接最终目标
```

---

## 9. API 参考

| 函数 | 描述 |
|------|------|
| `ninja_parse(nb, filepath)` | 解析 .ninja 文件 |
| `ninja_compute_dirty(nb)` | 计算脏节点 |
| `ninja_schedule(nb)` | 按关键路径生成调度方案 |
| `ninja_execute(nb, target)` | 模拟构建目标 |
| `ninja_print_graph(nb)` | 打印构建图 |
| `ninja_find_node(nb, path)` | 查找节点索引 |
| `ninja_add_node(nb, path, type)` | 添加节点 |
| `ninja_find_edge(nb, output)` | 按输出查找边 |
| `ninja_compute_critical_path(nb)` | 计算所有节点关键路径 |
| `ninja_lookup_var(nb, name)` | 查找构建变量 |

### 常量

| 常量 | 值 | 描述 |
|------|------|------|
| `NINJA_MAX_NODES` | 256 | 最大节点数 |
| `NINJA_MAX_EDGES` | 256 | 最大边数 |
| `NINJA_MAX_INPUTS` | 16 | 每条边最大显式输入 |
| `NINJA_MAX_IMPLICIT` | 8 | 每条边最大隐式输入 |
| `NINJA_MAX_ORDER_ONLY` | 8 | 每条边最大顺序依赖 |
| `NINJA_MAX_LINE` | 512 | 最大行长度 |
| `NINJA_MAX_VARS` | 128 | 最大构建变量数 |

---

## 10. 构建变量

### 内置变量

| 变量 | 含义 | 范围 |
|------|------|------|
| `$in` | 显式输入列表 | build 命令 |
| `$out` | 输出文件列表 | build 命令 |
| `$in_newline` | 换行分隔的输入 | build 命令 |
| `$out_newline` | 换行分隔的输出 | build 命令 |
| `$depfile` | 依赖文件名 | build 命令 |

### Rule 内变量

每个 `rule` 定义可以包含多个变量（如 `command`、`description`、`depfile` 等），在 `build` 边中使用 `$rulename.var` 引用。

---

## 11. 去重与重素 (Depfile)

### Depfile 格式

```make
# 由编译器生成，记录实际使用的头文件
hello.o: hello.cpp hello.h util.h common.h
```

`depfile` 机制使 Ninja 能够：
1. 首次构建时生成 depfile
2. 后续构建时读取 depfile 获取精确头文件依赖
3. 头文件修改时触发最小化重新编译

### 与 GCC 集成

```bash
g++ -c -MD -MF hello.o.d hello.cpp -o hello.o
```

`-MD -MF` 参数生成包含所有 `#include` 头文件的 depfile。

---

## 12. 完整示例

### build.ninja

```ninja
cxx = g++
cflags = -Wall -O2 -std=c++17
ldflags =

rule cxx
  command = $cxx -c $cflags $in -o $out
  description = CXX $out

rule link
  command = $cxx $ldflags -o $out $in
  description = LINK $out

build hello.obj: cxx hello.cpp | hello.h common.h
build main.obj: cxx main.cpp | main.h common.h
build hello: link hello.obj main.obj

default hello
```

### 解析后的图

```
Nodes:
  [0] hello.cpp   (FILE)
  [1] hello.h     (FILE)
  [2] common.h    (FILE)
  [3] main.cpp    (FILE)
  [4] main.h      (FILE)
  [5] hello.obj   (FILE)
  [6] main.obj    (FILE)
  [7] hello       (FILE)

Edges:
  cxx:     hello.obj <- [hello.cpp]  implicit:[hello.h, common.h]
  cxx:     main.obj  <- [main.cpp]   implicit:[main.h, common.h]
  link:    hello     <- [hello.obj, main.obj]
```

---

## 13. 限制与改进方向

### 当前限制

1. 命令不实际执行（模拟打印）
2. 不支持 depfile 读取
3. 不支持 .ninja_log 持久化
4. 单个边仅支持一个输出
5. 不支持 pool（资源限制）

### 改进方向

1. 真实命令执行
2. `.ninja_log` 读写
3. `depfile` 集成
4. 多输出支持
5. Pool / 资源约束集成
6. Console pool 支持

---

> 参考 Ninja 设计文档 (https://ninja-build.org/manual.html) 与 Evan Martin 的 "The Ninja build system" 论文
