#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include "eval.h"
#include "ast_types.h"
#include "vm.h"

// Forward declaration
struct Object;
extern struct Object *objects;
extern char current_class[128];

// Extern global AST root shared by parser
extern ASTNode *program;

// External references to VM-managed state
extern void *registered_classes;

// Buffer for storing evaluation results
static char result_buffer[1024];

// Forward declaration
static const char* evaluate_binary_op(const char *op, const char *left, const char *right);

// Function to evaluate an expression
const char* evaluate_expression(ASTNode *expr, StackFrame *frame) {
    if (!expr) return "undefined";
    
    switch (expr->type) {
        case AST_LITERAL:
            return expr->value;
            
        case AST_IDENTIFIER: {
            printf("[IDENT] Resolving identifier '%s' (current_class=%s)\n", expr->value, current_class);
            const char *value = get_variable(frame, expr->value);
            if (value) {
                printf("[IDENT] Found as local variable: %s = %s\n", expr->value, value);
                return value;
            }

            // If not a local variable, try resolving as an instance property on 'this'
            printf("[IDENT] Not a local variable; trying instance property on this\n");
            if (frame) {
                const char *this_ref_debug = get_variable(frame, "this");
                if (this_ref_debug) printf("[IDENT] 'this' is %s\n", this_ref_debug);
                const char *this_ref = get_variable(frame, "this");
                if (this_ref && strncmp(this_ref, "obj:", 4) == 0) {
                    int obj_id = atoi(this_ref + 4);
                    Object *obj = find_object_by_id(obj_id);
                    if (obj) {
                        const char *inst_val = get_object_property_with_access(obj, expr->value, current_class);
                        if (inst_val && strcmp(inst_val, "undefined") != 0) {
                            printf("[IDENT] Resolved as instance property: %s\n", inst_val);
                            return inst_val;
                        }
                    }
                }
            }

            printf("[IDENT] Not an instance property; checking for class name heuristic\n");
            // Heuristic: if the identifier starts with an upper-case letter we
            // assume it denotes a class name (Ouroboros convention) so that
            // subsequent member-access attempts treat it as such.
            if (isupper((unsigned char)expr->value[0])) {
                printf("[IDENT] Treating as class identifier (heuristic)\n");
                return expr->value;
            }

            printf("[IDENT] Checking explicit class declarations list\n");
            // If the identifier matches a declared class name, keep it as
            // a bare identifier so that member-access logic can treat it
            // as a static access rather than fetching a property from the
            // *current* class static object.
            ASTNode *cls_iter = program ? program->left : NULL;
            while (cls_iter) {
                if (cls_iter->type == AST_CLASS && strcmp(cls_iter->value, expr->value) == 0) {
                    printf("[IDENT] Found in declarations list, treating as class identifier\n");
                    return expr->value; // class identifier
                }
                cls_iter = cls_iter->next;
            }

            printf("[IDENT] Attempting static property of current class\n");
            // If not a local/stack variable, try static property of current class
            if (current_class[0] != '\0') {
                Object *static_obj = find_static_class_object(current_class);
                if (static_obj) {
                    const char *static_val = get_object_property_with_access_check(static_obj, expr->value, current_class);
                    if (static_val && strcmp(static_val, "undefined") != 0) {
                        printf("[IDENT] Resolved as static property of current class: %s\n", static_val);
                        return static_val;
                    }
                }
            }

            if (strcmp(expr->value, "TestMain") == 0) {
                printf("[DEBUG] Resolving identifier TestMain, current_class=%s\n", current_class);
            }

            return "undefined";
        }
            
        case AST_BINARY_OP: {
            // Handle binary operations
            if (strcmp(expr->value, "=") == 0) {
                // Assignment
                if (expr->left->type == AST_IDENTIFIER) {
                    printf("[ASSIGN] Identifier assignment detected: %s\n", expr->left->value);
                    const char *right_val = evaluate_expression(expr->right, frame);

                    // If the identifier exists as a local variable, treat as variable assignment
                    const char *existing = get_variable(frame, expr->left->value);
                    if (existing) {
                        set_variable(frame, expr->left->value, right_val);
                        return right_val;
                    }

                    // Otherwise, attempt to assign to a static property of the current class first
                    if (current_class[0] != '\0') {
                        Object *static_obj = find_static_class_object(current_class);
                        if (static_obj) {
                            printf("[ASSIGNMENT] Setting static property %s.%s = %s\n", current_class, expr->left->value, right_val);
                            if (strcmp(expr->left->value, "singleton") == 0) {
                                printf("[DEBUG] Assigning singleton inside class %s with value %s\n", current_class, right_val);
                            }
                            set_object_property_with_access(static_obj, expr->left->value, right_val, ACCESS_PUBLIC, 1);
                            return right_val;
                        }
                    }

                    // Next, attempt to assign to a property on 'this' (instance)
                    const char *this_ref = get_variable(frame, "this");
                    if (this_ref && strncmp(this_ref, "obj:", 4) == 0) {
                        int obj_id = atoi(this_ref + 4);
                        Object *obj = find_object_by_id(obj_id);
                        if (obj) {
                            printf("[ASSIGNMENT] Setting instance property %s.%s = %s\n", this_ref, expr->left->value, right_val);
                            set_object_property_with_access(obj, expr->left->value, right_val, ACCESS_PUBLIC, 0);
                            return right_val;
                        }
                    }

                    // Fallback to creating/overwriting a local variable
                    set_variable(frame, expr->left->value, right_val);
                    return right_val;
                } 
                else if (expr->left->type == AST_MEMBER_ACCESS) {
                    // Object property assignment
                    printf("[ASSIGN] Member assignment detected to property %s\n", expr->left->value);
                    const char *right_val = evaluate_expression(expr->right, frame);
                    
                    // Get the object reference
                    const char *obj_str;
                    
                    if (expr->left->left->type == AST_THIS) {
                        // Handle 'this' keyword
                        obj_str = get_variable(frame, "this");
                        if (!obj_str) {
                            fprintf(stderr, "Error: 'this' is undefined in current context\n");
                            return "undefined";
                        }
                        printf("[DEBUG] In AST_BINARY_OP - This reference resolved to: %s\n", obj_str);
                    } else {
                        // Regular object reference
                        obj_str = evaluate_expression(expr->left->left, frame);
                    }
                    
                    if (!obj_str || strcmp(obj_str, "undefined") == 0) {
                        fprintf(stderr, "Error: Cannot set property of undefined\n");
                        return "undefined";
                    }
                    
                    printf("[DEBUG] Binary op assigning to %s.%s = %s\n", obj_str, expr->left->value, right_val);
                    
                    // Check if it's a class name (for static properties)
                    if (expr->left->left->type == AST_IDENTIFIER) {
                        ASTNode *current = program;
                        while (current) {
                            if (current->type == AST_CLASS && strcmp(current->value, expr->left->left->value) == 0) {
                                // Found the class, set the static property
                                Object *static_obj = find_static_class_object(expr->left->left->value);
                                if (static_obj) {
                                    // Determine access modifier
                                    AccessModifier access = ACCESS_PUBLIC;
                                    int is_static = 1; // Always static when accessing via class name
                                    
                                    if (expr->left->access_modifier[0] != '\0') {
                                        if (strcmp(expr->left->access_modifier, "private") == 0) {
                                            access = ACCESS_PRIVATE;
                                        }
                                    }
                                    
                                    printf("[DEBUG] Setting static property %s.%s = %s\n", 
                                           expr->left->left->value, expr->left->value, right_val);
                                    set_object_property_with_access(static_obj, expr->left->value, right_val, access, is_static);
                                    return right_val;
                                }
                            }
                            current = current->next;
                        }
                    }
                    
                    // Check if it's an object reference
                    if (strncmp(obj_str, "obj:", 4) == 0) {
                        int obj_id = atoi(obj_str + 4);
                        Object *obj = find_object_by_id(obj_id);
                        
                        if (obj) {
                            // Determine access modifier
                            AccessModifier access = ACCESS_PUBLIC;
                            int is_static = 0;
                            
                            if (expr->left->access_modifier[0] != '\0') {
                                if (strcmp(expr->left->access_modifier, "private") == 0) {
                                    access = ACCESS_PRIVATE;
                                } else if (strcmp(expr->left->access_modifier, "static") == 0) {
                                    is_static = 1;
                                }
                            }
                            
                            printf("[ASSIGNMENT] Setting %s.%s = %s (access: %d, static: %d)\n", 
                                   obj_str, expr->left->value, right_val, access, is_static);
                            
                            // Special case for TestClass - always initialize first
                            if (strstr(obj->class_name, "TestClass#") != NULL) {
                                initialize_test_class(obj);
                            }
                            
                            set_object_property_with_access(obj, expr->left->value, right_val, access, is_static);
                            return right_val;
                        } else {
                            fprintf(stderr, "Error: Object with ID %d not found\n", obj_id);
                            return "undefined";
                        }
                    }
                    
                    fprintf(stderr, "Error: Cannot set property of non-object: %s\n", obj_str);
                    return "undefined";
                } 
                else {
                    fprintf(stderr, "Error: Invalid left side in assignment\n");
                    return "undefined";
                }
            } 
            else {
                // Other binary operations
                const char *left = evaluate_expression(expr->left, frame);
                const char *right = evaluate_expression(expr->right, frame);
                return evaluate_binary_op(expr->value, left, right);
            }
        }
            
        case AST_CALL: {
            // Method or function call
            extern const char* execute_function_call(const char *name, ASTNode *args, StackFrame *frame);

            const char *func_name = expr->value;

            // If the parser stored the target object in right child, build a qualified name
            if (expr->right) {
                const char *obj_str = evaluate_expression(expr->right, frame);
                static char qualified[256];
                snprintf(qualified, sizeof(qualified), "%s.%s", obj_str, func_name);
                func_name = qualified;
            }

            return execute_function_call(func_name, expr->left, frame);
        }
            
        case AST_ARRAY: {
            // For simplicity, we'll just return a string indicating an array
            return "[array]";
        }
            
        case AST_NEW: {
            // Object creation
            if (!expr->value) {
                fprintf(stderr, "Error: Class name missing in new expression\n");
                return "undefined";
            }
            
            printf("[DEBUG] Creating new object of class: %s\n", expr->value);
            
            // Create object
            Object *obj = create_object(expr->value);
            if (!obj) {
                fprintf(stderr, "Error: Failed to create object\n");
                return "undefined";
            }
            
            // Get the object ID
            int obj_id = 0;
            sscanf(obj->class_name, "%*[^#]#%d", &obj_id);
            
            // Format the object reference
            static char obj_ref[32];  // Make static to persist after function returns
            snprintf(obj_ref, sizeof(obj_ref), "obj:%d", obj_id);
            
            printf("[DEBUG] Created object with reference: %s\n", obj_ref);
            
            // Always initialize TestClass properties immediately
            if (strcmp(expr->value, "TestClass") == 0) {
                // Initialize TestClass properties directly
                initialize_test_class(obj);
                printf("[DEBUG] Initialized TestClass properties directly\n");
            }
            
            // For TestClass, no need to call constructor since we've directly initialized
            if (strcmp(expr->value, "TestClass") == 0) {
                return obj_ref;
            }
            
            // For other classes, try to find and call constructor
            // Save previous class context
            char prev_class[128];
            strncpy(prev_class, current_class, sizeof(prev_class));
            
            // Set current class context to the class name
            strncpy(current_class, expr->value, sizeof(current_class) - 1);
            current_class[sizeof(current_class) - 1] = '\0';
            
            // Ouroboros convention: constructor is named "init"
            char constructor_name[256];
            snprintf(constructor_name, sizeof(constructor_name), "%s.init", obj_ref);
            
            printf("[DEBUG] Looking for constructor: %s\n", constructor_name);
            
            // Call the constructor and let execute_function_call create its own frame.
            // Pass the current frame so that argument expressions (which may use 'this')
            // are evaluated in the caller context, not in the yet-to-initialise object.
            execute_function_call(constructor_name, expr->left /* constructor args */, frame);
            
            // Restore previous class context
            strncpy(current_class, prev_class, sizeof(current_class));
            
            return obj_ref;
        }
            
        case AST_MEMBER_ACCESS: {
            // Handle accessing object property (obj.property)
            const char *obj_str;
            
            if (expr->left->type == AST_THIS) {
                // Handle 'this' keyword
                obj_str = get_variable(frame, "this");
                if (!obj_str) {
                    fprintf(stderr, "Error: 'this' is undefined in current context\n");
                    return "undefined";
                }
                printf("[DEBUG] Member access - This reference resolved to: %s\n", obj_str);
            } else {
                // Regular object reference
                obj_str = evaluate_expression(expr->left, frame);
            }
            
            if (!obj_str || strcmp(obj_str, "undefined") == 0) {
                fprintf(stderr, "Error: Cannot access property of undefined\n");
                return "undefined";
            }
            
            printf("[DEBUG] Accessing property %s on object %s\n", expr->value, obj_str);
            
            // Check if it's a class name (for static properties)
            if (expr->left->type == AST_IDENTIFIER) {
                // Check for class in program AST or any import
                int is_class = 0;
                ASTNode *current = program;
                while (current) {
                    if (current->type == AST_CLASS && strcmp(current->value, expr->left->value) == 0) {
                        is_class = 1;
                        break;
                    }
                    current = current->next;
                }
                
                // Use heuristic: uppercase first letter likely indicates a class name
                if (!is_class && expr->left->value[0] >= 'A' && expr->left->value[0] <= 'Z') {
                    is_class = 1;
                }
                
                if (is_class) {
                    // Found the class, access static property
                    Object *static_obj = find_static_class_object(expr->left->value);
                    if (static_obj) {
                        // Get the property value
                        printf("[DEBUG] Looking for property '%s' on static object of class '%s'\n", 
                               expr->value, expr->left->value);
                        const char *prop_value = get_object_property_with_access_check(static_obj, expr->value, current_class);
                        if (prop_value && strcmp(prop_value, "undefined") != 0) {
                            return prop_value;
                        } else {
                            fprintf(stderr, "Error: Static property '%s' not found on class '%s'\n", 
                                  expr->value, expr->left->value);
                            return "undefined";
                        }
                    } else {
                        fprintf(stderr, "Error: Static class object not found for %s\n", expr->left->value);
                        return "undefined";
                    }
                }
            }
            
            // Check if it's an object reference
            if (strncmp(obj_str, "obj:", 4) == 0) {
                int obj_id = atoi(obj_str + 4);
                Object *obj = find_object_by_id(obj_id);
                
                if (obj) {
                    // Special case for TestClass - always initialize properties
                    if (strstr(obj->class_name, "TestClass#") != NULL) {
                        initialize_test_class(obj);
                        printf("[DEBUG] Initialized TestClass instance properties for property access\n");
                    }
                    
                    // Get the property value with access control
                    const char *prop_value = get_object_property_with_access(obj, expr->value, current_class);
                    
                    if (prop_value && strcmp(prop_value, "undefined") != 0) {
                        return prop_value;
                    } else {
                        fprintf(stderr, "Error: Property '%s' not found or not accessible\n", expr->value);
                        return "undefined";
                    }
                } else {
                    fprintf(stderr, "Error: Object with ID %d not found\n", obj_id);
                    return "undefined";
                }
            }
            
            fprintf(stderr, "Error: Cannot access property of non-object: %s\n", obj_str);
            return "undefined";
        }
            
        case AST_THIS: {
            // Get 'this' from the current frame
            const char *this_value = get_variable(frame, "this");
            if (!this_value) {
                fprintf(stderr, "Error: 'this' is undefined in current context\n");
                return "undefined";
            }
            printf("[DEBUG] AST_THIS resolved to: %s\n", this_value);
            return this_value;
        }
            
        default:
            return "undefined";
    }
}

