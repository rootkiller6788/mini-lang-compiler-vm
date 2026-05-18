#include "parser.h"
#include "lexer.h"
#include "ast.h"
#include "semantic.h"
#include "symtab.h"
#include <stdio.h>
#include <stdlib.h>

static void run_check(const char *label, const char *source) {
    printf("\n=== %s ===\n%s\n", label, source);
    printf("=== Semantic Analysis ===\n");

    Parser parser;
    parser_init(&parser, source);
    ASTNode *program = parser_parse_program(&parser);

    if (parser_error_count(&parser) > 0) {
        printf("Parse errors: %d\n", parser_error_count(&parser));
    }

    SemanticChecker checker;
    sem_checker_init(&checker);
    sem_check_program(&checker, program);

    if (checker.error_count == 0) {
        printf("No semantic errors found.\n");
    } else {
        printf("Semantic errors: %d\n", checker.error_count);
    }

    ast_free(program);
}

int main(void) {
    run_check("Valid Program", 
        "int add(int a, int b) {\n"
        "    return a + b;\n"
        "}\n"
        "\n"
        "int main() {\n"
        "    int x;\n"
        "    int y;\n"
        "    x = 10;\n"
        "    y = 20;\n"
        "    return add(x, y);\n"
        "}\n");

    run_check("Undeclared Variable", 
        "int main() {\n"
        "    int x;\n"
        "    x = y;\n"
        "    return x;\n"
        "}\n");

    run_check("Redeclared Variable", 
        "int main() {\n"
        "    int x;\n"
        "    int x;\n"
        "    return x;\n"
        "}\n");

    run_check("Call to Undeclared Function", 
        "int main() {\n"
        "    int x;\n"
        "    x = foo();\n"
        "    return x;\n"
        "}\n");

    run_check("Missing main", 
        "int helper(int n) {\n"
        "    return n;\n"
        "}\n");

    return 0;
}
