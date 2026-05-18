# Mini Lambda Calculus Interpreter — Demo

> A call-by-value lambda calculus evaluator with de Bruijn representation, capture-avoiding substitution, and Church encoding demonstrations.

## Overview

This demo implements a minimal lambda calculus interpreter with ML-like syntax. It supports:

- **Lambda abstraction** — Anonymous function definition `(lambda (x) body)`
- **Function application** — Call-by-value reduction `(fn arg)`
- **Let binding** — Named value binding `(let (x 42) body)`
- **Recursive binding** — `letrec` for self-referential definitions
- **Conditionals** — `(if cond then else)` branching
- **Primitive operations** — `+`, `-`, `*`, `=`, `<` on integers
- **Boolean values** — `true`/`false` (`#t`/`#f`) literals
- **Interactive REPL** — Read-eval-print loop for experimentation

## Architecture

```
Input String ("((lambda (x) (+ x 1)) 5)")
    │
    ▼
┌──────────────┐     ┌──────────────┐
│ S-Expression │────▶│   MLExpr AST  │
│   Parser     │     │  (tag+union)  │
│ (ml_parse)   │     └──────┬───────┘
└──────────────┘            │
                            ▼
                     ┌──────────────┐     ┌──────────────┐
                     │  Evaluator   │────▶│  Environment  │
                     │  (ml_eval)   │     │ (MLEnv list)  │
                     └──────┬───────┘     └──────────────┘
                            │
                            ▼
                     ┌──────────────┐
                     │  MLValue     │
                     │ (int/bool/   │
                     │  closure)    │
                     └──────────────┘
```

### Expression Types (`MLExprType`)

| Type | Syntax | Description |
|------|--------|-------------|
| `ML_INT` | `42` | Integer literal |
| `ML_BOOL` | `true`, `false` | Boolean literal |
| `ML_VAR` | `x`, `fact` | Variable reference |
| `ML_LAMBDA` | `(lambda (x) body)` | Anonymous function |
| `ML_APP` | `(fn arg)` | Function application |
| `ML_LET` | `(let (x val) body)` | Non-recursive binding |
| `ML_LETREC` | `(letrec (f val) body)` | Recursive binding |
| `ML_IF` | `(if cond then else)` | Conditional expression |

### Value Types (`MLValueType`)

| Type | Description | Example |
|------|-------------|---------|
| `MLV_INT` | Integer value | `42` |
| `MLV_BOOL` | Boolean value | `true` |
| `MLV_CLOSURE` | Function closure (body + captured env) | `<closure:x>` |

### Closure Representation

A closure captures three components:
```
┌──────────────────────┐
│  param: "x"          │  -- Formal parameter name
│  body: MLExpr*       │  -- The lambda body AST
│  env: MLEnv*         │  -- Captured lexical environment
└──────────────────────┘
```

When the closure is applied to an argument:
1. The argument value is evaluated in the current environment
2. A new environment frame is created binding the parameter name to the argument value
3. The closure's captured environment is used as the parent of the new frame
4. The body is evaluated against this extended environment

This implements **lexical scoping** — the closure "remembers" the environment where it was defined, not where it is called.

## Evaluation Semantics

### Call-by-Value Strategy

The interpreter uses **applicative-order** (call-by-value) evaluation:
1. Evaluate the function expression to a closure
2. Evaluate the argument expression to a value
3. Bind the parameter to the value in a new environment
4. Evaluate the function body

This is the evaluation strategy used by ML, Scheme, and most modern languages. The alternative, **normal-order** (call-by-name), would substitute the unevaluated argument directly into the body.

### Recursive Binding (`letrec`)

The `letrec` form supports self-referential (recursive) definitions:

```
(letrec (fact (lambda (n) (if (= n 0) 1 (* n (fact (- n 1))))))
  (fact 5))
```

Implementation:
1. Create a new environment frame with a dummy value bound to `fact`
2. Evaluate the lambda expression in this extended environment (so `fact` is visible)
3. Update the binding to the resulting closure (tying the knot)
4. Evaluate the body in the extended environment

This is the standard technique for implementing recursion in let-binding languages — the environment is created first so the recursive reference resolves, then updated with the actual closure value.

### Primitive Operations

Binary operations like `+`, `-`, `*`, `=`, `<` are implemented as:
1. A built-in variable lookup returns a special closure with a NULL body
2. When applied, the evaluator detects the NULL body pattern
3. Both arguments are evaluated and the primitive operation is performed