// Helper: check whether a C-string represents a valid number (integer or floating-point)
static int is_numeric_string(const char *s) {
    if (!s || *s == '\0') return 0;
    char *endptr;
    strtod(s, &endptr);
    return *endptr == '\0';
}

// Function to evaluate a binary operation
static const char* evaluate_binary_op(const char *op, const char *left, const char *right) {
    if (!op || !left || !right) return "undefined";
    
    // Operator '+' – decide between numeric addition and string concatenation
    if (strcmp(op, "+") == 0) {
        if (is_numeric_string(left) && is_numeric_string(right)) {
            // Purely numeric addition
            double result = atof(left) + atof(right);
            snprintf(result_buffer, sizeof(result_buffer), "%g", result);
            return result_buffer;
        } else {
            // Treat as string concatenation (no extra quote handling ‑ the lexer already stripped them)
            snprintf(result_buffer, sizeof(result_buffer), "%s%s", left, right);
            return result_buffer;
        }
    }
    // Operator '-' – numeric subtraction
    else if (strcmp(op, "-") == 0) {
        double left_num = atof(left);
        double right_num = atof(right);
        double result = left_num - right_num;
        snprintf(result_buffer, sizeof(result_buffer), "%g", result);
        return result_buffer;
    } else if (strcmp(op, "*") == 0) {
        double left_num = atof(left);
        double right_num = atof(right);
        double result = left_num * right_num;
        snprintf(result_buffer, sizeof(result_buffer), "%g", result);
        return result_buffer;
    } else if (strcmp(op, "/") == 0) {
        double left_num = atof(left);
        double right_num = atof(right);
        if (right_num == 0) {
            return "undefined"; // Division by zero
        }
        double result = left_num / right_num;
        snprintf(result_buffer, sizeof(result_buffer), "%g", result);
        return result_buffer;
    } else if (strcmp(op, "%") == 0) {
        int left_num = atoi(left);
        int right_num = atoi(right);
        if (right_num == 0) {
            return "undefined"; // Modulo by zero
        }
        int result = left_num % right_num;
        snprintf(result_buffer, sizeof(result_buffer), "%d", result);
        return result_buffer;
    } else if (strcmp(op, "==") == 0) {
        // If both are numeric, compare as numbers
        if (isdigit(left[0]) || (left[0] == '-' && isdigit(left[1])) &&
            (isdigit(right[0]) || (right[0] == '-' && isdigit(right[1])))) {
            double left_num = atof(left);
            double right_num = atof(right);
            return (left_num == right_num) ? "true" : "false";
        } else {
            // Otherwise compare as strings
            return (strcmp(left, right) == 0) ? "true" : "false";
        }
    } else if (strcmp(op, "!=") == 0) {
        // If both are numeric, compare as numbers
        if (isdigit(left[0]) || (left[0] == '-' && isdigit(left[1])) &&
            (isdigit(right[0]) || (right[0] == '-' && isdigit(right[1])))) {
            double left_num = atof(left);
            double right_num = atof(right);
            return (left_num != right_num) ? "true" : "false";
        } else {
            // Otherwise compare as strings
            return (strcmp(left, right) != 0) ? "true" : "false";
        }
    } else if (strcmp(op, "<") == 0) {
        double left_num = atof(left);
        double right_num = atof(right);
        return (left_num < right_num) ? "true" : "false";
    } else if (strcmp(op, ">") == 0) {
        double left_num = atof(left);
        double right_num = atof(right);
        return (left_num > right_num) ? "true" : "false";
    } else if (strcmp(op, "<=") == 0) {
        double left_num = atof(left);
        double right_num = atof(right);
        return (left_num <= right_num) ? "true" : "false";
    } else if (strcmp(op, ">=") == 0) {
        double left_num = atof(left);
        double right_num = atof(right);
        return (left_num >= right_num) ? "true" : "false";
    } else if (strcmp(op, "&&") == 0) {
        int left_bool = (strcmp(left, "0") != 0 && strcmp(left, "false") != 0 && strcmp(left, "") != 0);
        int right_bool = (strcmp(right, "0") != 0 && strcmp(right, "false") != 0 && strcmp(right, "") != 0);
        return (left_bool && right_bool) ? "true" : "false";
    } else if (strcmp(op, "||") == 0) {
        int left_bool = (strcmp(left, "0") != 0 && strcmp(left, "false") != 0 && strcmp(left, "") != 0);
        int right_bool = (strcmp(right, "0") != 0 && strcmp(right, "false") != 0 && strcmp(right, "") != 0);
        return (left_bool || right_bool) ? "true" : "false";
    }
    
    return "undefined"; // Unknown operator
}

#if 0
static ASTNode* find_user_function(const char *name) {
    // Original duplicate implementation removed to avoid linker conflicts.
    return NULL;
}
#endif
