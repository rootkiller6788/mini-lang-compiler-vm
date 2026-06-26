# mini-compiler-backend — 编译器后端 (C 语言实现)

> 参考 CMU 15-745, Appel "Modern Compiler Implementation in C", Muchnick
> 代码行数: include/ + src/ = 3,578 行 ✅

## Module Status: COMPLETE ✅

- L1-L6: Complete
- L7: Complete (3 applications: JIT compilation, instruction scheduling, stack frame management)
- L8: Partial (SSA construction, graph coloring, delay slots)
- L9: Partial (JIT code emission documented)

## 概述

`mini-compiler-backend` 是一个教学用途的编译器后端实现, 涵盖编译器后端的主要阶段: 指令选择, 寄存器分配, 代码生成, 窥孔优化, 目标 ABI 抽象, SSA 构造, 控制流图分析, 数据流分析, 栈帧管理, JIT 编译, 以及指令调度。

所有代码用 C99 标准编写, 仅依赖 libc + libm。

## 模块总览

| 模块 | 文件 | 描述 |
|------|------|------|
| **指令选择** | `include/instruction_selection.h`, `src/instruction_selection.c` | 树覆盖最大吞噬 (Maximal Munch), 将 IR 树翻译为目标指令 |
| **寄存器分配** | `include/reg_alloc.h`, `src/reg_alloc.c` | 线性扫描 + 图着色 (Chaitin/Briggs), 8 个物理寄存器模型 |
| **代码生成** | `include/codegen.h`, `src/codegen.c` | 后端编排器: isel → regalloc → peephole → 汇编输出 |
| **窥孔优化** | `include/peephole.h`, `src/peephole.c` | 滑动窗口模式匹配 (15 条规则), 指令级优化 |
| **ABI 抽象** | `include/abi_target.h`, `src/abi_target.c` | 目标调用约定: x86-64 SysV, ARM64 AAPCS, RISC-V LP64 |
| **SSA 构造** | `include/ssa.h`, `src/ssa.c` | Cytron et al. 算法: 支配树, 支配边界, φ 节点插入, 变量重命名 |
| **控制流图** | `include/cfg.h`, `src/cfg.c` | CFG 构建, 支配者计算, 循环检测, SCC (Tarjan), DOT 输出 |
| **数据流分析** | `include/dataflow.h`, `src/dataflow.c` | Kildall 迭代框架: 活跃变量, 到达定值, 可用表达式, 非常忙表达式 |
| **栈帧管理** | `include/stackframe.h`, `src/stackframe.c` | 栈帧布局: x86/ARM/RISC-V, 局部变量, 溢出槽, 参数区 |
| **JIT 编译** | `include/jit.h`, `src/jit.c` | x86-64 机器码编码, VirtualAlloc/mmap, 运行时执行 |
| **指令调度** | `include/inst_sched.h`, `src/inst_sched.c` | 表调度 (List Scheduling), 优先级调度, 延迟槽填充 |

## 九层知识覆盖

| Level | 名称 | 状态 | 条目 |
|-------|------|------|------|
| **L1** | Definitions | Complete | 11 个头文件, 40+ struct/typedef, 80+ API 声明 |
| **L2** | Core Concepts | Complete | Maximal Munch, Linear Scan, Graph Coloring, SSA, Dataflow Framework |
| **L3** | Engineering Structures | Complete | CFG, Dominator Tree, Interference Graph, Live Intervals, Stack Layout |
| **L4** | Standards/Theorems | Complete | Cooper-Harvey-Kennedy Dominator Algorithm, Cytron SSA, Kildall Dataflow |
| **L5** | Algorithms/Methods | Complete | 线性扫描 O(n log n), 图着色 O(n²), Tarjan SCC, 表调度 |
| **L6** | Canonical Problems | Complete | 指令选择, 寄存器分配, 代码生成, 窥孔优化 |
| **L7** | Applications | Partial+ | JIT 编译, 指令调度, 栈帧管理 (3 个) |
| **L8** | Advanced Topics | Partial+ | SSA 构造, 图着色寄存器分配, 延迟槽填充 |
| **L9** | Industry Frontiers | Partial | JIT 机器码发射 (文档级) |

## 核心定理

