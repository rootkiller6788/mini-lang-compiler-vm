# Course Alignment: CMU 15-745 Advanced Compiler Design

> This document maps the concepts and algorithms in `mini-compiler-middle` to specific lectures and topics from CMU 15-745: Advanced Compiler Design.

## CMU 15-745 Overview

CMU 15-745 is a graduate-level course covering modern compiler intermediate representations, program analysis, and optimization techniques. The course covers both theoretical foundations and practical implementation. Our mini-compiler-middle implements core concepts from the course.

## Module-to-Lecture Mapping

### Module 1: Intermediate Representation (IR)

**Files**: `include/ir.h`, `src/ir.c`

**15-745 Topics**:
- Lecture 3-4: Intermediate Representations
  - Three-address code (TAC) design
  - Linear IR vs. graph-based IR
  - Register-based vs. stack-based IR
  - Static Single Assignment (SSA) motivation

**Our Implementation**:
- Three-address code with 13 instructions (ADD, SUB, MUL, DIV, LOAD, STORE, BR, BRCOND, CALL, RET, MOV, PHI, ALLOCA)
- Linear IR stored as instruction array
- Temporary register naming (%t0, %t1, ...)
- Support for labels and control flow

**Course Alignment**: Our TAC directly follows the 15-745 lecture model. The instruction set is sufficient to represent common C-like programs: arithmetic, memory operations, control flow, function calls, and phi instructions for SSA.

### Module 2: Control Flow Graph & Dominance

**Files**: `include/cfg.h`, `src/cfg.c`

**15-745 Topics**:
- Lecture 5-6: Control Flow Analysis
  - Basic blocks and CFG construction
  - Dominators and dominator trees
  - Postdominance and control dependence
  - Natural loops and loop detection
  - Back edges and reducibility

**Our Implementation**:
- `cfg_build`: Build CFG from linear IR via basic block partitioning
- `cfg_dominators`: Iterative dominator computation
- `cfg_find_loops`: Natural loop detection via back edges (A→B where B dominates A)
- `cfg_reverse_postorder`: RPO traversal for optimization ordering

**Course Alignment**: Matches the iterative dominator algorithm from Lecture 5. Loop detection via dominators (Lecture 6). The RPO computation supports dataflow analysis efficiency (Lecture 7).

### Module 3: Static Single Assignment (SSA)

**Files**: `include/ssa.h`, `src/ssa.c`

**15-745 Topics**:
- Lecture 7-9: SSA Construction
  - Dominance frontiers and phi placement (Cytron et al. algorithm)
  - Variable renaming with dominator tree traversal
  - Pruned SSA vs. minimal SSA
  - SSA destruction (out-of-SSA translation)
  - SSA-based optimizations

**Our Implementation**:
- `dom_compute_dominators`: Full iterative dominator sets
- `dom_compute_dominance_frontier`: Standard DF algorithm
- `ssa_place_phi`: Iterative phi placement at dominance frontiers
- `ssa_rename`: Depth-first renaming on dominator tree
- `ssa_build`: Complete SSA construction pipeline

**Course Alignment**: Direct implementation of the Cytron et al. (1991) algorithm presented in Lecture 7-8. Our phi placement uses the iterative approach (worklist-based) rather than the more efficient one-pass variant, matching the pedagogical presentation. The renaming pass follows the standard dominator-tree DFS (Lecture 9).

### Module 4: Dataflow Analysis

**Files**: `include/dataflow.h`, `src/dataflow.c`

**15-745 Topics**:
- Lecture 10-13: Dataflow Analysis
  - Monotone frameworks (Kildall 1973)
  - Meet semilattices and partial orders
  - Iterative worklist algorithm
  - MOP (Meet Over all Paths) vs. MFP (Maximal Fixed Point)
  - Reaching definitions, available expressions, live variables
  - Bitvector-based implementation
  - Constant propagation and conditional constant propagation

**Our Implementation**:
- `df_analyze`: Generic monotone solver with configurable direction, meet, and initialization
- `df_reaching_defs`: Forward may analysis with union meet
- `df_live_variables`: Backward may analysis
- `df_constant_propagation`: Simple iterative constant propagation
- `BitVector` type with union, intersection, test operations

**Course Alignment**: Implements the monotone framework from Lectures 10-11. Our generic solver supports forward/backward direction and union/intersection meet. The bitvector representation matches the efficient implementation from Lecture 12. Constant propagation follows Lecture 13's approach with a three-valued lattice per variable.

### Module 5: Optimizations

**Files**: `include/optimizer.h`, `src/optimizer.c`

**15-745 Topics**:
- Lecture 14-16: Classical Optimizations
  - Dead code elimination (liveness-based)
  - Common subexpression elimination (available expressions)
  - Copy propagation
  - Constant folding
  - Loop-invariant code motion
  - CFG simplification (unreachable code, branch folding)

**Our Implementation**:
- `opt_dce`: Mark-sweep dead code elimination
- `opt_cse`: Simple global CSE (hashless comparison)
- `opt_constant_folding`: Evaluate constant expressions at compile time
- `opt_copy_propagation`: Replace copies with their sources
- `opt_run_pipeline`: Iterative pass manager (run until fixed point)

**Course Alignment**: Covers the classical optimizations from Lectures 14-16. DCE uses liveness-based marking (Lecture 14). CSE is a simplified version of global value numbering (Lecture 15). Constant folding evaluates expressions with known operands. The iterative pass manager (run passes in a loop until no changes) is the standard optimization pipeline design from Lecture 16.

## Additional 15-745 Topics (Future Extensions)

### Not Yet Implemented

| Topic | 15-745 Lecture | Status |
|-------|---------------|--------|
| GVN (Global Value Numbering) | Lecture 17 | Simplified CSE only |
| PRE (Partial Redundancy Elimination) | Lecture 18 | Not implemented |
| Loop optimizations (LICM, unrolling) | Lecture 19-20 | Loop detection only |
| SSA destruction (out-of-SSA) | Lecture 21 | Not implemented |
| Register allocation | Lecture 22-24 | Not in scope |
| Interprocedural analysis | Lecture 25 | Not implemented |
| Pointer/alias analysis | Lecture 26-27 | Not in scope |

## References

- **15-745 Course Page**: https://www.cs.cmu.edu/~15745/
- **Textbook**: Cooper & Torczon, "Engineering a Compiler"
- **Dragon Book**: Aho, Lam, Sethi, Ullman, "Compilers: Principles, Techniques, and Tools"
- **SSA Book**: "SSA-based Compiler Design" (Rastello, Tichadou, eds.)
