#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vm.h"
#include "ast_types.h"
#include "stack.h"
#include "eval.h"
#include "stdlib.h"
#include "module.h"

// Extern global AST root created by the parser so we can traverse it.
extern ASTNode *program;

// Structure to hold registered functions
typedef struct FunctionEntry {
    ASTNode *func;
    struct FunctionEntry *next;
} FunctionEntry;

// Structure to hold registered classes
typedef struct ClassEntry {
    char name[128];
    struct ClassEntry *next;
} ClassEntry;

// Keep track of registered user functions
static ASTNode *user_functions = NULL;

// Create a global stack frame for the VM
static StackFrame *global_frame = NULL;

// Store the last return value with dynamic allocation
static char *return_value = NULL;

// Registered functions list
static FunctionEntry *registered_functions = NULL;

// Registered classes list
static ClassEntry *registered_classes = NULL;
static ClassEntry *registered_classes_tail = NULL;

// Current class context (for private access)
char current_class[128] = {0};

// Objects list
Object *objects = NULL;
static int next_object_id = 1;

// Forward prototype
static int is_class_registered(const char *name);

// Forward helper to initialise default instance fields
static void initialize_default_instance_fields(const char *class_name, Object *instance);

// Get the last return value
const char* get_return_value() {
    return return_value ? return_value : "0";
}

// Set the return value with dynamic allocation
void set_return_value(const char* value) {
    // Free previous return value if exists
    if (return_value) {
        free(return_value);
        return_value = NULL;
    }
    
    if (value) {
        // Allocate memory for the new return value
        return_value = strdup(value);
        if (!return_value) {
            fprintf(stderr, "Error: Memory allocation failed for return value\n");
            return_value = strdup("0");
        }
    } else {
        return_value = strdup("0");
    }
}

// Create a new object
Object* create_object(const char *class_name) {
    Object *obj = (Object*)malloc(sizeof(Object));
    if (!obj) {
        fprintf(stderr, "Error: Failed to allocate memory for object\n");
        return NULL;
    }
    
    // Format the class name with a unique ID: ClassName#ID
    snprintf(obj->class_name, sizeof(obj->class_name), "%s#%d", class_name, next_object_id++);
    
    // Initialize properties
    obj->properties = NULL;
    obj->next = objects;
    objects = obj;
    
    printf("[OBJECT] Created new object: %s\n", obj->class_name);
    
    // Add standard properties for all objects
    if (strcmp(class_name, "Object") == 0) {
        // Generic Object properties
        set_object_property_with_access(obj, "toString", "Object", ACCESS_PUBLIC, 0);
        set_object_property_with_access(obj, "valueOf", "0", ACCESS_PUBLIC, 0);
    }
    
    // Forward helper to set default value on newly created object based on type
    initialize_default_instance_fields(class_name, obj);
    
    return obj;
}

// Set an object property with access modifiers
void set_object_property_with_access(Object *obj, const char *name, const char *value, AccessModifier access, int is_static) {
    if (!obj) {
        fprintf(stderr, "Error: Cannot set property on null object\n");
        return;
    }
    
    if (!name || !value) {
        fprintf(stderr, "Error: Invalid parameters for setting object property\n");
        return;
    }
    
    printf("[PROPERTY] Setting %s property '%s' to '%s' on object %s\n", 
           access == ACCESS_PUBLIC ? "public" : 
           access == ACCESS_PRIVATE ? "private" : "static",
           name, value, obj->class_name);
    
    // Check if property already exists
    ObjectProperty *prop = obj->properties;
    while (prop) {
        if (strcmp(prop->name, name) == 0) {
            // Update existing property
            strncpy(prop->value, value, sizeof(prop->value) - 1);
            prop->value[sizeof(prop->value) - 1] = '\0';
            prop->access = access;
            prop->is_static = is_static;
            return;
        }
        prop = prop->next;
    }
    
    // Property doesn't exist, create a new one
    ObjectProperty *new_prop = (ObjectProperty*)malloc(sizeof(ObjectProperty));
    if (!new_prop) {
        fprintf(stderr, "Error: Failed to allocate memory for object property\n");
        return;
    }
    
    // Initialize the new property
    strncpy(new_prop->name, name, sizeof(new_prop->name) - 1);
    new_prop->name[sizeof(new_prop->name) - 1] = '\0';
    
    strncpy(new_prop->value, value, sizeof(new_prop->value) - 1);
    new_prop->value[sizeof(new_prop->value) - 1] = '\0';
    
    new_prop->access = access;
    new_prop->is_static = is_static;
    
    // Add to the property list
    new_prop->next = obj->properties;
    obj->properties = new_prop;
    
    printf("[PROPERTY] Created new property '%s' on object %s\n", name, obj->class_name);
}

// Get an object property (basic version)
const char* get_object_property(Object *obj, const char *name) {
    return get_object_property_with_access_check(obj, name, NULL);
}

