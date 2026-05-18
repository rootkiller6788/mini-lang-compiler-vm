#include "parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void advance_token(Parser *parser) {
    parser->current_token = parser->peek_token;
    parser->peek_token = lexer_next_token(&parser->lexer);
}

static bool check(Parser *parser, TokenType type) {
    return parser->current_token.type == type;
}

static bool check_peek(Parser *parser, TokenType type) {
    return parser->peek_token.type == type;
}

static bool match(Parser *parser, TokenType type) {
    if (check(parser, type)) {
        advance_token(parser);
        return true;
    }
    return false;
}

static void expect(Parser *parser, TokenType type, const char *context) {
    if (!match(parser, type)) {
        fprintf(stderr, "parser error at line %d col %d: expected '%s' in %s, got '%s'\n",
                parser->current_token.line, parser->current_token.col,
                lexer_token_type_name(type), context,
                parser->current_token.lexeme);
        parser->error_count++;
    }
}

void parser_init(Parser *parser, const char *source) {
    lexer_init(&parser->lexer, source);
    parser->error_count = 0;
    parser->current_token = lexer_next_token(&parser->lexer);
    parser->peek_token = lexer_next_token(&parser->lexer);
}

int parser_error_count(const Parser *parser) {
    return parser->error_count;
}

static ASTNode *parse_primary(Parser *parser) {
    if (match(parser, TOK_INT_LIT)) {
        ASTNode *node = ast_create_node(AST_INT_LIT);
        node->int_value = atoi(parser->current_token.lexeme);
        return node;
    }

    if (match(parser, TOK_STRING)) {
        ASTNode *node = ast_create_node(AST_STRING_LIT);
        ast_set_name(node, parser->current_token.lexeme);
        return node;
    }

    if (check(parser, TOK_IDENT)) {
        Token ident_tok = parser->current_token;
        advance_token(parser);

        if (match(parser, TOK_LPAREN)) {
            ASTNode *call = ast_create_node(AST_CALL);
            ast_set_name(call, ident_tok.lexeme);
            call->line = ident_tok.line;
            call->col = ident_tok.col;

            if (!check(parser, TOK_RPAREN)) {
                do {
                    ASTNode *arg = parser_parse_expr(parser);
                    ast_add_child(call, arg);
                } while (match(parser, TOK_COMMA));
            }
            expect(parser, TOK_RPAREN, "function call arguments");
            return call;
        }

        ASTNode *ident = ast_create_node(AST_IDENT);
        ast_set_name(ident, ident_tok.lexeme);
        ident->line = ident_tok.line;
        ident->col = ident_tok.col;
        return ident;
    }

    if (match(parser, TOK_LPAREN)) {
        ASTNode *expr = parser_parse_expr(parser);
        expect(parser, TOK_RPAREN, "parenthesized expression");
        return expr;
    }

    fprintf(stderr, "parser error at line %d col %d: unexpected token '%s'\n",
            parser->current_token.line, parser->current_token.col,
            parser->current_token.lexeme);
    parser->error_count++;
    return ast_create_node(AST_INT_LIT);
}

static ASTNode *parse_unary(Parser *parser) {
    if (match(parser, TOK_MINUS)) {
        ASTNode *node = ast_create_node(AST_UNARY_OP);
        node->op = '-';
        ast_add_child(node, parse_unary(parser));
        return node;
    }
    if (match(parser, TOK_NOT)) {
        ASTNode *node = ast_create_node(AST_UNARY_OP);
        node->op = '!';
        ast_add_child(node, parse_unary(parser));
        return node;
    }
    return parse_primary(parser);
}

static ASTNode *parse_multiplicative(Parser *parser) {
    ASTNode *left = parse_unary(parser);
    while (match(parser, TOK_STAR) || match(parser, TOK_SLASH)) {
        ASTNode *node = ast_create_node(AST_BINARY_OP);
        node->op = parser->current_token.lexeme[0];
        ast_add_child(node, left);
        ast_add_child(node, parse_unary(parser));
        left = node;
    }
    return left;
}

