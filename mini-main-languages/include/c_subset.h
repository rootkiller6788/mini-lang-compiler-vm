#ifndef C_SUBSET_H
#define C_SUBSET_H

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    T_INT,
    T_FLOAT,
    T_CHAR,
    T_PTR,
    T_ARRAY,
    T_STRUCT
} CType;

typedef struct {
    char name[64];
    CType type;
    union {
        int int_val;
        float float_val;
        char char_val;
        void *ptr_val;
    } value;
    int array_size;
    struct CVar *next;
} CVar;

typedef enum {
    NODE_INT,
    NODE_FLOAT,
    NODE_VAR,
    NODE_BINOP,
    NODE_CALL,
    NODE_IF,
    NODE_WHILE,
    NODE_BLOCK,
    NODE_ASSIGN,
    NODE_RETURN,
    NODE_INDEX,
    NODE_MEMBER
} ASTNodeType;

typedef struct ASTNode {
    ASTNodeType type;
    union {
        int int_val;
        float float_val;
        char name[64];
        struct {
            struct ASTNode *left;
            char op;
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
            struct ASTNode **stmts;
            int count;
        } block;
        struct {
            char name[64];
            struct ASTNode *expr;
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
    } data;
} ASTNode;

typedef struct {
    char name[64];
    CType return_type;
    char **param_names;
    CType *param_types;
    int param_count;
    ASTNode *body;
    struct CFunc *next;
} CFunc;

typedef struct {
    CVar *globals;
    CFunc *functions;
} CProgram;

void       c_init_program(CProgram *prog);
CVar      *c_find_var(CVar *head, const char *name);
CFunc     *c_find_func(CFunc *head, const char *name);
ASTNode   *c_parse_declaration(const char *source, int *pos);
ASTNode   *c_parse_function(const char *source, int *pos);
int        c_eval_expr(ASTNode *node, CVar *env, CFunc *funcs);
void       c_execute_statement(ASTNode *node, CVar **env, CFunc *funcs);
void       c_execute_block(ASTNode *node, CVar **env, CFunc *funcs);
void       c_print_env(CVar *env);
void       c_free_ast(ASTNode *node);
void       c_free_var_list(CVar *head);
void       c_free_func_list(CFunc *head);

#endif
