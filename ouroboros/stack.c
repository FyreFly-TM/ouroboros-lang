#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "stack.h"

// Create a new stack frame with the given name and parent
StackFrame* create_stack_frame(const char* name, StackFrame* parent) {
    StackFrame* frame = (StackFrame*)malloc(sizeof(StackFrame));
    if (!frame) {
        fprintf(stderr, "Error: Memory allocation failed for stack frame\n");
        return NULL;
    }
    
    strncpy(frame->name, name, sizeof(frame->name) - 1);
    frame->name[sizeof(frame->name) - 1] = '\0';
    
    // Store the function name for debugging / access control
    strncpy(frame->function_name, name, sizeof(frame->function_name) - 1);
    frame->function_name[sizeof(frame->function_name) - 1] = '\0';
    
    // Initialize return value pointer
    frame->return_value = NULL;
    
    frame->parent = parent;
    frame->var_count = 0;
    
    return frame;
}

// Destroy a stack frame and free memory
void destroy_stack_frame(StackFrame* frame) {
    if (frame) {
        if (frame->return_value) {
            free(frame->return_value);
            frame->return_value = NULL;
        }
        free(frame);
    }
}

// Set a variable in the stack frame
void set_variable(StackFrame* frame, const char* name, const char* value) {
    if (!frame || !name || !value) return;
    
    // Check if the variable already exists
    for (int i = 0; i < frame->var_count; i++) {
        if (strcmp(frame->variables[i].name, name) == 0) {
            strncpy(frame->variables[i].value, value, sizeof(frame->variables[i].value) - 1);
            frame->variables[i].value[sizeof(frame->variables[i].value) - 1] = '\0';
            return;
        }
    }
    
    // If not found and there's space, add a new variable
    if (frame->var_count < MAX_VARIABLES) {
        strncpy(frame->variables[frame->var_count].name, name, sizeof(frame->variables[frame->var_count].name) - 1);
        frame->variables[frame->var_count].name[sizeof(frame->variables[frame->var_count].name) - 1] = '\0';
        
        strncpy(frame->variables[frame->var_count].value, value, sizeof(frame->variables[frame->var_count].value) - 1);
        frame->variables[frame->var_count].value[sizeof(frame->variables[frame->var_count].value) - 1] = '\0';
        
        frame->var_count++;
    } else {
        fprintf(stderr, "Error: Stack frame variable limit reached\n");
    }
}

// Get a variable's value from the stack frame, searching parent frames if needed
const char* get_variable(StackFrame* frame, const char* name) {
    if (!frame || !name) return NULL;
    
    // Search in current frame
    for (int i = 0; i < frame->var_count; i++) {
        if (strcmp(frame->variables[i].name, name) == 0) {
            return frame->variables[i].value;
        }
    }
    
    // If not found and there's a parent frame, search there
    if (frame->parent) {
        return get_variable(frame->parent, name);
    }
    
    // Not found
    return NULL;
} 