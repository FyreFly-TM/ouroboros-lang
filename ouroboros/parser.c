#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser.h"
#include "lexer.h"
#include "ast_types.h"

// --- Forward Declarations ---
static ASTNode* parse_statement();
static ASTNode* parse_expression();
static ASTNode* parse_primary();
static ASTNode* parse_block();
static ASTNode* parse_variable_declaration();
static ASTNode* parse_typed_variable_declaration();
static ASTNode* parse_if_statement();
static ASTNode* parse_while_statement();
static ASTNode* parse_for_statement();
static ASTNode* parse_return_statement();
static ASTNode* parse_function();
static ASTNode* parse_typed_function();
static ASTNode* parse_parameters();
static ASTNode* parse_print_statement();
static ASTNode* parse_class_declaration();
static ASTNode* parse_struct_declaration();
static ASTNode* parse_binary_expression(ASTNode* left, int min_precedence);
static ASTNode* parse_literal_or_identifier();
static ASTNode* parse_array_literal();
static ASTNode* parse_new_expression();
static ASTNode* parse_member_access(ASTNode* target);
static ASTNode* parse_this_reference();
static ASTNode* parse_import();


// --- Globals ---
static Token* tokens;
static int token_pos;
static int num_tokens;
static Token current_token;

// Global pointer to the root AST so that other translation units (vm.c, eval.c)
// can inspect the parsed program.  This gets assigned each time `parse()` is
// called.
ASTNode *program = NULL;

// --- Helpers ---
static void advance() {
    if (token_pos < num_tokens) {
        current_token = tokens[token_pos++];
    }
}

static Token peek_token() {
    if (token_pos < num_tokens) {
        return tokens[token_pos];
    }
    Token eof_token = { .type = TOKEN_EOF, .text = "" };
    return eof_token;
}

static Token peek_token_n(int n) {
    if (token_pos + n - 1 < num_tokens) {
        return tokens[token_pos + n - 1];
    }
    Token eof_token = { .type = TOKEN_EOF, .text = "" };
    return eof_token;
}

static int is_type_keyword(const char* s) {
    return strcmp(s, "int") == 0 || strcmp(s, "float") == 0 ||
           strcmp(s, "bool") == 0 || strcmp(s, "string") == 0 ||
           strcmp(s, "void") == 0;
}

static int get_precedence(const char* op) {
    if (strcmp(op, "=") == 0) return 1;
    if (strcmp(op, "==") == 0 || strcmp(op, "!=") == 0) return 2;
    if (strcmp(op, "<") == 0 || strcmp(op, "<=") == 0 || strcmp(op, ">") == 0 || strcmp(op, ">=") == 0) return 3;
    if (strcmp(op, "+") == 0 || strcmp(op, "-") == 0) return 4;
    if (strcmp(op, "*") == 0 || strcmp(op, "/") == 0) return 5;
    return 0; // Not a binary operator
}


// --- Main Parsing Function ---
ASTNode* parse(Token* token_array) {
    tokens = token_array;
    token_pos = 0;
    num_tokens = 0;
    while (token_array[num_tokens].type != TOKEN_EOF) {
        num_tokens++;
    }
    num_tokens++; // for EOF

    advance();

    ASTNode* program_node = create_node(AST_PROGRAM, "program");
    ASTNode* last_stmt = NULL;

    printf("\n==== Parsing ====\n");
    while (current_token.type != TOKEN_EOF) {
        ASTNode* stmt = parse_statement();
        if (stmt) {
            if (last_stmt == NULL) {
                program_node->left = stmt;
                last_stmt = stmt;
            } else {
                last_stmt->next = stmt;
                last_stmt = stmt;
            }
        } else {
            fprintf(stderr, "Error: Failed to parse statement at line %d, col %d. Skipping token: '%s'\n", 
                    current_token.line, current_token.col, current_token.text);
            advance();
        }
    }

    // Expose the constructed AST to the rest of the VM
    program = program_node;
    return program_node;
}

// --- Statement Parsers ---

