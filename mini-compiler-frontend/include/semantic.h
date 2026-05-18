#ifndef SEMANTIC_H
#define SEMANTIC_H

#include <stdbool.h>
#include "ast.h"
#include "symtab.h"

typedef struct {
    SymTab *symtab;
    int error_count;
    ASTNode *current_function;
} SemanticChecker;

void sem_checker_init(SemanticChecker *checker);
void sem_check_program(SemanticChecker *checker, ASTNode *program);
void sem_check_function(SemanticChecker *checker, ASTNode *func_def);
void sem_check_expr(SemanticChecker *checker, ASTNode *expr);
void sem_check_statement(SemanticChecker *checker, ASTNode *stmt);
void sem_report_error(SemanticChecker *checker, int line, int col, const char *msg);
bool sem_check_has_return(ASTNode *node);

#endif
