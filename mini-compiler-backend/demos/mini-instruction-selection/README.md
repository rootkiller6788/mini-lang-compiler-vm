# Mini Instruction Selection — 指令选择

> 参考 CMU 15-745, Appel "Modern Compiler Implementation in C" Ch 9, Muchnick Ch 13

## 概述 Overview

指令选择 (Instruction Selection) 将编译器中间表示 (IR) 的操作翻译为目标机器指令。这是编译器后端的第一阶段，直接影响生成代码的质量和效率。

本项目实现**最大吞噬 (Maximal Munch)** 算法，同时讨论先进方法如 BURS (Bottom-Up Rewrite System)。

## 核心概念 Core Concepts

### 问题定义

给定：
- 一棵 IR 表达式树 T
- 一组目标机器指令模式 (tile patterns)

寻找：
- T 的一个最小代价的覆盖 (tiling)，用目标机器指令模式覆盖树的每个节点

```
IR 树:                           目标模式:
    +                           ADD t, s1, s2     (cost=1)
   / \                          LEA t, [s1+s2*4]  (cost=1, 仅当s2*4)
  a   *                         MOV t, s           (cost=1)
     / \                        LOAD t, [addr]     (cost=3)
    b   4
                                可能的覆盖:
                                [ADD a, t1; MUL t1, b, 4]  (cost=2)
                                [ADD a, t1; SHL t1, b, 2]  (cost=2)
                                [LEA a, [b*4+a]]           (cost=1) ✓
```

### 树文法 Tree Grammar

指令模式可以用树文法 (Tree Grammar) 描述：

```
非终结符:
  reg — 值在寄存器中
  mem — 值在内存中
  imm — 立即数

产生式规则:
  reg → ADD(reg, reg)        { emit: add r_dst, r_src1, r_src2; cost = 1 }
  reg → ADD(reg, imm)        { emit: add r_dst, r_src1, imm; cost = 1 }
  reg → LOAD(mem)            { emit: mov r_dst, [addr]; cost = 3 }
  reg → CONST(c)             { emit: mov r_dst, c; cost = 1 }
  reg → MUL(reg, CONST(2^n)) { emit: shl r_dst, r_src, n; cost = 1 }
  reg → MUL(reg, reg)        { emit: imul r_dst, r_src1, r_src2; cost = 3 }
  mem → BASE(b)              { cost = 0 }
  mem → ADD(mem, imm)        { emit: lea r_tmp, [b+imm]; cost = 1 }
```

### 最大吞噬 Maximal Munch

最大吞噬是一种**贪心**算法，从树的根节点开始，试图匹配最大的模式 (tile)。如果没有匹配，则失败。

**算法步骤:**

1. 从根节点开始
2. 尝试匹配拥有最大节点数的 tile pattern
3. 如果匹配成功，发出对应的目标指令，递归处理未覆盖的子节点
4. 如果匹配失败，尝试较小的 tile，直到匹配成功或报错

```
最大吞噬算法:

function munch(node):
    for tile in tiles.sorted_by_descending_size():
        if tile.matches(node):
            tile.emit(node)
            for child in uncovered_children(node, tile):
                munch(child)
            return
    error("no pattern matched")
```

**最大吞噬的优点:**
- 简单，易于实现 (~200 行代码)
- 直观的贪心策略
- 适合教学和小型编译器

**最大吞噬的缺点:**
- 贪心不一定产生全局最优解
- 无法同时考虑多个指令模式的交互
- 对某些目标机器需要手动调整 tile 顺序

### 最大吞噬 vs 动态规划

**动态规划 (Dynamic Programming)** 可以找到最优覆盖：

```
function dp_tiling(node):
    for tile in tiles:
        if tile.matches(node):
            cost = tile.cost + sum(dp_tiling(child) for child in uncovered)
            if cost < best_cost:
                best_cost = cost
                best_tile = tile
    node.best_tile = best_tile
    return best_cost
```

动态规划保证了最优解，但需要 O(n·t) 的时间 (n=树节点数, t=tile数量)，通常比最大吞噬慢。

### BURS (Bottom-Up Rewrite System)

BURS 是一种基于自动机理论的指令选择方法：

1. **输入**: 树文法 (一组产生式规则)
2. **构建**: BURS 自动机 (状态转移表)
3. **匹配**: 自底向上遍历 IR 树，自动机状态向上传播
4. **归约**: 在根节点处识别可应用的最小代价规则

**IBURG/Optimal (`iburg`, `ocamlburg`)** 是 BURS 代码生成器，接收文法描述，自动产生指令选择器。

```
BURS 流程:

文法描述 (.brg) → burg 工具 → C 代码 (指令选择器) → 链接到编译器

工具链: lburg (ASPLOS 1992), twig (Aho), iburg (Fraser), ocamlburg
```

IBURG 产生优化的表驱动指令选择器，相比手写的最大吞噬需要更少的维护工作。

### LLVM 中的指令选择

LLVM 使用 SelectionDAG 进行指令选择：

