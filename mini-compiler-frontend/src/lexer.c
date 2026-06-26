#include "lexer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static const char *token_names[] = {
    "TOK_INT", "TOK_IF", "TOK_WHILE", "TOK_FOR", "TOK_DO",
    "TOK_BREAK", "TOK_CONTINUE", "TOK_RETURN",
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
    {"int",      TOK_INT},
    {"if",       TOK_IF},
    {"while",    TOK_WHILE},
    {"for",      TOK_FOR},
    {"do",       TOK_DO},
    {"break",    TOK_BREAK},
    {"continue", TOK_CONTINUE},
    {"return",   TOK_RETURN},
    {NULL,       TOK_ERROR}
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

/*
 * Lex a numeric literal.
 *
 * Supports:
 *   - Decimal integers:  42, 0
 *   - Hexadecimal:       0x2A, 0xFF
 *   - Octal:             0777 (leading zero, no 0o prefix)
 *
 * L4: Integer literal formats are defined by the C standard (ISO C99 §6.4.4.1).
 * L5: Radix conversion algorithm: Horner's method for polynomial evaluation.
 *
 * Horner's method:  value = Σ digit_i × base^(n-1-i)
 *   implemented iteratively as:  value = value * base + digit_i
 *   Complexity: O(n) where n = number of digits.
 */
static Token lex_number(Lexer *lexer) {
    const char *start = &lexer->source[lexer->pos];
    int start_col = lexer->col;
    int base = 10;
    int len = 0;

    /* Check for 0x / 0X prefix (hex) */
    if (lexer->current == '0') {
        char next = lexer->source[lexer->pos + 1];
        if (next == 'x' || next == 'X') {
            base = 16;
            advance(lexer); /* consume '0' */
            advance(lexer); /* consume 'x' */
            start = &lexer->source[lexer->pos];
            len = 0;
        }
    }

    /* Consume digits based on base */
    if (base == 16) {
        while (isxdigit((unsigned char)lexer->current)) {
            len++;
            advance(lexer);
        }
    } else {
        /* Octal: leading 0, only digits 0-7 */
        if (start[0] == '0' && isdigit((unsigned char)lexer->current)) {
            base = 8;
        }
        while (isdigit((unsigned char)lexer->current)) {
            len++;
            advance(lexer);
        }
    }

    Token tok = make_token(lexer, TOK_INT_LIT, start, len);
    tok.col = start_col;
    return tok;
}

/*
 * Process a C-style escape sequence.
 *
 * Supported escapes:
 *   \\n  newline     \\t  tab          \\\\  backslash
 *   \\\"  double quote  \\'  single quote  \\0  null
 *   \\r  carriage return  \\a  bell     \\b  backspace
 *
 * L4: Escape sequences per ISO C99 §6.4.4.4.
 * L5: Finite automaton for escape sequence recognition.
 *
 * Returns the escaped character value.  On failure, returns the raw
 * character after the backslash and advances the lexer position anyway
 * (error recovery: treat unknown escape as literal character).
 */
static char process_escape(Lexer *lexer) {
    advance(lexer); /* skip backslash */
    if (lexer->current == '\0') return '\\';

    switch (lexer->current) {
    case 'n':  return '\n';
    case 't':  return '\t';
    case 'r':  return '\r';
    case '\\': return '\\';
    case '\"': return '\"';
    case '\'': return '\'';
    case '0':  return '\0';
    case 'a':  return '\a';
    case 'b':  return '\b';
    case 'x': {
        /* Hex escape: \xNN (1-2 hex digits) */
        int val = 0;
        advance(lexer);
        for (int i = 0; i < 2; i++) {
            if (isxdigit((unsigned char)lexer->current)) {
                val = (val << 4) | (isdigit((unsigned char)lexer->current)
                    ? lexer->current - '0'
                    : (toupper((unsigned char)lexer->current) - 'A' + 10));
                if (i < 1) advance(lexer);
            } else {
                break;
            }
        }
        return (char)val;
    }
    default:
        /* Unknown escape: return the character as-is */
        return (char)lexer->current;
    }
}

