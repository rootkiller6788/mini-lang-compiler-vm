# Dataflow Analysis Framework

> Dataflow analysis is a technique for gathering information about the possible set of values calculated at various points in a program. This document describes the monotone dataflow framework implemented in `src/dataflow.c`.

## The Monotone Framework

A dataflow analysis is defined by four components:

1. **Direction**: Forward (information flows from predecessors to successors) or Backward (from successors to predecessors)
2. **Lattice/domain**: The set of possible values (often bitvectors representing sets)
3. **Meet operator** (∧ or ⊓): How to combine information from multiple paths
4. **Transfer function**: How a node transforms incoming information into outgoing information

### Formal Definition

A dataflow problem is a tuple (L, ⊓, F) where:
- **L** is a complete lattice (or semilattice)
- **⊓**: L × L → L is the meet operator
- **F**: a set of monotone transfer functions f: L → L

For forward problems:
```
IN[B]  = ⊓{OUT[P] : P ∈ pred(B)}
OUT[B] = f_B(IN[B])
```

For backward problems:
```
OUT[B] = ⊓{IN[S] : S ∈ succ(B)}
IN[B]  = f_B(OUT[B])
```

### Monotonicity

A function f is **monotone** if x ⊑ y implies f(x) ⊑ f(y). All transfer functions in our framework are monotone, which guarantees termination of the iterative algorithm.

## The Iterative Algorithm

The classic worklist-based iterative solver:

```
for each block B:
    if forward:  IN[B] = ⊥ (bottom) or ⊤ (top)
    if backward: OUT[B] = ⊥ or ⊤
changed = true
while changed:
    changed = false
    for each block B (in appropriate order):
        if forward:
            IN[B] = ⊓{OUT[P] : P ∈ pred(B)}
            old = OUT[B]
            OUT[B] = f_B(IN[B])
        else:
            OUT[B] = ⊓{IN[S] : S ∈ succ(B)}
            old = IN[B]
            IN[B] = f_B(OUT[B])
        if OUT[B] ≠ old (or IN[B] ≠ old): changed = true
```

The algorithm always terminates for monotone frameworks on finite-height lattices. The number of iterations depends on the lattice height and graph structure.

## Bitvector Operations

Our implementation uses bitvectors for efficient set operations:

```
#define BITVECTOR_WORDS ((MAX_BLOCKS + 31) / 32)

typedef struct {
    uint32_t bits[BITVECTOR_WORDS];
} BitVector;
```

Operations provided:
- `bv_init` — Clear to zero
- `bv_set`, `bv_clear`, `bv_test` — Individual bit manipulation
- `bv_union`, `bv_intersect` — Set operations
- `bv_copy`, `bv_equals` — Copy and comparison
- `bv_print` — Pretty printing

Using 32-bit words gives us 32 bits per word. For 128 blocks, we need 4 words (128 bits). Union and intersection operate on 128 bits in constant time (4 word operations), making the solver efficient.

## Implemented Analyses

### 1. Reaching Definitions (Forward, May)

**Purpose**: Determine which definitions may reach each program point.

**Lattice**: Power set of all definitions (bitvector where bit i = 1 means definition i may reach this point).

**Meet operator**: ∪ (union). A definition reaches if it reaches via ANY path.

**Transfer function**: `OUT[B] = GEN[B] ∪ (IN[B] - KILL[B])`

- **GEN[B]**: Definitions created in B that reach the end of B (not overwritten later in B)
- **KILL[B]**: Definitions of variables that are redefined in B

**Initialization**: IN[entry] = ∅ (bottom element for union)

**Usage**: Used for use-def chains, constant propagation setup, and detecting uninitialized variables.

**Example**:
```
BB0: x = 1       (def 0)
BB1: y = x + 1   (def 1)
BB2: x = 2       (def 2)    BB3: z = x   (def 3)
BB4: ... use x, y, z ...
```

At BB4 entry, reaching definitions: {def 1, def 2, def 3}. Def 0 is killed by def 2.

### 2. Live Variables (Backward, May)

**Purpose**: Determine which variables may be live (used before being redefined) at each program point.

**Lattice**: Power set of variables (bitvector where bit v = 1 means variable v may be live).

**Meet operator**: ∪ (union). A variable is live if it's live along ANY successor path.

**Transfer function**: `IN[B] = USE[B] ∪ (OUT[B] - DEF[B])`

- **USE[B]**: Variables used in B before any definition
- **DEF[B]**: Variables defined in B before any use

**Initialization**: IN[exit] = ∅

**Usage**: Register allocation (determines when registers can be freed), dead code elimination.

**Example**:
```
BB0: a = 1
BB1: b = a + 2
BB2: c = b + 3    BB3: d = c
BB4: return c
```

