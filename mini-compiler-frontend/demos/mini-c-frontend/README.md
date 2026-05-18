# Mini C Frontend Walkthrough

> 从源码到语义检查：一个迷你 C 编译器前端的完整实现

## Overview

The Mini C Frontend is a hand-written compiler frontend for a small C-like language. It demonstrates the classic compiler pipeline:

```
Source Code ──► Lexer ──► Token Stream ──► Parser ──► AST ──► Semantic Checker ──► Validated AST
```

This document walks through each stage with concrete examples from the codebase.

---

## Stage 1: Lexical Analysis (Scanning)

### What It Does

The lexer converts raw source code characters into a stream of tokens. Each token is an atomic unit: keywords like `int` and `if`, identifiers like `foo` and `x`, literals like `42`, operators like `+` and `==`, and delimiters like `(` and `{`.

### Implementation: `src/lexer.c`

The lexer is a single-pass character scanner. It maintains:

- `source` — pointer to the input string
- `pos` — current byte offset
- `line`, `col` — position tracking for error reporting
- `current` — the current character (cached for performance)

#### Core Loop

```
lexer_next_token():
  1. skip_comments() — handles whitespace, // line comments, /* block comments */
  2. Check current character:
     - Alphabetic / '_' → lex_ident_or_keyword()
     - Digit           → lex_number()
     - '"'             → lex_string()
     - Operator char   → single/multi-char operator
  3. Return a Token struct with type, lexeme text, line, column
```

#### Keyword Matching

Keywords are recognized during identifier scanning. After collecting an identifier's characters, we compare against a static keyword table:

```c
static const KeywordEntry keywords[] = {
    {"int",    TOK_INT},
    {"if",     TOK_IF},
    {"while",  TOK_WHILE},
    {"return", TOK_RETURN},
    {NULL,     TOK_ERROR}
};
```

If the lexeme matches a keyword, the token type is set accordingly. Otherwise, it's `TOK_IDENT`.

#### Comment Handling

Two comment styles are supported:

- **Line comments** (`// ...`): Skip until newline or EOF
- **Block comments** (`/* ... */`): Skip until `*/`, tracking newlines for line counting. Unclosed block comments produce an error.

#### Multi-character Operators

The lexer uses single-character lookahead for operators like `==`, `!=`, `<=`, `>=`, `&&`, `||`:

```c
case '=':
    if (source[pos + 1] == '=') { return make_eq_eq(); }
    return make_eq();
```

#### Token Types

| Token | Description | Example |
|-------|-------------|---------|
| TOK_INT | `int` keyword | `int` |
| TOK_IF | `if` keyword | `if` |
| TOK_WHILE | `while` keyword | `while` |
| TOK_RETURN | `return` keyword | `return` |
| TOK_IDENT | Identifier | `foo`, `x`, `main` |
| TOK_INT_LIT | Integer literal | `42`, `0`, `12345` |
| TOK_STRING | String literal | `"hello"` |
| TOK_PLUS | `+` | |
| TOK_MINUS | `-` | |
| TOK_STAR | `*` | |
| TOK_SLASH | `/` | |
| TOK_LPAREN | `(` | |
| TOK_RPAREN | `)` | |
| TOK_LBRACE | `{` | |
| TOK_RBRACE | `}` | |
| TOK_SEMI | `;` | |
| TOK_EQ | `=` | |
| TOK_EQ_EQ | `==` | |
| TOK_NEQ | `!=` | |
| TOK_LT | `<` | |
| TOK_GT | `>` | |
| TOK_LE | `<=` | |
| TOK_GE | `>=` | |
| TOK_AND | `&&` | |
| TOK_OR | `\|\|` | |
| TOK_NOT | `!` | |
| TOK_COMMA | `,` | |
| TOK_EOF | End of file | |

### Example

Input:
```c
int main() { return 42; }
```