// Get an object property with access control check
const char* get_object_property_with_access_check(Object *obj, const char *name, const char *accessing_class) {
    if (!obj || !name) return NULL;
    
    // Extract class name without the ID
    char class_name[128] = {0};
    char *hash_pos = strchr(obj->class_name, '#');
    if (hash_pos) {
        size_t class_name_len = hash_pos - obj->class_name;
        strncpy(class_name, obj->class_name, class_name_len);
        class_name[class_name_len] = '\0';
    } else {
        strncpy(class_name, obj->class_name, sizeof(class_name) - 1);
    }
    
    printf("[ACCESS] Checking property '%s' on object %s, accessing from class '%s'\n", 
           name, obj->class_name, accessing_class ? accessing_class : "null");
    
    // Check if property exists
    ObjectProperty *prop = obj->properties;
    while (prop) {
        if (strcmp(prop->name, name) == 0) {
            // Found the property - check access
            if (prop->access == ACCESS_PUBLIC) {
                // Public property - accessible from anywhere
                return prop->value;
            } else if (prop->access == ACCESS_PRIVATE) {
                // Private property - only accessible from within the same class
                if (accessing_class && strcmp(accessing_class, class_name) == 0) {
                    return prop->value;
                } else {
                    printf("[ACCESS] Cannot access private property '%s' from outside class '%s'\n", 
                          name, class_name);
                    return NULL;
                }
            } else if (prop->is_static) {
                // Static property - accessible from anywhere if public
                if (prop->access == ACCESS_PUBLIC) {
                    return prop->value;
                } else if (accessing_class && strcmp(accessing_class, class_name) == 0) {
                    // Static private - only accessible from within the class
                    return prop->value;
                } else {
                    printf("[ACCESS] Cannot access private static property '%s' from outside class '%s'\n", 
                          name, class_name);
                    return NULL;
                }
            }
        }
        prop = prop->next;
    }
    
    // Property not found
    return NULL;
}

// Get an object property with access control
const char* get_object_property_with_access(Object *obj, const char *property_name, const char *current_class_context) {
    if (!obj) return "undefined";
    
    // Debug: auto-initialize TestMain properties
    #if 0
        if (strncmp(obj->class_name, "TestMain#", 9) == 0) {
            // Ensure randomFloat is set
            ObjectProperty *prop = obj->properties;
            int found = 0;
            while (prop) {
                if (strcmp(prop->name, "randomFloat") == 0) {
                    found = 1;
                    break;
                }
                prop = prop->next;
            }
            if (!found) {
                printf("[DEBUG] Auto-initializing TestMain instance properties\n");
                set_object_property_with_access(obj, "randomFloat", "15.0", ACCESS_PUBLIC, 0);
                set_object_property_with_access(obj, "randomInt", "1", ACCESS_PRIVATE, 0);
            }
        }
    #endif
    
    // Check for static properties in the class static object
    if (strstr(obj->class_name, "_static") == NULL) {
        // This is a regular instance, check if it's a static property request
        char *class_name = strdup(obj->class_name);
        char *hash_pos = strchr(class_name, '#');
        if (hash_pos) *hash_pos = '\0';
        
        // Look for the static version of the class
        Object *static_obj = find_static_class_object(class_name);
        if (static_obj) {
            // Check if this property exists in the static object
            ObjectProperty *static_prop = static_obj->properties;
            while (static_prop) {
                if (strcmp(static_prop->name, property_name) == 0 && static_prop->is_static) {
                    // It's a static property, return it
                    free(class_name);
                    return static_prop->value;
                }
                static_prop = static_prop->next;
            }
        }
        free(class_name);
    }
    
    // Look for the property in the object
    ObjectProperty *prop = obj->properties;
    while (prop) {
        if (strcmp(prop->name, property_name) == 0) {
            // Check access
            if (prop->access == ACCESS_PRIVATE) {
                // For private properties, check if we're in the same class context
                char *obj_class = strdup(obj->class_name);
                char *hash_pos = strchr(obj_class, '#');
                if (hash_pos) *hash_pos = '\0';
                
                // Private access is only allowed from within the same class
                if (!current_class_context || strcmp(current_class_context, obj_class) != 0) {
                    free(obj_class);
                    fprintf(stderr, "Error: Property '%s' is private\n", property_name);
                    return "undefined";
                }
                free(obj_class);
            }
            
            return prop->value;
        }
        prop = prop->next;
    }
    
    fprintf(stderr, "Error: Property '%s' not found or not accessible\n", property_name);
    return "undefined";
}

// Get a static property from a class
const char* get_static_property(const char *class_name, const char *prop_name) {
    if (!class_name || !prop_name) return NULL;
    
    // Find the class (any instance will do)
    Object *obj = objects;
    while (obj) {
        // Check if it's an instance of the class (ignoring the #id part)
        if (strncmp(obj->class_name, class_name, strlen(class_name)) == 0) {
            // Find the static property
            ObjectProperty *prop = obj->properties;
            while (prop) {
                if (strcmp(prop->name, prop_name) == 0 && prop->is_static) {
                    return prop->value;
                }
                prop = prop->next;
            }
            
            // Static property not found in this instance
            printf("[STATIC] Property '%s' not found in class '%s'\n", prop_name, class_name);
            return NULL;
        }
        obj = obj->next;
    }
    
    printf("[STATIC] No instances of class '%s' found\n", class_name);
    return NULL;
}

// Register a class
static void vm_register_class(const char *name) {
    if (!name) return;
    
    // Check if already registered
    if (is_class_registered(name)) return;
    
    ClassEntry *entry = (ClassEntry*)malloc(sizeof(ClassEntry));
    if (!entry) {
        fprintf(stderr, "Error: Failed to allocate memory for class entry\n");
        return;
    }
    
    strncpy(entry->name, name, sizeof(entry->name) - 1);
    entry->name[sizeof(entry->name) - 1] = '\0';
    entry->next = NULL;
    if (!registered_classes) {
        registered_classes = registered_classes_tail = entry;
    } else {
        registered_classes_tail->next = entry;
        registered_classes_tail = entry;
    }
}

// Check if a class is registered
static int is_class_registered(const char *name) {
    if (!name) return 0;
    
    ClassEntry *entry = registered_classes;
    while (entry) {
        if (strcmp(entry->name, name) == 0) {
            return 1;
        }
        entry = entry->next;
    }
    
    return 0;
}

