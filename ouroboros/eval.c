#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include "eval.h"
#include "ast_types.h"
#include "vm.h" // For Object, find_object_by_id, find_static_class_object, current_class, execute_function_call, etc.
#include "semantic.h" // For Symbol, SymbolTable related types (if needed directly, though usually through vm)

extern ASTNode *program; 
static char result_buffer[1024]; // Static buffer for string results of operations

// Internal helper for binary operations, ensuring operands are stable if they came from result_buffer
static const char* evaluate_binary_op_internal(ASTNode* expr_node, const char *op_str, const char *left_val_str_final, const char *right_val_str_final);


const char* evaluate_expression(ASTNode *expr_node, StackFrame *frame) {
    if (!expr_node) return "undefined";
    
    switch (expr_node->type) {
        case AST_LITERAL:
            return expr_node->value;
            
        case AST_IDENTIFIER: {
            const char *var_name = expr_node->value;
            const char *value_str = get_variable(frame, var_name);
            if (value_str) return value_str;

            if (current_class[0] != '\0') {
                const char *this_obj_ref_str = get_variable(frame, "this");
                if (this_obj_ref_str && strncmp(this_obj_ref_str, "obj:", 4) == 0) {
                    int this_obj_id = atoi(this_obj_ref_str + 4);
                    Object *this_obj = find_object_by_id(this_obj_id);
                    if (this_obj) {
                        const char *instance_member_val = get_object_property_with_access(this_obj, var_name, current_class);
                        if (instance_member_val && strcmp(instance_member_val, "undefined") != 0) {
                            return instance_member_val;
                        }
                    }
                }
                Object *static_obj_for_current_class = find_static_class_object(current_class);
                if (static_obj_for_current_class) {
                     const char *static_member_val = get_object_property_with_access(static_obj_for_current_class, var_name, current_class);
                     if (static_member_val && strcmp(static_member_val, "undefined") != 0) {
                        return static_member_val;
                     }
                }
            }
            
            if (isupper((unsigned char)var_name[0])) { 
                 // Check if it's a registered class to return its name for static member access
                 // This needs vm.c to expose a "is_class_registered" or similar.
                 // For now, relying on semantic analysis to have typed it or this heuristic.
                 return var_name; 
            }

            // fprintf(stderr, "Error (L%d:%d): Undefined identifier '%s'.\n", expr_node->line, expr_node->col, var_name);
            return "undefined";
        }
            
        case AST_BINARY_OP: {
            if (strcmp(expr_node->value, "=") == 0) { 
                const char *rhs_val_str = evaluate_expression(expr_node->right, frame);
                if (expr_node->left->type == AST_IDENTIFIER) {
                    set_variable(frame, expr_node->left->value, rhs_val_str);
                    return rhs_val_str; 
                } else if (expr_node->left->type == AST_MEMBER_ACCESS) {
                    ASTNode *member_access = expr_node->left; 
                    ASTNode *target_node = member_access->left;   
                    const char *prop_name = member_access->value; 

                    const char *target_ref_str;
                    if (target_node->type == AST_THIS) target_ref_str = get_variable(frame, "this");
                    else target_ref_str = evaluate_expression(target_node, frame);

                    if (target_ref_str && strncmp(target_ref_str, "obj:", 4) == 0) { 
                        int obj_id = atoi(target_ref_str + 4);
                        Object *obj_instance = find_object_by_id(obj_id);
                        if (obj_instance) {
                            set_object_property_with_access(obj_instance, prop_name, rhs_val_str, ACCESS_PUBLIC, 0);
                            return rhs_val_str;
                        } else { fprintf(stderr, "Error (L%d:%d): Object %s not found for assignment to '%s'.\n", member_access->line, member_access->col, target_ref_str, prop_name); }
                    } else if (target_ref_str && isupper((unsigned char)target_ref_str[0])) { // Assume ClassName for static
                        Object* static_obj = find_static_class_object(target_ref_str);
                        if (static_obj) {
                             set_object_property_with_access(static_obj, prop_name, rhs_val_str, ACCESS_PUBLIC, 1);
                             return rhs_val_str;
                        } else { fprintf(stderr, "Error (L%d:%d): Class %s not found for static assignment to '%s'.\n", target_node->line, target_node->col, target_ref_str, prop_name); }
                    } else { fprintf(stderr, "Error (L%d:%d): Invalid target for member assignment to '%s'. Target was '%s'\n", target_node->line, target_node->col, prop_name, target_ref_str ? target_ref_str : "null");}
                    return "undefined"; 
                } else {
                    fprintf(stderr, "Error (L%d:%d): Invalid left-hand side in assignment.\n", expr_node->left->line, expr_node->left->col);
                    return "undefined";
                }
            } else { 
                const char *left_eval_result = evaluate_expression(expr_node->left, frame);
                char left_stable_val[1024]; 
                const char* left_final_val_ptr;

                if (left_eval_result == result_buffer) { 
                    strncpy(left_stable_val, result_buffer, sizeof(left_stable_val) - 1);
                    left_stable_val[sizeof(left_stable_val) - 1] = '\0';
                    left_final_val_ptr = left_stable_val;
                } else {
                    left_final_val_ptr = left_eval_result; 
                }

                if (strcmp(expr_node->value, "&&") == 0) {
                    int left_truthy = left_final_val_ptr && strcmp(left_final_val_ptr, "0") != 0 && strcmp(left_final_val_ptr, "false") != 0 && strcmp(left_final_val_ptr, "") != 0;
                    if (!left_truthy) return "false"; 
                    const char *right_eval_result = evaluate_expression(expr_node->right, frame);
                    int right_truthy = right_eval_result && strcmp(right_eval_result, "0") != 0 && strcmp(right_eval_result, "false") != 0 && strcmp(right_eval_result, "") != 0;
                    return right_truthy ? "true" : "false";
                }
                if (strcmp(expr_node->value, "||") == 0) {
                    int left_truthy = left_final_val_ptr && strcmp(left_final_val_ptr, "0") != 0 && strcmp(left_final_val_ptr, "false") != 0 && strcmp(left_final_val_ptr, "") != 0;
                    if (left_truthy) return "true"; 
                    const char *right_eval_result = evaluate_expression(expr_node->right, frame);
                    int right_truthy = right_eval_result && strcmp(right_eval_result, "0") != 0 && strcmp(right_eval_result, "false") != 0 && strcmp(right_eval_result, "") != 0;
                    return right_truthy ? "true" : "false";
                }
                const char *right_eval_result = evaluate_expression(expr_node->right, frame);
                return evaluate_binary_op_internal(expr_node, expr_node->value, left_final_val_ptr, right_eval_result);
            }
        } // End AST_BINARY_OP
        case AST_UNARY_OP: {
            const char *operand_val_str = evaluate_expression(expr_node->left, frame);
            if (strcmp(expr_node->value, "-") == 0) {
                if (!operand_val_str || !is_numeric_string(operand_val_str)) {
                     fprintf(stderr, "Error (L%d:%d): Unary '-' requires numeric operand, got '%s'.\n", expr_node->line, expr_node->col, operand_val_str ? operand_val_str : "undefined"); return "undefined";
                }
                double val = atof(operand_val_str);
                snprintf(result_buffer, sizeof(result_buffer), "%g", -val); return result_buffer;
            } else if (strcmp(expr_node->value, "!") == 0) {
                 int truthy = operand_val_str && strcmp(operand_val_str, "0") != 0 && strcmp(operand_val_str, "false") != 0 && strcmp(operand_val_str, "") != 0;
                 return truthy ? "false" : "true";
            }
            fprintf(stderr, "Error (L%d:%d): Unknown unary operator '%s'.\n", expr_node->line, expr_node->col, expr_node->value);
            return "undefined";
        }
        case AST_CALL: {
            char qualified_name_buf[512];
            if (expr_node->right) { 
                const char* target_str = evaluate_expression(expr_node->right, frame);
                snprintf(qualified_name_buf, sizeof(qualified_name_buf), "%s.%s", target_str ? target_str : "undefined_target", expr_node->value);
            } else { 
                strncpy(qualified_name_buf, expr_node->value, sizeof(qualified_name_buf)-1);
                qualified_name_buf[sizeof(qualified_name_buf)-1] = '\0';
            }
            return execute_function_call(qualified_name_buf, expr_node->left, frame);
        }
        case AST_ARRAY: {
            // Simplified: if parser put elements in value, use that. Otherwise, placeholder.
            if (expr_node->value[0] != '\0' && strcmp(expr_node->value, "array_literal") != 0) {
                return expr_node->value; 
            } else if (expr_node->left) { // Chain of expression nodes for elements
                // This needs to build a string representation or an actual array object.
                // For now, very basic string join into result_buffer for demo.
                result_buffer[0] = '['; result_buffer[1] = '\0';
                ASTNode* elem = expr_node->left;
                int first = 1;
                size_t current_len = 1;
                while(elem) {
                    if (!first) { strncat(result_buffer, ",", sizeof(result_buffer) - current_len -1); current_len++;}
                    const char* elem_val = evaluate_expression(elem, frame);
                    strncat(result_buffer, elem_val, sizeof(result_buffer) - current_len -1);
                    current_len += strlen(elem_val);
                    first = 0;
                    elem = elem->next;
                }
                strncat(result_buffer, "]", sizeof(result_buffer) - current_len -1);
                return result_buffer;
            }
            return "[array_obj_ref]"; 
        }
        case AST_NEW: {
            if (!expr_node->value[0]) {
                fprintf(stderr, "Error (L%d:%d): Class name missing in new expression\n", expr_node->line, expr_node->col);
                return "undefined";
            }
            Object *obj = create_object(expr_node->value); 
            if (!obj) {
                fprintf(stderr, "Error (L%d:%d): Failed to create object of class '%s'\n", expr_node->line, expr_node->col, expr_node->value);
                return "undefined";
            }
            int obj_id_val = 0;
            sscanf(obj->class_name, "%*[^#]#%d", &obj_id_val);
            snprintf(result_buffer, sizeof(result_buffer), "obj:%d", obj_id_val); 

            char constructor_qname[512];
            snprintf(constructor_qname, sizeof(constructor_qname), "%s.init", result_buffer );
            execute_function_call(constructor_qname, expr_node->left , frame);
            return result_buffer; // Return the object reference string
        }
        case AST_MEMBER_ACCESS: {
            return evaluate_member_access(expr_node, frame);
        }
        case AST_THIS: {
            const char *this_val = get_variable(frame, "this");
            if (!this_val) {
                fprintf(stderr, "Error (L%d:%d): 'this' is undefined in current context.\n", expr_node->line, expr_node->col);
                return "undefined";
            }
            return this_val;
        }
        case AST_INDEX_ACCESS: {
            const char* target_val_str = evaluate_expression(expr_node->left, frame);
            const char* index_val_str = evaluate_expression(expr_node->right, frame);
            
            // Rudimentary string/array indexing for demo, very unsafe and basic
            if (target_val_str && strncmp(target_val_str, "[", 1) == 0 && target_val_str[strlen(target_val_str)-1] == ']') { // Simple check for array literal string
                // This is a placeholder for actual array object indexing
                // It assumes target_val_str is like "[elem1,elem2,elem3]"
                int index_num = atoi(index_val_str);
                // Basic CSV-like parsing (very fragile)
                char temp_array_str[1024];
                strncpy(temp_array_str, target_val_str + 1, strlen(target_val_str) - 2);
                temp_array_str[strlen(target_val_str) - 2] = '\0';

                char* token = strtok(temp_array_str, ",");
                int count = 0;
                while(token != NULL) {
                    if (count == index_num) {
                        strncpy(result_buffer, token, sizeof(result_buffer)-1);
                        result_buffer[sizeof(result_buffer)-1] = '\0';
                        return result_buffer;
                    }
                    token = strtok(NULL, ",");
                    count++;
                }
                fprintf(stderr, "Warning (L%d:%d): Index %d out of bounds for pseudo-array '%s'.\n", expr_node->line, expr_node->col, index_num, target_val_str);
                return "undefined";

            } else if (target_val_str && is_numeric_string(index_val_str)) { // Basic string indexing
                int index = atoi(index_val_str);
                if (index >= 0 && index < (int)strlen(target_val_str)) {
                    snprintf(result_buffer, 2, "%c", target_val_str[index]);
                    return result_buffer;
                } else {
                     fprintf(stderr, "Warning (L%d:%d): Index %d out of bounds for string '%s'.\n", expr_node->line, expr_node->col, index, target_val_str);
                     return "undefined";
                }
            }
            // Fallback for unhandled index access
            snprintf(result_buffer, sizeof(result_buffer), "indexed_value_of_%s_at_%s", target_val_str ? target_val_str : "null", index_val_str ? index_val_str : "null");
            return result_buffer;
        }
        default:
            fprintf(stderr, "Error (L%d:%d): Cannot evaluate unknown AST node type %s (%d).\n", expr_node->line, expr_node->col, node_type_to_string(expr_node->type), expr_node->type);
            return "undefined";
    }
}

