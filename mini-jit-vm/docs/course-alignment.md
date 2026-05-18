# 课程对齐文档

## Crafting Interpreters 对齐

| 本书章节                         | 本实现模块                 | 说明                                   |
|---------------------------------|--------------------------|---------------------------------------|
| Ch 14: Chunks of Bytecode      | `include/bytecode.h`      | 字节码块、常量池、指令编码              |
| Ch 15: A Virtual Machine       | `src/bytecode.c`          | 栈式 VM、fetch-decode-execute 循环     |
| Ch 28: Garbage Collection      | `include/gc.h`, `src/gc.c`| 标记-清除 GC、对象表示                  |
| Ch 29: Superclasses            | `include/inline_cache.h`  | 方法派发优化                           |
| Ch 30: Optimization            | `include/jit_method.h`    | JIT 编译、分层编译                     |

此外，Crafting Interpreters 中的闭包对象、值表示等设计在 `closure_values.h` 和 `closure_values.c` 中得到体现。

## V8 引擎参考

| V8 组件/博客                    | 本实现对应                     | 要点                                  |
|-------------------------------|------------------------------|---------------------------------------|
| Ignition (解释器)              | `include/bytecode.h`         | 寄存器式字节码解释器 (本实现为栈式)     |
| TurboFan (优化编译器)          | `include/jit_method.h`       | 分层 JIT 编译管线                     |
| Sparkplug (基线编译器)         | `src/jit_method.c`           | 模板编译、快速代码生成                  |
| V8 Blog: Launching Ignition   | `demos/mini-stack-vm/README.md` | 解释器设计原理                     |
| V8 Blog: Firing up TurboFan   | `demos/mini-method-jit/README.md` | JIT 编译器架构                    |
| 内联缓存 (IC)                  | `include/inline_cache.h`     | 单态/多态内联缓存                     |
| Orinoco (并发 GC)              | `include/gc.h`              | GC 架构 (本实现为单线程 mark-sweep)    |

关键 V8 博客参考：
- "Firing up the Ignition Interpreter" (2016)
- "Launching Ignition and TurboFan" (2017)
- "Concurrent marking in V8" (2018)

## LuaJIT 参考

| LuaJIT 特性         | 本实现对应              | 说明                       |
|--------------------|------------------------|---------------------------|
| Trace Compiler     | `src/jit_method.c`     | 方法 JIT (而非 trace JIT)  |
| Bytecode Interpreter | `src/bytecode.c`      | 基础解释器                   |
| NYI (Not Yet Implemented) | 多处功能简化      | 学习用，非生产级              |
| FFI                | `include/closure_values.h` | NativeFn 支持              |
| GC (增量式)        | `include/gc.h`         | Mark-sweep (非增量)         |

LuaJIT 2 的关键创新：
1. Trace compiler 而非 method compiler
2. 增量式标记清除 GC
3. NYI 回退机制
4. 极其高效的 FFI

## Dart VM 参考

| Dart VM 组件           | 本实现对应                 |
|-----------------------|--------------------------|
| Kernel Bytecode       | `include/bytecode.h`     |
| AOT / JIT 双模式       | `include/jit_method.h`   |
| Inline Cache          | `include/inline_cache.h` |
| Generational GC       | `include/gc.h` (minor/major) |

## 实现对照表

| 概念                    | 本书实现                             | 生产实现 (V8)                       |
|------------------------|-------------------------------------|------------------------------------|
| 字节码解释器            | StackVM (fetch-decode-execute switch)| Ignition (register-based, computed goto) |
| 常量池                  | 256 项 + tagged union                | 分离整数表/字符串表 + 压缩指针       |
| GC                     | 单线程 mark-sweep + stub nursery     | 并发 mark-sweep + generational     |
| 内联缓存                | Monomorphic + 4-way PIC              | 单态/多态/超多态 (8-way+)           |
| JIT 编译                | 二层解释 (native opcode executor)    | 直接生成 x64/arm64 机器码           |
| 值表示                  | Tagged union (8 bytes)              | 64-bit tagged pointer (SMI+HeapObj)|

## 未来扩展方向

1. **直接机器码生成**: 使用 libJIT / GNU Lightning / LLVM ORC JIT 生成真实机器码
2. **寄存器分配**: 将栈操作映射为虚拟寄存器，提升性能
3. **类型反馈**: 收集运行时类型信息驱动优化器
4. **并发 GC**: 使用写屏障实现并发标记
5. **AOT 编译**: 将字节码预编译为平台原生代码
