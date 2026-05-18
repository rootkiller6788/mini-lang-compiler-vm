# Mini Operator Fusion

> Operator fusion strategies and memory bandwidth optimization for computation graphs

## Overview

Operator fusion is a critical compiler optimization that combines multiple consecutive operations into a single fused kernel. This reduces memory bandwidth consumption by eliminating intermediate tensor reads and writes between the fused operations.

## Fusion Categories

### 1. Vertical Fusion (Producer-Consumer)

Combines operations in a linear chain where the output of one operation feeds directly into the next:

```
Before:  Input -> Conv2D -> [write intermediate] -> [read intermediate] -> BatchNorm
                                                      -> [read] -> ReLU -> Output
After:   Input -> FusedConvBNReLU -> Output
```

Memory savings: Intermediate tensors between Conv2D, BatchNorm, and ReLU are eliminated. Only the Conv2D input filter and final output are read/written to global memory.

### 2. Horizontal Fusion (Parallel Operations)

Merges operations that consume the same input but produce different outputs:

```
Before:  Input -> { MatMul_1 -> ...,
                    MatMul_2 -> ...,
                    MatMul_3 -> ... }
After:   Input -> FusedBatchMatMul -> { ... }
```

Benefit: Shared input loading across multiple operations, reducing redundant memory reads.

### 3. Mixed Fusion

Combines both vertical and horizontal fusion patterns:

```
Before:  X -> Conv -> BN -> Relu
          Y -> Conv -> BN -> Relu
          Z = Add(X_out, Y_out)
After:   {X,Y} -> FusedParallelConvBNRelu -> FusedAdd -> Z
```

## Implemented Fusion Patterns

### Pattern 1: Conv2D + BatchNorm + ReLU

```
sequence: [CONV2D, BATCH_NORM, RELU]
fused_op: FUSED_CONV_BN_RELU
savings: ~67% memory bandwidth reduction
```

**Why this works:**
- Conv2D produces a feature map that is immediately consumed by BatchNorm
- BatchNorm applies per-channel scale/shift, producing output directly fed to ReLU
- All three ops can be fused into a single kernel: after computing each convolution output element, apply BN normalization and ReLU activation inline, then write only the final result

**Mathematical formulation:**
```
y = conv(x, W)
y_norm = gamma * (y - running_mean) / sqrt(running_var + epsilon) + beta
y_relu = max(0, y_norm)

Fused: for each output position:
  acc = sum(x[region] * W[region])
  acc = gamma * (acc - mean) / sqrt(var + epsilon) + beta
  acc = max(0, acc)
  output[position] = acc
```

### Pattern 2: MatMul + Bias + ReLU

```
sequence: [MATMUL, ADD, RELU]
fused_op: FUSED_MATMUL_BIAS_RELU
savings: ~60% memory bandwidth reduction
```

**Why this works:**
- MatMul output is directly consumed by bias addition
- Bias addition result feeds into element-wise ReLU
- All three operations share the same loop structure over the output matrix

**Mathematical formulation:**
```
C = A @ B + bias
C = relu(C)

Fused: for each (i, j):
  c_ij = sum_k A[i,k] * B[k,j] + bias[j]
  c_ij = max(0, c_ij)
  C[i,j] = c_ij
```

### Pattern 3: Element-wise Chain (Add + Mul + Add)

```
sequence: [ADD, MUL, ADD]
fused_op: FUSED_ELEMWISE_CHAIN
savings: ~66% memory bandwidth reduction
```

**Why this works:**
- Element-wise operations require no spatial data reorganization
- Multiple element-wise ops can execute in a single pass over the data
- The compiler can generate a single loop body with all three operations

## Fusion Strategy

### Greedy Fusion Algorithm

```
1. Register all known fusion patterns
2. Repeat until no more fusions:
   a. Scan computation graph topologically
   b. For each node, attempt to match each pattern
   c. If pattern matches, record as candidate
   d. Select best candidate (highest savings)
   e. Replace matched subgraph with fused op
   f. Update graph structure
3. Return fused graph
```

### Pattern Matching

The matching algorithm walks the computation graph searching for consecutive nodes whose operation types match a registered pattern sequence. When a match is found, the subgraph is replaced with a single fused operation.

**Matching constraints:**
- Operations must be in topological order
- Intermediate results must not be used by other operations (single consumer)
- Operation attributes must be compatible (e.g., same data types)

### Cost Model

The fusion cost model estimates memory bandwidth savings:

```
savings = 1 - (fused_memory / original_memory)

where:
  original_memory = sum of sizes of all intermediate tensors
  fused_memory = size of final output only

For a pattern of N operations with intermediate tensors of size S:
  savings = 1 - (S / (N * S)) = 1 - 1/N
```

## Memory Bandwidth Model

### Roofline Analysis

The effectiveness of fusion depends on the arithmetic intensity of the fused operation:

| Metric | Value |
|--------|-------|
| Conv2D arithmetic intensity | ~200 FLOPS/byte |
| ReLU arithmetic intensity | ~0.25 FLOPS/byte |
| Batched MatMul | ~50-100 FLOPS/byte |
| Element-wise ops | ~0.2-0.5 FLOPS/byte |

**Key insight:** Fusing low arithmetic intensity operations (like ReLU, BN, element-wise) with high arithmetic intensity operations (like Conv2D) is highly beneficial because:
1. The low-AI ops are memory-bound — they waste time waiting for data
2. Fusing them with compute-bound ops hides the memory latency
3. The combined kernel becomes compute-bound, utilizing the processor more efficiently

### Bandwidth Savings Calculation

For Conv + BN + ReLU with feature map size H x W x C:

```
Without fusion:
  Write conv output:  H*W*C * 4 bytes
  Read BN input:      H*W*C * 4 bytes
  Write BN output:    H*W*C * 4 bytes
  Read ReLU input:    H*W*C * 4 bytes
  Write ReLU output:  H*W*C * 4 bytes
  Total:              5 * H*W*C * 4 bytes = 20 * H*W*C bytes

With fusion:
  Write fused output: H*W*C * 4 bytes
  Total:              H*W*C * 4 bytes

Savings: (20-4)/20 = 80% reduction in intermediate memory traffic
```

## Limitations and Considerations

1. **Increased register pressure**: Fused kernels use more registers, potentially reducing occupancy on GPUs
2. **Code size**: Generated kernels are larger, increasing instruction cache pressure
3. **Compilation time**: Pattern matching adds overhead to the compilation pipeline
4. **In-place constraints**: Some fusion combinations require careful aliasing analysis
5. **Debugging difficulty**: Fused kernels are harder to debug than individual operations

## Extensions

### Producer-Consumer with Multiple Consumers

When an intermediate tensor has multiple consumers, partial fusion may still be possible:
- Recompute the shared intermediate in each fused kernel
- Fuse with the dominant consumer only
- Use a cost model to decide based on recomputation overhead vs. memory savings

### Cross-operator Fusion

Advanced patterns can fuse across non-linear boundaries:
- Conv2D + MaxPool (share convolution region computation)
- MatMul + Softmax (share reduction step)
- Attention (Q*K^T + Softmax + *V) — fuse all three into one kernel

## References

- TVM Operator Fusion: https://tvm.apache.org/docs/how_to/optimize_operators/opt_conv_cuda.html
- XLA Fusion: https://www.tensorflow.org/xla/operation_semantics#fusion
- Triton Fused Attention: https://triton-lang.org/main/getting-started/tutorials/06-fused-attention.html
- "Operator Fusion in Deep Learning Compilers" — MLSys 2020
