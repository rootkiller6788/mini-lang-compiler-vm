/* ==========================================================================
 * compiler.c — Complete Compiler Frontend Implementation
 *
 * Pipeline: source → tokens (lexer) → AST (parser) → bytecode (codegen)
 *
 * Knowledge coverage:
 *   L2: Lexical analysis (DFA-based maximal-munch, keyword trie)
 *   L2: Recursive-descent parsing with Pratt operator precedence
 *   L3: Single-pass compiler pipeline with arena allocation
 *   L4: Chomsky hierarchy: DFA lexer (regex), LL(1)/Pratt parser (CFG)
 *   L5: Pratt (1973) top-down operator precedence, O(n)
 *   L6: Full expression/statement compiler with correct precedence
 *   L7: REPL, expression evaluator
 *
 * Refs: Pratt (1973), Wirth (1976), Nystrom (2015), Aho et al. (2006)
 * ========================================================================== */

#include "compiler.h"
#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

/* =========================================================================
 * Token Utilities & Error Reporting
 * ========================================================================= */

const char* token_type_name(TokenType t) {
    static const char* names[] = {
        [TOK_EOF]="EOF", [TOK_INT_LIT]="INT", [TOK_FLOAT_LIT]="FLOAT",
        [TOK_STRING_LIT]="STRING", [TOK_IDENT]="IDENT",
        [TOK_LET]="let", [TOK_IF]="if", [TOK_ELSE]="else",
        [TOK_WHILE]="while", [TOK_FN]="fn", [TOK_RETURN]="return",
        [TOK_PRINT]="print", [TOK_TRUE]="true", [TOK_FALSE]="false",
        [TOK_NIL]="nil", [TOK_PLUS]="+", [TOK_MINUS]="-",
        [TOK_STAR]="*", [TOK_SLASH]="/", [TOK_BANG]="!",
        [TOK_EQ]="==", [TOK_NE]="!=", [TOK_LT]="<", [TOK_GT]=">",
        [TOK_LE]="<=", [TOK_GE]=">=", [TOK_AND]="&&", [TOK_OR]="||",
        [TOK_ASSIGN]="=", [TOK_LPAREN]="(", [TOK_RPAREN]=")",
        [TOK_LBRACE]="{", [TOK_RBRACE]="}", [TOK_SEMICOLON]=";",
        [TOK_COMMA]=","
    };
    if (t >= 0 && t < TOK_COUNT) return names[t];
    return "UNKNOWN";
}

const char* binop_name(BinaryOp op) {
    static const char* names[] = {
        [BINOP_ADD]="+",[BINOP_SUB]="-",[BINOP_MUL]="*",
        [BINOP_DIV]="/",[BINOP_EQ]="==",[BINOP_NE]="!=",
        [BINOP_LT]="<",[BINOP_GT]=">",[BINOP_LE]="<=",
        [BINOP_GE]=">=",[BINOP_AND]="&&",[BINOP_OR]="||"
    };
    return names[op];
}

void compiler_error(Compiler* c, int32_t line, const char* fmt, ...) {
    c->had_error = true;
    va_list args;
    va_start(args, fmt);
    vsnprintf(c->error_msg, sizeof(c->error_msg), fmt, args);
    va_end(args);
    fprintf(stderr, "[line %d] Compiler Error: %s\n", line, c->error_msg);
}


/* =========================================================================
 * Lexer — DFA-based Tokenization (L2, L5)
 *
 * Complexity: O(n) over source. Each char examined ≤2 times.
 * Uses maximal-munch for multi-char operators (==, !=, <=, etc.)
 * and a first-char dispatch trie for keyword recognition.
 * ========================================================================= */

static bool lex_is_at_end(const Compiler* c) {
    return c->current >= c->source + c->source_len || *c->current == '\0';
}
static char lex_peek(const Compiler* c) {
    return lex_is_at_end(c) ? '\0' : *c->current;
}
static char lex_peek_next(const Compiler* c) {
    if (lex_is_at_end(c) || c->current+1 >= c->source + c->source_len)
        return '\0';
    return *(c->current + 1);
}
static char lex_advance(Compiler* c) {
    char ch = *c->current; c->current++; c->column++; return ch;
}
static bool lex_match(Compiler* c, char expected) {
    if (lex_is_at_end(c) || *c->current != expected) return false;
    c->current++; c->column++; return true;
}

/**
 * skip_whitespace_and_comments:
 * Handles spaces, tabs, newlines (counting lines), line comments,
 * and slash-star nested block comments. Nesting tracked for balance.
 */
static void skip_whitespace_and_comments(Compiler* c) {
    for (;;) {
        char ch = lex_peek(c);
        switch (ch) {
        case ' ': case '\r': case '\t': lex_advance(c); break;
        case '\n': c->line++; c->column = 0; lex_advance(c); break;
        case '/':
            if (lex_peek_next(c) == '/') {
                while (!lex_is_at_end(c) && lex_peek(c) != '\n')
                    lex_advance(c);
            } else if (lex_peek_next(c) == '*') {
                lex_advance(c); lex_advance(c);
                int nesting = 1;
                while (nesting > 0 && !lex_is_at_end(c)) {
                    if (lex_peek(c) == '/' && lex_peek_next(c) == '*')
                    { lex_advance(c); lex_advance(c); nesting++; }
                    else if (lex_peek(c) == '*' && lex_peek_next(c) == '/')
                    { lex_advance(c); lex_advance(c); nesting--; }
                    else if (lex_peek(c) == '\n')
                    { c->line++; c->column = 0; lex_advance(c); }
                    else { lex_advance(c); }
                }
            } else { return; }
            break;
        default: return;
        }
    }
}

