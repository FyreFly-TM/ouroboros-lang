#ifndef LEXER_H
#define LEXER_H

#include <stdio.h>

typedef enum {
    TOKEN_IDENTIFIER,
    TOKEN_KEYWORD,
    TOKEN_NUMBER,
    TOKEN_STRING,
    TOKEN_BOOL,
    TOKEN_OPERATOR,
    TOKEN_SYMBOL,
    TOKEN_EOF
} TokenType;

typedef struct Token {
    TokenType type;
    char text[256];
    int line;
    int col;
} Token;

Token next_token(FILE *file, int *line, int *col);
Token next_token_string(int *line, int *col);
Token* lex(const char* source);

#endif // LEXER_H
