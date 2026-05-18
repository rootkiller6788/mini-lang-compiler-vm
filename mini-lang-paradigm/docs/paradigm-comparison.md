# Paradigm Comparison — OOP vs FP vs Logic Programming

> A comparative analysis of the three major programming paradigms, with reference to our C99 implementations.

---

## Overview

| Dimension             | OOP                             | FP                                  | Logic                          |
|-----------------------|---------------------------------|--------------------------------------|---------------------------------|
| **Unit of abstraction** | Class / Object                | Function                            | Relation / Predicate           |
| **State model**         | Mutable, encapsulated         | Immutable, persistent                | Declarative, substitution      |
| **Control flow**        | Method dispatch, message passing | Function application, composition   | Unification, backtracking      |
| **Polymorphism**        | Subtype (inheritance)         | Parametric (generics)                | N/A (relations)                |
| **Composition**         | Object composition, inheritance | Function composition, combinators    | Clause composition, conjunction |
| **Error handling**      | Exceptions, null objects       | Option/Result types, monads          | Failure as backtracking         |

---

## 1. Encapsulation & Modularity

### OOP: Data + Behavior

Objects encapsulate state (fields) and behavior (methods). Information hiding through access control (public/private). The class serves as a module boundary.

```c
// src/oop_vtable.c
Object* obj = object_create(dog);
object_set_field(obj, 0, &name);     // encapsulate data
object_call_virtual(obj, "speak", NULL); // encapsulate behavior
```

**Strengths:**
- Natural modeling of real-world entities
- Interface/implementation separation
- Protected invariants (methods gate field access)

**Weaknesses:**
- Hidden mutable state creates coupling
- Inheritance breaks encapsulation (fragile base class problem)
- Concurrency requires synchronization

### FP: Pure Functions + Data

Functions are independent of state. Data is separate from behavior. Modules are collections of related functions.

```c
// src/fp_closure.c
FPList* mapped = fp_map(double_fn, list);
int* sum = fp_foldl(add_fn, &zero, list);
```

**Strengths:**
- Functions have no hidden dependencies → easy to test
- Data is immutable → no synchronization needed
- Modules are naturally compositional

**Weaknesses:**
- No natural place to bundle related operations
- Verbose without type classes or modules
- I/O and state require explicit threading (monads)

### Logic: Relations + Facts

Knowledge is represented as facts and rules. No notion of encapsulation — all relations are global.

```c
// src/logic_unify.c
LogicProgram prog = logic_program_create("family");
logic_program_add_clause(&prog, clause_create(parent(john, mary), ...));
bool found = logic_solve(&prog, goal, &result);
```

**Strengths:**
- Declarative: state *what*, not *how*
- Bidirectional: same relation works for query and computation
- No side effects within the solver

**Weaknesses:**
- No information hiding
- Performance depends on clause ordering
- Debugging is challenging

---

## 2. State Management

### OOP: Mutable State

State is local to objects, modified through methods. Identity matters — two objects with equal fields are not "the same."

```c
obj->fields[0] = new_name;    // mutation
// obj still has the same identity
```

**Implications:**
- Aliasing: multiple references to same object → shared mutable state
- Temporal coupling: order of method calls matters
- Must reason about *when* as well as *what*

### FP: Immutable State

State transformations produce new values. Identity is structural (value equality).

```c
FPList* new_list = fp_cons(x, old_list);
// old_list is unchanged; new_list shares tail
```

**Implications:**
- No aliasing problems: can freely share data
- Referential transparency: expression can be replaced by its value
- Persistent data structures enable undo/time-travel

### Logic: Constraint Store

State is a substitution (mapping variables to terms). Backtracking resets the substitution.

```c
Substitution s = subst_create();
unify(X, f(Y), &s);     // s = {X -> f(Y)}
// On backtrack: s is restored to previous state
```

**Implications:**
- State is a set of equalities, not locations
- Automatic undo on backtracking
- No destructive assignment — only constraint accumulation

---

## 3. Concurrency

### OOP: Shared Memory + Locks

Objects are shared between threads. Synchronization is explicit.

```c
pthread_mutex_lock(&obj->mutex);
obj->fields[0] = new_value;
pthread_mutex_unlock(&obj->mutex);
```

**Issues:**
- Deadlocks, race conditions
- Lock granularity tradeoffs
- Testing is combinatorially hard

**Approaches:**
- Actor model (Erlang): objects communicate via messages, no shared state
- Software transactional memory: optimistic concurrency

### FP: No Shared State

Immutable data is inherently thread-safe. Parallelism is "free" for pure computations.

```c
// These can run on separate threads safely
FPList* a = fp_map(f, first_half);
FPList* b = fp_map(f, second_half);
```

**Issues:**
- I/O still requires synchronization
- Parallelism may not produce speedup (overhead of GC, allocation)
- Laziness adds non-determinism in evaluation timing (but not result)

### Logic: And-parallelism + Or-parallelism

- **And-parallelism**: solve independent subgoals concurrently
- **Or-parallelism**: try multiple clauses simultaneously

```prolog
?- parent(X, Y), sibling(Y, Z).  % and: can solve concurrently if sharing = no
?- parent(X, john).              % or: try all parent clauses in parallel
```

**Issues:**
- Variable binding conflicts in and-parallelism
- Load balancing for or-parallelism
- Shared substitution must be managed carefully

---

## 4. Type Systems

### OOP: Nominal Typing

Types are identified by **name** (class identity). Subtyping is explicitly declared.

```
Dog <: Animal   (declared via class_inherit)
```

**Features:**
- Type safety through inheritance hierarchy
- Runtime type information (instanceof checks)
- Method dispatch via vtable

