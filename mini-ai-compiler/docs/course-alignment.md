# Course Alignment — AI Compiler Reference Mapping

> Mapping mini-ai-compiler modules to canonical AI compiler documentation and tutorials

## Module-to-Reference Mapping

### 1. MLIR Dialect → MLIR Official Documentation

| mini-ai-compiler | MLIR Reference |
|------------------|----------------|
| `mlir_dialect.h` — Operation, Block, Region | [MLIR LangRef — IR Structure](https://mlir.llvm.org/docs/LangRef/#ir-structure) |
| `MLIROp` struct (name, operands, results) | [Operation Definition Specification (ODS)](https://mlir.llvm.org/docs/DefiningDialects/Operations/) |
| `MLIRDialect` — operation registry | [Dialect Definition](https://mlir.llvm.org/docs/LangRef/#dialects) |
| `mlir_verify` — type checking | [MLIR Verifier](https://mlir.llvm.org/docs/DefiningDialects/Operations/#verification) |
| Arith dialect (addi, muli) | [Arith Dialect](https://mlir.llvm.org/docs/Dialects/ArithOps/) |
| MemRef dialect (alloc, load, store) | [MemRef Dialect](https://mlir.llvm.org/docs/Dialects/MemRef/) |
| Func dialect (func, return, call) | [Func Dialect](https://mlir.llvm.org/docs/Dialects/Func/) |
| `mlir_print_ir` — text format | [MLIR Printing](https://mlir.llvm.org/docs/LangRef/#printing) |
| IR region nested structure | [MLIR Tutorial — Toy Ch1](https://mlir.llvm.org/docs/Tutorials/Toy/Ch-1/) |
| Type system (i32, f32, memref) | [MLIR Type System](https://mlir.llvm.org/docs/Dialects/BuiltinDialect/#types) |

### 2. Graph IR → TVM Relay Documentation

| mini-ai-compiler | TVM Reference |
|------------------|---------------|
| `ComputeGraph` — node/edge graph | [Relay IR — Expression](https://tvm.apache.org/docs/arch/relay_intro.html) |
| `GraphNode` (op_type, inputs, outputs) | [Relay CallNode](https://tvm.apache.org/docs/reference/api/doxygen/classrelay_1_1_call_node.html) |
| `TensorNode` (shape, dtype) | [Tensor Type in Relay](https://tvm.apache.org/docs/reference/api/doxygen/classrelay_1_1_tensor_type_node.html) |
| `graph_infer_shapes` | [Relay Shape Inference](https://tvm.apache.org/docs/arch/relay_shape_inference.html) |
| `graph_topological_sort` | [Topological Order in Dataflow](https://tvm.apache.org/docs/arch/relay_pass.html) |
| Conv2D, BatchNorm, ReLU ops | [TVM TOPI Operations](https://tvm.apache.org/docs/reference/api/python/topi.html) |
| Graph print/dump | [Relay Text Format](https://tvm.apache.org/docs/arch/relay_text_format.html) |

**Key TVM Tutorials:**
- [Introduction to Relay IR](https://tvm.apache.org/docs/tutorial/relay.html)
- [Quick Start Tutorial for Compiling Deep Learning Models](https://tvm.apache.org/docs/tutorial/relay_quick_start.html)
- [Convert a Model to Relay](https://tvm.apache.org/docs/tutorial/frontend/from_pytorch.html)

### 3. Operator Fusion → TVM / XLA Fusion Passes

| mini-ai-compiler | Reference |
|------------------|-----------|
| `fusion_find_patterns` — pattern matching | [TVM Relay FuseOps Pass](https://tvm.apache.org/docs/arch/pass_infra.html) |
| `FusionPattern` (op sequence → fused op) | [XLA Fusion Pipeline](https://www.tensorflow.org/xla/operation_semantics#fusion) |
| Conv+BN+ReLU fusion | [TVM Auto-scheduling Conv2D](https://tvm.apache.org/docs/how_to/tune_with_autoscheduler/tune_conv2d_layer_cuda.html) |
| Greedy fusion strategy | [XLA HLO Fusion](https://openxla.org/xla/operation_semantics) |
| Memory bandwidth model | [TVM Cost Model for Fusion](https://tvm.apache.org/docs/arch/inferbound.html) |

**Key Resources:**
- [TVM Relay Pass Infrastructure](https://tvm.apache.org/docs/arch/relay_pass_infra.html)
- [XLA: Optimizing Compiler for Machine Learning](https://www.tensorflow.org/xla)
- [Operator Fusion in Deep Learning Compilers (MLSys 2020)](https://proceedings.mlsys.org/paper/2020)

### 4. Layout Optimization → TVM / TensorRT

| mini-ai-compiler | Reference |
|------------------|-----------|
| `DataLayout` (NCHW, NHWC) | [TVM Layout Transformation](https://tvm.apache.org/docs/arch/layout_transformation.html) |
| `LayoutOptimizer` cost model | [TVM AutoTVM Layout Optimization](https://tvm.apache.org/docs/how_to/tune_with_autotvm/index.html) |
| `layout_get_preferred` — GPU prefers NHWC | [NVIDIA TensorRT — Best Practices](https://docs.nvidia.com/deeplearning/tensorrt/developer-guide/index.html) |
| `layout_insert_transpose` | [TVM AlterOpLayout Pass](https://tvm.apache.org/docs/reference/api/python/relay/transform.html) |
| Layout propagation pass | [TVM ConvertLayout Pass](https://tvm.apache.org/docs/arch/layout_transformation.html) |

**Key Resources:**
- [NVIDIA Deep Learning Performance Guide](https://docs.nvidia.com/deeplearning/performance/index.html)
- [TensorRT Developer Guide — Data Formats](https://docs.nvidia.com/deeplearning/tensorrt/developer-guide/index.html#data-format-desc)
- [OneDNN Memory Format (blocked layouts)](https://oneapi-src.github.io/oneDNN/dev_guide_understanding_memory_formats.html)

### 5. Auto-Scheduling → TVM Ansor / Halide

| mini-ai-compiler | Reference |
|------------------|-----------|
| `ScheduleSpace` (split, reorder, unroll) | [TVM Schedule Primitives](https://tvm.apache.org/docs/how_to/work_with_schedules/schedule_primitives.html) |
| `tuner_random_sketch` | [Ansor: Generating High-Performance Tensor Programs](https://arxiv.org/abs/2006.06762) |
| `tuner_evolutionary_search` | [Ansor Evolutionary Search](https://tvm.apache.org/docs/how_to/tune_with_autoscheduler/tune_network_cuda.html) |
| `tuner_evaluate_candidate` — cost model | [TVM Cost Model](https://tvm.apache.org/docs/arch/cost_model.html) |
| Crossover and mutation operators | [Genetic Algorithm for Scheduling](https://tvm.apache.org/docs/arch/auto_scheduler.html) |

**Key Resources:**
- [Ansor Paper (OSDI 2020)](https://www.usenix.org/conference/osdi20/presentation/zheng)
- [TVM Auto-scheduler Tutorial](https://tvm.apache.org/docs/tutorials/index.html#auto-scheduling)
- [Halide Scheduling Language](https://halide-lang.org/tutorials/tutorial_lesson_05_tuning_schedule.html)

## Comparison Summary

| Aspect | MLIR | TVM Relay | XLA HLO | mini-ai-compiler |
|--------|------|-----------|---------|------------------|
| IR level | Multi-level | High-level graph | High-level + operator | Multi-level (simplified) |
| Dialect system | Yes (modular) | Relay IR (fixed) | HLO (fixed) | Yes (3 dialects) |
| Graph representation | Region+Block | Expression DAG | HloComputation | ComputeGraph (adjacency) |
| Fusion | Pattern rewrite | FuseOps pass | HLO fusion pass | Greedy pattern match |
| Layout | Affine/Linalg | AlterOpLayout | Layout assignment | Cost-based selection |
| Auto-scheduling | External (e.g., IREE) | Ansor/AutoTVM | Manual/HLO-level | Evolutionary search |
| Verification | Auto-generated | Type inference | HLO verifier | Manual mlir_verify |
| Code generation | LLVM/GPU backends | BYOC | XLA:CPU/GPU | N/A (educational) |
