# Compiler Frontend Primer

## The Compiler Pipeline

A compiler transforms source code into executable code through a series of stages. The **frontend** handles language-specific analysis:

```
Source Code -> Lexer -> Token Stream -> Parser -> AST -> Semantic Analyzer -> [Backend...]
```

The frontend is language-dependent; the backend is target-dependent. This separation allows retargeting compilers to new architectures without rewriting the frontend.

---

## Stage 1: Lexical Analysis (Scanning)

### Purpose

Convert a stream of characters into a stream of **tokens** — meaningful units like keywords, identifiers, operators, and literals. The lexer discards whitespace and comments.

### Input / Output

```
Input:  "int main() { return 42; }"
Output: TOK_INT TOK_IDENT("main") TOK_LPAREN TOK_RPAREN TOK_LBRACE TOK_RETURN TOK_INT_LIT("42") TOK_SEMI TOK_RBRACE TOK_EOF
```

### Key Concepts

**Token**: A pair of (type, lexeme). The type is a category (TOK_INT, TOK_IDENT); the lexeme is the actual text.

**Lexeme**: The substring of source code matched by a token pattern.

**Pattern**: A rule describing the character sequences that form a token. Usually specified by regular expressions.

### Implementation Approaches

| Approach | Description | Example |
|----------|-------------|---------|
| Hand-written scanner | Nested if/switch on characters | Our lexer, most production compilers |
| Lex/Flex | Scanner generator from regex specs | flex lexer.l |
| Regex-based | Use language regex library | Python: re.finditer() |

### Hand-written Lexer Pattern

```c
Token lexer_next_token(Lexer *lexer) {
    skip_whitespace_and_comments(lexer);
    if (isalpha(current)) return lex_identifier(lexer);
    if (isdigit(current)) return lex_number(lexer);
    switch (current) {
        case '+': return make_token(TOK_PLUS);
        case '-': return make_token(TOK_MINUS);
        // ... etc
    }
}
```

### Finite Automata Model

Every lexer corresponds to a deterministic finite automaton (DFA). The DFA's states represent positions in token recognition; transitions consume characters. The hand-written code directly encodes DFA transitions.

---

## Stage 2: Syntax Analysis (Parsing)

### Purpose

Verify that the token stream conforms to the language grammar and build an **Abstract Syntax Tree** (AST) representing the program's hierarchical structure.

### Input / Output

```
Input:  Token stream from lexer
Output: AST with root = PROGRAM, children = function definitions
```

### Key Concepts

**Context-Free Grammar (CFG)**: A formal specification of language syntax using productions:
```
E -> E + T | T
T -> T * F | F
F -> ( E ) | id | num
```

**Parse Tree**: A concrete tree showing exactly how each production was applied, including all tokens.

**Abstract Syntax Tree (AST)**: A simplified tree that captures the essential structure, omitting punctuation and syntactic sugar.

### Parsing Approaches

| Approach | Technique | Tools | Pros | Cons |
|----------|-----------|-------|------|------|
| Recursive Descent | Hand-written, one function per non-terminal | None | Full control, great errors | Must handle left recursion |
| LL(k) Parsing | Table-driven top-down | ANTLR | Predictable | Grammar restrictions |
| LR Parsing | Table-driven bottom-up | Yacc/Bison | Powerful grammar support | Harder to debug |
| Pratt Parsing | Precedence-driven | None | Compact for expressions | Less intuitive |
| PEG Parsing | Parsing expression grammar | peg/leg | No ambiguity | Recognizer, not generator |

### Recursive Descent in Practice

```c
ASTNode *parse_additive() {
    ASTNode *left = parse_multiplicative();
    while (match(TOK_PLUS) || match(TOK_MINUS)) {
        ASTNode *op = make_binary_node(op_char, left, parse_multiplicative());
        left = op;
    }
    return left;
}
```

### Operator Precedence Encoding

| Precedence | Operators | Associativity |
|-----------|-----------|---------------|
| Lowest | = | Right |
| | \|\| | Left |
| | && | Left |
| | == != | Left |
| | < > <= >= | Left |
| | + - | Left |
| | * / | Left |
| Highest | - ! (unary) | Right |

Each precedence level is a separate parsing function. Higher-precedence functions are called as "subexpression parsers" by lower-precedence functions.

---

## Stage 3: AST Construction

### Node Design

Three common patterns for AST node representation:

**1. Typed Hierarchy (OOP style)**
```java
abstract class Expr {}
class Binary extends Expr { Expr left; Token op; Expr right; }
class Literal extends Expr { Object value; }
```

**2. Union/Tagged Union (C style)**
```c
typedef struct ASTNode {
    ASTNodeType type;
    union { int int_val; char *str_val; char op; } data;
    ASTNode *children[MAX];
    int child_count;
} ASTNode;
```

