#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast_types.h"

// Function to create a new AST node
ASTNode* create_node(ASTNodeType type, const char* value) {
    ASTNode* node = (ASTNode*)malloc(sizeof(ASTNode));
    if (!node) {
        fprintf(stderr, "Memory allocation failed\n");
        return NULL;
    }
    
    node->type = type;
    strncpy(node->value, value ? value : "", sizeof(node->value) - 1);
    node->value[sizeof(node->value) - 1] = '\0';
    
    memset(node->data_type, 0, sizeof(node->data_type));
    memset(node->generic_type, 0, sizeof(node->generic_type));
    node->is_void = 0;
    node->is_array = 0;
    node->array_size = 0;
    
    node->left = NULL;
    node->right = NULL;
    node->next = NULL;
    
    return node;
}

// Function to print indentation
static void print_indent(int indent) {
    for (int i = 0; i < indent; i++) {
        printf("  ");
    }
}

// Convert node type to string representation
const char *node_type_to_string(ASTNodeType type) {
    switch (type) {
        case AST_PROGRAM: return "Program";
        case AST_FUNCTION: return "Function";
        case AST_CLASS: return "Class";
        case AST_VAR_DECL: return "VarDecl";
        case AST_ASSIGN: return "Assign";
        case AST_RETURN: return "Return";
        case AST_IF: return "If";
        case AST_ELSE: return "Else";
        case AST_WHILE: return "While";
        case AST_FOR: return "For";
        case AST_BLOCK: return "Block";
        case AST_CALL: return "Call";
        case AST_BINARY_OP: return "BinaryOp";
        case AST_UNARY_OP: return "UnaryOp";
        case AST_LITERAL: return "Literal";
        case AST_IDENTIFIER: return "Identifier";
        case AST_ARRAY: return "Array";
        case AST_IMPORT: return "Import";
        case AST_STRUCT: return "Struct";
        case AST_STRUCT_INIT: return "StructInit";
        case AST_CLASS_METHOD: return "ClassMethod";
        case AST_NEW: return "New";
        case AST_MEMBER_ACCESS: return "MemberAccess";
        case AST_THIS: return "This";
        case AST_GENERIC: return "Generic";
        case AST_TYPED_VAR_DECL: return "TypedVarDecl";
        case AST_TYPED_FUNCTION: return "TypedFunction";
        case AST_TYPE: return "Type";
        case AST_PARAMETER: return "Parameter";
        case AST_STRUCT_FIELD: return "StructField";
        case AST_CLASS_FIELD: return "ClassField";
        case AST_PRINT: return "Print";
        case AST_INDEX_ACCESS: return "IndexAccess";
        case AST_UNKNOWN: return "Unknown";
        default: return "Unknown";
    }
}

// Function to print AST
void print_ast(ASTNode *node, int indent) {
    if (!node) return;
    
    print_indent(indent);
    printf("%s: %s", node_type_to_string(node->type), node->value);
    
    // Print type information if available
    if (node->data_type[0] != '\0') {
        printf(" (Type: %s", node->data_type);
        
        if (node->generic_type[0] != '\0') {
            printf("<%s>", node->generic_type);
        }
        
        if (node->is_void) {
            printf(", void");
        }
        
        printf(")");
    } else if (node->generic_type[0] != '\0') {
        printf(" (Generic: %s)", node->generic_type);
    } else if (node->is_void) {
        printf(" (void)");
    }
    
    printf("\n");
    
    if (node->left) {
        print_indent(indent);
        printf("Left:\n");
        print_ast(node->left, indent + 1);
    }
    
    if (node->right) {
        print_indent(indent);
        printf("Right:\n");
        print_ast(node->right, indent + 1);
    }
    
    if (node->next) {
        print_indent(indent);
        printf("Next:\n");
        print_ast(node->next, indent + 1);
    }
}

// Function to free AST
void free_ast(ASTNode *node) {
    if (!node) return;
    
    // Free left subtree
    if (node->left) {
        free_ast(node->left);
    }
    
    // Free right subtree
    if (node->right) {
        free_ast(node->right);
    }
    
    // Free next node
    if (node->next) {
        free_ast(node->next);
    }
    
    // Free the node itself
    free(node);
}