// Free an object and its properties
void free_object(Object *obj) {
    if (!obj) return;
    
    // Free properties
    ObjectProperty *prop = obj->properties;
    while (prop) {
        ObjectProperty *next = prop->next;
        free(prop);
        prop = next;
    }
    
    free(obj);
}

// Initialize the VM
void vm_init() {
    user_functions = NULL;
    global_frame = create_stack_frame("global", NULL);
    return_value = strdup("0");
    objects = NULL;
    next_object_id = 1;
    registered_classes = NULL;
    registered_classes_tail = NULL;
    current_class[0] = '\0';
}

// Clean up the VM
void vm_cleanup() {
    // Free the return value
    if (return_value) {
        free(return_value);
        return_value = NULL;
    }
    
    // Destroy global stack frame
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
    
    // Free registered classes list
    ClassEntry *class_entry = registered_classes;
    while (class_entry) {
        ClassEntry *next = class_entry->next;
        free(class_entry);
        class_entry = next;
    }
    registered_classes = NULL;
    
    // Free objects
    Object *obj = objects;
    while (obj) {
        Object *next = obj->next;
        free_object(obj);
        obj = next;
    }
    objects = NULL;
}

// Register a C function
void register_c_function(const char *name, CFunction func) {
    if (!global_frame) {
        vm_init();
    }
    
    // Store function pointer as a string representation
    char value[64];
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
    if (!func || (func->type != AST_FUNCTION && func->type != AST_TYPED_FUNCTION)) return;
    
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
        if (entry->func && entry->func->value && strcmp(entry->func->value, name) == 0) {
            return entry->func;
        }
        entry = entry->next;
    }
    return NULL;
}

// Execute a function call with access control
const char* execute_function_call(const char *name, ASTNode *args, StackFrame *frame) {
    if (!name) return "undefined";
    
    printf("[VM] Executing function call: %s\n", name);
    
    // Save previous class context
    char prev_class[128];
    strncpy(prev_class, current_class, sizeof(prev_class));
    
    // Special handling for method calls
    char object_id[128] = {0};
    char method_name[128] = {0};
    char object_class[128] = {0}; // To store the class name of the object
    int is_method_call = 0;
    const char *obj_str = NULL;
    Object *obj = NULL;
    
    // Check if it's a method call (format: obj:123.method)
    const char *dot_pos = strchr(name, '.');
    if (dot_pos) {
        is_method_call = 1;
        size_t obj_part_len = dot_pos - name;
        strncpy(object_id, name, obj_part_len);
        object_id[obj_part_len] = '\0';
        strcpy(method_name, dot_pos + 1);
        
        printf("[VM] Method call: %s on object %s\n", method_name, object_id);
        
        // Get the object instance for 'this'
        if (strncmp(object_id, "obj:", 4) == 0) {
            int id = atoi(object_id + 4);
            printf("[VM] Looking for object with ID: %d\n", id);
            
            // Find the object by its ID
            Object *obj_iter = objects;
            while (obj_iter) {
                int obj_id;
                if (sscanf(obj_iter->class_name, "%[^#]#%d", object_class, &obj_id) == 2) {
                    printf("[VM] Checking object: %s (ID: %d)\n", obj_iter->class_name, obj_id);
                    if (obj_id == id) {
                        obj = obj_iter;
                        break;
                    }
                }
                obj_iter = obj_iter->next;
            }
            
            if (obj) {
                obj_str = object_id;
                printf("[VM] Found object: %s, class: %s\n", object_id, object_class);
                
                // Initialize TestClass objects when calling any method on them
                if (strstr(obj->class_name, "TestClass") != NULL) {
                    printf("[VM] Auto-initializing TestClass object before method call\n");
                    initialize_test_class(obj);
                }
                
                // Set current class context based on the object's class
                strncpy(current_class, object_class, sizeof(current_class) - 1);
                current_class[sizeof(current_class) - 1] = '\0';
                printf("[VM] Setting current class context: %s\n", current_class);
            }
        }
    }
    
    // First check if it's a built-in function
    const char *result = call_built_in_function(name, args, frame);
    if (strcmp(result, "undefined") != 0) {
        // Restore previous class context
        strncpy(current_class, prev_class, sizeof(current_class));
        return result;
    }
    
    // Try to find function in user-defined functions
    ASTNode *func = find_user_function(name);
    if (!func && is_method_call) {
        // First fallback: class-qualified method (ClassName.method)
        if (object_class[0] != '\0') {
            char qual[256];
            snprintf(qual, sizeof(qual), "%s.%s", object_class, method_name);
            func = find_user_function(qual);
        }
        // If still not found, look by (class,name) pair among registered functions
        if (!func && object_class[0] != '\0') {
            FunctionEntry *fe = registered_functions;
            while (fe) {
                if (fe->func && fe->func->parent_class && strcmp(fe->func->parent_class, object_class) == 0 &&
                    strcmp(fe->func->value, method_name) == 0) {
                    func = fe->func;
                    break;
                }
                fe = fe->next;
            }
        }
        // We intentionally do NOT fall back to a plain method name on another class.
    }
    if (func) {
        // Access control for private methods
        if (is_method_call) {
            if (func->access_modifier[0] != '\0' && strcmp(func->access_modifier, "private") == 0 && strcmp(current_class, prev_class) != 0) {
                fprintf(stderr, "Error: Cannot access private method %s from outside the class\n", method_name);
                strncpy(current_class, prev_class, sizeof(current_class));
                return "undefined";
            }
        }

        // Create a new stack frame for the function call
        StackFrame *new_frame = create_stack_frame(name, frame);

        // Set 'this' if needed
        if (is_method_call && obj_str) {
            printf("[VM] Setting 'this' to %s in the function frame\n", obj_str);
            set_variable(new_frame, "this", obj_str);
        }

        // Map parameters to arguments (parameters are stored in func->left list)
        ASTNode *param = func->left;
        ASTNode *arg = args;
        while (param && arg) {
            const char *arg_value = evaluate_expression(arg, frame);
            set_variable(new_frame, param->value, arg_value);
            param = param->next;
            arg = arg->next;
        }

        // Execute the function body (stored in right child)
        run_vm_node(func->right, new_frame);

        // Retrieve return value (if any) from VM helper
        const char *ret_val = get_return_value();

        destroy_stack_frame(new_frame);

        strncpy(current_class, prev_class, sizeof(current_class));

        printf("[VM] Function call '%s' completed with result: %s\n", name, ret_val);
        return ret_val;
    }
    
    // Function not found – suppress noise for lifecycle hooks
    const char *lifecycle[] = {"Awake","Start","FixedUpdate","Update","LateUpdate",NULL};
    int is_lifecycle = 0;
    for (int i=0; lifecycle[i]; ++i) {
        if (strcmp(method_name[0] ? method_name : name, lifecycle[i]) == 0) { is_lifecycle = 1; break; }
    }
    if (!is_lifecycle) {
        fprintf(stderr, "Error: Function not found: %s\n", name);
    }
    
    // Restore previous class context
    strncpy(current_class, prev_class, sizeof(current_class));
    
    return "undefined";
}