Working backward from BB4:
- BB4 (exit): OUT = ∅, USE = {c}, DEF = ∅ → IN = {c}
- BB3: OUT = IN[BB4] = {c}, USE = ∅, DEF = {d} → IN = {c}
- BB2: OUT = IN[BB4] = {c}, USE = {b}, DEF = {c} → IN = {b}
- BB1: OUT = IN[BB2] ∪ IN[BB3] = {b, c}, USE = {a}, DEF = {b} → IN = {a, c}
- BB0: OUT = IN[BB1] = {a, c}, USE = ∅, DEF = {a} → IN = {c}

Variable 'd' is never live — a candidate for dead code elimination.

### 3. Available Expressions (Forward, Must)

**Purpose**: Determine which expressions have been computed and their operands haven't changed since.

**Lattice**: Power set of expressions.

**Meet operator**: ∩ (intersection). An expression is available only if it's available along ALL paths.

**Transfer function**: `OUT[B] = GEN[B] ∪ (IN[B] - KILL[B])`

- **GEN[B]**: Expressions computed in B whose operands are not subsequently killed
- **KILL[B]**: Expressions whose operands are redefined in B

**Initialization**: IN[entry] = ∅, OUT[others] = ⊤ (all expressions) for maximal fixed point

**Usage**: Common subexpression elimination, redundant expression elimination.

### 4. Constant Propagation (Forward, Must)

**Purpose**: Determine which variables have constant values at each program point.

**Lattice**: For each variable: ⊥ (undefined), constant value c, or ⊤ (non-constant).

The lattice has infinite height (infinite possible constant values), so the standard iterative algorithm on individual variables may not terminate. We use a worklist of SSA edges or widen at merge points.

**Meet operator**: If all incoming values are the same constant c, result is c. Otherwise, ⊤ (non-constant).

**Implementation**: Our implementation uses a simple iterative propagation over the SSA form, which guarantees termination because each variable is defined exactly once.

## Implementation in `src/dataflow.c`

### `df_analyze` — Generic Solver

The generic solver takes a `DataflowAnalysis` type and computes IN/OUT sets for all blocks:

```
void df_analyze(IRFunction* func, IRBasicBlock blocks[], int num_blocks,
                DataflowAnalysis type, DataflowResult* result);
```

It automatically selects:
- Forward vs. backward direction
- Union vs. intersection meet operator
- Appropriate initialization (bottom or top)

### Iteration Order Matters

The order in which blocks are processed significantly affects convergence speed. For forward problems, processing blocks in **reverse postorder** (RPO) is optimal — it ensures that predecessors are processed before successors (when the graph is reducible).

For backward problems, reverse RPO (i.e., postorder) is optimal.

Our solver uses the natural block ordering, which works correctly but may require more iterations than RPO ordering.

### Termination Guarantee

All our analyses use finite-height lattices:
- Bitvectors over N elements: height N
- The meet operators are monotone
- Transfer functions are monotone

Therefore, the iterative algorithm always terminates. The maximum number of iterations is bounded by the lattice height times the number of blocks.

## Practical Considerations

### Efficient Bitvector Representation

For N ≤ 128 (our MAX_BLOCKS), each bitvector is 128 bits = 4 × 32-bit words. Operations are just a few machine instructions each.

### Time Complexity

- Per iteration: O(B × W) where B is number of blocks and W is bitvector words
- Number of iterations: typically 2-4 for structured code, up to O(N) worst case
- Total: O(B × W × I) where I is number of iterations

### Space Complexity

- Each DataflowResult stores 2 × B bitvectors (IN and OUT for each block)
- For 128 blocks: 128 × 2 × 128 bits = 32,768 bits ≈ 4 KB

## Extensions

### Interprocedural Analysis

Extending dataflow across function boundaries requires modeling call and return edges. This leads to the **call-strings** approach or the **functional approach** for context sensitivity.

### Conditional Constant Propagation

Combines constant propagation with unreachable code elimination using SSA edges. Uses a three-valued lattice (⊥, constant, ⊤) and the meet operator:

```
⊥ ⊓ c = c⊥ ⊓ ⊤ = ⊤
c1 ⊓ c2 = c1 (if c1 == c2) or ⊤ (otherwise)
```

## References

1. Kildall, G.A. "A Unified Approach to Global Program Optimization." POPL 1973.
2. Kam, J.B., Ullman, J.D. "Monotone Data Flow Analysis Frameworks." Acta Informatica, 1977.
3. Marlowe, T.J., Ryder, B.G. "Properties of Data Flow Frameworks." Acta Informatica, 1990.
4. Nielson, F., Nielson, H.R., Hankin, C. "Principles of Program Analysis." Springer, 1999.
5. Aho, A.V., Lam, M.S., Sethi, R., Ullman, J.D. "Compilers: Principles, Techniques, and Tools."

## Worklist Algorithm Variant

The iterative algorithm processes all blocks each iteration. A more efficient variant uses a **worklist**:

