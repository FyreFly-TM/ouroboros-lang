#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h> // For isupper
#include "semantic.h" 
#include "ast_types.h" // For node_type_to_string, ASTNode
#include "parser.h" // For is_builtin_type_keyword (ideally move to a common util)


// --- Symbol Table Implementation ---
SymbolTable* g_st = NULL; 

SymbolTable* symbol_table_create() {
    SymbolTable* st = (SymbolTable*)malloc(sizeof(SymbolTable));
    if (!st) {
        fprintf(stderr, "Fatal Error: Could not allocate symbol table.\n");
        exit(EXIT_FAILURE);
    }
    st->current_scope_idx = -1; 
    st->next_scope_level_to_assign = 0;
    symbol_table_enter_scope(st, "global"); 
    return st;
}

void symbol_table_destroy(SymbolTable* st) {
    if (!st) return;
    while (st->current_scope_idx >= 0) {
        Scope* current = st->scope_stack[st->current_scope_idx];
        free(current);
        st->scope_stack[st->current_scope_idx] = NULL;
        st->current_scope_idx--;
    }
    free(st);
}

Scope* symbol_table_get_current_scope(SymbolTable* st) {
    if (!st || st->current_scope_idx < 0) {
        return NULL;
    }
    return st->scope_stack[st->current_scope_idx];
}

void symbol_table_enter_scope(SymbolTable* st, const char* scope_name) {
    if (!st) return;
    if (st->current_scope_idx + 1 >= MAX_SCOPE_DEPTH) {
        fprintf(stderr, "Fatal Error: Maximum scope depth (%d) exceeded for scope '%s'.\n", MAX_SCOPE_DEPTH, scope_name);
        exit(EXIT_FAILURE); 
    }
    Scope* new_scope = (Scope*)calloc(1, sizeof(Scope)); // Use calloc for zero-initialization
    if (!new_scope) {
        fprintf(stderr, "Fatal Error: Could not allocate new scope '%s'.\n", scope_name);
        exit(EXIT_FAILURE);
    }
    new_scope->parent_scope = (st->current_scope_idx >= 0) ? st->scope_stack[st->current_scope_idx] : NULL;
    new_scope->level = st->next_scope_level_to_assign++;
    strncpy(new_scope->scope_name, scope_name, sizeof(new_scope->scope_name) - 1);
    new_scope->scope_name[sizeof(new_scope->scope_name) - 1] = '\0';

    st->current_scope_idx++;
    st->scope_stack[st->current_scope_idx] = new_scope;
    // printf("[Scope] Entered scope: %s (level %d)\n", scope_name, new_scope->level);
}

void symbol_table_exit_scope(SymbolTable* st) {
    if (!st || st->current_scope_idx < 0) {
        fprintf(stderr, "Warning: Attempted to exit scope when no scope is active.\n");
        return;
    }
    Scope* exited_scope = st->scope_stack[st->current_scope_idx];
    // printf("[Scope] Exited scope: %s (level %d)\n", exited_scope->scope_name, exited_scope->level);
    
    free(exited_scope);
    st->scope_stack[st->current_scope_idx] = NULL;
    st->current_scope_idx--;
}

int symbol_table_add_symbol(SymbolTable* st, const char* name, SymbolKind kind, const char* type_name, ASTNode* decl_node) {
    if (!st) return 0;
    Scope* current_scope = symbol_table_get_current_scope(st);
    if (!current_scope) {
        fprintf(stderr, "Error (L%d:%d): Cannot add symbol '%s', no active scope.\n", decl_node->line, decl_node->col, name);
        return 0;
    }

    for (int i = 0; i < current_scope->symbol_count; ++i) {
        if (strcmp(current_scope->symbols[i].name, name) == 0) {
            fprintf(stderr, "[SEMANTIC L%d:%d] Error: Symbol '%s' already defined in this scope (previous def at L%d:%d as %s).\n",
                    decl_node->line, decl_node->col, name, 
                    current_scope->symbols[i].declaration_node->line, current_scope->symbols[i].declaration_node->col,
                    current_scope->symbols[i].type_name);
            return 0; 
        }
    }

    if (current_scope->symbol_count >= MAX_SCOPE_SYMBOLS) {
        fprintf(stderr, "Error (L%d:%d): Maximum symbols (%d) reached in scope '%s' when adding '%s'.\n",
                decl_node->line, decl_node->col, MAX_SCOPE_SYMBOLS, current_scope->scope_name, name);
        return 0;
    }

    Symbol* new_sym = &current_scope->symbols[current_scope->symbol_count];
    strncpy(new_sym->name, name, sizeof(new_sym->name) - 1);
    new_sym->name[sizeof(new_sym->name) - 1] = '\0';
    new_sym->kind = kind;
    if (type_name) {
        strncpy(new_sym->type_name, type_name, sizeof(new_sym->type_name) - 1);
        new_sym->type_name[sizeof(new_sym->type_name) - 1] = '\0';
    } else {
        strcpy(new_sym->type_name, "unknown_type"); 
    }
    new_sym->declaration_node = decl_node;
    new_sym->scope_level = current_scope->level;

    current_scope->symbol_count++;
    // printf("  [Symbol Added in %s (L%d)]: %s (Kind: %d, Type: %s)\n", current_scope->scope_name, decl_node->line, name, kind, new_sym->type_name);
    return 1; 
}

Symbol* symbol_table_lookup_current_scope(SymbolTable* st, const char* name) {
    if (!st) return NULL;
    Scope* current_scope = symbol_table_get_current_scope(st);
    if (!current_scope) return NULL;

    for (int i = 0; i < current_scope->symbol_count; ++i) {
        if (strcmp(current_scope->symbols[i].name, name) == 0) {
            return &current_scope->symbols[i];
        }
    }
    return NULL; 
}