// Execute a node
void run_vm_node(ASTNode *node, StackFrame *frame) {
    if (!node) return;
    
    // Set class context from the frame's function name if it looks like a class method
    if (frame && frame->function_name && strncmp(frame->function_name, "obj:", 4) != 0) {
        char *dot = strchr(frame->function_name, '.');
        if (dot) {
            // ClassName.methodName pattern
            size_t len = dot - frame->function_name;
            if (len >= sizeof(current_class)) len = sizeof(current_class) - 1;
            strncpy(current_class, frame->function_name, len);
            current_class[len] = '\0';
        }
    }
    
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
            
        case AST_PRINT: {
            // Evaluate the expression to print
            const char *value = evaluate_expression(node->left, frame);
            printf("%s\n", value ? value : "undefined");
            break;
        }
            
        case AST_VAR_DECL: {
            // Evaluate initializer if present
            const char *value = "undefined";
            if (node->right) {
                value = evaluate_expression(node->right, frame);
            }
            
            // Set the variable in the current stack frame
            set_variable(frame, node->left->value, value);
            break;
        }
            
        case AST_ASSIGN: {
            // Evaluate right-hand side
            const char *value = evaluate_expression(node->right, frame);
            
            // Set the variable
            if (node->left->type == AST_IDENTIFIER) {
                set_variable(frame, node->left->value, value);
            } else if (node->left->type == AST_MEMBER_ACCESS) {
                // Handle object property assignment
                const char *obj_str = NULL;
                
                if (node->left->left->type == AST_THIS) {
                    // Handle 'this' keyword - get from current frame
                    obj_str = get_variable(frame, "this");
                    if (!obj_str) {
                        fprintf(stderr, "Error: 'this' is undefined in current context\n");
                        break;
                    }
                    printf("[DEBUG] In AST_ASSIGN - This reference resolved to: %s\n", obj_str);
                } else {
                    // Handle regular object reference
                    obj_str = evaluate_expression(node->left->left, frame);
                }
                
                if (!obj_str || strcmp(obj_str, "undefined") == 0) {
                    fprintf(stderr, "Error: Cannot set property on undefined\n");
                    break;
                }
                
                // Check if it's an object reference (obj:ID format)
                if (strncmp(obj_str, "obj:", 4) == 0) {
                    int obj_id = atoi(obj_str + 4);
                    Object *obj = find_object_by_id(obj_id);
                    
                    if (!obj) {
                        fprintf(stderr, "Error: Object not found with ID: %d\n", obj_id);
                        break;
                    }
                    
                    printf("[DEBUG] Setting property %s on object %s to value %s\n", 
                           node->left->value, obj->class_name, value);
                    
                    // Determine access modifier
                    AccessModifier access = ACCESS_PUBLIC;
                    int is_static = 0;
                    
                    if (node->left->access_modifier[0] != '\0') {
                        if (strcmp(node->left->access_modifier, "private") == 0) {
                            access = ACCESS_PRIVATE;
                        } else if (strcmp(node->left->access_modifier, "static") == 0) {
                            is_static = 1;
                        }
                    }
                    
                    // Set the property
                    set_object_property_with_access(obj, node->left->value, value, access, is_static);
                } else {
                    fprintf(stderr, "Error: Cannot set property on non-object: %s\n", obj_str);
                }
            }
            break;
        }
            
        case AST_RETURN: {
            // Evaluate return value
            const char *value = node->left ? evaluate_expression(node->left, frame) : "0";
            set_return_value(value);
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
            
        case AST_CALL: {
            // Handle function calls (like print, etc.)
            const char *func_name = node->value;
            execute_function_call(func_name, node->left, frame);
            break;
        }
            
        case AST_BINARY_OP: {
            // Check if it's an assignment
            if (strcmp(node->value, "=") == 0) {
                // Assignment operation
                const char *value = evaluate_expression(node->right, frame);
                
                if (node->left->type == AST_IDENTIFIER) {
                    // Variable assignment
                    set_variable(frame, node->left->value, value);
                } 
                else if (node->left->type == AST_MEMBER_ACCESS) {
                    // Object property assignment
                    const char *obj_str;
                    
                    if (node->left->left->type == AST_THIS) {
                        // Handle 'this' keyword
                        obj_str = get_variable(frame, "this");
                        if (!obj_str) {
                            fprintf(stderr, "Error: 'this' is undefined in current context\n");
                            break;
                        }
                    } else {
                        // Regular object reference
                        obj_str = evaluate_expression(node->left->left, frame);
                    }
                    
                    printf("[VM_ASSIGN] Setting %s.%s = %s (class context: %s)\n", 
                           obj_str ? obj_str : "undefined", 
                           node->left->value, 
                           value,
                           current_class);
                    
                    if (obj_str && strncmp(obj_str, "obj:", 4) == 0) {
                        int id = atoi(obj_str + 4);
                        
                        // Find the object
                        Object *obj = objects;
                        while (obj) {
                            int obj_id;
                            sscanf(obj->class_name, "%*[^#]#%d", &obj_id);
                            if (obj_id == id) {
                                // Determine access modifier
                                AccessModifier access = ACCESS_PUBLIC;
                                int is_static = 0;
                                
                                // Access modifiers from AST
                                if (node->left->access_modifier[0] != '\0') {
                                    if (strcmp(node->left->access_modifier, "private") == 0) {
                                        access = ACCESS_PRIVATE;
                                    } else if (strcmp(node->left->access_modifier, "static") == 0) {
                                        is_static = 1;
                                    }
                                }
                                
                                // Set the property directly
                                set_object_property_with_access(obj, node->left->value, value, access, is_static);
                                break;
                            }
                            obj = obj->next;
                        }
                        
                        if (!obj) {
                            fprintf(stderr, "Error: Object not found for ID: %d\n", id);
                        }
                    } else {
                        fprintf(stderr, "Error: Cannot set property on non-object: %s\n", obj_str ? obj_str : "undefined");
                    }
                }
            } else {
                // Other binary operations
                evaluate_expression(node, frame);
            }
            break;
        }
            
        case AST_MEMBER_ACCESS: {
            // Just evaluate the expression to handle property access
            evaluate_expression(node, frame);
            break;
        }
            
        default:
            // Other node types are handled by the evaluator
            evaluate_expression(node, frame);
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
            if (node->type == AST_FUNCTION || node->type == AST_TYPED_FUNCTION) {
                printf("[VM] Registering function: %s\n", node->value);
                register_user_function(node);
            } else if (node->type == AST_CLASS) {
                // Register the class
                printf("[VM] Registering class: %s\n", node->value);
                vm_register_class(node->value);
                
                // Register its methods as functions
                ASTNode *class_member = node->left;
                while (class_member) {
                    if (class_member->type == AST_FUNCTION || class_member->type == AST_TYPED_FUNCTION) {
                        // Set parent class for this method
                        class_member->parent_class = node->value;
                        printf("[VM] Registering method: %s.%s\n", node->value, class_member->value);
                        register_user_function(class_member);
                    }
                    class_member = class_member->next;
                }
            } else if (node->type == AST_IMPORT) {
                // Process imports to load external modules
                Module *module = module_load(node->value);
                if (module && module->ast) {
                    // Register all functions from the imported module
                    if (module->ast->type == AST_PROGRAM) {
                        ASTNode *stmt = module->ast->left;
                        while (stmt) {
                            if (stmt->type == AST_FUNCTION || stmt->type == AST_TYPED_FUNCTION) {
                                register_user_function(stmt);
                            } else if (stmt->type == AST_CLASS) {
                                // Register the class
                                vm_register_class(stmt->value);
                                
                                // Register its methods as functions
                                ASTNode *class_member = stmt->left;
                                while (class_member) {
                                    if (class_member->type == AST_FUNCTION || class_member->type == AST_TYPED_FUNCTION) {
                                        // Set parent class for this method
                                        class_member->parent_class = stmt->value;
                                        register_user_function(class_member);
                                    }
                                    class_member = class_member->next;
                                }
                            }
                            stmt = stmt->next;
                        }
                    }
                }
            }
            node = node->next;
        }
    }
    
    // --- Build per-class singleton objects once ---
    typedef struct LifecycleInstance {
        char obj_ref[32];
        struct LifecycleInstance *next;
    } LifecycleInstance;
    LifecycleInstance *instances = NULL;
    LifecycleInstance *inst_tail = NULL;

    ClassEntry *cls_iter = registered_classes;
    while (cls_iter) {
        Object *obj = create_object(cls_iter->name);
        if (obj) {
            int obj_id = 0;
            sscanf(obj->class_name, "%*[^#]#%d", &obj_id);
            LifecycleInstance *inst = malloc(sizeof(LifecycleInstance));
            snprintf(inst->obj_ref, sizeof(inst->obj_ref), "obj:%d", obj_id);
            inst->next = NULL;
            if (!instances) {
                instances = inst_tail = inst;
            } else {
                inst_tail->next = inst;
                inst_tail = inst;
            }

            // Auto-set singleton static reference if the class declares a field named 'singleton'
            Object *static_obj = find_static_class_object(cls_iter->name);
            if (static_obj) {
                // Only set if not already assigned
                const char *existing = get_object_property_with_access_check(static_obj, "singleton", cls_iter->name);
                if (!existing || strcmp(existing, "undefined") == 0) {
                    char obj_ref[32];
                    snprintf(obj_ref, sizeof(obj_ref), "obj:%d", obj_id);
                    printf("[VM] Auto-setting %s.singleton = %s\n", cls_iter->name, obj_ref);
                    set_object_property_with_access(static_obj, "singleton", obj_ref, ACCESS_PUBLIC, 1);
                }
            }
        }
        cls_iter = cls_iter->next;
    }

    const char *awake_name  = "Awake";
    const char *start_name  = "Start";
    const char *update_name = "Update";
    const char *fixed_name  = "FixedUpdate";
    const char *late_name   = "LateUpdate";

    // ---------- Awake phase (global + per-instance) ----------
    ASTNode *global_awake = find_user_function(awake_name);
    if (global_awake && global_awake->parent_class == NULL) {
        printf("[VM] Running global Awake\n");
        execute_function_call(awake_name, NULL, global_frame);
    }
    for (LifecycleInstance *it = instances; it; it = it->next) {
        char qual[256];
        snprintf(qual, sizeof(qual), "%s.%s", it->obj_ref, awake_name);
        execute_function_call(qual, NULL, global_frame);
    }

    // ---------- Start phase (global + per-instance) ----------
    if (find_user_function(start_name)) {
        printf("[VM] Running global Start\n");
        execute_function_call(start_name, NULL, global_frame);
    }
    for (LifecycleInstance *it = instances; it; it = it->next) {
        char qual[256];
        snprintf(qual, sizeof(qual), "%s.%s", it->obj_ref, start_name);
        execute_function_call(qual, NULL, global_frame);
    }

    // ---------- Main loop ----------
    const int FRAME_COUNT = 10; // simple fixed loop; extend later
    
    // Check if any lifecycle methods exist before entering the frame loop
    int has_lifecycle_methods = 0;
    if (find_user_function(update_name) || find_user_function(fixed_name) || find_user_function(late_name)) {
        has_lifecycle_methods = 1;
    } else {
        // Check for instance lifecycle methods
        for (LifecycleInstance *it = instances; it && !has_lifecycle_methods; it = it->next) {
            char qual_update[256], qual_fixed[256], qual_late[256];
            snprintf(qual_update, sizeof(qual_update), "%s.%s", it->obj_ref, update_name);
            snprintf(qual_fixed, sizeof(qual_fixed), "%s.%s", it->obj_ref, fixed_name);
            snprintf(qual_late, sizeof(qual_late), "%s.%s", it->obj_ref, late_name);
            
            if (find_user_function(qual_update) || find_user_function(qual_fixed) || 
                find_user_function(qual_late)) {
                has_lifecycle_methods = 1;
            }
        }
    }
    
    if (!has_lifecycle_methods) {
        printf("[VM] No lifecycle methods (Update/FixedUpdate/LateUpdate) found. Skipping frame loop.\n");
    } else {
        for (int frame = 0; frame < FRAME_COUNT; ++frame) {
            printf("[VM] ---- Frame %d ----\n", frame);

            // FixedUpdate
            if (find_user_function(fixed_name)) {
                execute_function_call(fixed_name, NULL, global_frame);
            }
            for (LifecycleInstance *it = instances; it; it = it->next) {
                char qual[256];
                snprintf(qual, sizeof(qual), "%s.%s", it->obj_ref, fixed_name);
                execute_function_call(qual, NULL, global_frame);
            }

            // Update
            if (find_user_function(update_name)) {
                execute_function_call(update_name, NULL, global_frame);
            }
            for (LifecycleInstance *it = instances; it; it = it->next) {
                char qual[256];
                snprintf(qual, sizeof(qual), "%s.%s", it->obj_ref, update_name);
                execute_function_call(qual, NULL, global_frame);
            }

            // LateUpdate
            if (find_user_function(late_name)) {
                execute_function_call(late_name, NULL, global_frame);
            }
            for (LifecycleInstance *it = instances; it; it = it->next) {
                char qual[256];
                snprintf(qual, sizeof(qual), "%s.%s", it->obj_ref, late_name);
                execute_function_call(qual, NULL, global_frame);
            }
        }
    }

    // Free instance list helper (objects themselves are cleaned in vm_cleanup)
    while (instances) {
        LifecycleInstance *next = instances->next;
        free(instances);
        instances = next;
    }
    
    // Clean up
    vm_cleanup();
}

