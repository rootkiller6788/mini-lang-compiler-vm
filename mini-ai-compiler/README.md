# mini-ai-compiler — AI Compiler (C Implementation)

> Reference: MLIR, Apache TVM, XLA, TinyGrad, Halide, Triton

## Module Status: COMPLETE ✅

- **L1-L6**: Complete (all knowledge levels fully implemented)
- **L7**: Complete (6 applications, requirement >= 2)
- **L8**: Complete (11 advanced topics, requirement >= 1)
- **L9**: Partial+ (4 items, 3 implemented + 1 documented)

### Key Metrics

| Metric | Value | Requirement |
|--------|-------|-------------|
| include/ + src/ lines | **5,997** | >= 3,000 |
| Headers | 10 files | >= 4 |
| Sources | 10 files | >= 4 |
| Tests | 5 files (492 lines) | >= 1 |
| Examples | 3 demos | >= 3 |
| `make test` | **ALL PASS** | required |
| Compiler warnings | 0 (clean with -Wall -Wextra) | 0 errors |

A minimal, educational AI compiler framework implemented in C99 (libc+libm only). Covers key concepts from production AI compilers: multi-level IR, computation graphs, operator fusion, layout optimization, auto-scheduling, IR optimization passes, code generation, quantization, type inference, and tensor expression DSL.

## Module Overview

| Module | Header | Source | Description |
|--------|--------|--------|-------------|
| **MLIR Dialect** | `include/mlir_dialect.h` | `src/mlir_dialect.c` | Simplified MLIR: operations, blocks, regions, dialects (arith, memref, func), type system, SSA IR printing |
| **Graph IR** | `include/graph_ir.h` | `src/graph_ir.c` | Computation graph (TVM Relay style): tensor nodes, graph operations (Conv2D, ReLU, BatchNorm, MatMul, etc.), topological sort, shape inference |
| **Operator Fusion** | `include/op_fusion.h` | `src/op_fusion.c` | Pattern-based fusion: Conv+BN+ReLU, MatMul+Bias+ReLU, element-wise chains; greedy fusion algorithm; memory bandwidth savings estimator |
| **Layout Optimization** | `include/layout_opt.h` | `src/layout_opt.c` | Data layout optimization: NCHW/NHWC selection, cost-based layout preference per operator, transpose insertion, propagation |
| **Auto-Scheduling** | `include/auto_schedule.h` | `src/auto_schedule.c` | Ansor-style auto-scheduling: split factors, dim reordering, unroll/vectorize; random sketch generation; evolutionary search with mutation and crossover; cost model |
| **IR Passes** | `include/ir_passes.h` | `src/ir_passes.c` | Compiler optimization passes: CSE (hash-based), DCE (liveness), constant folding, algebraic simplification (ring/field axioms), LICM, strength reduction |
| **Code Generation** | `include/codegen.h` | `src/codegen.c` | IR-to-C code generation: loop nest emission for Conv2D/MatMul/ReLU/element-wise, buffer allocation with liveness, AOT manifest, kernel specialization |
| **Quantization** | `include/quantize.h` | `src/quantize.c` | Model quantization: symmetric/asymmetric INT8, per-channel calibration, GPTQ-style group quantization, INT4 pack/unpack, fake quantization (QAT), dynamic quantization |
| **Type Inference** | `include/type_infer.h` | `src/type_infer.c` | Hindley-Milner type inference for tensors: unification algorithm, broadcast rules, shape function dispatch, type-driven fusion eligibility, dependent shape verification |
| **Tensor Expression** | `include/tensor_expr.h` | `src/tensor_expr.c` | Halide-style tensor expression DSL: compute/schedule separation, loop transforms (split/reorder/unroll/vectorize/parallel), compute_at, rfactor, auto-bound inference, storage flattening |

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
├── include/                    # 10 headers (1,281 lines)
│   ├── mlir_dialect.h         # MLIR dialect model
│   ├── graph_ir.h             # Computation graph IR
│   ├── op_fusion.h            # Operator fusion
│   ├── layout_opt.h           # Layout optimization
│   ├── auto_schedule.h        # Auto-scheduling
│   ├── ir_passes.h            # IR optimization passes
│   ├── codegen.h              # Code generation
│   ├── quantize.h             # Model quantization
│   ├── type_infer.h           # Type inference
│   └── tensor_expr.h          # Tensor expression DSL
├── src/                        # 10 sources (4,716 lines)
│   ├── mlir_dialect.c
│   ├── graph_ir.c
│   ├── op_fusion.c
│   ├── layout_opt.c
│   ├── auto_schedule.c
│   ├── ir_passes.c
│   ├── codegen.c
│   ├── quantize.c
│   ├── type_infer.c
│   └── tensor_expr.c
├── tests/                      # 5 tests (492 lines)
│   ├── test_ir_passes.c
│   ├── test_quantize.c
│   ├── test_type_infer.c
│   ├── test_tensor_expr.c
│   └── test_codegen.c
├── examples/
│   ├── mlir_demo.c
│   ├── graph_fusion_demo.c
│   └── autotune_demo.c
├── docs/
│   ├── knowledge-graph.md     # Nine-level knowledge coverage
│   ├── coverage-report.md     # Completion assessment
│   ├── gap-report.md          # Gap analysis
│   ├── course-alignment.md    # Course reference mapping
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
8. **Compiler optimization passes** — CSE, DCE, constant folding, algebraic simplification
9. **Code generation** — IR-to-C lowering with loop nest generation and buffer management
10. **Model quantization** — INT8/INT4 quantization with per-channel/group calibration
11. **Type inference** — Hindley-Milner adapted for tensor shapes with broadcasting
12. **Tensor expression DSL** — Halide-style compute/schedule separation with loop transforms

