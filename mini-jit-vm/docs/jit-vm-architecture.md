# JIT VM 架构文档

## 整体架构

```
┌─────────────────────────────────────────────────────────────┐
│                     Source Code (Lisp-like)                  │
└─────────────────────────────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────┐
│                     Parser / Compiler Frontend               │
└─────────────────────────────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────┐
│                     ByteCode (Stack-based)                   │
│                     include/bytecode.h                       │
└─────────────────────────────────────────────────────────────┘
                            │
              ┌─────────────┼─────────────┐
              ▼             ▼             ▼
┌─────────────────┐ ┌─────────────┐ ┌─────────────────┐
│   Interpreter    │ │  Baseline    │ │  Optimizing     │
│   (exec directly)│ │  JIT (L1)    │ │  JIT (L2)       │
│                 │ │              │ │                 │
│  src/bytecode.c │ │ src/jit_     │ │  (future)       │
│                 │ │ method.c     │ │                 │
└─────────────────┘ └─────────────┘ └─────────────────┘
              │             │             │
              └─────────────┼─────────────┘
                            ▼
┌─────────────────────────────────────────────────────────────┐
│                    Runtime System                            │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────────┐ │
│  │ Value    │  │ Closure  │  │ Inline   │  │ GC (Mark-    │ │
│  │ System   │  │ / Frames │  │ Cache    │  │ Sweep)       │ │
│  └──────────┘  └──────────┘  └──────────┘  └──────────────┘ │
└─────────────────────────────────────────────────────────────┘
```

## 模块职责

### 1. 字节码核心 (bytecode.h/c)
- 定义指令集 (OpCode) 和指令编码
- 栈式虚拟机执行引擎
- 常量池管理

### 2. JIT 编译器 (jit_method.h/c)
- 方法级编译 (Method-at-a-time)
- 模板化代码生成
- 编译阈值管理
- 执行计时与性能对比

### 3. 内联缓存 (inline_cache.h/c)
- 单态缓存 (Monomorphic IC)
- 多态缓存 (Polymorphic IC, 4-way)
- 缓存命中率统计
- 方法派发快速路径

### 4. 垃圾回收 (gc.h/c)
- 标记-清除算法 (Mark-Sweep)
- 分代 GC (Minor + Major)
- 对象分配接口
- GC 统计与调试输出

### 5. 值系统与闭包 (closure_values.h/c)
- 带标签的联合体值表示
- 闭包创建与捕获
- VM 上下文管理
- GC 根集标记

## 数据流：从字节码到执行

```
1. Compile:  Source → ByteCode (OP_PUSH, OP_ADD, etc.)
                    ↓
2. Interpret:  StackVM::vm_execute()
               while (ip < num_inst) {
                   decode(instr) → switch(op) { case OP_ADD: ... }
               }
                    ↓
3. Hot detection:  JITCompiler::jit_should_compile()
                   (call_count >= threshold)
                    ↓
4. JIT Compile:  jit_compile_function()
                 ByteCode → NativeOpcodeStream
                    ↓
5. Execute:  jit_execute_native()
             while (ip < size) {
                 switch(native_op) { case 0x01(ADD): ... }
             }
                    ↓
6. GC:  gc_collect() — 在分配阈值触发时
       标记堆栈 + 全局变量 → 递归标记 → 清除未标记对象
```

## 解释器 → 基线 JIT → 优化 JIT 分层细节

### 层级触发条件

```
Tier 0 (Interpreter):
  - Always starts here
  - Switch-based dispatch loop
  - Collects type feedback (未来扩展)

Tier 1 (Baseline JIT):
  - Triggered after `compilation_threshold` calls (default: 10)
  - Template-based code generation
  - Fast compilation, moderate speed
  - Implementation: native opcode executor

Tier 2 (Optimizing JIT) [未来]:
  - Triggered after more calls + type stability
  - Register allocation + GVN + inlining
  - Slow compilation, maximum speed
  - Deoptimization support
```

