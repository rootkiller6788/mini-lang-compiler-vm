# mini-compiler-middle — 编译器中端 (C 语言实现)

> 参考 CMU 15-745 Advanced Compiler Design, Dragon Book, Engineering a Compiler

## 模块总览

| # | Module | Header | Source | Description |
|---|--------|--------|--------|-------------|
| 1 | **IR** | `include/ir.h` | `src/ir.c` | 三地址码中间表示 (Three-Address Code IR): 指令类型、函数、基本块 |
| 2 | **SSA** | `include/ssa.h` | `src/ssa.c` | 静态单赋值形式构造: 支配前沿、φ函数插入、变量重命名 |
| 3 | **Dataflow** | `include/dataflow.h` | `src/dataflow.c` | 数据流分析框架: 单调框架、迭代求解器、位向量集合 |
| 4 | **Optimizer** | `include/optimizer.h` | `src/optimizer.c` | 优化通道: 死代码消除、公共子表达式消除、常量折叠、复制传播 |
| 5 | **CFG** | `include/cfg.h` | `src/cfg.c` | 控制流图: 构建、支配者、回边、自然循环检测 |

## 构建

```bash
make           # 构建所有示例
make clean     # 清理构建产物
```

生成文件在 `bin/` 目录:
- `bin/ir_demo.exe` — IR 与 CFG 演示
- `bin/ssa_demo.exe` — SSA 构造演示
- `bin/opt_demo.exe` — 优化通道演示

## 模块详解

### 1. IR (Intermediate Representation)

三地址码中间表示，每条指令最多三个操作数 (dest, src1, src2)。

**指令集 (13 条)**:

| 指令 | 格式 | 语义 |
|------|------|------|
| `ADD` | `%t_d = add %t_a, %t_b` | 整数加法 |
| `SUB` | `%t_d = sub %t_a, %t_b` | 整数减法 |
| `MUL` | `%t_d = mul %t_a, %t_b` | 整数乘法 |
| `DIV` | `%t_d = div %t_a, %t_b` | 整数除法 |
| `LOAD` | `%t_d = load %t_addr` | 内存加载 |
| `STORE` | `store %t_val, %t_addr` | 内存存储 |
| `BR` | `br label` | 无条件跳转 |
| `BRCOND` | `brcond %t, L1, L2` | 条件跳转 |
| `CALL` | `%t_d = call %t_fn(%t_arg)` | 函数调用 |
| `RET` | `ret %t` | 函数返回 |
| `MOV` | `%t_d = mov %t_s` | 寄存器复制 |
| `PHI` | `%t_d = phi(v1:L1, v2:L2)` | φ函数 (SSA) |
| `ALLOCA` | `%t_d = alloca size` | 栈分配 |

**关键 API**:
```c
IRFunction* func = ir_create_function("name");
int t0 = ir_new_temp(func);
int t1 = ir_new_temp(func);
ir_emit(func, IR_ADD, t1, t0, 5, NULL);
ir_print_function(func, stdout);
```

### 2. SSA (Static Single Assignment)

SSA 形式保证每个变量在程序正文中仅被定义一次。构造过程:

1. **支配者计算**: 迭代算法, O(N²) 最坏情况
2. **支配前沿**: 用于确定 φ 函数插入位置
3. **φ 函数插入**: 在支配前沿处插入 φ 节点
4. **变量重命名**: 在支配者树上 DFS，为每次定义分配唯一名称

```c
ir_print_function(func, stdout);  // 转换前
ssa_build(func);                   // SSA 构造
ir_print_function(func, stdout);  // 转换后
```

### 3. Dataflow (数据流分析)

基于单调框架的通用数据流分析器。使用位向量 (BitVector) 高效表示集合。

**支持的分析类型**:

| 分析 | 方向 | Meet | 用途 |
|------|------|------|------|
| Reaching Defs | Forward | ∪ | 到达定值、use-def 链 |
| Live Variables | Backward | ∪ | 活跃变量、寄存器分配 |
| Available Exprs | Forward | ∩ | 可用表达式、CSE |
| Constant Prop | Forward | ∩ | 常量传播 |

```c
DataflowResult result;
df_reaching_defs(func, blocks, num_blocks, &result);
df_print_result(&result, num_blocks, stdout);
```

### 4. Optimizer (优化器)

迭代式优化通道管理器。通道循环运行至不动点。

**已实现的优化**:

| 通道 | 说明 | 算法 |
|------|------|------|
| DCE | 死代码消除 | 标记-清除 (mark-sweep) |
| CSE | 公共子表达式消除 | 全局值编号简化版 |
| CONST_FOLD | 常量折叠 | 编译时求值 |
| COPY_PROP | 复制传播 | 替换复制源 |
| SIMPLIFY_CFG | CFG 简化 | 空块移除、分支折叠 |

```c
OptPass pipeline[] = {OPT_DCE, OPT_CSE, OPT_CONST_FOLD, OPT_COPY_PROP};
OptStats stats = opt_run_pipeline(func, pipeline, 4);
opt_print_changes(stats, stdout);
```

### 5. CFG (Control Flow Graph)

控制流图构建与分析工具。

**功能**:
- `cfg_build`: 从线性 IR 划分基本块，构建 CFG
- `cfg_print_graph`: 输出 Graphviz DOT 格式
- `cfg_reverse_postorder`: 计算逆后序 (RPO) 遍历顺序
- `cfg_dominators`: 计算支配者集
- `cfg_find_loops`: 通过回边检测自然循环

## 目录结构

```
mini-compiler-middle/
├── include/           # 头文件
│   ├── ir.h           # 中间表示
│   ├── ssa.h          # SSA 构造
│   ├── dataflow.h     # 数据流分析
│   ├── optimizer.h    # 优化器
│   └── cfg.h          # 控制流图
├── src/               # 实现文件
│   ├── ir.c           # IR 实现
│   ├── ssa.c          # SSA 实现
│   ├── dataflow.c     # 数据流实现
│   ├── optimizer.c    # 优化器实现
│   └── cfg.c          # CFG 实现
├── examples/          # 使用示例
│   ├── ir_demo.c      # IR 与 CFG 演示
│   ├── ssa_demo.c     # SSA 构造演示
│   └── opt_demo.c     # 优化演示
├── demos/             # 深度教程
│   ├── mini-ssa-construction/     # SSA 构造详解
│   └── mini-dataflow-analysis/    # 数据流分析详解
├── docs/              # 参考文档
│   ├── course-alignment.md        # 15-745 课程对照
│   └── ir-design-patterns.md      # IR 设计模式
├── Makefile           # 构建系统
└── README.md          # 本文件
```

## 设计原则

- **C99 标准**: 仅依赖 libc + libm
- **命名规范**: 函数 `snake_case`, 类型 `PascalCase`, 常量 `UPPER_SNAKE_CASE`
- **防御式头文件**: `#ifndef` / `#define` / `#endif` 保护
- **bool 类型**: 所有头文件包含 `<stdbool.h>`
- **静态数组**: 固定大小数组, 无动态分配 (除 IRFunction 本身)
- **教学导向**: 优先代码清晰度而非性能极致

## 参考资料

- **CMU 15-745**: Advanced Compiler Design, Spring 2024, Prof. Todd Mowry
- **Dragon Book**: Aho, Lam, Sethi, Ullman — Compilers: Principles, Techniques, and Tools
- **Engineering a Compiler**: Cooper & Torczon, 2nd Edition
- **SSA-based Compiler Design**: Rastello, Tichadou (eds.), Springer 2023
