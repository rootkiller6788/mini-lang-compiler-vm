#include "semantic.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void sem_checker_init(SemanticChecker *checker) {
    checker->symtab = NULL;
    checker->error_count = 0;
    checker->current_function = NULL;
}

void sem_report_error(SemanticChecker *checker, int line, int col, const char *msg) {
    fprintf(stderr, "semantic error at line %d col %d: %s\n", line, col, msg);
    checker->error_count++;
}

void sem_check_expr(SemanticChecker *checker, ASTNode *expr) {
    if (!expr) return;

    switch (expr->type) {
    case AST_INT_LIT:
    case AST_STRING_LIT:
        break;

    case AST_IDENT: {
        Symbol *sym = symtab_lookup(checker->symtab, expr->name);
        if (!sym) {
            char buf[256];
            snprintf(buf, sizeof(buf), "undeclared identifier '%s'", expr->name);
            sem_report_error(checker, expr->line, expr->col, buf);
        }
        break;
    }

    case AST_BINARY_OP: {
        ASTNode *left = ast_get_child(expr, 0);
        ASTNode *right = ast_get_child(expr, 1);
        sem_check_expr(checker, left);
        sem_check_expr(checker, right);

        if (expr->op == '=' || expr->op == '!' ||
            expr->op == '<' || expr->op == '>' ||
            expr->op == 'L' || expr->op == 'G' ||
            expr->op == '&' || expr->op == '|') {
            if (left && right) {
                if (left->type == AST_STRING_LIT || right->type == AST_STRING_LIT) {
                    sem_report_error(checker, expr->line, expr->col,
                                     "type mismatch: cannot compare strings with relational/equality operators");
                }
            }
        }

        if (expr->op == '|' || expr->op == '&') {
            if (left && right) {
                if (left->type == AST_STRING_LIT || right->type == AST_STRING_LIT) {
                    sem_report_error(checker, expr->line, expr->col,
                                     "type mismatch: logical operators require integer expressions");
                }
            }
        }
        break;
    }

    case AST_UNARY_OP: {
        ASTNode *operand = ast_get_child(expr, 0);
        sem_check_expr(checker, operand);
        if (expr->op == '!' && operand && operand->type == AST_STRING_LIT) {
            sem_report_error(checker, expr->line, expr->col,
                             "type mismatch: cannot apply '!' to string");
        }
        break;
    }

    case AST_ASSIGN: {
        ASTNode *target = ast_get_child(expr, 0);
        ASTNode *value = ast_get_child(expr, 1);

        if (target && target->type == AST_IDENT) {
            Symbol *sym = symtab_lookup(checker->symtab, target->name);
            if (!sym) {
                char buf[256];
                snprintf(buf, sizeof(buf), "assignment to undeclared variable '%s'", target->name);
                sem_report_error(checker, expr->line, expr->col, buf);
            }
        }

        sem_check_expr(checker, target);
        sem_check_expr(checker, value);

        if (value && value->type == AST_STRING_LIT) {
            sem_report_error(checker, expr->line, expr->col,
                             "type mismatch: cannot assign string to int variable");
        }
        break;
    }

    case AST_CALL: {
        Symbol *sym = symtab_lookup(checker->symtab, expr->name);
        if (!sym) {
            char buf[256];
            snprintf(buf, sizeof(buf), "call to undeclared function '%s'", expr->name);
            sem_report_error(checker, expr->line, expr->col, buf);
        } else if (sym->type != SYM_FUNC) {
            char buf[256];
            snprintf(buf, sizeof(buf), "'%s' is not a function", expr->name);
            sem_report_error(checker, expr->line, expr->col, buf);
        }

        for (int i = 0; i < expr->child_count; i++) {
            sem_check_expr(checker, expr->children[i]);
        }
        break;
    }

    default:
        break;
    }
}

