# AI Compiler Landscape

> Survey of production AI compiler frameworks: architectures, IR designs, and optimization strategies

## 1. XLA (Accelerated Linear Algebra)

**Organization:** Google / OpenXLA  
**Website:** https://github.com/openxla/xla

### Architecture

XLA takes ML model graphs (from TensorFlow, JAX, PyTorch) and compiles them into optimized executables for CPUs, GPUs, and TPUs.

```
Input: HLO (High-Level Optimizer) IR
  |
  v
Target-independent optimizations (CSE, fusion, algebraic simplification)
  |
  v
Target-dependent lowering (layout assignment, fusion heuristics)
  |
  v
Linalg on buffers / GPU code generation (LLVM IR / PTX)
  |
  v
Executable
```

### IR Design: HLO

HLO is a static, single-assignment IR with explicit operation semantics:

- **HloModule** — top-level compilation unit
- **HloComputation** — a function body (list of instructions)
- **HloInstruction** — a single operation with typed operands
- **FusionInstruction** — multiple fused operations in one kernel

### Key Features

- **Ahead-of-Time (AOT) Compilation**: Compile entire model before execution
- **Operation Fusion**: Combines element-wise and reduction ops into single kernels
- **Layout Assignment**: Chooses optimal data layout (NHWC vs NCHW) per operator
- **Memory Planning**: Buffer liveness analysis and reuse; pre-allocates buffers
- **Algebraic Simplification**: Fold constants, simplify broadcasts, remove identity ops

### Graph-Level vs Operator-Level

XLA is primarily **graph-level**: It optimizes the entire computation graph holistically. TensorFlow/XLA integrates graph-level and operator-level by:
1. Running XLA HLO optimizations on subgraphs
2. Falling back to TensorFlow eager execution for unsupported operations
3. Combining XLA-compiled subgraphs with framework operators

## 2. TVM (Tensor Virtual Machine)

**Organization:** Apache TVM  
**Website:** https://tvm.apache.org

### Architecture

TVM separates computation description from schedule optimization:

```
Input: Relay IR (high-level) / TIR (low-level)
  |
  v
Graph-level optimizations (fusion, layout, memory planning)
  |
  v
Operator-level auto-tuning (AutoTVM, Ansor)
  |
  v
Code generation (LLVM, CUDA, OpenCL, Metal, Vulkan, WebGPU, ...)
  |
  v
Runtime (GraphExecutor, VM, AOT)
```

### IR Design: Relay

Relay is a functional, statically-typed intermediate representation:

- **Expression-based**: Computation is described as expressions (not instructions)
- **Type system**: Tensor types with shapes, function types, ADT support
- **Let-binding**: Control flow and variable binding
- **Pattern matching**: First-class pattern language for rewriting

```
fn (%data: Tensor[(1, 3, 224, 224), float32]) {
  %0 = nn.conv2d(%data, %weight, strides=[1, 1], padding=[1, 1])
  %1 = nn.batch_norm(%0, %gamma, %beta, %mean, %var)
  %2 = nn.relu(%1)
  %2
}
```

### Key Features

- **BYOC (Bring Your Own Codegen)**: Hardware vendors can plug in custom code generators
- **Ansor Auto-scheduler**: Generates high-performance schedules via evolutionary search
- **AutoTVM**: Template-based auto-tuning with ML cost models
- **Unified IR (Relay + TIR)**: High-level graph ops lower to low-level loop nests
- **MicroTVM**: Bare-metal deployment on microcontrollers

### Graph-Level vs Operator-Level

TVM is both **graph-level** and **operator-level**:
- Relay optimizations run at the graph level (fusion, inlining, layout)
- AutoTVM/Ansor run at the operator level (tuning individual convolution schedules)
- The two levels are connected through TIR lowering

## 3. MLIR (Multi-Level Intermediate Representation)

**Organization:** LLVM Foundation  
**Website:** https://mlir.llvm.org

### Architecture

MLIR provides infrastructure for building domain-specific compilers at multiple levels of abstraction:

