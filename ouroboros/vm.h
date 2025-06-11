#ifndef VM_H
#define VM_H

#include "ast_types.h"
#include "stack.h"

// Property access modifiers
typedef enum {
    ACCESS_PUBLIC,
    ACCESS_PRIVATE,
    ACCESS_STATIC
} AccessModifier;

// Object property structure
typedef struct ObjectProperty {
    char name[128];
    char value[1024];
    AccessModifier access;
    int is_static;
    struct ObjectProperty *next;
} ObjectProperty;

// Object structure
typedef struct Object {
    char class_name[128];
    ObjectProperty *properties;
    struct Object *next;
} Object;

// C function type
typedef void (*CFunction)();

// External globals
extern Object *objects;

// VM initialization and cleanup
void vm_init();
void vm_cleanup();

// Function registration
void register_c_function(const char *name, CFunction func);
CFunction lookup_c_function(const char *name);
void register_user_function(ASTNode *func);
ASTNode* find_user_function(const char *name);

// Object operations
Object* create_object(const char* class_name);
void set_object_property(Object *obj, const char *name, const char *value);
void set_object_property_with_access(Object *obj, const char *name, const char *value, AccessModifier access, int is_static);
const char* get_object_property(Object *obj, const char *name);
const char* get_object_property_with_access_check(Object *obj, const char *name, const char *accessing_class);
const char* get_static_property(const char *class_name, const char *prop_name);
void free_object(Object *obj);

// VM execution
const char* execute_function_call(const char *name, ASTNode *args, StackFrame *frame);
void run_vm_node(ASTNode *node, StackFrame *frame);
void run_vm(ASTNode *root);

// Return value handling
const char* get_return_value();
void set_return_value(const char* value);

// Function declarations
void init_vm();
void run_vm(ASTNode *program_node);
void run_vm_node(ASTNode *node, StackFrame *frame);
StackFrame* create_stack_frame(const char *function_name, StackFrame *parent);
void destroy_stack_frame(StackFrame *frame);
void set_variable(StackFrame *frame, const char *name, const char *value);
const char* get_variable(StackFrame *frame, const char *name);
void set_return_value(const char *value);
const char* get_return_value();
void print_stack_trace();
Object* create_object(const char *class_name);
void set_object_property_with_access(Object *obj, const char *name, const char *value, AccessModifier access, int is_static);
const char* get_object_property_with_access_check(Object *obj, const char *name, const char *accessing_class);
const char* execute_function_call(const char *name, ASTNode *args, StackFrame *frame);
Object* find_object_by_id(int id);
Object* find_static_class_object(const char *class_name);
const char* evaluate_member_access(ASTNode *expr, StackFrame *frame);
const char* call_built_in_function(const char* func_name, ASTNode* args, StackFrame* frame);

// Object management functions
Object* create_object(const char *class_name);
Object* find_object_by_id(int id);
Object* find_static_class_object(const char *class_name);
void initialize_test_class(Object *obj);
const char* get_object_property_with_access(Object *obj, const char *property_name, const char *current_class_context);
void set_object_property_with_access(Object *obj, const char *property_name, const char *value, AccessModifier access, int is_static);

#endif // VM_H
