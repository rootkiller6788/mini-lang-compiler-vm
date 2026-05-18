#include "c_subset.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static CFunc *make_fib_func(void) {
    CFunc *f = (CFunc *)calloc(1, sizeof(CFunc));
    strcpy(f->name, "fib");
    f->return_type = T_INT;
    f->param_count = 1;
    f->param_names = (char **)calloc(1, sizeof(char *));
    f->param_names[0] = strdup("n");
    f->param_types = (CType *)calloc(1, sizeof(CType));
    f->param_types[0] = T_INT;

    ASTNode *body = (ASTNode *)calloc(1, sizeof(ASTNode));
    body->type = NODE_BLOCK;
    body->data.block.count = 7;
    body->data.block.stmts = (ASTNode **)calloc(7, sizeof(ASTNode *));

    ASTNode *stmt0 = (ASTNode *)calloc(1, sizeof(ASTNode));
    stmt0->type = NODE_ASSIGN;
    strcpy(stmt0->data.assign.name, "a");
    stmt0->data.assign.expr = (ASTNode *)calloc(1, sizeof(ASTNode));
    stmt0->data.assign.expr->type = NODE_INT;
    stmt0->data.assign.expr->data.int_val = 0;

    ASTNode *stmt1 = (ASTNode *)calloc(1, sizeof(ASTNode));
    stmt1->type = NODE_ASSIGN;
    strcpy(stmt1->data.assign.name, "b");
    stmt1->data.assign.expr = (ASTNode *)calloc(1, sizeof(ASTNode));
    stmt1->data.assign.expr->type = NODE_INT;
    stmt1->data.assign.expr->data.int_val = 1;

    ASTNode *stmt2 = (ASTNode *)calloc(1, sizeof(ASTNode));
    stmt2->type = NODE_ASSIGN;
    strcpy(stmt2->data.assign.name, "i");
    stmt2->data.assign.expr = (ASTNode *)calloc(1, sizeof(ASTNode));
    stmt2->data.assign.expr->type = NODE_INT;
    stmt2->data.assign.expr->data.int_val = 0;

    ASTNode *while_stmt = (ASTNode *)calloc(1, sizeof(ASTNode));
    while_stmt->type = NODE_WHILE;

    ASTNode *cond = (ASTNode *)calloc(1, sizeof(ASTNode));
    cond->type = NODE_BINOP;
    cond->data.binop.op = '<';
    cond->data.binop.left = (ASTNode *)calloc(1, sizeof(ASTNode));
    cond->data.binop.left->type = NODE_VAR;
    strcpy(cond->data.binop.left->data.name, "i");
    cond->data.binop.right = (ASTNode *)calloc(1, sizeof(ASTNode));
    cond->data.binop.right->type = NODE_VAR;
    strcpy(cond->data.binop.right->data.name, "n");
    while_stmt->data.while_stmt.cond = cond;

    ASTNode *while_body = (ASTNode *)calloc(1, sizeof(ASTNode));
    while_body->type = NODE_BLOCK;
    while_body->data.block.count = 3;
    while_body->data.block.stmts = (ASTNode **)calloc(3, sizeof(ASTNode *));

    ASTNode *temp_assign = (ASTNode *)calloc(1, sizeof(ASTNode));
    temp_assign->type = NODE_ASSIGN;
    strcpy(temp_assign->data.assign.name, "temp");
    temp_assign->data.assign.expr = (ASTNode *)calloc(1, sizeof(ASTNode));
    temp_assign->data.assign.expr->type = NODE_BINOP;
    temp_assign->data.assign.expr->data.binop.op = '+';
    temp_assign->data.assign.expr->data.binop.left = (ASTNode *)calloc(1, sizeof(ASTNode));
    temp_assign->data.assign.expr->data.binop.left->type = NODE_VAR;
    strcpy(temp_assign->data.assign.expr->data.binop.left->data.name, "a");
    temp_assign->data.assign.expr->data.binop.right = (ASTNode *)calloc(1, sizeof(ASTNode));
    temp_assign->data.assign.expr->data.binop.right->type = NODE_VAR;
    strcpy(temp_assign->data.assign.expr->data.binop.right->data.name, "b");

    ASTNode *a_assign = (ASTNode *)calloc(1, sizeof(ASTNode));
    a_assign->type = NODE_ASSIGN;
    strcpy(a_assign->data.assign.name, "a");
    a_assign->data.assign.expr = (ASTNode *)calloc(1, sizeof(ASTNode));
    a_assign->data.assign.expr->type = NODE_VAR;
    strcpy(a_assign->data.assign.expr->data.name, "b");

    ASTNode *b_assign = (ASTNode *)calloc(1, sizeof(ASTNode));
    b_assign->type = NODE_ASSIGN;
    strcpy(b_assign->data.assign.name, "b");
    b_assign->data.assign.expr = (ASTNode *)calloc(1, sizeof(ASTNode));
    b_assign->data.assign.expr->type = NODE_VAR;
    strcpy(b_assign->data.assign.expr->data.name, "temp");

    while_body->data.block.stmts[0] = temp_assign;
    while_body->data.block.stmts[1] = a_assign;
    while_body->data.block.stmts[2] = b_assign;

    ASTNode *inc_stmt = (ASTNode *)calloc(1, sizeof(ASTNode));
    inc_stmt->type = NODE_ASSIGN;
    strcpy(inc_stmt->data.assign.name, "i");
    inc_stmt->data.assign.expr = (ASTNode *)calloc(1, sizeof(ASTNode));
    inc_stmt->data.assign.expr->type = NODE_BINOP;
    inc_stmt->data.assign.expr->data.binop.op = '+';
    inc_stmt->data.assign.expr->data.binop.left = (ASTNode *)calloc(1, sizeof(ASTNode));
    inc_stmt->data.assign.expr->data.binop.left->type = NODE_VAR;
    strcpy(inc_stmt->data.assign.expr->data.binop.left->data.name, "i");
    inc_stmt->data.assign.expr->data.binop.right = (ASTNode *)calloc(1, sizeof(ASTNode));
    inc_stmt->data.assign.expr->data.binop.right->type = NODE_INT;
    inc_stmt->data.assign.expr->data.binop.right->data.int_val = 1;

    while_stmt->data.while_stmt.body = while_body;

    ASTNode *stmt3 = inc_stmt;

    ASTNode *stmt4 = (ASTNode *)calloc(1, sizeof(ASTNode));
    stmt4->type = NODE_RETURN;
    stmt4->data.ret_expr = (ASTNode *)calloc(1, sizeof(ASTNode));
    stmt4->data.ret_expr->type = NODE_VAR;
    strcpy(stmt4->data.ret_expr->data.name, "a");

    body->data.block.stmts[0] = stmt0;
    body->data.block.stmts[1] = stmt1;
    body->data.block.stmts[2] = stmt2;
    body->data.block.stmts[3] = while_stmt;
    body->data.block.stmts[4] = stmt3;
    body->data.block.stmts[5] = stmt4;
    body->data.block.stmts[6] = NULL;

    f->body = body;
    return f;
}

