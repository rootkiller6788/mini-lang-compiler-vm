# Recursive Descent Parsing Deep Dive

> 递归下降解析：从文法规则到 AST 的完整推导

## What is Recursive Descent Parsing?

Recursive descent is a top-down parsing technique where each non-terminal in the grammar corresponds to a function. The parser starts from the start symbol and recursively expands productions, consuming tokens from left to right.

### Key Characteristics

1. **One function per grammar rule**: `parse_expression()`, `parse_statement()`, etc.
2. **Predictive**: Uses 1 token of lookahead to decide which production to apply (LL(1))
3. **Hand-written**: No parser generator (Yacc/Bison), full control over error handling
4. **Operator precedence**: Naturally encoded via function call nesting

---

## The Grammar

Our language grammar, expressed in EBNF notation:

```ebnf
program     ::= function*

function    ::= 'int' IDENT '(' params? ')' block

params      ::= param (',' param)*
param       ::= 'int' IDENT

block       ::= '{' statement* '}'

statement   ::= if_stmt
              | while_stmt
              | return_stmt
              | var_decl
              | block
              | expr? ';'

if_stmt     ::= 'if' '(' expr ')' statement
while_stmt  ::= 'while' '(' expr ')' statement
return_stmt ::= 'return' expr? ';'
var_decl    ::= 'int' IDENT ';'

expr        ::= assignment
assignment  ::= logical_or ('=' assignment)?
logical_or  ::= logical_and ('||' logical_and)*
logical_and ::= equality ('&&' equality)*
equality    ::= relational (('==' | '!=') relational)*
relational  ::= additive (('<' | '>' | '<=' | '>=') additive)*
additive    ::= multiplicative (('+' | '-') multiplicative)*
multiplicative ::= unary (('*' | '/') unary)*
unary       ::= ('-' | '!') unary
              | primary
primary     ::= INT_LIT
              | STRING
              | IDENT ('(' args? ')')?
              | '(' expr ')'
args        ::= expr (',' expr)*
```

---

## Left Factoring

Our grammar is already left-factored. Consider a problematic grammar:

```
expr ::= expr '+' term | expr '-' term | term
```

This is left-recursive: `expr` appears on the left side of its own productions. A recursive descent parser would call `parse_expr()` infinitely.

### Solution: Left-to-Right Rewriting

The standard transformation converts left recursion to right recursion using iteration:

```
expr ::= term (('+' | '-') term)*
```

In code, this becomes a loop:

```c
ASTNode *parse_additive(Parser *parser) {
    ASTNode *left = parse_multiplicative(parser);
    while (match(parser, TOK_PLUS) || match(parser, TOK_MINUS)) {
        ASTNode *node = ast_create_node(AST_BINARY_OP);
        node->op = parser->current_token.lexeme[0];
        ast_add_child(node, left);
        ast_add_child(node, parse_multiplicative(parser));
        left = node;
    }
    return left;
}
```

This iterative approach preserves left-to-right evaluation order while avoiding infinite recursion.

---

## Lookahead and LL(1) Properties

Our parser uses one token of lookahead (`peek_token`). The `match()` function consumes a token if it matches the expected type:

```c
static bool match(Parser *parser, TokenType type) {
    if (check(parser, type)) {
        advance_token(parser);
        return true;
    }
    return false;
}
```

### FIRST Sets

For any non-terminal, the FIRST set is the set of terminals that can begin strings derived from that non-terminal:

| Non-terminal | FIRST Set |
|-------------|-----------|
| program | `{int}` |
| function | `{int}` |
| statement | `{if, while, return, int, IDENT, \{` |
| expr | `{INT_LIT, STRING, IDENT, (, -, !` |

### Parsing `int main() { int x; x = 5; return x; }`

Let's trace through the parser step by step:

```
Step  Token        Function Called           Action
----  -----        ---------------           ------
  1   TOK_INT      parse_program            Enter program loop
  2   TOK_INT      parse_function           Expect 'int' -> match
  3   TOK_IDENT    parse_function           Read 'main' as function name
  4   TOK_LPAREN   parse_function           Expect '(' -> match
  5   TOK_RPAREN   parse_function           Expect ')' -> match (no params)
  6   TOK_LBRACE   parse_block              Expect '{' -> match
  7   TOK_INT      parse_statement          Lookahead: INT + IDENT = var decl
  8   TOK_IDENT    parse_var_decl           Read 'x', expect ';'
  9   TOK_SEMI     parse_var_decl           Match ';'
 10   TOK_IDENT    parse_statement          IDENT = expr statement
 11   TOK_EQ       parse_assignment         Match '=', parse right side
 12   TOK_INT_LIT  parse_primary            Create INT_LIT node
 13   TOK_SEMI     parse_expr_statement     Match ';'
 14   TOK_RETURN   parse_statement          Match 'return'
 15   TOK_IDENT    parse_return_statement   Parse expression
 16   TOK_SEMI     parse_return_statement   Match ';'
 17   TOK_RBRACE   parse_block              Match '}'
 18   TOK_EOF      parse_program            End loop
```

---

## Operator Precedence Implementation

Precedence is encoded structurally — higher precedence functions are called by lower precedence functions. Each level builds AST nodes with left-recursive grouping.

### Precedence Table

| Precedence | Operators | Function | Associativity |
|-----------|-----------|----------|---------------|
| 0 (lowest) | `=` | `parse_assignment` | Right-to-left |
| 1 | `\|\|` | `parse_logical_or` | Left-to-right |
| 2 | `&&` | `parse_logical_and` | Left-to-right |
| 3 | `==`, `!=` | `parse_equality` | Left-to-right |
| 4 | `<`, `>`, `<=`, `>=` | `parse_relational` | Left-to-right |
| 5 | `+`, `-` | `parse_additive` | Left-to-right |
| 6 | `*`, `/` | `parse_multiplicative` | Left-to-right |
| 7 | `-`, `!` (unary) | `parse_unary` | Right-to-left |
| 8 (highest) | literals, `()`, calls | `parse_primary` | N/A |