/* Token factory helpers */
static Token make_tok(Compiler* c, TokenType type) {
    Token tok; memset(&tok, 0, sizeof(tok));
    tok.type = type; tok.line = c->line; tok.column = c->column;
    tok.start = c->current; return tok;
}
static Token make_tok_str(Compiler* c, TokenType type,
                          const char* start, int len) {
    Token tok; memset(&tok, 0, sizeof(tok));
    tok.type = type; tok.line = c->line;
    tok.column = c->column - len; tok.start = start; tok.length = len;
    return tok;
}

/* Keyword trie: O(1) lookup by first-char dispatch (L5) */
static TokenType check_keyword(const char* start, int len) {
    if (len < 2 || len > 6) return TOK_IDENT;
    switch (start[0]) {
    case 'e': if (len==4 && !memcmp(start,"else",4)) return TOK_ELSE; break;
    case 'f': if (len==5 && !memcmp(start,"false",5)) return TOK_FALSE;
              if (len==2 && !memcmp(start,"fn",2)) return TOK_FN; break;
    case 'i': if (len==2 && !memcmp(start,"if",2)) return TOK_IF; break;
    case 'l': if (len==3 && !memcmp(start,"let",3)) return TOK_LET; break;
    case 'n': if (len==3 && !memcmp(start,"nil",3)) return TOK_NIL; break;
    case 'p': if (len==5 && !memcmp(start,"print",5)) return TOK_PRINT; break;
    case 'r': if (len==6 && !memcmp(start,"return",6)) return TOK_RETURN; break;
    case 't': if (len==4 && !memcmp(start,"true",4)) return TOK_TRUE; break;
    case 'w': if (len==5 && !memcmp(start,"while",5)) return TOK_WHILE; break;
    default: break;
    }
    return TOK_IDENT;
}

/* scan_token: Main lexer dispatch. One token per call. */
static void scan_token(Compiler* c) {
    skip_whitespace_and_comments(c);
    if (lex_is_at_end(c)) {
        if (c->token_count < CPL_MAX_TOKENS)
            c->tokens[c->token_count++] = make_tok(c, TOK_EOF);
        return;
    }
    const char* start = c->current;
    char ch = lex_advance(c);

    /* Numbers: integer or float with maximal-munch */
    if (isdigit((unsigned char)ch)) {
        bool is_float = false;
        while (isdigit((unsigned char)lex_peek(c))) lex_advance(c);
        if (lex_peek(c) == '.' && isdigit((unsigned char)lex_peek_next(c))) {
            is_float = true; lex_advance(c);
            while (isdigit((unsigned char)lex_peek(c))) lex_advance(c);
        }
        int len = (int)(c->current - start);
        if (c->token_count >= CPL_MAX_TOKENS) return;
        Token tok = make_tok_str(c, is_float ? TOK_FLOAT_LIT : TOK_INT_LIT, start, len);
        if (is_float) tok.value.float_val = strtod(start, NULL);
        else tok.value.int_val = (int64_t)strtoll(start, NULL, 10);
        c->tokens[c->token_count++] = tok;
        return;
    }

    /* Identifiers & keywords */
    if (isalpha((unsigned char)ch) || ch == '_') {
        while (isalnum((unsigned char)lex_peek(c)) || lex_peek(c) == '_')
            lex_advance(c);
        int len = (int)(c->current - start);
        if (c->token_count >= CPL_MAX_TOKENS) return;
        TokenType type = check_keyword(start, len);
        c->tokens[c->token_count++] = make_tok_str(c, type, start, len);
        return;
    }

    /* String literals */
    if (ch == '"') {
        while (!lex_is_at_end(c) && lex_peek(c) != '"') {
            if (lex_peek(c) == '\n') { c->line++; c->column = 0; }
            lex_advance(c);
        }
        if (!lex_is_at_end(c)) lex_advance(c);
        if (c->token_count < CPL_MAX_TOKENS)
            c->tokens[c->token_count++] = make_tok_str(c, TOK_STRING_LIT, start,
                (int)(c->current - start));
        return;
    }

    /* Two-char operators (maximal-munch) */
    if (ch == '=' && lex_match(c, '=')) {
        if (c->token_count < CPL_MAX_TOKENS)
            c->tokens[c->token_count++] = make_tok(c, TOK_EQ); return; }
    if (ch == '!' && lex_match(c, '=')) {
        if (c->token_count < CPL_MAX_TOKENS)
            c->tokens[c->token_count++] = make_tok(c, TOK_NE); return; }
    if (ch == '<' && lex_match(c, '=')) {
        if (c->token_count < CPL_MAX_TOKENS)
            c->tokens[c->token_count++] = make_tok(c, TOK_LE); return; }
    if (ch == '>' && lex_match(c, '=')) {
        if (c->token_count < CPL_MAX_TOKENS)
            c->tokens[c->token_count++] = make_tok(c, TOK_GE); return; }
    if (ch == '&' && lex_match(c, '&')) {
        if (c->token_count < CPL_MAX_TOKENS)
            c->tokens[c->token_count++] = make_tok(c, TOK_AND); return; }
    if (ch == '|' && lex_match(c, '|')) {
        if (c->token_count < CPL_MAX_TOKENS)
            c->tokens[c->token_count++] = make_tok(c, TOK_OR); return; }

    /* Single-char operators/punctuation */
    TokenType t;
    switch (ch) {
    case '+': t = TOK_PLUS; break;    case '-': t = TOK_MINUS; break;
    case '*': t = TOK_STAR; break;    case '/': t = TOK_SLASH; break;
    case '!': t = TOK_BANG; break;    case '<': t = TOK_LT; break;
    case '>': t = TOK_GT; break;      case '=': t = TOK_ASSIGN; break;
    case '(': t = TOK_LPAREN; break;  case ')': t = TOK_RPAREN; break;
    case '{': t = TOK_LBRACE; break;  case '}': t = TOK_RBRACE; break;
    case ';': t = TOK_SEMICOLON; break; case ',': t = TOK_COMMA; break;
    default:
        compiler_error(c, c->line, "unexpected char '%c'", ch); return;
    }
    if (c->token_count < CPL_MAX_TOKENS)
        c->tokens[c->token_count++] = make_tok(c, t);
}

