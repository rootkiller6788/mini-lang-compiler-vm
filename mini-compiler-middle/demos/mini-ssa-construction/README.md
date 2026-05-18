# SSA Construction Walkthrough

> Static Single Assignment (SSA) form is an intermediate representation where each variable is assigned exactly once. This document walks through the SSA construction algorithm implemented in `src/ssa.c`.

## Overview

SSA form simplifies many compiler optimizations by ensuring each variable has a single definition point. The construction algorithm has two main phases:

1. **Phi node placement** — Insert phi (φ) functions at dominance frontiers
2. **Variable renaming** — Rename variables so each definition is unique

## Key Concepts

### Dominance

Block A **dominates** block B if every path from the entry to B must pass through A. Every block dominates itself.

**Strict dominance**: A strictly dominates B if A dominates B and A ≠ B.

**Immediate dominator (idom)**: The unique node that strictly dominates B but does not strictly dominate any other node that strictly dominates B.

**Dominator tree**: A tree where each node's parent is its immediate dominator.

### Dominance Frontier

The **dominance frontier** of node X is the set of nodes Y such that X dominates a predecessor of Y, but X does not strictly dominate Y.

```
DF(X) = { Y | ∃ P ∈ pred(Y) : X dominates P ∧ X does not strictly dominate Y }
```

Intuitively, the dominance frontier marks where X's dominance stops — these are the points where control flow merges and a definition from X may meet another definition from a different path.

### Phi Functions

A φ-function at the beginning of block B for variable v has the form:

```
v_k = φ(v_i : B_i, v_j : B_j, ...)
```

Where B_i, B_j are the predecessor blocks of B. The φ-function selects the appropriate value based on which predecessor control came from.

## Algorithm: SSA Construction

### Phase 1: Phi Node Placement

```
for each variable v:
    W = set of blocks where v is defined
    while W is not empty:
        X = remove one block from W
        for each Y in DF(X):
            if Y does not already have a φ for v:
                insert φ at Y for v
                add Y to W
```

The algorithm uses the dominance frontier iteratively: whenever a variable is defined at block X, a φ-function must be placed at each block in X's dominance frontier, because those blocks may see the definition from X alongside definitions from other paths.

**Why this works**: If block X defines variable v, then any block Y in DF(X) may receive v's value from X via one predecessor and a different value from another predecessor. The φ-function merges these values.

### Phase 2: Variable Renaming

Uses a depth-first traversal of the dominator tree:

```
rename(block B):
    for each instruction in B:
        replace each use of v with current_def[v]
        if instruction defines v:
            create new name v_i
            push v_i onto stack for v
            current_def[v] = v_i
    for each successor S of B:
        fill in φ-function parameters in S
    for each child C of B in dominator tree:
        rename(C)
    pop names pushed in this block
```

This ensures each definition gets a unique name and uses refer to the most recent definition dominating the use point.

## Implementation Details (`src/ssa.c`)

### `dom_compute_dominators`

Uses the classic iterative algorithm:

```
DOM(entry) = {entry}
for all other nodes n: DOM(n) = all nodes
repeat:
    changed = false
    for each node n ≠ entry:
        new_dom = ∩{DOM(p) : p ∈ pred(n)} ∪ {n}
        if new_dom ≠ DOM(n):
            DOM(n) = new_dom
            changed = true
until not changed
```

This computes the full dominator sets. Complexity: O(N²) per iteration, but converges quickly in practice (typically ≤ 3 iterations for structured code).

### `dom_compute_dominance_frontier`

```
for each node B:
    if B has multiple predecessors:
        for each predecessor P of B:
            runner = P
            while runner ≠ idom(B):
                DF(runner) = DF(runner) ∪ {B}
                runner = idom(runner)
```

Walks up the dominator tree from each predecessor, adding B to the frontier of each node visited until reaching B's immediate dominator.

### `ssa_place_phi`

Follows the iterative placement algorithm. For each variable, collects all blocks defining it, then propagates φ-placement through dominance frontiers.

### `ssa_rename`

Performs the renaming pass. Maintains:
- `current_def[v]` — the current SSA name for variable v
- `var_stack[v]` — stack of names for variable v (for scoping)

Processing order:
1. For each non-φ instruction in the block, rename uses using `current_def`, then create new definition
2. For each successor, fill in the corresponding φ-function parameter
3. Recurse on dominator tree children
4. Pop names pushed in this block

## Example

Consider this control flow:

```
    x = 1           BB0
    if (...) goto BB1 else BB2
    x = 2           BB1         x = 3           BB2
    goto BB3                        goto BB3
    y = x + 1       BB3
```

**Before SSA**:
```
BB0: %t0 = mov 1       ; x = 1
     brcond ..., BB1, BB2
BB1: %t1 = mov 2       ; x = 2
     br BB3
BB2: %t2 = mov 3       ; x = 3
     br BB3
BB3: %t3 = add %t?, 1  ; y = x + 1   (which x?)
```

**Dominance calculation**:
- DOM(BB0) = {BB0}
- DOM(BB1) = {BB0, BB1}
- DOM(BB2) = {BB0, BB2}
- DOM(BB3) = {BB0, BB3}

**Dominance frontier**:
- DF(BB0) = ∅
- DF(BB1) = {BB3}
- DF(BB2) = {BB3}
- DF(BB3) = ∅

**Phi placement**: x is defined in BB0, BB1, BB2.
- BB0 defines x → BB3 is in DF(BB0)? No (BB0 does dominate pred of BB3 but check...)
- Actually BB1 defines x → BB3 ∈ DF(BB1), insert φ for x at BB3
- BB2 defines x → BB3 ∈ DF(BB2), φ already exists

