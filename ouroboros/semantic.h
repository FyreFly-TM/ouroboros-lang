#ifndef SEMANTIC_H
#define SEMANTIC_H

#include "ast_types.h"

void analyze_program(ASTNode *ast);
void check_semantics(ASTNode *ast);

#endif // SEMANTIC_H
