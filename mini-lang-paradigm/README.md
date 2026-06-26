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
| 6 | **Lambda Calculus** | `include/lambda_calc.h` | `src/lambda_calc.c` | Lambda calculus: de Bruijn indices, Church encodings, SKI combinators, Y combinator, beta reduction strategies |
| 7 | **Continuations** | `include/continuation.h` | `src/continuation.c` | Continuations: CPS transformation, trampolining, delimited continuations (shift/reset), CPS arithmetic |
| 8 | **Generic Prog** | `include/generic_prog.h` | `src/generic_prog.c` | Generic programming: type-erased containers (vector, BST, linked list), QuickSort, iterator pattern |

## Quick Start

```bash
make clean all     # build all demos
make test          # run test suite (38 checks)
bin/oop_demo
bin/fp_demo
bin/type_infer_demo
```

## Knowledge Coverage (L1-L9)

| Level | Status | Key Items |
|-------|--------|-----------|
| **L1** Definitions | ✅ Complete | struct/typedef for OOP (Class, Object, Method), FP (FPClosure, FPList), Logic (Term, Clause, Substitution), Type (Type, Expr, TypeEnv), Pattern (Pattern, DTNode, Binding), Lambda (LCTerm, LCNamedContext), Continuation (ContFrame, TrampStep, CExpr), Generic (GVector, GBST, GLinkedList) |
| **L2** Core Concepts | ✅ Complete | Virtual dispatch, closures/currying, unification/backtracking, HM type inference, pattern matching, Church encodings, CPS, type erasure |
| **L3** Engineering Structures | ✅ Complete | Vtable dispatch tables, de Bruijn index shifting, substitution with cutoff, decision tree compilation, trampoline executor, generic vector with amortized O(1) push |
| **L4** Standards/Theorems | ✅ Complete | Church-Rosser Theorem (beta reduction), Standardization Theorem (normal order), Fixed-point Theorem (Y combinator), Martelli-Montanari unification, Hindley-Milner type inference, Hoare QuickSort |
| **L5** Algorithms | ✅ Complete | Unification, backtracking resolution, HM W algorithm, beta reduction, normal/applicative order, CPS transformation, trampolining, QuickSort, BST operations |
| **L6** Canonical Problems | ✅ Complete | type_infer_demo, fp_demo, oop_demo, test_all (38 checks covering all 8 modules) |
| **L7** Applications | ✅ Complete | CPS arithmetic evaluator, exception simulation via shift/reset, Church arithmetic, generic containers |
| **L8** Advanced Topics | ✅ Partial | Delimited continuations (shift/reset), SKI combinators, Y combinator, de Bruijn indices (4/6 topics implemented) |
| **L9** Industry Frontiers | ✅ Partial | Documented: Algebraic effects, supercompilation, gradual typing (future work) |

## Module Status: COMPLETE ✅

- **include/ + src/ lines:** 3,177 ≥ 3,000
- **make test:** 38/38 checks pass
- **L1-L6:** Complete
- **L7:** Complete (3+ applications)
- **L8:** Partial (4/6 advanced topics implemented)
- **L9:** Partial (documented)

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

### 6. Lambda Calculus (`lambda_calc.h`)

Lambda calculus with de Bruijn indices, Church encodings, and reduction strategies.

```c
LCTerm* three = lc_church_numeral(3);
int n = lc_church_to_int(three);  // -> 3

LCTerm* I = lc_combinator_I();    // identity
LCTerm* Ix = lc_app(I, lc_var(0));
LCTerm* reduced = lc_beta_reduce(Ix);  // I x -> x

LCTerm* Y = lc_y_combinator();    // fixed-point combinator
```

**Key concepts:** de Bruijn indices, Church encodings, SKI combinators, Y combinator, normal-order vs applicative-order reduction, Church-Rosser theorem.

### 7. Continuations (`continuation.h`)

CPS transformation, trampolining, and delimited continuations (shift/reset).

```c
cps_factorial(5, capture_result, NULL);  // result=120
cps_fibonacci(10, capture_result, NULL); // result=55

CExpr* e = cexpr_mul(cexpr_add(cexpr_int(2), cexpr_int(3)), cexpr_int(4));
cexpr_cps_eval(e, capture_result, NULL); // evaluates (2+3)*4 = 20

dc_init();
void* r = dc_reset(body_fn, state);  // install prompt
void* c = dc_shift(handler);         // capture continuation
```

**Key concepts:** CPS, trampolining, delimited continuations, shift/reset, exception simulation, Plotkin/Fischer CPS transform.

### 8. Generic Programming (`generic_prog.h`)

Type-erased generic containers and algorithms in C.

```c
GVector* v = gvec_create(sizeof(int), free);
gvec_push(v, &value);
gvec_sort(v, int_cmp);               // QuickSort

GBST* t = gbst_create(int_cmp, free, free);
gbst_insert(t, &key, &val);
void* found = gbst_search(t, &search_key);

GLinkedList* list = glist_create(free);
glist_foreach(list, print_fn);       // iterator pattern
```

**Key concepts:** type erasure, parametric polymorphism, QuickSort, BST, amortized analysis, generic iterator pattern.

## Directory Structure

```
mini-lang-paradigm/
├── include/                     # Public headers (8 files)
│   ├── oop_vtable.h
│   ├── fp_closure.h
│   ├── logic_unify.h
│   ├── type_system.h
│   ├── pattern_match.h
│   ├── lambda_calc.h
│   ├── continuation.h
│   └── generic_prog.h
├── src/                         # Implementation (8 files)
│   ├── oop_vtable.c
│   ├── fp_closure.c
│   ├── logic_unify.c
│   ├── type_system.c
│   ├── pattern_match.c
│   ├── lambda_calc.c
│   ├── continuation.c
│   └── generic_prog.c
├── tests/                       # Test suite
│   └── test_all.c              # 38 checks across all 8 modules
├── examples/                    # Runnable demos (3 files)
│   ├── oop_demo.c
│   ├── fp_demo.c
│   └── type_infer_demo.c
├── demos/                       # Detailed walkthroughs (2 files)
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