/* L5: compiler_lex — O(n) full-scan tokenizer */
void compiler_lex(Compiler* c) {
    c->current = c->source; c->line = 1; c->column = 0; c->token_count = 0;
    while (c->token_count < CPL_MAX_TOKENS) {
        int prev = c->token_count;
        scan_token(c);
        if (c->token_count == prev) break;
        if (c->tokens[c->token_count - 1].type == TOK_EOF) break;
    }
    if (c->token_count < CPL_MAX_TOKENS &&
        (c->token_count == 0 ||
         c->tokens[c->token_count - 1].type != TOK_EOF))
        c->tokens[c->token_count++] = make_tok(c, TOK_EOF);
}


/* =========================================================================
 * Parser — Recursive Descent + Pratt Operator Precedence (L5, L6)
 *
 * Grammar:  program := statement* EOF
 * Statement forms: let, if, while, print, return, fn, block, expr;
 * Expression: Pratt-style precedence climbing, levels 1-6.
 *
 * Time: O(n) over tokens. Space: O(d) stack for recursion depth d.
 * ========================================================================= */

/* Token stream navigation */
static Token cur_tok(const Compiler* c) {
    return (c->token_pos < c->token_count)
        ? c->tokens[c->token_pos] : (Token){TOK_EOF,0,0,0,NULL,{0}};
}
static Token adv_tok(Compiler* c) {
    return (c->token_pos < c->token_count)
        ? c->tokens[c->token_pos++] : (Token){TOK_EOF,0,0,0,NULL,{0}};
}
static bool chk_tok(const Compiler* c, TokenType t) {
    return cur_tok(c).type == t;
}
static bool mat_tok(Compiler* c, TokenType t) {
    if (chk_tok(c, t)) { adv_tok(c); return true; }
    return false;
}
static bool cnsm_tok(Compiler* c, TokenType t, const char* msg) {
    if (chk_tok(c, t)) { adv_tok(c); return true; }
    compiler_error(c, cur_tok(c).line, "%s (got '%s')",
                   msg, token_type_name(cur_tok(c).type));
    return false;
}

/* AST node arena allocation — O(1) amortized from Compiler.nodes[] */
static ASTNode* alloc_ast(Compiler* c, ASTNodeType type) {
    if (c->node_count >= CPL_MAX_AST_NODES) {
        compiler_error(c, 0, "AST overflow (max %d)", CPL_MAX_AST_NODES);
        return NULL;
    }
    ASTNode* n = &c->nodes[c->node_count++];
    memset(n, 0, sizeof(ASTNode));
    n->type = type; n->line = cur_tok(c).line;
    return n;
}

/* Forward declarations */
static ASTNode* parse_expr(Compiler* c);
static ASTNode* parse_stmt(Compiler* c);

/* Pratt precedence: higher number = tighter binding */
static int prec_of(TokenType op) {
    switch (op) {
    case TOK_OR:  return 1; case TOK_AND: return 2;
    case TOK_EQ: case TOK_NE: case TOK_LT: case TOK_GT:
    case TOK_LE: case TOK_GE: return 3;
    case TOK_PLUS: case TOK_MINUS: return 4;
    case TOK_STAR: case TOK_SLASH:  return 5;
    default: return -1; /* -1 ensures non-operators break the Pratt loop */
    }
}
static BinaryOp tok2bin(TokenType t) {
    switch (t) {
    case TOK_PLUS: return BINOP_ADD; case TOK_MINUS: return BINOP_SUB;
    case TOK_STAR: return BINOP_MUL; case TOK_SLASH: return BINOP_DIV;
    case TOK_EQ:   return BINOP_EQ; case TOK_NE:     return BINOP_NE;
    case TOK_LT:   return BINOP_LT; case TOK_GT:     return BINOP_GT;
    case TOK_LE:   return BINOP_LE; case TOK_GE:     return BINOP_GE;
    case TOK_AND:  return BINOP_AND;case TOK_OR:     return BINOP_OR;
    default: return BINOP_ADD;
    }
}

