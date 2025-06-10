#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "semantic.h"
#include "ast_types.h"

// Forward declarations
static void analyze_node(ASTNode *node);
static void analyze_function(ASTNode *node);
static void analyze_block(ASTNode *node);
static void analyze_var_decl(ASTNode *node);
static void analyze_typed_var_decl(ASTNode *node);
static void analyze_assignment(ASTNode *node);
static void analyze_return(ASTNode *node);
static void analyze_if(ASTNode *node);
static void analyze_while(ASTNode *node);
static void analyze_for(ASTNode *node);
static void analyze_call(ASTNode *node);
static void analyze_expression(ASTNode *node);
static void analyze_struct(ASTNode *node);
static void analyze_class(ASTNode *node);
static void analyze_class_method(ASTNode *node);
static void analyze_typed_function(ASTNode *node);
static void analyze_member_access(ASTNode *node);
static void analyze_new(ASTNode *node);

// Function to check if a function is already defined
static int is_function_defined(const char *name, ASTNode *program) {
    ASTNode *node = program->left;
    while (node) {
        if (node->type == AST_FUNCTION && strcmp(node->value, name) == 0) {
            return 1;
        }
        node = node->next;
    }
    return 0;
}

// Function to check if a variable is defined in the current scope
static int is_variable_defined(const char *name, ASTNode *scope) {
    ASTNode *node = scope->left;
    while (node) {
        if ((node->type == AST_VAR_DECL || node->type == AST_ASSIGN) && 
            node->left && node->left->type == AST_IDENTIFIER && 
            strcmp(node->left->value, name) == 0) {
    return 1;
        }
        node = node->next;
    }
    return 0;
}

// Function to analyze a function definition
static void analyze_function(ASTNode *func) {
    printf("[SEMANTIC] Analyzing function: %s\n", func->value);
    
    // Check parameters
    if (func->left) {
        ASTNode *param = func->left;
        printf("[SEMANTIC]   Parameters: ");
        while (param) {
            printf("%s ", param->value);
            param = param->next;
        }
        printf("\n");
    }
    
    // Analyze function body
    if (func->right && func->right->type == AST_BLOCK) {
        analyze_block(func->right);
    }
}

// Function to analyze a block of statements
static void analyze_block(ASTNode *block) {
    printf("[SEMANTIC] Analyzing block\n");
    
    if (!block || block->type != AST_BLOCK) {
        printf("[SEMANTIC] Warning: Expected block node\n");
        return;
    }
    
    // Analyze each statement in the block
    ASTNode *stmt = block->left;
    while (stmt) {
        analyze_node(stmt);
        stmt = stmt->next;
    }
}

// Function to analyze a variable declaration
static void analyze_var_decl(ASTNode *decl) {
    printf("[SEMANTIC] Analyzing variable declaration: %s\n", 
           decl->left ? decl->left->value : "unknown");
    
    // Check if the variable has an initializer
    if (decl->right) {
        printf("[SEMANTIC]   With initializer\n");
        analyze_expression(decl->right);
    } else {
        printf("[SEMANTIC]   Without initializer\n");
    }
}

// Function to analyze an assignment
static void analyze_assignment(ASTNode *assign) {
    printf("[SEMANTIC] Analyzing assignment to: %s\n",
           assign->left ? assign->left->value : "unknown");
    
    if (assign->right) {
        analyze_expression(assign->right);
    }
}

// Function to analyze a return statement
static void analyze_return(ASTNode *ret) {
    printf("[SEMANTIC] Analyzing return statement\n");
    
    if (ret->left) {
        analyze_expression(ret->left);
    }
}

// Function to analyze an if statement
static void analyze_if(ASTNode *if_stmt) {
    printf("[SEMANTIC] Analyzing if statement\n");
    
    // Analyze condition
    if (if_stmt->left) {
        printf("[SEMANTIC]   Condition:\n");
        analyze_expression(if_stmt->left);
    }
    
    // Analyze then branch
    if (if_stmt->right) {
        printf("[SEMANTIC]   Then branch:\n");
        analyze_node(if_stmt->right);
    }
    
    // Analyze else branch if present
    if (if_stmt->next && if_stmt->next->type == AST_ELSE) {
        printf("[SEMANTIC]   Else branch:\n");
        analyze_node(if_stmt->next->left);
    }
}

// Function to analyze a while statement
static void analyze_while(ASTNode *while_stmt) {
    printf("[SEMANTIC] Analyzing while statement\n");
    
    // Analyze condition
    if (while_stmt->left) {
        printf("[SEMANTIC]   Condition:\n");
        analyze_expression(while_stmt->left);
    }
    
    // Analyze body
    if (while_stmt->right) {
        printf("[SEMANTIC]   Body:\n");
        analyze_node(while_stmt->right);
    }
}