Symbol* symbol_table_lookup_all_scopes(SymbolTable* st, const char* name) {
    if (!st) return NULL;
    Scope* scope_to_search = symbol_table_get_current_scope(st);
    while (scope_to_search) {
        for (int i = 0; i < scope_to_search->symbol_count; ++i) {
            if (strcmp(scope_to_search->symbols[i].name, name) == 0) {
                return &scope_to_search->symbols[i];
            }
        }
        scope_to_search = scope_to_search->parent_scope; 
    }
    return NULL; 
}


// --- Forward declarations for analysis functions ---
static void analyze_node(ASTNode *node); 
static void analyze_function_decl(ASTNode *func_node, ASTNode *parent_class_node_or_null);
static void analyze_block_stmts(ASTNode *block_node);
static void analyze_var_decl_stmt(ASTNode *decl_node);
static void analyze_assignment_stmt(ASTNode *assign_node);
static void analyze_return_stmt(ASTNode *return_node);
static void analyze_if_stmt(ASTNode *if_node);
static void analyze_while_stmt(ASTNode *while_node);
static void analyze_for_stmt(ASTNode *for_node);
static void analyze_call_expr_or_stmt(ASTNode *call_node);
static const char* analyze_expression_node(ASTNode *expr_node); 
static void analyze_struct_decl(ASTNode *struct_node);
static void analyze_class_decl(ASTNode *class_node);
static const char* analyze_member_access_expr(ASTNode *access_node); // Changed to return type
static void analyze_new_expr(ASTNode *new_node);


static void analyze_function_decl(ASTNode *func_node, ASTNode *parent_class_node_or_null) {
    const char* func_type_str = (func_node->type == AST_TYPED_FUNCTION) ? "Typed Function" : "Function";
    // printf("[SEMANTIC L%d:%d] Analyzing %s: %s", func_node->line, func_node->col, func_type_str, func_node->value);
    
    const char* func_return_type = (func_node->type == AST_TYPED_FUNCTION && func_node->data_type[0]) ? func_node->data_type : "any"; 
    if (parent_class_node_or_null) { // It's a method
        // This symbol is added within the class's scope when analyze_class_decl calls this.
        // printf(" (Method of class %s)", parent_class_node_or_null->value);
    } else { // Global function
        if (!symbol_table_add_symbol(g_st, func_node->value, SYMBOL_FUNCTION, func_return_type, func_node)) {
            return; 
        }
    }
    // printf("\n");
    
    char scope_name_buf[256];
    if (parent_class_node_or_null) {
        snprintf(scope_name_buf, sizeof(scope_name_buf), "method_%s.%s", parent_class_node_or_null->value, func_node->value);
    } else {
        snprintf(scope_name_buf, sizeof(scope_name_buf), "function_%s", func_node->value);
    }
    symbol_table_enter_scope(g_st, scope_name_buf);

    if (func_node->left) { 
        ASTNode *param = func_node->left;
        while (param) {
            if (param->type == AST_PARAMETER) {
                const char* param_type_name = param->data_type[0] ? param->data_type : "any";
                symbol_table_add_symbol(g_st, param->value, SYMBOL_PARAMETER, param_type_name, param);
            }
            param = param->next;
        }
    }
    
    if (func_node->right && func_node->right->type == AST_BLOCK) {
        analyze_block_stmts(func_node->right); 
    }
    symbol_table_exit_scope(g_st);
}

static void analyze_block_stmts(ASTNode *block_node) {
    symbol_table_enter_scope(g_st, "block");
    ASTNode *stmt = block_node->left;
    while (stmt) {
        analyze_node(stmt);
        stmt = stmt->next;
    }
    symbol_table_exit_scope(g_st);
}

static void analyze_var_decl_stmt(ASTNode *decl_node) {
    const char* declared_type_name = "any"; 
    if (decl_node->type == AST_TYPED_VAR_DECL && decl_node->data_type[0]) {
        declared_type_name = decl_node->data_type;
        if (!is_builtin_type_keyword(declared_type_name) && !symbol_table_lookup_all_scopes(g_st, declared_type_name)) {
             fprintf(stderr, "[SEMANTIC L%d:%d] Error: Unknown type '%s' for variable '%s'.\n", 
                     decl_node->line, decl_node->col, declared_type_name, decl_node->value);
        }
    }
    
    if (!symbol_table_add_symbol(g_st, decl_node->value, SYMBOL_VARIABLE, declared_type_name, decl_node)) {
        return; 
    }
    strncpy(decl_node->data_type, declared_type_name, sizeof(decl_node->data_type)-1);
    decl_node->data_type[sizeof(decl_node->data_type)-1] = '\0';


    if (decl_node->right) { 
        const char* initializer_type = analyze_expression_node(decl_node->right);
        if (strcmp(declared_type_name, "any") != 0 && strcmp(initializer_type, "any") != 0 &&
            strcmp(declared_type_name, initializer_type) != 0 && strcmp(initializer_type, "error_type") != 0) {
            if (!( (strcmp(declared_type_name, "float")==0 || strcmp(declared_type_name, "double")==0) && 
                   (strcmp(initializer_type, "int")==0 || strcmp(initializer_type, "long")==0) ) &&
                !( strstr(declared_type_name, "[]") && strcmp(initializer_type, "array")==0 ) // Allow assigning generic array literal to typed array
                ) { 
                fprintf(stderr, "[SEMANTIC L%d:%d] Type Mismatch: Cannot initialize variable '%s' (type %s) with expression of type %s.\n",
                        decl_node->line, decl_node->col, decl_node->value, declared_type_name, initializer_type);
            }
        }
    }
}