| 定理 | 公式 | 实现 |
|------|------|------|
| **最大吞噬** | 选择最大 tile 覆盖 IR 树 | `isel_tile_tree()` |
| **线性扫描** | 按 start 排序, 溢出最远 end 的活跃区间 | `ra_linear_scan()` |
| **图着色** | 构建干扰图, simplify → select, Briggs 保守合并 | `ra_graph_coloring()` |
| **支配者** | DOM(n) = {n} ∪ ∩_{p∈pred(n)} DOM(p) | `cfg_compute_dominators()` |
| **支配边界** | DF(X) = {Y \| ∃p∈pred(Y), X dom p ∧ ¬X sdom Y} | `ssa_compute_dominance_frontiers()` |
| **活跃变量** | LIVE-IN[b] = USE[b] ∪ (LIVE-OUT[b] - DEF[b]) | `df_compute_liveness()` |
| **表调度** | 贪心选择就绪指令, 优化 makespan | `sched_list_schedule()` |

## 关键算法

| 算法 | 复杂度 | 实现 |
|------|--------|------|
| Maximal Munch 树覆盖 | O(n·t) | `isel_tile_tree()` |
| 线性扫描寄存器分配 | O(n log n) | `ra_linear_scan()` |
| 图着色寄存器分配 | O(n²) | `ra_graph_coloring()` |
| 滑动窗口窥孔优化 | O(n·r) | `peephole_optimize()` |
| 迭代支配者计算 | O(N²) | `cfg_compute_dominators()` |
| Tarjan SCC | O(V+E) | `cfg_find_scc()` |
| Kildall 数据流迭代 | O(k·B²) | `df_solve_forward()` |
| Cytron SSA 构造 | O(N³) | `ssa_construct()` |
| 表调度 | O(N²) | `sched_list_schedule()` |

## 九校课程映射

| 学校 | 课程 | 对应模块 |
|------|------|---------|
| **CMU** | 15-745 Advanced Compiler Design | isel, regalloc, peephole, SSA |
| **Stanford** | CS 243 Program Analysis | dataflow (liveness, reaching defs) |
| **Berkeley** | CS 264 Compiler Optimizations | SSA, dominators, graph coloring |
| **MIT** | 6.035 Computer Language Engineering | codegen, instruction scheduling |
| **Cambridge** | Compiler Construction (Tripos) | isel, regalloc, peephole |
| **清华** | 编译原理 | 全模块 |
| **ETH** | Program Analysis | dataflow framework |
| **UT Austin** | CS 380C Compilers | register allocation |
| **Georgia Tech** | CS 6241 Compiler Design | instruction scheduling |

## 构建

```bash
make all          # 构建所有演示程序和测试
make test         # 构建并运行测试套件 (11/11 通过)
make isel_demo    # 仅构建指令选择演示
make regalloc_demo   # 仅构建寄存器分配演示
make codegen_demo    # 仅构建端到端代码生成演示
make clean        # 清理构建产物
```

## 运行演示

```bash
./bin/isel_demo
./bin/regalloc_demo
./bin/codegen_demo
./bin/test_suite
```

## 目录结构

```
mini-compiler-backend/
├── include/
│   ├── instruction_selection.h   # 指令选择接口
│   ├── reg_alloc.h               # 寄存器分配接口
│   ├── codegen.h                 # 代码生成编排接口
│   ├── peephole.h                # 窥孔优化接口
│   ├── abi_target.h              # ABI 抽象接口
│   ├── ssa.h                     # SSA 构造接口 (Cytron)
│   ├── cfg.h                     # 控制流图接口
│   ├── dataflow.h                # 数据流分析接口 (Kildall)
│   ├── stackframe.h              # 栈帧管理接口
│   ├── jit.h                     # JIT 编译接口
│   └── inst_sched.h              # 指令调度接口
├── src/
│   ├── instruction_selection.c   # 最大吞噬实现
│   ├── reg_alloc.c               # 线性扫描 & 图着色实现
│   ├── codegen.c                 # 代码生成编排
│   ├── peephole.c                # 窥孔规则匹配 & 替换
│   ├── abi_target.c              # x86-64/ARM64/RISC-V ABI 实现
│   ├── ssa.c                     # SSA 构造 (支配树/φ节点/重命名)
│   ├── cfg.c                     # CFG/支配者/循环/SCC
│   ├── dataflow.c                # 活跃变量/到达定值/可用表达式/非常忙
│   ├── stackframe.c              # 栈帧布局 x86/ARM/RISC-V
│   ├── jit.c                     # x86-64 机器码编码/VirtualAlloc
│   └── inst_sched.c              # 表调度/优先级调度/延迟槽
├── tests/
│   └── test_all_inline.c         # 综合测试套件 (11 项测试)
├── examples/
│   ├── isel_demo.c               # 指令选择演示
│   ├── regalloc_demo.c           # 寄存器分配演示
│   └── codegen_demo.c            # 端到端代码生成演示
├── demos/
│   ├── mini-register-allocator/
│   └── mini-instruction-selection/
├── docs/
│   ├── course-alignment.md
│   └── backend-design-patterns.md
├── Makefile
└── README.md
```

