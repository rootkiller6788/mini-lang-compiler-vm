# Mini MLIR Dialect

> Simplified MLIR-style Intermediate Representation dialect system in C99

## Overview

This module implements a lightweight, text-format-compatible Intermediate Representation inspired by MLIR (Multi-Level Intermediate Representation). It provides the foundational infrastructure for building compiler dialects, operations, regions, and blocks.

## Architecture

### Core Concepts

#### Operations (MLIROp)

An operation is the fundamental unit of execution in MLIR. Each operation has:

- **name**: human-readable identifier
- **operands**: input values with associated types
- **results**: output values with associated types
- **attributes**: compile-time constant metadata (int, float, string)
- **location**: source location for debugging and diagnostics
- **dialect**: namespace qualifier (e.g., `arith`, `memref`, `func`)

Example SSA form:
```
%0 = "arith.addi"(%arg0, %arg1) : (i32, i32) -> i32
```

#### Values (MLIRValue)

Values are SSA (Static Single Assignment) variables with an associated type. Each value is produced by exactly one operation result and may be consumed by multiple operation operands.

#### Types (MLIRType)

Supported types:
- **Integer types**: i8, i16, i32, i64
- **Float types**: f32, f64
- **Memory reference types**: memref<?xi32>, memref<?xf32>
- **None type**: for operations with no meaningful type

#### Attributes (MLIRAttribute)

Key-value metadata attached to operations. Three value kinds:
1. Integer attributes (key=%d)
2. Float attributes (key=%g)
3. String attributes (key="value")

Example: `{kernel_size = 3, stride = 1, padding = "SAME"}`

### Blocks (MLIRBlock)

A block is a sequence of operations with an optional argument list. Blocks form a control-flow graph where each block may have:
- Block arguments (equivalent to phi nodes in SSA)
- Terminator operation (branch, return, etc.)
- Predecessor and successor blocks

### Regions (MLIRRegion)

A region contains a list of blocks. The first block is the entry block. Regions are attached to operations that need nested control flow or isolated scopes (e.g., `func.func`, `scf.if`, `scf.for`).

### Dialects (MLIRDialect)

A dialect groups related operations under a namespace. Dialects define:
- Operation names and signatures
- Type constraints
- Verification rules
- Lowering and optimization passes

## Implemented Dialects

### 1. Builtin Dialect

Foundation types and attributes shared across all dialects.

### 2. Arith Dialect (Arithmetic)

Integer and floating-point arithmetic operations:
- `arith.addi` — integer addition
- `arith.muli` — integer multiplication
- `arith.subi` — integer subtraction
- `arith.divsi` — signed integer division
- `arith.addf` — floating-point addition
- `arith.mulf` — floating-point multiplication

### 3. MemRef Dialect (Memory Reference)

Memory allocation and access operations:
- `memref.alloc` — allocate uninitialized memory
- `memref.load` — load value from memory
- `memref.store` — store value to memory
- `memref.dealloc` — free allocated memory
- `memref.subview` — create a view into a slice of a memref

### 4. Func Dialect (Function)

Function definition and call operations:
- `func.func` — define a function with signature
- `func.return` — return from function
- `func.call` — call a function
- `func.constant` — define a compile-time constant

## Type System

### Type Hierarchy
```
Type
├── IntegerType (i8, i16, i32, i64)
├── FloatType (f32, f64)
├── MemRefType (memref<shape x element_type>)
├── NoneType (no value)
└── FunctionType (input types -> output types)
```

### Type Checking

The `mlir_verify` function performs basic type checking:
1. All operands must have non-NONE type
2. All results must have non-NONE type
3. Dialect and operation name must be non-empty
4. Note: Type compatibility between connected ops is not enforced in this simplified model; a full implementation would track SSA def-use chains

## Pass Infrastructure

### Transformation Passes

Compilation flows through a series of passes, each transforming the IR:

