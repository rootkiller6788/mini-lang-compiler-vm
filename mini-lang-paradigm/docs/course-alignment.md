# Course Alignment — Programming Language Paradigms

> Mapping this module's concepts to Stanford CS242, MIT 6.945, and Pierce's "Types and Programming Languages".

---

## Stanford CS242: Programming Languages

### Course Overview

CS242 explores the fundamental concepts behind programming language design: syntax, semantics, type systems, and paradigms. The course examines how different language features interact and how they can be implemented.

### Module Mapping

| CS242 Topic                        | Our Module            | Implementation                      |
|------------------------------------|----------------------|-------------------------------------|
| Lambda calculus & operational semantics | `type_system.h` / `fp_closure.h` | Expression AST, substitution, beta-reduction |
| Hindley-Milner type inference       | `type_system.h`      | `type_infer_hm()`, unification      |
| Subtyping & inheritance             | `oop_vtable.h`       | `class_inherit()`, vtable copy       |
| Currying & higher-order functions   | `fp_closure.h`       | `fp_curry()`, `fp_compose()`         |
| Algebraic data types & pattern matching | `pattern_match.h` | Decision tree compilation            |
| Logic programming & unification     | `logic_unify.h`      | Martelli-Montanari algorithm         |
| Garbage collection                  | (future)             | Mark-and-sweep in C                  |
| Continuations & control flow        | (future)             | CPS transform, call/cc               |

### Key CS242 Assignments Replicated

1. **Homework 1: Lambda Calculus Interpreter** → `type_infer_demo.c` — parse, reduce, type-check lambda terms
2. **Homework 2: Type Inference** → Hindley-Milner W algorithm in `src/type_system.c`
3. **Homework 3: Subtyping** → Vtable-based dispatch with inheritance in `src/oop_vtable.c`
4. **Homework 4: Prolog Interpreter** → `src/logic_unify.c` — unification and backtracking solver

---

## MIT 6.945: Adventures in Advanced Symbolic Programming

### Course Overview

6.945 (formerly 6.905) covers advanced programming language concepts with a focus on abstraction, metaprogramming, and symbolic computation. The course is taught in Scheme and emphasizes building linguistic abstractions.

### Module Mapping

| 6.945 Topic                                | Our Module            | Implementation                           |
|--------------------------------------------|----------------------|------------------------------------------|
| Combinators & pattern-directed invocation  | `pattern_match.h`    | `match_compile()`, decision tree         |
| Generic operators & dispatch               | `oop_vtable.h`       | Vtable dispatch, method lookup           |
| Continuation-passing style                 | (future)             | Stack-rip CPS transform                  |
| Ambient calculus & propagation             | (future)             | Propagator networks                      |
| Macros & syntactic abstraction             | (future)             | AST transformations                      |
| Unification & pattern matching             | `logic_unify.h`      | Term unification, occurs check           |
| Memoization & lazy evaluation              | (future)             | Thunk-based lazy lists                   |

### Key 6.945 Concepts in Our Code

1. **Pattern-directed invocation**: Instead of explicit conditionals, the `match_execute()` function dispatches based on pattern structure — a core 6.945 technique.

2. **Dispatch on type**: The vtable mechanism is a form of generic operation — one function name (`speak`) dispatches to different implementations based on the receiver's class.

3. **Symbolic unification**: Our `unify()` implements the same algorithm used in the 6.945 term-rewriting system. The occurs check prevents infinite terms.

4. **Combinatory logic**: `fp_compose()` and `fp_identity()` form the basis of combinatory algebra. The B combinator (composition) is:
   ```
   B f g x = f (g x)
   ```
   Our implementation:
   ```c
   FPClosure* composed = fp_compose(f, g);
   // composed(x) = f(g(x))
   ```

### Sussman's "Art of the Propagator"

The propagator model from 6.945 maps to our architecture:
- **Cells** → `Term` with variable bindings
- **Propagators** → `unify()` as a constraint propagator
- **Merge** → `subst_compose()` combining partial information

---

## Benjamin Pierce: "Types and Programming Languages" (TAPL)

### Book Overview

TAPL is the canonical text on type systems. It builds from the untyped lambda calculus through simple types, subtypes, recursive types, polymorphism, and beyond.

### Chapter Mapping