## 使用示例

```c
#include "codegen.h"

int main(void) {
    CodeGen cg;
    codegen_init(&cg, ARCH_X86);

    // 构建 IR: add(load(mem(add(rbp, -16))), 2)
    IRNode *base = ir_node_create(IRO_BASE, 0);
    IRNode *off  = ir_node_create(IRO_CONST, -16);
    IRNode *addr = ir_node_create(IRO_ADD, 0);
    addr->left = base; addr->right = off;

    IRNode *load = ir_node_create(IRO_LOAD, 0);
    load->left = ir_node_create(IRO_MEM, 0);
    load->left->left = addr;

    IRNode *two  = ir_node_create(IRO_CONST, 2);
    IRNode *add  = ir_node_create(IRO_ADD, 0);
    add->left = load; add->right = two;

    IRFunction func = {"my_func", 0, 0};
    codegen_run(&cg, &func, add);
    codegen_emit_asm(&cg, &func, stdout);
    codegen_free(&cg);
    return 0;
}
```

## 设计原则

- **C99 标准**: 无 GCC 扩展, 仅用标准库
- **蛇形命名**: `snake_case` 函数, `PascalCase` 类型, `UPPER_SNAKE_CASE` 常量
- **头文件保护**: `#ifndef X_H` / `#define X_H` / `#endif`
- **显式包含**: 所有需要 bool 的头文件均 `#include <stdbool.h>`
- **模拟目标**: 简化的 x86-like 2-操作数架构, 8 个通用寄存器 R0-R7

## 使用示例

```c
#include "codegen.h"

int main(void) {
    CodeGen cg;
    codegen_init(&cg, ARCH_X86);

    // 构建 IR: add(load(mem(add(rbp, -16))), 2)
    IRNode *base = ir_node_create(IRO_BASE, 0);
    IRNode *off  = ir_node_create(IRO_CONST, -16);
    IRNode *addr = ir_node_create(IRO_ADD, 0);
    addr->left = base; addr->right = off;

    IRNode *load = ir_node_create(IRO_LOAD, 0);
    load->left = ir_node_create(IRO_MEM, 0);
    load->left->left = addr;

    IRNode *two  = ir_node_create(IRO_CONST, 2);
    IRNode *add  = ir_node_create(IRO_ADD, 0);
    add->left = load; add->right = two;

    IRFunction func = {"my_func", 0, 0};
    codegen_run(&cg, &func, add);
    codegen_emit_asm(&cg, &func, stdout);
    codegen_free(&cg);
    return 0;
}
```

## 关键算法

| 算法 | 复杂度 | 实现 |
|------|--------|------|
| Maximal Munch 树覆盖 | O(n·t) | `isel_tile_tree()` |
| 线性扫描寄存器分配 | O(n log n) | `ra_linear_scan()` |
| 图着色寄存器分配 | O(n²) | `ra_graph_coloring()` |
| 滑动窗口窥孔优化 | O(n·r) | `peephole_optimize()` |

## 参考资料

- [CMU 15-745: Advanced Compiler Design](https://www.cs.cmu.edu/~15745/)
- Appel, "Modern Compiler Implementation in C", Cambridge University Press, 1998
- Muchnick, "Advanced Compiler Design and Implementation", Morgan Kaufmann, 1997
- [LLVM Writing a Backend](https://llvm.org/docs/WritingAnLLVMBackend.html)