**Limitations:**
- No structural equivalence: two classes with identical methods are different types
- No higher-kinded types
- Parametric polymorphism requires generics (Java) or templates (C++)

### FP: Structural + Parametric Typing

Types are identified by **structure**. Polymorphism through type variables.

```
id : ∀a. a → a
map : ∀a b. (a → b) → [a] → [b]
```

Our HM implementation:

```c
Type* id_type = type_infer_hm(env, id_lambda, &subst);
// Inferred: t0 → t0
```

**Features:**
- Type inference (don't need to annotate)
- Parametricity: free theorems (e.g., `f : ∀a. [a] → [a]` can't inspect elements)
- Higher-kinded types (Haskell, Scala)

### Logic: Untyped / Dynamically Typed

Traditional Prolog is untyped. Terms can be any structure. "Type errors" manifest as unification failure.

```
?- X = f(3), X = g(Y).    % fails: f ≠ g
```

**Extensions:**
- Mercury: statically typed logic programming
- LambdaProlog: higher-order typed logic programming
- Type constraints as part of unification

---

## 5. Composition Mechanisms

### OOP: Inheritance + Composition

```c
// Inheritance: "is-a"
Class* dog = class_inherit("Dog", animal);

// Composition: "has-a"
object_set_field(car, ENGINE_FIELD, engine);
```

**Design principle:** "Favor composition over inheritance"
- Inheritance creates tight coupling
- Composition allows runtime reconfiguration
- Inheritance hierarchies tend to become deep and fragile

### FP: Function Composition + Combinators

```c
// Composition
FPClosure* h = fp_compose(f, g);   // h(x) = f(g(x))

// Combinators: building abstractions without lambda
FPList* result = fp_foldl(add, init, fp_map(double, fp_filter(positive, list)));
```

**Design principle:** "Build small, compose large"
- Every function is a reusable building block
- Pipelines transform data through stages
- Functors, applicatives, monads encode composition patterns

### Logic: Conjunction + Disjunction

```prolog
% Conjunction: solve all subgoals
grandparent(X, Z) :- parent(X, Y), parent(Y, Z).

% Disjunction: try multiple clauses
ancestor(X, Y) :- parent(X, Y).
ancestor(X, Y) :- parent(X, Z), ancestor(Z, Y).
```

**Design principle:** "Declare what must hold, not how to find it"
- Programs as specifications
- Separate logic (what) from control (how)
- Composition via shared logical variables

---

## 6. Error Handling

### OOP: Exceptions (try/catch)

```c
// Hypothetical exception handling
try {
    object_call_virtual(obj, "risky_operation", args);
} catch (Exception* e) {
    // handle error
}
```

**Issues:**
- Breaks normal control flow
- Exception safety (resource leaks)
- Checked vs unchecked exceptions debate

### FP: Result Types / Monads

```c
// Maybe monad: chain computations that might fail
Maybe result = maybe_bind(safe_div(10, 2), safe_div(5));
// If any step returns Nothing, whole chain returns Nothing
```

**Approaches:**
- `Maybe` / `Option` for absent values
- `Either` / `Result` for errors with information
- Effects systems for tracking possible failures in types

### Logic: Failure as Computation

In logic programming, "error" is just failure to prove a goal — it triggers backtracking.

```prolog
% If parent fails, try alternative
query(X) :- (parent(X, Y) ; sibling(X, Y)).
```

**Properties:**
- Failure is a first-class control mechanism
- No explicit error handling code
- Debugging: *why* did the proof fail?

---

## 7. When to Use Each Paradigm

### OOP is best for:

- **GUI frameworks**: widgets as objects with state
- **Game engines**: game objects with behaviors
- **Large systems with clear interfaces**: plugin architectures
- **Simulation**: entities with autonomous behavior

### FP is best for:

- **Data pipelines**: ETL, stream processing
- **Compilers/parsers**: transformation passes
- **Mathematical/scientific computing**: pure transformations
- **Concurrent/distributed systems**: immutable data sharing
- **Financial systems**: correctness and auditability

### Logic is best for:

- **Expert systems / rule engines**: business rules
- **Natural language processing**: parsing, grammar rules
- **Constraint solving**: scheduling, configuration
- **Type inference**: the HM algorithm itself is logic programming
- **Query languages**: SQL is essentially logic programming

---

## 8. Paradigm Convergence

Modern languages increasingly blend paradigms:

| Language     | OOP     | FP       | Logic   |
|-------------|---------|----------|---------|
| Scala       | ✓✓✓     | ✓✓✓      |         |
| F# / OCaml  | ✓       | ✓✓✓      |         |
| Rust        | (traits)| ✓✓       |         |
| Swift       | ✓✓✓     | ✓✓       |         |
| Kotlin      | ✓✓✓     | ✓✓       |         |
| Clojure     | (Java)  | ✓✓✓      | ✓ (core.logic) |
| Racket      | ✓       | ✓✓✓      | ✓ (Datalog) |
| Prolog      | (modules)| ✓ (call) | ✓✓✓      |

### Our C99 implementation demonstrates:

1. All three paradigms can be implemented in a systems language
2. The runtime support for each paradigm is relatively small
3. Vtables, closures, and unification are the three fundamental mechanisms
4. A paradigm-agnostic VM could support all three

---

## References

- Steele, "Growing a Language" — on language design and extensibility
- Harper, "Practical Foundations for Programming Languages"
- Van Roy & Haridi, "Concepts, Techniques, and Models of Computer Programming"
- Gamma et al., "Design Patterns: Elements of Reusable Object-Oriented Software"
- Okasaki, "Purely Functional Data Structures"
- Kowalski, "Algorithm = Logic + Control"