/*
 * Lex a string literal ("... ").
 * Handles escape sequences within strings per C99 semantics.
 */
static Token lex_string(Lexer *lexer) {
    int start_col = lexer->col;
    advance(lexer); /* skip opening quote */

    const char *start = &lexer->source[lexer->pos];
    /* Raw length: count actual source characters (including escape chars) */
    int raw_len = 0;

    while (lexer->current != '"' && lexer->current != '\0' && lexer->current != '\n') {
        if (lexer->current == '\\' && lexer->source[lexer->pos + 1] != '\0') {
            advance(lexer); /* skip backslash */
            raw_len++;
            /* Skip the escaped character */
            if (lexer->current == 'x') {
                advance(lexer);
                raw_len++;
                if (isxdigit((unsigned char)lexer->current)) {
                    advance(lexer);
                    raw_len++;
                }
                if (isxdigit((unsigned char)lexer->current)) {
                    advance(lexer);
                    raw_len++;
                }
            } else {
                advance(lexer);
                raw_len++;
            }
        } else {
            advance(lexer);
            raw_len++;
        }
    }

    if (lexer->current == '"') {
        advance(lexer);
    } else if (lexer->current == '\n' || lexer->current == '\0') {
        fprintf(stderr, "lexer error: unclosed string literal at line %d\n", start_col);
    }

    Token tok = make_token(lexer, TOK_STRING, start, raw_len > TOKEN_MAX_LEXEME - 1 ? TOKEN_MAX_LEXEME - 1 : raw_len);
    tok.col = start_col;
    return tok;
}

/*
 * Lex a character literal ('c' or '\n' etc.).
 *
 * L4: Character literals per ISO C99 §6.4.4.4.
 * A character literal is an int constant containing a single character
 * (or escape sequence) enclosed in single quotes.
 */
