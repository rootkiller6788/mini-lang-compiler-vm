# mini-ai-compiler — AI 编译器 (C 语言实现)

> 参考 MLIR, Apache TVM, XLA, TinyGrad

A minimal, educational AI compiler framework implemented in C99 (libc+libm only). Covers key concepts from production AI compilers: multi-level IR, computation graphs, operator fusion, layout optimization, and auto-scheduling.

## Module Overview

| Module | Header | Source | Description |
|--------|--------|--------|-------------|
| **MLIR Dialect** | `include/mlir_dialect.h` | `src/mlir_dialect.c` | Simplified MLIR: operations, blocks, regions, dialects (arith, memref, func), type system, SSA IR printing |
| **Graph IR** | `include/graph_ir.h` | `src/graph_ir.c` | Computation graph (TVM Relay style): tensor nodes, graph operations (Conv2D, ReLU, BatchNorm, MatMul, etc.), topological sort, shape inference |
| **Operator Fusion** | `include/op_fusion.h` | `src/op_fusion.c` | Pattern-based fusion: Conv+BN+ReLU, MatMul+Bias+ReLU, element-wise chains; greedy fusion algorithm; memory bandwidth savings estimator |
| **Layout Optimization** | `include/layout_opt.h` | `src/layout_opt.c` | Data layout optimization: NCHW/NHWC selection, cost-based layout preference per operator, transpose insertion, propagation |
| **Auto-Scheduling** | `include/auto_schedule.h` | `src/auto_schedule.c` | Ansor-style auto-scheduling: split factors, dim reordering, unroll/vectorize; random sketch generation; evolutionary search with mutation and crossover; cost model |

## Building

```
make
```

Builds all example demos to `bin/`:
- `bin/mlir_demo` — MLIR dialect operations and IR printing
- `bin/graph_fusion_demo` — Operator fusion on computation graphs
- `bin/autotune_demo` — Auto-scheduling with evolutionary search

### Build Requirements

- GCC (or any C99 compiler)
- libc + libm (standard C library)
- No external dependencies

### Build Options

```
make CC=clang          # Use clang instead of gcc
make CFLAGS=-g         # Debug build
make clean             # Remove binaries
```

## Quick Start

```c
#include "mlir_dialect.h"
#include "graph_ir.h"
#include "op_fusion.h"
#include "layout_opt.h"
#include "auto_schedule.h"
```

### MLIR Dialect Example

```c
MLIRContext ctx = mlir_context_create();
mlir_context_register_arith_ops(&ctx);

MLIRBlock *block = mlir_create_block();
mlir_block_add_arg(block, "arg0", MLIR_TYPE_I32);

MLIROp add = mlir_arith_addi("loc:0:0");
mlir_op_add_operand(&add, "%arg0", MLIR_TYPE_I32);
mlir_op_add_result(&add, "%0", MLIR_TYPE_I32);
mlir_block_add_op(block, add);

MLIRRegion region = mlir_create_region();
mlir_region_add_block(&region, block);
mlir_print_ir(&region);
```

### Graph & Fusion Example

```c
ComputeGraph g = graph_create();
int conv = graph_add_node(&g, GOp_CONV2D, NULL, 0, "conv");
graph_node_set_kernel(&g.nodes[0], 3, 3, 1, 1, 1, 1);
int bn = graph_add_node(&g, GOp_BATCH_NORM, (int[]){conv}, 1, "bn");
int relu = graph_add_node(&g, GOp_RELU, (int[]){bn}, 1, "relu");
int fused = fusion_apply_greedy(&g);
```

### Auto-Scheduling Example

```c
TuneTask task = tuner_task_create("matmul_1024", "matmul", 1024, 1024, 1024);
AutoTuner tuner = tuner_init(task, 16, 10, 0.3, 0.6);
tuner_evolutionary_search(&tuner);
tuner_print_best(&tuner);
```

## Project Structure

```
mini-ai-compiler/
├── include/
│   ├── mlir_dialect.h      # MLIR dialect model
│   ├── graph_ir.h          # Computation graph IR
│   ├── op_fusion.h         # Operator fusion
│   ├── layout_opt.h        # Layout optimization
│   └── auto_schedule.h     # Auto-scheduling
├── src/
│   ├── mlir_dialect.c
│   ├── graph_ir.c
│   ├── op_fusion.c
│   ├── layout_opt.c
│   └── auto_schedule.c
├── examples/
│   ├── mlir_demo.c
│   ├── graph_fusion_demo.c
│   └── autotune_demo.c
├── demos/
│   ├── mini-mlir-dialect/
│   │   └── README.md
│   └── mini-operator-fusion/
│       └── README.md
├── docs/
│   ├── course-alignment.md
│   └── ai-compiler-landscape.md
├── Makefile
└── README.md
```

## Design Philosophy

- **C99 only**: Maximum portability; compiles on any platform with a C99 compiler
- **libc+libm only**: No external dependencies; single-file compilation possible
- **Educational clarity**: Structures and algorithms mirror production compilers but are simplified for learning
- **Minimal abstraction**: Direct struct manipulation rather than deep class hierarchies
- **Text-format IR printing**: Output mirrors MLIR/Relay text format for visual verification

## Key Concepts Covered

1. **SSA-based IR** — Static Single Assignment with typed values and operations
2. **Dialect system** — Namespaced operation groupings with custom verification
3. **Computation graphs** — DAG representation with adjacency-based traversal
4. **Pattern-based rewriting** — Match subgraphs and replace with fused operations
5. **Data layout optimization** — Select NCHW vs NHWC based on hardware target
6. **Auto-scheduling** — Evolutionary search over schedule space (split, reorder, vectorize)
7. **Cost models** — Arithmetic intensity, memory bandwidth, compute throughput estimation

## References

- [MLIR Language Reference](https://mlir.llvm.org/docs/LangRef/)
- [TVM: End-to-End Optimizing Compiler (OSDI 2018)](https://arxiv.org/abs/1802.04799)
- [XLA: Optimizing Compiler for ML](https://www.tensorflow.org/xla)
- [Ansor: Generating High-Performance Tensor Programs (OSDI 2020)](https://www.usenix.org/conference/osdi20/presentation/zheng)
- [Triton Language](https://triton-lang.org)
- [TinyGrad](https://github.com/tinygrad/tinygrad)

## License

Educational use.