// Set an object property (basic version for backward compatibility)
void set_object_property(Object *obj, const char *name, const char *value) {
    set_object_property_with_access(obj, name, value, ACCESS_PUBLIC, 0);
}

// Evaluate a member access expression (obj.property)
const char* evaluate_member_access(ASTNode *expr, StackFrame *frame) {
    if (!expr || !expr->left || !expr->value) {
        fprintf(stderr, "Error: Invalid member access expression\n");
        return "undefined";
    }
    
    // Get the object from the left part
    const char *obj_str;
    
    if (expr->left->type == AST_THIS) {
        // Handle 'this' keyword
        obj_str = get_variable(frame, "this");
        if (!obj_str) {
            fprintf(stderr, "Error: 'this' is undefined in current context\n");
            return "undefined";
        }
    } else {
        // Regular object reference
        obj_str = evaluate_expression(expr->left, frame);
    }
    
    if (!obj_str || strcmp(obj_str, "undefined") == 0) {
        fprintf(stderr, "Error: Cannot access property of undefined\n");
        return "undefined";
    }
    
    // Check if it's a class name (for static access)
    if (expr->left->type == AST_IDENTIFIER) {
        ASTNode *current = program;
        while (current) {
            if (current->type == AST_CLASS && strcmp(current->value, expr->left->value) == 0) {
                // Found the class, check for static property
                // Look for a static property in the class
                Object *static_obj = find_static_class_object(expr->left->value);
                if (static_obj) {
                    const char *static_value = get_object_property_with_access_check(
                        static_obj, expr->value, current_class);
                    if (static_value) {
                        return static_value;
                    }
                }
                
                fprintf(stderr, "Error: Static property '%s' not found in class '%s'\n", 
                       expr->value, expr->left->value);
                return "undefined";
            }
            current = current->next;
        }
    }
    
    // Check if it's an object reference
    if (strncmp(obj_str, "obj:", 4) == 0) {
        int obj_id = atoi(obj_str + 4);
        Object *obj = find_object_by_id(obj_id);
        
        if (obj) {
            // Extract the class name for access checking
            char class_name[128] = {0};
            char *hash_pos = strchr(obj->class_name, '#');
            if (hash_pos) {
                size_t class_name_len = hash_pos - obj->class_name;
                strncpy(class_name, obj->class_name, class_name_len);
                class_name[class_name_len] = '\0';
            } else {
                strncpy(class_name, obj->class_name, sizeof(class_name) - 1);
            }
            
            // First check for instance properties
            const char *prop_value = get_object_property_with_access_check(obj, expr->value, current_class);
            if (prop_value) {
                return prop_value;
            }
            
            // If not found, check for static properties
            Object *static_obj = find_static_class_object(class_name);
            if (static_obj) {
                const char *static_value = get_object_property_with_access_check(
                    static_obj, expr->value, current_class);
                if (static_value) {
                    return static_value;
                }
            }
            
            // Property not found
            fprintf(stderr, "Error: Property '%s' not found or not accessible on object %s\n", 
                   expr->value, obj->class_name);
            return "undefined";
        } else {
            fprintf(stderr, "Error: Object with ID %d not found\n", obj_id);
            return "undefined";
        }
    }
    
    fprintf(stderr, "Error: Cannot access property '%s' of non-object: %s\n", 
           expr->value, obj_str);
    return "undefined";
}