```
TF Graph -> TF Dialect -> HLO Dialect -> Linalg Dialect -> Affine/SCF -> LLVM Dialect -> LLVM IR
                                                    |
                                              GPU Dialect -> NVVM/ROCDL
```

### IR Design

- **Operations, Regions, Blocks**: Nested IR structure supporting arbitrary control flow
- **Dialects**: Extensible namespace system for domain-specific operations
- **Type system**: Recursive, parametric types; first-class function types
- **Pass infrastructure**: Declarative pass pipeline with analysis management

### Key Features

- **Progressive Lowering**: Multiple intermediate dialects between framework and hardware
- **Dialect Conversion**: Framework for legalizing ops between dialects
- **Pattern Rewriting**: Declarative IR transformations (DAG-to-DAG)
- **Open compilation**: IREE, CIRCT, Torch-MLIR, ONNX-MLIR built on MLIR

### Graph-Level vs Operator-Level

MLIR is fundamentally **multi-level**:
- TF/HLO dialects = graph level
- Linalg = structured operations (operator level)
- Affine/SCF = loop level
- LLVM/GPU/NVVM = instruction level

## 4. Triton

**Organization:** OpenAI  
**Website:** https://triton-lang.org

### Architecture

Triton is a language and compiler for writing highly efficient GPU kernels:

```
Triton Language (Python DSL) -> Triton-IR -> Triton-GPU-IR -> LLVM IR + PTX
```

### Key Features

- **Block-level programming**: Programmers reason about tile/blocks, not individual threads
- **Automatic memory coalescing**: Compiler handles shared memory allocation and coalescing
- **Fused attention**: FlashAttention-2 implemented in ~50 lines of Triton
- **Just-in-time compilation**: Kernels compiled at runtime for specific input shapes

### Graph-Level vs Operator-Level

Triton is **operator-level**: It compiles individual kernels but does not perform graph-level optimizations. Integration with PyTorch inductor enables graph-level fusion.

## 5. ONNX Runtime & ONNX-MLIR

**Organization:** Microsoft / Linux Foundation  
**Website:** https://onnxruntime.ai

### Architecture

```
ONNX Model -> Graph Optimizer -> Execution Provider -> Hardware
                                  |
                           CUDA / TensorRT / OpenVINO / DNNL / custom
```

### Key Features

- **Graph optimizations**: Constant folding, node fusion, layout transformation
- **Execution Providers (EPs)**: Pluggable backends for different hardware
- **ONNX-MLIR**: Compiling ONNX through MLIR for ahead-of-time optimization

### Graph-Level vs Operator-Level

ONNX Runtime is **graph-level**: Optimizations run on the ONNX graph before dispatching to operator-level execution providers.

## 6. TensorRT

**Organization:** NVIDIA  
**Website:** https://developer.nvidia.com/tensorrt

### Architecture

```
Framework Model -> ONNX/TensorFlow -> TensorRT Builder -> Optimized Engine -> Runtime
```

### Key Features

- **Layer & tensor fusion**: Aggressive vertical and horizontal fusion
- **Precision calibration**: INT8 and FP16 quantization with calibration
- **Kernel auto-tuning**: Selects best kernel implementation for target GPU
- **Dynamic shapes**: Support for variable input dimensions
- **Plugin system**: Custom layer implementations

### Graph-Level vs Operator-Level

TensorRT is **graph-level**: Heavily optimized at the graph level with 50+ graph optimization passes, then generates optimized kernels for the fused operations.

## 7. Glow (Facebook/Meta)

**Organization:** Meta (Facebook)  
**Website:** https://github.com/pytorch/glow

### Architecture

```
Caffe2/ONNX -> Glow Graph -> High-Level IR -> Low-Level IR -> Machine Code
                                    |
                              Graph optimizations
                              (fusion, DCE, CSE, layout)
```

### Key Features

- **Two-phase IR**: High-level (graph) and low-level (instruction) IR
- **Ahead-of-time compilation**: Compile entire model for inference servers
- **Quantization-aware lowering**: Float-to-int8 with profile-guided quantization
- **Interpreter**: Model can run through interpreter for debugging

## Comparison Table

