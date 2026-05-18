#include "parser.h"
#include "lexer.h"
#include "ast.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    const char *source =
        "/* Recursive Descent Parser Demo */\n"
        "int factorial(int n) {\n"
        "    int result;\n"
        "    result = 1;\n"
        "    if (n <= 1) {\n"
        "        return 1;\n"
        "    }\n"
        "    while (n > 0) {\n"
        "        result = result * n;\n"
        "        n = n - 1;\n"
        "    }\n"
        "    return result;\n"
        "}\n"
        "\n"
        "int main() {\n"
        "    int x;\n"
        "    x = factorial(5);\n"
        "    return x;\n"
        "}\n";

    printf("=== Source Code ===\n%s\n", source);
    printf("=== AST (Abstract Syntax Tree) ===\n");

    Parser parser;
    parser_init(&parser, source);

    ASTNode *program = parser_parse_program(&parser);

    if (parser_error_count(&parser) > 0) {
        printf("=== Parsing completed with %d errors ===\n", parser_error_count(&parser));
    } else {
        printf("=== Parsing successful ===\n\n");
    }

    ast_print_tree(program, 0);
    ast_free(program);

    return 0;
}
