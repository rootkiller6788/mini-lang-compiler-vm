/* test_c_subset.c */
#include "c_subset.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name, expr) do { \
    tests_passed++; \
    if (!(expr)) { \
        printf("FAIL: %s (%s:%d)\n", name, __FILE__, __LINE__); \
        tests_failed++; tests_passed--; \
    } \
} while(0)

int main(void) {
    printf("\n=== C Subset Interpreter Tests ===\n");
    
    /* L1: Definitions */
    {
        CProgram prog;
        c_init_program(&prog);
        TEST("init null globals", prog.globals == NULL);
        TEST("init null funcs", prog.functions == NULL);
        
        ASTNode *n = (ASTNode *)calloc(1, sizeof(ASTNode));
        n->type = NODE_INT; n->data.int_val = 100;
        TEST("AST int", n->data.int_val == 100);
        c_free_ast(n);
    }
    
    /* L2: Environment */
    {
        CVar *env = NULL;
        CVar *x = (CVar *)calloc(1, sizeof(CVar));
        strcpy(x->name, "x"); x->type = T_INT; x->value.int_val = 10;
        x->next = env; env = x;
        TEST("find x", c_find_var(env, "x") != NULL);
        TEST("x=10", c_find_var(env, "x")->value.int_val == 10);
        TEST("no z", c_find_var(env, "z") == NULL);
        c_free_var_list(env);
    }
    
    /* L3: Parser */
    {
        int pos = 0;
        ASTNode *expr = c_parse_expr("42", &pos);
        TEST("parse 42", expr != NULL && expr->data.int_val == 42);
        c_free_ast(expr);
        
        pos = 0;
        expr = c_parse_expr("x", &pos);
        TEST("parse var", expr != NULL && expr->type == NODE_VAR);
        c_free_ast(expr);
        
        pos = 0;
        expr = c_parse_expr("1 + 2", &pos);
        TEST("parse +", expr != NULL);
        c_free_ast(expr);
    }
    
    /* L5: Evaluator */
    {
        CProgram prog; c_init_program(&prog);
        CVar *env = NULL;
        CVar *v = (CVar *)calloc(1, sizeof(CVar));
        strcpy(v->name, "x"); v->type = T_INT; v->value.int_val = 10;
        v->next = env; env = v;
        
        int pos = 0;
        ASTNode *expr = c_parse_expr("x + 5", &pos);
        int result = c_eval_expr(expr, env, prog.functions);
        TEST("x+5=15", result == 15);
        c_free_ast(expr);
        
        pos = 0; expr = c_parse_expr("x < 20", &pos);
        result = c_eval_expr(expr, env, prog.functions);
        TEST("10<20", result == 1);
        c_free_ast(expr);
        
        pos = 0; expr = c_parse_expr("1 && 1", &pos);
        result = c_eval_expr(expr, env, prog.functions);
        TEST("1&&1=1", result == 1);
        c_free_ast(expr);
        
        c_free_var_list(env);
        c_free_func_list(prog.functions);
    }
    
    /* L7: Bytecode */
    {
        int pos = 0;
        ASTNode *expr = c_parse_expr("2 + 3 * 4", &pos);
        Bytecode *bc = c_compile_ast(expr);
        TEST("bytecode", bc != NULL && bc->len > 0);
        int result = c_execute_bytecode(bc, NULL);
        TEST("bc:2+3*4=14", result == 14);
        c_free_bytecode(bc);
        c_free_ast(expr);
    }
    
    printf("\n=== Results: %d passed, %d failed ===\n\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