1. **SelectionDAGBuilder** — 将 LLVM IR 基本块转换为 DAG (有向无环图)
2. **Legalize** — 将不合法操作转换为目标支持的操作
3. **DAG Combine** — DAG 级优化
4. **Instruction Selection** — 用 TableGen 描述的模式匹配 DAG
5. **Schedule** — 将 DAG 线性化为指令序列

LLVM 的 `TableGen` 文件 (.td) 描述目标机器指令：

```
// X86InstrInfo.td
def ADD32ri : I<0x81, MRM0r, (outs GR32:$dst), (ins GR32:$src, i32imm:$imm),
    "add{l}\t{$imm, $dst|$dst, $imm}", []>;
```

### 目标机器描述 Target Machine Description

目标机器描述是编译器后端的关键抽象，定义：

1. **寄存器文件 (Register File)**
   - 寄存器类别 (通用, 浮点, 向量)
   - 寄存器别名 (sub-register)
   - 调用约定 (caller/callee saved)

2. **指令集 (Instruction Set)**
   - 指令操作码、格式、编码
   - 操作数类型约束
   - 指令延迟和吞吐量

3. **指令模式 (Pattern)**
   - IR → 目标指令的映射
   - 多指令模式 (例如 fneg + fmul → fmsub)

4. **ABI / 调用约定**
   - 函数序言/跋言
   - 参数传递规则
   - 栈布局

**LLVM TableGen** 是最成熟的目标描述语言，被 LLVM、MLIR、Chisel 等项目使用。

```
TableGen 示例:

def MOV32ri : I<0xB8, (outs GR32:$dst), (ins i32imm:$src),
    "mov{l}\t{$src, $dst}", [(set GR32:$dst, (i32 imm:$src))]>;
```

### 比较 Comparison: Maximal Munch vs BURS vs LLVM

| 方法 | 时间 | 代码量 | 质量 | 可维护性 |
|------|------|--------|------|---------|
| Maximal Munch | 简单 | ~200 LOC | 中等 | 手动 |
| DP Tiling | 中等 | ~400 LOC | 最优 | 手动 |
| BURS (IBURG) | 自动 | .brg + 生成 | 最优 | 文法驱动 |
| LLVM TableGen | 复杂 | .td + 框架 | 高 | 表格驱动 |

## 实现细节 Implementation Details

### IR 节点类型

```c
typedef enum {
    IRO_ADD, IRO_SUB, IRO_MUL, IRO_DIV,
    IRO_LOAD, IRO_STORE,
    IRO_MEM, IRO_DEREF,
    IRO_CONST, IRO_TEMP,
    IRO_BASE, IRO_IMM,
    IRO_LABEL, IRO_CALL,
    IRO_RETURN, IRO_CMP,
    IRO_JMP, IRO_JE, IRO_JNE, IRO_JL,
    IRO_PUSH, IRO_POP
} IROp;
```

### 目标指令 (x86-like)

```c
typedef enum {
    ISEL_MOV, ISEL_ADD, ISEL_SUB, ISEL_MUL, ISEL_DIV,
    ISEL_LOAD, ISEL_STORE,
    ISEL_PUSH, ISEL_POP,
    ISEL_RET, ISEL_CALL,
    ISEL_CMP, ISEL_JMP, ISEL_JE, ISEL_JNE, ISEL_JL,
    ISEL_LEA, ISEL_SHL, ISEL_XOR,
    ISEL_NOP
} InstructionOp;
```

### 接口 API

- `isel_init(ts)` — 初始化 tile 集合
- `isel_register_tile(ts, pattern, cost, emit_fn, target_op)` — 注册指令模式
- `isel_tile_tree(root, ts, ilist)` — 执行最大吞噬
- `isel_print_mapping(ilist, out)` — 打印指令映射

### Tile 结构

```c
typedef struct {
    IROp pattern;          // IR 操作模式
    size_t cost;           // 代价 (越小越好)
    EmitFn emit_fn;        // 发射函数
    InstructionOp target_op; // 目标指令
    bool is_memory;        // 是否涉及内存
} Tile;
```

## 参考资料 References

1. Aho, Ganapathi, Tjiang, "Code Generation Using Tree Matching and Dynamic Programming", TOPLAS 1989
2. Fraser, Henry, Proebsting, "BURG: Fast Optimal Instruction Selection and Tree Parsing", SIGPLAN 1992
3. Emmelmann, Schrör, Landwehr, "BEG — A Generator for Efficient Back Ends", SIGPLAN 1989
4. LLVM Documentation: "The LLVM Target-Independent Code Generator", https://llvm.org/docs/CodeGenerator.html
5. Appel, "Modern Compiler Implementation in C", Cambridge 1998, Chapter 9
6. Muchnick, "Advanced Compiler Design and Implementation", Morgan Kaufmann 1997, Chapter 13
7. CMU 15-745, Lecture 7-8: Instruction Selection
8. Dias & Ramsey, "Automatically Generating Instruction Selectors Using BURS", JFP 2011