/* --- Primary expressions --- */
static ASTNode* parse_primary(Compiler* c) {
    Token tok = cur_tok(c);
    if (mat_tok(c, TOK_INT_LIT)) {
        ASTNode* n = alloc_ast(c, AST_INT_LIT);
        if (n) { n->data.int_val = tok.value.int_val; n->line = tok.line; }
        return n;
    }
    if (mat_tok(c, TOK_FLOAT_LIT)) {
        ASTNode* n = alloc_ast(c, AST_FLOAT_LIT);
        if (n) { n->data.float_val = tok.value.float_val; n->line = tok.line; }
        return n;
    }
    if (mat_tok(c, TOK_STRING_LIT)) {
        ASTNode* n = alloc_ast(c, AST_STRING_LIT);
        if (n) {
            int len = tok.length - 2;
            if (len >= VALUE_STRING_LEN) len = VALUE_STRING_LEN - 1;
            strncpy(n->data.str_val.str, tok.start + 1, len);
            n->data.str_val.str[len] = '\0'; n->line = tok.line;
        }
        return n;
    }
    if (mat_tok(c, TOK_TRUE)) {
        ASTNode* n = alloc_ast(c, AST_BOOL_LIT);
        if (n) n->data.bool_val = true; return n;
    }
    if (mat_tok(c, TOK_FALSE)) {
        ASTNode* n = alloc_ast(c, AST_BOOL_LIT);
        if (n) n->data.bool_val = false; return n;
    }
    if (mat_tok(c, TOK_NIL)) return alloc_ast(c, AST_NIL_LIT);

    if (mat_tok(c, TOK_IDENT)) {
        ASTNode* n = alloc_ast(c, AST_IDENT);
        if (n) {
            strncpy(n->data.str_val.str, tok.start, tok.length);
            n->data.str_val.str[tok.length] = '\0'; n->line = tok.line;
        }
        /* Function call? f(args) */
        if (chk_tok(c, TOK_LPAREN)) {
            adv_tok(c); /* consume '(' */
            ASTNode* call = alloc_ast(c, AST_CALL);
            if (call) {
                strncpy(call->data.call.name, n->data.str_val.str,
                        VALUE_STRING_LEN - 1);
                call->data.call.num_args = 0; call->line = tok.line;
                if (!chk_tok(c, TOK_RPAREN)) {
                    ASTNode* args[16]; call->data.call.num_args = 0;
                    do {
                        if (call->data.call.num_args < 16)
                            args[call->data.call.num_args] = parse_expr(c);
                        call->data.call.num_args++;
                    } while (mat_tok(c, TOK_COMMA) && call->data.call.num_args < 16);
                    call->data.call.args = (ASTNode**)calloc(
                        call->data.call.num_args, sizeof(ASTNode*));
                    for (int i = 0; i < call->data.call.num_args; i++)
                        call->data.call.args[i] = args[i];
                }
                cnsm_tok(c, TOK_RPAREN, "expected ')' after args");
                return call;
            }
        }
        return n;
    }
    if (mat_tok(c, TOK_LPAREN)) {
        ASTNode* e = parse_expr(c);
        cnsm_tok(c, TOK_RPAREN, "expected ')'");
        return e;
    }
    compiler_error(c, tok.line, "expected expression, got '%s'",
                   token_type_name(tok.type));
    return NULL;
}

/* --- Unary expressions: -expr, !expr --- */
static ASTNode* parse_unary(Compiler* c) {
    Token tok = cur_tok(c);
    if (mat_tok(c, TOK_MINUS) || mat_tok(c, TOK_BANG)) {
        ASTNode* n = alloc_ast(c, AST_UNARY);
        if (n) {
            n->data.unary.op = (tok.type == TOK_MINUS) ? UNOP_NEG : UNOP_NOT;
            n->data.unary.operand = parse_unary(c); n->line = tok.line;
        }
        return n;
    }
    return parse_primary(c);
}

/* --- Pratt expression parser: precedence climbing (L5) --- */
static ASTNode* parse_precedence(Compiler* c, int min_prec) {
    ASTNode* left = parse_unary(c);
    if (!left) return NULL;
    while (true) {
        Token op_tok = cur_tok(c);
        int p = prec_of(op_tok.type);
        if (p < min_prec) break;
        TokenType op_type = op_tok.type; adv_tok(c);
        ASTNode* right = parse_precedence(c, p + 1);
        if (!right) return left;
        ASTNode* bin = alloc_ast(c, AST_BINARY);
        if (bin) {
            bin->data.binary.op = tok2bin(op_type);
            bin->data.binary.left = left;
            bin->data.binary.right = right;
            bin->line = op_tok.line;
        }
        left = bin;
    }
    return left;
}
static ASTNode* parse_expr(Compiler* c) { return parse_precedence(c, 0); }

