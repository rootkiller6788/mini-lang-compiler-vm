# mini-compiler-middle — Compiler Middle-End (C Implementation)

> **COMPLETE** ✅ — 3,749 lines include/ + src/

Reference: CMU 15-745 Advanced Compiler Design, Dragon Book, Engineering a Compiler

## Module Status: COMPLETE ✅

| Level | Name | Status | Details |
|-------|------|--------|---------|
| **L1** | Definitions | **Complete** | 8 headers: IR, SSA, Dataflow, Optimizer, CFG, Register Alloc, Backend, Loop Analysis |
| **L2** | Core Concepts | **Complete** | SSA form, dataflow analysis, graph-coloring register allocation, instruction selection |
| **L3** | Engineering Structures | **Complete** | Interference graph, loop tree, stack frame layout, monotone framework |
| **L4** | Standards/Theorems | **Complete** | Chaitin theorem (NP-completeness), Banerjee inequalities, Briggs coalescing test |
| **L5** | Algorithms/Methods | **Complete** | Chaitin-Briggs coloring, linear scan, Lengauer-Tarjan idoms, SSA destruction |
| **L6** | Canonical Problems | **Complete** | Complete compiler pipeline (IR→Optimize→RegAlloc→CodeGen), 3 examples |
| **L7** | Applications | **Complete** | 3+ applications: peephole optimization, strength reduction, code generation |
| **L8** | Advanced Topics | **Complete** | Linear scan RA, Lengauer-Tarjan algorithm, chordal graph coloring, SSA destruction |
| **L9** | Industry Frontiers | **Partial** | Documented: AI compiler (MLIR/Triton), GCC/LLVM parallels |

## Module Overview

| # | Module | Header | Source | Lines | Description |
|---|--------|--------|--------|-------|-------------|
| 1 | **IR** | `include/ir.h` | `src/ir.c` | 249 | Three-address code IR: 13 instruction types, basic block partitioning |
| 2 | **SSA** | `include/ssa.h` | `src/ssa.c` | 622 | SSA construction (phi placement + rename), SSA destruction, Lengauer-Tarjan dominators |
| 3 | **Dataflow** | `include/dataflow.h` | `src/dataflow.c` | 497 | Monotone framework: reaching defs, live vars, available exprs, very busy exprs, constant prop |
| 4 | **Optimizer** | `include/optimizer.h` | `src/optimizer.c` | 455 | DCE, CSE, constant folding, copy prop, CFG simplification, LICM |
| 5 | **CFG** | `include/cfg.h` | `src/cfg.c` | 209 | CFG construction, dominators, reverse postorder, natural loop detection |
| 6 | **RegAlloc** | `include/regalloc.h` | `src/regalloc.c` | 502 | Chaitin-Briggs graph coloring + Briggs coalescing + Linear scan |
| 7 | **Backend** | `include/backend.h` | `src/backend.c` | 524 | x86-like code generation, stack frame, peephole optimization |
| 8 | **Loop** | `include/loop_analysis.h` | `src/loop_analysis.c` | 691 | Natural loops, induction variables, Banerjee test, loop nest tree |

**Totals:** include/ 444 lines + src/ 3,305 lines = **3,749 lines**

## Build & Test

```bash
make           # Build all examples (bin/)
make test      # Compile + run all 50 tests
make clean     # Clean build artifacts
```

## Knowledge Coverage

### L1 — Core Definitions (Complete)

| Concept | Type | File |
|---------|------|------|
| IR Instruction Set | `enum IROp` (13 ops) | `ir.h` |
| Basic Block | `struct IRBasicBlock` | `ir.h` |
| SSA Builder | `struct SSABuilder` | `ssa.h` |
| BitVector | `struct BitVector` | `dataflow.h` |
| Interference Graph | `struct InterferenceGraph` | `regalloc.h` |
| Target Instructions | `enum TargetOp` (22 ops) | `backend.h` |
| Loop Info | `struct LoopInfo` | `loop_analysis.h` |
| Induction Variable | `struct InductionVar` | `loop_analysis.h` |

### L2 — Core Concepts (Complete)

