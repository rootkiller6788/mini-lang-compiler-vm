# mini-lang-paradigm — 语言范式 (C 语言实现)

> 参考 Stanford CS242 Programming Languages, MIT 6.945 Adventures in Advanced Symbolic Programming

A C99 implementation of three major programming language paradigms — Object-Oriented, Functional, and Logic — plus type systems and pattern matching. Each module provides a self-contained runtime for the paradigm, with no external dependencies beyond libc and libm.

## Modules

| # | Module | Header | Source | Description |
|---|--------|--------|--------|-------------|
| 1 | **OOP Vtable** | `include/oop_vtable.h` | `src/oop_vtable.c` | Object-oriented runtime: classes, vtable dispatch, single inheritance, virtual method calls |
| 2 | **FP Closure** | `include/fp_closure.h` | `src/fp_closure.c` | Functional programming: closures, currying, composition, map/fold/filter on cons lists |
| 3 | **Logic Unify** | `include/logic_unify.h` | `src/logic_unify.c` | Logic programming: Martelli-Montanari unification, backtracking solver, Prolog-like clauses |
| 4 | **Type System** | `include/type_system.h` | `src/type_system.c` | Type system: Hindley-Milner W algorithm, type unification, occurs check, lambda calculus AST |
| 5 | **Pattern Match** | `include/pattern_match.h` | `src/pattern_match.c` | Pattern matching: wildcards, variables, constructors, decision tree compilation |

## Quick Start

```bash
make clean all
bin/oop_demo
bin/fp_demo
bin/type_infer_demo
```

## Module Details

### 1. OOP Vtable (`oop_vtable.h`)

Virtual method table dispatch with single inheritance.

```c
Class* animal = class_create("Animal", sizeof(Object));
class_add_method(animal, "speak", 0, animal_speak);

Class* dog = class_inherit("Dog", animal);
class_override_method(dog, "speak", dog_speak);

Object* d = object_create(dog);
object_call_virtual(d, "speak", NULL);  // dispatches to dog_speak
```

**Key concepts:** vtable, method dispatch, inheritance, polymorphism, base pointer calls.

### 2. FP Closure (`fp_closure.h`)

Closures with captured environments, currying, and higher-order list operations.

```c
FPClosure* add = fp_closure_create(sum_fn, 2, 0);
int result = *(int*)fp_apply(add, (void*[]){&a, &b});

FPClosure* composed = fp_compose(double_fn, add10_fn);
FPList* mapped = fp_map(double_fn, list);
int* sum = fp_foldl(add_fn, &zero, list);
```

**Key concepts:** closures, currying, composition, map/foldr/foldl, cons lists, lambda calculus.

### 3. Logic Unify (`logic_unify.h`)

Martelli-Montanari unification algorithm with backtracking solver.

```c
Term* X = term_create_var(0);
Term* goal = term_create_compound("parent", (Term*[]){X, term_create_atom("mary")}, 2);

LogicProgram prog = logic_program_create("family");
logic_program_add_clause(&prog, clause_create(parent_john_mary, NULL, 0));

Substitution result;
if (logic_solve(&prog, goal, &result)) {
    // X is bound to "john"
}
```

**Key concepts:** unification, occurs check, substitution, backtracking, depth-first search.

### 4. Type System (`type_system.h`)

Simplified Hindley-Milner type inference (W algorithm).

```c
Expr* id = expr_create_lambda("x", expr_create_var("x"));  // λx.x
TypeSubst subst = type_subst_create();
Type* t = type_infer_hm(&env, id, &subst);
// Inferred: t0 → t0
```

**Key concepts:** type variables, unification, generalization, instantiation, occurs check.

### 5. Pattern Match (`pattern_match.h`)

Pattern matching with decision tree compilation.

```c
Pattern* p = pattern_cons("Cons", (Pattern*[]){pattern_var("x"), pattern_var("xs")}, 2);
MatchCase cases[] = { match_case_create(p, handler_cons) };
DTNode* tree = match_compile(cases, 1);
void* result = match_execute(tree, &value);
```

**Key concepts:** wildcard, variable binding, constructor patterns, decision trees, exhaustiveness.

## Directory Structure

```
mini-lang-paradigm/
├── include/                     # Public headers (5 files)
│   ├── oop_vtable.h
│   ├── fp_closure.h
│   ├── logic_unify.h
│   ├── type_system.h
│   └── pattern_match.h
├── src/                         # Implementation (5 files)
│   ├── oop_vtable.c
│   ├── fp_closure.c
│   ├── logic_unify.c
│   ├── type_system.c
│   └── pattern_match.c
├── examples/                    # Runnable demos (3 files)
│   ├── oop_demo.c
│   ├── fp_demo.c
│   └── type_infer_demo.c
├── demos/                       # Detailed walkthroughs (2 files)
│   ├── mini-oop-runtime/README.md
│   └── mini-fp-core/README.md
├── docs/                        # Documentation (2 files)
│   ├── course-alignment.md
│   └── paradigm-comparison.md
├── Makefile
└── README.md
```

## Building

**Requirements:** GCC (C99), GNU Make.

```bash
make           # build all 3 demos
make clean     # remove build artifacts
make oop_demo  # build OOP demo only
make fp_demo   # build FP demo only
```

**Compiler flags:** `-Wall -Wextra -O2 -I include -lm`

## Design Philosophy

- **C99 only** — no C11/C17 features, no POSIX extensions beyond standard libc
- **No external dependencies** — only `libc` and `libm`
- **Self-contained modules** — each module compiles independently
- **Explicit memory management** — `_create` and `_destroy` functions for all types
- **Consistent naming** — `snake_case` functions, `PascalCase` types, `UPPER_SNAKE_CASE` constants

## References

- **Stanford CS242**: Programming Languages — Grossman, Aiken
- **MIT 6.945**: Adventures in Advanced Symbolic Programming — Sussman
- **TAPL**: Types and Programming Languages — Pierce
- **SICP**: Structure and Interpretation of Computer Programs — Abelson, Sussman
- **CTM**: Concepts, Techniques, and Models of Computer Programming — Van Roy, Haridi

## License

Educational use.