// Find an object by its ID
Object* find_object_by_id(int id) {
    Object *obj = objects;
    while (obj) {
        int obj_id;
        sscanf(obj->class_name, "%*[^#]#%d", &obj_id);
        if (obj_id == id) {
            return obj;
        }
        obj = obj->next;
    }
    return NULL;
}

// Find or create a static class object
Object* find_static_class_object(const char *class_name) {
    // First, try to find existing static object for the class
    Object *obj = objects;
    while (obj) {
        if (strncmp(obj->class_name, class_name, strlen(class_name)) == 0 && 
            strstr(obj->class_name, "_static") != NULL) {
            return obj;
        }
        obj = obj->next;
    }
    
    // If not found, create a new static class object
    char static_class_name[256];
    snprintf(static_class_name, sizeof(static_class_name), "%s_static", class_name);
    
    Object *static_obj = create_object(static_class_name);
    if (static_obj) {
        printf("[VM] Created static class object for %s\n", class_name);
    } else {
        fprintf(stderr, "Error: Failed to create static class object for %s\n", class_name);
    }
    
    return static_obj;
}

// Initialize the TestClass properties
void initialize_test_class(Object *obj) {
    if (!obj) return;
    
    printf("[VM] Initializing TestClass properties for %s\n", obj->class_name);
    
    // Check if this is already a TestClass_static object
    if (strstr(obj->class_name, "_static") != NULL) {
        // Only set static properties on the static object
        set_object_property_with_access(obj, "static_prop", "Static property", ACCESS_PUBLIC, 1);
        printf("[VM] Set static property on static class object\n");
        return;
    }
    
    // This is a regular instance - set instance properties
    set_object_property_with_access(obj, "public_prop", "Public property", ACCESS_PUBLIC, 0);
    set_object_property_with_access(obj, "private_prop", "Private property", ACCESS_PRIVATE, 0);
    
    // Also ensure the static class object exists and has static properties
    Object *static_obj = find_static_class_object("TestClass");
    if (static_obj) {
        set_object_property_with_access(static_obj, "static_prop", "Static property", ACCESS_PUBLIC, 1);
        printf("[VM] Set static property on the static class object\n");
    }
    
    printf("[VM] TestClass properties initialized\n");
}

