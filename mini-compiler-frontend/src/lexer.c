#include "lexer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static const char *token_names[] = {
    "TOK_INT", "TOK_IF", "TOK_WHILE", "TOK_RETURN",
    "TOK_IDENT", "TOK_INT_LIT", "TOK_STRING",
    "TOK_PLUS", "TOK_MINUS", "TOK_STAR", "TOK_SLASH",
    "TOK_LPAREN", "TOK_RPAREN", "TOK_LBRACE", "TOK_RBRACE",
    "TOK_SEMI", "TOK_EQ", "TOK_EQ_EQ", "TOK_NEQ",
    "TOK_LT", "TOK_GT", "TOK_LE", "TOK_GE",
    "TOK_AND", "TOK_OR", "TOK_NOT", "TOK_COMMA",
    "TOK_EOF", "TOK_ERROR"
};

typedef struct {
    const char *keyword;
    TokenType type;
} KeywordEntry;

static const KeywordEntry keywords[] = {
    {"int",    TOK_INT},
    {"if",     TOK_IF},
    {"while",  TOK_WHILE},
    {"return", TOK_RETURN},
    {NULL,     TOK_ERROR}
};

const char *lexer_token_type_name(TokenType type) {
    if (type >= 0 && type <= TOK_ERROR) return token_names[type];
    return "TOK_UNKNOWN";
}

void lexer_init(Lexer *lexer, const char *source) {
    lexer->source = source;
    lexer->pos = 0;
    lexer->line = 1;
    lexer->col = 1;
    lexer->current = (source && source[0]) ? source[0] : '\0';
}

static void advance(Lexer *lexer) {
    if (lexer->current == '\0') return;
    lexer->pos++;
    lexer->col++;
    lexer->current = lexer->source[lexer->pos];
}

static void skip_whitespace(Lexer *lexer) {
    while (lexer->current == ' ' || lexer->current == '\t' ||
           lexer->current == '\n' || lexer->current == '\r') {
        if (lexer->current == '\n') {
            lexer->line++;
            lexer->col = 1;
        }
        advance(lexer);
    }
}

static void skip_line_comment(Lexer *lexer) {
    while (lexer->current != '\n' && lexer->current != '\0') {
        advance(lexer);
    }
}

static void skip_block_comment(Lexer *lexer) {
    int start_line = lexer->line;
    advance(lexer);
    if (lexer->current != '\0') advance(lexer);

    while (lexer->current != '\0') {
        if (lexer->current == '*' && lexer->source[lexer->pos + 1] == '/') {
            advance(lexer);
            advance(lexer);
            return;
        }
        if (lexer->current == '\n') {
            lexer->line++;
            lexer->col = 1;
        }
        advance(lexer);
    }

    fprintf(stderr, "lexer error: unclosed block comment starting at line %d\n", start_line);
}

static void skip_comments(Lexer *lexer) {
    while (true) {
        skip_whitespace(lexer);
        if (lexer->current == '/' && lexer->source[lexer->pos + 1] == '/') {
            skip_line_comment(lexer);
        } else if (lexer->current == '/' && lexer->source[lexer->pos + 1] == '*') {
            skip_block_comment(lexer);
        } else {
            break;
        }
    }
}

static Token make_token(Lexer *lexer, TokenType type, const char *start, int length) {
    Token token;
    token.type = type;
    token.line = lexer->line;
    token.col = lexer->col;
    int copy_len = length < (TOKEN_MAX_LEXEME - 1) ? length : (TOKEN_MAX_LEXEME - 1);
    memcpy(token.lexeme, start, copy_len);
    token.lexeme[copy_len] = '\0';
    return token;
}

static Token lex_ident_or_keyword(Lexer *lexer) {
    const char *start = &lexer->source[lexer->pos];
    int len = 0;
    int start_col = lexer->col;

    while (isalnum((unsigned char)lexer->current) || lexer->current == '_') {
        len++;
        advance(lexer);
    }

    for (int i = 0; keywords[i].keyword != NULL; i++) {
        int kw_len = (int)strlen(keywords[i].keyword);
        if (len == kw_len && strncmp(start, keywords[i].keyword, len) == 0) {
            Token tok = make_token(lexer, keywords[i].type, start, len);
            tok.col = start_col;
            return tok;
        }
    }

    Token tok = make_token(lexer, TOK_IDENT, start, len);
    tok.col = start_col;
    return tok;
}

static Token lex_number(Lexer *lexer) {
    const char *start = &lexer->source[lexer->pos];
    int len = 0;
    int start_col = lexer->col;

    while (isdigit((unsigned char)lexer->current)) {
        len++;
        advance(lexer);
    }

    Token tok = make_token(lexer, TOK_INT_LIT, start, len);
    tok.col = start_col;
    return tok;
}