void sem_check_statement(SemanticChecker *checker, ASTNode *stmt) {
    if (!stmt) return;

    switch (stmt->type) {
    case AST_RETURN_STMT: {
        ASTNode *ret_expr = ast_get_child(stmt, 0);
        if (ret_expr) {
            sem_check_expr(checker, ret_expr);
            if (ret_expr->type == AST_STRING_LIT) {
                sem_report_error(checker, stmt->line, stmt->col,
                                 "type mismatch: function returns int, but return expression is string");
            }
        }
        break;
    }

    case AST_IF_STMT: {
        ASTNode *cond = ast_get_child(stmt, 0);
        ASTNode *then_body = ast_get_child(stmt, 1);
        ASTNode *else_body = ast_get_child(stmt, 2);

        sem_check_expr(checker, cond);
        sem_check_statement(checker, then_body);

        if (else_body) {
            SymTab *saved = checker->symtab;
            checker->symtab = symtab_enter_scope(saved);
            sem_check_statement(checker, ast_get_child(else_body, 0));
            checker->symtab = symtab_exit_scope(checker->symtab);
            checker->symtab = saved;
        }
        break;
    }

    case AST_WHILE_STMT: {
        ASTNode *cond = ast_get_child(stmt, 0);
        ASTNode *body = ast_get_child(stmt, 1);

        sem_check_expr(checker, cond);
        sem_check_statement(checker, body);
        break;
    }

    case AST_BLOCK: {
        SymTab *saved = checker->symtab;
        checker->symtab = symtab_enter_scope(saved);

        for (int i = 0; i < stmt->child_count; i++) {
            sem_check_statement(checker, stmt->children[i]);
        }

        checker->symtab = symtab_exit_scope(checker->symtab);
        checker->symtab = saved;
        break;
    }

    case AST_VAR_DECL: {
        if (symtab_lookup_current(checker->symtab, stmt->name)) {
            char buf[256];
            snprintf(buf, sizeof(buf), "redeclaration of variable '%s'", stmt->name);
            sem_report_error(checker, stmt->line, stmt->col, buf);
        } else {
            symtab_insert(checker->symtab, stmt->name, SYM_INT, stmt->line);
        }
        break;
    }

    case AST_ASSIGN:
    case AST_CALL:
    case AST_BINARY_OP:
    case AST_UNARY_OP:
        sem_check_expr(checker, stmt);
        break;

    default:
        break;
    }
}

bool sem_check_has_return(ASTNode *node) {
    if (!node) return false;

    if (node->type == AST_RETURN_STMT) {
        return true;
    }

    if (node->type == AST_IF_STMT) {
        ASTNode *then_body = ast_get_child(node, 1);
        ASTNode *else_body = ast_get_child(node, 2);
        bool then_has = sem_check_has_return(then_body);
        bool else_has = else_body ? sem_check_has_return(ast_get_child(else_body, 0)) : false;
        return then_has && else_has;
    }

    if (node->type == AST_BLOCK) {
        for (int i = 0; i < node->child_count; i++) {
            if (sem_check_has_return(node->children[i])) {
                return true;
            }
        }
    }

    return false;
}

void sem_check_function(SemanticChecker *checker, ASTNode *func_def) {
    if (!func_def || func_def->type != AST_FUNC_DEF) return;

    checker->current_function = func_def;

    SymTab *saved = checker->symtab;
    checker->symtab = symtab_enter_scope(saved);

    bool has_duplicate = false;
    for (int i = 0; i < func_def->child_count; i++) {
        ASTNode *param = func_def->children[i];
        if (param->type == AST_PARAM) {
            if (symtab_lookup_current(checker->symtab, param->name)) {
                char buf[256];
                snprintf(buf, sizeof(buf), "duplicate parameter '%s' in function '%s'",
                         param->name, func_def->name);
                sem_report_error(checker, param->line, param->col, buf);
                has_duplicate = true;
            } else {
                symtab_insert(checker->symtab, param->name, SYM_PARAM, param->line);
            }
        } else {
            break;
        }
    }

    for (int i = 0; i < func_def->child_count; i++) {
        ASTNode *child = func_def->children[i];
        if (child->type == AST_BLOCK) {
            sem_check_statement(checker, child);
        }
    }

    bool has_return = false;
    for (int i = 0; i < func_def->child_count; i++) {
        if (func_def->children[i]->type == AST_BLOCK) {
            has_return = sem_check_has_return(func_def->children[i]);
        }
    }

    if (!has_return) {
        char buf[256];
        snprintf(buf, sizeof(buf), "function '%s' might not return a value on all paths", func_def->name);
        sem_report_error(checker, func_def->line, func_def->col, buf);
    }

    checker->symtab = symtab_exit_scope(checker->symtab);
    checker->symtab = saved;
}

void sem_check_program(SemanticChecker *checker, ASTNode *program) {
    if (!program || program->type != AST_PROGRAM) return;

    checker->symtab = symtab_enter_scope(NULL);

    bool has_main = false;
    for (int i = 0; i < program->child_count; i++) {
        ASTNode *func = program->children[i];
        if (func->type == AST_FUNC_DEF) {
            if (symtab_lookup_current(checker->symtab, func->name)) {
                char buf[256];
                snprintf(buf, sizeof(buf), "duplicate function definition '%s'", func->name);
                sem_report_error(checker, func->line, func->col, buf);
            } else {
                symtab_insert(checker->symtab, func->name, SYM_FUNC, func->line);
            }

            if (strcmp(func->name, "main") == 0) {
                has_main = true;
            }
        }
    }

    for (int i = 0; i < program->child_count; i++) {
        sem_check_function(checker, program->children[i]);
    }

    if (!has_main) {
        sem_report_error(checker, 0, 0, "program must have a 'main' function");
    }

    checker->symtab = symtab_exit_scope(checker->symtab);
}