**3. S-Expression (Lisp style)**
```lisp
(+ (* 2 3) 4)  ; = AST for 2 * 3 + 4
```

Our implementation uses pattern 2: a single struct with a type tag and n-ary children array.

### Visitor Pattern

The visitor pattern separates operations on the AST from the node structure:

```c
typedef void (*ASTVisitCallback)(ASTNode *node, void *user_data);

void ast_visit_preorder(ASTNode *node, ASTVisitCallback cb, void *data) {
    cb(node, data);
    for (int i = 0; i < node->child_count; i++)
        ast_visit_preorder(node->children[i], cb, data);
}
```

This allows semantic analysis, code generation, and optimization passes to be written as separate modules that traverse the same AST.

---

## Stage 4: Symbol Tables

### Purpose

Track identifiers (variables, functions) and their attributes across scopes. The symbol table maps names to information needed for semantic checking and code generation.

### Key Operations

- **Insert**: Register a new declaration in the current scope
- **Lookup**: Find the declaration for a name (searching outward through scopes)
- **Enter Scope**: Push a new scope (at block entry)
- **Exit Scope**: Pop the current scope (at block exit)

### Implementation Options

| Structure | Lookup | Insert | Scope Exit | Complexity |
|-----------|--------|--------|------------|------------|
| Hash table with chaining | O(1) avg | O(1) | O(n) per scope | Medium |
| Balanced tree | O(log n) | O(log n) | O(n log n) | High |
| Linear list per scope | O(n) | O(1) | O(n) | Low |
| Persistent hash map | O(1) | O(1) | O(1) | High |

Our implementation: hash table with chaining (256 buckets). Each scope has its own symbol table. Scopes form a linked list via parent pointers.

### Scope Rules

```
int x;           // Global scope, x declared

int foo() {      // Function scope entered
    int y;       // Function scope, y declared
    {            // Block scope entered
        int z;   // Block scope, z declared (shadows any outer z)
        x = y + z;  // x found in global, y in function, z in block
    }
    // z no longer visible
    return y;
}
```

---

## Stage 5: Semantic Analysis

### Purpose

Verify that the program is meaningful according to the language's semantic rules. This is the bridge between syntax (form) and meaning.

### Checks Performed

1. **Name Resolution**: Every identifier used must be declared. This catches typos and missing includes.

2. **Type Checking**: Operations must be applied to compatible types. `"hello" + 5` might be legal in some languages but is flagged here.

3. **Control Flow**: Functions declared to return a value must do so on all paths. `if (x) return 1;` without else is flagged if the function expects a return.

4. **Uniqueness**: No duplicate function names, no duplicate variable names in the same scope.

5. **Call Validation**: Function calls must reference actual functions with correct argument counts.

### Implementation

The semantic checker walks the AST, maintaining the symbol table as it goes:

```
visit FUNCTION_DEF:
    enter function scope
    register parameters
    walk body
    check return paths
    exit function scope

visit VARIABLE_DECL:
    check for redeclaration in current scope
    insert into symbol table

visit IDENTIFIER:
    lookup in symbol table
    report error if not found

visit BINARY_OP:
    check left operand
    check right operand
    verify type compatibility
```

---

## Lex vs Yacc vs Hand-written

| Aspect | Lex + Yacc | Hand-written |
|--------|------------|--------------|
| Development speed | Fast for prototypes | More initial code |
| Error messages | Generic "syntax error" | Custom, context-aware |
| Performance | Good (table-driven) | Excellent (direct code) |
| Grammar changes | Edit .l/.y files | Edit C functions |
| Debugging | Hard (state machine) | Standard debugger |
| Tool dependency | Requires Flex/Bison | None |
| Industry usage | GCC (historically) | Clang/LLVM, v8, modern compilers |

Most production compilers (Clang, Rust, Go) use hand-written parsers for full control over error recovery and diagnostics. Parser generators shine in research, DSLs, and rapid prototyping.

---

## Error Recovery Strategies

**Panic Mode**: Skip tokens until a synchronization token (;, }, etc.) is found. Simple but may cascade errors.

**Phrase-Level Recovery**: Replace or insert tokens based on known error patterns (e.g., missing semicolon).

**Error Productions**: Add grammar rules for common mistakes so they parse "correctly" with a diagnostic.

Our implementation uses error counting: errors are reported with location, the error count increments, and parsing continues. No synchronization points are defined.

---

## Summary

| Stage | Input | Output | Key Data Structure |
|-------|-------|--------|-------------------|
| Lexing | Characters | Tokens | Token stream |
| Parsing | Tokens | AST | Tree |
| Symbol Resolution | AST | Annotated identifiers | Hash table |
| Semantic Checking | AST + symbols | Diagnostics | Error list |

The frontend produces a validated AST and symbol table, ready for the middle-end (optimization) and back-end (code generation).
