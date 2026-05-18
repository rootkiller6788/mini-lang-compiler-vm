# Backend Design Patterns — 编译器后端设计模式

> 编译器后端架构, 目标描述抽象, LLVM TableGen, 设计模式

## 编译器后端阶段 Compiler Backend Stages

典型编译器后端分为以下阶段 (pipeline):

```
IR 输入
  │
  ▼
┌─────────────────────┐
│ 1. 指令选择          │  Instruction Selection
│    IR → 目标指令      │
└────────┬────────────┘
         │
         ▼
┌─────────────────────┐
│ 2. 指令调度 (可选)   │  Instruction Scheduling
│    重排指令, 减少延迟  │
└────────┬────────────┘
         │
         ▼
┌─────────────────────┐
│ 3. 寄存器分配         │  Register Allocation
│    虚拟→物理寄存器     │
└────────┬────────────┘
         │
         ▼
┌─────────────────────┐
│ 4. 代码发射           │  Code Emission
│    输出汇编/机器码     │
└────────┬────────────┘
         │
         ▼
    汇编输出
```

## 阶段详细说明

### 1. 指令选择 Instruction Selection

**输入**: IR 表示 (树/DAG/SSA 三地址码)

**输出**: 目标机器指令序列 (使用虚拟寄存器)

**方法**:
- **树覆盖 (Tree Covering)**: 最大吞噬, 动态规划, BURS
- **DAG 覆盖**: SelectionDAG (LLVM), 模式匹配
- **语法导向**: 文法描述, 自动生成选择器

**设计模式**: **策略模式 (Strategy)** — 不同目标机器使用不同的 tile 集合

```c
// 策略: 切换 tile set
TileSet x86_tiles = load_x86_patterns();
TileSet arm_tiles = load_arm_patterns();
isel_tile_tree(ir, &target_tiles, &instructions);
```

### 2. 指令调度 Instruction Scheduling

**目标**: 重新排序指令, 最大化处理器流水线利用率, 减少停顿

**输入**: 基本块内的指令序列 (依赖 DAG)

**输出**: 重新排序的指令序列

**方法**:
- **表调度 (List Scheduling)**: 贪心选择就绪指令
- **模调度 (Modulo Scheduling)**: 软件流水 (循环)
- **跟踪调度 (Trace Scheduling)**: 跨基本块

**设计模式**: **优先级队列 (Priority Queue)** — 基于延迟和资源约束选择下一条指令

### 3. 寄存器分配 Register Allocation

**输入**: 虚拟寄存器指令序列 + 活跃信息

**输出**: 物理寄存器指令序列 (可能包含 spill/reload)

**方法**:
- **线性扫描 (Linear Scan)**: O(n log n), JIT 友好
- **图着色 (Graph Coloring)**: Chaitin/Briggs, 质量更高
- **SSA-based**: SSA 形式简化分配 (每个定义恰好一次分配)

**设计模式**: **图算法 + 贪心/回溯** — 干扰图建模冲突, 贪心选择颜色

### 4. 代码发射 Code Emission

**输入**: 已分配物理寄存器的指令序列

**输出**: 汇编文本 或 机器码二进制

**方法**:
- **文本发射**: fprintf 汇编指令 (简单)
- **二进制发射**: 编码机器指令为字节序列 (JIT)
- **MC 层** (LLVM): 统一的汇编/目标文件生成

**设计模式**: **访问者模式 (Visitor)** — 遍历指令序列, 每条指令调用对应的 emit 函数

## 目标描述 Target Description

目标描述是编译器后端的核心抽象。它分离了**通用算法**和**目标特定信息**。

### 手动描述 (本项目方式)

```c
typedef struct {
    ABIType abi_type;
    int32_t num_arg_regs;
    const char *arg_regs[6];
    const char *return_reg;
    int32_t stack_alignment;
    // ...
} ABIInfo;

void abi_init(ABIInfo *abi, ABIType type) {
    switch (type) {
        case ABI_X86_64_SYSV: /* ... */ break;
        case ABI_ARM64_AAPCS: /* ... */ break;
        case ABI_RISCV64_LP64: /* ... */ break;
    }
}
```

### LLVM TableGen 方式

TableGen 是一种声明式的领域特定语言 (DSL), 用于描述目标机器的各个方面:

```
// 寄存器定义
def GR32 : RegisterClass<"X86", [i32], 32,
    (add EAX, ECX, EDX, ESI, EDI, EBX, EBP, ESP)>;

// 指令定义
def ADD32rr : I<0x01, MRMDestReg, (outs GR32:$dst),
                (ins GR32:$src1, GR32:$src2),
                "add{l}\t{$src2, $dst|$dst, $src2}",
                [(set GR32:$dst, (add GR32:$src1, GR32:$src2))]>;
```

TableGen 文件被编译为 C++ 代码, 被 LLVM 后端链接使用。

