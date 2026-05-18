#ifndef LEXER_H
#define LEXER_H

#include <stdbool.h>
#include <stddef.h>

#define TOKEN_MAX_LEXEME 256

typedef enum {
    TOK_INT,
    TOK_IF,
    TOK_WHILE,
    TOK_RETURN,
    TOK_IDENT,
    TOK_INT_LIT,
    TOK_STRING,
    TOK_PLUS,
    TOK_MINUS,
    TOK_STAR,
    TOK_SLASH,
    TOK_LPAREN,
    TOK_RPAREN,
    TOK_LBRACE,
    TOK_RBRACE,
    TOK_SEMI,
    TOK_EQ,
    TOK_EQ_EQ,
    TOK_NEQ,
    TOK_LT,
    TOK_GT,
    TOK_LE,
    TOK_GE,
    TOK_AND,
    TOK_OR,
    TOK_NOT,
    TOK_COMMA,
    TOK_EOF,
    TOK_ERROR
} TokenType;

typedef struct {
    TokenType type;
    char lexeme[TOKEN_MAX_LEXEME];
    int line;
    int col;
} Token;

typedef struct {
    const char *source;
    int pos;
    int line;
    int col;
    char current;
} Lexer;

void lexer_init(Lexer *lexer, const char *source);
Token lexer_next_token(Lexer *lexer);
Token lexer_peek(Lexer *lexer);
void lexer_print_token(const Token *token);
const char *lexer_token_type_name(TokenType type);

#endif
