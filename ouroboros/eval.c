#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "eval.h"
#include "ast_types.h"

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
            const char *value = get_variable(frame, expr->value);
            return value ? value : "undefined";
        }
            
        case AST_BINARY_OP: {
            const char *left = evaluate_expression(expr->left, frame);
            const char *right = evaluate_expression(expr->right, frame);
            return evaluate_binary_op(expr->value, left, right);
        }
            
        case AST_CALL: {
            // Function calls should be handled by the VM
            // This shouldn't normally be reached in expressions
            // but if it is, we need to handle it properly
            extern const char* execute_function_call(const char *name, ASTNode *args, StackFrame *frame);
            return execute_function_call(expr->value, expr->left, frame);
        }
            
        case AST_ARRAY: {
            // For simplicity, we'll just return a string indicating an array
            return "[array]";
        }
            
        default:
            return "undefined";
    }
}

// Function to evaluate a binary operation
static const char* evaluate_binary_op(const char *op, const char *left, const char *right) {
    // Handle numeric operations
    if (strcmp(op, "+") == 0) {
        // Check if both operands are numbers
        char *end_left, *end_right;
        double left_num = strtod(left, &end_left);
        double right_num = strtod(right, &end_right);
        
        if (*end_left == '\0' && *end_right == '\0') {
            // Both are numbers, perform addition
            double result = left_num + right_num;
            snprintf(result_buffer, sizeof(result_buffer), "%g", result);
            return result_buffer;
        } else {
            // String concatenation
            snprintf(result_buffer, sizeof(result_buffer), "%s%s", left, right);
            return result_buffer;
        }
    } else if (strcmp(op, "-") == 0) {
        // Subtraction (numeric only)
        double left_num = atof(left);
        double right_num = atof(right);
        double result = left_num - right_num;
        snprintf(result_buffer, sizeof(result_buffer), "%g", result);
        return result_buffer;
    } else if (strcmp(op, "*") == 0) {
        // Multiplication (numeric only)
        double left_num = atof(left);
        double right_num = atof(right);
        double result = left_num * right_num;
        snprintf(result_buffer, sizeof(result_buffer), "%g", result);
        return result_buffer;
    } else if (strcmp(op, "/") == 0) {
        // Division (numeric only)
        double left_num = atof(left);
        double right_num = atof(right);
        if (right_num == 0) {
            return "NaN"; // Division by zero
        }
        double result = left_num / right_num;
        snprintf(result_buffer, sizeof(result_buffer), "%g", result);
        return result_buffer;
    } else if (strcmp(op, "%") == 0) {
        // Modulo (numeric only)
        int left_num = atoi(left);
        int right_num = atoi(right);
        if (right_num == 0) {
            return "NaN"; // Modulo by zero
        }
        int result = left_num % right_num;
        snprintf(result_buffer, sizeof(result_buffer), "%d", result);
        return result_buffer;
    } else if (strcmp(op, "==") == 0) {
        // Equality
        if (strcmp(left, right) == 0) {
            return "1";
        } else {
            return "0";
        }
    } else if (strcmp(op, "!=") == 0) {
        // Inequality
        if (strcmp(left, right) != 0) {
            return "1";
        } else {
            return "0";
        }
    } else if (strcmp(op, ">") == 0) {
        // Greater than
        double left_num = atof(left);
        double right_num = atof(right);
        if (left_num > right_num) {
            return "1";
        } else {
            return "0";
        }
    } else if (strcmp(op, "<") == 0) {
        // Less than
        double left_num = atof(left);
        double right_num = atof(right);
        if (left_num < right_num) {
            return "1";
        } else {
            return "0";
        }
    } else if (strcmp(op, ">=") == 0) {
        // Greater than or equal
        double left_num = atof(left);
        double right_num = atof(right);
        if (left_num >= right_num) {
            return "1";
        } else {
            return "0";
        }
    } else if (strcmp(op, "<=") == 0) {
        // Less than or equal
        double left_num = atof(left);
        double right_num = atof(right);
        if (left_num <= right_num) {
            return "1";
        } else {
            return "0";
        }
    } else if (strcmp(op, "&&") == 0) {
        // Logical AND
        if (strcmp(left, "0") != 0 && strcmp(left, "") != 0 &&
            strcmp(right, "0") != 0 && strcmp(right, "") != 0) {
            return "1";
        } else {
            return "0";
        }
    } else if (strcmp(op, "||") == 0) {
        // Logical OR
        if (strcmp(left, "0") != 0 && strcmp(left, "") != 0 ||
            strcmp(right, "0") != 0 && strcmp(right, "") != 0) {
            return "1";
        } else {
            return "0";
        }
    }
    
    // Unknown operator
    fprintf(stderr, "Warning: Unknown operator '%s'\n", op);
    return "undefined";
}
