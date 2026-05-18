# mini-jit-vm — JIT 编译与字节码 VM (C 语言实现)

> 参考 Dart VM, LuaJIT, V8 Ignition+TurboFan, Crafting Interpreters Ch 14-15, 28-30

## 模块

| # | 模块 | 文件 | 说明 |
|---|------|------|------|
| 1 | 栈式字节码 VM | `include/bytecode.h`, `src/bytecode.c` | OpCode 指令集, StackVM 解释器, fetch-decode-execute 循环 |
| 2 | 方法 JIT | `include/jit_method.h`, `src/jit_method.c` | 模板化编译, 二层 native opcode 执行器, 热检测与分层 |
| 3 | 内联缓存 | `include/inline_cache.h`, `src/inline_cache.c` | Monomorphic IC, 4-way PIC, 方法派发优化 |
| 4 | 垃圾回收 | `include/gc.h`, `src/gc.c` | Mark-sweep + 分代 GC, nursery/minor GC, 写屏障 |
| 5 | 值系统与闭包 | `include/closure_values.h`, `src/closure_values.c` | Tagged union value, Closure, VMContext, GC 根集 |

## 构建

```sh
make all          # 构建所有示例
make bytecode_vm  # 仅构建字节码 VM 演示
make jit_demo     # 仅构建 JIT 演示
make gc_demo      # 仅构建 GC 演示
make clean        # 清理构建产物
```

## 运行示例

```sh
./bin/bytecode_vm_demo     # 表达式 (+ 3 (* 4 5)) → 23
./bin/jit_demo             # Fibonacci 循环, 解释 vs JIT 计时对比
./bin/gc_demo              # GC 分配、minor/major collection、统计
```

## 设计理念

本实现是一个最小化的、教学导向的 JIT VM 核心。所有模块均用 C99 + libc/libm 实现，无外部依赖。JIT 编译器采用二层解释策略 (字节码→紧凑本机操作码→专用执行器)，在可移植性与性能之间取得平衡。

代码结构遵循 "一个概念一个文件" 原则，头文件声明接口，源文件实现细节，示例程序演示使用方式。

## 目录结构

```
mini-jit-vm/
├── include/
│   ├── bytecode.h          # 字节码 VM 接口
│   ├── jit_method.h        # JIT 编译器接口
│   ├── inline_cache.h      # 内联缓存接口
│   ├── gc.h                # 垃圾回收接口
│   └── closure_values.h    # 值系统与闭包接口
├── src/
│   ├── bytecode.c          # 字节码 VM 实现
│   ├── jit_method.c        # JIT 编译器实现
│   ├── inline_cache.c      # 内联缓存实现
│   ├── gc.c                # 垃圾回收实现
│   └── closure_values.c    # 值系统与闭包实现
├── examples/
│   ├── bytecode_vm_demo.c  # 字节码 VM 演示
│   ├── jit_demo.c          # JIT 编译演示
│   └── gc_demo.c           # GC 演示
├── demos/
│   ├── mini-stack-vm/README.md   # 栈式 VM 设计文档
│   └── mini-method-jit/README.md # 方法 JIT 设计文档
├── docs/
│   ├── course-alignment.md       # 课程对齐文档
│   └── jit-vm-architecture.md    # JIT VM 架构文档
├── Makefile
└── README.md
```

## 许可证

MIT License