### Example: Parsing `1 + 2 * 3`

The parse trace for `1 + 2 * 3`:

```
parse_assignment()
  parse_logical_or()
    parse_logical_and()
      parse_equality()
        parse_relational()
          parse_additive()
            parse_multiplicative() ← called first
              parse_unary()
                parse_primary() → INT_LIT(1)
            TOK_PLUS found → loop:
              node = BINARY_OP('+')
              node->left = INT_LIT(1)
              node->right = parse_multiplicative()
                parse_unary()
                  parse_primary() → INT_LIT(2)
                TOK_STAR found → loop:
                  n2 = BINARY_OP('*')
                  n2->left = INT_LIT(2)
                  n2->right = parse_unary()
                    parse_primary() → INT_LIT(3)
                returns n2 (*)
              node->right = BINARY_OP('*', left=2, right=3)
            returns BINARY_OP('+', left=1, right=BINARY_OP('*', 2, 3))
```

Result:
```
BINARY_OP: '+'
  INT_LIT: 1
  BINARY_OP: '*'
    INT_LIT: 2
    INT_LIT: 3
```

### Right Associativity (Assignment)

Assignment is right-associative: `a = b = 5` parses as `a = (b = 5)`.

```c
static ASTNode *parse_assignment(Parser *parser) {
    ASTNode *left = parse_logical_or(parser);
    if (match(parser, TOK_EQ)) {
        ASTNode *assign = ast_create_node(AST_ASSIGN);
        ast_add_child(assign, left);
        ast_add_child(assign, parse_assignment(parser));  // Recursive on RHS
        return assign;
    }
    return left;
}
```

The recursive call to `parse_assignment(parser)` for the right side gives us right associativity.

---

## Pratt Parsing

While our implementation uses classic recursive descent, it's worth noting the Pratt (top-down operator precedence) approach:

```c
// Pratt-style: parse_expr(min_precedence)
ASTNode *parse_expr(int min_prec) {
    ASTNode *left = parse_prefix();    // Parse NUD (null denotation)
    while (get_precedence(current) >= min_prec) {
        left = parse_infix(left);      // Parse LED (left denotation)
    }
    return left;
}
```

Our recursive descent achieves the same result through explicit function nesting rather than a precedence table. Both approaches are valid; Pratt's is more compact for languages with many operator levels.

---

## Error Handling Strategies

### Panic Mode

When the parser encounters an unexpected token, it uses `expect()` which:
1. Prints an error with location
2. Increments the error counter
3. The caller decides whether to continue or bail

### Synchronization

For serious errors, parsers often skip tokens until a synchronization point (e.g., `;`, `}`). Our parser doesn't implement full sync, but the `match()` and `expect()` pattern allows it to continue past simple syntax errors.

### Example Error

Input: `int main() { int x y; }`
```
parser error at line 1 col 16: expected ';' in variable declaration, got 'y'
```

The parser reports the error and continues, so additional errors in the same file are also caught.

---

## AST Node Relationships

Understanding how AST nodes relate to grammar productions:

```
function → 'int' IDENT '(' params ')' block
              ↓
        AST_FUNC_DEF
        ├── name = IDENT.lexeme
        ├── child[0] = PARAM(param1)
        ├── child[1] = PARAM(param2)
        └── child[N] = BLOCK (the function body)

if_stmt → 'if' '(' expr ')' statement
              ↓
        AST_IF_STMT
        ├── child[0] = condition_expr
        ├── child[1] = then_statement
        └── child[2] = else_statement (optional)

binary_op → left OP right
              ↓
        AST_BINARY_OP
        ├── op = OP character
        ├── child[0] = left_expr
        └── child[1] = right_expr
```

---

## Comparison: Recursive Descent vs Parser Generators

| Aspect | Recursive Descent | Yacc/Bison |
|--------|-------------------|------------|
| Approach | Hand-written functions | Grammar specification + code gen |
| Control | Full control over AST construction | Limited control in semantic actions |
| Error messages | Custom, human-friendly | Often cryptic |
| Performance | Good, and predictable | Good, but depends on table construction |
| Learning curve | Understanding recursion and lookahead | Understanding LALR(1) conflicts |
| Debugging | Standard debugger, step through | Harder; state machine debugging |
| Grammar restrictions | Must avoid left recursion | Shift/reduce and reduce/reduce conflicts |
| Maintenance | Refactoring functions directly | Refactoring grammar rules |

---

## Further Reading

- **Crafting Interpreters** (Robert Nystrom), Chapters 5-6: A complete introduction to recursive descent parsing with a practical example in Java/C.
- **The Dragon Book** (Aho, Lam, Sethi, Ullman), Chapters 3-4: Formal treatment of LL and LR parsing, FIRST and FOLLOW sets.
- **Stanford CS143**: Lecture notes on top-down parsing, LL(1) grammars, and recursive descent.
- **Pratt Parsing** (Vaughan Pratt, 1973): The original paper on top-down operator precedence parsing.

---

## Key Takeaway

Recursive descent parsing transforms a grammar into a set of mutually recursive functions. Each function handles one non-terminal. Precedence is encoded by the call chain: `parse_additive` → `parse_multiplicative` → `parse_unary` → `parse_primary`. With one token of lookahead and proper left factoring, we get a clean, maintainable parser that directly mirrors the language specification.