/* --- Statement parsers --- */
static ASTNode* parse_let_stmt(Compiler* c) {
    Token nm = cur_tok(c);
    if (!cnsm_tok(c, TOK_IDENT, "expected name after 'let'")) return NULL;
    ASTNode* n = alloc_ast(c, AST_LET);
    if (!n) return NULL;
    strncpy(n->data.let_stmt.name, nm.start, nm.length);
    n->data.let_stmt.name[nm.length] = '\0'; n->line = nm.line;
    cnsm_tok(c, TOK_ASSIGN, "expected '=' in let");
    n->data.let_stmt.value = parse_expr(c);
    cnsm_tok(c, TOK_SEMICOLON, "expected ';' after let");
    return n;
}
static ASTNode* parse_if_stmt(Compiler* c) {
    Token ift = cur_tok(c);
    cnsm_tok(c, TOK_LPAREN, "expected '(' after 'if'");
    ASTNode* n = alloc_ast(c, AST_IF);
    if (!n) return NULL;
    n->line = ift.line;
    n->data.if_stmt.cond = parse_expr(c);
    cnsm_tok(c, TOK_RPAREN, "expected ')' after condition");
    n->data.if_stmt.then_body = parse_stmt(c);
    n->data.if_stmt.else_body = mat_tok(c, TOK_ELSE) ? parse_stmt(c) : NULL;
    return n;
}
static ASTNode* parse_while_stmt(Compiler* c) {
    cnsm_tok(c, TOK_LPAREN, "expected '(' after 'while'");
    ASTNode* n = alloc_ast(c, AST_WHILE);
    if (!n) return NULL;
    n->data.while_stmt.cond = parse_expr(c);
    cnsm_tok(c, TOK_RPAREN, "expected ')' after condition");
    n->data.while_stmt.body = parse_stmt(c);
    return n;
}
static ASTNode* parse_block_stmt(Compiler* c) {
    ASTNode* n = alloc_ast(c, AST_BLOCK);
    if (!n) return NULL;
    ASTNode* stmts[128]; int cnt = 0;
    while (!chk_tok(c, TOK_RBRACE) && !chk_tok(c, TOK_EOF) && cnt < 128)
        stmts[cnt++] = parse_stmt(c);
    cnsm_tok(c, TOK_RBRACE, "expected '}'");
    n->data.block.count = cnt;
    n->data.block.stmts = (ASTNode**)calloc(cnt ? cnt : 1, sizeof(ASTNode*));
    for (int i = 0; i < cnt; i++) n->data.block.stmts[i] = stmts[i];
    return n;
}
static ASTNode* parse_print_stmt(Compiler* c) {
    ASTNode* n = alloc_ast(c, AST_PRINT);
    if (!n) return NULL;
    n->data.print_stmt.expr = parse_expr(c);
    cnsm_tok(c, TOK_SEMICOLON, "expected ';' after print");
    return n;
}
static ASTNode* parse_return_stmt(Compiler* c) {
    ASTNode* n = alloc_ast(c, AST_RETURN);
    if (!n) return NULL;
    n->data.return_stmt.expr = chk_tok(c, TOK_SEMICOLON) ? NULL : parse_expr(c);
    cnsm_tok(c, TOK_SEMICOLON, "expected ';' after return");
    return n;
}
static ASTNode* parse_fn_def(Compiler* c) {
    Token nm = cur_tok(c);
    if (!cnsm_tok(c, TOK_IDENT, "expected function name")) return NULL;
    ASTNode* n = alloc_ast(c, AST_FUNC_DEF);
    if (!n) return NULL;
    strncpy(n->data.func_def.name, nm.start, nm.length);
    n->data.func_def.name[nm.length] = '\0'; n->line = nm.line;
    n->data.func_def.num_params = 0;
    cnsm_tok(c, TOK_LPAREN, "expected '('");
    if (!chk_tok(c, TOK_RPAREN)) {
        do {
            Token p = cur_tok(c);
            if (p.type == TOK_IDENT && n->data.func_def.num_params < 8) {
                adv_tok(c);
                strncpy(n->data.func_def.params[n->data.func_def.num_params],
                        p.start, p.length);
                n->data.func_def.params[n->data.func_def.num_params][p.length]='\0';
                n->data.func_def.num_params++;
            }
        } while (mat_tok(c, TOK_COMMA));
    }
    cnsm_tok(c, TOK_RPAREN, "expected ')'");
    n->data.func_def.body = parse_stmt(c);
    return n;
}
static ASTNode* parse_expr_stmt(Compiler* c) {
    ASTNode* n = alloc_ast(c, AST_EXPR_STMT);
    if (!n) return NULL;
    n->data.expr_stmt.expr = parse_expr(c);
    cnsm_tok(c, TOK_SEMICOLON, "expected ';'");
    return n;
}
/* Statement dispatcher */
static ASTNode* parse_stmt(Compiler* c) {
    if (mat_tok(c, TOK_LET))    return parse_let_stmt(c);
    if (mat_tok(c, TOK_IF))     return parse_if_stmt(c);
    if (mat_tok(c, TOK_WHILE))  return parse_while_stmt(c);
    if (mat_tok(c, TOK_LBRACE)) return parse_block_stmt(c);
    if (mat_tok(c, TOK_PRINT))  return parse_print_stmt(c);
    if (mat_tok(c, TOK_RETURN)) return parse_return_stmt(c);
    if (mat_tok(c, TOK_FN))     return parse_fn_def(c);
    /* L5: Assignment statement: IDENT = expr ; (2-token lookahead) */
    if (chk_tok(c, TOK_IDENT) &&
        c->token_pos + 1 < c->token_count &&
        c->tokens[c->token_pos + 1].type == TOK_ASSIGN) {
        Token id = adv_tok(c); /* consume IDENT */
        adv_tok(c); /* consume = */
        ASTNode* n = alloc_ast(c, AST_ASSIGN);
        if (n) {
            strncpy(n->data.assign.name, id.start, id.length);
            n->data.assign.name[id.length] = '\0';
            n->line = id.line;
            n->data.assign.value = parse_expr(c);
        }
        cnsm_tok(c, TOK_SEMICOLON, "expected ';' after assignment");
        return n;
    }
    return parse_expr_stmt(c);
}

/**
 * L6: compiler_parse_program — Builds AST from token stream.
 * Returns a synthetic AST_BLOCK wrapping all top-level statements.
 * Time: O(n) over tokens. Space: O(n) AST nodes.
 */
ASTNode* compiler_parse_program(Compiler* c) {
    c->token_pos = 0; c->node_count = 0;
    ASTNode* prog = alloc_ast(c, AST_BLOCK);
    if (!prog) return NULL;
    ASTNode* stmts[256]; int cnt = 0;
    while (!chk_tok(c, TOK_EOF) && !c->had_error && cnt < 256)
        stmts[cnt++] = parse_stmt(c);
    prog->data.block.count = cnt;
    prog->data.block.stmts = (ASTNode**)calloc(cnt ? cnt : 1, sizeof(ASTNode*));
    for (int i = 0; i < cnt; i++) prog->data.block.stmts[i] = stmts[i];
    return prog;
}


/* =========================================================================
 * Code Generator — AST → ByteCode (L3, L6)
 *
 * Post-order tree walk emitting stack-machine bytecodes.
 * Variable resolution via linear-scan symbol table.
 *
 * O(n) over AST nodes. Each node emits 1-5 bytecode instructions.
 * ========================================================================= */

