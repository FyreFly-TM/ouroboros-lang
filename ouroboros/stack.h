#ifndef STACK_H
#define STACK_H

// Maximum number of variables in a stack frame
#define MAX_VARIABLES 64

// Variable structure
typedef struct {
    char name[128];
    char value[1024];
} Variable;

// Stack frame structure
typedef struct StackFrame {
    char name[128];
    // Name of the function this frame represents (for stack traces, access control, etc.)
    char function_name[128];
    Variable variables[MAX_VARIABLES];
    int var_count;
    // Pointer to the return value string allocated by the VM when the function sets it.
    char *return_value;
    struct StackFrame *parent;
} StackFrame;

// Stack frame functions
StackFrame* create_stack_frame(const char* name, StackFrame *parent);
void destroy_stack_frame(StackFrame *frame);
void set_variable(StackFrame *frame, const char *name, const char *value);
const char* get_variable(StackFrame *frame, const char *name);

#endif // STACK_H 