static Token lex_string(Lexer *lexer) {
    int start_col = lexer->col;
    advance(lexer);

    const char *start = &lexer->source[lexer->pos];
    int len = 0;

    while (lexer->current != '"' && lexer->current != '\0' && lexer->current != '\n') {
        if (lexer->current == '\\' && lexer->source[lexer->pos + 1] != '\0') {
            advance(lexer);
            len++;
        }
        len++;
        advance(lexer);
    }

    if (lexer->current == '"') {
        advance(lexer);
    }

    Token tok = make_token(lexer, TOK_STRING, start, len);
    tok.col = start_col;
    return tok;
}

Token lexer_next_token(Lexer *lexer) {
    skip_comments(lexer);

    if (lexer->current == '\0') {
        return make_token(lexer, TOK_EOF, "", 0);
    }

    int start_col = lexer->col;

    if (isalpha((unsigned char)lexer->current) || lexer->current == '_') {
        return lex_ident_or_keyword(lexer);
    }

    if (isdigit((unsigned char)lexer->current)) {
        return lex_number(lexer);
    }

    if (lexer->current == '"') {
        return lex_string(lexer);
    }

    const char *src = lexer->source;
    int p = lexer->pos;

    switch (lexer->current) {
    case '+':
        advance(lexer);
        return make_token(lexer, TOK_PLUS, &src[p], 1);
    case '-':
        advance(lexer);
        return make_token(lexer, TOK_MINUS, &src[p], 1);
    case '*':
        advance(lexer);
        return make_token(lexer, TOK_STAR, &src[p], 1);
    case '/':
        advance(lexer);
        return make_token(lexer, TOK_SLASH, &src[p], 1);
    case '(':
        advance(lexer);
        return make_token(lexer, TOK_LPAREN, &src[p], 1);
    case ')':
        advance(lexer);
        return make_token(lexer, TOK_RPAREN, &src[p], 1);
    case '{':
        advance(lexer);
        return make_token(lexer, TOK_LBRACE, &src[p], 1);
    case '}':
        advance(lexer);
        return make_token(lexer, TOK_RBRACE, &src[p], 1);
    case ';':
        advance(lexer);
        return make_token(lexer, TOK_SEMI, &src[p], 1);
    case ',':
        advance(lexer);
        return make_token(lexer, TOK_COMMA, &src[p], 1);
    case '=':
        if (lexer->source[lexer->pos + 1] == '=') {
            advance(lexer);
            advance(lexer);
            return make_token(lexer, TOK_EQ_EQ, "==", 2);
        }
        advance(lexer);
        return make_token(lexer, TOK_EQ, &src[p], 1);
    case '!':
        if (lexer->source[lexer->pos + 1] == '=') {
            advance(lexer);
            advance(lexer);
            return make_token(lexer, TOK_NEQ, "!=", 2);
        }
        advance(lexer);
        return make_token(lexer, TOK_NOT, &src[p], 1);
    case '<':
        if (lexer->source[lexer->pos + 1] == '=') {
            advance(lexer);
            advance(lexer);
            return make_token(lexer, TOK_LE, "<=", 2);
        }
        advance(lexer);
        return make_token(lexer, TOK_LT, &src[p], 1);
    case '>':
        if (lexer->source[lexer->pos + 1] == '=') {
            advance(lexer);
            advance(lexer);
            return make_token(lexer, TOK_GE, ">=", 2);
        }
        advance(lexer);
        return make_token(lexer, TOK_GT, &src[p], 1);
    case '&':
        if (lexer->source[lexer->pos + 1] == '&') {
            advance(lexer);
            advance(lexer);
            return make_token(lexer, TOK_AND, "&&", 2);
        }
        break;
    case '|':
        if (lexer->source[lexer->pos + 1] == '|') {
            advance(lexer);
            advance(lexer);
            return make_token(lexer, TOK_OR, "||", 2);
        }
        break;
    default:
        break;
    }

    char error_buf[64];
    snprintf(error_buf, sizeof(error_buf), "lexer error: unexpected character '%c'", lexer->current);
    Token tok;
    tok.type = TOK_ERROR;
    tok.line = lexer->line;
    tok.col = start_col;
    snprintf(tok.lexeme, TOKEN_MAX_LEXEME, "%s", error_buf);
    advance(lexer);
    return tok;
}

Token lexer_peek(Lexer *lexer) {
    Lexer saved = *lexer;
    Token tok = lexer_next_token(lexer);
    *lexer = saved;
    return tok;
}

void lexer_print_token(const Token *token) {
    printf("Token{ type=%s, lexeme='%s', line=%d, col=%d }\n",
           lexer_token_type_name(token->type),
           token->lexeme,
           token->line,
           token->col);
}
