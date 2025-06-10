#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vm.h"
#include "ast_types.h"
#include "stack.h"
#include "eval.h"
#include "stdlib.h"
#include "module.h"

// Structure to hold registered functions
typedef struct FunctionEntry {
    ASTNode *func;
    struct FunctionEntry *next;
} FunctionEntry;

// Keep track of registered user functions
static ASTNode *user_functions = NULL;

// Create a global stack frame for the VM
static StackFrame *global_frame = NULL;

// Store the last return value
static char return_value[1024] = "0";

// Registered functions list
static FunctionEntry *registered_functions = NULL;

// Get the last return value
const char* get_return_value() {
    return return_value;
}

// Set the return value
void set_return_value(const char* value) {
    if (value) {
        strncpy(return_value, value, sizeof(return_value) - 1);
        return_value[sizeof(return_value) - 1] = '\0';
    } else {
        strcpy(return_value, "0");
    }
}

// Initialize the VM
void vm_init() {
    user_functions = NULL;
    global_frame = create_stack_frame("global", NULL);
}

// Clean up the VM
void vm_cleanup() {
    destroy_stack_frame(global_frame);
    global_frame = NULL;
    
    // Free registered functions list
    FunctionEntry *entry = registered_functions;
    while (entry) {
        FunctionEntry *next = entry->next;
        free(entry);
        entry = next;
    }
    registered_functions = NULL;
}

// Register a C function
void register_c_function(const char *name, CFunction func) {
    if (!global_frame) {
        vm_init();
    }
    
    // Store function pointer as a string representation
    char value[32];
    snprintf(value, sizeof(value), "%p", (void*)func);
    set_variable(global_frame, name, value);
}

// Look up a C function
CFunction lookup_c_function(const char *name) {
    if (!global_frame) return NULL;
    
    const char *value = get_variable(global_frame, name);
    if (!value) return NULL;
    
    // Convert string back to function pointer
    void *ptr;
    sscanf(value, "%p", &ptr);
    return (CFunction)ptr;
}

// Register a user-defined function
void register_user_function(ASTNode *func) {
    if (!func || func->type != AST_FUNCTION) return;
    
    // Create a new entry
    FunctionEntry *entry = (FunctionEntry *)malloc(sizeof(FunctionEntry));
    if (!entry) {
        fprintf(stderr, "Error: Failed to allocate memory for function entry\n");
        return;
    }
    
    entry->func = func;
    entry->next = registered_functions;
    registered_functions = entry;
}

// Find a user-defined function by name
ASTNode* find_user_function(const char *name) {
    FunctionEntry *entry = registered_functions;
    while (entry) {
        if (entry->func && strcmp(entry->func->value, name) == 0) {
            return entry->func;
        }
        entry = entry->next;
    }
    return NULL;
}

// Execute a function call
const char* execute_function_call(const char *name, ASTNode *args, StackFrame *frame) {
    // Prepare arguments array for builtin functions
    const char *arg_values[32];  // Max 32 args
    int arg_count = 0;
    
    // Collect all arguments
    ASTNode *arg = args;
    while (arg && arg_count < 32) {
        const char *arg_value = NULL;
        if (arg->type == AST_CALL) {
            arg_value = execute_function_call(arg->value, arg->left, frame);
        } else {
            arg_value = evaluate_expression(arg, frame);
        }
        arg_values[arg_count++] = arg_value ? arg_value : "";
        arg = arg->next;
    }
    
    // Try built-in functions first (through stdlib)
    if (call_builtin_function(name, arg_values, arg_count)) {
        return "0"; // Most built-in functions return 0
    }
    
    // Check if it's a user-defined function
    ASTNode *func = find_user_function(name);
    if (func) {
        // Create a new stack frame for the function
        StackFrame *func_frame = create_stack_frame(name, frame);
        
        // Bind arguments to parameters
        if (func->left && args) {
            ASTNode *param = func->left;
            ASTNode *arg = args;
            
            while (param && arg) {
                const char *arg_value = NULL;
                if (arg->type == AST_CALL) {
                    // If the argument is a function call, execute it first
                    arg_value = execute_function_call(arg->value, arg->left, frame);
                } else {
                    arg_value = evaluate_expression(arg, frame);
                }
                set_variable(func_frame, param->value, arg_value ? arg_value : "undefined");
                
                param = param->next;
                arg = arg->next;
            }
        }
        
        // Clear return value before executing function
        set_return_value("0");
        
        // Execute the function body
        run_vm_node(func->right, func_frame);
        
        // Get the return value
        const char* result = get_return_value();
        
        // Clean up
        destroy_stack_frame(func_frame);
        
        return result;
    }
    
    // Function not found
    fprintf(stderr, "Error: Function '%s' not found\n", name);
    return "undefined";
}

