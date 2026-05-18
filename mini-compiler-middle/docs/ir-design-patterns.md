# IR Design Patterns

> Intermediate Representation (IR) design is a critical architectural decision in compiler construction. This document surveys IR design patterns and describes the choices made in `mini-compiler-middle`.

## What is an IR?

An Intermediate Representation (IR) is a program representation that sits between the source language and target machine code. A good IR should be:

1. **Expressive**: Capable of representing all source language constructs
2. **Language-independent**: Not tied to a specific source language
3. **Analysis-friendly**: Easy to analyze (dataflow, control flow, dependence)
4. **Transformable**: Easy to rewrite and optimize
5. **Lowerable**: Easy to translate to machine code
6. **Compact**: Memory efficient for large programs

## IR Taxonomy

### By Level of Abstraction

| Level | Examples | Characteristics |
|-------|----------|-----------------|
| **HIR** (High-level IR) | AST, GIMPLE, CIL | Close to source, retains types, scoping |
| **MIR** (Mid-level IR) | LLVM IR, FIRM, libFirm | Type-annotated, SSA, machine-independent |
| **LIR** (Low-level IR) | Three-address code, RTL | Register-oriented, close to machine |

Our mini-compiler implements a **mid-level linear IR** with SSA support.

### By Structure

#### Stack-based IR

Instructions operate on an implicit stack. Used by JVM bytecode, .NET CIL, WebAssembly.

```
push 1      ; push constant 1
push 2      ; push constant 2
add         ; pop two, push sum
store x     ; pop and store to x
```

**Pros**: Very compact, easy to generate
**Cons**: Implicit operands complicate analysis; must simulate stack for dataflow

#### Register-based IR (Three-Address Code)

Each instruction names its operands explicitly. Used by LLVM, GCC, most research compilers.

```
%t1 = add i32 1, 2
%t2 = load i32, i32* %ptr
%t3 = mul i32 %t1, %t2
```

**Pros**: Explicit data dependences, easy to analyze and transform
**Cons**: More verbose, more memory, more naming work

#### Graph-based IR

Program represented as a graph of nodes (operations, basic blocks, functions) with edges (control flow, data flow, memory dependence). Used by Java HotSpot, V8 TurboFan.

```
[Node:Add] --data--> [Node:Mul]
     |                     |
  control              control
     |                     |
[Node:Store] <--control--/
```

**Pros**: Rich structural information, efficient for certain analyses
**Cons**: Complex, high memory overhead, harder to print/debug

#### Hybrid IR

Most production compilers use multiple IRs:
- **Clang/LLVM**: AST → LLVM IR → SelectionDAG → MachineInstr
- **GCC**: GENERIC → GIMPLE → RTL
- **Rustc**: AST → HIR → MIR → LLVM IR
- **Go**: AST → SSA → machine code

## Our Design: Linear Three-Address Code

### Instruction Set

```
typedef enum {
    IR_ADD,     // %t_dest = %t_src1 + %t_src2
    IR_SUB,     // %t_dest = %t_src1 - %t_src2
    IR_MUL,     // %t_dest = %t_src1 * %t_src2
    IR_DIV,     // %t_dest = %t_src1 / %t_src2
    IR_LOAD,    // %t_dest = *(%t_src1)
    IR_STORE,   // *(%t_dest) = %t_src1
    IR_BR,      // goto label
    IR_BRCOND,  // if %t_src1 then goto label1 else goto label2
    IR_CALL,    // %t_dest = call %t_src1(%t_src2)
    IR_RET,     // return %t_src1
    IR_MOV,     // %t_dest = %t_src1 (copy)
    IR_PHI,     // %t_dest = phi(%t_src1:label1, %t_src2:label2)
    IR_ALLOCA   // %t_dest = alloca %t_src1 (stack allocation)
} IROp;
```

### Design Rationale

**Why 13 instructions?** This is the minimal set needed to represent C-like programs with functions, control flow, and memory. It maps cleanly to:
- **Arithmetic**: ADD/SUB/MUL/DIV cover basic arithmetic
- **Memory**: LOAD/STORE/ALLOCA model a flat address space
- **Control flow**: BR/BRCOND handle conditional and unconditional branches
- **Functions**: CALL/RET support function calls
- **SSA**: PHI nodes enable SSA construction
- **Convenience**: MOV simplifies copy propagation