int main(void) {
    CProgram prog;
    c_init_program(&prog);

    CFunc *fib = make_fib_func();
    fib->next = prog.functions;
    prog.functions = fib;

    printf("=== Mini C Subset Interpreter Demo ===\n");
    printf("Fibonacci function (iterative with while loop):\n\n");

    for (int n = 0; n <= 10; n++) {
        CVar *env = NULL;
        CVar *param = (CVar *)calloc(1, sizeof(CVar));
        strcpy(param->name, "n");
        param->type = T_INT;
        param->value.int_val = n;
        param->next = env;
        env = param;

        int result = 0;
        if (fib->body && fib->body->type == NODE_BLOCK) {
            for (int i = 0; i < fib->body->data.block.count && fib->body->data.block.stmts[i]; i++) {
                ASTNode *s = fib->body->data.block.stmts[i];
                if (s->type == NODE_RETURN)
                    result = c_eval_expr(s->data.ret_expr, env, prog.functions);
                else if (s->type == NODE_WHILE)
                    c_execute_statement(s, &env, prog.functions);
                else if (s->type == NODE_ASSIGN)
                    c_execute_statement(s, &env, prog.functions);
            }
        }

        printf("  fib(%d) = %d\n", n, result);
        c_free_var_list(env);
    }

    printf("\n=== Variable Environment Demo ===\n");
    CVar *env = NULL;
    CVar *x = (CVar *)calloc(1, sizeof(CVar));
    strcpy(x->name, "x"); x->type = T_INT; x->value.int_val = 42;
    x->next = env; env = x;
    CVar *y = (CVar *)calloc(1, sizeof(CVar));
    strcpy(y->name, "y"); y->type = T_INT; y->value.int_val = 100;
    y->next = env; env = y;
    c_print_env(env);
    printf("  x + y = %d\n",
           c_eval_expr(x->next ? NULL : NULL, env, prog.functions));

    CVar *result = c_find_var(env, "x");
    if (result) printf("  Found x = %d\n", result->value.int_val);

    c_free_var_list(env);
    c_free_func_list(prog.functions);

    return 0;
}