static void analyze_assignment_stmt(ASTNode *assign_node) {
    const char* lhs_type = analyze_expression_node(assign_node->left); 
    if (strcmp(lhs_type, "error_type")==0) return; 

    if (assign_node->left->type == AST_LITERAL || assign_node->left->type == AST_CALL ) { 
        Symbol* sym_lhs = symbol_table_lookup_all_scopes(g_st, assign_node->left->value);
        if(assign_node->left->type == AST_CALL && sym_lhs && strcmp(sym_lhs->type_name, "void")==0) {
             fprintf(stderr, "[SEMANTIC L%d:%d] Error: Left-hand side of assignment (void function call) is not assignable.\n", 
                assign_node->left->line, assign_node->left->col);
            return;
        } else if (assign_node->left->type == AST_LITERAL) {
            fprintf(stderr, "[SEMANTIC L%d:%d] Error: Left-hand side of assignment (literal) is not assignable.\n", 
                assign_node->left->line, assign_node->left->col);
            return;
        }
    }


    if (assign_node->right) { 
        const char* rhs_type = analyze_expression_node(assign_node->right);
        if (strcmp(rhs_type, "error_type")==0) return; 

        if (strcmp(lhs_type, "any") != 0 && strcmp(rhs_type, "any") != 0 &&
            strcmp(lhs_type, rhs_type) != 0) {
            if (!( (strcmp(lhs_type, "float")==0 || strcmp(lhs_type, "double")==0) && 
                   (strcmp(rhs_type, "int")==0 || strcmp(rhs_type, "long")==0) ) &&
                !( strstr(lhs_type, "[]") && strcmp(rhs_type, "array")==0 ) 
                ) {
                fprintf(stderr, "[SEMANTIC L%d:%d] Type Mismatch: Cannot assign expression of type %s to target of type %s.\n",
                        assign_node->line, assign_node->col, rhs_type, lhs_type);
            }
        }
    } else {
        fprintf(stderr, "[SEMANTIC L%d:%d] Error: Assignment statement missing right-hand side.\n", assign_node->line, assign_node->col);
    }
}

static void analyze_return_stmt(ASTNode *return_node) {
    Scope* func_scope = symbol_table_get_current_scope(g_st);
    Symbol* func_sym = NULL;

    while(func_scope){ // Find the enclosing function's symbol
        if(strncmp(func_scope->scope_name, "method_", strlen("method_")) == 0 || strncmp(func_scope->scope_name, "function_", strlen("function_")) == 0) {
            char func_name_from_scope[128];
            const char* name_start = strchr(func_scope->scope_name, '_') + 1;
            const char* method_sep = strchr(name_start, '.'); // For methods like "ClassName.methodName"
            
            Scope* search_in_scope = func_scope->parent_scope; // Search in parent (class or global)

            if (method_sep) { // Method
                // This logic is a bit complex as method name is combined with class in scope name
                // We need the actual function symbol.
                // This part needs refinement for robustly finding the method's symbol.
                // For now, we'll assume func_sym would be found by searching the class scope.
            } else { // Global function
                 strncpy(func_name_from_scope, name_start, sizeof(func_name_from_scope)-1);
                 func_name_from_scope[sizeof(func_name_from_scope)-1] = '\0';
                 if(search_in_scope) func_sym = symbol_table_lookup_current_scope(search_in_scope, func_name_from_scope);
            }
            break; 
        }
        if(strcmp(func_scope->scope_name, "global")==0) break;
        func_scope = func_scope->parent_scope;
    }
    
    const char* expected_return_type = func_sym ? func_sym->type_name : "any";

    if (return_node->left) { 
        const char* actual_return_type = analyze_expression_node(return_node->left);
        if (strcmp(expected_return_type, "void") == 0 && strcmp(actual_return_type, "void") != 0 && strcmp(actual_return_type, "any") !=0 && strcmp(actual_return_type, "error_type") != 0 ) {
             fprintf(stderr, "[SEMANTIC L%d:%d] Error: Function declared as void cannot return a value of type '%s'.\n",
                    return_node->line, return_node->col, actual_return_type);
        } else if (strcmp(expected_return_type, "void") != 0 && strcmp(actual_return_type, "void") == 0 ) {
             fprintf(stderr, "[SEMANTIC L%d:%d] Error: Function expects return type '%s' but got void/no value.\n",
                    return_node->line, return_node->col, expected_return_type);
        }
        else if (strcmp(expected_return_type, "any") != 0 && strcmp(actual_return_type, "any") != 0 &&
            strcmp(expected_return_type, actual_return_type) != 0 && strcmp(actual_return_type, "error_type") != 0) {
            fprintf(stderr, "[SEMANTIC L%d:%d] Type Mismatch: Function expects return type %s but got %s.\n",
                    return_node->line, return_node->col, expected_return_type, actual_return_type);
        }
    } else { 
        if (strcmp(expected_return_type, "void") != 0 && strcmp(expected_return_type, "any") != 0) {
            fprintf(stderr, "[SEMANTIC L%d:%d] Error: Function expects return type %s but no value was returned.\n",
                    return_node->line, return_node->col, expected_return_type);
        }
    }
}

static void analyze_if_stmt(ASTNode *if_node) {
    if (if_node->left) { 
        const char* cond_type = analyze_expression_node(if_node->left);
        if (strcmp(cond_type, "bool") != 0 && strcmp(cond_type, "any") != 0 && strcmp(cond_type, "error_type") != 0) {
            fprintf(stderr, "[SEMANTIC L%d:%d] Warning: If condition is type '%s', expected boolean.\n",
                    if_node->left->line, if_node->left->col, cond_type);
        }
    } else { /* error handled by parser */ }
    if (if_node->right) analyze_node(if_node->right); else { /* error */ }
    if (if_node->next && if_node->next->type == AST_ELSE) analyze_node(if_node->next->left); 
}

