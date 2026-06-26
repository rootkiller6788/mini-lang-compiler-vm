#ifndef COMPILER_H
#define COMPILER_H

/* ==========================================================================
 * mini-compiler.h — Source-to-Bytecode Compiler Frontend
 *
 * Covers knowledge layers:
 *   L1: TokenType/ASTNode/Compiler struct definitions
 *   L2: Lexical analysis, recursive-descent parsing, AST→bytecode
 *   L3: Compiler pipeline: source → tokens → AST → bytecode → VM
 *   L4: Chomsky hierarchy: regular languages (lexer), context-free (parser)
 *   L5: Recursive descent + operator-precedence parsing, shunting-yard
 *   L6: Complete expression & statement compiler (classic compiler problem)
 *   L7: Interactive REPL, expression evaluator
 *
 * References:
 *   - Crafting Interpreters, Nystrom (2015) Ch 4-8, 14-17
 *   - Compilers: Principles, Techniques, and Tools (Dragon Book) Ch 2-6
 *   - MIT 6.035 Computer Language Engineering
 *   - Stanford CS143 Compilers
 *   - CMU 15-411 Compiler Design
 * ========================================================================== */

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

#include "bytecode.h"
#include "closure_values.h"

/* --------------------------------------------------------------------------
 * L1: Core Definitions — Token types, AST node types, compiler state
 * -------------------------------------------------------------------------- */

#define CPL_MAX_TOKENS      256
#define CPL_MAX_AST_NODES   256
#define CPL_MAX_SYMBOLS      64
#define CPL_MAX_PARAMS       8
#define CPL_PARAM_NAME_LEN   32
#define CPL_MAX_SOURCE_LEN  4096

/** Token types — the regular-language output of the lexer stage */
typedef enum {
    TOK_EOF = 0,         /* end of input */
    TOK_INT_LIT,         /* integer literal: 42 */
    TOK_FLOAT_LIT,       /* float literal: 3.14 */
    TOK_STRING_LIT,      /* string literal: "hello" */
    TOK_IDENT,           /* identifier */
    /* reserved words */
    TOK_LET,             /* let */
    TOK_IF,              /* if */
    TOK_ELSE,            /* else */
    TOK_WHILE,           /* while */
    TOK_FN,              /* fn (function definition) */
    TOK_RETURN,          /* return */
    TOK_PRINT,           /* print */
    TOK_TRUE,            /* true */
    TOK_FALSE,           /* false */
    TOK_NIL,             /* nil */
    /* operators and punctuation */
    TOK_PLUS,            /* + */
    TOK_MINUS,           /* - */
    TOK_STAR,            /* * */
    TOK_SLASH,           /* / */
    TOK_BANG,            /* ! */
    TOK_EQ,              /* == */
    TOK_NE,              /* != */
    TOK_LT,              /* < */
    TOK_GT,              /* > */
    TOK_LE,              /* <= */
    TOK_GE,              /* >= */
    TOK_AND,             /* && */
    TOK_OR,              /* || */
    TOK_ASSIGN,          /* = */
    TOK_LPAREN,          /* ( */
    TOK_RPAREN,          /* ) */
    TOK_LBRACE,          /* { */
    TOK_RBRACE,          /* } */
    TOK_SEMICOLON,       /* ; */
    TOK_COMMA,           /* , */
    TOK_COUNT
} TokenType;

/** A single token produced by the lexer */
typedef struct {
    TokenType type;              /* token classification */
    int32_t   line;              /* source line (1-based) */
    int32_t   column;            /* source column (0-based) */
    int32_t   length;            /* lexeme length */
    const char* start;           /* pointer into source string */
    union {
        int64_t  int_val;        /* TOK_INT_LIT value */
        double   float_val;      /* TOK_FLOAT_LIT value */
    } value;
} Token;

/** AST node types — context-free grammar output of the parser stage */
typedef enum {
    AST_NONE = 0,
    AST_INT_LIT,         /* integer literal leaf */
    AST_FLOAT_LIT,       /* float literal leaf */
    AST_STRING_LIT,      /* string literal leaf */
    AST_BOOL_LIT,        /* boolean literal leaf */
    AST_NIL_LIT,         /* nil literal leaf */
    AST_IDENT,           /* variable reference leaf */
    AST_BINARY,          /* binary operation: a OP b */
    AST_UNARY,           /* unary operation: OP a */
    AST_LET,             /* let name = value */
    AST_ASSIGN,          /* name = value */
    AST_PRINT,           /* print expr */
    AST_IF,              /* if cond then body [else else_body] */
    AST_WHILE,           /* while cond body */
    AST_BLOCK,           /* { stmt* } */
    AST_CALL,            /* func(arg*) */
    AST_FUNC_DEF,        /* fn name(params) body */
    AST_RETURN,          /* return expr */
    AST_EXPR_STMT        /* bare expression statement */
} ASTNodeType;