1. **Canonicalization** — simplify and normalize operations
2. **CSE (Common Subexpression Elimination)** — remove redundant computations
3. **Inlining** — inline function bodies at call sites
4. **Loop optimization** — unrolling, fusion, tiling
5. **Lowering** — translate from higher-level to lower-level dialects
6. **Bufferization** — convert tensors to memrefs

### Pass Manager (conceptual)

```
Pipeline: Canonicalize -> Inline -> CSE -> LoopFusion -> LowerToAffine -> LowerToLLVM
```

### Pattern Rewriting

Operations are transformed using pattern-match-and-replace:
1. Match a subgraph of operations against a pattern
2. Verify operands, types, and attributes
3. Replace matched operations with a new (simplified/fused/lowered) operation

## Traits and Interfaces

Operations may implement interfaces that enable generic pass behavior:

| Trait | Description |
|-------|-------------|
| Commutative | Operands may be swapped without changing semantics |
| Idempotent | Repeated application produces same result |
| Elementwise | Maps one-to-one over tensor elements |
| Broadcastable | Supports NumPy-style broadcasting |
| MemoryEffect | Read/Write/Allocate/Free effects |
| ConstantLike | Produces a compile-time constant |

## IR Printing Format

### Custom Format (MLIR Text)

```
module {
  func.func @main(%arg0: i32, %arg1: i32, %arg2: i32) -> i32 {
    %0 = arith.addi %arg0, %arg1 : i32
    %1 = arith.muli %0, %arg2 : i32
    %2 = arith.addi %1, %arg0 : i32
    func.return %2 : i32
  }
}
```

Format components:
- `module { }` — top-level container
- `func.func @name(args)` — function with typed arguments
- `%N = dialect.op operands : types` — SSA operation result
- `func.return %val : type` — function return

## Usage Example

```c
#include "mlir_dialect.h"

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

## Verification

The simplified verifier checks:
- All operands have assigned types
- All results have assigned types
- Operation and dialect names are non-empty string

A production verifier would additionally check:
- Operand types match the operation's type constraints
- Result types are consistent with producers
- Control flow is well-formed (blocks have terminators)
- Dominance property holds (all uses dominated by definitions)
- Memory effects are correctly modeled

## Relationship to Real MLIR

| Feature | Real MLIR | This Implementation |
|---------|-----------|-------------------|
| ODS (Operation Definition Spec) | TableGen | Manual C structs |
| Dialect loading | Dynamic plugin (.so/.dll) | Static registration |
| Type system | Recursive, parametric | Fixed enum |
| Pass infrastructure | Full pass manager | Manual pass application |
| Verifier | Auto-generated from ODS | Manual check function |
| Use-def chains | Built-in IR infrastructure | Not tracked |
| Location tracking | Source/file/line/column | String location |
| Symbol table | Multi-level symbol resolution | Not implemented |

## Design Decisions

1. **C99 + libc only**: Maximizes portability; no C++ STL, Boost, or third-party dependencies
2. **Static arrays with bounds**: Avoids dynamic allocation for most IR elements, reducing fragmentation
3. **String-based value references**: Simpler than pointer-based SSA use-def chains for demonstration
4. **Enum type system**: Sacrifices parametric types for implementation simplicity
5. **Location as string**: Models MLIR's file:line:col without full location infrastructure

## Extending with New Dialects

To add a new dialect:

1. Define operation signatures in the dialect header
2. Implement `mlir_create_op` calls for each operation
3. Register operations in `mlir_context_create`
4. Implement custom verification if needed
5. Add IR printing support for the dialect
6. Write lowering/optimization passes

## References

- MLIR Language Reference: https://mlir.llvm.org/docs/LangRef/
- MLIR Dialects: https://mlir.llvm.org/docs/Dialects/
- MLIR Tutorial (Toy): https://mlir.llvm.org/docs/Tutorials/Toy/
- Operation Definition Specification (ODS): https://mlir.llvm.org/docs/DefiningDialects/Operations/