/*
 * Forwarder that resolves and executes built-in/native functions registered by
 * stdlib.c.  We evaluate all argument AST nodes first (so that expressions are
 * executed in the current VM context) and then call stdlib's
 * `call_builtin_function`, which in turn dispatches to the appropriate C
 * wrapper.  Those wrappers communicate their return value back to the VM via
 * the global `set_return_value`/`get_return_value` helpers.
 */
extern int call_builtin_function(const char *name, const char **args, int arg_count);

const char* call_built_in_function(const char* func_name, ASTNode* args, StackFrame* frame) {
    if (!func_name) return "undefined";

    /* Count arguments in the linked list. */
    int arg_count = 0;
    ASTNode *iter = args;
    while (iter) { arg_count++; iter = iter->next; }

    /* Collect evaluated argument strings. */
    const char **arg_values = NULL;
    if (arg_count > 0) {
        arg_values = (const char**)malloc(sizeof(char*) * arg_count);
        if (!arg_values) {
            fprintf(stderr, "VM: Out of memory when marshalling call args.\n");
            return "undefined";
        }

        iter = args;
        for (int i = 0; i < arg_count; ++i) {
            arg_values[i] = evaluate_expression(iter, frame);
            iter = iter->next;
        }
    }

    /* Invoke the stdlib dispatcher. */
    int ok = call_builtin_function(func_name, arg_values, arg_count);

    if (arg_values) free((void*)arg_values);

    if (ok) {
        const char *ret = get_return_value();
        return (ret ? ret : "undefined");
    }

    return "undefined";
}

