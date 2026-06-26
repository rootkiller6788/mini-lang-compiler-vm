# mini-main-languages — Programming Language Implementations (C99)

> Reference: Programming Language Pragmatics, SICP, Dragon Book, TAPL

**Module Status: COMPLETE ✅**

A collection of five mini-language implementations in C99 demonstrating core concepts in programming language design: parsing, evaluation, type systems, scope management, runtime environments, regular expressions, code generation, binary serialization, and garbage collection.

## Knowledge Coverage (L1-L9)

| Level | Name | Status | Highlights |
|-------|------|--------|------------|
| **L1** | Definitions | ✅ Complete | CType, ASTNode, MLExpr, ProtoField, RegexNode, ScriptValue — all tagged unions with full API |
| **L2** | Core Concepts | ✅ Complete | Tree-walking interpreter, metacircular evaluator, hash tables (djb2), mark-sweep GC, wire format encoding |
| **L3** | Engineering Structures | ✅ Complete | Recursive descent parsers (5 variants), NFA/DFA state machines, protobuf schema graph, call stack |
| **L4** | Standards/Theorems | ✅ Complete | Kleene's theorem (regex≡NFA), Brzozowski derivatives, Hindley-Milner types, proto3 wire format spec, Church-Rosser |
| **L5** | Algorithms/Methods | ✅ Complete | Thompson NFA construction, subset construction DFA, Hopcroft minimization, zigzag encoding, Y combinator, de Bruijn indices, constant folding |
| **L6** | Canonical Problems | ✅ Complete | Fibonacci via AST, factorial via letrec, Church encoding, regex search/match_all, proto schema→C header generator |
| **L7** | Applications | ✅ Complete | Bytecode compiler, CPS transformation, partial evaluation, JSON output, string library, native function binding |
| **L8** | Advanced Topics | ✅ Complete | Mark-sweep GC, lazy evaluation (thunks), DFA minimization, coroutines (yield/resume), varint/zigzag encoding |
| **L9** | Industry Frontiers | ✅ Partial | Hindley-Milner type inference (W-algorithm), de Bruijn nameless representation, gRPC foundations (documented) |

## Modules

| # | Module | Lines | Description |
|---|--------|-------|-------------|
| 1 | **C Subset Interpreter** | 1,323 | Tree-walking AST interpreter. Supports int/float/bool, `+ - * / %`, `== != < > <= >=`, `&& \|\| !`, while, for, if/else, break/continue, function calls, arrays, `{}` blocks. Includes constant folding optimizer and bytecode compiler. |
| 2 | **ML-like Lambda Calculus** | 1,309 | Call-by-value lambda calculus with S-expression syntax. Supports lambda, let/letrec, if, cons/car/cdr/nil, pattern matching, Y combinator, delay/force (lazy eval). Includes de Bruijn conversion, Hindley-Milner type inference, CPS transformation, partial evaluation. |
| 3 | **Lua-like Scripting Language** | 1,417 | Dynamic scripting language with tagged union values (nil, bool, int, float, string, table, func, native). Features recursive descent parser, AST evaluator, mark-sweep GC, chained hash tables, string library, coroutines. |
| 4 | **Protobuf/IDL Compiler** | 1,036 | Proto3 schema parser → validation → C header generator → binary serializer/deserializer. Implements wire format (varint, fixed32/64, length-delimited), zigzag encoding, JSON output. Full round-trip: parse schema → serialize message → deserialize → validate. |
| 5 | **Regular Expression Engine** | 1,244 | Regex parser + Thompson NFA compiler + DFA subset construction. Supports `\|`, `*`, `+`, `?`, `.`, `[a-z]`, `\d\w\s`, `^$`. Implements Brzozowski derivatives, nullable predicate, DFA minimization (Hopcroft). APIs: match, search, full_match, match_all. |

## Build & Test

```bash
make          # Build all demo executables
make test     # Build and run all tests (5 test suites)
make clean    # Remove build artifacts
```

Outputs in `bin/`:
- `c_interp_demo` — Fibonacci via C subset interpreter
- `ml_repl_demo` — Lambda calculus REPL with factorial, Church encoding
- `proto_demo` — Parse Person.proto, generate C header
- `test_c_subset`, `test_ml_like`, `test_script_lang`, `test_protobuf`, `test_regex` — Test suites

## Core Definitions (L1)

