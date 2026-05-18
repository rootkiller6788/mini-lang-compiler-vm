# mini-compiler-backend — 编译器后端 (C 语言实现)

> 参考 CMU 15-745, Appel "Modern Compiler Implementation in C", Muchnick

## 概述

`mini-compiler-backend` 是一个教学用途的编译器后端实现, 涵盖编译器后端的主要阶段: 指令选择, 寄存器分配, 代码生成, 窥孔优化, 以及目标 ABI 抽象。

所有代码用 C99 标准编写, 仅依赖 libc + libm。

## 模块总览

| 模块 | 文件 | 描述 |
|------|------|------|
| **指令选择** | `include/instruction_selection.h`, `src/instruction_selection.c` | 树覆盖最大吞噬 (Maximal Munch), 将 IR 树翻译为目标指令 |
| **寄存器分配** | `include/reg_alloc.h`, `src/reg_alloc.c` | 线性扫描 + 图着色 (Chaitin/Briggs), 8 个物理寄存器模型 |
| **代码生成** | `include/codegen.h`, `src/codegen.c` | 后端编排器: isel → regalloc → peephole → 汇编输出 |
| **窥孔优化** | `include/peephole.h`, `src/peephole.c` | 滑动窗口模式匹配 (15 条规则), 指令级优化 |
| **ABI 抽象** | `include/abi_target.h`, `src/abi_target.c` | 目标调用约定: x86-64 SysV, ARM64 AAPCS, RISC-V LP64 |

## 构建

```bash
make all          # 构建所有演示程序
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
```

## 目录结构

```
mini-compiler-backend/
├── include/
│   ├── instruction_selection.h   # 指令选择接口
│   ├── reg_alloc.h               # 寄存器分配接口
│   ├── codegen.h                 # 代码生成编排接口
│   ├── peephole.h                # 窥孔优化接口
│   └── abi_target.h              # ABI 抽象接口
├── src/
│   ├── instruction_selection.c   # 最大吞噬实现
│   ├── reg_alloc.c               # 线性扫描 & 图着色实现
│   ├── codegen.c                 # 代码生成编排 & x86/ARM/RISC-V 发射
│   ├── peephole.c                # 窥孔规则匹配 & 替换
│   └── abi_target.c              # x86-64/ARM64/RISC-V ABI 实现
├── examples/
│   ├── isel_demo.c               # 指令选择演示
│   ├── regalloc_demo.c           # 寄存器分配演示
│   └── codegen_demo.c            # 端到端代码生成演示
├── demos/
│   ├── mini-register-allocator/
│   │   └── README.md             # 寄存器分配: 图着色 vs 线性扫描
│   └── mini-instruction-selection/
│       └── README.md             # 指令选择: 最大吞噬 vs BURS
├── docs/
│   ├── course-alignment.md       # 课程对齐 (CMU 15-745, Appel Ch 9-11)
│   └── backend-design-patterns.md # 后端设计模式与目标描述
├── Makefile
└── README.md
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