static int32_t emit_push_int_c(Compiler* c, int64_t v) {
    Constant cnst; cnst.type = CONST_INT; cnst.data.int_val = v;
    int32_t idx = bc_add_constant(c->bytecode, cnst);
    return bc_emit(c->bytecode, (idx << 8) | OP_PUSH);
}
/* Symbol table: linear scan O(s) per lookup (s = symbol_count) */
static int32_t sym_find_or_add(Compiler* c, const char* name) {
    for (int32_t i = 0; i < c->symbol_count; i++)
        if (!strcmp(c->symbols[i].name, name)) return i;
    if (c->symbol_count >= CPL_MAX_SYMBOLS) return -1;
    int32_t idx = c->symbol_count++;
    strncpy(c->symbols[idx].name, name, VALUE_STRING_LEN-1);
    c->symbols[idx].name[VALUE_STRING_LEN-1] = '\0';
    c->symbols[idx].slot = idx; c->symbols[idx].depth = c->scope_depth;
    c->symbols[idx].is_mutable = true; c->symbols[idx].is_function = false;
    return idx;
}
static int32_t sym_lookup(const Compiler* c, const char* name) {
    for (int32_t i = 0; i < c->symbol_count; i++)
        if (!strcmp(c->symbols[i].name, name)) return i;
    return -1;
}

/* Forward */
static bool cg_node(Compiler* c, const ASTNode* n);

/* Binary: post-order emit left, right, then operator */
static bool cg_binary(Compiler* c, const ASTNode* n) {
    if (!cg_node(c, n->data.binary.left)) return false;
    if (!cg_node(c, n->data.binary.right)) return false;
    switch (n->data.binary.op) {
    case BINOP_ADD: bc_emit(c->bytecode, (0<<8)|OP_ADD); return true;
    case BINOP_SUB: bc_emit(c->bytecode, (0<<8)|OP_SUB); return true;
    case BINOP_MUL: bc_emit(c->bytecode, (0<<8)|OP_MUL); return true;
    case BINOP_DIV: bc_emit(c->bytecode, (0<<8)|OP_DIV); return true;
    case BINOP_AND: bc_emit(c->bytecode, (0<<8)|OP_AND); return true;
    case BINOP_OR:  bc_emit(c->bytecode, (0<<8)|OP_OR);  return true;
    /* Comparisons: a==b → !(a-b); a!=b → !!(a-b); etc. */
    case BINOP_EQ: bc_emit(c->bytecode, (0<<8)|OP_SUB);
                   bc_emit(c->bytecode, (0<<8)|OP_NOT); return true;
    case BINOP_NE: bc_emit(c->bytecode, (0<<8)|OP_SUB);
                   bc_emit(c->bytecode, (0<<8)|OP_NOT);
                   bc_emit(c->bytecode, (0<<8)|OP_NOT); return true;
    case BINOP_LT: /* a<b: a-b, push 0, sub (a-b-0), not (a<b iff a-b<0) */
                   bc_emit(c->bytecode, (0<<8)|OP_SUB);
                   emit_push_int_c(c, 0);
                   bc_emit(c->bytecode, (0<<8)|OP_SUB);
                   /* if a<b then result<0, integer sign bit */
                   bc_emit(c->bytecode, (0<<8)|OP_NOT);
                   bc_emit(c->bytecode, (0<<8)|OP_NOT); return true;
    case BINOP_GT: /* a>b: a-b, push 0, sub, not */
                   bc_emit(c->bytecode, (0<<8)|OP_SUB);
                   emit_push_int_c(c, 0); bc_emit(c->bytecode, (0<<8)|OP_SUB);
                   bc_emit(c->bytecode, (0<<8)|OP_NOT); return true;
    case BINOP_LE: bc_emit(c->bytecode, (0<<8)|OP_SUB);
                   emit_push_int_c(c, 0); bc_emit(c->bytecode, (0<<8)|OP_SUB);
                   bc_emit(c->bytecode, (0<<8)|OP_NOT);
                   bc_emit(c->bytecode, (0<<8)|OP_NOT); return true;
    case BINOP_GE: bc_emit(c->bytecode, (0<<8)|OP_SUB);
                   emit_push_int_c(c, 0); bc_emit(c->bytecode, (0<<8)|OP_SUB);
                   return true;
    default: return true;
    }
}
/* Unary */
static bool cg_unary(Compiler* c, const ASTNode* n) {
    if (!cg_node(c, n->data.unary.operand)) return false;
    OpCode op = (n->data.unary.op == UNOP_NEG) ? OP_NEG : OP_NOT;
    bc_emit(c->bytecode, (0<<8)|op); return true;
}
/* Let: evaluate value, store to slot */
static bool cg_let(Compiler* c, const ASTNode* n) {
    if (!cg_node(c, n->data.let_stmt.value)) return false;
    int32_t s = sym_find_or_add(c, n->data.let_stmt.name);
    if (s < 0) { compiler_error(c, n->line, "too many symbols"); return false; }
    bc_emit(c->bytecode, (s<<8)|OP_STORE); return true;
}
/* Assign */
static bool cg_assign(Compiler* c, const ASTNode* n) {
    int32_t s = sym_lookup(c, n->data.assign.name);
    if (s < 0) {
        compiler_error(c, n->line, "undefined '%s'", n->data.assign.name);
        return false;
    }
    if (!cg_node(c, n->data.assign.value)) return false;
    bc_emit(c->bytecode, (s<<8)|OP_STORE); return true;
}
/* Ident load */
static bool cg_ident(Compiler* c, const ASTNode* n) {
    int32_t s = sym_lookup(c, n->data.str_val.str);
    if (s < 0) {
        compiler_error(c, n->line, "undefined '%s'", n->data.str_val.str);
        return false;
    }
    bc_emit(c->bytecode, (s<<8)|OP_LOAD); return true;
}
/* Print */
static bool cg_print(Compiler* c, const ASTNode* n) {
    if (!cg_node(c, n->data.print_stmt.expr)) return false;
    bc_emit(c->bytecode, (0<<8)|OP_PRINT); return true;
}

