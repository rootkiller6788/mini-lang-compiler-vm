# mini-jit-vm — JIT Compilation & Bytecode VM (C Implementation)

> Reference: Dart VM, LuaJIT, V8 Ignition+TurboFan, Crafting Interpreters Ch 14-30

## Module Status: COMPLETE ✅

- L1-L6: Complete
- L7: Complete (3 applications: REPL, Expression Evaluator, AST Printer)
- L8: Complete (Linear Scan Register Allocation, Peephole Optimization)
- L9: Partial (documented, JIT backend reference)

## Knowledge Coverage (L1-L9)

| Level | Name | Status | Key Artifacts |
|-------|------|--------|---------------|
| **L1** | Definitions | ✅ Complete | OpCode, ByteCode, StackVM, TokenType, ASTNode, Compiler, LiveInterval, PhysReg, GCObject, Value, Closure, ICsCache, PolymorphicICS, JITCompiler |
| **L2** | Core Concepts | ✅ Complete | Stack VM fetch-decode-execute, Mark-Sweep GC, Generational GC, Lexical Analysis (DFA), Recursive Descent Parsing, Pratt Operator Precedence, Inline Caching, Method JIT, Liveness Analysis |
| **L3** | Engineering Structures | ✅ Complete | Compiler pipeline (lex→parse→codegen), Optimization pipeline (fold→peephole→DCE), Register allocation pipeline, GC write barrier, Tiered compilation |
| **L4** | Standards/Theorems | ✅ Complete | Chomsky Hierarchy (regex→tokenizer, CFG→parser); Rice's Theorem (optimizer limits); NP-Completeness of Graph Coloring (register allocation); Pratt (1973) Top-Down Operator Precedence; Poletto & Sarkar (1999) Linear Scan |
| **L5** | Algorithms/Methods | ✅ Complete | Pratt expression parser O(n); DFA tokenizer O(n); Constant folding O(n); Dead code elimination (Kildall dataflow); Peephole optimization (McKeeman 1965); Linear scan register allocation O(n log n); Mark-sweep GC O(n) |
| **L6** | Canonical Problems | ✅ Complete | Full expression & statement compiler; Stack VM interpreter; bytecode optimizer; 25-test comprehensive suite |
| **L7** | Applications | ✅ Complete | Interactive REPL; `compiler_eval_expression()` expression evaluator; AST pretty printer |
| **L8** | Advanced Topics | ✅ Complete | Linear scan register allocation (Poletto & Sarkar); Peephole optimization pipeline; Multi-pass optimizer with fixed-point convergence |
| **L9** | Industry Frontiers | ⚠️ Partial | JIT backend architecture documented (see docs/jit-vm-architecture.md); x86-64 native code emission reference |

## Module Files