static ASTNode* parse_statement() {
    // Handle keywords that start statements
    if (current_token.type == TOKEN_KEYWORD) {
        if (strcmp(current_token.text, "let") == 0 || strcmp(current_token.text, "var") == 0) return parse_variable_declaration();
        if (strcmp(current_token.text, "if") == 0) return parse_if_statement();
        if (strcmp(current_token.text, "while") == 0) return parse_while_statement();
        if (strcmp(current_token.text, "for") == 0) return parse_for_statement();
        if (strcmp(current_token.text, "return") == 0) return parse_return_statement();
        if (strcmp(current_token.text, "function") == 0) return parse_function();
        if (strcmp(current_token.text, "print") == 0) return parse_print_statement();
        if (strcmp(current_token.text, "class") == 0) return parse_class_declaration();
        if (strcmp(current_token.text, "struct") == 0) return parse_struct_declaration();
        if (strcmp(current_token.text, "import") == 0) return parse_import();
        // Handle access modifiers
        if (strcmp(current_token.text, "public") == 0 || 
            strcmp(current_token.text, "private") == 0 || 
            strcmp(current_token.text, "static") == 0) {
            
            // Save the modifier
            char modifier[16];
            strncpy(modifier, current_token.text, sizeof(modifier) - 1);
            modifier[sizeof(modifier) - 1] = '\0';
            
            advance(); // consume the modifier
            
            // Check if it's a function
            if (current_token.type == TOKEN_KEYWORD && strcmp(current_token.text, "function") == 0) {
                ASTNode* func = parse_function();
                if (func) {
                    strncpy(func->access_modifier, modifier, sizeof(func->access_modifier) - 1);
                    func->access_modifier[sizeof(func->access_modifier) - 1] = '\0';
                }
                return func;
            }
            
            // Check if it's a member access (for properties)
            if (current_token.type == TOKEN_KEYWORD && strcmp(current_token.text, "this") == 0) {
                ASTNode* expr = parse_expression();
                if (expr && current_token.type == TOKEN_SYMBOL && strcmp(current_token.text, ";") == 0) {
                    advance(); // consume ';'
                    
                    // For binary operation (assignments)
                    if (expr->type == AST_BINARY_OP && strcmp(expr->value, "=") == 0) {
                        if (expr->left && expr->left->type == AST_MEMBER_ACCESS) {
                            strncpy(expr->left->access_modifier, modifier, sizeof(expr->left->access_modifier) - 1);
                            expr->left->access_modifier[sizeof(expr->left->access_modifier) - 1] = '\0';
                        }
                    }
                    
                    return expr;
                }
                
                fprintf(stderr, "Error: Expected ';' after property declaration with access modifier\n");
                return NULL;
            }
            
            // Check if it's a class declaration with an access modifier
            if (current_token.type == TOKEN_KEYWORD && strcmp(current_token.text, "class") == 0) {
                ASTNode *cls = parse_class_declaration();
                if (cls) {
                    strncpy(cls->access_modifier, modifier, sizeof(cls->access_modifier) - 1);
                    cls->access_modifier[sizeof(cls->access_modifier) - 1] = '\0';
                }
                return cls;
            }
            
            // Check for a typed variable declaration after the modifier, e.g.
            //   "public float myVar = 1.0;"  or  "private MyClass ref;"
            if (is_type_keyword(current_token.text) || current_token.type == TOKEN_IDENTIFIER) {
                ASTNode *var_decl = parse_typed_variable_declaration();
                if (var_decl) {
                    strncpy(var_decl->access_modifier, modifier, sizeof(var_decl->access_modifier) - 1);
                    var_decl->access_modifier[sizeof(var_decl->access_modifier) - 1] = '\0';
                }
                return var_decl;
            }
            
            fprintf(stderr, "Error: Expected function or property after access modifier\n");
            return NULL;
        }
        if (is_type_keyword(current_token.text)) {
            Token peek = peek_token();
            if (peek.type == TOKEN_IDENTIFIER) {
                Token peek2 = peek_token_n(2);
                if (peek2.type == TOKEN_SYMBOL && strcmp(peek2.text, "(") == 0) {
                    return parse_typed_function();
                }
                return parse_typed_variable_declaration();
            }
        }
    }

    // Handle user-defined types (simplified)
    if (current_token.type == TOKEN_IDENTIFIER) {
         Token peek = peek_token();
         if (peek.type == TOKEN_IDENTIFIER) {
            // Could be: `MyType myVar;`
            return parse_typed_variable_declaration();
         }
    }
    
    // Default to expression statement
    ASTNode* expr = parse_expression();
    if (!expr) return NULL;
    
    if (current_token.type == TOKEN_SYMBOL && strcmp(current_token.text, ";") == 0) {
        advance(); // consume ';'
        return expr;
    }
    
    fprintf(stderr, "Error: Expected ';' after expression at line %d, col %d\n", 
            current_token.line, current_token.col);
    return NULL;
}

static ASTNode* parse_block() {
    ASTNode* block = create_node(AST_BLOCK, "block");
    ASTNode* last_stmt = NULL;

    while (current_token.type != TOKEN_EOF && !(current_token.type == TOKEN_SYMBOL && strcmp(current_token.text, "}") == 0)) {
        ASTNode* stmt = parse_statement();
        if (stmt) {
            if (last_stmt == NULL) {
                block->left = stmt;
                last_stmt = stmt;
            } else {
                last_stmt->next = stmt;
                last_stmt = stmt;
            }
        } else {
             fprintf(stderr, "Error in block: Failed to parse statement at line %d, col %d. Skipping token: '%s'\n",
                    current_token.line, current_token.col, current_token.text);
            advance();
        }
    }
    return block;
}