// Function to analyze a for statement
static void analyze_for(ASTNode *for_stmt) {
    printf("[SEMANTIC] Analyzing for statement\n");
    
    // Analyze initialization
    if (for_stmt->left) {
        printf("[SEMANTIC]   Initialization:\n");
        analyze_node(for_stmt->left);
    }
    
    // Analyze condition and increment
    if (for_stmt->right && for_stmt->right->type == AST_BLOCK) {
        if (for_stmt->right->left) {
            printf("[SEMANTIC]   Condition:\n");
            analyze_expression(for_stmt->right->left);
        }
        
        if (for_stmt->right->right) {
            printf("[SEMANTIC]   Increment:\n");
            analyze_node(for_stmt->right->right);
        }
    }
    
    // Analyze body
    if (for_stmt->next) {
        printf("[SEMANTIC]   Body:\n");
        analyze_node(for_stmt->next);
    }
}

// Function to analyze a function call
static void analyze_call(ASTNode *call) {
    printf("[SEMANTIC] Analyzing call to: %s\n", call->value);
    
    // Analyze arguments
    if (call->left) {
        ASTNode *arg = call->left;
        int arg_count = 0;
        
        printf("[SEMANTIC]   Arguments:\n");
        while (arg) {
            arg_count++;
            analyze_expression(arg);
            arg = arg->next;
        }
        
        printf("[SEMANTIC]   Total arguments: %d\n", arg_count);
    } else {
        printf("[SEMANTIC]   No arguments\n");
    }
}

// Function to analyze an expression
static void analyze_expression(ASTNode *expr) {
    if (!expr) return;
    
    switch (expr->type) {
        case AST_LITERAL:
            printf("[SEMANTIC] Literal: %s\n", expr->value);
            break;
            
        case AST_IDENTIFIER:
            printf("[SEMANTIC] Identifier: %s\n", expr->value);
            break;
            
        case AST_BINARY_OP:
            printf("[SEMANTIC] Binary operation: %s\n", expr->value);
            analyze_expression(expr->left);
            analyze_expression(expr->right);
            break;
            
        case AST_UNARY_OP:
            printf("[SEMANTIC] Unary operation: %s\n", expr->value);
            analyze_expression(expr->left);
            break;
            
        case AST_CALL:
            analyze_call(expr);
            break;
            
        case AST_ARRAY:
            printf("[SEMANTIC] Array literal\n");
            if (expr->left) {
                ASTNode *item = expr->left;
                int item_count = 0;
                
                while (item) {
                    item_count++;
                    analyze_expression(item);
                    item = item->next;
                }
                
                printf("[SEMANTIC]   Total items: %d\n", item_count);
            } else {
                printf("[SEMANTIC]   Empty array\n");
            }
            break;
            
        default:
            printf("[SEMANTIC] Unknown expression type: %d\n", expr->type);
            break;
    }
}

// Function to analyze any AST node based on its type
static void analyze_node(ASTNode *node) {
    if (!node) return;
    
    switch (node->type) {
        case AST_PROGRAM:
            printf("[SEMANTIC] Analyzing program\n");
            {
                ASTNode *child = node->left;
                while (child) {
                    analyze_node(child);
                    child = child->next;
                }
            }
            break;
            
        case AST_FUNCTION:
            analyze_function(node);
            break;
            
        case AST_TYPED_FUNCTION:
            analyze_typed_function(node);
            break;
            
        case AST_BLOCK:
            analyze_block(node);
            break;
            
        case AST_VAR_DECL:
            analyze_var_decl(node);
            break;
            
        case AST_TYPED_VAR_DECL:
            analyze_typed_var_decl(node);
            break;
            
        case AST_ASSIGN:
            analyze_assignment(node);
            break;
            
        case AST_RETURN:
            analyze_return(node);
            break;
            
        case AST_IF:
            analyze_if(node);
            break;
            
        case AST_WHILE:
            analyze_while(node);
            break;
            
        case AST_FOR:
            analyze_for(node);
            break;
            
        case AST_CALL:
            analyze_call(node);
            break;
            
        case AST_STRUCT:
            analyze_struct(node);
            break;
            
        case AST_CLASS:
            analyze_class(node);
            break;
            
        case AST_CLASS_METHOD:
            analyze_class_method(node);
            break;
            
        case AST_MEMBER_ACCESS:
            analyze_member_access(node);
            break;
            
        case AST_NEW:
            analyze_new(node);
            break;
            
        case AST_LITERAL:
        case AST_IDENTIFIER:
        case AST_BINARY_OP:
        case AST_UNARY_OP:
        case AST_ARRAY:
        case AST_THIS:
            analyze_expression(node);
            break;
            
        default:
            printf("[SEMANTIC] Unknown node type: %d\n", node->type);
            break;
    }
}