| Compiler | Organization | IR | Graph Opt | Operator Opt | Auto-Tuning | Backends |
|----------|-------------|-----|-----------|--------------|-------------|----------|
| **XLA** | Google | HLO | Yes (fusion, layout, simplification) | Limited (XLA:GPU) | No | CPU, GPU, TPU |
| **TVM** | Apache | Relay + TIR | Yes (fusion, layout, memory) | Yes (AutoTVM, Ansor) | Yes | CPU, GPU, FPGA, micro |
| **MLIR** | LLVM | Multi-dialect | Progressive lowering framework | Via dialects | External (IREE) | Any via LLVM |
| **Triton** | OpenAI | Triton-IR | No (integrated via PyTorch) | Yes (kernel-level) | No | NVIDIA GPU |
| **ONNX RT** | Microsoft | ONNX Graph | Yes (constant fold, fusion) | Via EPs | No | CPU, GPU, VPU, NPU |
| **TensorRT** | NVIDIA | Layer Graph | Yes (aggressive fusion) | Yes (kernel selection) | Yes | NVIDIA GPU |
| **Glow** | Meta | High/Low IR | Yes (fusion, DCE, CSE) | Yes | No | CPU, GPU (limited) |

## Key Architectural Patterns

### Graph-Level Optimizations (common across all frameworks)

1. **Constant folding**: Evaluate compile-time-known subgraphs
2. **Dead code elimination**: Remove unused computations
3. **Common subexpression elimination**: Deduplicate redundant ops
4. **Operation fusion**: Combine consecutive ops into single kernels
5. **Layout assignment**: Choose data format (NCHW vs NHWC) per operation
6. **Memory planning**: Pre-allocate and reuse buffers based on liveness analysis
7. **Algebraic simplification**: Apply mathematical identities (x*1=x, x+0=x)

### Operator-Level Optimizations (TVM, Triton, TensorRT)

1. **Loop tiling (blocking)**: Split loops for cache locality
2. **Loop reordering**: Change loop nest order for memory access patterns
3. **Vectorization**: Use SIMD instructions (AVX, NEON, warp-level)
4. **Unrolling**: Reduce loop overhead for small loop bounds
5. **Memory hierarchy**: Shared memory, registers, texture memory
6. **Thread/block mapping**: Map computation to GPU thread hierarchy
7. **Pipeline synchronization**: Software pipelining of memory and compute

### The Golden Triangle

The three forces that shape AI compiler design:

```
                    Expressiveness
                         ▲
                        / \
                       /   \
                      /     \
                     /       \
                    /         \
                   /   AI      \
                  /  Compiler   \
                 /               \
                /                 \
               /                   \
              /                     \
    Performance ◄──────────────────► Portability
```

- **Expressiveness**: Support for diverse model architectures and operations
- **Performance**: Competitive with hand-tuned libraries (cuDNN, MKL)
- **Portability**: Deploy on diverse hardware (training GPUs, inference accelerators, edge devices, mobile)

## Recommended Reading

1. **XLA**: [XLA Architecture](https://www.tensorflow.org/xla/architecture) — operation semantics and HLO
2. **TVM**: [TVM: An Automated End-to-End Optimizing Compiler for Deep Learning (OSDI 2018)](https://arxiv.org/abs/1802.04799)
3. **MLIR**: [MLIR: A Compiler Infrastructure for the End of Moore's Law (arXiv 2020)](https://arxiv.org/abs/2002.11054)
4. **Triton**: [Triton: An Intermediate Language and Compiler for Tiled Neural Network Computations (MAPS 2019)](https://www.eecs.harvard.edu/~htk/publication/2019-mapl-tillet-kung-cox.pdf)
5. **Ansor**: [Ansor: Generating High-Performance Tensor Programs for Deep Learning (OSDI 2020)](https://www.usenix.org/conference/osdi20/presentation/zheng)
6. **Halide**: [Halide: A Language and Compiler for Optimizing Parallelism, Locality, and Recomputation (PLDI 2013)](https://people.csail.mit.edu/jrk/halide-pldi13.pdf)
