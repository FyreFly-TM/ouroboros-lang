#ifndef VM_H
#define VM_H

#include "ast_types.h"
#include "stack.h"

// C function type
typedef void (*CFunction)();

// VM initialization and cleanup
void vm_init();
void vm_cleanup();

// Function registration
void register_c_function(const char *name, CFunction func);
CFunction lookup_c_function(const char *name);
void register_user_function(ASTNode *func);
ASTNode* find_user_function(const char *name);

// VM execution
const char* execute_function_call(const char *name, ASTNode *args, StackFrame *frame);
void run_vm_node(ASTNode *node, StackFrame *frame);
void run_vm(ASTNode *root);

// Return value handling
const char* get_return_value();
void set_return_value(const char* value);

#endif // VM_H