// Function to analyze the AST
void analyze_program(ASTNode *ast) {
    if (!ast) {
        fprintf(stderr, "[SEMANTIC] Error: NULL AST for analysis\n");
        return;
    }
    
    if (ast->type != AST_PROGRAM) {
        fprintf(stderr, "[SEMANTIC] Error: Expected program node, got type %d\n", ast->type);
        return;
    }
    
    printf("[SEMANTIC] Starting semantic analysis...\n");
    
    // Analyze the program
    analyze_node(ast);
    
    printf("[SEMANTIC] Semantic analysis complete\n");
}

// Function to perform semantic checks and generate warnings/errors
void check_semantics(ASTNode *ast) {
    if (!ast || ast->type != AST_PROGRAM) {
        fprintf(stderr, "[SEMANTIC] Error: Invalid AST for semantic checks\n");
        return;
    }
    
    // Check for duplicate function definitions
    ASTNode *func1 = ast->left;
    while (func1) {
        if (func1->type == AST_FUNCTION) {
            ASTNode *func2 = func1->next;
            while (func2) {
                if (func2->type == AST_FUNCTION && strcmp(func1->value, func2->value) == 0) {
                    fprintf(stderr, "[SEMANTIC] Warning: Duplicate function definition: %s\n", func1->value);
                }
                func2 = func2->next;
            }
        }
        func1 = func1->next;
    }
}

static void analyze_typed_var_decl(ASTNode *node) {
    printf("[SEMANTIC] Analyzing typed variable declaration: %s (Type: %s)\n", 
           node->value, node->data_type);
    
    if (node->right) {
        analyze_expression(node->right);
    }
}

static void analyze_typed_function(ASTNode *node) {
    printf("[SEMANTIC] Analyzing typed function: %s (Return type: %s)\n", 
           node->value, node->data_type);
    
    // Analyze parameters
    if (node->left) {
        ASTNode *param = node->left;
        while (param) {
            if (param->type == AST_TYPED_VAR_DECL) {
                printf("[SEMANTIC] Parameter: %s (Type: %s)\n", 
                       param->value, param->data_type);
            } else {
                printf("[SEMANTIC] Parameter: %s\n", param->value);
            }
            param = param->next;
        }
    }
    
    // Analyze function body
    if (node->right) {
        analyze_block(node->right);
    }
}

static void analyze_struct(ASTNode *node) {
    printf("[SEMANTIC] Analyzing struct: %s\n", node->value);
    
    // Analyze fields
    if (node->left) {
        ASTNode *field = node->left;
        while (field) {
            if (field->type == AST_TYPED_VAR_DECL) {
                printf("[SEMANTIC] Field: %s (Type: %s)\n", 
                       field->value, field->data_type);
            }
            field = field->next;
        }
    }
}

static void analyze_class(ASTNode *node) {
    printf("[SEMANTIC] Analyzing class: %s", node->value);
    
    if (node->generic_type[0] != '\0') {
        printf(" (Generic: %s)", node->generic_type);
    }
    
    if (node->right) {
        printf(" extends %s", node->right->value);
    }
    
    printf("\n");
    
    // Analyze class members
    if (node->left) {
        ASTNode *member = node->left;
        while (member) {
            if (member->type == AST_TYPED_VAR_DECL) {
                printf("[SEMANTIC] Field: %s (Type: %s)\n", 
                       member->value, member->data_type);
            } else if (member->type == AST_CLASS_METHOD) {
                analyze_class_method(member);
            }
            member = member->next;
        }
    }
}

static void analyze_class_method(ASTNode *node) {
    printf("[SEMANTIC] Analyzing class method: %s", node->value);
    
    if (node->data_type[0] != '\0') {
        printf(" (Return type: %s)", node->data_type);
    }
    
    printf("\n");
    
    // Analyze parameters
    if (node->left) {
        ASTNode *param = node->left;
        while (param) {
            if (param->type == AST_TYPED_VAR_DECL) {
                printf("[SEMANTIC] Parameter: %s (Type: %s)\n", 
                       param->value, param->data_type);
            } else {
                printf("[SEMANTIC] Parameter: %s\n", param->value);
            }
            param = param->next;
        }
    }
    
    // Analyze method body
    if (node->right) {
        analyze_block(node->right);
    }
}

static void analyze_member_access(ASTNode *node) {
    printf("[SEMANTIC] Analyzing member access: %s\n", node->value);
    
    // Analyze the object
    if (node->left) {
        analyze_expression(node->left);
    }
    
    // Analyze method arguments if this is a method call
    if (node->right) {
        ASTNode *arg = node->right;
        while (arg) {
            analyze_expression(arg);
            arg = arg->next;
        }
    }
}

static void analyze_new(ASTNode *node) {
    printf("[SEMANTIC] Analyzing new expression: %s", node->value);
    
    if (node->generic_type[0] != '\0') {
        printf("<%s>", node->generic_type);
    }
    
    printf("\n");
    
    // Analyze constructor arguments
    if (node->left) {
        ASTNode *arg = node->left;
        while (arg) {
            analyze_expression(arg);
            arg = arg->next;
        }
    }
}