static void analyze_while_stmt(ASTNode *while_node) {
    if (while_node->left) { 
        const char* cond_type = analyze_expression_node(while_node->left);
         if (strcmp(cond_type, "bool") != 0 && strcmp(cond_type, "any") != 0 && strcmp(cond_type, "error_type") != 0) {
            fprintf(stderr, "[SEMANTIC L%d:%d] Warning: While condition is type '%s', expected boolean.\n",
                    while_node->left->line, while_node->left->col, cond_type);
        }
    }  else { /* error */ }
    if (while_node->right) analyze_node(while_node->right);  else { /* error */ }
}

static void analyze_for_stmt(ASTNode *for_node) {
    symbol_table_enter_scope(g_st, "for_loop");
    ASTNode* init_node = NULL, *cond_node = NULL, *incr_node = NULL;
    ASTNode* control_chain = for_node->left;
    if (control_chain) { init_node = control_chain; control_chain = control_chain->next; }
    if (control_chain) { cond_node = control_chain; control_chain = control_chain->next; }
    if (control_chain) { incr_node = control_chain; }

    if (init_node) analyze_node(init_node); 
    if (cond_node) {
        const char* cond_type = analyze_expression_node(cond_node);
        if (strcmp(cond_type, "bool") != 0 && strcmp(cond_type, "any") != 0 && strcmp(cond_type, "error_type") != 0) {
            fprintf(stderr, "[SEMANTIC L%d:%d] Warning: For loop condition is type '%s', expected boolean.\n",
                    cond_node->line, cond_node->col, cond_type);
        }
    }
    if (incr_node) analyze_expression_node(incr_node);
    if (for_node->right) analyze_node(for_node->right); else { /* error */ }
    symbol_table_exit_scope(g_st);
}

static void analyze_call_expr_or_stmt(ASTNode *call_node) {
    Symbol* func_sym = NULL;
    const char* class_context_for_method_lookup = NULL;
    char actual_func_name_to_lookup[128]; 
    strncpy(actual_func_name_to_lookup, call_node->value, sizeof(actual_func_name_to_lookup)-1);
    actual_func_name_to_lookup[sizeof(actual_func_name_to_lookup)-1] = '\0';

    if (call_node->right) { 
        const char* target_type = analyze_expression_node(call_node->right);
        if (strcmp(target_type, "error_type")!=0 && strcmp(target_type, "any")!=0) {
            class_context_for_method_lookup = target_type; 
        }
    }
    
    if (class_context_for_method_lookup) { 
        Symbol* class_sym = symbol_table_lookup_all_scopes(g_st, class_context_for_method_lookup);
        if (class_sym && (class_sym->kind == SYMBOL_CLASS || class_sym->kind == SYMBOL_STRUCT)) {
            ASTNode* class_decl_node = class_sym->declaration_node;
            ASTNode* member_node = class_decl_node->left; // Start of members
            while(member_node) {
                if((member_node->type == AST_FUNCTION || member_node->type == AST_TYPED_FUNCTION) &&
                   strcmp(member_node->value, actual_func_name_to_lookup) == 0) {
                    // Found method declaration node, use its type for func_sym
                    // This is a bit manual. Ideally, class scope would be directly searchable.
                    // For now, construct a temporary Symbol-like info.
                    func_sym = (Symbol*)malloc(sizeof(Symbol)); // Temp, needs freeing or better way
                    if(func_sym) {
                        strcpy(func_sym->name, member_node->value);
                        func_sym->kind = SYMBOL_FUNCTION;
                        strcpy(func_sym->type_name, member_node->data_type[0] ? member_node->data_type : "any");
                        func_sym->declaration_node = member_node;
                    }
                    break;
                }
                member_node = member_node->next;
            }
        }
    } else { 
        func_sym = symbol_table_lookup_all_scopes(g_st, actual_func_name_to_lookup);
    }

    if (!func_sym) {
        if (!strchr(call_node->value, '.') && 
            strcmp(call_node->value, "print") != 0 && strcmp(call_node->value, "get_input") != 0 &&
            !strstr(call_node->value, "opengl_") && !strstr(call_node->value, "vulkan_") ) { 
             fprintf(stderr, "[SEMANTIC L%d:%d] Error: Function or method '%s' not found.\n", call_node->line, call_node->col, call_node->value);
        }
        strncpy(call_node->data_type, "any", sizeof(call_node->data_type)-1); // Assume any if not found
    } else if (func_sym->kind != SYMBOL_FUNCTION) {
        fprintf(stderr, "[SEMANTIC L%d:%d] Error: '%s' is a %s, not a function or method.\n", call_node->line, call_node->col, call_node->value, func_sym->type_name /* kind as string would be better */);
        strncpy(call_node->data_type, "error_type", sizeof(call_node->data_type)-1);
    } else {
        strncpy(call_node->data_type, func_sym->type_name, sizeof(call_node->data_type)-1);
        // TODO: Argument checking
        ASTNode* actual_arg_node = call_node->left;
        ASTNode* formal_param_node = func_sym->declaration_node->left;
        int arg_count = 0;
        while(actual_arg_node) {
            const char* actual_arg_type = analyze_expression_node(actual_arg_node);
            if (!formal_param_node) {
                fprintf(stderr, "[SEMANTIC L%d:%d] Error: Too many arguments for function '%s'.\n", actual_arg_node->line, actual_arg_node->col, func_sym->name);
                break;
            }
            const char* formal_param_type = formal_param_node->data_type[0] ? formal_param_node->data_type : "any";
            if(strcmp(formal_param_type, "any") != 0 && strcmp(actual_arg_type, "any") != 0 &&
               strcmp(formal_param_type, actual_arg_type) != 0 && strcmp(actual_arg_type, "error_type") != 0) {
                 if (!( (strcmp(formal_param_type, "float")==0 ) && (strcmp(actual_arg_type, "int")==0 )) &&
                      !( strstr(formal_param_type, "[]") && strcmp(actual_arg_type, "array")==0 ) )
                 {
                    fprintf(stderr, "[SEMANTIC L%d:%d] Type Mismatch: Argument %d for function '%s'. Expected %s, got %s.\n",
                            actual_arg_node->line, actual_arg_node->col, arg_count + 1, func_sym->name, formal_param_type, actual_arg_type);
                 }
            }
            actual_arg_node = actual_arg_node->next;
            formal_param_node = formal_param_node->next;
            arg_count++;
        }
        if (formal_param_node != NULL) {
            fprintf(stderr, "[SEMANTIC L%d:%d] Error: Too few arguments for function '%s'.\n", call_node->line, call_node->col, func_sym->name);
        }
    }
    if (class_context_for_method_lookup && func_sym && func_sym->declaration_node == NULL) { // Crude check if it was a temp symbol
        free(func_sym);
    }
}

