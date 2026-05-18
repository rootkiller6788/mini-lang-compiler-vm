# Mini Register Allocator — 寄存器分配器

> 参考 CMU 15-745, Appel "Modern Compiler Implementation in C" Ch 11, Muchnick "Advanced Compiler Design" Ch 16

## 概述 Overview

寄存器分配器将无限多的虚拟寄存器映射到有限数量的物理机器寄存器上。当物理寄存器不足时，必须将部分虚拟寄存器溢出 (spill) 到内存栈。

本项目实现两种经典寄存器分配算法：
1. **线性扫描 (Linear Scan)** — 快速、O(n log n)，常用于 JIT 编译器
2. **图着色 (Graph Coloring)** — 基于 Chaitin/Briggs 框架，质量更高

## 核心概念 Core Concepts

### 活跃区间 Live Intervals

活跃区间表示一个虚拟寄存器从定义位置到最后一次使用位置的范围。两个活跃区间如果重叠 (overlap)，则它们不能占用同一个物理寄存器。

```
v0: [0=====4]
v1:    [2=======8]
v2:       [4=========10]
v3:          [6==========12]

时间轴: 0--1--2--3--4--5--6--7--8--9--10--11--12--13--14--15
```

上图中，v0 和 v1 在 [2,4] 区间重叠，因此它们需要不同的物理寄存器。v0 和 v2 在 [4] 处仅在点 4 重叠，也需要不同寄存器。v3 在 [6,12] 独立，可以与 v0 或 v1 共用寄存器。

### 线性扫描 Linear Scan (Poletto & Sarkar, 1999)

线性扫描算法步骤：

1. 将所有活跃区间按起始位置排序
2. 维护一个"活跃列表" active list，记录当前活跃且已分配寄存器的区间
3. 遍历每个区间：
   - 从 active list 中移除已结束的区间 (expire)
   - 如果 active list 未满 (空余寄存器)，分配一个新寄存器
   - 如果 active list 已满，需要溢出 (spill) 一个区间

**溢出策略**: 选择活跃区间中结束最晚的 (furthest end) 进行溢出。因为溢出当前区间意味着它需要从内存加载/存储，如果被溢出的区间结束更晚，那么它的溢出代价更大——所以我们溢出结束更晚的，让当前 (较早结束的) 获得寄存器。

```
线性扫描伪代码:

-线性扫描伪代码 (Linear Scan Pseudocode)
for interval in sorted_intervals:
    expire_old_intervals(active, interval.start)
    if active.length < R:
        reg = allocate_reg()
        active.add(interval, reg)
    else:
        spill_candidate = active.max_by_end()
        if spill_candidate.end > interval.end:
            spill(spill_candidate)
            reg = spill_candidate.reg
            active.remove(spill_candidate)
            active.add(interval, reg)
        else:
            spill(interval)
```

### 图着色 Graph Coloring (Chaitin, 1981; Briggs, 1994)

图着色将寄存器分配问题建模为图着色问题：

1. **构建干扰图 (Build Interference Graph)**
   - 节点 = 虚拟寄存器
   - 边 = 两个虚拟寄存器同时活跃 (即无法共用物理寄存器)
   - 如果两个区间在任何点重叠，则添加一条边

2. **简化 (Simplify)**
   - 反复移除度数 < k 的节点 (k = 物理寄存器数量)
   - 这些节点压入栈中
   - 如果所有剩余节点度数 >= k，需要进行溢出决策

3. **溢出试探 (Spill Heuristic)**
   - 选择一个节点标记为溢出
   - 将其从图中移除，继续简化过程
   - 常用启发式: 选择 spill cost / degree 比值最小的节点
   - spill cost = (uses + defs) × 10^depth / degree

4. **选择 (Select)**
   - 按栈弹出顺序为节点分配颜色 (寄存器)
   - 每个节点选择邻居未使用的最小颜色编号

5. **可能溢出 (Potential Spill)**
   - 如果某个节点无法分配颜色，标记为实际溢出
   - 溢出的变量需要在每次使用前从内存加载 (reload)，定义后存储到内存 (spill)

**Briggs 优化**: 在简化阶段，Briggs 观察到即使节点度数 >= k，如果存在度数 < k 的邻居，该节点在邻居删除后度数会降低，可以推迟溢出决策。这被称为"乐观着色" (Optimistic Coloring)。