| TAPL Chapter                        | Our Implementation                   |
|-------------------------------------|--------------------------------------|
| Ch 3: Untyped Arithmetic Expressions | `Expr` AST, `expr_create_*` functions |
| Ch 5: Untyped Lambda Calculus        | `EXPR_LAMBDA`, `EXPR_APPLY`          |
| Ch 7-8: Nameless Representation      | (future: de Bruijn indices)          |
| Ch 9: Simply Typed Lambda Calculus   | `T_FUNC`, `T_INT`, `T_BOOL`          |
| Ch 11: Simple Extensions             | `T_STRING`, `T_FLOAT`, `T_VOID`      |
| Ch 15: Subtyping                     | `oop_vtable.h` inheritance hierarchy |
| Ch 22: Type Reconstruction           | Hindley-Milner W algorithm           |
| Ch 23: Universal Types               | (future: System F)                   |
| Ch 24: Existential Types             | (future: abstract data types)        |
| Ch 29: Type Operators                | (future: higher-kinded types)        |

### TAPL Definitions Implemented

#### Typing relation (Γ ⊢ t : T):

```
Γ ⊢ x : T                       (T-Var, if x:T ∈ Γ)
Γ ⊢ true : Bool                 (T-True)
Γ ⊢ false : Bool                (T-False)
Γ ⊢ n : Int                     (T-Int, if n is integer literal)
Γ, x:T₁ ⊢ t₂ : T₂              (T-Abs)
─────────────────
Γ ⊢ λx:T₁. t₂ : T₁→T₂

Γ ⊢ t₁ : T₁₁→T₁₂   Γ ⊢ t₂ : T₁₁  (T-App)
─────────────────────────────────
Γ ⊢ t₁ t₂ : T₁₂
```

Our implementation:

```c
Type* type_infer_hm(TypeEnv* env, Expr* expr, TypeSubst* subst) {
    switch (expr->tag) {
    case EXPR_VAR:    return type_env_lookup(env, expr->var_name);  // T-Var
    case EXPR_INT:    return type_create_primitive(T_INT);           // T-Int
    case EXPR_LAMBDA: /* ... create T₁→T₂ ... */                     // T-Abs
    case EXPR_APPLY:  /* ... unify and return T₁₂ ... */             // T-App
    }
}
```

#### Subtyping (S <: T):

```
S <: S                          (S-Refl)
S <: U   U <: T                 (S-Trans)
───────────────────
S <: T

class C extends D               (S-Class)
─────────────────
C <: D
```

Our implementation uses the parent_class pointer:

```c
bool is_subtype(Class* sub, Class* super) {
    while (sub) {
        if (sub == super) return true;
        sub = sub->parent_class;
    }
    return false;
}
```

#### Unification algorithm (Martelli-Montanari):

```
unify(t₁, t₂):
  if t₁ == t₂: return {}
  if t₁ is variable: return {t₁ ↦ t₂} (with occurs check)
  if t₂ is variable: return {t₂ ↦ t₁} (with occurs check)
  if t₁ = f(a₁..aₙ), t₂ = g(b₁..bₘ):
    if f ≠ g or n ≠ m: FAIL
    return unify([a₁..aₙ], [b₁..bₘ])
```

Our implementation in `src/logic_unify.c:unify()` and `src/type_system.c:type_unify()`.

---

## Additional References

### Structure and Interpretation of Computer Programs (SICP)

- **Metacircular evaluator** → Our expression evaluator follows the eval/apply loop pattern
- **Data-directed programming** → Vtable dispatch is a data-directed technique
- **Streams as delayed lists** → `Thunk` and lazy evaluation in demos/

### Concepts, Techniques, and Models of Computer Programming (CTM)

- **Declarative computation model** → Logic programming with unification
- **Stateful computation model** → Object-oriented with mutable fields
- **Concurrent computation model** → (future: actors, message-passing)

### The Implementation of Functional Programming Languages (Peyton Jones)

- **G-machine** → Stack-based graph reduction (future VM)
- **STG machine** → Spineless Tagless G-machine for lazy evaluation

---

## Summary Table

| Concept              | CS242 | MIT 6.945 | TAPL  | Our File             |
|---------------------|-------|----------|-------|----------------------|
| Lambda calculus      | ✓     | ✓        | ✓     | type_system.h         |
| Type inference (HM)  | ✓     |          | ✓     | type_system.h         |
| Subtyping            | ✓     |          | ✓     | oop_vtable.h          |
| Pattern matching     | ✓     | ✓        |       | pattern_match.h        |
| Unification          | ✓     | ✓        |       | logic_unify.h         |
| Higher-order functions| ✓    | ✓        | ✓     | fp_closure.h          |
| Closures             | ✓     | ✓        |       | fp_closure.h          |
| Monads               |       | ✓        |       | demos/mini-fp-core    |
| Continuations        | ✓     | ✓        |       | (future)              |
| Garbage collection   | ✓     |          |       | (future)              |