| # | Module | Files | Lines |
|---|--------|-------|-------|
| 1 | Stack Bytecode VM | `include/bytecode.h`, `src/bytecode.c` | 329 |
| 2 | Method JIT | `include/jit_method.h`, `src/jit_method.c` | 251 |
| 3 | Inline Cache | `include/inline_cache.h`, `src/inline_cache.c` | 165 |
| 4 | Garbage Collection | `include/gc.h`, `src/gc.c` | 258 |
| 5 | Value System & Closures | `include/closure_values.h`, `src/closure_values.c` | 269 |
| 6 | Compiler Frontend | `include/compiler.h`, `src/compiler.c` | 1198 |
| 7 | Bytecode Optimizer | `include/optimizer.h`, `src/optimizer.c` | 414 |
| 8 | Register Allocator | `include/reg_alloc.h`, `src/reg_alloc.c` | 328 |
| **Total** | | **include/ + src/** | **3212** |

## Core Definitions (L1)

- **OpCode**: 18 stack-machine instructions (PUSH, POP, ADD, SUB, MUL, DIV, NEG, NOT, AND, OR, LOAD, STORE, JMP, JMP_IF_FALSE, CALL, RET, PRINT, HALT)
- **ByteCode**: Instruction buffer + constant pool
- **StackVM**: Operand stack + local variable slots + IP/SP/frame pointer
- **Token**: 33 token types for lexer output
- **ASTNode**: 17 node types forming the compiler IR
- **LiveInterval**: Virtual register live range [start_ip, end_ip]
- **GCObject**: Tagged union heap object with mark bit
- **Value**: Tagged union runtime value (int, float, bool, string, closure, native, nil)

## Core Theorems (L4)

1. **Chomsky Hierarchy**: Tokenizer = DFA (regular language); Parser = LL(1)/Pratt (context-free)
2. **Rice's Theorem (1953)**: No perfect optimizer — all optimizations are conservative approximations
3. **Graph Coloring NP-Completeness (Chaitin 1982)**: Optimal register allocation is NP-complete; linear scan is a polynomial-time approximation
4. **Poletto & Sarkar (1999)**: Linear scan achieves O(n log n) with ≤2x optimal spill
5. **Pratt (1973)**: Top-down operator precedence parsing in O(n) with correct associativity
6. **McKeeman (1965)**: Peephole optimization is sound if each replacement preserves observable semantics

## Core Algorithms (L5)

| Algorithm | Complexity | Source |
|-----------|-----------|--------|
| DFA Tokenizer | O(n) | `compiler_lex()` |
| Pratt Expression Parser | O(n) | `parse_precedence()` |
| Constant Folding | O(n) | `opt_constant_folding()` |
| Dead Code Elimination | O(n·d) | `opt_dead_code_elimination()` |
| Peephole Optimization | O(n) per pass | `opt_peephole()` |
| Linear Scan Register Alloc | O(n log n) | `ra_linear_scan_allocate()` |
| Mark-Sweep GC | O(live + dead) | `gc_mark()` + `gc_sweep()` |
| Inline Cache Dispatch | O(1) mono, O(k) PIC | `ic_lookup()` / `pic_lookup()` |

## Nine-School Course Mapping

| School | Course | Mapped To |
|--------|--------|-----------|
| **MIT** | 6.004 Computation Structures | Bytecode VM (ISA design) |
| **MIT** | 6.035 Computer Language Engineering | Compiler frontend (lex+parse+codegen) |
| **Stanford** | CS143 Compilers | Pratt parser, optimizer pipeline |
| **Berkeley** | CS164 Programming Languages | AST design, closure values |
| **CMU** | 15-411 Compiler Design | Register allocation, peephole opt |
| **CMU** | 15-418 Parallel Comp. | JIT method compilation |
| **UT Austin** | CS380D Distributed Systems | (future: distributed JIT) |
| **ETH** | 263-2810 Compiler Design | Linear scan register allocation |
| **Cambridge** | Part II: Compiler Construction | Full compiler pipeline |
| **清华** | 编译原理 (Compiler Principles) | Lex→Parse→Codegen pipeline |
| **Georgia Tech** | CS6241 Compiler Design | Bytecode optimization passes |

## Build & Test

```sh
make all          # Build all example demos
make bytecode_vm  # Build bytecode VM demo
make jit_demo     # Build JIT compilation demo
make gc_demo      # Build GC demo
make test         # Run comprehensive test suite (25 tests)
make clean        # Clean build artifacts
```

## Running Examples

```sh
./bin/bytecode_vm_demo.exe     # Expression (+ 3 (* 4 5)) → 23
./bin/jit_demo.exe             # Fibonacci loop, interp vs JIT comparison
./bin/gc_demo.exe              # GC alloc, minor/major collection, stats
```

## Design Philosophy

All modules implemented in C99 with libc, no external dependencies. Code follows "one concept, one file" with separate include/ and src/. Each function implements an independent knowledge point — no stubs, no fillers, no TODOs.

### Architecture

```
Source Code
    │
    ▼
┌──────────┐    ┌───────────┐    ┌──────────┐
│  Lexer   │───▶│  Parser   │───▶│ CodeGen  │
│  (DFA)   │    │  (Pratt)  │    │ (AST→BC) │
└──────────┘    └───────────┘    └────┬─────┘
                                      │
                    ┌─────────────────┘
                    ▼
             ┌────────────┐     ┌──────────────┐
             │ Optimizer  │────▶│ Register     │
             │ (fold/dce) │     │ Allocator    │
             └────────────┘     └──────┬───────┘
                                       │
                                       ▼
                              ┌────────────────┐
                              │  Stack VM      │
                              │  (interpreter) │
                              └───────┬────────┘
                                      │
                              ┌───────▼────────┐
                              │  JIT Compiler  │
                              │ (tiered exec)  │
                              └────────────────┘
```

## License

MIT
