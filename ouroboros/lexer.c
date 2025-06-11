#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include "lexer.h"

static int string_pos = 0;
static const char* source_string = NULL;

static int string_getc() {
    if (!source_string || source_string[string_pos] == '\0')
        return EOF;
    return source_string[string_pos++];
}

static void string_ungetc() {
    if (string_pos > 0)
        string_pos--;
}

static int peek_string() {
    if (!source_string || source_string[string_pos] == '\0')
        return EOF;
    return source_string[string_pos];
}

static int peek(FILE *file) {
    int c = fgetc(file);
    ungetc(c, file);
    return c;
}

static void skip_whitespace_string(int *line, int *col) {
    int c;
    while ((c = string_getc()) != EOF) {
        if (c == ' ' || c == '\t') {
            (*col)++;
        } else if (c == '\n') {
            (*line)++;
            *col = 1;
        } else if (c == '/' && peek_string() == '/') {
            while ((c = string_getc()) != '\n' && c != EOF);
            (*line)++;
            *col = 1;
        } else if (c == '/' && peek_string() == '*') {
            // Multi-line comment: /* ... */
            string_getc(); // consume '*'
            (*col) += 2; // for /*
            while ((c = string_getc()) != EOF) {
                (*col)++;
                if (c == '*') {
                    if (peek_string() == '/') {
                        string_getc(); // consume '/'
                        (*col)++;
                        break; // End of multi-line comment
                    }
                } else if (c == '\n') {
                    (*line)++;
                    *col = 1;
                }
            }
        } else {
            string_ungetc();
            break;
        }
    }
}

static void skip_whitespace(FILE *file, int *line, int *col) {
    int c;
    while ((c = fgetc(file)) != EOF) {
        if (c == ' ' || c == '\t') {
            (*col)++;
        } else if (c == '\n') {
            (*line)++;
            *col = 1;
        } else if (c == '/' && peek(file) == '/') {
            while ((c = fgetc(file)) != '\n' && c != EOF);
            (*line)++;
            *col = 1;
        } else if (c == '/' && peek(file) == '*') {
            // Multi-line comment: /* ... */
            fgetc(file); // consume '*'
            (*col) += 2; // for /*
            while ((c = fgetc(file)) != EOF) {
                (*col)++;
                if (c == '*') {
                    if (peek(file) == '/') {
                        fgetc(file); // consume '/'
                        (*col)++;
                        break; // End of multi-line comment
                    }
                } else if (c == '\n') {
                    (*line)++;
                    *col = 1;
                }
            }
        } else {
            ungetc(c, file);
            break;
        }
    }
}

static int is_symbol(int c) {
    // Added '<', '>' for generics, ':' for type declarations
    return strchr("(){}[];,:.<>", c) != NULL;
}

static int is_operator_char(int c) {
    return strchr("+-*/%=&|!<>", c) != NULL;
}

static const char *keywords[] = {
    "let", "const", "fn", "function", "return", "if", "else", "while", "for", "true", "false",
    "class", "new", "import", "public", "private", "protected", "static", "null", "var",
    // Added new keywords for types and OOP
    "int", "float", "bool", "string", "void", "print", "struct", "this", "extends"
};

static int is_keyword(const char *text) {
    for (int i = 0; i < (int)(sizeof(keywords)/sizeof(keywords[0])); i++) {
        if (strcmp(text, keywords[i]) == 0) return 1;
    }
    return 0;
}

Token next_token(FILE *file, int *line, int *col) {
    skip_whitespace(file, line, col);

    Token tok = { TOKEN_EOF, "", *line, *col };
    int c = fgetc(file);
    if (c == EOF) return tok;

    if (isalpha(c) || c == '_') {
        int i = 0;
        tok.text[i++] = c;
        while ((c = fgetc(file)) != EOF && (isalnum(c) || c == '_')) {
            if (i < (int)sizeof(tok.text) - 1) tok.text[i++] = c;
        }
        tok.text[i] = '\0';
        if (c != EOF) ungetc(c, file);
        tok.type = is_keyword(tok.text) ? TOKEN_KEYWORD :
                   (strcmp(tok.text, "true") == 0 || strcmp(tok.text, "false") == 0 ? TOKEN_BOOL : TOKEN_IDENTIFIER);
    } else if (isdigit(c)) {
        int i = 0;
        tok.text[i++] = c;

        // Hexadecimal literal? 0xFF style
        if (c == '0' && (peek(file) == 'x' || peek(file) == 'X')) {
            // Consume the 'x' or 'X'
            c = fgetc(file);
            tok.text[i++] = c;
            // Read hex digits
            while ((c = fgetc(file)) != EOF && isxdigit(c)) {
                if (i < (int)sizeof(tok.text) - 1) tok.text[i++] = c;
            }
        } else {
            // Decimal / float literal path
            while ((c = fgetc(file)) != EOF && isdigit(c)) {
                if (i < (int)sizeof(tok.text) - 1) tok.text[i++] = c;
            }
            // Check for decimal point
            if (c == '.') {
                if (i < (int)sizeof(tok.text) - 1) tok.text[i++] = c;
                while ((c = fgetc(file)) != EOF && isdigit(c)) {
                    if (i < (int)sizeof(tok.text) - 1) tok.text[i++] = c;
                }
            }
        }
        tok.text[i] = '\0';
        if (c != EOF) ungetc(c, file);
        tok.type = TOKEN_NUMBER;
    } else if (c == '"') {
        int i = 0;
        while ((c = fgetc(file)) != '"' && c != EOF) {
            if (i < (int)sizeof(tok.text) - 1) tok.text[i++] = c;
        }
        tok.text[i] = '\0';
        tok.type = TOKEN_STRING;
    } else if (is_operator_char(c)) {
        int i = 0;
        tok.text[i++] = c;
        if (is_operator_char(peek(file))) {
            tok.text[i++] = fgetc(file);
        }
        tok.text[i] = '\0';
        tok.type = TOKEN_OPERATOR;
    } else if (is_symbol(c)) {
        tok.text[0] = c;
        tok.text[1] = '\0';
        tok.type = TOKEN_SYMBOL;
    }

    return tok;
}