// Execute a node
void run_vm_node(ASTNode *node, StackFrame *frame) {
    if (!node || !frame) return;
    
    switch (node->type) {
        case AST_PROGRAM: {
            // Execute all top-level statements
            ASTNode *stmt = node->left;
            while (stmt) {
                run_vm_node(stmt, frame);
                stmt = stmt->next;
            }
            break;
        }
            
        case AST_FUNCTION: {
            // Just register the function in this pass
            // This is handled in the first pass of run_vm
            break;
        }
            
        case AST_BLOCK: {
            // Execute all statements in the block
            ASTNode *stmt = node->left;
            while (stmt) {
                run_vm_node(stmt, frame);
                stmt = stmt->next;
            }
            break;
        }
            
        case AST_IMPORT: {
            // Load the module
            Module *module = module_load(node->value);
            if (module && module->ast) {
                // Register all functions from the imported module
                if (module->ast->type == AST_PROGRAM) {
                    ASTNode *stmt = module->ast->left;
                    while (stmt) {
                        if (stmt->type == AST_FUNCTION) {
                            register_user_function(stmt);
                        }
                        stmt = stmt->next;
                    }
                }
            }
            break;
        }
            
        case AST_VAR_DECL: {
            // Evaluate initializer if present
            const char *value = "undefined";
            if (node->right) {
                if (node->right->type == AST_CALL) {
                    // If the initializer is a function call, execute it
                    value = execute_function_call(node->right->value, node->right->left, frame);
                } else {
                    value = evaluate_expression(node->right, frame);
                }
            }
            
            // Set variable in the current frame
            set_variable(frame, node->left->value, value);
            break;
        }
            
        case AST_ASSIGN: {
            // Evaluate right-hand side
            const char *value = NULL;
            
            if (node->right->type == AST_CALL) {
                // If the right side is a function call, execute it
                value = execute_function_call(node->right->value, node->right->left, frame);
            } else {
                value = evaluate_expression(node->right, frame);
            }
            
            // Set variable
            set_variable(frame, node->left->value, value);
            break;
        }
            
        case AST_CALL: {
            // Execute function call
            execute_function_call(node->value, node->left, frame);
            break;
        }
            
        case AST_RETURN: {
            // Evaluate the return expression and set return value
            if (node->left) {
                const char *value = NULL;
                if (node->left->type == AST_CALL) {
                    value = execute_function_call(node->left->value, node->left->left, frame);
                } else {
                    value = evaluate_expression(node->left, frame);
                }
                set_return_value(value);
            } else {
                set_return_value("0");
            }
            break;
        }
            
        case AST_IF: {
            // Evaluate condition
            const char *condition = evaluate_expression(node->left, frame);
            
            // Check if condition is truthy
            if (condition && strcmp(condition, "0") != 0 && strcmp(condition, "") != 0) {
                // Execute then branch
                run_vm_node(node->right, frame);
            } else if (node->next && node->next->type == AST_ELSE) {
                // Execute else branch
                run_vm_node(node->next->left, frame);
            }
            break;
        }
            
        case AST_WHILE: {
            // Execute while loop
            while (1) {
                // Evaluate condition
                const char *condition = evaluate_expression(node->left, frame);
                
                // Check if condition is falsy
                if (!condition || strcmp(condition, "0") == 0 || strcmp(condition, "") == 0) {
                    break;
                }
                
                // Execute body
                run_vm_node(node->right, frame);
            }
            break;
        }
            
        default:
            // Other node types are handled by the evaluator
            break;
    }
}

// Execute the whole program
void run_vm(ASTNode *root) {
    // Initialize VM if needed
    if (!global_frame) {
        vm_init();
    }
    
    printf("\n==== Program Output ====\n");
    
    // First pass: process imports and register all functions before executing anything
    if (root && root->type == AST_PROGRAM) {
        ASTNode *node = root->left;
        while (node) {
            if (node->type == AST_FUNCTION) {
                register_user_function(node);
            } else if (node->type == AST_IMPORT) {
                // Process imports to load external modules
                Module *module = module_load(node->value);
                if (module && module->ast) {
                    // Register all functions from the imported module
                    if (module->ast->type == AST_PROGRAM) {
                        ASTNode *stmt = module->ast->left;
                        while (stmt) {
                            if (stmt->type == AST_FUNCTION) {
                                register_user_function(stmt);
                            }
                            stmt = stmt->next;
                        }
                    }
                }
            }
            node = node->next;
        }
    }
    
    // Second pass: execute main function if it exists
    ASTNode *main_func = find_user_function("main");
    if (main_func) {
        execute_function_call("main", NULL, global_frame);
    } else {
        // Execute any test function if main isn't found
        ASTNode *test_func = find_user_function("test");
        if (test_func) {
            printf("No main function found, executing test function:\n");
            execute_function_call("test", NULL, global_frame);
        } else {
            // Execute the entire program if no main or test function is found
            printf("No main or test function found, executing entire program:\n");
            run_vm_node(root, global_frame);
        }
    }
    
    // Clean up
    vm_cleanup();
}
