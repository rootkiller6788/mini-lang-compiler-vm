#ifndef C_SUBSET_H
#define C_SUBSET_H

#include <stdbool.h>
#include <stddef.h>

/* ── L1: Core Type System ──────────────────────────────────────────
 * C-like static type system: int, float, char, pointer, array, struct.
 * Reference: C99 standard (ISO/IEC 9899:1999) §6.2.5 Types.
 */
typedef enum {
    T_INT,
    T_FLOAT,
    T_CHAR,
    T_PTR,
    T_ARRAY,
    T_STRUCT,
    T_VOID,
    T_BOOL
} CType;

/* ── L1: Variable (environment entry) ───────────────────────────── */
typedef struct CVar {
    char name[64];
    CType type;
    union {
        int int_val;
        float float_val;
        char char_val;
        void *ptr_val;
        bool bool_val;
    } value;
    int array_size;
    /* For array variables: allocated int array */
    int *array_data;
    struct CVar *next;
} CVar;

/* ── L1: AST Node Types ───────────────────────────────────────────
 * Extended with comparison/logical operators, for-loop, break/continue.
 */
typedef enum {
    NODE_INT,
    NODE_FLOAT,
    NODE_BOOL,
    NODE_CHAR,
    NODE_VAR,
    NODE_BINOP,       /* + - * / % */
    NODE_COMPARE,     /* == != < > <= >= */
    NODE_LOGICAL,     /* && || */
    NODE_UNARY,       /* ! - */
    NODE_CALL,
    NODE_IF,
    NODE_WHILE,
    NODE_FOR,         /* for (init; cond; incr) body */
    NODE_BREAK,       /* break statement */
    NODE_CONTINUE,    /* continue statement */
    NODE_BLOCK,
    NODE_ASSIGN,
    NODE_RETURN,
    NODE_INDEX,
    NODE_MEMBER,
    NODE_FUNC_DEF,
    NODE_ARRAY_LITERAL  /* {1, 2, 3} */
} ASTNodeType;

/* ── L3: AST Node (Tagged Union) ────────────────────────────────── */
typedef struct ASTNode {
    ASTNodeType type;
    union {
        int int_val;
        float float_val;
        bool bool_val;
        char name[64];
        struct {
            struct ASTNode *left;
            char op;            /* + - * / % == != < > L G & | */
            struct ASTNode *right;
        } binop;
        struct {
            char name[64];
            struct ASTNode **args;
            int arg_count;
        } call;
        struct {
            struct ASTNode *cond;
            struct ASTNode *then_branch;
            struct ASTNode *else_branch;
        } if_stmt;
        struct {
            struct ASTNode *cond;
            struct ASTNode *body;
        } while_stmt;
        struct {
            struct ASTNode *init;
            struct ASTNode *cond;
            struct ASTNode *incr;
            struct ASTNode *body;
        } for_stmt;
        struct {
            struct ASTNode **stmts;
            int count;
        } block;
        struct {
            char name[64];
            struct ASTNode *expr;
            struct ASTNode *index; /* for a[i] = expr */
        } assign;
        struct ASTNode *ret_expr;
        struct {
            struct ASTNode *arr;
            struct ASTNode *idx;
        } index_expr;
        struct {
            struct ASTNode *obj;
            char field[64];
        } member;
        struct ASTNode *unary_expr;  /* for NODE_UNARY */
        struct {
            struct ASTNode **elements;
            int count;
        } array_lit;
    } data;
} ASTNode;

/* ── L3: Function definition ───────────────────────────────────── */
typedef struct CFunc {
    char name[64];
    CType return_type;
    char **param_names;
    CType *param_types;
    int param_count;
    ASTNode *body;
    struct CFunc *next;
} CFunc;

/* ── L3: Program (global scope) ─────────────────────────────────── */
typedef struct {
    CVar *globals;
    CFunc *functions;
} CProgram;

/* ── API Declarations ────────────────────────────────────────────── */

/* L1: Program lifecycle */
void       c_init_program(CProgram *prog);

/* L2: Environment (linked-list scope) */
CVar      *c_find_var(CVar *head, const char *name);
CFunc     *c_find_func(CFunc *head, const char *name);

/* L5: Parser */
ASTNode   *c_parse_declaration(const char *source, int *pos);
ASTNode   *c_parse_function(const char *source, int *pos);
ASTNode   *c_parse_block(const char *source, int *pos);
ASTNode   *c_parse_expr(const char *source, int *pos);
ASTNode   *c_parse_statement(const char *source, int *pos);

/* L5: Evaluator */
int        c_eval_expr(ASTNode *node, CVar *env, CFunc *funcs);
void       c_execute_statement(ASTNode *node, CVar **env, CFunc *funcs);
void       c_execute_block(ASTNode *node, CVar **env, CFunc *funcs);

/* L4: Type checking */
bool       c_type_check(ASTNode *node, CVar *env, CFunc *funcs);

/* L4: Constant folding optimization */
ASTNode   *c_constant_fold(ASTNode *node);

/* L7: Memory tracking */
void       c_print_env(CVar *env);
int        c_env_size(CVar *env);

/* Cleanup */
void       c_free_ast(ASTNode *node);
void       c_free_var_list(CVar *head);
void       c_free_func_list(CFunc *head);

/* L8: Bytecode compiler (simple stack VM) */
typedef enum {
    BC_PUSH_INT, BC_PUSH_VAR, BC_STORE_VAR,
    BC_ADD, BC_SUB, BC_MUL, BC_DIV, BC_MOD,
    BC_CMP_EQ, BC_CMP_NE, BC_CMP_LT, BC_CMP_LE, BC_CMP_GT, BC_CMP_GE,
    BC_JMP, BC_JMP_IF_FALSE, BC_CALL, BC_RET, BC_HALT
} BytecodeOp;

typedef struct {
    int *code;
    int len;
    int cap;
} Bytecode;

Bytecode   *c_compile_ast(ASTNode *node);
int         c_execute_bytecode(Bytecode *bc, CVar *env);
void        c_free_bytecode(Bytecode *bc);

#endif