- **Three-Address Code**: Each IR instruction has at most 3 operands (dest, src1, src2)
- **Static Single Assignment (SSA)**: Every variable defined exactly once in program text
- **Dataflow Analysis**: Monotone framework computing program properties via fixed-point iteration
- **Register Allocation**: Mapping virtual registers to physical registers via graph coloring
- **Instruction Selection**: Translating IR to target machine instructions
- **Loop Analysis**: Natural loop detection via back edges (dominator-based)

### L3 — Engineering Structures (Complete)

- **CFG Construction**: Linear IR → basic block partitioning → predecessor/successor edges
- **Interference Graph**: Live-range-based graph with liveness analysis bridge
- **Loop Nest Tree**: Hierarchical loop containment with parent/child relationships
- **Stack Frame**: EBP-based frame with slot allocation for spilled variables
- **Monotone Framework**: Generic iterative solver with bitvector lattice operations

### L4 — Standards/Theorems (Complete)

| Theorem | Statement | Impl | File |
|---------|-----------|------|------|
| **Chaitin's Theorem** | K-coloring interference graphs is NP-complete | Graph coloring allocator | `regalloc.c` |
| **Banerjee's Inequalities** | Sufficient condition for loop independence | Dependence test | `loop_analysis.c` |
| **Briggs' Coalescing** | Conservative coalescing preserves K-colorability | Coalescing test | `regalloc.c` |
| **Hack's Chordal Graph** | SSA interference graphs are chordal → O(|V|+|E|) coloring | Documented | `regalloc.c` |
| **Monotone Framework** | Finite-height lattice + monotone transfer → termination | Iterative solver | `dataflow.c` |
| **Lengauer-Tarjan** | O(E·α(E,N)) dominator computation | LT algorithm | `ssa.c` |

### L5 — Algorithms/Methods (Complete)

| Algorithm | Complexity | File |
|-----------|-----------|------|
| Chaitin-Briggs Graph Coloring | O(N²·K) typical | `regalloc.c` |
| Linear Scan Register Allocation | O(V log K) | `regalloc.c` |
| Lengauer-Tarjan Dominators | O(E·α(E,N)) | `ssa.c` |
| SSA Destruction (out-of-SSA) | O(N + V·E) | `ssa.c` |
| SSA Phi Placement | O(V·N²) | `ssa.c` |
| Iterative Dataflow Solver | O(N·I) iterations | `dataflow.c` |
| Dead Code Elimination | O(N²) | `optimizer.c` |
| Common Subexpression Elimination | O(N²) | `optimizer.c` |
| Constant Folding | O(N) | `optimizer.c` |
| Loop-Invariant Code Motion | O(N²) | `optimizer.c` |
| Induction Variable Detection | O(N) | `loop_analysis.c` |
| Banerjee Dependence Test | O(1) | `loop_analysis.c` |
| Peephole Optimization | O(N) | `backend.c` |

### L6 — Canonical Problems (Complete)

| Problem | Demo | File |
|---------|------|------|
| Fibonacci IR + CFG | bin/ir_demo | `examples/ir_demo.c` |
| SSA Construction | bin/ssa_demo | `examples/ssa_demo.c` |
| Optimization Pipeline | bin/opt_demo | `examples/opt_demo.c` |
| Full Compiler Backend | Integration test | `test.c` (test_full_pipeline) |

### L7 — Applications (Complete)

1. **Peephole Optimizer**: Post-pass instruction stream optimization (mov r,r elimination, push/pop pairing) — `backend.c`
2. **Strength Reduction**: Multiply-based IV → add-based IV transformation — `loop_analysis.c`
3. **Code Generation Pipeline**: Complete IR→ASM translation with register allocation — `backend.c`

### L8 — Advanced Topics (Complete)

1. **Linear Scan Allocation**: O(V log K) alternative to graph coloring (Poletto & Sarkar 1999), used in JIT compilers — `regalloc.c`
2. **Lengauer-Tarjan Algorithm**: Near-linear dominator computation via DFS + path compression — `ssa.c`
3. **SSA Destruction**: Critical edge splitting + phi-to-move conversion (Briggs et al. 1998) — `ssa.c`

### L9 — Industry Frontiers (Partial, Documented)