/* If: cond, JMP_IF_FALSE→else/end, then, JMP→end, else, end */
static bool cg_if(Compiler* c, const ASTNode* n) {
    if (!cg_node(c, n->data.if_stmt.cond)) return false;
    int32_t jf_idx = bc_emit(c->bytecode, (0<<8)|OP_JMP_IF_FALSE);
    if (!cg_node(c, n->data.if_stmt.then_body)) return false;
    int32_t jmp_idx = bc_emit(c->bytecode, (0<<8)|OP_JMP);
    int32_t else_start = c->bytecode->num_inst;
    c->bytecode->instructions[jf_idx] = (else_start<<8)|OP_JMP_IF_FALSE;
    if (n->data.if_stmt.else_body)
        if (!cg_node(c, n->data.if_stmt.else_body)) return false;
    int32_t end = c->bytecode->num_inst;
    c->bytecode->instructions[jmp_idx] = (end<<8)|OP_JMP;
    return true;
}
/* While: loop_start, cond, JMP_IF_FALSE→after, body, JMP→loop_start, after */
static bool cg_while(Compiler* c, const ASTNode* n) {
    int32_t loop = c->bytecode->num_inst;
    if (!cg_node(c, n->data.while_stmt.cond)) return false;
    int32_t jf_idx = bc_emit(c->bytecode, (0<<8)|OP_JMP_IF_FALSE);
    if (!cg_node(c, n->data.while_stmt.body)) return false;
    bc_emit(c->bytecode, (loop<<8)|OP_JMP);
    int32_t after = c->bytecode->num_inst;
    c->bytecode->instructions[jf_idx] = (after<<8)|OP_JMP_IF_FALSE;
    return true;
}
/* Block: sequential emit */
static bool cg_block(Compiler* c, const ASTNode* n) {
    for (int32_t i = 0; i < n->data.block.count; i++)
        if (!cg_node(c, n->data.block.stmts[i])) return false;
    return true;
}
/* Expr statement: emit, then pop (discard result) */
static bool cg_expr_stmt(Compiler* c, const ASTNode* n) {
    if (n->data.expr_stmt.expr && !cg_node(c, n->data.expr_stmt.expr))
        return false;
    bc_emit(c->bytecode, (0<<8)|OP_POP); return true;
}
/* Return */
static bool cg_return(Compiler* c, const ASTNode* n) {
    if (n->data.return_stmt.expr)
        if (!cg_node(c, n->data.return_stmt.expr)) return false;
    bc_emit(c->bytecode, (0<<8)|OP_RET); return true;
}
/* Call: push args, then CALL with argc as arg */
static bool cg_call(Compiler* c, const ASTNode* n) {
    for (int32_t i = 0; i < n->data.call.num_args; i++)
        if (!cg_node(c, n->data.call.args[i])) return false;
    bc_emit(c->bytecode, (n->data.call.num_args<<8)|OP_CALL);
    return true;
}
/* Function def: emit body + RET */
static bool cg_fn_def(Compiler* c, const ASTNode* n) {
    int32_t fn_start = c->bytecode->num_inst;
    if (n->data.func_def.body)
        cg_node(c, n->data.func_def.body);
    bc_emit(c->bytecode, (0<<8)|OP_RET);
    (void)fn_start; return true;
}

/* Master AST→bytecode dispatch */
static bool cg_node(Compiler* c, const ASTNode* n) {
    if (!n || c->had_error) return false;
    switch (n->type) {
    case AST_INT_LIT:    emit_push_int_c(c, n->data.int_val); return true;
    case AST_FLOAT_LIT:  emit_push_int_c(c, (int64_t)n->data.float_val); return true;
    case AST_BOOL_LIT:   emit_push_int_c(c, n->data.bool_val?1:0); return true;
    case AST_NIL_LIT:    emit_push_int_c(c, 0); return true;
    case AST_STRING_LIT: {
        Constant sc; sc.type = CONST_STRING;
        sc.data.str_val = (char*)n->data.str_val.str;
        int32_t idx = bc_add_constant(c->bytecode, sc);
        bc_emit(c->bytecode, (idx<<8)|OP_PUSH); return true;
    }
    case AST_IDENT:    return cg_ident(c, n);
    case AST_BINARY:   return cg_binary(c, n);
    case AST_UNARY:    return cg_unary(c, n);
    case AST_LET:      return cg_let(c, n);
    case AST_ASSIGN:   return cg_assign(c, n);
    case AST_PRINT:    return cg_print(c, n);
    case AST_IF:       return cg_if(c, n);
    case AST_WHILE:    return cg_while(c, n);
    case AST_BLOCK:    return cg_block(c, n);
    case AST_EXPR_STMT:return cg_expr_stmt(c, n);
    case AST_RETURN:   return cg_return(c, n);
    case AST_CALL:     return cg_call(c, n);
    case AST_FUNC_DEF: return cg_fn_def(c, n);
    default: compiler_error(c, n->line, "unknown AST type %d", n->type);
             return false;
    }
}

/* =========================================================================
 * Compiler Driver (L3 pipeline orchestration)
 * ========================================================================= */

void compiler_init(Compiler* c, const char* source, ByteCode* output) {
    memset(c, 0, sizeof(Compiler));
    c->source = source; c->source_len = (int32_t)strlen(source);
    c->bytecode = output; c->scope_depth = 0;
    memset(output, 0, sizeof(ByteCode));
}

/**
 * L6: compiler_compile — Full 3-stage compilation pipeline.
 *   Stage 1: lex() — O(n) lexical analysis
 *   Stage 2: parse() — O(n) syntactic analysis
 *   Stage 3: codegen() — O(n) bytecode emission
 * Appends HALT sentinel. Returns true on success.
 */