static ASTNode *parse_additive(Parser *parser) {
    ASTNode *left = parse_multiplicative(parser);
    while (match(parser, TOK_PLUS) || match(parser, TOK_MINUS)) {
        ASTNode *node = ast_create_node(AST_BINARY_OP);
        node->op = parser->current_token.lexeme[0];
        ast_add_child(node, left);
        ast_add_child(node, parse_multiplicative(parser));
        left = node;
    }
    return left;
}

static ASTNode *parse_relational(Parser *parser) {
    ASTNode *left = parse_additive(parser);
    while (match(parser, TOK_LT) || match(parser, TOK_GT) ||
           match(parser, TOK_LE) || match(parser, TOK_GE)) {
        ASTNode *node = ast_create_node(AST_BINARY_OP);
        const char *lexeme = parser->current_token.lexeme;
        if (strcmp(lexeme, "<=") == 0) { node->op = 'L'; }
        else if (strcmp(lexeme, ">=") == 0) { node->op = 'G'; }
        else { node->op = lexeme[0]; }
        ast_add_child(node, left);
        ast_add_child(node, parse_additive(parser));
        left = node;
    }
    return left;
}

static ASTNode *parse_equality(Parser *parser) {
    ASTNode *left = parse_relational(parser);
    while (match(parser, TOK_EQ_EQ) || match(parser, TOK_NEQ)) {
        ASTNode *node = ast_create_node(AST_BINARY_OP);
        node->op = parser->current_token.lexeme[0];
        ast_add_child(node, left);
        ast_add_child(node, parse_relational(parser));
        left = node;
    }
    return left;
}

static ASTNode *parse_logical_and(Parser *parser) {
    ASTNode *left = parse_equality(parser);
    while (match(parser, TOK_AND)) {
        ASTNode *node = ast_create_node(AST_BINARY_OP);
        node->op = '&';
        ast_add_child(node, left);
        ast_add_child(node, parse_equality(parser));
        left = node;
    }
    return left;
}

static ASTNode *parse_logical_or(Parser *parser) {
    ASTNode *left = parse_logical_and(parser);
    while (match(parser, TOK_OR)) {
        ASTNode *node = ast_create_node(AST_BINARY_OP);
        node->op = '|';
        ast_add_child(node, left);
        ast_add_child(node, parse_logical_and(parser));
        left = node;
    }
    return left;
}

static ASTNode *parse_assignment(Parser *parser) {
    ASTNode *left = parse_logical_or(parser);

    if (match(parser, TOK_EQ)) {
        if (left->type != AST_IDENT) {
            fprintf(stderr, "parser error at line %d: invalid assignment target\n",
                    parser->current_token.line);
            parser->error_count++;
        }
        ASTNode *assign = ast_create_node(AST_ASSIGN);
        ast_add_child(assign, left);
        ast_add_child(assign, parse_assignment(parser));
        return assign;
    }

    return left;
}

ASTNode *parser_parse_expr(Parser *parser) {
    return parse_assignment(parser);
}

ASTNode *parser_parse_block(Parser *parser) {
    ASTNode *block = ast_create_node(AST_BLOCK);
    expect(parser, TOK_LBRACE, "block start");
    while (!check(parser, TOK_RBRACE) && !check(parser, TOK_EOF)) {
        ASTNode *stmt = parser_parse_statement(parser);
        if (stmt) {
            ast_add_child(block, stmt);
        }
    }
    expect(parser, TOK_RBRACE, "block end");
    return block;
}

static ASTNode *parse_if_statement(Parser *parser) {
    ASTNode *if_node = ast_create_node(AST_IF_STMT);
    if_node->line = parser->current_token.line;
    expect(parser, TOK_LPAREN, "if condition");
    ast_add_child(if_node, parser_parse_expr(parser));
    expect(parser, TOK_RPAREN, "if condition");

    if (check(parser, TOK_LBRACE)) {
        ast_add_child(if_node, parser_parse_block(parser));
    } else {
        ast_add_child(if_node, parser_parse_statement(parser));
    }

    if (match(parser, TOK_IF)) {
        ASTNode *else_node = ast_create_node(AST_BLOCK);
        ast_add_child(else_node, parse_if_statement(parser));
        ast_add_child(if_node, else_node);
    }

    return if_node;
}

