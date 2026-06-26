# Knowledge Graph — mini-ai-compiler

## L1: Definitions (Complete)

| # | Definition | File | Type |
|---|-----------|------|------|
| 1.1 | MLIR Operation (MLIROp) | include/mlir_dialect.h | struct |
| 1.2 | MLIR Block & Region | include/mlir_dialect.h | struct |
| 1.3 | MLIR Type System (MLIRType) | include/mlir_dialect.h | enum |
| 1.4 | MLIR Dialect Context | include/mlir_dialect.h | struct |
| 1.5 | ComputeGraph & GraphNode | include/graph_ir.h | struct |
| 1.6 | TensorNode & DataType | include/graph_ir.h | struct |
| 1.7 | FusionPattern & FusionMatch | include/op_fusion.h | struct |
| 1.8 | DataLayout enum (NCHW/NHWC) | include/layout_opt.h | enum |
| 1.9 | ScheduleSpace & TuneTask | include/auto_schedule.h | struct |
| 1.10 | TunerMetrics & AutoTuner | include/auto_schedule.h | struct |
| 1.11 | AlgebraicRule & PassStatistics | include/ir_passes.h | struct |
| 1.12 | CodeGenerator & CodegenBuffer | include/codegen.h | struct |
| 1.13 | CodegenLoop & CodegenStmt | include/codegen.h | struct |
| 1.14 | CodegenTarget & CodegenPrecision | include/codegen.h | enum |
| 1.15 | QuantParams & QuantScheme | include/quantize.h | struct/enum |
| 1.16 | QuantObserver & QuantContext | include/quantize.h | struct |
| 1.17 | TypeNode & TypeEnv | include/type_infer.h | struct |
| 1.18 | TypeConstructor & TypeConstraint | include/type_infer.h | enum/struct |
| 1.19 | ExprNode & ExprKind (tensor DSL) | include/tensor_expr.h | struct/enum |
| 1.20 | ComputeStage & ScheduleTransform | include/tensor_expr.h | struct |

## L2: Core Concepts (Complete)

| # | Concept | Implementation | File |
|---|---------|---------------|------|
| 2.1 | SSA-based IR | MLIROp with typed operands/results | src/mlir_dialect.c |
| 2.2 | Dialect system | MLIRContext with arith/memref/func | src/mlir_dialect.c |
| 2.3 | Dataflow computation graph | ComputeGraph with DAG nodes | src/graph_ir.c |
| 2.4 | Pattern-based rewriting | FusionPattern matching engine | src/op_fusion.c |
| 2.5 | Layout selection (NCHW/NHWC) | Cost-based GPU/CPU layout opt | src/layout_opt.c |
| 2.6 | Schedule space exploration | Evolutionary search over schedules | src/auto_schedule.c |
| 2.7 | Compute/schedule separation | Halide-style TensorExpr DSL | src/tensor_expr.c |
| 2.8 | IR lowering pipeline | Target-independent code generation | src/codegen.c |
| 2.9 | Affine quantization mapping | float = scale * (q - zp) | src/quantize.c |
| 2.10 | Symmetric vs asymmetric quant | scale/zp computation | src/quantize.c |
| 2.11 | Type inference for tensors | Hindley-Milner adapted | src/type_infer.c |
| 2.12 | Progressive lowering (MLIR-style) | Multi-level IR lowering docs | src/codegen.c |

## L3: Engineering Structures (Complete)

| # | Structure | Implementation | File |
|---|-----------|---------------|------|
| 3.1 | MLIR Region+Block nested IR | Linked list of blocks | src/mlir_dialect.c |
| 3.2 | Graph adjacency representation | Node inputs/outputs arrays | src/graph_ir.c |
| 3.3 | Fusion pattern registry | Pattern registration + matching | src/op_fusion.c |
| 3.4 | Layout optimizer with cost model | LayoutOptimizer + transforms | src/layout_opt.c |
| 3.5 | Auto-tuner with evolutionary GA | Population + selection + mutation | src/auto_schedule.c |
| 3.6 | Expression DAG for tensor DSL | ExprNode tree with args | src/tensor_expr.c |
| 3.7 | Schedule tree (loop transforms) | Split, reorder, unroll, etc. | src/tensor_expr.c |
| 3.8 | Buffer allocation (liveness) | Interval-based buffer reuse | src/codegen.c |
| 3.9 | Loop nest generation | 7-nested conv2d, 3-nested matmul | src/codegen.c |
| 3.10 | Calibration observer pipeline | Min/max/histogram collection | src/quantize.c |
| 3.11 | Type environment with substitution | Hindley-Milner env structs | src/type_infer.c |
| 3.12 | Storage flattening (N-d -> 1-d) | Row-major stride computation | src/tensor_expr.c |

