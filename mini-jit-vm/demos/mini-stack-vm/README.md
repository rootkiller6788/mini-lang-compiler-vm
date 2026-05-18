# Stack VM 字节码虚拟机设计

## 概述

Stack VM 是基于操作数栈的虚拟机实现，参照 Dart VM、LuaJIT 和 Crafting Interpreters 的设计。本章节深入探讨指令集架构、操作数栈机制、调用帧管理和派发循环优化技术。

## 字节码指令集设计

### 指令编码格式

每条指令编码为一个 32 位整数：

```
|-------- 8 bits --------|-------- 24 bits --------|
|       OpCode          |         Operand          |
```

- 低 8 位：操作码 (OpCode)，支持最多 256 种指令
- 高 24 位：操作数，可编码常量池索引、跳转偏移或寄存器地址

### 指令分类

#### 栈操作指令
| 指令  | 操作数 | 栈变化              | 说明               |
|-------|--------|---------------------|-------------------|
| PUSH  | index  | ... → ..., value    | 将常量池[index]压栈 |
| POP   | -      | ..., value → ...    | 弹出栈顶值          |

#### 算术指令
| 指令  | 栈变化                     | 说明       |
|-------|----------------------------|-----------|
| ADD   | ..., a, b → ..., (a+b)     | 加法       |
| SUB   | ..., a, b → ..., (a-b)     | 减法       |
| MUL   | ..., a, b → ..., (a*b)     | 乘法       |
| DIV   | ..., a, b → ..., (a/b)     | 除法       |
| NEG   | ..., a → ..., (-a)         | 取负       |

#### 逻辑指令
| 指令  | 栈变化                     | 说明       |
|-------|----------------------------|-----------|
| NOT   | ..., a → ..., (!a)         | 逻辑非     |
| AND   | ..., a, b → ..., (a&&b)    | 逻辑与     |
| OR    | ..., a, b → ..., (a\|\|b)  | 逻辑或     |

#### 控制流指令
| 指令           | 操作数  | 说明                         |
|---------------|---------|------------------------------|
| JMP           | offset  | 无条件跳转到 offset          |
| JMP_IF_FALSE  | offset  | 弹出栈顶，若为 false 则跳转   |
| CALL          | nargs   | 调用函数，设置 frame_ptr      |
| RET           | -       | 返回调用者，保留返回值         |

#### I/O 指令
| 指令   | 说明                 |
|-------|---------------------|
| PRINT | 弹出并打印栈顶值      |
| HALT  | 停止虚拟机执行        |

## 操作数栈机制

### 栈布局

```
高地址
+-------------------+
|   栈顶 (sp)       | ← 当前栈指针
+-------------------+
|   value_n         |
+-------------------+
|   value_{n-1}     |
+-------------------+
|       ...         |
+-------------------+
|   value_1         |
+-------------------+
|   栈底 (0)        |
+-------------------+
低地址
```

### 调用帧管理

```c
typedef struct {
    int64_t    stack[VM_STACK_SIZE];  // 操作数栈
    int32_t    sp;                     // 栈指针
    int32_t    ip;                     // 指令指针
    int32_t    frame_ptr;              // 当前帧基址
    ByteCode*  bytecode;               // 当前字节码
} StackVM;
```

调用帧通过 `frame_ptr` 实现，CALL 指令将 `frame_ptr` 设置为 `sp - nargs`，RET 指令将 `sp` 恢复到 `frame_ptr` 处。这种设计避免了显式的栈帧结构，降低了内存开销。

### 调用规约

调用者：
1. 将参数按顺序压入栈
2. 执行 CALL 指令 (操作数为参数个数)

被调用者：
1. 通过 LOAD (frame_ptr + offset) 读取参数
2. 将返回值留在栈顶
3. 执行 RET 指令

调用者恢复：
1. 返回值位于栈顶
2. 继续执行后续指令

## 派发循环 (Dispatch Loop)

### 基本 switch-case 派发

```c
while (vm->ip < bc->num_inst) {
    OpCode op = decode_op(instr);
    switch (op) {
        case OP_PUSH: /* ... */ break;
        case OP_ADD:  /* ... */ break;
        // ...
    }
}
```

优点：简单、可移植
缺点：分支预测失败率高

### 计算型 goto (Computed Goto)

GCC/Clang 扩展语法：

```c
static void* dispatch_table[] = {
    &&L_PUSH, &&L_ADD, &&L_SUB, /* ... */
};
goto *dispatch_table[op];

L_PUSH:
    /* PUSH 实现 */
    op = decode_next();
    goto *dispatch_table[op];

L_ADD:
    /* ADD 实现 */
    op = decode_next();
    goto *dispatch_table[op];
```

计算型 goto 将间接跳转表内联在派发循环中，消除中央 switch 分支，在 CPU 上使用 BTB (Branch Target Buffer) 可以更好地预测。V8 的 Ignition 解释器和 LuaJIT 的解释器都使用了这种技术。

### 线程化代码 (Threaded Code)

进一步优化：将每个 handler 的地址直接嵌入字节码中。

```
指令格式: [handler_ptr | operand]
执行: goto **(ip++)
```

优点：消除解码开销
缺点：可移植性差，指令体积增大

## 寄存器 VM 对比

### 寄存器式 VM 特征
- 指令格式：`ADD R1, R2, R3`
- 操作数直接来自虚拟寄存器
- 指令数量更少（3-address code）
- 解码更复杂

### 栈式 VM 优势
1. **指令编码紧凑**：隐式操作数，无需编码寄存器号
2. **实现简单**：解释器代码量小
3. **适合 JIT**：栈到寄存器的映射可以在编译时完成
4. **编译器友好**：表达式树遍历即可生成栈式代码

### 对比表

| 特性         | 栈式 VM          | 寄存器式 VM       |
|-------------|-----------------|------------------|
| 指令数量     | 多 (每条操作简单) | 少 (每条操作复杂) |
| 解码开销     | 低               | 高 (需解码寄存器号)|
| 内存带宽     | 高 (频繁栈操作)   | 低                 |
| 实现复杂度   | ⭐⭐              | ⭐⭐⭐⭐             |
| JIT 友好度   | ⭐⭐⭐             | ⭐⭐⭐⭐⭐            |
| 代表实现     | JVM, .NET CLR    | Lua, Dalvik       |

## 常量池设计

```c
typedef enum { CONST_INT, CONST_FLOAT, CONST_STRING } ConstType;

typedef struct {
    ConstType type;
    union {
        int64_t  int_val;
        double   float_val;
        char*    str_val;
    } data;
} Constant;
```

常量池最大 256 项，支持整数、浮点数和字符串常量。PUSH 指令通过索引引用常量池，实现了指令与数据的分离。

## 性能优化策略

1. **指令合并 (Superinstructions)**
   将常见的指令序列合并为单条指令，如 `PUSH_ADD` 代替 `PUSH; ADD` 序列，减少派发次数。

2. **内联缓存 (Inline Cache)**
   在方法调用点缓存目标方法地址，避免每次调用都查找方法表。

3. **快速路径 (Fast Path)**
   对常见操作（如整数加法）提供优化的快速路径，避免类型检查开销。

4. **预解码 (Pre-decoding)**
   在加载字节码时预解码操作码，将紧凑编码展开为更高效的内部表示。

## 参考文献

- Nystrom, R. (2021). *Crafting Interpreters*. Ch 14-15: Chunks of Bytecode, A Virtual Machine.
- Dart VM Documentation: Bytecode and Interpreter Design.
- LuaJIT Wiki: Bytecode and Interpreter Internals.