static ASTNode* parse_typed_variable_declaration() {
    // Type
    if (!is_type_keyword(current_token.text) && current_token.type != TOKEN_IDENTIFIER) {
        fprintf(stderr, "Error: Expected type name for variable declaration.\n");
        return NULL;
    }
    char* type_str = strdup(current_token.text);
    advance();

    // Name
    if (current_token.type != TOKEN_IDENTIFIER) {
        fprintf(stderr, "Error: Expected identifier after type in variable declaration\n");
        free(type_str);
        return NULL;
    }
    char var_name[50];
    strcpy(var_name, current_token.text);
    advance();

    int is_array_decl = 0;
    /* Optional [] for array declaration */
    if (current_token.type == TOKEN_SYMBOL && strcmp(current_token.text, "[") == 0) {
        advance(); /* consume '[' */
        if (current_token.type != TOKEN_SYMBOL || strcmp(current_token.text, "]") != 0) {
            fprintf(stderr, "Error: Expected ']' after '[' in array declaration.\n");
            free(type_str);
            return NULL;
        }
        advance(); /* consume ']' */
        is_array_decl = 1;
    }

    ASTNode* var_decl = create_node(AST_VAR_DECL, var_name);
    strncpy(var_decl->data_type, type_str, sizeof(var_decl->data_type) - 1);
    var_decl->data_type[sizeof(var_decl->data_type) - 1] = '\0';
    var_decl->is_array = is_array_decl;
    free(type_str);

    // Optional initialization ( '=' may be tokenised as SYMBOL or OPERATOR )
    if ((current_token.type == TOKEN_SYMBOL || current_token.type == TOKEN_OPERATOR) && strcmp(current_token.text, "=") == 0) {
        advance();
        var_decl->left = parse_expression();
        if (!var_decl->left) {
            fprintf(stderr, "Error: Expected expression after '='\n");
            free_ast(var_decl);
            return NULL;
        }
    }

    // Semicolon
    if (current_token.type != TOKEN_SYMBOL || strcmp(current_token.text, ";") != 0) {
        fprintf(stderr, "Error: Expected ';' after variable declaration\n");
        free_ast(var_decl);
        return NULL;
    }
    advance();
    return var_decl;
}


static ASTNode* parse_typed_function() {
    // Return Type
    if (!is_type_keyword(current_token.text) && current_token.type != TOKEN_IDENTIFIER) {
         fprintf(stderr, "Error: Expected return type for function.\n");
        return NULL;
    }
    char* type_str = strdup(current_token.text);
    advance();

    // Name
    if (current_token.type != TOKEN_IDENTIFIER) {
        fprintf(stderr, "Error: Expected function name after type\n");
        free(type_str);
        return NULL;
    }
    ASTNode* func = create_node(AST_TYPED_FUNCTION, current_token.text);
    strncpy(func->data_type, type_str, sizeof(func->data_type) - 1);
    func->data_type[sizeof(func->data_type) - 1] = '\0';
    free(type_str);
    advance();

    // Parameters
    if (current_token.type != TOKEN_SYMBOL || strcmp(current_token.text, "(") != 0) {
        fprintf(stderr, "Error: Expected '(' after function name\n");
        free_ast(func);
        return NULL;
    }
    advance();
    func->left = parse_parameters();

    // Body
    if (current_token.type != TOKEN_SYMBOL || strcmp(current_token.text, "{") != 0) {
        fprintf(stderr, "Error: Expected '{' to open function body\n");
        free_ast(func);
        return NULL;
    }
    advance();
    func->right = parse_block();
    
    if (current_token.type != TOKEN_SYMBOL || strcmp(current_token.text, "}") != 0) {
        fprintf(stderr, "Error: Expected '}' to close function body\n");
        free_ast(func);
        return NULL;
    }
    advance();

    return func;
}