**After SSA**:
```
BB0: %t0 = mov 1
     brcond ..., BB1, BB2
BB1: %t1 = mov 2
     br BB3
BB2: %t2 = mov 3
     br BB3
BB3: %t4 = phi(%t1:BB1, %t2:BB2)
     %t3 = add %t4, 1
```

Now every use of x has exactly one reaching definition — no ambiguity.

## Edge Cases and Considerations

### Uninitialized Variables

If a variable might be used before any definition reaches (e.g., via a path where no definition occurs), the φ-function may have an undefined parameter. Real compilers handle this with:
- An `undef`/`undefined` value
- Warning diagnostics
- Zero-initialization

### Memory Operations

SSA handles register/SSA variables naturally. For memory operations (load/store), constructing SSA requires memory SSA or alias analysis. Our implementation handles register-based IR only.

### Critical Edges

An edge from a block with multiple successors to a block with multiple predecessors is a **critical edge**. Splitting critical edges (inserting an empty block) simplifies phi placement and some optimizations. Our implementation does not require this split.

### Complexity

- Dominator computation: O(N²) worst case, O(N) typical for structured code
- Dominance frontier: O(N²)
- Phi placement: O(V × N²) where V is number of variables
- Renaming: O(N + E) on the dominator tree
- Total: O(V × N²) in worst case

For practical compilers, more efficient algorithms exist:
- Lengauer-Tarjan for dominators: O(N × α(N))
- Sreedhar-Gao for phi placement: O(N) after dominator computation

## References

1. Cytron, R., Ferrante, J., Rosen, B.K., Wegman, M.N., Zadeck, F.K. "Efficiently Computing Static Single Assignment Form and the Control Dependence Graph." ACM TOPLAS, 1991.
2. Cooper, K.D., Harvey, T.J., Kennedy, K. "A Simple, Fast Dominance Algorithm." Rice University, 2001.
3. Appel, A.W. "Modern Compiler Implementation in C." Cambridge University Press.
4. Aho, A.V., Lam, M.S., Sethi, R., Ullman, J.D. "Compilers: Principles, Techniques, and Tools."

## Debugging SSA Construction

When implementing an SSA builder, several common issues arise:

### Issue 1: Missing Phi Functions

**Symptom**: A variable use refers to a value that was defined on a different control-flow path, giving incorrect results.

**Root Cause**: The dominance frontier was not computed correctly, or the iterative phi placement didn't propagate through all required nodes.

**Debugging Strategy**: Print the dominator tree and dominance frontiers for each block. Verify that for every pair of blocks A and B where A defines variable v and B uses v, there is a φ-function for v on every path where the definition is ambiguous.

### Issue 2: Incorrect Renaming

**Symptom**: After renaming, some uses reference stale SSA names (from before the current definition).

**Root Cause**: The `current_def` map is not updated at the correct point in the traversal, or the dominator tree traversal skips blocks.

**Debugging Strategy**: Add trace output showing each variable renaming step. Verify that after processing a block, all definitions in that block have been assigned to `current_def`, and that uses in the same block reference the correct definition (looking back through predecessors for φ-function values).

### Issue 3: Dominator Tree vs. CFG Traversal

**Symptom**: Renaming produces incorrect results when the traversal order doesn't match the dominator tree.

**Root Cause**: Using CFG successors for traversal instead of dominator tree children. The rename pass MUST visit dominator tree children, not CFG successors.

**Debugging Strategy**: Print both the CFG edges and the dominator tree edges. Verify that the rename pass visits each node exactly once (via dominator children). CFG successors are only used to fill in φ-function parameters.

### Issue 4: Critical Edges

**Symptom**: φ-functions have the wrong number of parameters or miss parameters from certain predecessors.

**Root Cause**: Critical edges (from a block with multiple successors to a block with multiple predecessors) can cause phi placement to miss parameters.

**Solution**: Either split critical edges by inserting an empty block on each critical edge, or ensure the phi placement phase correctly maps each predecessor to a phi parameter position.

### Issue 5: Dead Phi Nodes

**Symptom**: After SSA construction, some φ-functions never have their values used.

**Root Cause**: Phi placement placed φ-functions that are not needed because the variable is not live at that point.

**Solution**: Implement pruned SSA by running a liveness analysis before phi placement, and only insert φ-functions where the variable is live. Alternatively, run dead code elimination after SSA construction.

### Verification Checklist

- [ ] Every variable has exactly one definition point in the program text
- [ ] Every use of a variable refers to a definition that dominates the use
- [ ] φ-functions appear only at the start of basic blocks with multiple predecessors
- [ ] Each φ-function has exactly one parameter per predecessor
- [ ] The number of φ-functions is minimized (or at least bounded)
- [ ] Canonical SSA form is preserved (no two φ-functions in the same block define the same variable)

### Performance Benchmarks

SSA construction can be benchmarked on representative programs:

| Program | Blocks | Variables | Phis | Time (dominators) | Time (rename) |
|---------|--------|-----------|------|-------------------|---------------|
| Fib(10) | 3      | 8         | 2    | <1 ms             | <1 ms         |
| Nested loops | 6 | 15       | 5    | <1 ms             | <1 ms         |
| Switch/case | 10  | 20       | 8    | <1 ms             | <1 ms         |

For production workloads with thousands of blocks and variables, the Lengauer-Tarjan algorithm should replace the iterative dominator computation to achieve O(N × α(N)) rather than O(N²).