bool compiler_compile(Compiler* c) {
    compiler_lex(c);
    if (c->had_error) return false;
    ASTNode* prog = compiler_parse_program(c);
    if (c->had_error || !prog) return false;
    if (!compiler_codegen(c, prog)) return false;
    if (c->had_error) return false;
    bc_emit(c->bytecode, (0<<8)|OP_HALT);
    return true;
}
bool compiler_codegen(Compiler* c, const ASTNode* prog) {
    return cg_node(c, prog);
}

/* =========================================================================
 * AST Pretty Printer (L7 debugging tool)
 * ========================================================================= */
static void pindent(int n) { for (int i=0;i<n;i++) printf("  "); }

void compiler_print_ast(const ASTNode* n, int indent) {
    if (!n) { printf("(null)\n"); return; }
    pindent(indent);
    switch (n->type) {
    case AST_INT_LIT: printf("INT(%lld)\n", (long long)n->data.int_val); break;
    case AST_FLOAT_LIT: printf("FLOAT(%g)\n", n->data.float_val); break;
    case AST_STRING_LIT: printf("STRING(\"%s\")\n", n->data.str_val.str); break;
    case AST_BOOL_LIT: printf("BOOL(%s)\n", n->data.bool_val?"true":"false"); break;
    case AST_NIL_LIT: printf("NIL\n"); break;
    case AST_IDENT: printf("IDENT(%s)\n", n->data.str_val.str); break;
    case AST_BINARY:
        printf("BINARY(%s)\n", binop_name(n->data.binary.op));
        compiler_print_ast(n->data.binary.left, indent+1);
        compiler_print_ast(n->data.binary.right, indent+1); break;
    case AST_UNARY:
        printf("UNARY(%s)\n", n->data.unary.op==UNOP_NEG?"-":"!");
        compiler_print_ast(n->data.unary.operand, indent+1); break;
    case AST_LET:
        printf("LET(%s)\n", n->data.let_stmt.name);
        compiler_print_ast(n->data.let_stmt.value, indent+1); break;
    case AST_PRINT:
        printf("PRINT\n");
        compiler_print_ast(n->data.print_stmt.expr, indent+1); break;
    case AST_IF:
        printf("IF\n"); compiler_print_ast(n->data.if_stmt.cond, indent+1);
        pindent(indent+1); printf("THEN:\n");
        compiler_print_ast(n->data.if_stmt.then_body, indent+2);
        if (n->data.if_stmt.else_body) {
            pindent(indent+1); printf("ELSE:\n");
            compiler_print_ast(n->data.if_stmt.else_body, indent+2);
        } break;
    case AST_WHILE:
        printf("WHILE\n");
        compiler_print_ast(n->data.while_stmt.cond, indent+1);
        compiler_print_ast(n->data.while_stmt.body, indent+1); break;
    case AST_BLOCK:
        printf("BLOCK[%d]\n", n->data.block.count);
        for (int i=0; i<n->data.block.count; i++)
            compiler_print_ast(n->data.block.stmts[i], indent+1); break;
    case AST_FUNC_DEF:
        printf("FUNC(%s,%d)\n", n->data.func_def.name, n->data.func_def.num_params);
        compiler_print_ast(n->data.func_def.body, indent+1); break;
    case AST_CALL:
        printf("CALL(%s,%d)\n", n->data.call.name, n->data.call.num_args);
        for (int i=0; i<n->data.call.num_args; i++)
            compiler_print_ast(n->data.call.args[i], indent+1); break;
    case AST_RETURN:
        printf("RETURN\n");
        if (n->data.return_stmt.expr)
            compiler_print_ast(n->data.return_stmt.expr, indent+1); break;
    default: printf("UNKNOWN(%d)\n", n->type); break;
    }
}

/* =========================================================================
 * Applications (L7) — REPL + Expression Evaluator
 * ========================================================================= */

/** L7: compiler_repl_interactive — Read-Eval-Print Loop */
void compiler_repl_interactive(void) {
    printf("=== Mini JIT VM REPL ===\n");
    printf("Enter expressions/statements. Empty line to quit.\n");
    printf("Supports: let x=expr; print expr; if/while; arithmetic\n\n");
    char buf[1024];
    while (true) {
        printf("> "); fflush(stdout);
        if (!fgets(buf, sizeof(buf), stdin)) break;
        if (buf[0]=='\n'||buf[0]=='\0') break;
        ByteCode bc; Compiler comp;
        compiler_init(&comp, buf, &bc);
        if (!compiler_compile(&comp)) {
            printf("Error: %s\n", comp.error_msg); continue;
        }
        StackVM vm; vm_init(&vm, &bc);
        vm_execute(&vm);
    }
    printf("Goodbye.\n");
}

/** L7: compiler_eval_expression — One-shot expression evaluator.
 *  Embeds the full compile+execute pipeline.
 *
 *  Complexity: O(n) for source of length n (lex+parse+codegen+VmExecute).
 *  Returns int64 result, 0 on error.
 */
int64_t compiler_eval_expression(const char* source) {
    char buf[CPL_MAX_SOURCE_LEN];
    /* Store expression result in local slot 0, then read it after execution */
    snprintf(buf, sizeof(buf), "let _eval_tmp = (%s);", source);
    ByteCode bc; Compiler comp;
    compiler_init(&comp, buf, &bc);
    if (!compiler_compile(&comp)) return 0;
    StackVM vm; vm_init(&vm, &bc);
    vm_execute(&vm);
    return vm.locals[0]; /* result stored in slot 0 by 'let' */
}