Token next_token_string(int *line, int *col) {
    skip_whitespace_string(line, col);

    Token tok = { TOKEN_EOF, "", *line, *col };
    int c = string_getc();
    if (c == EOF) return tok;

    if (isalpha(c) || c == '_') {
        int i = 0;
        tok.text[i++] = c;
        (*col)++;
        while ((c = string_getc()) != EOF && (isalnum(c) || c == '_')) {
            if (i < (int)sizeof(tok.text) - 1) tok.text[i++] = c;
            (*col)++;
        }
        tok.text[i] = '\0';
        if (c != EOF) string_ungetc();
        tok.type = is_keyword(tok.text) ? TOKEN_KEYWORD :
                  (strcmp(tok.text, "true") == 0 || strcmp(tok.text, "false") == 0 ? TOKEN_BOOL : TOKEN_IDENTIFIER);
    } else if (isdigit(c)) {
        int i = 0;
        tok.text[i++] = c;
        (*col)++;

        // Hexadecimal literal? 0xFF style
        if (c == '0' && (peek_string() == 'x' || peek_string() == 'X')) {
            // Consume the 'x' or 'X'
            c = string_getc();
            tok.text[i++] = c;
            (*col)++;
            // Read hex digits
            while ((c = string_getc()) != EOF && isxdigit(c)) {
                if (i < (int)sizeof(tok.text) - 1) tok.text[i++] = c;
                (*col)++;
            }
            tok.text[i] = '\0';
            tok.type = TOKEN_NUMBER;
        } else {
            // Decimal / float literal path
            while ((c = string_getc()) != EOF && isdigit(c)) {
                if (i < (int)sizeof(tok.text) - 1) tok.text[i++] = c;
            }
            // Check for decimal point
            if (c == '.') {
                if (i < (int)sizeof(tok.text) - 1) tok.text[i++] = c;
                (*col)++;
                while ((c = string_getc()) != EOF && isdigit(c)) {
                    if (i < (int)sizeof(tok.text) - 1) tok.text[i++] = c;
                    (*col)++;
                }
            }
        }
        tok.text[i] = '\0';
        if (c != EOF) string_ungetc();
        tok.type = TOKEN_NUMBER;
    } else if (c == '"') {
        int i = 0;
        (*col)++; // For opening quote
        while ((c = string_getc()) != EOF && c != '"') {
            if (c == '\n') {
                (*line)++;
                *col = 1;
            } else {
                (*col)++;
            }
            if (c == '\\' && peek_string() != EOF) {
                // Handle escape sequences
                c = string_getc();
                (*col)++;
                switch (c) {
                    case 'n': c = '\n'; break;
                    case 't': c = '\t'; break;
                    case 'r': c = '\r'; break;
                    case '\\': c = '\\'; break;
                    case '"': c = '"'; break;
                    default: break;
                }
            }
            if (i < (int)sizeof(tok.text) - 1) tok.text[i++] = c;
        }
        tok.text[i] = '\0';
        if (c == '"') (*col)++; // For closing quote
        tok.type = TOKEN_STRING;
    } else if (is_operator_char(c)) {
        int i = 0;
        tok.text[i++] = c;
        (*col)++;
        if (is_operator_char(peek_string())) {
            tok.text[i++] = string_getc();
            (*col)++;
        }
        tok.text[i] = '\0';
        tok.type = TOKEN_OPERATOR;
    } else if (is_symbol(c)) {
        tok.text[0] = c;
        tok.text[1] = '\0';
        (*col)++;
        tok.type = TOKEN_SYMBOL;
    } else {
        // Unknown character - add as a symbol to avoid infinite loop
        tok.text[0] = c;
        tok.text[1] = '\0';
        (*col)++;
        tok.type = TOKEN_SYMBOL;
        fprintf(stderr, "Warning: Unknown character '%c' at line %d, col %d\n", c, *line, *col);
    }

    return tok;
}

// Function to lex a source string and return array of tokens
Token* lex(const char* source) {
    // Store the source string globally
    source_string = source;
    string_pos = 0;
    
    // Allocate initial token array
    int capacity = 128;
    Token* tokens = (Token*)malloc(capacity * sizeof(Token));
    if (!tokens) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        return NULL;
    }
    
    // Lex the source
    int line = 1, col = 1;
    int count = 0;
    
    while (1) {
        // Resize array if needed
        if (count >= capacity) {
            capacity *= 2;
            tokens = (Token*)realloc(tokens, capacity * sizeof(Token));
            if (!tokens) {
                fprintf(stderr, "Error: Memory allocation failed\n");
                return NULL;
            }
        }
        
        // Get next token from string
        tokens[count] = next_token_string(&line, &col);
        
        // Stop when we reach EOF
        if (tokens[count].type == TOKEN_EOF) {
            count++; // Include EOF token
            break;
        }
        
        count++;
    }
    
    // Add a sentinel NULL token at the end
    tokens[count].type = TOKEN_EOF;
    tokens[count].text[0] = '\0';
    tokens[count].line = line;
    tokens[count].col = col;
    
    return tokens;
}
