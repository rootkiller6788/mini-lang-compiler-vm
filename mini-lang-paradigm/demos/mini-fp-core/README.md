# mini-fp-core — Functional Programming Core

> Lambda calculus, closures, currying, monads, algebraic data types, and lazy evaluation — implemented in C99.

## Table of Contents

1. [Lambda Calculus Foundations](#lambda-calculus-foundations)
2. [Closures & Environments](#closures--environments)
3. [Currying & Partial Application](#currying--partial-application)
4. [Function Composition](#function-composition)
5. [Higher-Order Functions on Lists](#higher-order-functions-on-lists)
6. [Monads: Maybe & List](#monads-maybe--list)
7. [Algebraic Data Types](#algebraic-data-types)
8. [Lazy Evaluation](#lazy-evaluation)
9. [Fixed-Point Combinators](#fixed-point-combinators)
10. [Performance & Tradeoffs](#performance--tradeoffs)

---

## Lambda Calculus Foundations

### Church encoding in C

The untyped lambda calculus has three syntactic forms:

```
e ::= x           (variable)
    | λx. e       (abstraction)
    | e₁ e₂       (application)
```

In our C implementation:

```c
Expr* var = expr_create_var("x");
Expr* lam = expr_create_lambda("x", body);
Expr* app = expr_create_apply(fn, arg);
```

### Church booleans:

```
true  = λt.λf.t
false = λt.λf.f
```

Implementation:

```c
Expr* church_true  = expr_create_lambda("t",
                       expr_create_lambda("f", expr_create_var("t")));
Expr* church_false = expr_create_lambda("t",
                       expr_create_lambda("f", expr_create_var("f")));
```

### Church numerals:

```
0 = λf.λx.x
1 = λf.λx.f x
2 = λf.λx.f (f x)
```

### Evaluation strategies:

| Strategy        | Description                           | Our default |
|-----------------|--------------------------------------|-------------|
| Call-by-value    | Evaluate arguments before application | Yes         |
| Call-by-name     | Substitute arguments unevaluated      | No          |
| Call-by-need     | Evaluate once, memoize               | No          |

---

## Closures & Environments

### Environment model

A closure pairs a function body with its **captured environment**:

```c
struct FPClosure {
    FPFnPtr fn_ptr;                  // code pointer
    void*   captured_env[16];        // captured free variables
    int     env_count;               // how many variables captured
    int     arity;                   // remaining args needed
    int     total_arity;             // original arity
};
```

### How capturing works:

When a lambda is created, it "closes over" variables that are **free** in its body (referenced but not bound within the lambda):

```
λx. x + y    // x is bound, y is free → y must be captured
```

In our implementation:

```c
FPClosure* cl = fp_closure_create(fn, total_arity, 0);
fp_closure_capture(cl, 0, &free_var_y);
```

### Environment representation choices:

1. **Flat closure** (our approach): array of captured values — fast, simple, fixed-size limit
2. **Linked environment**: chain of frames — supports nested scopes, slower lookup
3. **Display vector**: array indexed by nesting level — O(1) lookup, O(n) on scope entry/exit

---

## Currying & Partial Application

### Currying (Schonfinkelization)

Transform a function of N arguments into a chain of N unary functions:

```
f: (A, B) → C   becomes   curry(f): A → (B → C)
```

### Implementation approach:

```c
FPClosure* fp_curry(FPFnPtr fn, int arity) {
    // Returns a closure that:
    //   - when called with 1 arg: captures it, returns closure with arity-1
    //   - when arity reaches 1: calls the original fn with all captured args
    return fp_closure_create(fn, arity, 0);
}
```

### Partial application:

Unlike currying, partial application fixes **some** arguments immediately:

```haskell
add x y = x + y
add3 = add 3    -- partially applied: add3 y = 3 + y
```

### Implementation:

```c
FPClosure* partial_apply(FPFnPtr fn, int total_arity,
                          void** fixed_args, int fixed_count) {
    FPClosure* cl = fp_closure_create(fn, total_arity, fixed_count);
    for (int i = 0; i < fixed_count; i++) {
        fp_closure_capture(cl, i, fixed_args[i]);
    }
    return cl;
}
```

### Practical use cases:

- Configuration: fix database connection, remaining args are query parameters
- Event handlers: fix component reference, remaining arg is event
- Pipeline stages: fix transformer function, remaining arg is data

---

## Function Composition

### Mathematical definition:

```
(f ∘ g)(x) = f(g(x))
```

### Properties:

| Property          | Definition                          |
|-------------------|--------------------------------------|
| Associativity      | (h ∘ g) ∘ f = h ∘ (g ∘ f)          |
| Left identity      | id ∘ f = f                          |
| Right identity     | f ∘ id = f                          |
| (Not) commutative  | f ∘ g ≠ g ∘ f (in general)         |

### Implementation:

```c
FPClosure* fp_compose(FPClosure* f, FPClosure* g) {
    // Returns closure that:
    //   1. calls g with input args
    //   2. calls f with g's result
    FPClosure* cl = fp_closure_create(compose_invoke, g->arity, 0);
    cl->captured_env[0] = g;    // inner function
    cl->captured_env[1] = f;    // outer function
    cl->env_count = 2;
    return cl;
}
```

### Composition as category:

Functions and composition form a **category**:
- Objects are types
- Morphisms are functions
- Composition is `fp_compose`
- Identity is `fp_identity`

---

## Higher-Order Functions on Lists

### map: apply function to each element

```
map f [x₁, x₂, ..., xₙ] = [f x₁, f x₂, ..., f xₙ]
```

Type: `map :: (a → b) → [a] → [b]`

### filter: keep elements satisfying predicate

```
filter p [x₁, ..., xₙ] = [xᵢ | p xᵢ = true]
```

Type: `filter :: (a → Bool) → [a] → [a]`

### foldl (left fold):

```
foldl f z [x₁, x₂, x₃] = f (f (f z x₁) x₂) x₃
```

Type: `foldl :: (b → a → b) → b → [a] → b`

Properties:
- Tail-recursive (efficient in strict languages)
- Left-associative

### foldr (right fold):

```
foldr f z [x₁, x₂, x₃] = f x₁ (f x₂ (f x₃ z))
```

Type: `foldr :: (a → b → b) → b → [a] → b`

Properties:
- Works on infinite lists (in lazy languages)
- Right-associative

### Universal property of fold:

Any function on lists can be written as a fold:

```c
sum     = fp_foldl(add, 0, list)
product = fp_foldr(mul, 1, list)
length  = fp_foldl(inc, 0, list)
map f   = fp_foldr(cons ∘ f, nil, list)
```

---

## Monads: Maybe & List

### What is a Monad?

A monad is a design pattern with three components:

1. **Type constructor** M: wraps a type `a` into `M a`
2. **unit (return)**: `a → M a` — lifts a value into the monad
3. **bind (>>=)**: `M a → (a → M b) → M b` — chains computations

### Monad laws:

| Law             | Definition                               |
|-----------------|---------------------------------------|
| Left identity    | `return a >>= f`    ≡ `f a`             |
| Right identity   | `m >>= return`      ≡ `m`               |
| Associativity    | `(m >>= f) >>= g`   ≡ `m >>= (λx. f x >>= g)` |

### Maybe monad (handles nullable):

```c
typedef enum { MAYBE_NOTHING, MAYBE_JUST } MaybeTag;

typedef struct {
    MaybeTag tag;
    void*    value;
} Maybe;

Maybe maybe_unit(void* val) {
    return (Maybe){ .tag = MAYBE_JUST, .value = val };
}

Maybe maybe_bind(Maybe m, Maybe (*f)(void*)) {
    if (m.tag == MAYBE_NOTHING) return m;
    return f(m.value);
}
```

### Usage: safe division chain

```c
Maybe safe_div(int a, int b) {
    if (b == 0) return (Maybe){ MAYBE_NOTHING };
    int* result = malloc(sizeof(int));
    *result = a / b;
    return maybe_unit(result);
}

// Compute (100 / 5) / 2 safely
Maybe result = maybe_bind(
    maybe_bind(maybe_unit(&(int){100}),
               lambda(safe_div_5)),
    lambda(safe_div_2));
```

### List monad (nondeterminism):

```c
FPList* list_unit(void* val) {
    return fp_cons(val, NULL);
}

FPList* list_bind(FPList* xs, FPList* (*f)(void*)) {
    if (!xs) return NULL;
    return fp_list_append(f(xs->value), list_bind(xs->tail, f));
}
```

### List monad as backtracking:

```c
// All pairs (x, y) where x ∈ [1,2], y ∈ [3,4], x + y > 4
FPList* pairs = list_bind(list, λx.
                  list_bind(list, λy.
                    (x + y > 4) ? list_unit(pair(x,y)) : NULL));
```

---

## Algebraic Data Types (ADTs)

### Sum types (tagged unions):

```
data Option a = None | Some a
```

Implementation:

```c
typedef enum { OPTION_NONE, OPTION_SOME } OptionTag;

typedef struct {
    OptionTag tag;
    union {
        void* some_value;
    };
} Option;
```

### Product types:

```
data Pair a b = Pair a b
```

### Recursive types:

```
data List a = Nil | Cons a (List a)
```

### Pattern matching on ADTs:

```c
MatchCase cases[] = {
    match_case_create(pattern_cons("Nil", NULL, 0), handle_nil),
    match_case_create(pattern_cons("Cons", (Pattern*[]){
        pattern_var("x"), pattern_var("xs")
    }, 2), handle_cons),
};
DTNode* dt = match_compile(cases, 2);
```

### Generalized ADTs (GADTs):

GADTs allow type parameters to vary based on constructor:

```
data Expr a where
    Lit  :: Int → Expr Int
    Bool :: Bool → Expr Bool
    Add  :: Expr Int → Expr Int → Expr Int
    Eq   :: Expr Int → Expr Int → Expr Bool
```

---

## Lazy Evaluation

### Thunks: delayed computation

```c
typedef struct Thunk Thunk;
struct Thunk {
    void* (*compute)(Thunk* self);
    bool   evaluated;
    void*  value;
    Thunk* dependencies[8];
};

void* thunk_force(Thunk* t) {
    if (!t->evaluated) {
        t->value = t->compute(t);
        t->evaluated = true;
    }
    return t->value;
}
```

### Infinite data structures:

```c
Thunk* ones = make_thunk(λ. cons(1, ones));
Thunk* nats = make_thunk(λ. cons_from(0, λn. n+1));
Thunk* fibs = make_thunk(λ. cons(0, cons(1, zipWith(+)(fibs, tail(fibs)))));
```

### Strictness analysis:

Not all arguments need evaluation. A function is **strict** in an argument if it must evaluate that argument to produce a result. The compiler can use this to avoid creating thunks.

---

## Fixed-Point Combinators

### Y combinator (call-by-name):

```
Y = λf. (λx. f (x x)) (λx. f (x x))
```

### Z combinator (call-by-value):

```
Z = λf. (λx. f (λv. x x v)) (λx. f (λv. x x v))
```

### Implementation:

```c
Expr* z_combinator = expr_create_lambda("f",
    expr_create_apply(
        expr_create_lambda("x",
            expr_create_apply(expr_create_var("f"),
                expr_create_lambda("v",
                    expr_create_apply(
                        expr_create_apply(expr_create_var("x"), expr_create_var("x")),
                        expr_create_var("v")))),
        expr_create_lambda("x",
            expr_create_apply(expr_create_var("f"),
                expr_create_lambda("v",
                    expr_create_apply(
                        expr_create_apply(expr_create_var("x"), expr_create_var("x")),
                        expr_create_var("v"))))
    )));
```

### Factorial via Z combinator:

```
fact = Z (λf. λn. if (n == 0) 1 else n * f (n - 1))
```

---

## Performance & Tradeoffs

### Space:

| Structure      | Size per element    |
|----------------|---------------------|
| FPList node    | 16 bytes (value* + tail*) |
| FPClosure      | ~144 bytes (fn* + env[16] + metadata) |
| Thunk          | ~96 bytes (fn* + value + evaluated + deps) |

### Time:

| Operation      | Complexity     |
|----------------|----------------|
| fp_cons        | O(1)           |
| fp_map         | O(n)           |
| fp_foldl       | O(n)           |
| fp_foldr       | O(n), non-tail-recursive |
| fp_compose     | O(1) setup, O(depth) call |

### Memory considerations:

- Boxed primitives: ints/floats allocated on heap
- No sharing: each `fp_map` creates entirely new list
- Functional data structures (persistent) vs mutable

### Garbage collection:

Our implementation requires **manual memory management**. In a production FP runtime, you would add:

1. **Mark-and-sweep** tracing GC
2. **Reference counting** (with cycle detection)
3. **Region-based** memory management
