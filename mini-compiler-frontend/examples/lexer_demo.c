#include "lexer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    const char *source =
        "/* Mini C Frontend Demo */\n"
        "int main() {\n"
        "    int x;\n"
        "    x = 42;\n"
        "    return x + 1;\n"
        "}\n"
        "\n"
        "int add(int a, int b) {\n"
        "    return a + b;\n"
        "}\n";

    printf("=== Source Code ===\n%s\n", source);
    printf("=== Token Stream ===\n");

    Lexer lexer;
    lexer_init(&lexer, source);

    Token token;
    do {
        token = lexer_next_token(&lexer);
        lexer_print_token(&token);
    } while (token.type != TOK_EOF);

    printf("=== Lexing Complete ===\n");
    return 0;
}