static const char* analyze_expression_node(ASTNode *expr_node) {
    if (!expr_node) return "error_type"; 
    
    const char* inferred_type = "any"; 

    switch (expr_node->type) {
        case AST_LITERAL:
            if (expr_node->data_type[0] == '\0') { 
                if (strcmp(expr_node->value, "true") == 0 || strcmp(expr_node->value, "false") == 0) strcpy(expr_node->data_type, "bool");
                else if (strchr(expr_node->value, '.') != NULL || strchr(expr_node->value, 'e') != NULL || strchr(expr_node->value, 'E') != NULL) { 
                    char* endptr; strtod(expr_node->value, &endptr);
                    if (*endptr == '\0') strcpy(expr_node->data_type, "float"); else strcpy(expr_node->data_type, "string"); 
                } else { 
                    char* endptr; strtol(expr_node->value, &endptr, 10);
                    if (*endptr == '\0') strcpy(expr_node->data_type, "int"); else strcpy(expr_node->data_type, "string"); 
                }
            }
            inferred_type = expr_node->data_type;
            break;
            
        case AST_IDENTIFIER: {
            Symbol* sym = symbol_table_lookup_all_scopes(g_st, expr_node->value);
            if (sym) {
                inferred_type = sym->type_name;
                strncpy(expr_node->data_type, sym->type_name, sizeof(expr_node->data_type)-1);
                expr_node->data_type[sizeof(expr_node->data_type)-1] = '\0';
            } else {
                if (isupper((unsigned char)expr_node->value[0])) { 
                    strncpy(expr_node->data_type, expr_node->value, sizeof(expr_node->data_type)-1); 
                    expr_node->data_type[sizeof(expr_node->data_type)-1] = '\0';
                    inferred_type = expr_node->data_type;
                } else {
                    fprintf(stderr, "[SEMANTIC L%d:%d] Error: Undefined identifier '%s'.\n", expr_node->line, expr_node->col, expr_node->value);
                    strcpy(expr_node->data_type, "error_type"); 
                    inferred_type = "error_type";
                }
            }
            break;
        }
        case AST_BINARY_OP: {
            const char* left_type = analyze_expression_node(expr_node->left);
            const char* right_type = analyze_expression_node(expr_node->right);
            if (strcmp(left_type, "error_type")==0 || strcmp(right_type, "error_type")==0) {
                inferred_type = "error_type";
            } else if (strcmp(expr_node->value, "+")==0) { 
                if ((strcmp(left_type, "int")==0 || strcmp(left_type, "float")==0) && 
                    (strcmp(right_type, "int")==0 || strcmp(right_type, "float")==0)) {
                    inferred_type = (strcmp(left_type, "float")==0 || strcmp(right_type, "float")==0) ? "float" : "int";
                } else if (strcmp(left_type, "string")==0 || strcmp(right_type, "string")==0) {
                    inferred_type = "string";
                } else if (strcmp(left_type, "any")==0 || strcmp(right_type, "any")==0) {
                    inferred_type = "any"; 
                } else {
                     fprintf(stderr, "[SEMANTIC L%d:%d] Error: Invalid operands for binary '+': types '%s' and '%s'.\n", expr_node->line, expr_node->col, left_type, right_type);
                     inferred_type = "error_type";
                }
            } else if (strchr("-*/%", expr_node->value[0]) && expr_node->value[1]=='\0') { 
                 if ((strcmp(left_type, "int")==0 || strcmp(left_type, "float")==0) && 
                    (strcmp(right_type, "int")==0 || strcmp(right_type, "float")==0)) {
                    inferred_type = (strcmp(left_type, "float")==0 || strcmp(right_type, "float")==0) ? "float" : "int";
                    if (expr_node->value[0] == '/' && (strcmp(inferred_type, "int") == 0)) {
                        inferred_type = "float"; 
                    }
                } else if (strcmp(left_type, "any")==0 || strcmp(right_type, "any")==0) {
                    inferred_type = "any";
                } else {
                     fprintf(stderr, "[SEMANTIC L%d:%d] Error: Invalid operands for binary '%s': types '%s' and '%s'.\n", expr_node->line, expr_node->col, expr_node->value, left_type, right_type);
                     inferred_type = "error_type";
                }
            } else if (strcmp(expr_node->value, "==")==0 || strcmp(expr_node->value, "!=")==0 ||
                       strcmp(expr_node->value, "<")==0 || strcmp(expr_node->value, ">")==0 ||
                       strcmp(expr_node->value, "<=")==0 || strcmp(expr_node->value, ">=")==0 ||
                       strcmp(expr_node->value, "&&")==0 || strcmp(expr_node->value, "||")==0) {
                inferred_type = "bool";
            } else if (strcmp(expr_node->value, "=")==0) { 
                 inferred_type = right_type; 
            } else {
                inferred_type = "any"; 
            }
            strncpy(expr_node->data_type, inferred_type, sizeof(expr_node->data_type)-1);
            expr_node->data_type[sizeof(expr_node->data_type)-1] = '\0';
            break;
        }
        case AST_UNARY_OP: {
            const char* operand_type = analyze_expression_node(expr_node->left);
            if (strcmp(operand_type, "error_type")==0) {
                inferred_type = "error_type";
            } else if (strcmp(expr_node->value, "-")==0 || strcmp(expr_node->value, "+")==0 ) {
                if (strcmp(operand_type, "int")==0 || strcmp(operand_type, "float")==0) {
                    inferred_type = operand_type;
                } else if (strcmp(operand_type, "any")==0) {
                    inferred_type = "any";
                } else {
                    fprintf(stderr, "[SEMANTIC L%d:%d] Error: Invalid operand for unary '%s': type '%s'.\n", expr_node->line, expr_node->col, expr_node->value, operand_type);
                    inferred_type = "error_type";
                }
            } else if (strcmp(expr_node->value, "!")==0) {
                inferred_type = "bool";
            } else {
                inferred_type = "any";
            }
            strncpy(expr_node->data_type, inferred_type, sizeof(expr_node->data_type)-1);
            expr_node->data_type[sizeof(expr_node->data_type)-1] = '\0';
            break;
        }
        case AST_CALL:
            analyze_call_expr_or_stmt(expr_node); 
            inferred_type = expr_node->data_type; 
            break;
        case AST_ARRAY:
            if (expr_node->left) {
                char common_elem_type[64] = "any";
                ASTNode* elem = expr_node->left;
                const char* first_elem_type = analyze_expression_node(elem);
                if(strcmp(first_elem_type, "error_type") != 0) strcpy(common_elem_type, first_elem_type);
                elem = elem->next;
                while(elem){
                    const char* current_elem_type = analyze_expression_node(elem);
                    if(strcmp(common_elem_type, current_elem_type) != 0 && strcmp(current_elem_type, "error_type") !=0){
                        strcpy(common_elem_type, "any"); // Mixed types or error
                        break;
                    }
                    elem = elem->next;
                }
                snprintf(expr_node->data_type, sizeof(expr_node->data_type), "%s[]", common_elem_type);
            } else {
                 strcpy(expr_node->data_type, "any[]"); // Empty array, type unknown or 'any'
            }
            inferred_type = expr_node->data_type;
            break;
        case AST_NEW:
            analyze_new_expr(expr_node); 
            inferred_type = expr_node->data_type;
            break;
        case AST_MEMBER_ACCESS:
            inferred_type = analyze_member_access_expr(expr_node); 
            break;
        case AST_THIS: {
            Scope* current_scope = symbol_table_get_current_scope(g_st);
            char class_name_from_scope[128] = {0};
            while(current_scope) {
                if(strncmp(current_scope->scope_name, "class_", strlen("class_")) == 0 ||
                   strncmp(current_scope->scope_name, "method_", strlen("method_")) == 0 ) { // Also check method scopes
                    const char* name_ptr = strchr(current_scope->scope_name, '_') + 1;
                    const char* dot_ptr = strchr(name_ptr, '.'); // For "method_Class.name"
                    if (dot_ptr) { // method_Class.name -> Class
                        strncpy(class_name_from_scope, name_ptr, dot_ptr - name_ptr);
                        class_name_from_scope[dot_ptr-name_ptr] = '\0';
                    } else { // class_Class
                         strncpy(class_name_from_scope, name_ptr, sizeof(class_name_from_scope)-1);
                         class_name_from_scope[sizeof(class_name_from_scope)-1] = '\0';
                    }
                    break;
                }
                if(strcmp(current_scope->scope_name, "global") == 0) break; 
                current_scope = current_scope->parent_scope;
            }
            if(class_name_from_scope[0]) {
                strncpy(expr_node->data_type, class_name_from_scope, sizeof(expr_node->data_type)-1);
                expr_node->data_type[sizeof(expr_node->data_type)-1] = '\0';
                inferred_type = expr_node->data_type;
            } else {
                 fprintf(stderr, "[SEMANTIC L%d:%d] Error: 'this' used outside of a class context.\n", expr_node->line, expr_node->col);
                 strcpy(expr_node->data_type, "error_type");
                 inferred_type = "error_type";
            }
            break;
        }
        case AST_INDEX_ACCESS: {
            const char* target_type = analyze_expression_node(expr_node->left);
            analyze_expression_node(expr_node->right); // Analyze index, ensure it's int later
            
            if (strcmp(target_type, "error_type") == 0) {
                inferred_type = "error_type";
            } else if (strstr(target_type, "[]") != NULL) { 
                size_t len = strlen(target_type) - 2;
                if (len < sizeof(expr_node->data_type)) {
                    strncpy(expr_node->data_type, target_type, len);
                    expr_node->data_type[len] = '\0';
                } else { strcpy(expr_node->data_type, "any"); } // Buffer too small
                inferred_type = expr_node->data_type;
            } else if (strcmp(target_type, "array")==0 || strcmp(target_type, "any")==0) { 
                inferred_type = "any"; 
            } else if (strcmp(target_type, "string")==0) {
                inferred_type = "char"; // Or string, depending on language semantics
            } else {
                fprintf(stderr, "[SEMANTIC L%d:%d] Error: Type '%s' is not indexable.\n", expr_node->line, expr_node->col, target_type);
                inferred_type = "error_type";
            }
            if (strcmp(inferred_type, "error_type") != 0 && strcmp(inferred_type, "any") != 0) {
                 strncpy(expr_node->data_type, inferred_type, sizeof(expr_node->data_type)-1);
                 expr_node->data_type[sizeof(expr_node->data_type)-1] = '\0';
            } else {
                 strcpy(expr_node->data_type, inferred_type); // "any" or "error_type"
            }
            break;
        }
        default:
            inferred_type = "any"; 
            break;
    }
    // Ensure data_type on node is set if not error
    if (expr_node->data_type[0] == '\0' && strcmp(inferred_type, "error_type") != 0) { 
        strncpy(expr_node->data_type, inferred_type, sizeof(expr_node->data_type)-1);
        expr_node->data_type[sizeof(expr_node->data_type)-1] = '\0';
    }
    return expr_node->data_type[0] ? expr_node->data_type : "any";
}