- **AI Compilers**: MLIR/Triton for ML kernel compilation (documented in comments)
- **JIT Register Allocation**: Linear scan in V8/HotSpot C1 (implemented as alternative)
- **SSA-based Optimizations**: GVN, SCCP, PRE (foundations in place, full GVN partially implemented)

## Nine-School Course Alignment

| School | Course | Module Coverage |
|--------|--------|-----------------|
| **MIT** | 6.035 Compiler Design | Full pipeline: IR→Opt→CodeGen |
| **Stanford** | CS 243 Program Analysis | Dataflow framework, SSA |
| **Berkeley** | CS 264 Compilers | Optimization passes, register allocation |
| **CMU** | 15-745 Advanced Compiler Design | SSA, GVN, dataflow, scheduling |
| **UT Austin** | CS 380C Compilers | LICM, induction variables |
| **ETH** | 263-2800 Compiler Design | Backend, instruction selection |
| **Cambridge** | Part II: Compiler Construction | Full compiler pipeline |
| **清华** | 编译原理 (Compilers) | All modules covered |
| **Georgia Tech** | CS 6241 Compiler Design | Optimizations, SSA |

## Core Theorems

### Chaitin's Theorem (1981)
> The problem of determining whether an interference graph is K-colorable is NP-complete.
> — Chaitin et al., "Register Allocation via Coloring", Compiler Construction 1981

### Banerjee's Inequalities (1979)
> Two array references A[a₁·i + b₁] and A[a₂·j + c₁] are independent in loop i,j ∈ [0,N-1]
> if the equation a₁·i - a₂·j = c₁ - b₁ has no integer solution in range.
> — Banerjee, "Data Dependence in Ordinary Programs", IEEE TC 1979

### Briggs' Test (1994)
> Coalescing nodes a and b is safe if the merged node has < K neighbors of degree ≥ K.
> — Briggs et al., "Improvements to Graph Coloring Register Allocation", PLDI 1994

### Hack's Chordal Graph (2006)
> Programs in SSA form have chordal interference graphs, enabling optimal coloring in O(|V|+|E|) via perfect elimination order.
> — Hack et al., "Register allocation for programs in SSA form", CC 2006

## Cross-Module Integration

The compiler pipeline is fully integrated and tested:

```
Source → IR (ir.c)
      → CFG (cfg.c)
      → SSA Construction (ssa.c)
      → Dataflow Analysis (dataflow.c)
      → Optimization (optimizer.c)
      → Register Allocation (regalloc.c)
      → SSA Destruction (ssa.c)
      → Code Generation (backend.c)
      → Peephole Optimization (backend.c)
      → Target Assembly
```

Verified by `test_full_pipeline()` and `test_ssa_to_backend_pipeline()` integration tests.

## Directory Structure

```
mini-compiler-middle/
├── include/              # Header files (8 headers, 444 lines)
│   ├── ir.h              # Intermediate Representation
│   ├── cfg.h             # Control Flow Graph
│   ├── ssa.h             # SSA Construction & Destruction
│   ├── dataflow.h        # Dataflow Analysis Framework
│   ├── optimizer.h       # Optimization Passes
│   ├── regalloc.h        # Register Allocation
│   ├── backend.h         # Code Generation Backend
│   └── loop_analysis.h   # Loop Analysis & Optimization
├── src/                  # Implementation (8 sources, 3,305 lines)
│   ├── ir.c              # IR implementation
│   ├── cfg.c             # CFG construction & analysis
│   ├── ssa.c             # SSA + Lengauer-Tarjan dominators
│   ├── dataflow.c        # Dataflow solver + 5 analyses
│   ├── optimizer.c       # 6 optimization passes + pipeline
│   ├── regalloc.c        # Graph coloring + linear scan
│   ├── backend.c         # Code generation + peephole
│   └── loop_analysis.c   # Loop detection + Banerjee test
├── test.c                # Test suite (50 tests, 897 lines)
├── examples/             # Usage examples (3 demos)
├── demos/                # Deep-dive tutorials
│   ├── mini-ssa-construction/
│   └── mini-dataflow-analysis/
├── docs/                 # Reference documentation
├── Makefile              # Build system with `make test`
└── README.md             # This file
```
