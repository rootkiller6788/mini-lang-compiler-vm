# Mini Lang Compiler VM（迷你语言编译器虚拟机）

**从零开始、零依赖的 C 语言实现**，涵盖编程语言、编译器和虚拟机核心概念。每个模块实现编译器流水线的一个阶段或语言运行时 — 从词法/语法分析到 IR 优化再到代码生成、JIT 编译和语言范式。模块映射到 Stanford、CMU、MIT 课程，将编译器理论桥接到可运行的 C 代码。

## 模块总览

| 模块 | 主题 | 参考标准 |
|--------|--------|----------------|
| [mini-compiler-frontend](mini-compiler-frontend/) | 词法分析（DFA/Regex）、递归下降解析、AST 构造、语义分析、符号表 | Stanford CS143, Crafting Interpreters |
| [mini-compiler-middle](mini-compiler-middle/) | IR 设计（三地址码）、SSA 形式、数据流分析、优化（DCE、CSE、内联） | CMU 15-745, Dragon Book |
| [mini-compiler-backend](mini-compiler-backend/) | 指令选择（Tiling）、寄存器分配（图着色/线性扫描）、代码发射 | CMU 15-745, Appel "Modern Compiler" |
| [mini-jit-vm](mini-jit-vm/) | 栈 VM 字节码、JIT 编译（方法/追踪）、内联缓存、垃圾回收 | Dart VM, LuaJIT, V8 |
| [mini-lang-paradigm](mini-lang-paradigm/) | OOP（虚表、继承）、FP（闭包、柯里化、Monad）、逻辑（合一）、过程式 | Stanford CS242, MIT 6.945 |
| [mini-main-languages](mini-main-languages/) | C 子集解释器、简单 ML/Haskell 解释器、Protobuf 语言、Lua 类脚本 | Programming Language Pragmatics |
| [mini-ai-compiler](mini-ai-compiler/) | MLIR 方言、TVM Relay、XLA HLO、算子融合、Layout 优化、Auto-scheduling | MLIR, Apache TVM, XLA |
| [mini-build-system](mini-build-system/) | Make 规则求值、Ninja 构建图、依赖解析（拓扑排序）、增量构建、缓存 | GNU Make, Ninja, Buck/Bazel 概念 |

## 设计理念

- **零外部依赖** — 纯 C（C99/C11），仅使用 `libc` 和 `libm`
- **模块自包含** — 每个目录自带 `Makefile`、`include/`、`src/`、`examples/`、`demos/`、`tests/`
- **完整编译器流水线** — 每个阶段可独立使用或组合为工具链
- **理论到代码的映射** — 每个模块包含 `docs/` 目录，内有课程对齐说明
- **实用演示程序** — 简单 C 编译器、带 JIT 的栈 VM、MLIR 方言、构建图求解器等

## 构建方式

每个模块相互独立。进入模块目录后运行：

```bash
cd mini-compiler-frontend
make all    # 构建全部
make test   # 运行测试
```

需要 **GCC** 和 **GNU Make**。

## 项目结构

```
mini-lang-compiler-vm/
├── mini-compiler-frontend/     # 词法分析、语法分析、AST、语义分析
├── mini-compiler-middle/       # IR、SSA、优化
├── mini-compiler-backend/      # 代码生成、寄存器分配
├── mini-jit-vm/                # JIT 编译与字节码 VM
├── mini-lang-paradigm/         # 语言范式与运行时
├── mini-main-languages/        # 编程语言实现
├── mini-ai-compiler/           # AI 编译器（MLIR、TVM、XLA）
└── mini-build-system/          # 构建系统
```

## 许可证

MIT