int is_numeric_string(const char *s) {
    if (!s || *s == '\0') return 0;
    char *endptr;
    strtod(s, &endptr); 
    while(isspace((unsigned char)*endptr)) endptr++; 
    return *endptr == '\0'; 
}

static const char* evaluate_binary_op_internal(ASTNode* expr_node, const char *op_str, const char *left_val_str_final, const char *right_val_str_final) {
    if (!op_str || !left_val_str_final || !right_val_str_final) return "undefined";
    
    if ( (strcmp(left_val_str_final, "undefined") == 0 || strcmp(right_val_str_final, "undefined") == 0) &&
         strcmp(op_str, "+") != 0 /* Allow string concat with undefined */ ) {
        fprintf(stderr, "Error (L%d:%d): Operand is undefined for binary operation '%s' (%s %s %s).\n", 
            expr_node->line, expr_node->col, op_str, left_val_str_final, op_str, right_val_str_final);
        return "undefined";
    }

    if (strcmp(op_str, "+") == 0) {
        if (is_numeric_string(left_val_str_final) && is_numeric_string(right_val_str_final)) {
            double result = atof(left_val_str_final) + atof(right_val_str_final);
            snprintf(result_buffer, sizeof(result_buffer), "%g", result);
        } else { 
            snprintf(result_buffer, sizeof(result_buffer), "%s%s", left_val_str_final, right_val_str_final);
        }
        return result_buffer;
    } else if (strchr("-*/%", op_str[0]) && op_str[1]=='\0') { 
        if (!is_numeric_string(left_val_str_final) || !is_numeric_string(right_val_str_final)) {
            fprintf(stderr, "Error (L%d:%d): Arithmetic op '%s' requires numeric operands, got '%s', '%s'.\n", expr_node->line, expr_node->col, op_str, left_val_str_final, right_val_str_final);
            return "undefined";
        }
        double l = atof(left_val_str_final); double r = atof(right_val_str_final);
        if (op_str[0] == '-') snprintf(result_buffer, sizeof(result_buffer), "%g", l-r);
        else if (op_str[0] == '*') snprintf(result_buffer, sizeof(result_buffer), "%g", l*r);
        else if (op_str[0] == '/') { if(r==0.0) {fprintf(stderr, "Error (L%d:%d): Division by zero.\n", expr_node->line, expr_node->col); return "undefined";} snprintf(result_buffer, sizeof(result_buffer), "%g", l/r); }
        else if (op_str[0] == '%') { if((long long)r==0) {fprintf(stderr, "Error (L%d:%d): Modulo by zero.\n", expr_node->line, expr_node->col); return "undefined";} snprintf(result_buffer, sizeof(result_buffer), "%lld", (long long)l % (long long)r); }
        return result_buffer;
    } else if (strcmp(op_str, "==") == 0 || strcmp(op_str, "!=") == 0 ||
               strcmp(op_str, "<") == 0 || strcmp(op_str, ">") == 0 ||
               strcmp(op_str, "<=")==0 || strcmp(op_str, ">=")==0 ) {
        int result_bool = 0;
        if(is_numeric_string(left_val_str_final) && is_numeric_string(right_val_str_final)){
            double l=atof(left_val_str_final), r=atof(right_val_str_final);
            if(strcmp(op_str, "==")==0) result_bool = (fabs(l-r) < 1e-9); // Float comparison
            else if(strcmp(op_str, "!=")==0) result_bool = (fabs(l-r) >= 1e-9);
            else if(strcmp(op_str, "<")==0) result_bool = (l<r);
            else if(strcmp(op_str, ">")==0) result_bool = (l>r);
            else if(strcmp(op_str, "<=")==0) result_bool = (l<=r);
            else if(strcmp(op_str, ">=")==0) result_bool = (l>=r);
        } else { // String comparison for equality/inequality only
            if(strcmp(op_str, "==")==0) result_bool = (strcmp(left_val_str_final, right_val_str_final)==0);
            else if(strcmp(op_str, "!=")==0) result_bool = (strcmp(left_val_str_final, right_val_str_final)!=0);
            else { fprintf(stderr, "Error (L%d:%d): Comparison '%s' not supported for non-numeric string types '%s', '%s' (only ==, !=).\n", expr_node->line, expr_node->col, op_str, left_val_str_final, right_val_str_final); return "undefined";}
        }
        return result_bool ? "true" : "false";
    } 
    // Note: && and || are handled with short-circuiting in the caller (evaluate_expression for AST_BINARY_OP)
    // This part would only be hit if they were passed directly, which shouldn't happen with the current structure.
    else if (strcmp(op_str, "&&") == 0 || strcmp(op_str, "||") == 0) {
        int l_truthy = left_val_str_final && strcmp(left_val_str_final, "0") !=0 && strcmp(left_val_str_final, "false") !=0 && strcmp(left_val_str_final, "") !=0 && strcmp(left_val_str_final, "undefined") !=0;
        int r_truthy = right_val_str_final && strcmp(right_val_str_final, "0") !=0 && strcmp(right_val_str_final, "false") !=0 && strcmp(right_val_str_final, "") !=0 && strcmp(right_val_str_final, "undefined") !=0;
        if(strcmp(op_str, "&&") == 0) return (l_truthy && r_truthy) ? "true" : "false";
        if(strcmp(op_str, "||") == 0) return (l_truthy || r_truthy) ? "true" : "false";
    }

    fprintf(stderr, "Error (L%d:%d): Unknown or unsupported binary operator '%s' in eval_binary_op_internal.\n", expr_node->line, expr_node->col, op_str);
    return "undefined";
}