```
worklist = {entry}
while worklist not empty:
    B = remove_block(worklist)
    old_OUT = OUT[B]
    IN[B] = meet{OUT[P] : P in pred(B)}
    OUT[B] = f_B(IN[B])
    if OUT[B] != old_OUT:
        for each successor S of B:
            add S to worklist
```

Only blocks whose inputs changed trigger reprocessing of successors. This is typically 2-5x faster than the full iterative approach.

## Debugging Dataflow Analyses

### Issue 1: Non-termination

**Symptom**: The while loop never exits because `changed` remains true.

**Root Cause**: The meet operator is not monotone, or the transfer functions are not monotone on the lattice.

**Debugging Strategy**: 
1. Verify that for all x ⊑ y implies f(x) ⊑ f(y)
2. Check that the lattice height is finite
3. For bitvector implementations, ensure the direction (union vs. intersection) matches the "may" vs. "must" semantics of the analysis
4. Add an iteration counter with a hard limit (e.g., 1000 iterations) to prevent infinite loops

### Issue 2: Empty IN/OUT Sets

**Symptom**: All IN and OUT sets are empty after analysis.

**Root Cause**: Incorrect initialization — either bottom elements used where top was needed, or vice versa. For forward union (may) analyses like Reaching Definitions, IN[entry] should be ∅. For forward intersection (must) analyses like Available Expressions, IN[entry] should be ∅ but OUT[other blocks] should be ⊤ (universal set).

**Debugging Strategy**:
1. Print which initialization strategy is being used
2. Verify the first iteration: does any block produce non-empty output?
3. Check if the transfer function is correctly computing GEN and KILL sets

### Issue 3: Incorrect Meet Semantics

**Symptom**: Analysis is overly conservative (too many values reported) or overly optimistic (missing values).

**Root Cause**: Using intersection where union is needed, or vice versa.

| Analysis | Direction | Meet | Why |
|----------|-----------|------|-----|
| Reaching Defs | Forward | Union | A def may reach if it reaches via ANY path |
| Live Variables | Backward | Union | A var is live if it's live along ANY path |
| Available Exprs | Forward | Intersection | An expr is available only if it's available on ALL paths |
| Very Busy Exprs | Backward | Intersection | An expr is very busy only if it's needed on ALL paths |

### Issue 4: Handling SSA Phi Functions

**Symptom**: Dataflow analysis produces incorrect results around phi functions.

**Root Cause**: φ-functions are both definitions (of their destination) and uses (of their source operands). Failing to account for both can cause incorrect GEN/KILL computation.

**Solution**:
- Treat φ dest as a definition (kills previous definitions of the same variable)
- Treat φ sources as uses (like any other instruction's operands)
- In liveness analysis: a φ dest is a DEF, φ sources are USEs

### Issue 5: Large Bitvectors

**Symptom**: Analysis is slow or memory-intensive.

**Root Cause**: Bitvector width grows with the number of definitions (for reaching defs) or variables (for liveness), which can be thousands in real programs.

**Optimizations**:
1. **Sparse analysis**: Use linked lists instead of bitvectors for sparse data
2. **SSA-based analysis**: Exploit the SSA property (one definition per variable) to reduce problem size
3. **Segmented bitvectors**: Split bitvectors into chunks, only allocate non-zero chunks
4. **SIMD**: Use 128-bit or 256-bit SIMD operations for bitvector union/intersection

### Convergence Rate

The number of iterations depends on:
1. **Graph structure**: Deep loop nests require more iterations (one per nesting level in worst case)
2. **Processing order**: Reverse postorder for forward analyses, postorder for backward
3. **Lattice height**: Higher lattices may require more iterations

Typical convergence:
- Structured code (if/while): 2-3 iterations
- Goto-heavy code: 4-6 iterations
- Irreducible flow graphs: up to O(N) iterations

### CFG Reducibility

A CFG is **reducible** if it can be reduced to a single node by repeatedly applying:
1. T1: Remove a self-loop
2. T2: Remove a node with a single predecessor, merging it with that predecessor

Irreducible graphs contain multiple-entry loops (e.g., from goto into a loop body). Most programs from structured languages produce reducible CFGs.

For reducible CFGs, reverse postorder is optimal for forward dataflow — it visits nodes in topological order of the dominator tree, so information flows forward in a single pass.

### Testing the Solver

A comprehensive test suite should include:

```
test_reaching_defs_linear():     // Sequential code, verify def propagation
test_reaching_defs_branch():     // Branch with merge, verify union at join
test_live_variables_simple():    // Single use-def pair, verify backward propagation
test_live_variables_loop():      // Loop with carried dependency, verify union at loop header
test_available_exprs_kill():     // Expression killed by operand redefinition
test_constant_prop_basic():      // Constants from literals propagate through moves/arithmetic
test_constant_prop_loop_carried(): // Loop-carried constant (phi convergence)
test_convergence():              // Verify fixed point after limited iterations
```

