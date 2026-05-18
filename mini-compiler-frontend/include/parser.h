#ifndef PARSER_H
#define PARSER_H

#include <stdbool.h>
#include "lexer.h"
#include "ast.h"

typedef struct {
    Lexer lexer;
    Token current_token;
    Token peek_token;
    int error_count;
} Parser;

void parser_init(Parser *parser, const char *source);
ASTNode *parser_parse_program(Parser *parser);
ASTNode *parser_parse_function(Parser *parser);
ASTNode *parser_parse_statement(Parser *parser);
ASTNode *parser_parse_expr(Parser *parser);
ASTNode *parser_parse_block(Parser *parser);
int parser_error_count(const Parser *parser);

#endif
