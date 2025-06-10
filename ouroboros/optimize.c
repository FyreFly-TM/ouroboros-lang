#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "optimize.h"
#include "parser.h"

void constant_fold(ASTNode *node) {
    if (!node) return;

    if (node->type == AST_BINARY_OP) {
        constant_fold(node->left);
        constant_fold(node->right);

        if (node->left && node->right &&
            node->left->type == AST_LITERAL &&
            node->right->type == AST_LITERAL) {

            int l = atoi(node->left->value);
            int r = atoi(node->right->value);
            int result = 0;

            if (strcmp(node->value, "+") == 0) result = l + r;
            else if (strcmp(node->value, "-") == 0) result = l - r;
            else if (strcmp(node->value, "*") == 0) result = l * r;
            else if (strcmp(node->value, "/") == 0 && r != 0) result = l / r;

            snprintf(node->value, sizeof(node->value), "%d", result);
            node->type = AST_LITERAL;
            node->left = node->right = NULL;

            printf("[OPT] Folded constant: %d\n", result);
        }
    }

    constant_fold(node->left);
    constant_fold(node->right);
    constant_fold(node->next);
}

void optimize_ast(ASTNode *root) {
    if (!root) return;
    printf("[OPT] Optimizing AST: %s\n", root->value);
    constant_fold(root->left);
    optimize_ast(root->left);
    optimize_ast(root->right);
    optimize_ast(root->next);
}