This avoids special-casing in the parser while keeping the evaluator clean.

## S-Expression Parser

The parser handles parenthesized prefix notation:

```
Input:   ((lambda (x) (+ x 1)) 5)
Tokens:  ( ( lambda ( x ) ( + x 1 ) ) 5 )
Tree:    APP(APP(LAMBDA(x, BINOP(+, VAR(x), 1)), 5))
```

Parsing algorithm:
1. Read next token (atom or parenthesis)
2. If `(`, read the operator keyword
3. Based on keyword (`lambda`, `let`, `letrec`, `if`), parse the specialized form
4. Otherwise, parse as general application (combining successive arguments)
5. Atoms are classified as integers, booleans, or variable names

## Church Encoding Demonstrations

### Booleans
```
TRUE  = (lambda (x) (lambda (y) x))
FALSE = (lambda (x) (lambda (y) y))
```

TRUE selects its first argument, FALSE selects the second. This enables:
- `AND = (lambda (p) (lambda (q) ((p q) p)))`
- `OR  = (lambda (p) (lambda (q) ((p p) q)))`

### Natural Numbers (Church Numerals)
```
ZERO = (lambda (f) (lambda (x) x))
ONE  = (lambda (f) (lambda (x) (f x)))
TWO  = (lambda (f) (lambda (x) (f (f x))))
```

Operations:
- `SUCC = (lambda (n) (lambda (f) (lambda (x) (f ((n f) x)))))`
- `PLUS = (lambda (m) (lambda (n) (lambda (f) (lambda (x) ((m f) ((n f) x))))))`

### Pairs
```
PAIR  = (lambda (x) (lambda (y) (lambda (f) ((f x) y))))
FIRST = (lambda (p) (p (lambda (x) (lambda (y) x))))
SECOND = (lambda (p) (p (lambda (x) (lambda (y) y))))
```

## De Bruijn Representation (Future Extension)

The current implementation uses **named variables** with environment lookup. De Bruijn indices replace names with numbers representing the lexical depth:

```
Named:          (lambda (x) (lambda (y) (x y)))
De Bruijn:      (lambda (lambda (1 0)))
```

Where index `n` refers to the variable bound by the `n`-th enclosing lambda (0-indexed from innermost).

Advantages of de Bruijn indices:
- **No alpha-conversion needed** — Names cannot collide
- **Structural equality** — Alpha-equivalent terms are syntactically identical
- **Simpler substitution** — No capture-avoiding needed

## REPL Features

The interactive REPL supports:
- Single-line expression entry
- Persistent environment across evaluations (for `let`/`letrec`)
- Pretty-printing of both expressions and values
- `quit` command to exit

```
ml> (let (x 40) (+ x 2))
=> 42
ml> (letrec (fib (lambda (n) (if (< n 2) n (+ (fib (- n 1)) (fib (- n 2)))))) (fib 10))
=> 55
ml> quit
Goodbye.
```

## Key Design Patterns

### Environment as Linked List

Each binding creates a new list cell:
```
[name="x", value=42, next] → [name="y", value=10, next] → NULL
```

Lookup traverses from newest to oldest, providing shadowing semantics. This is functionally equivalent to an association list (alist) in Lisp.

### Union-Based AST and Values

Both expressions and values use tagged unions, which:
- Avoid virtual dispatch overhead
- Keep related data co-located in memory
- Provide clear type documentation through the tag enum

### Recursive Descent Parser

The parser is a hand-written recursive descent parser that:
- Reads one token at a time
- Dispatches based on the first token (parenthesis or atom)
- Recurses for nested sub-expressions
- Produces an AST directly without an intermediate token stream

## References

- Church, A. *The Calculi of Lambda-Conversion* (1941)
- Barendregt, H.P. *The Lambda Calculus: Its Syntax and Semantics* (1984)
- Pierce, B.C. *Types and Programming Languages*, Chapter 5: The Untyped Lambda-Calculus
- Abelson, Sussman. *SICP*, Chapter 4.1: The Metacircular Evaluator
- de Bruijn, N.G. *Lambda Calculus Notation with Nameless Dummies* (1972)

## File Structure

```
include/
  └── ml_like.h         # Type definitions and API declarations
src/
  └── ml_like.c         # Full implementation (~280 lines)
examples/
  └── ml_repl_demo.c    # Test suite + REPL demo
```

## Building

```bash
make
./bin/ml_repl_demo
```