## L4: Standards/Theorems (Complete)

| # | Standard/Theorem | Code Verification | File |
|---|-----------------|-------------------|------|
| 4.1 | Algebraic identity axioms | a+0=a, a*1=a, a*0=0 verified | src/ir_passes.c |
| 4.2 | Ring/field axioms (ℝ,+,x) | Identity/annihilator tables | src/ir_passes.c |
| 4.3 | Hindley-Milner type system | Algorithm W with occurs check | src/type_infer.c |
| 4.4 | Robinson unification (1965) | type_unify() with occurs check | src/type_infer.c |
| 4.5 | Damas-Milner soundness theorem | Type checking for all nodes | src/type_infer.c |
| 4.6 | Jacob et al. quantization | Affine mapping float<->int | src/quantize.c |
| 4.7 | IEEE 754 FP32 -> INT8 mapping | qmin/qmax clipping bounds | src/quantize.c |
| 4.8 | Quantization MSE/SNR metrics | Error measurement functions | src/quantize.c |
| 4.9 | Broadcast semantics (NumPy) | Shape compatibility rules | src/type_infer.c |
| 4.10 | Amdahl's Law (speedup limits) | Cost model bounded by memory BW | src/auto_schedule.c |

## L5: Algorithms/Methods (Complete)

| # | Algorithm | Implementation | File |
|---|-----------|---------------|------|
| 5.1 | Hash-based CSE (value numbering) | djb2 hash + equivalence check | src/ir_passes.c |
| 5.2 | Liveness-based DCE (mark-and-sweep) | Backward dataflow marking | src/ir_passes.c |
| 5.3 | Constant folding (abstract interp.) | Bottom-up tree evaluation | src/ir_passes.c |
| 5.4 | Algebraic simplification (rewrite) | Identity/annihilator rules | src/ir_passes.c |
| 5.5 | LICM (loop-invariant code motion) | Consumer fan-out heuristic | src/ir_passes.c |
| 5.6 | Strength reduction | Mult-by-power-of-2 -> shift | src/ir_passes.c |
| 5.7 | Topological sort (Kahn's algorithm) | In-degree based traversal | src/graph_ir.c |
| 5.8 | Cycle detection (DFS) | Recursive DFS with rec_stack | src/graph_ir.c |
| 5.9 | Greedy fusion | Iterative pattern match+replace | src/op_fusion.c |
| 5.10 | Evolutionary search (GA) | Random sketch + mutation + crossover | src/auto_schedule.c |
| 5.11 | Cost model estimation | FLOPS + memory bandwidth model | src/auto_schedule.c |
| 5.12 | Conv2D output shape formula | H_out = (H+2P-K)/S+1 | src/graph_ir.c |
| 5.13 | Pooling output shape formula | H_out = (H-K)/S+1 | src/graph_ir.c |
| 5.14 | Row-major storage flattening | offset = sum(idx[i] * stride[i]) | src/tensor_expr.c |
| 5.15 | Auto-bound inference | Walk DAG to derive loop extents | src/tensor_expr.c |
| 5.16 | Per-channel quantization | Independent min/max per channel | src/quantize.c |
| 5.17 | KL-divergence calibration | Histogram-based calibration | src/quantize.c |
| 5.18 | INT4 pack/unpack | Nibble packing 2 vals per byte | src/quantize.c |
| 5.19 | GPTQ group quantization | Per-block quantization params | src/quantize.c |
| 5.20 | Fake quantization (STE) | Forward quantize + backward STE | src/quantize.c |

## L6: Canonical Problems (Complete)

| # | Problem | Solution | File |
|---|---------|----------|------|
| 6.1 | Multi-level IR for AI compilers | MLIR dialect system | src/mlir_dialect.c + examples/mlir_demo.c |
| 6.2 | Computation graph optimization | Fusion + layout + CSE + DCE | src/op_fusion.c + examples/graph_fusion_demo.c |
| 6.3 | Auto-scheduling for tensor programs | Evolutionary search over schedules | src/auto_schedule.c + examples/autotune_demo.c |
| 6.4 | End-to-end code generation | Graph -> C lowering pipeline | src/codegen.c |
| 6.5 | Model quantization for inference | INT8 calibration + quantization | src/quantize.c |
| 6.6 | Type-safe tensor computation | Hindley-Milner type inference | src/type_infer.c |

## L7: Applications (Complete)

| # | Application | Implementation | File |
|---|-------------|---------------|------|
| 7.1 | MLIR dialect visualization | mlir_demo prints IR in MLIR format | examples/mlir_demo.c |
| 7.2 | Operator fusion for Conv+BN+ReLU | graph_fusion_demo with fusion report | examples/graph_fusion_demo.c |
| 7.3 | Auto-tuning for MatMul/Conv2D | autotune_demo with schedule search | examples/autotune_demo.c |
| 7.4 | Optimization pass report | pass_print_stats() | src/ir_passes.c |
| 7.5 | Type-driven fusion eligibility | type_can_fuse() | src/type_infer.c |
| 7.6 | AOT manifest generation | codegen_emit_aot_manifest() | src/codegen.c |

## L8: Advanced Topics (3+ Partial, Complete)

| # | Topic | Implementation | File |
|---|-------|---------------|------|
| 8.1 | Loop-Invariant Code Motion (LICM) | pass_run_licm() | src/ir_passes.c |
| 8.2 | Strength Reduction | pass_run_strength_reduction() | src/ir_passes.c |
| 8.3 | Halide-style rfactor | texpr_rfactor() for reductions | src/tensor_expr.c |
| 8.4 | compute_at scheduling | texpr_schedule_compute_at() | src/tensor_expr.c |
| 8.5 | Storage alignment | texpr_storage_align() | src/tensor_expr.c |
| 8.6 | AOT compilation manifest | codegen_emit_aot_manifest() | src/codegen.c |
| 8.7 | Kernel shape specialization | codegen_specialize_for_shape() | src/codegen.c |
| 8.8 | Dynamic quantization | quantize_dynamic_params() | src/quantize.c |
| 8.9 | QAT fake quantization | quantize_fake_quant_*() | src/quantize.c |
| 8.10 | Let-polymorphism | type_generalize/instantiate() | src/type_infer.c |
| 8.11 | Dependent shape types | type_verify_shape_dependent() | src/type_infer.c |

## L9: Industry Frontiers (Partial)

| # | Frontier | Coverage | File |
|---|----------|----------|------|
| 9.1 | MLIR progressive lowering pipeline | Documented full pipeline | src/codegen.c (codegen_print_lowering_pipeline) |
| 9.2 | GPTQ-style INT4 group quantization | Implemented | src/quantize.c |
| 9.3 | AI compiler landscape survey | Comprehensive doc | docs/ai-compiler-landscape.md |
| 9.4 | MLIR/TVM/XLA/Triton comparison | Architecture analysis | docs/ai-compiler-landscape.md |

## Knowledge Coverage Summary

| Level | Status | Items |
|-------|--------|-------|
| L1: Definitions | **Complete** | 20 struct/enum/API definitions |
| L2: Core Concepts | **Complete** | 12 concepts implemented |
| L3: Engineering Structures | **Complete** | 12 engineering structures |
| L4: Standards/Theorems | **Complete** | 10 theorems with code verification |
| L5: Algorithms/Methods | **Complete** | 20 algorithms implemented |
| L6: Canonical Problems | **Complete** | 6 problems solved in examples/ |
| L7: Applications | **Complete** | 6 applications |
| L8: Advanced Topics | **Complete** | 11 advanced topics implemented |
| L9: Industry Frontiers | **Partial+** | 4 items (3 implemented, 1 documented) |

## Module Statistics

- **Total Lines (include/ + src/)**: 5,997
- **Headers**: 10 files (1,281 lines)
- **Sources**: 10 files (4,716 lines)
- **Tests**: 5 files (492 lines)
- **Examples**: 3 end-to-end demos
- **Documentation**: 3 doc files

## School Mappings

| School | Course | Module Coverage |
|--------|--------|-----------------|
| MIT | 6.004 Computation Structures | Dataflow graphs, IR design |
| MIT | 6.824 Distributed Systems | Cost models, scheduling |
| Stanford | CS 243 Program Analysis | CSE, DCE, constant folding |
| Berkeley | CS 267 HPC | Loop optimization, vectorization, cache tiling |
| CMU | 15-411 Compiler Design | IR, passes, code generation |
| CMU | 15-418 Parallel Computer Arch | Auto-scheduling, cost models |
| UT Austin | CS 395T Systems ML | AI compiler design, quantization |
| ETH | 263-3501 Parallel Programming | Schedule space, loop transforms |
| Cambridge | Part II Compiler Construction | Type systems, Hindley-Milner |
| Georgia Tech | CS 6241 Compiler Design | Optimization pipeline |