### 类型反馈反馈循环

```
Interpreter execution
    │
    ├── Record: ADD(a, b) where a,b are always int
    │
    ▼
JIT compile with assumption: a,b are int
    │
    ├── Generate: add rax, rbx (no type checks)
    │
    ▼
Deoptimize if assumption broken
    │
    ▼
Back to interpreter, re-collect types
```

## 去优化 (Deoptimization)

### 何时反优化

- 类型假设被破坏 (int → string)
- 隐藏类变化 (hidden class transition)
- GC 安全点到达
- 调试断点命中
- 单步执行被激活

### 反优化栈帧重建

```
Native frame:            Interpreter frame:
┌──────────────┐        ┌──────────────┐
│ return addr  │        │ return addr  │
│ saved rbp    │        │ saved fp     │
│ local_0: rax │   →    │ local_0      │
│ local_1: rbx │        │ local_1      │
│ operand rcx  │        │ stack[0]     │
│ operand rdx  │        │ stack[1]     │
└──────────────┘        └──────────────┘
```

映射表 (`FrameTranslationTable`):
- 每个 safepoint 记录 native 寄存器/栈位置到解释器 slot 的对应关系
- 反优化时遍历映射表重建解释器状态

## GC 集成

### GC 安全点

所有代码 (解释器和 JIT) 中，在以下位置插入 GC 安全点：
1. 循环回边
2. 函数调用前
3. 分配操作前

### 写屏障

分代 GC 要求老年代到新生代的引用被追踪：

```c
void gc_write_barrier(GCHeap* heap, GCObject* parent, GCObject* child) {
    if (parent->in_old_gen && child->in_nursery) {
        add_to_remembered_set(parent, child);
    }
}
```

### 线程暂停 (Stop-The-World)

当前单线程实现不涉及并发 GC，所有 GC 都是 STW：
1. 暂停 mutator 线程
2. 标记所有可达对象
3. 清除不可达对象
4. 恢复 mutator 执行

## 内存布局

```
┌───────────────────────┐ 高地址
│     Native Stack      │
│   (C 运行时栈帧)       │
├───────────────────────┤
│     VM Operand Stack   │
│   stack[0..255]       │
├───────────────────────┤
│     GC Heap            │
│   ┌─────────────────┐ │
│   │  Old Generation  │ │
│   │  (mark-sweep)    │ │
│   ├─────────────────┤ │
│   │  Nursery         │ │
│   │  (minor GC)      │ │
│   └─────────────────┘ │
├───────────────────────┤
│     Global Data        │
│   globals[0..127],   │
│   const_pool          │
├───────────────────────┤
│     Code Segment       │
│   bytecode[],         │
│   native_code[]       │
└───────────────────────┘ 低地址
```

## 扩展点与未来方向

1. **直接的机器码生成**: 集成 LLVM ORC JIT 或 GNU libjit，实现真正的 native code generation
2. **Trace-based JIT**: 参照 LuaJIT，在运行时 trace 热路径
3. **Concurrent GC**: 使用原子操作和写屏障实现 GC 与 mutator 并发
4. **Profile-guided optimization**: 收集运行时 profile 驱动内联决策
5. **AOT compilation**: 离线编译字节码为目标平台原生代码
6. **寄存器分配器**: 将栈式操作映射为寄存器操作 (SCCP/GVN/RA)

## 参考文献

- Google V8 Team. "Firing up the Ignition Interpreter." V8 Blog, 2016.
- Google V8 Team. "Launching Ignition and TurboFan." V8 Blog, 2017.
- Mike Pall. "LuaJIT 2.0 Intellectual Overview." LuaJIT Wiki, 2012.
- Bolz, C. F., & Tratt, L. "The Impact of Meta-Tracing on VM Design." 2013.
- Nystrom, R. *Crafting Interpreters*. Genever Benning, 2021.