| Type | File | Purpose |
|------|------|---------|
| `CType`, `ASTNode`, `ASTNodeType` | `c_subset.h` | C-like static type system, AST tagged union (15 node types) |
| `MLExpr`, `MLValue`, `MLEnv` | `ml_like.h` | Lambda calculus AST (17 expression types), value types (5), environment frames |
| `ScriptValue`, `Table`, `ScriptAST` | `script_lang.h` | Dynamic typing (9 types), chained hash table, script AST (11 node types) |
| `ProtoFile`, `ProtoMessage`, `ProtoField` | `protobuf_lang.h` | Proto3 schema graph, wire format types, runtime message values |
| `RegexNode`, `NFA`, `DFA`, `RegexPattern` | `regex_lang.h` | Regex AST (12 node types), Thompson NFA, DFA transition table |

## Core Theorems (L4)

| Theorem | Implementation | Reference |
|---------|---------------|-----------|
| Kleene's Theorem: Regex ≡ NFA | `regex_build_nfa()` — Thompson construction | Kleene, 1956 |
| Rabin-Scott: NFA → DFA | `regex_nfa_to_dfa()` — subset construction | Rabin & Scott, 1959 |
| Hopcroft DFA minimization | `regex_dfa_minimize()` — partition refinement in O(n log n) | Hopcroft, 1971 |
| Brzozowski derivatives | `regex_derivative()` — symbolic regex differentiation | Brzozowski, 1964 |
| Church-Rosser (confluence) | `ml_eval()` — call-by-value reduction | Church & Rosser, 1936 |
| Damas-Milner type inference | `ml_type_infer()` — simplified W-algorithm | Damas & Milner, 1982 |
| Proto3 wire format | `proto_buffer_write_varint()`, zigzag encoding | Google, 2016 |

## Core Algorithms (L5)

| Algorithm | File | Complexity |
|-----------|------|------------|
| Recursive descent parser (Pratt-style) | `c_subset.c` | O(n) |
| Tree-walking evaluator | `c_subset.c` | O(n) per statement |
| Constant folding | `c_subset.c` | O(n) tree traversal |
| Bytecode compiler (stack VM) | `c_subset.c` | O(n) |
| Metacircular evaluator (eval/apply) | `ml_like.c` | Depends on reduction steps |
| Y combinator (fixpoint) | `ml_like.c` | Self-application |
| De Bruijn index conversion | `ml_like.c` | O(n) tree traversal |
| CPS transformation | `ml_like.c` | O(n) |
| djb2 hash function | `script_lang.c` | O(len) |
| Mark-sweep GC | `script_lang.c` | O(live + dead) |
| Varint encoding (ULEB128) | `protobuf_lang.c` | O(log₁₂₈ n) |
| Zigzag encoding | `protobuf_lang.c` | O(1) |
| Thompson NFA construction | `regex_lang.c` | O(\|regex\|) |
| NFA simulation (on-the-fly subset) | `regex_lang.c` | O(\|text\| × \|states\|²) |
| Subset construction (NFA → DFA) | `regex_lang.c` | O(2ⁿ) worst case |
| DFA minimization (Hopcroft) | `regex_lang.c` | O(kn log n) |

## Classic Problems (L6)

- **Fibonacci** — Iterative with while loop, tested for n=0..10
- **Factorial** — Recursive with letrec (functional style)
- **Church encoding** — TRUE/FALSE via lambda calculus, curried application
- **Proto schema → C code** — Parse .proto, validate, generate C header
- **Regex pattern matching** — Compile patterns, match against text

## Nine-School Course Mapping

| School | Course | Files |
|--------|--------|-------|
| **MIT** | 6.004, 6.824, 6.858 | `c_subset.c` (computation structures), `script_lang.c` (distributed systems patterns) |
| **Stanford** | CS 144, CS 245, CS 229 | `protobuf_lang.c` (data serialization), `ml_like.c` (ML lineage) |
| **Berkeley** | CS 162, CS 186, CS 267 | `regex_lang.c` (automata theory), `c_subset.c` (systems) |
| **CMU** | 15-410, 15-445, 15-721 | All modules (systems focus) |
| **UT Austin** | CS 380D, CS 395T | `script_lang.c` (distributed systems) |
| **ETH** | 263-0006, 263-3501 | `c_subset.c` (compiler construction) |
| **Cambridge** | Part II: OS, Concurrent, Compiler | `ml_like.c` (functional programming) |
| **清华** | OS, Compilers, Networking | `protobuf_lang.c` (network serialization) |
| **Georgia Tech** | CS 6210, CS 6290, CS 7641 | All modules (systems + ML) |

## Design

- **C99** — strict compliance, zero external dependencies
- **Snake case** — `c_eval_expr`, `ml_parse`, `proto_validate`, `regex_compile`, `script_new_value`
- **PascalCase types** — `ASTNode`, `MLExpr`, `ProtoFile`, `RegexPattern`, `ScriptValue`
- **Header guards** — `#ifndef`/`#define`/`#endif`

## License

Educational use. See individual file headers for details.