static ASTNode* parse_parameters() {
    ASTNode* head = NULL;
    ASTNode* tail = NULL;

    if (current_token.type == TOKEN_SYMBOL && strcmp(current_token.text, ")") == 0) {
        advance();
        return NULL; // No parameters
    }

    while (current_token.type != TOKEN_EOF) {
        // Type
        if (!is_type_keyword(current_token.text) && current_token.type != TOKEN_IDENTIFIER) {
             fprintf(stderr, "Error: Expected parameter type\n");
             free_ast(head);
             return NULL;
        }
        char* param_type = strdup(current_token.text);
        advance();

        // Name
        if (current_token.type != TOKEN_IDENTIFIER) {
            fprintf(stderr, "Error: Expected parameter name\n");
            free(param_type);
            free_ast(head);
            return NULL;
        }
        ASTNode* param_node = create_node(AST_PARAMETER, current_token.text);
        strncpy(param_node->data_type, param_type, sizeof(param_node->data_type) - 1);
        param_node->data_type[sizeof(param_node->data_type) - 1] = '\0';
        free(param_type);
        advance();

        if (head == NULL) {
            head = tail = param_node;
        } else {
            tail->next = param_node;
            tail = param_node;
        }

        if (current_token.type == TOKEN_SYMBOL && strcmp(current_token.text, ")") == 0) {
            break;
        }

        if (current_token.type != TOKEN_SYMBOL || strcmp(current_token.text, ",") != 0) {
            fprintf(stderr, "Error: Expected ',' or ')' in parameter list\n");
            free_ast(head);
            return NULL;
        }
        advance();
    }

    if (current_token.type == TOKEN_SYMBOL && strcmp(current_token.text, ")") == 0) {
        advance();
    } else {
        fprintf(stderr, "Error: Expected ')' to close parameter list\n");
        free_ast(head);
        return NULL;
    }
    return head;
}


static ASTNode* parse_struct_declaration() {
    advance(); // consume 'struct'
    if (current_token.type != TOKEN_IDENTIFIER) {
        fprintf(stderr, "Error: Expected struct name\n");
        return NULL;
    }
    ASTNode* node = create_node(AST_STRUCT, current_token.text);
    advance();

    if (current_token.type != TOKEN_SYMBOL || strcmp(current_token.text, "{") != 0) {
        fprintf(stderr, "Error: Expected '{' after struct name\n");
        free_ast(node);
        return NULL;
    }
    advance();

    ASTNode* members = NULL;
    ASTNode* last_member = NULL;
    while (current_token.type != TOKEN_EOF && !(current_token.type == TOKEN_SYMBOL && strcmp(current_token.text, "}") == 0)) {
        ASTNode* member = parse_typed_variable_declaration();
        if (member) {
            if (members == NULL) {
                members = last_member = member;
            } else {
                last_member->next = member;
                last_member = member;
            }
        } else {
             fprintf(stderr, "Error: Failed to parse struct member.\n");
             free_ast(node);
             free_ast(members);
             return NULL;
        }
    }
    node->left = members;

    if (current_token.type != TOKEN_SYMBOL || strcmp(current_token.text, "}") != 0) {
        fprintf(stderr, "Error: Expected '}' to close struct definition.\n");
        free_ast(node);
        return NULL;
    }
    advance();
    return node;
}


static ASTNode* parse_class_declaration() {
    advance(); // consume 'class'
    if (current_token.type != TOKEN_IDENTIFIER) {
        fprintf(stderr, "Error: Expected class name\n");
        return NULL;
    }
    ASTNode* node = create_node(AST_CLASS, current_token.text);
    advance();

    if (current_token.type != TOKEN_SYMBOL || strcmp(current_token.text, "{") != 0) {
        fprintf(stderr, "Error: Expected '{' after class name\n");
        free_ast(node);
        return NULL;
    }
    advance();

    ASTNode* members = NULL;
    ASTNode* last_member = NULL;
    while (current_token.type != TOKEN_EOF && !(current_token.type == TOKEN_SYMBOL && strcmp(current_token.text, "}") == 0)) {
        // Check for access modifiers before parsing the member
        char access_modifier[16] = {0};
        if (current_token.type == TOKEN_KEYWORD) {
            if (strcmp(current_token.text, "public") == 0 || 
                strcmp(current_token.text, "private") == 0 || 
                strcmp(current_token.text, "static") == 0) {
                
                strncpy(access_modifier, current_token.text, sizeof(access_modifier) - 1);
                advance(); // consume the access modifier
            }
        }
        
        ASTNode* member = parse_statement(); // can be var or func
        if(member) {
            // Set the access modifier if one was specified
            if (access_modifier[0] != '\0') {
                strncpy(member->access_modifier, access_modifier, sizeof(member->access_modifier) - 1);
            }
            
            if (members == NULL) {
                members = last_member = member;
            } else {
                last_member->next = member;
                last_member = member;
            }
        } else {
            fprintf(stderr, "Error: Failed to parse field or method in class body\n");
            advance(); // Skip bad token
        }
    }
    node->left = members;

    if (current_token.type != TOKEN_SYMBOL || strcmp(current_token.text, "}") != 0) {
        fprintf(stderr, "Error: Expected '}' to close class definition.\n");
        free_ast(node);
        return NULL;
    }
    advance();
    return node;
}

// --- Expression Parsers ---

static ASTNode* parse_expression() {
    ASTNode* left = parse_primary();
    if (!left) {
        return NULL;
    }

    // First, consume any binary operators with the usual precedence rules.
    left = parse_binary_expression(left, 0);

    // After the binary expression has been built, there may still be a trailing
    // member-access chain (e.g. the expression "(a * b).x").  Run the member-access
    // parser once more so that the '.' operator binds tighter than any binary
    // arithmetic operators.
    return parse_member_access(left);
}