Output token stream:
```
TOK_INT    'int'      line=1 col=1
TOK_IDENT  'main'     line=1 col=5
TOK_LPAREN '('        line=1 col=9
TOK_RPAREN ')'        line=1 col=10
TOK_LBRACE '{'        line=1 col=12
TOK_RETURN 'return'   line=1 col=14
TOK_INT_LIT '42'      line=1 col=21
TOK_SEMI   ';'        line=1 col=23
TOK_RBRACE '}'        line=1 col=25
TOK_EOF    ''         line=1 col=26
```

---

## Stage 2: Parsing (Syntax Analysis)

### What It Does

The parser takes the token stream and builds an Abstract Syntax Tree (AST) according to the language grammar. It uses recursive descent parsing, where each grammar rule maps to a parsing function.

### Implementation: `src/parser.c`

The parser maintains:
- `lexer` — the lexer instance
- `current_token` — the token currently being processed
- `peek_token` — one token of lookahead
- `error_count` — error tracking for recovery

#### Grammar

```
program     → function*
function    → 'int' IDENT '(' params? ')' block
params      → param (',' param)*
param       → 'int' IDENT
block       → '{' statement* '}'
statement   → if_stmt | while_stmt | return_stmt | var_decl | block | expr ';'
if_stmt     → 'if' '(' expr ')' statement
while_stmt  → 'while' '(' expr ')' statement
return_stmt → 'return' expr? ';'
var_decl    → 'int' IDENT ';'
expr        → assignment
assignment  → logical_or ('=' assignment)?
logical_or  → logical_and ('||' logical_and)*
logical_and → equality ('&&' equality)*
equality    → relational (('==' | '!=') relational)*
relational  → additive (('<' | '>' | '<=' | '>=') additive)*
additive    → multiplicative (('+' | '-') multiplicative)*
multiplicative → unary (('*' | '/') unary)*
unary       → ('-' | '!') unary | primary
primary     → INT_LIT | STRING | IDENT ('(' args? ')')? | '(' expr ')'
```

#### Operator Precedence

Each expression parsing function handles one precedence level:

| Level | Function | Operators | Associativity |
|-------|----------|-----------|---------------|
| 1 | `parse_assignment` | `=` | Right |
| 2 | `parse_logical_or` | `\|\|` | Left |
| 3 | `parse_logical_and` | `&&` | Left |
| 4 | `parse_equality` | `==`, `!=` | Left |
| 5 | `parse_relational` | `<`, `>`, `<=`, `>=` | Left |
| 6 | `parse_additive` | `+`, `-` | Left |
| 7 | `parse_multiplicative` | `*`, `/` | Left |
| 8 | `parse_unary` | `-`, `!` | Right |
| 9 | `parse_primary` | literals, identifiers, calls, parens | N/A |

This is a standard top-down operator precedence (Pratt-style) pattern where each level calls the next higher precedence level for operands.

#### Error Recovery

The parser uses `expect()` for required tokens, printing an error and advancing past the problem. Errors produce a non-zero count but parsing continues as best as possible:

```c
static void expect(Parser *parser, TokenType type, const char *context) {
    if (!match(parser, type)) {
        fprintf(stderr, "parser error: expected '%s' in %s, got '%s'\n", ...);
        parser->error_count++;
    }
}
```

---

## Stage 3: AST Construction

### What It Does

The parser builds tree nodes representing the program structure. Each node has:
- `type` — kind of node (FUNC_DEF, BINARY_OP, INT_LIT, etc.)
- `name` — identifier name or string content
- `int_value` — integer literal value
- `op` — operator character
- `children[]` — array of child subtrees
- `line`, `col` — source position

### AST Node Types

