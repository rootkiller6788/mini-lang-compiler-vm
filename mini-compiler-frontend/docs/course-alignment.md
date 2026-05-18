# Course Alignment

This module maps to Stanford CS143, Crafting Interpreters, and the Dragon Book.

## Stanford CS143: Compilers

| CS143 Topic | Our Module | Coverage |
|-------------|------------|----------|
| Lexical Analysis (Week 1-2) | src/lexer.c, include/lexer.h | Full: DFA-based scanning, token specification, keyword recognition, comment handling |
| Regular Expressions and Automata | Implicit in lexer design | Hand-written DFA via character-by-character scanning |
| Top-Down Parsing (Week 3-4) | src/parser.c, include/parser.h | Full: Recursive descent with 1-token lookahead (LL(1)) |
| Context-Free Grammars | Grammar in parser code and docs | Complete EBNF grammar for mini-C language |
| Abstract Syntax Trees (Week 4) | src/ast.c, include/ast.h | Full: 14 node types, n-ary tree, visitor pattern |
| Semantic Analysis (Week 5) | src/semantic.c, include/semantic.h | Full: identifier resolution, type checking, scope management |
| Symbol Tables (Week 5) | src/symtab.c, include/symtab.h | Full: hash table with chaining, nested scopes, parent-pointer chain |
| Error Recovery | Parser expect() and error counting | Error reporting with line/col, continued parsing |
| Type Systems | sem_check_expr() type checks | Int-only, demonstrates the framework |
| Control Flow | IF_STMT, WHILE_STMT in AST | AST nodes for control flow structures |

### CS143 Concepts Demonstrated

1. **Lexer-DFA Equivalence**: The lexer's switch on current character implements a deterministic finite automaton. Each state transition reads one character.

2. **LL(1) Property**: Our grammar is LL(1). One token of lookahead decides which production to apply. For example, TOK_INT followed by TOK_IDENT discriminates var_decl from function.

3. **FIRST and FOLLOW**: Implicitly encoded in match() calls. parse_statement() uses FIRST sets: TOK_IF -> if_stmt, TOK_WHILE -> while_stmt, TOK_RETURN -> return_stmt, TOK_INT -> var_decl, TOK_LBRACE -> block, default -> expr.

4. **Left Recursion Elimination**: E -> E + T transforms to E -> T (+ T)* via a while loop in parse_additive().

## Crafting Interpreters (Robert Nystrom)

| CI Chapter | Content | Our Implementation |
|------------|---------|-------------------|
| Chapter 4: Scanning | Hand-written lexer for Lox | src/lexer.c: keyword table, character-by-character scanning |
| Chapter 5: Representing Code | AST design and tree structure | src/ast.c: 14 node types with n-ary children |
| Chapter 6: Parsing Expressions | Recursive descent with precedence | src/parser.c: parse_additive(), parse_multiplicative(), etc. |
| Chapter 7: Evaluating Expressions | Tree-walk interpreter | Deferred to backend |
| Chapter 8: Statements and State | Statement parsing, environments | parse_statement(), src/symtab.c for scopes |

### Key Differences from Crafting Interpreters

| Aspect | Crafting Interpreters | Our Implementation |
|--------|----------------------|-------------------|
| Language | Java (jlox) / C (clox) | C99 only |
| Language parsed | Lox (dynamically typed) | Mini-C (statically typed, int-only) |
| AST representation | Class hierarchy per node type | Single ASTNode struct with type enum |
| Symbol table | HashMap in Java | Custom hash table with chaining |
| Visitor pattern | Generated via Java tooling | Manual ast_visit_preorder / ast_visit_postorder |
| Error recovery | Throw/catch in Java | Error count + fprintf |

## Dragon Book (Compilers: Principles, Techniques, and Tools)

| Dragon Book Chapter | Topic | Our Module Coverage |
|---------------------|-------|---------------------|
| Chapter 1 | Introduction to Compiling | README.md, docs/compiler-frontend-primer.md |
| Chapter 2 | Simple Syntax-Directed Translator | Not directly implemented |
| Chapter 3: Lexical Analysis | | src/lexer.c, include/lexer.h |
| 3.1-3.3 | Tokens, patterns, lexemes | Token struct, TokenType enum |
| 3.4 | Input buffering | Single-pass character scanning |
| 3.5 | Specification of tokens | Keyword table, operator matching |
| 3.6-3.7 | Finite automata, DFA to code | Hand-written DFA in lexer switch |
| Chapter 4: Syntax Analysis | | src/parser.c, include/parser.h |
| 4.1-4.2 | Context-free grammars | EBNF grammar in code and docs |
| 4.3 | Writing a grammar | Left-factored grammar for recursive descent |
| 4.4 | Top-down parsing | Full recursive descent implementation |
| 4.5 | Bottom-up parsing | Not covered |
| Chapter 5: Syntax-Directed Translation | | Semantic checker implementation |
| Chapter 6: Intermediate-Code Generation | | Deferred to backend |

### Dragon Book Techniques Used

1. **Chapter 3.4 - Lexeme Buffer**: The lexer copies lexemes into a fixed-size buffer in each Token struct, avoiding dangling pointers to the source string.

2. **Chapter 3.6 - DFA Implementation**: The switch statement on current character directly encodes DFA transition logic. Each case label is a state; state transitions call advance() and continue.

3. **Chapter 4.4 - Predictive Parsing**: Our parser is a predictive (LL(1)) recursive descent parser. No backtracking needed because the grammar is designed to be LL(1).

4. **Chapter 4.4 - First/Follow Conflict Resolution**: parse_statement() resolves the ambiguity between var_decl (int x;) and an expression starting with an identifier by checking two tokens of lookahead (int + IDENT).

## Course Alignment Summary

| Concept | CS143 | CI | Dragon Book | Our Module |
|---------|-------|----|-------------|------------|
| Lexical Analysis | Week 1-2 | Ch 4 | Ch 3 | src/lexer.c |
| Recursive Descent | Week 3-4 | Ch 6 | Ch 4.4 | src/parser.c |
| AST Construction | Week 4 | Ch 5 | Ch 5 | src/ast.c |
| Symbol Tables | Week 5 | Ch 8 | Ch 2 | src/symtab.c |
| Semantic Analysis | Week 5 | Ch 8 | Ch 5 | src/semantic.c |
| Error Handling | Week 2,4 | Ch 4,6 | Ch 4.8 | All sources |
