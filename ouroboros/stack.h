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
    Variable variables[MAX_VARIABLES];
    int var_count;
    struct StackFrame *parent;
} StackFrame;

// Stack frame functions
StackFrame* create_stack_frame(const char* name, StackFrame *parent);
void destroy_stack_frame(StackFrame *frame);
void set_variable(StackFrame *frame, const char *name, const char *value);
const char* get_variable(StackFrame *frame, const char *name);

#endif // STACK_H 