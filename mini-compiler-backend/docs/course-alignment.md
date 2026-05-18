# Course Alignment — 课程对齐

> 映射本项目实现到 CMU 15-745 和 Appel "Modern Compiler Implementation in C"

## CMU 15-745: Advanced Compiler Design

| 课程主题 | 对应模块 | 本项目实现 |
|---------|---------|-----------|
| **Lecture 7-8: Instruction Selection** | `instruction_selection.h/.c` | Maximal Munch 树匹配覆盖 |
| **Lecture 9-12: Register Allocation** | `reg_alloc.h/.c` | 线性扫描 + 图着色 (Chaitin/Briggs) |
| **Lecture 13-14: Instruction Scheduling** | — | (未来扩展) |
| **Lecture 15-16: SSA Construction** | — | (未来扩展) |
| **Lecture 19-20: Code Generation** | `codegen.h/.c` | 指令选择编排 → 发射汇编 |
| **Lecture 21-22: Peephole Optimization** | `peephole.h/.c` | 滑动窗口模式匹配替换 |
| **Lecture 23-24: ABI & Calling Convention** | `abi_target.h/.c` | x86-64 SysV, ARM64, RISC-V |

## Appel, "Modern Compiler Implementation in C" (1998)

| 章节 | 主题 | 本项目实现 |
|-----|------|-----------|
| **Ch 7: Intermediate Representation** | IR 树结构 | `IROp` 枚举, `IRNode` 树 |
| **Ch 8: Basic Blocks and Traces** | 基本块, 追踪 | — (IR 输入来自前端) |
| **Ch 9: Instruction Selection** | 树覆盖, Maximal Munch | `isel_tile_tree()`, `TileSet` |
| **Ch 10: Liveness Analysis** | 活跃变量分析 | `LiveInterval`, 活跃区间计算 |
| **Ch 11: Register Allocation** | 线性扫描, 图着色 | `ra_linear_scan()`, `ra_graph_coloring()` |
| **Ch 12: Putting It All Together** | 完整编译器组装 | `codegen_run()` 端到端流程 |

## Muchnick, "Advanced Compiler Design and Implementation" (1997)

| 章节 | 主题 | 对应 |
|-----|------|------|
| **Ch 13: Code Generation** | 树覆盖, BURS | `demos/mini-instruction-selection/README.md` |
| **Ch 16: Register Allocation** | 图着色, 合并, 溢出 | `demos/mini-register-allocator/README.md` |
| **Ch 17: Code Scheduling** | 指令调度 | — (未来扩展) |

## 实现特性 vs 理论

| 理论概念 | 实现特性 |
|---------|---------|
| **Maximal Munch Tiling** | 贪心选择最大模式 (O(n·t)) |
| **Linear Scan** | 按起始时间排序, 维护 active list, 溢出最远结束 |
| **Graph Coloring** | 构建干扰图, 简化-选择, Briggs 保守合并 |
| **Calling Convention** | x86-64 SysV ABI: 6 arg regs (RDI-R9), RAX return, caller/callee saved |
| **Peephole** | mov r,r→nop, push+pop→nop, mul r,2→shl, cmp+jmp 消除等 |
| **Stack Frame** | push rbp, mov rbp→rsp, 局部变量分配 |

## 运行示例

```bash
make all
./bin/isel_demo          # 指令选择演示
./bin/regalloc_demo      # 寄存器分配演示
./bin/codegen_demo       # 端到端代码生成 (x86/ARM/RISC-V)
```