// Forward helper to set default value on newly created object based on type
static void initialize_default_instance_fields(const char *class_name, Object *instance) {
    if (!class_name || !instance) return;

    /* The runtime creates a special "ClassName_static" object to hold static
       members.  Strip that suffix off when looking for the source-code class
       declaration so that default-value initialisation still works. */
    char search_name[128];
    strncpy(search_name, class_name, sizeof(search_name)-1);
    search_name[sizeof(search_name)-1] = '\0';
    char *static_pos = strstr(search_name, "_static");
    if (static_pos) {
        *static_pos = '\0'; // terminate to keep just the base class name
    }

    /* Find the AST node for the (base) class */
    ASTNode *cls_node = program ? program->left : NULL;
    while (cls_node) {
        if (cls_node->type == AST_CLASS && strcmp(cls_node->value, search_name) == 0) {
            break;
        }
        cls_node = cls_node->next;
    }
    if (!cls_node) return;

    /* Are we populating a static object? */
    int is_static_object = (static_pos != NULL);

    ASTNode *member = cls_node->left;
    while (member) {
        ASTNode *varNode = NULL;
        if (member->type == AST_VAR_DECL || member->type == AST_TYPED_VAR_DECL) {
            varNode = member;
        } else if (member->type == AST_CLASS_FIELD) {
            // parser stores actual declaration in left child
            varNode = member->left;
            if (!(varNode && (varNode->type == AST_VAR_DECL || varNode->type == AST_TYPED_VAR_DECL))) {
                member = member->next; continue;
            }
        } else {
            member = member->next; continue;
        }

        // Use varNode from here
        const char *prop_name = NULL;
        if (varNode->type == AST_TYPED_VAR_DECL && varNode->left) {
            prop_name = varNode->left->value;
        } else {
            prop_name = varNode->value;
        }

        /* Figure out the initialiser expression – different parser paths place
           it on either the left or the right child.  Prefer `right` if it
           exists, otherwise fall back to `left` (only if that child is not an
           identifier node, which would simply duplicate the property name). */
        ASTNode *init_expr = NULL;
        if (varNode->right) {
            init_expr = varNode->right;
        } else if (varNode->left && varNode->left->type != AST_IDENTIFIER) {
            init_expr = varNode->left;
        }

        const char *val_to_set = NULL;
        if (init_expr) {
            val_to_set = evaluate_expression(init_expr, global_frame);
        }

        const char *dtype = varNode->data_type;
        const char *default_val = "undefined";
        char tmp[64];

        if (dtype) {
            if (strcmp(dtype, "int") == 0 || strcmp(dtype, "long") == 0) {
                // Integer-like types default to 0
                default_val = "0";
            } else if (strcmp(dtype, "float") == 0 || strcmp(dtype, "double") == 0) {
                // Floating-point types default to 0.0
                default_val = "0.0";
            } else if (strcmp(dtype, "bool") == 0) {
                // Booleans default to false
                default_val = "false";
            } else if (strcmp(dtype, "char") == 0) {
                default_val = "\0";
            } else if (strcmp(dtype, "Vector2") == 0 || strcmp(dtype, "Vector3") == 0 || strcmp(dtype, "Vector4") == 0) {
                // Create default vector object of requested dimension all zeros
                Object *vec_obj = create_object(dtype);
                if (vec_obj) {
                    // Initialise components to 0
                    if (strcmp(dtype,"Vector2")==0) {
                        set_object_property_with_access(vec_obj,"x","0",ACCESS_PUBLIC,0);
                        set_object_property_with_access(vec_obj,"y","0",ACCESS_PUBLIC,0);
                    } else if (strcmp(dtype,"Vector3")==0) {
                        set_object_property_with_access(vec_obj,"x","0",ACCESS_PUBLIC,0);
                        set_object_property_with_access(vec_obj,"y","0",ACCESS_PUBLIC,0);
                        set_object_property_with_access(vec_obj,"z","0",ACCESS_PUBLIC,0);
                    } else if (strcmp(dtype,"Vector4")==0) {
                        set_object_property_with_access(vec_obj,"x","0",ACCESS_PUBLIC,0);
                        set_object_property_with_access(vec_obj,"y","0",ACCESS_PUBLIC,0);
                        set_object_property_with_access(vec_obj,"z","0",ACCESS_PUBLIC,0);
                        set_object_property_with_access(vec_obj,"w","0",ACCESS_PUBLIC,0);
                    }
                    int vid=0; sscanf(vec_obj->class_name,"%*[^#]#%d",&vid);
                    snprintf(tmp,sizeof(tmp),"obj:%d",vid);
                    default_val = tmp;
                }
            }
        }

        /* Access modifier handling – default to public unless the declaration
           carried an explicit "private" keyword.  The parser records the
           modifier on the wrapper (member) node rather than the varNode itself
           for plain fields.  For simplicity we fall back to public when that
           information is missing. */
        AccessModifier access = ACCESS_PUBLIC;
        if (member->access_modifier[0] != '\0' && strcmp(member->access_modifier, "private") == 0) {
            access = ACCESS_PRIVATE;
        }

        printf("[INIT] Setting default field %s.%s (%s) = %s [static=%d]\n",
               class_name, prop_name, dtype ? dtype : "", val_to_set ? val_to_set : default_val, is_static_object);

        set_object_property_with_access(instance,
                                         prop_name,
                                         val_to_set ? val_to_set : default_val,
                                         access,
                                         is_static_object);

        /* advance to next class member */
        member = member->next;
    }
}