static ASTNode* parse_binary_expression(ASTNode* left, int min_precedence) {
    while (1) {
        if (current_token.type != TOKEN_OPERATOR && current_token.type != TOKEN_SYMBOL) {
            break;
        }
        int prec = get_precedence(current_token.text);
        if (prec <= min_precedence) {
            break;
        }
        
        char op[10];
        strcpy(op, current_token.text);
        advance();

        ASTNode* right = parse_primary();
        if (!right) {
            fprintf(stderr, "Error: Expected primary expression for right-hand side of '%s'\n", op);
            free_ast(left);
            return NULL;
        }

        while (1) {
            if (current_token.type != TOKEN_OPERATOR && current_token.type != TOKEN_SYMBOL) break;
            int next_prec = get_precedence(current_token.text);
            if (next_prec <= prec) break;
            right = parse_binary_expression(right, next_prec);
        }

        ASTNode* new_left = create_node(AST_BINARY_OP, op);
        new_left->left = left;
        new_left->right = right;
        left = new_left;
    }
    return left;
}


static ASTNode* parse_primary() {
    ASTNode* node = NULL;
    if (current_token.type == TOKEN_KEYWORD &&
        (strcmp(current_token.text, "true") == 0 || strcmp(current_token.text, "false") == 0)) {
        /* Boolean literal */
        node = create_node(AST_LITERAL, current_token.text);
        advance();
    } else if (current_token.type == TOKEN_KEYWORD && strcmp(current_token.text, "this") == 0) {
        node = parse_this_reference();
    } else if (current_token.type == TOKEN_KEYWORD && strcmp(current_token.text, "new") == 0) {
        node = parse_new_expression();
    } else if (current_token.type == TOKEN_SYMBOL && strcmp(current_token.text, "(") == 0) {
        advance(); // consume '('
        node = parse_expression();
        if (current_token.type != TOKEN_SYMBOL || strcmp(current_token.text, ")") != 0) {
            fprintf(stderr, "Error: Expected ')'\n");
            free_ast(node);
            return NULL;
        }
        advance(); // consume ')'
    } else if (current_token.type == TOKEN_SYMBOL && (strcmp(current_token.text, "[") == 0 || strcmp(current_token.text, "{") == 0)) {
        node = parse_array_literal();
    } else {
        node = parse_literal_or_identifier();
    }

    // Support plain function calls like "foo(a, b)" where 'foo' is an identifier
    if (node && node->type == AST_IDENTIFIER && current_token.type == TOKEN_SYMBOL && strcmp(current_token.text, "(") == 0) {
        advance(); // consume '('

        // Parse argument list
        ASTNode *args = NULL;
        ASTNode *last_arg = NULL;

        if (current_token.type != TOKEN_SYMBOL || strcmp(current_token.text, ")") != 0) {
            while (1) {
                ASTNode *arg = parse_expression();
                if (!arg) {
                    fprintf(stderr, "Error parsing function call argument.\n");
                    free_ast(node);
                    free_ast(args);
                    return NULL;
                }

                if (!args) {
                    args = last_arg = arg;
                } else {
                    last_arg->next = arg;
                    last_arg = arg;
                }

                if (current_token.type == TOKEN_SYMBOL && strcmp(current_token.text, ")") == 0) {
                    break;
                }

                if (current_token.type != TOKEN_SYMBOL || strcmp(current_token.text, ",") != 0) {
                    fprintf(stderr, "Error: Expected ',' or ')' in argument list.\n");
                    free_ast(node);
                    free_ast(args);
                    return NULL;
                }

                advance(); // consume ','
            }
        }

        // Expect closing ')'
        if (current_token.type != TOKEN_SYMBOL || strcmp(current_token.text, ")") != 0) {
            fprintf(stderr, "Error: Expected ')' after argument list.\n");
            free_ast(node);
            free_ast(args);
            return NULL;
        }
        advance(); // consume ')'

        // Build AST_CALL node
        ASTNode *call_node = create_node(AST_CALL, node->value);
        call_node->left = args;
        // For plain calls there is no target object; evaluator will treat this as global/static
        call_node->right = NULL;

        node = call_node;
    }

    if(node) {
        return parse_member_access(node);
    }
    return NULL;
}