**TableGen 的优势**:
- **声明式**: 描述"是什么"而非"怎么做"
- **类型安全**: 编译时检查一致性
- **自动生成**: 模式匹配器, 汇编打印器, 反汇编器
- **单点真理**: 寄存器/指令只定义一次

### 目标描述的元素

| 元素 | 描述 | 示例 |
|------|------|------|
| **Register Classes** | 寄存器类别 | GR32, FR32, VR128 |
| **Register Tuples** | 寄存器对/组 | R0_R1, Q0_Q1 |
| **Instructions** | 机器指令 | ADD, SUB, MOV |
| **Operand Types** | 操作数约束 | i32imm, GR32, mem |
| **Patterns** | IR→指令映射 | (add reg, imm) → ADDri |
| **Calling Conventions** | 调用约定 | CC_SysV, CC_Win64 |
| **Features** | 目标特性 | SSE2, AVX, NEON |

## 设计模式 Design Patterns

### 1. 管线模式 Pipeline / Chain of Responsibility

每个阶段独立, 按顺序处理:

```c
void codegen_run(CodeGen *cg) {
    isel_tile_tree(ir, ts, &ilist);          // 阶段1
    peephole_optimize(peep, &ilist);          // 阶段1.5
    compute_liveness(&ilist, &intervals);     // 阶段2前置
    ra_linear_scan(ra_ctx);                   // 阶段2
    codegen_emit_asm(cg, &ilist);             // 阶段3
}
```

### 2. 策略模式 Strategy

不同目标/算法可插拔:

```c
// 指令选择策略
isel_tile_tree(ir, &x86_tiles, ...)   vs   isel_tile_tree(ir, &arm_tiles, ...)

// 寄存器分配策略
ra_linear_scan(ctx)                    vs   ra_graph_coloring(ctx)
```

### 3. 构建者模式 Builder

逐步构建汇编输出:

```c
void emit_prologue(FILE *out) {
    fputs("  push rbp\n", out);
    fputs("  mov  rbp, rsp\n", out);
}
void emit_instruction(Instruction *in, FILE *out) { /* ... */ }
void emit_epilogue(FILE *out) { /* ... */ }
```

### 4. 享元模式 Flyweight

使用字符串池避免重复分配操作数名称:

```c
const char *reg_names[] = {"rax", "rbx", "rcx", "rdx", ...};
// 所有指令共享同一个 reg_names 数组
```

### 5. 访问者模式 Visitor

遍历指令列表进行优化/分析:

```c
void peephole_optimize(PeepholeContext *ctx, InstructionList *ilist) {
    // 滑动窗口遍历, 每个窗口尝试所有规则
    for (size_t i = 0; i < ilist->count; i++) {
        for (size_t r = 0; r < ctx->rule_count; r++) {
            try_apply_rule(&ctx->rules[r], ilist, i);
        }
    }
}
```

## 后端可移植性层次

```
┌──────────────────────────────────────────────┐
│  前端 (Frontend) — 语言特定 (C, Rust, ...)    │
├──────────────────────────────────────────────┤
│  IR (中间表示) — 语言无关, 架构无关              │
├──────────────────────────────────────────────┤
│  后端优化 (Mid-level Optimizer) — 架构无关      │
│  - 内联, DCE, GVN, LICM, ...                  │
├──────────────────────────────────────────────┤
│  目标描述层 — 架构参数, ABI, 指令模式           │
├──────────────────────────────────────────────┤
│  后端通用算法 — 架构无关                      │
│  - isel, reg_alloc, scheduling, peephole     │
├──────────────────────────────────────────────┤
│  代码发射器 — 目标特定 (x86, ARM, RISC-V)     │
└──────────────────────────────────────────────┘
```

关键洞察: **后端算法是通用的, 目标描述是特定的**。分离这两个关注点使编译器可以支持多个目标。

## LLVM 后端架构

LLVM 后端架构是最成熟的实现:

```
SelectionDAGBuilder    →  LLVM IR → SelectionDAG
LegalizeTypes          →  类型合法化
LegalizeDAG            →  操作合法化
DAGCombine             →  DAG 优化
Instruction Selection  →  DAG→MachineInstr  (TableGen)
Scheduling             →  指令调度
Register Allocation    →  RegAllocGreedy  / RegAllocFast
Prolog/Epilog Insert   →  栈帧设置
Peephole               →  机器级窥孔优化
AsmPrinter             →  MCInst→汇编文本
```

## 参考资料

1. LLVM Writing a Backend: https://llvm.org/docs/WritingAnLLVMBackend.html
2. LLVM TableGen: https://llvm.org/docs/TableGen/
3. Fraser & Hanson, "A Retargetable C Compiler: Design and Implementation" (lcc), 1995
4. Appel, "Modern Compiler Implementation in C", Cambridge 1998
5. Muchnick, "Advanced Compiler Design and Implementation", 1997