static const char* analyze_member_access_expr(ASTNode *access_node) {
    if (!access_node || !access_node->left || !access_node->value[0]) {
         if(access_node) strcpy(access_node->data_type, "error_type"); return "error_type";
    }
    const char* target_type_name = analyze_expression_node(access_node->left);
    if (strcmp(target_type_name, "error_type") == 0) {
        strcpy(access_node->data_type, "error_type"); return "error_type";
    }
    if (strcmp(target_type_name, "any") == 0) {
         strcpy(access_node->data_type, "any"); return "any"; 
    }

    Symbol* type_sym = symbol_table_lookup_all_scopes(g_st, target_type_name);
    if (type_sym && (type_sym->kind == SYMBOL_CLASS || type_sym->kind == SYMBOL_STRUCT)) {
        ASTNode* type_decl_node = type_sym->declaration_node;
        if (!type_decl_node) { // Should not happen if symbol table is consistent
             strcpy(access_node->data_type, "error_type"); return "error_type";
        }
        ASTNode* member_decl = type_decl_node->left; 
        int found = 0;
        while(member_decl) {
            // Check for variable members or function members (methods)
            if ((member_decl->type == AST_VAR_DECL || member_decl->type == AST_TYPED_VAR_DECL || 
                 member_decl->type == AST_FUNCTION || member_decl->type == AST_TYPED_FUNCTION) &&
                strcmp(member_decl->value, access_node->value) == 0) {
                
                // Check accessibility (basic, assumes 'private' in access_modifier)
                Scope* current_analyzer_scope = symbol_table_get_current_scope(g_st);
                char current_class_context_name[128] = "";
                // Determine current class context for access check
                Scope* temp_scope = current_analyzer_scope;
                while(temp_scope) {
                    if(strncmp(temp_scope->scope_name, "class_", strlen("class_"))==0) {
                        strncpy(current_class_context_name, temp_scope->scope_name + strlen("class_"), sizeof(current_class_context_name)-1);
                        break;
                    } else if (strncmp(temp_scope->scope_name, "method_", strlen("method_"))==0) {
                         const char* name_ptr = strchr(temp_scope->scope_name, '_') + 1;
                         const char* dot_ptr = strchr(name_ptr, '.');
                         if(dot_ptr) strncpy(current_class_context_name, name_ptr, dot_ptr - name_ptr);
                         break;
                    }
                    if(strcmp(temp_scope->scope_name, "global")==0) break;
                    temp_scope = temp_scope->parent_scope;
                }


                if(member_decl->access_modifier[0] && strcmp(member_decl->access_modifier, "private") == 0) {
                    if(strcmp(target_type_name, current_class_context_name) != 0) {
                        fprintf(stderr, "[SEMANTIC L%d:%d] Error: Member '%s' of type '%s' is private and cannot be accessed from context '%s'.\n", 
                                 access_node->line, access_node->col, access_node->value, target_type_name, current_class_context_name[0] ? current_class_context_name : "global");
                        strcpy(access_node->data_type, "error_type"); return "error_type";
                    }
                }
                // Static check: if target_type_name implies static access (e.g. node->left was an IDENTIFIER that is a class name)
                int is_static_access_attempt = (access_node->left->type == AST_IDENTIFIER && type_sym && strcmp(access_node->left->value, type_sym->name)==0);
                int member_is_static = (member_decl->access_modifier[0] && strcmp(member_decl->access_modifier, "static")==0);

                if(is_static_access_attempt && !member_is_static) {
                     fprintf(stderr, "[SEMANTIC L%d:%d] Error: Cannot access instance member '%s' of type '%s' statically.\n", 
                                 access_node->line, access_node->col, access_node->value, target_type_name);
                     strcpy(access_node->data_type, "error_type"); return "error_type";
                }
                // Accessing static member via instance is often allowed (e.g. Java, C#) but can be a warning.


                if(member_decl->data_type[0]) { 
                    strncpy(access_node->data_type, member_decl->data_type, sizeof(access_node->data_type)-1);
                } else if (member_decl->type == AST_VAR_DECL || member_decl->type == AST_FUNCTION) { 
                     strcpy(access_node->data_type, "any"); 
                }
                found = 1;
                break;
            }
            member_decl = member_decl->next;
        }
        if (!found) {
             fprintf(stderr, "[SEMANTIC L%d:%d] Error: Member '%s' not found in type '%s'.\n", 
                     access_node->line, access_node->col, access_node->value, target_type_name);
             strcpy(access_node->data_type, "error_type");
        }
    } else if ( (strcmp(target_type_name, "string")==0 || strstr(target_type_name, "[]") || strcmp(target_type_name, "array")==0 ) &&
                strcmp(access_node->value, "length")==0) {
        strcpy(access_node->data_type, "int");
    } else {
        fprintf(stderr, "[SEMANTIC L%d:%d] Error: Cannot access member '%s' on primitive or unknown type '%s'.\n", 
                access_node->line, access_node->col, access_node->value, target_type_name);
        strcpy(access_node->data_type, "error_type");
    }
    return access_node->data_type[0] ? access_node->data_type : "error_type";
}