static ASTNode* parse_member_access(ASTNode* target) {
    while (current_token.type == TOKEN_SYMBOL && (strcmp(current_token.text, ".") == 0 || strcmp(current_token.text, "[") == 0)) {
        if (strcmp(current_token.text, ".") == 0) {
            advance(); // consume '.'
            if (current_token.type != TOKEN_IDENTIFIER) {
                fprintf(stderr, "Error: Expected identifier for member access.\n");
                free_ast(target);
                return NULL;
            }
            
            // Create member access node
            ASTNode* member_node = create_node(AST_MEMBER_ACCESS, current_token.text);
            member_node->left = target;
            target = member_node;
            advance(); // consume identifier
            
            // Check if this is a method call
            if (current_token.type == TOKEN_SYMBOL && strcmp(current_token.text, "(") == 0) {
                advance(); // consume '('
                
                // Parse arguments
                ASTNode* args = NULL;
                ASTNode* last_arg = NULL;
                
                if (current_token.type != TOKEN_SYMBOL || strcmp(current_token.text, ")") != 0) {
                    while (1) {
                        ASTNode* arg = parse_expression();
                        if (!arg) {
                            fprintf(stderr, "Error parsing method argument.\n");
                            free_ast(target);
                            free_ast(args);
                            return NULL;
                        }
                        
                        if (!args) {
                            args = last_arg = arg;
                        } else {
                            last_arg->next = arg;
                            last_arg = arg;
                        }
                        
                        if (current_token.type == TOKEN_SYMBOL && strcmp(current_token.text, ")") == 0) {
                            break;
                        }
                        
                        if (current_token.type != TOKEN_SYMBOL || strcmp(current_token.text, ",") != 0) {
                            fprintf(stderr, "Error: Expected ',' or ')' in method arguments.\n");
                            free_ast(target);
                            free_ast(args);
                            return NULL;
                        }
                        advance(); // consume ','
                    }
                }
                
                advance(); // consume ')'
                
                // Convert the member access to a method call
                ASTNode* method_call = create_node(AST_CALL, member_node->value);
                method_call->left = args;
                member_node->type = AST_MEMBER_ACCESS; // Ensure it's a member access
                method_call->right = member_node->left; // Store the target object
                free(member_node); // Free the original member_node
                target = method_call;
            }
        } else { // It's '['
            advance(); // consume '['
            ASTNode* index_expr = parse_expression();
            if (!index_expr) {
                fprintf(stderr, "Error: Expected expression for index access.\n");
                free_ast(target);
                return NULL;
            }
            ASTNode* index_node = create_node(AST_INDEX_ACCESS, "[]");
            index_node->left = target;
            index_node->right = index_expr;
            target = index_node;
            if (current_token.type != TOKEN_SYMBOL || strcmp(current_token.text, "]") != 0) {
                fprintf(stderr, "Error: Expected ']'.\n");
                free_ast(target);
                return NULL;
            }
            advance(); // consume ']'
        }
    }
    return target;
}

static ASTNode* parse_literal_or_identifier() {
    ASTNodeType type;
    switch(current_token.type) {
        case TOKEN_NUMBER:
        case TOKEN_STRING:
        case TOKEN_BOOL:
            type = AST_LITERAL;
            break;
        case TOKEN_IDENTIFIER:
            type = AST_IDENTIFIER;
            break;
        default:
            return NULL;
    }
    ASTNode* node = create_node(type, current_token.text);
    advance();
    return node;
}


// --- Stubs for other parsers ---
// These need to be implemented properly

static ASTNode* parse_variable_declaration() { 
    advance(); // consume 'var' or 'let'
    
    if (current_token.type != TOKEN_IDENTIFIER) {
        fprintf(stderr, "Error: Unexpected token after identifier at line %d, col %d\n", 
                current_token.line, current_token.col);
        return NULL;
    }
    
    // Create variable declaration node with the identifier as name
    ASTNode* var_decl = create_node(AST_VAR_DECL, current_token.text);
    ASTNode* id_node = create_node(AST_IDENTIFIER, current_token.text);
    var_decl->left = id_node;
    advance(); // consume identifier
    
    // Check for initialization
    if (current_token.type == TOKEN_OPERATOR && strcmp(current_token.text, "=") == 0) {
        advance(); // consume '='
        
        // Parse the initializer expression
        ASTNode* initializer = parse_expression();
        if (!initializer) {
            fprintf(stderr, "Error: Failed to parse initializer expression\n");
            free_ast(var_decl);
            return NULL;
        }
        var_decl->right = initializer;
    }
    
    // Expect semicolon
    if (current_token.type != TOKEN_SYMBOL || strcmp(current_token.text, ";") != 0) {
        fprintf(stderr, "Error: Expected ';' after variable declaration\n");
        free_ast(var_decl);
        return NULL;
    }
    advance(); // consume ';'
    
    return var_decl;
}

static ASTNode* parse_if_statement() { 
    fprintf(stderr, "Parsing 'if' is not fully implemented.\n");
    advance();
    return create_node(AST_UNKNOWN, "if_stub");
}

