#ifndef LEXER_H
#define LEXER_H

#include <stdbool.h>
#include <stddef.h>

#define TOKEN_MAX_LEXEME 256

typedef enum {
    TOK_INT,
    TOK_IF,
    TOK_WHILE,
    TOK_FOR,
    TOK_DO,
    TOK_BREAK,
    TOK_CONTINUE,
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

/* ─── Lexer Utilities (L3: Engineering Structure) ─────────────────── */

/* Tokenize entire source into an array (up to max_tokens).
 * Sets *count to total number of tokens including EOF. */
void lexer_tokenize_all(Lexer *lexer, Token *tokens, int max_tokens, int *count);

/* Convert a numeric literal token to its integer value.
 * Supports decimal, hex (0x), octal (0) formats.
 * Uses Horner's method (O(n) in digit count). */
int lexer_token_to_int(const Token *token);

/* Extract source line for error reporting.
 * Returns pointer into source buffer and sets *out_len. */
const char *lexer_get_source_line(const Lexer *lexer, int line, int *out_len);

/* GCC-style error message with source context and caret. */
void lexer_error_at(const Lexer *lexer, int line, int col,
                     const char *filename, const char *msg);

/* Count token types in a source (uses a copy of the lexer). */
void lexer_count_tokens(const Lexer *lexer, int counts[TOK_ERROR + 1]);

/* Print token type histogram. */
void lexer_print_token_stats(const int counts[TOK_ERROR + 1]);

#endif