**Why register-based?** Explicit operands make dataflow analysis straightforward. Each instruction's sources and destinations are explicit in the instruction structure, enabling:
- Def-use chains by scanning instruction operands
- Liveness analysis through bitvector operations on register indices
- Value numbering through operand pattern matching

**Why linear?** A flat array of instructions is:
- Simple to traverse, modify, and print
- Easy to serialize/deserialize
- Efficient to iterate over (good cache locality)
- Sufficient for basic block partitioning and CFG construction

### Register Naming Convention

Temporary registers are named `%t0`, `%t1`, ... as they are created:

```
int ir_new_temp(IRFunction* func) {
    return func->next_temp++;
}
```

Each temp is a unique integer. The `next_temp` counter ensures uniqueness — useful for SSA renaming where each definition creates a new name.

### Control Flow Representation

Labels are integers. Branch targets reference labels:

```
brcond %t0, 1, 2   ; if %t0, goto label 1 (true), else goto label 2 (false)
br 1                 ; unconditional goto label 1
```

This flat labeling scheme (rather than block references) simplifies IR construction and serialization. Basic blocks are derived by scanning for branch targets and terminators.

## SSA Properties

SSA (Static Single Assignment) form is a variant of TAC where:
- Each variable is assigned exactly once in the program text
- φ-functions merge values at join points

### Minimal SSA

A program is in **minimal SSA** if φ-functions are inserted only at dominance frontiers of definitions. This is the standard form produced by our `ssa_build`.

### Pruned SSA

**Pruned SSA** removes φ-functions for variables that are never live at the insertion point. This reduces unnecessary φ-nodes but requires liveness information.

Our implementation produces minimal SSA (not pruned), which is sufficient for most analyses.

### SSA Destruction

After optimizations, SSA must be destroyed (translated back to non-SSA form) before code generation. Common techniques:
- **Naive**: Replace each φ with copies in predecessor blocks
- **CSSA (Conventional SSA)**: Ensure no interference before parallel copy insertion
- **SSI (Static Single Information)**: Extended form with sigma nodes

Our implementation does not include SSA destruction (out of scope for the middle-end).

## IR Design Trade-offs

### Linear vs. Graph

| Aspect | Linear (array) | Graph (linked) |
|--------|---------------|----------------|
| Insertion/deletion | O(N) shift | O(1) with pointers |
| Traversal | O(N), good locality | O(N), pointer chasing |
| Memory | Compact (no pointers) | Higher (pointers per node) |
| Serialization | Trivial | Requires graph serialization |
| Debugging | Easy to print | Requires graph walking |

**Verdict**: Linear IR is simpler for a teaching compiler. Production compilers often use a hybrid: linear per-block, graph for inter-block.

### Register-based vs. Stack-based

| Aspect | Register-based | Stack-based |
|--------|---------------|-------------|
| Code size | Larger | Smaller |
| Analysis clarity | Excellent | Poor (implicit stack) |
| Generation | Slightly harder | Trivial |
| Optimization | Natural | Requires stack simulation |

**Verdict**: Register-based IR is the standard for optimizing compilers. Stack-based is used primarily for distribution formats (JVM, .NET, Wasm).

## Future Extensions

### Typed IR

Add type information to temporaries:
```
%t0:i32 = add %t1:i32, %t2:i32
```

Enables type-based optimizations and simplifies lowering.

### Memory SSA

Model memory as an SSA variable:
```
%mem1 = store %mem0, %ptr, %val
%t1 = load %mem1, %ptr
```

Enables memory dependence analysis and redundant load elimination.

### Region-based IR

Group basic blocks into regions (loop bodies, if-then-else, etc.) for structural analysis and optimization.

### Machine-specific Lowering

Extend with addressing modes, calling conventions, register constraints, and instruction selection.

## References

1. Muchnick, S.S. "Advanced Compiler Design and Implementation." Morgan Kaufmann, 1997.
2. Lattner, C., Adve, V. "LLVM: A Compilation Framework for Lifelong Program Analysis & Transformation." CGO 2004.
3. Click, C., Paleczny, M. "A Simple Graph-Based Intermediate Representation." IR '95.
4. Cytron, R. et al. "Efficiently Computing SSA Form..." ACM TOPLAS, 1991.
5. Cooper, K.D., Torczon, L. "Engineering a Compiler." Morgan Kaufmann, 2nd Ed., 2011.
