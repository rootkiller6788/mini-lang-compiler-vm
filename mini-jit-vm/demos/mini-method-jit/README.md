# Method JIT — 方法级即时编译

## 概述

Method JIT (方法即时编译) 是以方法/函数为编译单元，将字节码翻译为原生机器码的技术。本章节覆盖模板编译、序言与尾声代码生成、栈上替换 (OSR) 和分层编译策略。

> 参考 V8 Ignition+TurboFan、LuaJIT 和 Dart VM 的编译管线设计。

## 模板化编译 (Template JIT)

模板 JIT 为每种字节码指令预定义对应的原生代码模板（宏汇编序列），编译时直接拼接模板。

### 模板映射

| 字节码   | 原生操作码 (x86-64) | 寄存器操作                   |
|---------|--------------------|-----------------------------|
| PUSH i  | mov + push         | mov rax, [const_pool+i]    |
|         |                    | push rax                    |
| ADD     | pop + add          | pop rbx; pop rax            |
|         |                    | add rax, rbx; push rax      |
| SUB     | pop + sub          | pop rbx; pop rax            |
|         |                    | sub rax, rbx; push rax      |
| JMP t   | jmp                | jmp target_address          |
| RET     | pop + ret          | pop rax; ret                |

### 本实现中的模板编译

由于 C99 无法直接生成机器码，本实现采用**二层解释**策略：

1. 字节码 → **紧凑本机操作码** (native opcode stream)
2. 运行时 **native opcode 执行器** 解释执行此紧凑码

紧凑本机操作码编码表：

| 操作码 | 含义     | 数据        |
|-------|---------|------------|
| 0x00  | PUSH    | 4 字节立即数 |
| 0x01  | ADD     | 无          |
| 0x02  | SUB     | 无          |
| 0x03  | MUL     | 无          |
| 0x04  | DIV     | 无          |
| 0x05  | NEG     | 无          |
| 0x06  | AND     | 无          |
| 0x07  | OR      | 无          |
| 0x08  | NOT     | 无          |
| 0x09  | JMP     | 2 字节偏移   |
| 0x0A  | JMP_IF  | 2 字节偏移   |
| 0x0B  | RET     | 无          |
| 0x0C  | HALT    | 无          |

这种二层解释尽管仍在虚拟机内执行，但由于指令格式更紧凑且无需解码常量池索引，执行效率优于原始字节码解释器。

## 序言与尾声 (Prologue/Epilogue)

### 函数序言

进入编译函数时需要保存寄存器状态和设置栈帧：

```nasm
push rbp              ; 保存调用者栈帧
mov  rbp, rsp         ; 设置新栈帧
sub  rsp, local_size  ; 分配局部变量空间
```

### 函数尾声

返回前恢复寄存器状态：

```nasm
mov  rsp, rbp         ; 恢复栈指针
pop  rbp              ; 恢复调用者栈帧
ret                   ; 返回
```

### 本实现对应

`jit_compile_function()` 生成紧凑码后在 `jit_execute_native()` 中通过专用解释器执行，隐含了序言/尾声逻辑。

## 栈上替换 (On-Stack Replacement, OSR)

### 问题

函数被频繁调用时触发 JIT 编译，但当前正在执行的是解释器代码。如何将正在运行的解释器栈帧替换为编译后的原生代码栈帧？

### 解决方案

1. **循环回边计数 (Loop Back-edge Counter)**
   在每个循环回边插入计数器递减和检查。当计数器归零且 JIT 版本已准备好时，触发 OSR。

2. **栈帧映射 (Stack Frame Mapping)**
   记录解释器栈帧中每个 slot 与 JIT 栈帧的映射关系。OSR 时：
   - 暂停解释器
   - 将解释器栈上的值复制到 JIT 所需的位置
   - 跳转到编译后代码中对应的位置

3. **OSR 入口点**
   编译时为每个循环回边生成额外的 OSR 入口点，OSR 后跳转到这些入口。

### 伪代码

```c
if (jit_should_compile(&jc)) {
    jit_compile_function(&jc, &bc);
    if (can_osr(current_ip)) {
        migrate_stack_frame(vm, &jc);
        jump_to_native(jc, osr_entry_point);
    }
}
```

## 分层编译 (Tiered Compilation)

### 三层架构

```
Level 0: 解释器 (Ignition-style)
    ↓ 被频繁调用 (hot)
Level 1: 基线 JIT (模板编译, 快速生成)
    ↓ 被频繁调用 (hotter)  
Level 2: 优化 JIT (类型反馈 + 内联 + GVN)
```

### 层间切换

| 切换方向         | 触发条件           | 机制          |
|-----------------|-------------------|--------------|
| L0 → L1         | 调用次数达阈值      | 异步/同步编译  |
| L1 → L2         | 更热 + 类型稳定    | 异步编译 + OSR |
| Ln → L0 (降级)  | 假设失效/调试      | 反优化 (Deopt) |

### 编译阈值策略

```c
#define L1_THRESHOLD 10   // 10 次调用后触发基线 JIT
#define L2_THRESHOLD 1000 // 1000 次调用后触发优化 JIT
```

### V8 分层编译历程

| 版本     | 架构                                        |
|---------|--------------------------------------------|
| v5.9-   | Full-codegen (基线) + Crankshaft (优化)     |
| v5.9+   | Ignition (解释器) + TurboFan (优化)          |
| v8.0+   | Ignition → Sparkplug (基线) → TurboFan (优化) |
| 现代    | Ignition → Sparkplug → Maglev (中层) → TurboFan |

### 本实现对应

`jit_compile_function()` 模拟了 L0→L1 的基线编译，`jit_execute_native()` 提供了 "编译后" 代码的执行入口。通过 `compilation_threshold` 控制升级时机。

## 去优化 (Deoptimization)

当 JIT 编译时基于的类型预测不再成立（如假设整数操作数但传入浮点数），需要"反优化"回解释执行。

### 反优化点 (Deopt Point)

编译时在可能失败的位置记录 safepoint：
- 类型检查失败点
- GC 安全点
- OSR 入口/出口

### 反优化过程

1. 保存当前原生执行状态
2. 根据映射表重建解释器状态（栈帧、局部变量）
3. 跳转到解释器中对应的位置继续执行

### 数据流

```
Native Code:  reg A = 5; reg B = obj.field
                │ (类型不匹配!)
                ▼
Deopt Frame:  { ip: 42, locals: [5, obj], stack: [...] }
                │
                ▼
Interpreter:  LOAD_FIELD obj, field
```

## 性能基准参考

| 方法            | 相对速度 | 编译时间 | 内存开销 |
|---------------|---------|---------|---------|
| 解释器          | 1×      | 0       | 低       |
| 模板 JIT       | 3-5×    | 极低     | 中       |
| 优化 JIT       | 10-50×  | 高       | 高       |
| 本实现 (二层)   | 1.5-2×  | 极低     | 低       |

> 本实现中由于二层解释特性，性能提升约为 1.5-2 倍。真实 JIT 直接生成机器码可获得 3-100 倍加速。

## 参考文献

- Dart VM: Method JIT and Optimizing Compiler.
- LuaJIT: Trace Compiler and JIT Internals – Mike Pall.
- V8 Blog: Firing up the Ignition Interpreter (2016), Launching Ignition and TurboFan (2017).
- Nystrom, R. (2021). *Crafting Interpreters*. Ch 28-30.