static ASTNode *parse_while_statement(Parser *parser) {
    ASTNode *while_node = ast_create_node(AST_WHILE_STMT);
    while_node->line = parser->current_token.line;
    expect(parser, TOK_LPAREN, "while condition");
    ast_add_child(while_node, parser_parse_expr(parser));
    expect(parser, TOK_RPAREN, "while condition");

    if (check(parser, TOK_LBRACE)) {
        ast_add_child(while_node, parser_parse_block(parser));
    } else {
        ast_add_child(while_node, parser_parse_statement(parser));
    }

    return while_node;
}

static ASTNode *parse_return_statement(Parser *parser) {
    ASTNode *ret_node = ast_create_node(AST_RETURN_STMT);
    ret_node->line = parser->current_token.line;

    if (!check(parser, TOK_SEMI)) {
        ast_add_child(ret_node, parser_parse_expr(parser));
    }

    expect(parser, TOK_SEMI, "return statement");
    return ret_node;
}

static ASTNode *parse_var_decl(Parser *parser) {
    Token ident_tok = parser->current_token;
    advance_token(parser);
    expect(parser, TOK_SEMI, "variable declaration");

    ASTNode *decl = ast_create_node(AST_VAR_DECL);
    ast_set_name(decl, ident_tok.lexeme);
    decl->line = ident_tok.line;
    decl->col = ident_tok.col;
    return decl;
}

static ASTNode *parse_expr_statement(Parser *parser) {
    ASTNode *expr = parser_parse_expr(parser);
    expect(parser, TOK_SEMI, "expression statement");
    return expr;
}

ASTNode *parser_parse_statement(Parser *parser) {
    if (match(parser, TOK_IF)) {
        return parse_if_statement(parser);
    }

    if (match(parser, TOK_WHILE)) {
        return parse_while_statement(parser);
    }

    if (match(parser, TOK_RETURN)) {
        return parse_return_statement(parser);
    }

    if (check(parser, TOK_INT) && check_peek(parser, TOK_IDENT)) {
        advance_token(parser);
        return parse_var_decl(parser);
    }

    if (check(parser, TOK_LBRACE)) {
        return parser_parse_block(parser);
    }

    return parse_expr_statement(parser);
}

ASTNode *parser_parse_function(Parser *parser) {
    expect(parser, TOK_INT, "function return type");

    Token name_tok = parser->current_token;
    if (name_tok.type != TOK_IDENT) {
        fprintf(stderr, "parser error at line %d col %d: expected function name\n",
                name_tok.line, name_tok.col);
        parser->error_count++;
        return NULL;
    }
    advance_token(parser);

    ASTNode *func = ast_create_node(AST_FUNC_DEF);
    ast_set_name(func, name_tok.lexeme);
    func->line = name_tok.line;
    func->col = name_tok.col;

    expect(parser, TOK_LPAREN, "function parameters");

    if (!check(parser, TOK_RPAREN)) {
        do {
            if (!match(parser, TOK_INT)) {
                fprintf(stderr, "parser error at line %d col %d: expected parameter type 'int'\n",
                        parser->current_token.line, parser->current_token.col);
                parser->error_count++;
                break;
            }

            Token param_tok = parser->current_token;
            if (param_tok.type != TOK_IDENT) {
                fprintf(stderr, "parser error at line %d col %d: expected parameter name\n",
                        param_tok.line, param_tok.col);
                parser->error_count++;
                break;
            }
            advance_token(parser);

            ASTNode *param = ast_create_node(AST_PARAM);
            ast_set_name(param, param_tok.lexeme);
            param->line = param_tok.line;
            param->col = param_tok.col;
            ast_add_child(func, param);
        } while (match(parser, TOK_COMMA));
    }

    expect(parser, TOK_RPAREN, "function parameters");
    ast_add_child(func, parser_parse_block(parser));
    return func;
}

ASTNode *parser_parse_program(Parser *parser) {
    ASTNode *program = ast_create_node(AST_PROGRAM);

    while (!check(parser, TOK_EOF)) {
        if (parser->current_token.type == TOK_ERROR) {
            fprintf(stderr, "parser error: lexer error encountered\n");
            advance_token(parser);
            continue;
        }

        ASTNode *func = parser_parse_function(parser);
        if (func) {
            ast_add_child(program, func);
        }

        if (parser->error_count > 0) {
            advance_token(parser);
        }
    }

    return program;
}
