# Course Alignment

> Mapping of this implementation to standard programming language curriculum texts.

## SICP — Structure and Interpretation of Computer Programs

*Abelson, Sussman, and Sussman. MIT Press, 2nd Edition.*

| SICP Section | Topic | Our Implementation |
|---|---|---|
| 1.1 | Elements of Programming | `c_subset.c` — Expressions, naming, environment |
| 1.2 | Procedures and Processes | `ml_like.c` — Recursive functions, `letrec` |
| 1.3 | Higher-Order Procedures | `ml_like.c` — Lambda closures, first-class functions |
| 2.1 | Data Abstraction | `script_lang.c` — Table/associative array abstraction |
| 2.3 | Symbolic Data | `ml_like.c` — Symbolic expressions (S-expressions) |
| 3.1 | Assignment and Local State | `c_subset.c` — Variable assignment, mutable state |
| 3.2 | Environment Model of Evaluation | `ml_like.c` — Environment frames, lexical scope |
| 4.1 | The Metacircular Evaluator | `ml_like.c` — `ml_eval` mirrors metacircular eval |
| 4.2 | Lazy Evaluation | (future) — Could add call-by-need thunks |
| 4.3 | Nondeterministic Computing | (future) — Could add `amb` operator |
| 5.1 | Register Machine Design | `regex_lang.c` — NFA state machines |
| 5.2 | A Register-Machine Simulator | (future) — Stack-based VM |

### Key SICP Concepts Implemented

1. **Environment Model (3.2)**: Our `MLEnv` linked list directly implements the frames-and-pointers model from SICP Figure 3.1. Each frame maps variable names to values, with a pointer to the enclosing environment.

2. **Metacircular Evaluation (4.1)**: `ml_eval` follows the `eval`/`apply` cycle described in SICP:
   - `eval` dispatches on expression type (self-evaluating, variable, lambda, application, conditional)
   - Application evaluates to a procedure, then `apply`s it to arguments by extending the environment

3. **Recursive Definitions (4.1.6)**: `letrec` uses the "let over lambda with assignment" technique: create the binding first (with a dummy value), evaluate the body (which may reference the name), then back-patch.

## Programming Language Pragmatics (Scott)

*Michael L. Scott. Morgan Kaufmann, 4th Edition.*

| PLP Chapter | Topic | Our Implementation |
|---|---|---|
| 1 | Introduction | `README.md` — Language taxonomy |
| 2 | Programming Language Syntax | `c_subset.c`, `ml_like.c` — Recursive descent parsers |
| 3 | Names, Scopes, Bindings | `ml_like.c` — Static/lexical scope via environment chains |
| 4 | Semantic Analysis | `protobuf_lang.c` — Schema validation |
| 5 | Assembly-Level Computer Architecture | (future) — Stack-based VM |
| 6 | Control Flow | `c_subset.c` — While loops, if/else |
| 7 | Type Systems | `c_subset.h` — `CType` enum, type annotations |
| 8 | Composite Types | `c_subset.h` — Structs, arrays; `script_lang.h` — Tables |
| 9 | Subroutines and Control Abstraction | `ml_like.c` — Lambda, closure, function calls |
| 10 | Data Abstraction and OOP | `protobuf_lang.c` — Message types, schema |
| 11 | Alternative Programming Models | `ml_like.c` — Functional programming model |
| 12 | Concurrency | (future) |
| 13 | Code Improvement | (future) |
| 14 | Building a Runnable Program | `Makefile`, linking, headers |

### Key PLP Concepts Implemented

1. **Recursive Descent Parsing (Ch. 2)**: The parsers in `c_subset.c` and `ml_like.c` use recursive descent — a hand-written LL parser where each grammar nonterminal corresponds to a function.

2. **Static Scoping (Ch. 3)**: Our environment implementation enforces lexical/static scope: variables are resolved by examining the textual nesting of definitions, not the dynamic call chain.

3. **Type Checking (Ch. 7)**: `protobuf_validate` performs schema validation analogous to type checking: verifying field numbers are in valid ranges, no duplicate names, and label/type consistency.

4. **Control Flow (Ch. 6)**: The C subset interpreter handles structured control flow (while loops, conditionals) via recursive evaluation of AST nodes.

5. **Subroutines (Ch. 9)**: Lambda abstraction, closure creation, and application in `ml_like.c` demonstrate the implementation of first-class subroutines with captured environments.

## Compilers: Principles, Techniques, and Tools (Dragon Book)

| Dragon Book Chapter | Topic | Our Implementation |
|---|---|---|
| 1 | Introduction to Compiling | Overview in README |
| 2 | A Simple Syntax-Directed Translator | `c_subset.c` — Simple translation to evaluation |
| 3 | Lexical Analysis | `regex_lang.c` — Regular expressions, NFA for token patterns |
| 4 | Syntax Analysis | `c_subset.c`, `ml_like.c` — Recursive descent, S-exp parsing |
| 5 | Syntax-Directed Translation | `protobuf_lang.c` — Schema→C header code generation |
| 6 | Intermediate-Code Generation | (future) — Could emit three-address code |
| 7 | Run-Time Environments | `ml_like.c` — Environment frames, closures |
| 8 | Code Generation | (future) — Target code emission |
| 9 | Machine-Independent Optimizations | (future) |
| 10 | Instruction-Level Parallelism | (future) |
| 11 | Optimizing for Parallelism and Locality | (future) |
| 12 | Interprocedural Analysis | (future) |

## Types and Programming Languages (Pierce)

| TAPL Chapter | Topic | Our Implementation |
|---|---|---|
| 1-2 | Mathematical Preliminaries | (implicit in implementations) |
| 3 | Untyped Arithmetic Expressions | `c_subset.c` — Simple arithmetic evaluation |
| 5 | The Untyped Lambda-Calculus | `ml_like.c` — Lambda calculus interpreter |
| 8 | Typed Arithmetic Expressions | `c_subset.c` — `CType` annotations (future type checking) |
| 9 | Simply Typed Lambda-Calculus | (future) — Could add type annotations to ml_like |

## Cross-Reference Matrix

| Concept | SICP § | PLP § | Dragon § | File |
|---|---|---|---|---|
| Environment model | 3.2 | 3.3 | 7.2 | `ml_like.c:118` |
| Recursive descent parser | — | 2.3 | 4.4 | `c_subset.c:42` |
| S-expression parser | 4.1 | — | — | `ml_like.c:35` |
| Closure/lexical scope | 3.2 | 3.5 | — | `ml_like.c:158` |
| While-loop semantics | — | 6.4 | — | `c_subset.c:207` |
| Regular expressions | — | 2.1 | 3.7 | `regex_lang.c:1` |
| Schema validation | — | 4.2 | — | `protobuf_lang.c:140` |
| Code generation | — | 14.2 | 8.2 | `protobuf_lang.c:156` |
| Hash tables | — | — | — | `script_lang.c:29` |
| NFA simulation | — | — | 3.7 | `regex_lang.c:119` |
| First-class functions | 1.3 | 3.6 | — | `ml_like.h:47` |
| Tree-walking interpreter | 4.1 | 4.4 | — | `c_subset.c:174` |

## Suggested Reading Order

1. Start with `c_subset.c` — simplest model: tree-walk interpreter with while loops
2. Then `ml_like.c` — introduces functional abstraction: lambdas, closures, environment model
3. Then `regex_lang.c` — finite automata for lexical analysis
4. Then `protobuf_lang.c` — schema grammar, validation, code generation
5. Finally `script_lang.c` — dynamic typing, hash tables, runtime VM

This order follows the progression: imperative → functional → automata theory → code generation → dynamic languages.