static ASTNode* parse_while_statement() {
    /* Syntax supported:
       while ( condition ) { body }
       Both the condition and the body are parsed via existing helpers so they
       can contain any expressions/statements already implemented elsewhere. */

    advance(); /* consume 'while' */

    /* Expect '(' */
    if (current_token.type != TOKEN_SYMBOL || strcmp(current_token.text, "(") != 0) {
        fprintf(stderr, "Error: Expected '(' after 'while'.\n");
        return NULL;
    }
    advance(); /* consume '(' */

    /* Parse condition expression */
    ASTNode *condition = parse_expression();
    if (!condition) {
        fprintf(stderr, "Error: Failed to parse while-condition expression.\n");
        return NULL;
    }

    /* Expect ')' */
    if (current_token.type != TOKEN_SYMBOL || strcmp(current_token.text, ")") != 0) {
        fprintf(stderr, "Error: Expected ')' after while-condition.\n");
        free_ast(condition);
        return NULL;
    }
    advance(); /* consume ')' */

    /* Expect '{' opening block */
    if (current_token.type != TOKEN_SYMBOL || strcmp(current_token.text, "{") != 0) {
        fprintf(stderr, "Error: Expected '{' to open while-body.\n");
        free_ast(condition);
        return NULL;
    }
    advance(); /* consume '{' */

    /* Parse body statements */
    ASTNode *body = parse_block();

    /* Expect closing '}' */
    if (current_token.type != TOKEN_SYMBOL || strcmp(current_token.text, "}") != 0) {
        fprintf(stderr, "Error: Expected '}' to close while-body.\n");
        free_ast(condition);
        free_ast(body);
        return NULL;
    }
    advance(); /* consume '}' */

    /* Build AST_WHILE node */
    ASTNode *while_node = create_node(AST_WHILE, "while");
    while_node->left = condition; /* condition */
    while_node->right = body;      /* body */
    return while_node;
}

static ASTNode* parse_for_statement() { 
    fprintf(stderr, "Parsing 'for' is not fully implemented.\n");
    advance();
    return create_node(AST_UNKNOWN, "for_stub");
}

static ASTNode* parse_return_statement() { 
    advance(); // consume 'return'
    ASTNode* node = create_node(AST_RETURN, "return");
    node->left = parse_expression();
    if(current_token.type == TOKEN_SYMBOL && strcmp(current_token.text, ";") == 0) {
        advance();
    } else {
        fprintf(stderr, "Warning: Missing semicolon after return statement.\n");
    }
    return node;
}

static ASTNode* parse_function() { 
    advance(); // consume 'function'
    
    if (current_token.type != TOKEN_IDENTIFIER) {
        fprintf(stderr, "Error: Expected function name\n");
        return NULL;
    }
    
    ASTNode* func = create_node(AST_FUNCTION, current_token.text);
    advance(); // consume function name
    
    // Parse parameters
    if (current_token.type != TOKEN_SYMBOL || strcmp(current_token.text, "(") != 0) {
        fprintf(stderr, "Error: Expected '(' after function name\n");
        free_ast(func);
        return NULL;
    }
    advance(); // consume '('
    
    // Parse parameter list
    ASTNode* params = NULL;
    ASTNode* last_param = NULL;
    
    if (current_token.type != TOKEN_SYMBOL || strcmp(current_token.text, ")") != 0) {
        while (1) {
            if (current_token.type != TOKEN_IDENTIFIER) {
                fprintf(stderr, "Error: Expected parameter name\n");
                free_ast(func);
                free_ast(params);
                return NULL;
            }
            
            ASTNode* param = create_node(AST_PARAMETER, current_token.text);
            advance(); // consume parameter name
            
            if (params == NULL) {
                params = last_param = param;
            } else {
                last_param->next = param;
                last_param = param;
            }
            
            if (current_token.type == TOKEN_SYMBOL && strcmp(current_token.text, ")") == 0) {
                break;
            }
            
            if (current_token.type != TOKEN_SYMBOL || strcmp(current_token.text, ",") != 0) {
                fprintf(stderr, "Error: Expected ',' or ')' in parameter list\n");
                free_ast(func);
                free_ast(params);
                return NULL;
            }
            
            advance(); // consume ','
        }
    }
    
    func->left = params;
    advance(); // consume ')'
    
    // Parse function body
    if (current_token.type != TOKEN_SYMBOL || strcmp(current_token.text, "{") != 0) {
        fprintf(stderr, "Error: Expected '{' to begin function body\n");
        free_ast(func);
        return NULL;
    }
    
    advance(); // consume '{'
    func->right = parse_block();
    
    if (current_token.type != TOKEN_SYMBOL || strcmp(current_token.text, "}") != 0) {
        fprintf(stderr, "Error: Expected '}' to close function body\n");
        free_ast(func);
        return NULL;
    }
    
    advance(); // consume '}'
    return func;
}