```
图着色流程:

- 图着色流程 (Graph Coloring Pipeline)
Build → Simplify → (Spill?) → Select → (Spill?)

Build:    构建干扰图 G = (V, E), V=虚拟寄存器, E=冲突
Simplify: while 存在 v ∈ V, degree(v) < k:
              push(v, stack), remove v from G
Select:   while stack not empty:
              v = pop(stack)
              分配颜色 c, 使得 c ∉ colors(neighbors(v))
              如果分配失败 → 标记 v 为溢出
```

### 图着色 vs 线性扫描

| 特性 | 线性扫描 | 图着色 |
|------|---------|--------|
| 时间复杂度 | O(n log n) | O(n²) 到 O(n³) |
| 分配质量 | 中等 (more spills) | 高 (fewer spills) |
| 实现复杂度 | 简单 (~200 LOC) | 复杂 (~800 LOC) |
| 典型应用 | JIT (V8, HotSpot C1) | AOT (GCC, LLVM) |
| 溢出决策 | 贪心 (最远结束) | 基于代价模型 |

### Spill Code Generation 溢出代码生成

当变量被溢出到栈上时，编译器必须在每次使用/定义该变量的地方插入内存操作：

**定义点 (Def) 溢出**
```
store r_temp, [rbp - spill_slot]    ; 将定义存入栈
```

**使用点 (Use) 重载**
```
load r_temp, [rbp - spill_slot]     ; 从栈重新加载
```

溢出代码插入的位置通常在：
- 基本块入口 (对于跨基本块的全局寄存器分配)
- 每个定义/使用点之后/之前 (简单方法)

### Coalescing 合并

**合并 (Coalescing)**: 消除不必要的寄存器复制。当两个虚拟寄存器通过 mov 指令关联，且它们的活跃区间不重叠时，可以将它们合并为同一个寄存器，从而消除 mov。

```
mov v1, v2   ; 如果 v1 和 v2 不重叠，可以删除这条指令
```

**Chaitin 合并**: 保守合并 — 只有当合并后新节点的度数 < k (物理寄存器数量) 时才合并，确保不会引入新的溢出。

**Briggs 合并**: 更积极 — 只要合并后新节点的邻居中度数 >= k 的节点数 < k，就允许合并。这允许更多合并机会。

**George 合并**: 当 v1 的每个邻居要么已经是 v2 的邻居，要么度数 < k 时，可以安全合并 v1 → v2。

## 实现细节 Implementation Details

### 数据结构

```c
typedef struct {
    int32_t virt_reg_id;    // 虚拟寄存器 ID (v0, v1, v2, ...)
    int32_t start;          // 起始程序点
    int32_t end;            // 结束程序点
    int32_t uses[16];       // 使用点列表
    int32_t num_uses;       // 使用次数
    int32_t assigned_reg;   // 分配的物理寄存器 (-1 = spilled)
    bool spilled;           // 是否溢出
    int32_t spill_slot;     // 溢出栈槽编号
} LiveInterval;
```

### 接口 API

- `ra_linear_scan(ctx)` — 执行线性扫描寄存器分配
- `ra_graph_coloring(ctx)` — 执行图着色寄存器分配  
- `ra_print_assignment(ctx, out)` — 打印分配结果
- `ra_get_assignment(ctx, virt_id)` — 查询物理寄存器
- `ra_is_spilled(ctx, virt_id)` — 查询是否溢出

### 模拟配置

- 物理寄存器: 8 个通用寄存器 R0-R7
- R7 预留给溢出处理 (spill register)
- 可用作分配的: R0-R6 (7个)

## 参考资料 References

1. Poletto & Sarkar, "Linear Scan Register Allocation", TOPLAS 1999
2. Chaitin et al., "Register Allocation via Coloring", Computer Languages 1981
3. Briggs, Cooper, Torczon, "Improvements to Graph Coloring Register Allocation", TOPLAS 1994
4. George & Appel, "Iterated Register Coalescing", TOPLAS 1996
5. Hack, Grund, Goos, "Register Allocation for Programs in SSA Form", CC 2006
6. LLVM RegAlloc: Greedy, Basic, Fast — https://llvm.org/docs/CodeGenerator.html#register-allocator
7. CMU 15-745 Lecture Notes: Register Allocation
