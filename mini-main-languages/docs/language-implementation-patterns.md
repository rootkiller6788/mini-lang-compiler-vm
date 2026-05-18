# Language Implementation Patterns

> Interpreter and compiler design patterns used across the mini-main-languages project.

## Interpreter Architectures

### 1. AST Walker (Tree-Walking Interpreter)

The simplest interpreter pattern. An AST walker recursively traverses the abstract syntax tree, evaluating each node according to its type.

**Used in**: `c_subset.c`, `ml_like.c`

```
eval(Node):
    switch Node.type:
        INT:     return Node.value
        VAR:     return env.lookup(Node.name)
        BINOP:   l = eval(Node.left); r = eval(Node.right)
                 return APPLY(Node.op, l, r)
        CALL:    fn = env.lookup(Node.name)
                 args = map(eval, Node.args)
                 return fn.apply(args)
        IF:      if eval(Node.cond) then eval(Node.then) else eval(Node.else)
        WHILE:   while eval(Node.cond): eval(Node.body)
```

| Advantages | Disadvantages |
|---|---|
| Simple to implement | Slow for repeated execution |
| Direct 1:1 AST mapping | No optimization opportunity |
| Easy to debug | Interpreter overhead per node |
| Good for teaching | Memory: AST must be retained |

### 2. Bytecode Compiler + Stack VM

Compile source to bytecode instructions, then execute on a stack-based virtual machine.

**Architecture**:

```
Source → Parser → AST → Code Generator → Bytecode → VM Interpreter → Output
```

**Bytecode Instruction Set** (example):

| Instruction | Stack Effect | Description |
|---|---|---|
| `PUSH_INT n` | `→ n` | Push integer literal |
| `PUSH_VAR name` | `→ val` | Push variable value |
| `STORE_VAR name` | `val →` | Pop and store to variable |
| `ADD` | `b a → a+b` | Pop two, push sum |
| `SUB` | `b a → a-b` | Pop two, push difference |
| `MUL` | `b a → a*b` | Pop two, push product |
| `CALL fn nargs` | `args → result` | Call function with n args |
| `RET` | `val →` | Return from function |
| `JMP offset` | `→` | Unconditional jump |
| `JMP_IF_FALSE off` | `cond →` | Conditional jump |
| `DUP` | `a → a a` | Duplicate top |

**Stack-Based Evaluation**:

```
Expression: (a + b) * (c - d)
Bytecode:   PUSH_VAR a, PUSH_VAR b, ADD, PUSH_VAR c, PUSH_VAR d, SUB, MUL
Stack trace:
  [a]
  [a, b]
  [a+b]
  [a+b, c]
  [a+b, c, d]
  [a+b, c-d]
  [(a+b)*(c-d)]
```

| Advantages | Disadvantages |
|---|---|
| Fast execution | Compilation step required |
| Compact bytecode | Two-pass architecture |
| Enables optimizations | More complex implementation |
| Portable VM | Indirect mapping to source |

### 3. Register-Based VM

Instead of an operand stack, use virtual registers for intermediate values.

**Example**:

```
Expression: (a + b) * (c - d)
Register code:
  r0 = load a
  r1 = load b
  r0 = r0 + r1
  r1 = load c
  r2 = load d
  r1 = r1 - r2
  r0 = r0 * r1
  return r0
```

| Advantages | Disadvantages |
|---|---|
| Fewer instructions | Larger instruction format |
| Closer to real hardware | Register allocation needed |
| Better for JIT | More complex code gen |

## REPL Architecture

Read-Eval-Print Loop is the standard interactive language interface.

```
┌─────────────────────────────────────┐
│               REPL Loop             │
│                                     │
│  ┌──────────┐   ┌──────────┐       │
│  │  READ    │──▶│  PARSE   │       │
│  │ (stdin)  │   │ (s-expr) │       │
│  └──────────┘   └────┬─────┘       │
│                      │              │
│                      ▼              │
│                ┌──────────┐         │
│                │   EVAL   │◀──┐     │
│                │(env+AST) │   │     │
│                └────┬─────┘   │     │
│                     │         │     │
│                     ▼         │     │
│               ┌──────────┐   │     │
│               │  PRINT   │   │     │
│               │ (stdout) │   │     │
│               └──────────┘   │     │
│                     │         │     │
│                     └─────────┘     │
│               (loop back)           │
└─────────────────────────────────────┘
```

**Key Components**:

1. **Input Buffer** — Line-oriented input with history (future)
2. **Parser** — Convert text to AST (S-expression parser in `ml_like.c`)
3. **Global Environment** — Persistent bindings across evaluations
4. **Evaluator** — Reduce AST to value in environment context
5. **Printer** — Convert value back to human-readable text
6. **Error Handler** — Catch evaluation errors, print message, continue loop

**REPL State**:

```c
struct REPL {
    MLEnv *global_env;      // Persistent top-level bindings
    char   input_buffer[1024]; // Current input line
    int    line_number;     // For error reporting
    bool   running;         // Loop control flag
};
```

## Environment Models

### Association List (Alist)

Linked list of (name, value) pairs. Used in `ml_like.c`.

```
env = [("x", 42), ("y", 100), ("+", <primitive>), nullptr]
```

- **Lookup**: O(n) linear scan from newest to oldest
- **Insert**: O(1) prepend to front, shadows existing bindings
- **Shadowing**: Natural — newer bindings hide older ones
- **Copying**: Fast — just copy the head pointer (shared structure)

### Hash Map

Array of buckets with chained entries. Used in `script_lang.c`.