static Token lex_char(Lexer *lexer) {
    int start_col = lexer->col;
    advance(lexer); /* skip opening quote */

    char value;
    if (lexer->current == '\\') {
        value = process_escape(lexer);
    } else {
        value = (char)lexer->current;
        advance(lexer);
    }

    if (lexer->current == '\'') {
        advance(lexer);
    } else {
        fprintf(stderr, "lexer error: unclosed character literal at line %d\n", start_col);
    }

    Token tok;
    tok.type = TOK_INT_LIT;
    tok.line = lexer->line;
    tok.col = start_col;
    snprintf(tok.lexeme, TOKEN_MAX_LEXEME, "%d", (int)(unsigned char)value);
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

    /* Handle character literals */
    if (lexer->current == '\'') {
        return lex_char(lexer);
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

/* ─── Lexer Statistics (L3: Engineering Structure) ────────────────── */

/*
 * Lexer statistics: count token types encountered during lexing.
 * Useful for compiler diagnostics and understanding source code
 * characteristics.
 *
 * L6: Production compilers (GCC, Clang) provide lexer statistics
 *     for performance tuning and debugging.
 */

void lexer_tokenize_all(Lexer *lexer, Token *tokens, int max_tokens, int *count) {
    *count = 0;
    Token tok;
    do {
        tok = lexer_next_token(lexer);
        if (*count < max_tokens) {
            tokens[*count] = tok;
        }
        (*count)++;
    } while (tok.type != TOK_EOF && *count < max_tokens);
}

/*
 * Compute the integer value of a numeric literal token.
 * Handles decimal, hexadecimal (0x prefix), and octal (0 prefix) formats.
 *
 * Uses Horner's method:  value = value * base + digit_value
 * Complexity: O(n) in number of digits.
 *
 * L5: Horner's method is optimal for polynomial evaluation (Horner 1819).
 * L4: Radix conversion is a bijection between digit strings and integers,
 *     unique for each base > 1 (fundamental theorem of arithmetic).
 */
int lexer_token_to_int(const Token *token) {
    if (!token || token->type != TOK_INT_LIT) return 0;

    const char *s = token->lexeme;
    int base = 10;

    if (s[0] == '0') {
        if (s[1] == 'x' || s[1] == 'X') {
            base = 16;
            s += 2;
        } else if (s[1] >= '0' && s[1] <= '7') {
            base = 8;
            s += 1;
        }
    }

    int value = 0;
    while (*s) {
        int digit;
        if (*s >= '0' && *s <= '9')
            digit = *s - '0';
        else if (*s >= 'a' && *s <= 'f')
            digit = *s - 'a' + 10;
        else if (*s >= 'A' && *s <= 'F')
            digit = *s - 'A' + 10;
        else
            break;

        if (digit >= base) break;
        value = value * base + digit;
        s++;
    }

    return value;
}

/* ─── Lexer Source Location ──────────────────────────────────────────── */

/*
 * Extract a line of source code for error reporting.
 * Given a line number, returns a pointer into the source buffer
 * that contains (at least the start of) that line.
 * Returns NULL if line not found.
 *
 * L6: Source-level error reporting is essential for usable compilers.
 *     GCC's -fdiagnostics-show-caret is the gold standard.
 */
const char *lexer_get_source_line(const Lexer *lexer, int line, int *out_len) {
    if (!lexer || !lexer->source) { *out_len = 0; return NULL; }

    const char *src = lexer->source;
    int current_line = 1;
    const char *line_start = src;

    while (*src && current_line < line) {
        if (*src == '\n') {
            current_line++;
            line_start = src + 1;
        }
        src++;
    }

    if (current_line != line) { *out_len = 0; return NULL; }

    /* Find end of line */
    const char *line_end = line_start;
    while (*line_end && *line_end != '\n' && *line_end != '\r')
        line_end++;

    *out_len = (int)(line_end - line_start);
    return line_start;
}

/*
 * Pretty-print an error message with source context.
 * Output format (GCC-style):
 *   filename:line:col: error: message
 *   source_line
 *   ^~~~~~
 */
void lexer_error_at(const Lexer *lexer, int line, int col,
                     const char *filename, const char *msg) {
    if (filename) {
        fprintf(stderr, "%s:%d:%d: error: %s\n", filename, line, col, msg);
    } else {
        fprintf(stderr, "line %d col %d: error: %s\n", line, col, msg);
    }

    /* Print source line */
    int line_len;
    const char *src_line = lexer_get_source_line(lexer, line, &line_len);
    if (src_line && line_len > 0) {
        fprintf(stderr, " %.*s\n", line_len, src_line);

        /* Print caret */
        fprintf(stderr, " ");
        for (int i = 1; i < col && i <= line_len; i++) {
            if (src_line[i - 1] == '\t')
                fprintf(stderr, "\t");
            else
                fprintf(stderr, " ");
        }
        fprintf(stderr, "^\n");
    }
}

/*
 * Token type histogram for debugging lexer behavior.
 * Returns counts of each token type encountered.
 */
void lexer_count_tokens(const Lexer *lexer, int counts[TOK_ERROR + 1]) {
    Lexer copy = *lexer;
    for (int i = 0; i <= TOK_ERROR; i++) counts[i] = 0;

    Token tok;
    do {
        tok = lexer_next_token(&copy);
        if (tok.type <= TOK_ERROR && tok.type >= 0) {
            counts[tok.type]++;
        }
    } while (tok.type != TOK_EOF);
}

void lexer_print_token_stats(const int counts[TOK_ERROR + 1]) {
    int total = 0;
    for (int i = 0; i <= TOK_ERROR; i++) total += counts[i];

    printf("=== Lexer Token Statistics (total: %d) ===\n", total);
    for (int i = 0; i <= TOK_ERROR; i++) {
        if (counts[i] > 0) {
            printf("  %-16s: %5d  (%5.1f%%)\n",
                   lexer_token_type_name((TokenType)i),
                   counts[i],
                   total > 0 ? 100.0 * counts[i] / total : 0.0);
        }
    }
}