static ASTNode* parse_print_statement() { 
    advance(); // consume 'print'
    
    if (current_token.type != TOKEN_SYMBOL || strcmp(current_token.text, "(") != 0) {
        fprintf(stderr, "Error: Expected '(' after print.\n");
        return NULL;
    }
    advance(); // consume '('
    
    ASTNode* expr = parse_expression();
    if (!expr) {
        fprintf(stderr, "Error: Expected expression in print statement.\n");
        return NULL;
    }
    
    if (current_token.type != TOKEN_SYMBOL || strcmp(current_token.text, ")") != 0) {
        fprintf(stderr, "Error: Expected ')' after print argument\n");
        free_ast(expr);
        return NULL;
    }
    advance(); // consume ')'
    
    if (current_token.type != TOKEN_SYMBOL || strcmp(current_token.text, ";") != 0) {
        fprintf(stderr, "Error: Expected ';' after print statement.\n");
        free_ast(expr);
        return NULL;
    }
    advance(); // consume ';'
    
    ASTNode* print_node = create_node(AST_PRINT, "print");
    print_node->left = expr;
    return print_node;
}

static ASTNode* parse_array_literal() {
    /* Handles either '[' elem1, elem2, ... ']' or '{' elem1, elem2 ... '}' */
    const char *open_sym = current_token.text;
    const char *close_sym = (strcmp(open_sym, "[") == 0) ? "]" : "}";

    advance(); /* consume opening symbol */

    /* For now we collect element texts into a single comma-separated string so
       the evaluator / native wrappers can parse them later without needing a
       full AST representation per element. This keeps implementation simple
       while unblocking compilation of code that merely forwards raw vertex
       data to native calls. */

    char combined[1024] = "";
    int first = 1;
    while (current_token.type != TOKEN_EOF && !(current_token.type == TOKEN_SYMBOL && strcmp(current_token.text, close_sym) == 0)) {
        /* Append token text unless it's whitespace/comma we will manually add */
        if (current_token.type != TOKEN_SYMBOL || (strcmp(current_token.text, ",") != 0)) {
            if (!first) strcat(combined, " ");
            strcat(combined, current_token.text);
            first = 0;
        }
        advance();
    }

    if (current_token.type == TOKEN_EOF) {
        fprintf(stderr, "Error: Unterminated array literal, expected '%s'.\n", close_sym);
        return NULL;
    }
    advance(); /* consume close_sym */

    ASTNode *arr_node = create_node(AST_ARRAY, combined);
    return arr_node;
}

static ASTNode* parse_new_expression() {
    advance(); // new
    if(current_token.type != TOKEN_IDENTIFIER) {
         fprintf(stderr, "Error: Expected class name after 'new'.\n");
        return NULL;
    }
    ASTNode* node = create_node(AST_NEW, current_token.text);
    strncpy(node->data_type, current_token.text, sizeof(node->data_type) - 1);
    node->data_type[sizeof(node->data_type) - 1] = '\0';
    advance();
    
    // Arguments
    if(strcmp(current_token.text, "(") != 0) {
        return node; // No constructor args
    }
    advance();
    
    ASTNode* args = NULL;
    ASTNode* last_arg = NULL;

    if (strcmp(current_token.text, ")") != 0) {
        while(1) {
            ASTNode* arg = parse_expression();
            if(!arg) {
                fprintf(stderr, "Error parsing constructor argument.\n");
                free_ast(node);
                free_ast(args);
                return NULL;
            }
            if(args == NULL) {
                args = last_arg = arg;
            } else {
                last_arg->next = arg;
                last_arg = arg;
            }
            if(strcmp(current_token.text, ")") == 0) break;
            if(strcmp(current_token.text, ",") != 0) {
                fprintf(stderr, "Error: expected ',' or ')' in arguments.\n");
                free_ast(node);
                free_ast(args);
                return NULL;
            }
            advance(); // consume ','
        }
    }
    advance(); // consume ')'
    node->left = args;
    return node;
}

static ASTNode* parse_this_reference() {
    advance(); // this
    return create_node(AST_THIS, "this");
}

static ASTNode* parse_import() {
    advance(); // consume 'import'
    
    if (current_token.type != TOKEN_STRING) {
        fprintf(stderr, "Error: Expected string literal after import\n");
        return NULL;
    }
    
    ASTNode* import_node = create_node(AST_IMPORT, current_token.text);
    advance(); // consume module name
    
    if (current_token.type != TOKEN_SYMBOL || strcmp(current_token.text, ";") != 0) {
        fprintf(stderr, "Error: Expected ';' after import statement\n");
        free_ast(import_node);
        return NULL;
    }
    advance(); // consume ';'
    
    return import_node;
}
