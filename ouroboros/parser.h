#ifndef PARSER_H
#define PARSER_H

#include <stdio.h>
#include "lexer.h"
#include "ast_types.h"

// Functions
ASTNode* parse_program(FILE *file);
ASTNode* parse(Token *tokens);

#endif // PARSER_H