| Node Type | Children | Purpose |
|-----------|----------|---------|
| AST_PROGRAM | function* | Root node |
| AST_FUNC_DEF | param*, block | Function definition |
| AST_BLOCK | statement* | Braced block `{...}` |
| AST_IF_STMT | cond, then, [else] | If statement |
| AST_WHILE_STMT | cond, body | While loop |
| AST_RETURN_STMT | [expr] | Return statement |
| AST_BINARY_OP | left, right | Binary operation (`a + b`) |
| AST_UNARY_OP | operand | Unary operation (`-a`) |
| AST_INT_LIT | none | Integer literal |
| AST_IDENT | none | Identifier reference |
| AST_ASSIGN | target, value | Assignment (`a = b`) |
| AST_CALL | args* | Function call |
| AST_PARAM | none | Parameter declaration |
| AST_VAR_DECL | none | Variable declaration |
| AST_STRING_LIT | none | String literal |

---

## Stage 4: Symbol Table

### What It Does

The symbol table tracks all declared identifiers across nested scopes. It supports:
- Block scoping (each `{...}` creates a new scope)
- Lookup with parent scope fallback
- Shadowing (inner scopes can hide outer declarations)
- Duplicate detection within a scope

### Implementation: `src/symtab.c`

The symbol table uses:
- A 256-bucket hash table with chaining for each scope
- A linked list of scopes (parent pointer) for nested blocks
- djb2 hash function for string hashing

#### Scope Management

```c
SymTab *symtab_enter_scope(SymTab *tab);   // Create new nested scope
SymTab *symtab_exit_scope(SymTab *tab);    // Pop scope, free its symbols
```

When entering a block `{`, push a new scope. Variables declared inside are only visible within that block and its children. When exiting `}`, the scope is popped and its symbols are freed.

#### Lookup Rules

- `symtab_lookup()` — searches current scope, then parent, then grandparent, etc.
- `symtab_lookup_current()` — searches only the current scope (for duplicate detection)

---

## Stage 5: Semantic Analysis

### What It Does

The semantic checker validates the AST after parsing:

1. **Function existence**: Checks that `main()` is defined
2. **No duplicate definitions**: Two functions with the same name, or two variables in the same scope
3. **Identifier resolution**: Every identifier used must be declared
4. **Type checking**: Expressions are checked for type consistency (int-only)
5. **Return paths**: Functions declared `int` must return on all code paths
6. **Call validity**: Function calls target declared functions

### Implementation: `src/semantic.c`

The checker walks the AST tree, building the symbol table as it encounters declarations:

```
sem_check_program():
  1. Enter global scope
  2. Register all function names in global scope
  3. Check for 'main'
  4. For each function: call sem_check_function()

sem_check_function():
  1. Enter function scope
  2. Register all parameters
  3. Check for duplicate parameters
  4. Walk the function body with sem_check_statement()
  5. Verify all paths return
  6. Exit function scope

sem_check_statement():
  - For blocks: enter new scope, recurse, exit scope
  - For var_decl: check redeclaration, insert into scope
  - For return: check expression type
  - For if/while: check condition, recurse body

sem_check_expr():
  - For identifiers: verify declared in symtab
  - For binary ops: check operand types
  - For assignments: verify target declared
  - For calls: verify function declared, is a function
```

#### Error Reporting

Errors are printed to stderr with line/column information:

```
semantic error at line 5 col 10: undeclared identifier 'y'
semantic error at line 0 col 0: program must have a 'main' function
semantic error at line 3 col 5: duplicate parameter 'a' in function 'foo'
```

---

## Building and Running

```bash
make                    # Build all demos
make run-lexer          # Tokenize sample code
make run-parser         # Parse and print AST
make run-semantic       # Run semantic checks
```

---

## Design Principles

1. **Single-pass lexer**: The lexer processes characters once, producing tokens on demand.
2. **Recursive descent with lookahead**: One token of lookahead is sufficient for our grammar (LL(1)).
3. **Immutable AST during checking**: The semantic checker reads the AST but doesn't modify it.
4. **Error recovery**: Parsing continues after errors to report as many issues as possible.
5. **C99 compliance**: Uses only standard C99 + libc + libm, no external dependencies.

---

## Limitations

- Only `int` type is supported
- No structs, arrays, or pointers
- No function overloading
- Single-file programs only (no `#include`)
- No short-circuit evaluation semantics (handled in code generation phase)
- String literals are recognized but have limited operations