## Knowledge Coverage (L1-L9)

| Level | Status | Items |
|-------|--------|-------|
| **L1** Definitions | ✅ Complete | 20 struct/enum/API definitions |
| **L2** Core Concepts | ✅ Complete | 12 core concepts with implementations |
| **L3** Engineering Structures | ✅ Complete | 12 data+operation structures |
| **L4** Standards/Theorems | ✅ Complete | 10 theorems with code verification |
| **L5** Algorithms/Methods | ✅ Complete | 20 algorithms fully implemented |
| **L6** Canonical Problems | ✅ Complete | 6 problems solved in examples/ |
| **L7** Applications | ✅ Complete | 6 applications |
| **L8** Advanced Topics | ✅ Complete | 11 advanced topics implemented |
| **L9** Industry Frontiers | ✅ Partial+ | 4 items (3 implemented + 1 documented) |

See [docs/knowledge-graph.md](docs/knowledge-graph.md) for full coverage details.

## Nine School Course Alignment

| School | Course | Coverage |
|--------|--------|----------|
| **MIT** | 6.004, 6.824 | Dataflow, scheduling, cost models |
| **Stanford** | CS 243, CS 229 | Program analysis, ML systems |
| **Berkeley** | CS 267, CS 294 | HPC optimizations, AI systems |
| **CMU** | 15-411, 15-418 | Compiler design, parallel arch |
| **UT Austin** | CS 395T | Systems ML, AI compilers |
| **ETH** | 263-3501 | Parallel programming, schedule space |
| **Cambridge** | Part II Compiler | Type systems, IR design |
| **Tsinghua** | Compilers, OS | Optimization pipeline, codegen |
| **Georgia Tech** | CS 6241, CS 7641 | Compiler design, ML systems |

See [docs/course-alignment.md](docs/course-alignment.md) for detailed mapping.

## References

- [MLIR Language Reference](https://mlir.llvm.org/docs/LangRef/)
- [TVM: End-to-End Optimizing Compiler (OSDI 2018)](https://arxiv.org/abs/1802.04799)
- [XLA: Optimizing Compiler for ML](https://www.tensorflow.org/xla)
- [Ansor: Generating High-Performance Tensor Programs (OSDI 2020)](https://www.usenix.org/conference/osdi20/presentation/zheng)
- [Triton Language](https://triton-lang.org)
- [TinyGrad](https://github.com/tinygrad/tinygrad)

## License

Educational use.
