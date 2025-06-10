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


// --- Globals ---
static Token* tokens;
static int token_pos;
static int num_tokens;
static Token current_token;

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

    ASTNode* program = create_node(AST_PROGRAM, "program");
    ASTNode* last_stmt = NULL;

    printf("\n==== Parsing ====\n");
    while (current_token.type != TOKEN_EOF) {
        ASTNode* stmt = parse_statement();
        if (stmt) {
            if (last_stmt == NULL) {
                program->left = stmt;
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
    return program;
}

// --- Statement Parsers ---

static ASTNode* parse_statement() {
    // Handle keywords that start statements
    if (current_token.type == TOKEN_KEYWORD) {
        if (strcmp(current_token.text, "let") == 0) return parse_variable_declaration();
        if (strcmp(current_token.text, "if") == 0) return parse_if_statement();
        if (strcmp(current_token.text, "while") == 0) return parse_while_statement();
        if (strcmp(current_token.text, "for") == 0) return parse_for_statement();
        if (strcmp(current_token.text, "return") == 0) return parse_return_statement();
        if (strcmp(current_token.text, "function") == 0) return parse_function();
        if (strcmp(current_token.text, "print") == 0) return parse_print_statement();
        if (strcmp(current_token.text, "class") == 0) return parse_class_declaration();
        if (strcmp(current_token.text, "struct") == 0) return parse_struct_declaration();
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
    if (expr) {
        if (current_token.type == TOKEN_SYMBOL && strcmp(current_token.text, ";") == 0) {
            advance(); // consume ';'
            return expr;
        }
        fprintf(stderr, "Error: Expected ';' after expression statement.\n");
        free_ast(expr);
        return NULL;
    }

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

    ASTNode* var_decl = create_node(AST_VAR_DECL, var_name);
    strncpy(var_decl->data_type, type_str, sizeof(var_decl->data_type) - 1);
    var_decl->data_type[sizeof(var_decl->data_type) - 1] = '\0';
    free(type_str);

    // Optional initialization
    if (current_token.type == TOKEN_SYMBOL && strcmp(current_token.text, "=") == 0) {
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
        ASTNode* member = parse_statement(); // can be var or func
        if(member) {
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
    return parse_binary_expression(left, 0);
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
    if (current_token.type == TOKEN_KEYWORD && strcmp(current_token.text, "this") == 0) {
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
    } else if (current_token.type == TOKEN_SYMBOL && strcmp(current_token.text, "[") == 0) {
        node = parse_array_literal();
    } else {
        node = parse_literal_or_identifier();
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
            ASTNode* member_node = create_node(AST_MEMBER_ACCESS, current_token.text);
            member_node->left = target;
            target = member_node;
            advance(); // consume identifier
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
    fprintf(stderr, "Parsing 'let' is not fully implemented.\n");
    advance(); // let
    advance(); // name
    if(strcmp(current_token.text, "=") == 0) {
        advance(); // =
        parse_expression(); // value
    }
    if(strcmp(current_token.text, ";") == 0) advance();
    return create_node(AST_UNKNOWN, "let_stub"); 
}

static ASTNode* parse_if_statement() { 
    fprintf(stderr, "Parsing 'if' is not fully implemented.\n");
    advance();
    return create_node(AST_UNKNOWN, "if_stub");
}

static ASTNode* parse_while_statement() {
    fprintf(stderr, "Parsing 'while' is not fully implemented.\n");
    advance();
    return create_node(AST_UNKNOWN, "while_stub");
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
    fprintf(stderr, "Parsing 'function' is not fully implemented.\n");
    advance();
    return create_node(AST_UNKNOWN, "function_stub");
}
static ASTNode* parse_print_statement() { 
    advance(); // consume 'print'
    if(strcmp(current_token.text, "(") != 0) {
        fprintf(stderr, "Error: Expected '(' after print.\n");
        return NULL;
    }
    advance();
    ASTNode* expr = parse_expression();
    if(strcmp(current_token.text, ")") != 0) {
        fprintf(stderr, "Error: Expected ')' after print expression.\n");
        free_ast(expr);
        return NULL;
    }
    advance();
    if(strcmp(current_token.text, ";") != 0) {
         fprintf(stderr, "Error: Expected ';' after print statement.\n");
        free_ast(expr);
        return NULL;
    }
    advance();
    ASTNode* print_node = create_node(AST_PRINT, "print");
    print_node->left = expr;
    return print_node;
}

static ASTNode* parse_array_literal() {
    fprintf(stderr, "Parsing array literal not implemented.\n");
    advance(); // consume '['
    while(strcmp(current_token.text, "]") != 0 && current_token.type != TOKEN_EOF) advance();
    advance(); // consume ']'
    return create_node(AST_UNKNOWN, "array_literal_stub");
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