/** Binary operator tags */
typedef enum {
    BINOP_ADD = 0,       /* + */
    BINOP_SUB,           /* - */
    BINOP_MUL,           /* * */
    BINOP_DIV,           /* / */
    BINOP_EQ,            /* == */
    BINOP_NE,            /* != */
    BINOP_LT,            /* < */
    BINOP_GT,            /* > */
    BINOP_LE,            /* <= */
    BINOP_GE,            /* >= */
    BINOP_AND,           /* && */
    BINOP_OR             /* || */
} BinaryOp;

/** Unary operator tags */
typedef enum {
    UNOP_NEG = 0,        /* - */
    UNOP_NOT             /* ! */
} UnaryOp;

/** AST node — the central IR between parser and code generator */
typedef struct ASTNode {
    ASTNodeType type;            /* node classification */
    int32_t     line;            /* source line for error reporting */
    union {
        int64_t  int_val;        /* AST_INT_LIT */
        double   float_val;      /* AST_FLOAT_LIT */
        bool     bool_val;       /* AST_BOOL_LIT */
        struct {
            char str[VALUE_STRING_LEN];
        } str_val;               /* AST_STRING_LIT, AST_IDENT */
        struct {
            BinaryOp op;
            struct ASTNode* left;
            struct ASTNode* right;
        } binary;                /* AST_BINARY */
        struct {
            UnaryOp op;
            struct ASTNode* operand;
        } unary;                 /* AST_UNARY */
        struct {
            char name[VALUE_STRING_LEN];
            struct ASTNode* value;
        } let_stmt;              /* AST_LET */
        struct {
            char name[VALUE_STRING_LEN];
            struct ASTNode* value;
        } assign;                /* AST_ASSIGN */
        struct {
            struct ASTNode* expr;
        } print_stmt;            /* AST_PRINT */
        struct {
            struct ASTNode* cond;
            struct ASTNode* then_body;
            struct ASTNode* else_body;
        } if_stmt;               /* AST_IF */
        struct {
            struct ASTNode* cond;
            struct ASTNode* body;
        } while_stmt;            /* AST_WHILE */
        struct {
            struct ASTNode** stmts;
            int32_t         count;
        } block;                 /* AST_BLOCK */
        struct {
            char name[CPL_PARAM_NAME_LEN];
            char params[CPL_MAX_PARAMS][CPL_PARAM_NAME_LEN];
            int32_t num_params;
            struct ASTNode* body;
        } func_def;              /* AST_FUNC_DEF */
        struct {
            char name[VALUE_STRING_LEN];
            struct ASTNode** args;
            int32_t         num_args;
        } call;                  /* AST_CALL */
        struct {
            struct ASTNode* expr;
        } return_stmt;           /* AST_RETURN */
        struct {
            struct ASTNode* expr;
        } expr_stmt;             /* AST_EXPR_STMT */
    } data;
} ASTNode;

/** Symbol table entry for variable resolution */
typedef struct {
    char    name[VALUE_STRING_LEN];
    int32_t slot;                /* stack slot index */
    int32_t depth;               /* lexical depth (0 = global) */
    bool    is_mutable;          /* can be reassigned */
    bool    is_function;         /* is a function definition */
} Symbol;

/** Compiler state — the main compilation driver */
typedef struct {
    const char* source;          /* source text */
    int32_t     source_len;      /* source length */
    /* lexer state */
    const char* current;         /* current char pointer */
    int32_t     line;            /* current line */
    int32_t     column;          /* current column */
    Token       tokens[CPL_MAX_TOKENS];
    int32_t     token_count;
    int32_t     token_pos;       /* current token index */
    /* AST arena */
    ASTNode     nodes[CPL_MAX_AST_NODES];
    int32_t     node_count;
    /* symbol table */
    Symbol      symbols[CPL_MAX_SYMBOLS];
    int32_t     symbol_count;
    int32_t     scope_depth;     /* current lexical depth */
    /* output */
    ByteCode*   bytecode;        /* target bytecode buffer */
    /* error reporting */
    bool        had_error;
    char        error_msg[256];
} Compiler;

/* --------------------------------------------------------------------------
 * L2/L5: API — Lexer, Parser, Code Generator
 * -------------------------------------------------------------------------- */

/* --- Compiler lifecycle --- */
void    compiler_init(Compiler* c, const char* source, ByteCode* output);
bool    compiler_compile(Compiler* c);

/* --- Lexer (O(n) over source, n = source_len) --- */
void    compiler_lex(Compiler* c);

/* --- Parser (recursive descent, O(n) over tokens) --- */
ASTNode* compiler_parse_program(Compiler* c);

/* --- Code generation (single-pass walk over AST, O(n)) --- */
bool    compiler_codegen(Compiler* c, const ASTNode* program);

/* --- Utilities --- */
const char* token_type_name(TokenType t);
const char* binop_name(BinaryOp op);
void        compiler_print_ast(const ASTNode* node, int indent);
void        compiler_error(Compiler* c, int32_t line, const char* fmt, ...);

/* --------------------------------------------------------------------------
 * L7: Applications — REPL
 * -------------------------------------------------------------------------- */

/** Interactive REPL */
void    compiler_repl_interactive(void);

/** Evaluate a single source expression, returning int64 result */
int64_t compiler_eval_expression(const char* source);

#endif /* COMPILER_H */
