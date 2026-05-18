# mini-main-languages — 编程语言实现 (C 语言实现)

> 参考 Programming Language Pragmatics, Structure and Interpretation of Computer Programs

A collection of five mini-language implementations written in C99, demonstrating core concepts in programming language design: parsing, evaluation, type systems, scope management, runtime environments, regular expressions, and code generation.

## Modules

| # | Module | Directory | Description |
|---|--------|-----------|-------------|
| 1 | **C Subset Interpreter** | `include/c_subset.h`, `src/c_subset.c` | Tree-walking AST interpreter for a restricted C-like language. Supports int arithmetic, while loops, if/else, function calls, 1D arrays, and structs. Uses environment-based scoping with linked-list frames. |
| 2 | **ML-like Lambda Calculus** | `include/ml_like.h`, `src/ml_like.c` | Call-by-value lambda calculus interpreter with ML-like syntax. Supports lambda abstraction, application, let/letrec binding, conditionals, and primitive arithmetic. Includes an interactive REPL. |
| 3 | **Lua-like Scripting Language** | `include/script_lang.h`, `src/script_lang.c` | Minimal scripting language with dynamic types (nil, bool, int, float, string, table, function). Implements chained hash tables for associative arrays, global variable management, and a call stack. |
| 4 | **Protobuf/IDL Parser** | `include/protobuf_lang.h`, `src/protobuf_lang.c` | Schema parser for Protocol Buffers-like IDL. Parses .proto files (message, enum, field definitions), validates schema constraints, and generates C header files with corresponding structs. |
| 5 | **Regular Expression Engine** | `include/regex_lang.h`, `src/regex_lang.c` | Regex parser and pattern matcher with NFA compilation. Supports union (`|`), star (`*`), plus (`+`), question (`?`), dot (`.`), character ranges (`[a-z]`), and escape sequences (`\d`, `\w`, `\s`). |

## Project Structure

```
mini-main-languages/
├── include/
│   ├── c_subset.h          # C subset interpreter header
│   ├── ml_like.h           # ML-like language header
│   ├── script_lang.h       # Lua-like scripting header
│   ├── protobuf_lang.h     # Protobuf/IDL language header
│   └── regex_lang.h        # Regex engine header
├── src/
│   ├── c_subset.c          # C subset implementation
│   ├── ml_like.c           # ML-like implementation
│   ├── script_lang.c       # Script language implementation
│   ├── protobuf_lang.c     # Protobuf implementation
│   └── regex_lang.c        # Regex implementation
├── examples/
│   ├── c_interp_demo.c     # Fibonacci interpreter demo
│   ├── ml_repl_demo.c      # Lambda calculus REPL demo
│   └── proto_demo.c        # .proto file parsing demo
├── demos/
│   ├── mini-c-subset-interpreter/
│   │   └── README.md        # C interpreter walkthrough (250+ lines)
│   └── mini-lambda-calculus/
│       └── README.md        # Lambda calculus walkthrough (250+ lines)
├── docs/
│   ├── course-alignment.md  # Mapping to SICP, PLP, Dragon Book
│   └── language-implementation-patterns.md  # Interpreter patterns

├── Makefile
└── README.md
```

## Build

```bash
make          # Build all demo executables
make clean    # Remove build artifacts
```

Three demo programs will be created in `bin/`:
- `c_interp_demo` — Fibonacci via while loop in C subset interpreter
- `ml_repl_demo` — Lambda calculus REPL with factorial and Church encoding
- `proto_demo` — Parse Person.proto and generate C header

## Design Philosophy

- **C99 Standard** — Strict adherence to C99, using only libc and libm
- **Snake case functions** — `c_eval_expr`, `ml_parse`, `proto_validate`
- **PascalCase types** — `ASTNode`, `MLExpr`, `ProtoFile`, `RegexPattern`
- **UPPER_SNAKE_CASE constants** — `T_INT`, `ML_LAMBDA`, `PF_STRING`
- **Header guards** — Standard `#ifndef`/`#define`/`#endif` pattern
- **Zero dependencies** — Only `<stdio.h>`, `<stdlib.h>`, `<string.h>`, `<ctype.h>`, `<math.h>`, `<stdbool.h>`

## Key Concepts

### Parsing
- Recursive descent (C subset)
- S-expression parser (ML-like)
- Line-based parser (Lua-like)
- Keyword-driven IDL parser (Protobuf)
- Regex parser with operator precedence

### Evaluation
- Tree-walking interpreter with environment scoping
- Metacircular evaluator (eval/apply cycle)
- Hash-table-based global lookup
- Schema validation rules
- Thompson NFA construction and simulation

### Runtime
- Linked-list environment frames for lexical scope
- Chained hash tables for associative arrays
- Tagged union value representation
- Closure capture of lexical environment
- Code generation from data schemas

## Course Context

This project aligns with standard programming language curricula:

- **SICP** (Abelson & Sussman): Chapters 1-4 — evaluation models, environment, metacircular evaluator
- **PLP** (Scott): Chapters 2-14 — syntax, scoping, types, subroutines, code generation
- **Dragon Book** (Aho et al.): Chapters 1-8 — lexing, parsing, translation, runtime environments

See `docs/course-alignment.md` for a detailed chapter-by-chapter mapping.

## License

Educational use. See individual file headers for details.
