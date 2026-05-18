# Mini C Subset Interpreter — Demo

> A tree-walking AST interpreter for a restricted C-like language, demonstrating environment-based evaluation, stack frames, and control flow.

## Overview

This demo implements a minimal interpreter for a C-like language with the following features:

- **Expression Evaluation** — Integer arithmetic (`+`, `-`, `*`, `/`, `%`) with operator precedence
- **Variable Management** — Dynamic environment model using linked-list frames with name→value bindings
- **Control Flow** — `while` loops and `if`/`else` conditional branching
- **Function Calls** — Named functions with parameters and local variable scoping
- **Data Types** — Integer, float, char, pointer, array (1D), and struct declarations
- **AST Representation** — Union-based abstract syntax tree nodes for all language constructs

## Architecture

```
Source Text
    │
    ▼
┌──────────────┐     ┌──────────────┐
│  Lexer/Parser │────▶│   AST Nodes  │
│  (c_parse_*)  │     │ (ASTNode*)   │
└──────────────┘     └──────┬───────┘
                            │
                            ▼
                     ┌──────────────┐     ┌──────────────┐
                     │   Evaluator  │────▶│  Environment  │
                     │ (c_eval_*,   │     │  (CVar list)  │
                     │  c_execute_*)│     └──────────────┘
                     └──────────────┘
                            │
                            ▼
                     ┌──────────────┐
                     │   Output     │
                     └──────────────┘
```

### Component Details

#### 1. AST Node Types (`ASTNodeType`)

| Node Type | Purpose | Example |
|-----------|---------|---------|
| `NODE_INT` | Integer literal | `42` |
| `NODE_FLOAT` | Float literal | `3.14` |
| `NODE_VAR` | Variable reference | `counter` |
| `NODE_BINOP` | Binary operation | `a + b`, `x * y` |
| `NODE_CALL` | Function call | `fib(10)` |
| `NODE_IF` | Conditional | `if (x > 0) ...` |
| `NODE_WHILE` | Loop | `while (i < n) ...` |
| `NODE_BLOCK` | Statement block | `{ ... }` |
| `NODE_ASSIGN` | Assignment | `x = 5` |
| `NODE_RETURN` | Return statement | `return x` |
| `NODE_INDEX` | Array indexing | `arr[i]` |
| `NODE_MEMBER` | Struct member access | `p.name` |

#### 2. C Type System (`CType`)

```
T_INT    — Signed integer values, default type for arithmetic
T_FLOAT  — Floating point values
T_CHAR   — Character values
T_PTR    — Pointer to any type
T_ARRAY  — Single-dimensional array with size
T_STRUCT — Composite type with named fields
```

#### 3. Environment Model (`CVar`)

The environment is a singly-linked list of variable bindings. Each frame contains:
- **Variable name** — Identifier string (64 char max)
- **Type annotation** — One of the `CType` values
- **Value union** — Tagged union of int, float, char, or pointer
- **Array size** — For `T_ARRAY` typed variables
- **Next pointer** — Links to parent scope

```
Global scope:  [x=10] → [y=20] → NULL
                     ↓ (push frame)
Function scope:  [n=5] → [temp=0] → [x=10] → [y=20] → NULL
```

Variable lookup traverses the chain from innermost to outermost scope, implementing lexical shadowing.

#### 4. Function Representation (`CFunc`)

Each function is described by:
- Name identifier
- Return type annotation
- Parameter list (names and types)
- AST body (the function's statement block)
- Linked-list next pointer for program function table

#### 5. Program Structure (`CProgram`)

The top-level container holds:
- `globals` — Head of the global variable list
- `functions` — Head of the function definition list

## Evaluation Algorithm

### Expression Evaluation (`c_eval_expr`)

1. **Leaf nodes**: Return literal values directly
2. **Variable nodes**: Look up name in the current environment chain
3. **Binary operations**: Recursively evaluate left and right children, then apply the operator
4. **Function calls**:
   - Look up function by name in the function table
   - Evaluate argument expressions in the caller's environment
   - Create a new local environment frame by binding parameters to argument values
   - Execute the function body against this new frame
   - Return the result (or 0 if void)

### Statement Execution (`c_execute_statement`)

1. **Assignment**: Evaluate right-hand side, find or create variable binding, store value
2. **While loop**: Repeatedly evaluate the condition; if truthy, execute the body; then re-evaluate condition
3. **If/else**: Evaluate condition; execute then-branch if truthy, else-branch otherwise
4. **Block**: Iterate through statement list, executing each sequentially

### Safety Limits

- While loops have a maximum iteration count of 10,000 to prevent infinite loops
- All dynamic allocations are tracked for proper cleanup

## Usage Example

```c
// Build a fibonacci function programmatically
CFunc *fib = make_fib_func();  // a=0; b=1; i=0; while(i<n) { temp=a+b; a=b; b=temp; i=i+1; } return a;

// Create environment with parameter
CVar *env = NULL;
CVar *param = create_var("n", 10);
param->next = env;
env = param;

// Evaluate in environment
int result = eval_function_body(fib->body, env);
// result == 55

// Print all bindings
c_print_env(env);
```

## Key Design Patterns

### Tree-Walking Interpreter

The evaluator recursively descends through the AST, computing values at leaf nodes and combining them at internal nodes. This pattern is:
- **Simple** — Direct mapping from AST structure to evaluation logic
- **Readable** — Each node type has a clear, localized evaluation strategy
- **Extensible** — New node types simply add a new case to the switch statement

### Environment-Based Scoping

Rather than mutating a global symbol table, each function call creates a new environment frame. This naturally implements:
- **Lexical scope** — Inner frames shadow outer bindings
- **Stack discipline** — Frames are created at call time, freed at return
- **Closure-like behavior** — Future extension to capture free variables

### Union-Based AST

The AST uses a tagged union (type enum + union of structs) which is:
- **Memory efficient** — Only one variant is stored per node
- **Type safe** — The type tag prevents misinterpretation of union data
- **C99 compatible** — Uses anonymous unions and designated initializers where available

## Comparison with Other Approaches

| Approach | Pros | Cons |
|----------|------|------|
| **Tree-walk** (this demo) | Simple, direct, debuggable | Slow for repeated evaluation |
| **Bytecode compiler** | Fast execution, compact | Requires compilation step |
| **JIT compiler** | Very fast, adaptive | Complex implementation |
| **Stack-based VM** | Portable, simple ISA | Less direct than tree-walk |

## Potential Extensions

1. **Type checking** — Verify operand types before evaluation
2. **Break/continue** — Loop control flow statements
3. **Multi-dimensional arrays** — Nested array indexing
4. **Pointer arithmetic** — Full pointer operations
5. **Struct member assignment** — L-value struct field access
6. **Error reporting** — Source location tracking in AST nodes
7. **Debug info** — Stack trace on runtime errors

## References

- Scott, M.L. *Programming Language Pragmatics*, Chapter 4: Semantic Analysis
- Abelson, Sussman. *Structure and Interpretation of Computer Programs*, Chapter 4: Metalinguistic Abstraction
- Aho, Lam, Sethi, Ullman. *Compilers: Principles, Techniques, and Tools*, Chapter 6: Intermediate Code Generation

## File Structure

```
include/
  └── c_subset.h        # Type definitions and API declarations
src/
  └── c_subset.c        # Full implementation (~250 lines)
examples/
  └── c_interp_demo.c   # Fibonacci demo with environment visualization
```

## Building

```bash
make
./bin/c_interp_demo
```
