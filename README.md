# Mini Lang Compiler VM

A collection of **from-scratch, zero-dependency C implementations** of programming languages, compilers, and virtual machines. Each module implements a compiler pipeline stage or language runtime — from lexing/parsing through IR optimization to code generation, JIT compilation, and language paradigms. Modules map to Stanford, CMU, MIT courses, bridging compiler theory to runnable C code.

## Modules

| Module | Topics | Key References |
|--------|--------|----------------|
| [mini-compiler-frontend](mini-compiler-frontend/) | Lexer (DFA/regex), recursive-descent parser, AST construction, semantic analysis, symbol table | Stanford CS143, Crafting Interpreters |
| [mini-compiler-middle](mini-compiler-middle/) | IR design (three-address code), SSA form, dataflow analysis, optimizations (DCE, CSE, inlining) | CMU 15-745, Dragon Book |
| [mini-compiler-backend](mini-compiler-backend/) | Instruction selection (tiling), register allocation (graph coloring/linear scan), code emission | CMU 15-745, Appel "Modern Compiler" |
| [mini-jit-vm](mini-jit-vm/) | Stack VM bytecode, JIT compilation (method/tracing), inline caching, garbage collection | Dart VM, LuaJIT, V8 |
| [mini-lang-paradigm](mini-lang-paradigm/) | OOP (vtabling, inheritance), FP (closures, currying, monads), logic (unification), procedural | Stanford CS242, MIT 6.945 |
| [mini-main-languages](mini-main-languages/) | C subset interpreter, simple ML/Haskell interpreter, protobuf-based language, Lua-like scripting | Programming Language Pragmatics |
| [mini-ai-compiler](mini-ai-compiler/) | MLIR dialects, TVM relay, XLA HLO, operator fusion, layout optimization, auto-scheduling | MLIR, Apache TVM, XLA |
| [mini-build-system](mini-build-system/) | Make rule evaluation, Ninja build graph, dependency resolution (topo sort), incremental builds, caching | GNU Make, Ninja, Buck/Bazel concepts |

## Design Philosophy

- **Zero external dependencies** — pure C (C99/C11), only `libc` and `libm`
- **Self-contained modules** — each directory has its own `Makefile`, `include/`, `src/`, `examples/`, `demos/`, `tests/`
- **Full compiler pipeline** — each stage usable standalone or composed into a toolchain
- **Theory-to-code mapping** — every module includes `docs/` with course-alignment notes
- **Practical demos** — simple C compiler, stack VM with JIT, MLIR dialect, build graph solver, and more

## Building

Each module is standalone. Navigate to a module directory and run:

```bash
cd mini-compiler-frontend
make all    # build everything
make test   # run tests
```

Requires **GCC** and **GNU Make**.

## Project Structure

```
mini-lang-compiler-vm/
├── mini-compiler-frontend/     # Lexer, Parser, AST, Semantic Analysis
├── mini-compiler-middle/       # IR, SSA, Optimizations
├── mini-compiler-backend/      # Code Gen, Register Allocation
├── mini-jit-vm/                # JIT Compilation & Bytecode VM
├── mini-lang-paradigm/         # Language Paradigms & Runtimes
├── mini-main-languages/        # Programming Language Implementations
├── mini-ai-compiler/           # AI Compilers (MLIR, TVM, XLA)
└── mini-build-system/          # Build Systems
```

## License

MIT