static void analyze_new_expr(ASTNode *new_node) {
    Symbol* class_sym = symbol_table_lookup_all_scopes(g_st, new_node->value);
    if (!class_sym || (class_sym->kind != SYMBOL_CLASS && class_sym->kind != SYMBOL_STRUCT)) {
        fprintf(stderr, "[SEMANTIC L%d:%d] Error: Class or struct '%s' not found for 'new' expression.\n", 
                new_node->line, new_node->col, new_node->value);
        strncpy(new_node->data_type, "error_type", sizeof(new_node->data_type)-1);
        new_node->data_type[sizeof(new_node->data_type)-1] = '\0';
        return;
    }
    strncpy(new_node->data_type, new_node->value, sizeof(new_node->data_type)-1);
    new_node->data_type[sizeof(new_node->data_type)-1] = '\0';

    if (new_node->left) { 
        ASTNode *arg = new_node->left;
        while (arg) {
            analyze_expression_node(arg);
            arg = arg->next;
        }
        // TODO: Find constructor for class_sym and check args.
    }
}


static void analyze_node(ASTNode *node) {
    if (!node) return;
    
    switch (node->type) {
        case AST_PROGRAM:
            {
                ASTNode *child = node->left; 
                while (child) {
                    analyze_node(child); 
                    child = child->next;
                }
            }
            break;
        case AST_FUNCTION: 
        case AST_TYPED_FUNCTION:
            if (g_st && g_st->current_scope_idx == 0 && strcmp(g_st->scope_stack[0]->scope_name, "global")==0) {
                analyze_function_decl(node, NULL); 
            } else {
                 // This implies method declaration, which should be handled by analyze_class_decl calling analyze_function_decl
                 // If not in class scope, it's an error (nested function not in class)
                 Scope* current_scope = symbol_table_get_current_scope(g_st);
                 if (!(current_scope && strncmp(current_scope->scope_name, "class_", strlen("class_")) == 0)) {
                     fprintf(stderr, "[SEMANTIC L%d:%d] Error: Function '%s' declared in unexpected scope '%s'. Functions can only be global or class methods.\n",
                             node->line, node->col, node->value, 
                             current_scope ? current_scope->scope_name : "unknown");
                 }
                 // If it *is* in class scope, analyze_class_decl would have called analyze_function_decl(node, class_node)
            }
            break;
        case AST_BLOCK: analyze_block_stmts(node); break;
        case AST_VAR_DECL: 
        case AST_TYPED_VAR_DECL: analyze_var_decl_stmt(node); break;
        case AST_ASSIGN: analyze_assignment_stmt(node); break;
        case AST_RETURN: analyze_return_stmt(node); break;
        case AST_IF: analyze_if_stmt(node); break;
        case AST_WHILE: analyze_while_stmt(node); break;
        case AST_FOR: analyze_for_stmt(node); break;
        case AST_CALL: analyze_call_expr_or_stmt(node); break;
        case AST_STRUCT: analyze_struct_decl(node); break;
        case AST_CLASS: analyze_class_decl(node); break;
        case AST_PRINT:
            if(node->left) analyze_expression_node(node->left);
            else fprintf(stderr, "[SEMANTIC L%d:%d] Error: Print statement missing expression.\n", node->line, node->col);
            break;
        case AST_IMPORT: /* TODO */ break;
        case AST_LITERAL: case AST_IDENTIFIER: case AST_BINARY_OP: case AST_UNARY_OP:
        case AST_ARRAY:   case AST_MEMBER_ACCESS: case AST_NEW:     case AST_THIS:
        case AST_INDEX_ACCESS:
            analyze_expression_node(node);
            break;
        case AST_ELSE: break; 
        default: break;
    }
}

void analyze_program(ASTNode *program_ast_root) {
    if (!program_ast_root) {
        fprintf(stderr, "[SEMANTIC] Error: NULL AST provided for analysis.\n");
        return;
    }
    if (program_ast_root->type != AST_PROGRAM) {
        fprintf(stderr, "[SEMANTIC] Error: Expected AST_PROGRAM node at root, got %s.\n", node_type_to_string(program_ast_root->type));
        return;
    }
    
    printf("\n==== Semantic Analysis ====\n");
    if (g_st) symbol_table_destroy(g_st); 
    g_st = symbol_table_create();
    
    analyze_node(program_ast_root); 
    
    symbol_table_destroy(g_st);
    g_st = NULL;
    printf("[SEMANTIC] Semantic analysis pass complete.\n");
}

void check_semantics(ASTNode *program_ast_root) {
    if (!program_ast_root) return;
    // This is now largely integrated into the main analyze_program pass.
    // Could be used for multi-pass analysis or more complex checks later.
    // printf("[SEMANTIC CHECKS] Detailed semantic checks (currently placeholder).\n");
}