```
table = [
    bucket[0]: [("name", val)] → [("age", val)]
    bucket[1]: []
    bucket[2]: [("score", val)]
    ...
    bucket[63]: []
]
```

- **Lookup**: O(1) average, O(n) worst case
- **Insert**: O(1) average
- **Shadowing**: Must explicitly handle (delete old first)
- **Copying**: Must copy entire table

### Frame Stack

Array of frames accessed by stack discipline. Used for function call scoping.

```
[Frame: params + locals] ← SP (Stack Pointer)
[Frame: caller's locals]
[Frame: global variables]
```

- **Lookup**: Walk frame chain from top to bottom
- **Allocation**: Push new frame on call, pop on return
- **Locals**: Contiguous allocation within frame

## Parser Patterns

### Recursive Descent

Hand-written parser where each grammar rule is a function.

```
parse_expr()    → parse_term() { '+' parse_term() }
parse_term()    → parse_factor() { '*' parse_factor() }
parse_factor()  → NUMBER | '(' parse_expr() ')' | IDENT
```

- **Pros**: Simple, good error messages, no tool dependency
- **Cons**: Can't handle left recursion directly, manual effort
- **LL(k)**: Left-to-right, Leftmost derivation, k token lookahead

### S-Expression Parser

Special case of recursive descent for parenthesized prefix notation.

```
parse():
    skip whitespace
    if '(':
        read operator keyword
        dispatch on keyword
        parse arguments
        expect ')'
    else if digit:
        return int literal
    else:
        return variable reference
```

### Schema Parser (ProtoBuf)

Keyword-driven parser for IDL-like languages.

```
parse_proto():
    while not EOF:
        read keyword
        if "syntax":    parse syntax declaration
        if "package":   parse package name
        if "message":   parse message body { fields }
        if "enum":      parse enum body { values }
        skip comments
```

## Pattern Match Implementation

### Thompson NFA Construction

Build a nondeterministic finite automaton from a regex AST.

```
Regex AST:          NFA:
  CONCAT              ┌───┐       ┌───┐
  ├── CHAR 'a'        │ 0 │──a──▶│ 1 │
  └── UNION           └───┘       └───┘
      ├── CHAR 'b'              ┌───┐
      └── CHAR 'c'         ┌──ε▶│ 2 │──b──▶┐
                           │    └───┘       │    ┌───┐
                     ┌───┐ │               ├───▶│ 4 │
                     │ 0 │─┤               │    └───┘
                     └───┘ │    ┌───┐       │
                           └──ε▶│ 3 │──c──▶┘
                                └───┘
```

**Rules**:
- `CHAR c`: states n → c → n+1
- `CONCAT e1 e2`: connect e1's accept to e2's start with ε
- `UNION e1 e2`: branch with ε from start to both e1 and e2
- `STAR e`: add ε back edge from accept to start, ε skip from start to accept

### NFA Simulation (On-the-fly subset construction)

Instead of converting NFA to DFA explicitly, simulate the NFA on the input string:

```
simulate(NFA, text):
    current = ε-closure({start_state})
    for each char c in text:
        next = ∅
        for each state s in current:
            if edge(s, c, t) exists:
                next = next ∪ ε-closure({t})
        if next is empty: return false
        current = next
    return accept_state ∈ current
```

## Runtime Data Structures

### Tagged Union (Value Representation)

```c
typedef enum { INT, BOOL, STRING, TABLE, CLOSURE } Type;
typedef struct {
    Type tag;
    union {
        int int_val;
        bool bool_val;
        char *str_val;
        Table *table;
        struct { MLExpr *body; MLEnv *env; } closure;
    };
} Value;
```

- **Benefits**: Type-safe, memory efficient, C89/C99 compatible
- **Drawbacks**: Must manually check tag before access, no inheritance

### Boxing vs Unboxing

- **Boxed**: Values are heap-allocated objects (pointer to union)
- **Unboxed**: Values are stack-allocated or immediate (tagged pointer)
- **Our approach**: Hybrid — primitives are unboxed (stored inline), closures and tables are boxed (stored via pointer)

### NaN-Boxing (Alternative)

Pack small values into unused NaN bits in IEEE 754 doubles:

```
double val;
// If NaN with specific bit pattern: it's an int/bool/pointer
// Otherwise: it's a real double
```

## Comparison of Project Approaches

| Feature | c_subset | ml_like | script_lang | protobuf | regex_lang |
|---|---|---|---|---|---|
| Parser type | Recursive descent | S-expression | Line-based | Keyword-driven | Recursive descent |
| Evaluator | Tree-walk | Tree-walk + env | Table-based | Schema validation | NFA simulation |
| Scoping | Dynamic frames | Lexical frames | Global table | N/A | N/A |
| Type system | Static (C-like) | Dynamic (tagged union) | Dynamic (tagged) | Static (proto types) | N/A |
| State | Mutable vars | Immutable (functional) | Mutable globals | Schema describes state | Stateless automaton |
| Control flow | while, if/else | if, recursion | (future) | N/A | N/A |
| Output | Side effects | Return values | Side effects | Generated code | Match results |

## References

- Nystrom, R. *Crafting Interpreters* — Tree-walk vs bytecode VM comparison
- Wirth, N. *Compiler Construction* — Recursive descent parsing
- Thompson, K. *Regular Expression Search Algorithm* (1968) — NFA-based matching
- Peyton Jones, S. *Implementing Functional Languages* — Graph reduction, STG machine
- McCarthy, J. *Recursive Functions of Symbolic Expressions* (1960) — Original Lisp evaluator
