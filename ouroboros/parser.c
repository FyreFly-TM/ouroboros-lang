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

ASTNode* program = NULL;

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
    Token eof_token = { .type = TOKEN_EOF, .text = "", .line = current_token.line, .col = current_token.col };
    return eof_token;
}

static Token peek_token_n(int n) {
    if (token_pos + n - 1 < num_tokens) {
        return tokens[token_pos + n - 1];
    }
    Token eof_token = { .type = TOKEN_EOF, .text = "", .line = current_token.line, .col = current_token.col };
    return eof_token;
}

// Utility to check if a string is a built-in type keyword
int is_builtin_type_keyword(const char* s) { // Renamed to avoid conflict if parser.c included elsewhere
    return strcmp(s, "int") == 0 || strcmp(s, "float") == 0 ||
        strcmp(s, "bool") == 0 || strcmp(s, "string") == 0 ||
        strcmp(s, "void") == 0 || strcmp(s, "array") == 0 || // "array" might be a type
        strcmp(s, "object") == 0 || strcmp(s, "any") == 0; // More generic types
}


static int get_precedence(const char* op) {
    if (strcmp(op, "=") == 0) return 1; // Assignment (right-associative, usually handled specially)
    if (strcmp(op, "||") == 0) return 2;
    if (strcmp(op, "&&") == 0) return 3;
    // Bitwise ops could go here if added
    if (strcmp(op, "==") == 0 || strcmp(op, "!=") == 0) return 7;
    if (strcmp(op, "<") == 0 || strcmp(op, "<=") == 0 || strcmp(op, ">") == 0 || strcmp(op, ">=") == 0) return 8;
    // Bitwise shifts
    if (strcmp(op, "+") == 0 || strcmp(op, "-") == 0) return 10; // Additive
    if (strcmp(op, "*") == 0 || strcmp(op, "/") == 0 || strcmp(op, "%") == 0) return 11; // Multiplicative
    // Unary operators are handled by parse_primary or a dedicated unary parse function
    // Member access (.), array index ([]), function call (()) are usually highest or handled by parse_primary loop
    return 0;
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

    program = create_node(AST_PROGRAM, "program", 1, 1);
    ASTNode* last_stmt = NULL;

    // printf("\n==== Parsing ====\n");
    while (current_token.type != TOKEN_EOF) {
        ASTNode* stmt = parse_statement();
        if (stmt) {
            if (last_stmt == NULL) {
                program->left = stmt;
                last_stmt = stmt;
            }
            else {
                last_stmt->next = stmt;
                last_stmt = stmt;
            }
        }
        else {
            fprintf(stderr, "Error: Failed to parse statement at line %d, col %d. Current token: '%s' (Type %d). Skipping.\n",
                current_token.line, current_token.col, current_token.text, current_token.type);
            if (current_token.type != TOKEN_EOF) advance(); else break;
        }
    }
    return program;
}

// --- Statement Parsers ---

static ASTNode* parse_statement() {
    Token start_token = current_token;

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

        if (strcmp(current_token.text, "public") == 0 ||
            strcmp(current_token.text, "private") == 0 ||
            strcmp(current_token.text, "static") == 0) {

            char modifier[16];
            strncpy(modifier, current_token.text, sizeof(modifier) - 1);
            modifier[sizeof(modifier) - 1] = '\0';
            Token modifier_token = current_token;
            advance();

            ASTNode* modified_node = NULL;
            if (current_token.type == TOKEN_KEYWORD && strcmp(current_token.text, "function") == 0) {
                modified_node = parse_function();
            }
            else if (current_token.type == TOKEN_KEYWORD && strcmp(current_token.text, "class") == 0) {
                modified_node = parse_class_declaration();
            }
            else if (is_builtin_type_keyword(current_token.text) || current_token.type == TOKEN_IDENTIFIER) {
                Token peek = peek_token();
                Token peek2 = peek_token_n(2);
                if ((is_builtin_type_keyword(current_token.text) || current_token.type == TOKEN_IDENTIFIER) &&
                    peek.type == TOKEN_IDENTIFIER &&
                    peek2.type == TOKEN_SYMBOL && strcmp(peek2.text, "(") == 0) {
                    modified_node = parse_typed_function();
                }
                else {
                    modified_node = parse_typed_variable_declaration();
                }
            }
            else if (current_token.type == TOKEN_KEYWORD && (strcmp(current_token.text, "var") == 0 || strcmp(current_token.text, "let") == 0)) {
                modified_node = parse_variable_declaration();
            }
            else {
                fprintf(stderr, "Error (L%d:%d): Expected function, class, or variable declaration after access modifier '%s'. Got '%s'.\n",
                    modifier_token.line, modifier_token.col, modifier_token.text, current_token.text);
                return NULL;
            }

            if (modified_node) {
                strncpy(modified_node->access_modifier, modifier, sizeof(modified_node->access_modifier) - 1);
                modified_node->access_modifier[sizeof(modified_node->access_modifier) - 1] = '\0';
                modified_node->line = modifier_token.line;
                modified_node->col = modifier_token.col;
            }
            return modified_node;
        }
        if (is_builtin_type_keyword(current_token.text)) { // Check for `int foo()` or `float bar;`
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

    if (current_token.type == TOKEN_IDENTIFIER) { // Could be `MyType myVar;` or `myFunc();` or `myVar = ...;`
        Token peek = peek_token();
        if (peek.type == TOKEN_IDENTIFIER) {
            Token peek2 = peek_token_n(2);
            if (peek2.type == TOKEN_SYMBOL && strcmp(peek2.text, "(") == 0) {
                return parse_typed_function(); // MyType myFunc(...
            }
            return parse_typed_variable_declaration(); // MyType myVar;
        }
    }

    ASTNode* expr = parse_expression();
    if (!expr) return NULL;

    if (current_token.type == TOKEN_SYMBOL && strcmp(current_token.text, ";") == 0) {
        advance();
        return expr;
    }

    fprintf(stderr, "Error (L%d:%d): Expected ';' after expression statement. Got token '%s' (type %d) after expression starting L%d:%d.\n",
        current_token.line, current_token.col, current_token.text, current_token.type, expr->line, expr->col);
    return NULL;
}

static ASTNode* parse_block() {
    Token start_token = current_token;
    ASTNode* block = create_node(AST_BLOCK, "block", start_token.line, start_token.col);
    ASTNode* last_stmt = NULL;

    while (current_token.type != TOKEN_EOF && !(current_token.type == TOKEN_SYMBOL && strcmp(current_token.text, "}") == 0)) {
        ASTNode* stmt = parse_statement();
        if (stmt) {
            if (last_stmt == NULL) {
                block->left = stmt;
                last_stmt = stmt;
            }
            else {
                last_stmt->next = stmt;
                last_stmt = stmt;
            }
        }
        else {
            fprintf(stderr, "Error in block (L%d:%d): Failed to parse statement. Skipping token: '%s'\n",
                current_token.line, current_token.col, current_token.text);
            if (current_token.type != TOKEN_EOF) advance(); else break;
        }
    }
    return block;
}


static ASTNode* parse_typed_variable_declaration() {
    Token type_token = current_token;

    if (!is_builtin_type_keyword(current_token.text) && current_token.type != TOKEN_IDENTIFIER) {
        fprintf(stderr, "Error (L%d:%d): Expected type name for variable declaration.\n", current_token.line, current_token.col);
        return NULL;
    }
    char* type_str = strdup(current_token.text);
    advance();

    if (current_token.type != TOKEN_IDENTIFIER) {
        fprintf(stderr, "Error (L%d:%d): Expected identifier after type '%s'\n", current_token.line, current_token.col, type_token.text);
        free(type_str);
        return NULL;
    }
    char var_name_str[sizeof(((ASTNode*)0)->value)]; // Ensure buffer is same size as ASTNode.value
    strncpy(var_name_str, current_token.text, sizeof(var_name_str) - 1);
    var_name_str[sizeof(var_name_str) - 1] = '\0';
    advance();

    int is_array_decl = 0;
    if (current_token.type == TOKEN_SYMBOL && strcmp(current_token.text, "[") == 0) {
        advance();
        if (current_token.type != TOKEN_SYMBOL || strcmp(current_token.text, "]") != 0) {
            fprintf(stderr, "Error (L%d:%d): Expected ']' after '[' in array declaration.\n", current_token.line, current_token.col);
            free(type_str);
            return NULL;
        }
        advance();
        is_array_decl = 1;
    }

    ASTNode* var_decl = create_node(AST_TYPED_VAR_DECL, var_name_str, type_token.line, type_token.col);
    strncpy(var_decl->data_type, type_str, sizeof(var_decl->data_type) - 1);
    var_decl->data_type[sizeof(var_decl->data_type) - 1] = '\0';
    if (is_array_decl) strcat(var_decl->data_type, "[]"); // Append [] to type name string
    var_decl->is_array = is_array_decl;
    free(type_str);

    var_decl->left = NULL;
    if ((current_token.type == TOKEN_SYMBOL || current_token.type == TOKEN_OPERATOR) && strcmp(current_token.text, "=") == 0) {
        advance();
        var_decl->right = parse_expression();
        if (!var_decl->right) {
            fprintf(stderr, "Error (L%d:%d): Expected expression after '='\n", current_token.line, current_token.col);
            free_ast(var_decl);
            return NULL;
        }
    }
    else {
        var_decl->right = NULL;
    }

    if (current_token.type != TOKEN_SYMBOL || strcmp(current_token.text, ";") != 0) {
        fprintf(stderr, "Error (L%d:%d): Expected ';' after variable declaration of '%s'\n", current_token.line, current_token.col, var_name_str);
        free_ast(var_decl);
        return NULL;
    }
    advance();
    return var_decl;
}
// ... (rest of parser.c, ensure all create_node calls have line/col) ...

// The rest of parser.c (parse_typed_function, parse_parameters, etc.) would be here.
// I'll continue with the rest of the file, assuming the create_node updates are applied.

static ASTNode* parse_typed_function() {
    Token type_token = current_token;
    if (!is_builtin_type_keyword(current_token.text) && current_token.type != TOKEN_IDENTIFIER) {
        fprintf(stderr, "Error (L%d:%d): Expected return type for function.\n", current_token.line, current_token.col);
        return NULL;
    }
    char* type_str = strdup(current_token.text);
    advance();

    if (current_token.type != TOKEN_IDENTIFIER) {
        fprintf(stderr, "Error (L%d:%d): Expected function name after type '%s'\n", current_token.line, current_token.col, type_token.text);
        free(type_str);
        return NULL;
    }
    ASTNode* func = create_node(AST_TYPED_FUNCTION, current_token.text, type_token.line, type_token.col);
    strncpy(func->data_type, type_str, sizeof(func->data_type) - 1);
    func->data_type[sizeof(func->data_type) - 1] = '\0';
    free(type_str);
    advance();

    if (current_token.type != TOKEN_SYMBOL || strcmp(current_token.text, "(") != 0) {
        fprintf(stderr, "Error (L%d:%d): Expected '(' after function name '%s'\n", current_token.line, current_token.col, func->value);
        free_ast(func);
        return NULL;
    }
    advance();
    func->left = parse_parameters();

    if (current_token.type != TOKEN_SYMBOL || strcmp(current_token.text, "{") != 0) {
        fprintf(stderr, "Error (L%d:%d): Expected '{' to open function body for '%s'\n", current_token.line, current_token.col, func->value);
        free_ast(func);
        return NULL;
    }
    Token body_start_token = current_token;
    advance();
    func->right = parse_block();
    if (!func->right) {
        fprintf(stderr, "Error (L%d:%d): Failed to parse function body for '%s'\n", body_start_token.line, body_start_token.col, func->value);
        free_ast(func);
        return NULL;
    }

    if (current_token.type != TOKEN_SYMBOL || strcmp(current_token.text, "}") != 0) {
        fprintf(stderr, "Error (L%d:%d): Expected '}' to close function body for '%s'. Got '%s'.\n", current_token.line, current_token.col, func->value, current_token.text);
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
        return NULL;
    }

    while (current_token.type != TOKEN_EOF) {
        Token param_type_token = current_token;
        if (!is_builtin_type_keyword(current_token.text) && current_token.type != TOKEN_IDENTIFIER) {
            fprintf(stderr, "Error (L%d:%d): Expected parameter type\n", current_token.line, current_token.col);
            free_ast(head);
            return NULL;
        }
        char* param_type_str = strdup(current_token.text);
        advance();

        if (current_token.type != TOKEN_IDENTIFIER) {
            fprintf(stderr, "Error (L%d:%d): Expected parameter name after type '%s'\n", current_token.line, current_token.col, param_type_str);
            free(param_type_str);
            free_ast(head);
            return NULL;
        }
        ASTNode* param_node = create_node(AST_PARAMETER, current_token.text, param_type_token.line, param_type_token.col);
        strncpy(param_node->data_type, param_type_str, sizeof(param_node->data_type) - 1);
        param_node->data_type[sizeof(param_node->data_type) - 1] = '\0';
        free(param_type_str);
        advance();

        // Check for array parameter type like: type name[]
        if (current_token.type == TOKEN_SYMBOL && strcmp(current_token.text, "[") == 0) {
            advance();
            if (current_token.type == TOKEN_SYMBOL && strcmp(current_token.text, "]") == 0) {
                advance();
                param_node->is_array = 1;
                strcat(param_node->data_type, "[]");
            }
            else {
                fprintf(stderr, "Error (L%d:%d): Expected ']' for array parameter '%s'.\n", current_token.line, current_token.col, param_node->value);
                free_ast(param_node); free_ast(head); return NULL;
            }
        }


        if (head == NULL) {
            head = tail = param_node;
        }
        else {
            tail->next = param_node;
            tail = param_node;
        }

        if (current_token.type == TOKEN_SYMBOL && strcmp(current_token.text, ")") == 0) {
            break;
        }

        if (current_token.type != TOKEN_SYMBOL || strcmp(current_token.text, ",") != 0) {
            fprintf(stderr, "Error (L%d:%d): Expected ',' or ')' in parameter list\n", current_token.line, current_token.col);
            free_ast(head);
            return NULL;
        }
        advance();
    }

    if (current_token.type == TOKEN_SYMBOL && strcmp(current_token.text, ")") == 0) {
        advance();
    }
    else {
        fprintf(stderr, "Error (L%d:%d): Expected ')' to close parameter list.\n", current_token.line, current_token.col);
        free_ast(head);
        return NULL;
    }
    return head;
}


static ASTNode* parse_struct_declaration() {
    Token struct_keyword_token = current_token;
    advance();
    if (current_token.type != TOKEN_IDENTIFIER) {
        fprintf(stderr, "Error (L%d:%d): Expected struct name\n", current_token.line, current_token.col);
        return NULL;
    }
    ASTNode* node = create_node(AST_STRUCT, current_token.text, struct_keyword_token.line, struct_keyword_token.col);
    advance();

    if (current_token.type != TOKEN_SYMBOL || strcmp(current_token.text, "{") != 0) {
        fprintf(stderr, "Error (L%d:%d): Expected '{' after struct name '%s'\n", current_token.line, current_token.col, node->value);
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
            }
            else {
                last_member->next = member;
                last_member = member;
            }
        }
        else {
            fprintf(stderr, "Error (L%d:%d): Failed to parse struct member in '%s'.\n", current_token.line, current_token.col, node->value);
            free_ast(node);
            free_ast(members);
            return NULL;
        }
    }
    node->left = members;

    if (current_token.type != TOKEN_SYMBOL || strcmp(current_token.text, "}") != 0) {
        fprintf(stderr, "Error (L%d:%d): Expected '}' to close struct definition '%s'.\n", current_token.line, current_token.col, node->value);
        free_ast(node);
        return NULL;
    }
    advance();
    return node;
}


static ASTNode* parse_class_declaration() {
    Token class_keyword_token = current_token;
    advance();
    if (current_token.type != TOKEN_IDENTIFIER) {
        fprintf(stderr, "Error (L%d:%d): Expected class name\n", current_token.line, current_token.col);
        return NULL;
    }
    ASTNode* node = create_node(AST_CLASS, current_token.text, class_keyword_token.line, class_keyword_token.col);
    advance();

    if (current_token.type == TOKEN_KEYWORD && strcmp(current_token.text, "extends") == 0) {
        advance();
        if (current_token.type != TOKEN_IDENTIFIER) {
            fprintf(stderr, "Error (L%d:%d): Expected base class name after 'extends' for class '%s'.\n", current_token.line, current_token.col, node->value);
            free_ast(node);
            return NULL;
        }
        node->right = create_node(AST_IDENTIFIER, current_token.text, current_token.line, current_token.col);
        advance();
    }


    if (current_token.type != TOKEN_SYMBOL || strcmp(current_token.text, "{") != 0) {
        fprintf(stderr, "Error (L%d:%d): Expected '{' after class name or inheritance specifier for '%s'\n", current_token.line, current_token.col, node->value);
        free_ast(node);
        return NULL;
    }
    advance();

    ASTNode* members = NULL;
    ASTNode* last_member = NULL;
    while (current_token.type != TOKEN_EOF && !(current_token.type == TOKEN_SYMBOL && strcmp(current_token.text, "}") == 0)) {
        Token member_start_token = current_token;
        ASTNode* member = parse_statement();

        if (member) {
            if (members == NULL) {
                members = last_member = member;
            }
            else {
                last_member->next = member;
                last_member = member;
            }
        }
        else {
            fprintf(stderr, "Error (L%d:%d): Failed to parse field or method in class '%s'.\n", member_start_token.line, member_start_token.col, node->value);
            if (current_token.type != TOKEN_EOF) advance(); else break;
        }
    }
    node->left = members;

    if (current_token.type != TOKEN_SYMBOL || strcmp(current_token.text, "}") != 0) {
        fprintf(stderr, "Error (L%d:%d): Expected '}' to close class definition '%s'.\n", current_token.line, current_token.col, node->value);
        free_ast(node);
        return NULL;
    }
    advance();
    return node;
}

// --- Expression Parsers ---

static ASTNode* parse_expression() {
    ASTNode* left = parse_primary(); // This handles unary prefix operators as part of primary
    if (!left) {
        // parse_primary would have reported an error if it failed to parse anything meaningful
        // No need to check for unary here again if parse_primary covers it.
        // If parse_primary returns NULL, it's a genuine parsing error for an expression.
        return NULL;
    }
    return parse_binary_expression(left, 0);
}

static ASTNode* parse_binary_expression(ASTNode* left, int min_precedence) {
    while (1) {
        Token op_token = current_token; // Save for potential operator
        int prec = 0;
        // Check if current token is a potential binary operator
        if (current_token.type == TOKEN_OPERATOR ||
            (current_token.type == TOKEN_SYMBOL && (strcmp(current_token.text, "=") == 0 ||
                strcmp(current_token.text, "<") == 0 ||
                strcmp(current_token.text, ">") == 0)
                )) {
            prec = get_precedence(current_token.text);
        }
        else {
            break; // Not a binary operator we handle here
        }

        if (prec <= min_precedence) {
            break;
        }

        advance(); // Consume the operator

        ASTNode* right = parse_primary(); // Parse RHS primary
        if (!right) { // Higher precedence ops bind tighter
            // If parse_primary fails, it's an error on RHS
            fprintf(stderr, "Error (L%d:%d): Expected expression for right-hand side of binary operator '%s'\n", op_token.line, op_token.col, op_token.text);
            free_ast(left);
            return NULL;
        }

        // Handle right-associativity or higher precedence on the right
        while (1) {
            Token next_op_token = current_token;
            int next_prec = 0;
            if (next_op_token.type == TOKEN_OPERATOR ||
                (next_op_token.type == TOKEN_SYMBOL && (strcmp(next_op_token.text, "=") == 0 ||
                    strcmp(next_op_token.text, "<") == 0 ||
                    strcmp(next_op_token.text, ">") == 0))) {
                next_prec = get_precedence(next_op_token.text);
            }
            else {
                break;
            }

            // For left-associative: if next_prec <= prec, break.
            // For right-associative (like '='): if next_prec < prec, break. (or handle with prec-1 for recursive call)
            if (strcmp(op_token.text, "=") == 0) { // Assignment is right-associative
                if (next_prec < prec) break; // For right-associative: recurse if same or higher precedence
                right = parse_binary_expression(right, prec - 1); // Pass (prec - 1) for right-associativity
            }
            else { // Left-associative
                if (next_prec <= prec) break;
                right = parse_binary_expression(right, next_prec);
            }

            if (!right) { free_ast(left); return NULL; }
        }

        ASTNode* new_left = create_node(AST_BINARY_OP, op_token.text, op_token.line, op_token.col);
        new_left->left = left;
        new_left->right = right;
        left = new_left;
    }
    return left;
}


static ASTNode* parse_primary() {
    ASTNode* node = NULL;
    Token start_token = current_token;

    // Handle unary prefix operators
    if (current_token.type == TOKEN_OPERATOR &&
        (strcmp(current_token.text, "-") == 0 || strcmp(current_token.text, "+") == 0 || strcmp(current_token.text, "!") == 0)) {
        Token op_token = current_token;
        advance();
        // The operand of a unary operator should be parsed with a precedence higher than most binary operators.
        // parse_primary() itself or a specific parse_unary_operand() that handles high precedence (like member access) is needed.
        ASTNode* operand = parse_primary(); // Recursive call for chained unary or high-precedence constructs
        if (!operand) {
            fprintf(stderr, "Error (L%d:%d): Expected operand after unary operator '%s'.\n", op_token.line, op_token.col, op_token.text);
            return NULL;
        }
        node = create_node(AST_UNARY_OP, op_token.text, op_token.line, op_token.col);
        node->left = operand;
        // After parsing a unary expression, it can be the start of member access, etc.
        // So, fall through to the postfix operator loop.
    }
    else if (current_token.type == TOKEN_KEYWORD &&
        (strcmp(current_token.text, "true") == 0 || strcmp(current_token.text, "false") == 0)) {
        node = create_node(AST_LITERAL, current_token.text, start_token.line, start_token.col);
        strncpy(node->data_type, "bool", sizeof(node->data_type) - 1);
        node->data_type[sizeof(node->data_type) - 1] = '\0';
        advance();
    }
    else if (current_token.type == TOKEN_KEYWORD && strcmp(current_token.text, "this") == 0) {
        node = parse_this_reference();
    }
    else if (current_token.type == TOKEN_KEYWORD && strcmp(current_token.text, "new") == 0) {
        node = parse_new_expression();
    }
    else if (current_token.type == TOKEN_SYMBOL && strcmp(current_token.text, "(") == 0) {
        advance();
        node = parse_expression();
        if (!node) { return NULL; }
        if (current_token.type != TOKEN_SYMBOL || strcmp(current_token.text, ")") != 0) {
            fprintf(stderr, "Error (L%d:%d): Expected ')' after parenthesized expression.\n", start_token.line, start_token.col);
            free_ast(node); return NULL;
        }
        advance();
    }
    else if (current_token.type == TOKEN_SYMBOL && strcmp(current_token.text, "[") == 0) {
        node = parse_array_literal();
    }
    else {
        node = parse_literal_or_identifier(); // Handles numbers, strings, identifiers
    }

    // Loop for postfix operators: member access '.', index '[]', function call '()'
    while (node != NULL) { // Condition ensures we don't loop if primary parsing failed
        if (current_token.type == TOKEN_SYMBOL && strcmp(current_token.text, ".") == 0) {
            node = parse_member_access(node); // Update node with the member access AST
            if (!node) return NULL; // Error in member access
        }
        else if (current_token.type == TOKEN_SYMBOL && strcmp(current_token.text, "[") == 0) {
            node = parse_member_access(node); // parse_member_access handles '[' for index
            if (!node) return NULL; // Error in index access
        }
        else if (current_token.type == TOKEN_SYMBOL && strcmp(current_token.text, "(") == 0) {
            // This is a function call where `node` is the function identifier/expression
            Token call_start_token = current_token; // For '('
            advance(); // consume '('
            ASTNode* args = NULL;
            ASTNode* last_arg = NULL;

            if (!(current_token.type == TOKEN_SYMBOL && strcmp(current_token.text, ")") == 0)) {
                while (1) {
                    ASTNode* arg = parse_expression();
                    if (!arg) {
                        fprintf(stderr, "Error (L%d:%d): Failed to parse function call argument for '%s'.\n", call_start_token.line, call_start_token.col, node->value);
                        free_ast(node); free_ast(args); return NULL;
                    }
                    if (!args) args = last_arg = arg;
                    else { last_arg->next = arg; last_arg = arg; }

                    if (current_token.type == TOKEN_SYMBOL && strcmp(current_token.text, ")") == 0) break;
                    if (current_token.type != TOKEN_SYMBOL || strcmp(current_token.text, ",") != 0) {
                        fprintf(stderr, "Error (L%d:%d): Expected ',' or ')' in argument list for '%s'.\n", current_token.line, current_token.col, node->value);
                        free_ast(node); free_ast(args); return NULL;
                    }
                    advance();
                }
            }
            if (current_token.type != TOKEN_SYMBOL || strcmp(current_token.text, ")") != 0) {
                fprintf(stderr, "Error (L%d:%d): Expected ')' to close argument list for '%s'.\n", current_token.line, current_token.col, node->value);
                free_ast(node); free_ast(args); return NULL;
            }
            advance();

            // Create AST_CALL node. `node` is the function being called.
            // If `node` was AST_MEMBER_ACCESS (obj.method), its value is "method", left is "obj".
            ASTNode* call_node = create_node(AST_CALL, node->value, node->line, node->col);
            call_node->left = args;
            if (node->type == AST_MEMBER_ACCESS) {
                call_node->right = node->left; // The object/class expression
                strncpy(call_node->value, node->value, sizeof(call_node->value) - 1); // The method name from member access
                // The original 'node' (which was AST_MEMBER_ACCESS) is now replaced by 'call_node'.
                // We need to free the old 'node' to avoid memory leak if it was heap allocated.
                // However, 'node' is just a pointer. If parse_member_access returned a new node,
                // the old 'target' was its child.
                // This part is tricky. Let's assume `node` points to the AST_MEMBER_ACCESS node.
                // We are effectively "upgrading" it to a call.
                // We need to ensure the original target (node->left) isn't lost if node itself is freed.
                ASTNode* target_for_call = node->left; // Save target from member access
                node->left = NULL; // Detach from old member access node before freeing it
                free_ast(node);    // Free the member access node
                call_node->right = target_for_call; // Set the target for the call
            }
            else {
                call_node->right = NULL; // Plain function call, no specific target object
            }
            node = call_node; // Update node to be the new AST_CALL node
        }
        else {
            break; // Not a postfix operator we handle here, end loop
        }
    }
    return node;
}


// Simplified parse_member_access called by parse_primary's loop
// It handles ONE level of '.' or '[' access.
static ASTNode* parse_member_access(ASTNode* target) {
    Token op_token = current_token;

    if (current_token.type == TOKEN_SYMBOL && strcmp(current_token.text, ".") == 0) {
        advance();
        if (current_token.type != TOKEN_IDENTIFIER) {
            fprintf(stderr, "Error (L%d:%d): Expected identifier for member access after '.'.\n", op_token.line, op_token.col);
            free_ast(target);
            return NULL;
        }

        ASTNode* member_node = create_node(AST_MEMBER_ACCESS, current_token.text, op_token.line, op_token.col);
        member_node->left = target;
        advance();
        return member_node;

    }
    else if (current_token.type == TOKEN_SYMBOL && strcmp(current_token.text, "[") == 0) {
        advance();
        ASTNode* index_expr = parse_expression();
        if (!index_expr) {
            fprintf(stderr, "Error (L%d:%d): Expected expression for index access.\n", op_token.line, op_token.col);
            free_ast(target);
            return NULL;
        }
        ASTNode* index_node = create_node(AST_INDEX_ACCESS, "[]", op_token.line, op_token.col);
        index_node->left = target;
        index_node->right = index_expr;

        if (current_token.type != TOKEN_SYMBOL || strcmp(current_token.text, "]") != 0) {
            fprintf(stderr, "Error (L%d:%d): Expected ']'.\n", current_token.line, current_token.col);
            free_ast(target); free_ast(index_expr); free_ast(index_node);
            return NULL;
        }
        advance();
        return index_node;
    }
    return target; // Should not be reached if called correctly from parse_primary loop
}


static ASTNode* parse_literal_or_identifier() {
    ASTNodeType type;
    Token current_start_token = current_token;

    switch (current_token.type) {
    case TOKEN_NUMBER:
        type = AST_LITERAL;
        break;
    case TOKEN_STRING:
        type = AST_LITERAL;
        break;
    case TOKEN_BOOL:
        type = AST_LITERAL;
        break;
    case TOKEN_IDENTIFIER:
        type = AST_IDENTIFIER;
        break;
    default:
        fprintf(stderr, "Error (L%d:%d): Expected literal or identifier, got '%s'.\n", current_start_token.line, current_start_token.col, current_start_token.text);
        return NULL;
    }
    ASTNode* node = create_node(type, current_token.text, current_start_token.line, current_start_token.col);
    if (type == AST_LITERAL) { // Set data_type for literals
        if (current_token.type == TOKEN_NUMBER) {
            strncpy(node->data_type, strchr(current_token.text, '.') ? "float" : "int", sizeof(node->data_type) - 1);
        }
        else if (current_token.type == TOKEN_STRING) {
            strncpy(node->data_type, "string", sizeof(node->data_type) - 1);
        }
        else if (current_token.type == TOKEN_BOOL) { // Should be "true" or "false"
            strncpy(node->data_type, "bool", sizeof(node->data_type) - 1);
        }
        node->data_type[sizeof(node->data_type) - 1] = '\0';
    }
    advance();
    return node;
}


static ASTNode* parse_variable_declaration() {
    Token keyword_token = current_token;
    advance();

    if (current_token.type != TOKEN_IDENTIFIER) {
        fprintf(stderr, "Error (L%d:%d): Expected identifier after '%s'\n",
            keyword_token.line, keyword_token.col, keyword_token.text); // Use keyword's line/col
        return NULL;
    }

    ASTNode* var_decl = create_node(AST_VAR_DECL, current_token.text, keyword_token.line, keyword_token.col);
    var_decl->left = NULL; // For untyped VarDecl, left is not used for name node. Name is in value.
    advance();

    if ((current_token.type == TOKEN_OPERATOR || current_token.type == TOKEN_SYMBOL) && strcmp(current_token.text, "=") == 0) {
        advance();
        var_decl->right = parse_expression();
        if (!var_decl->right) {
            fprintf(stderr, "Error (L%d:%d): Failed to parse initializer expression for '%s'\n", current_token.line, current_token.col, var_decl->value);
            free_ast(var_decl); return NULL;
        }
    }
    else {
        var_decl->right = NULL;
    }

    if (current_token.type != TOKEN_SYMBOL || strcmp(current_token.text, ";") != 0) {
        fprintf(stderr, "Error (L%d:%d): Expected ';' after variable declaration of '%s'\n", current_token.line, current_token.col, var_decl->value);
        free_ast(var_decl); return NULL;
    }
    advance();
    return var_decl;
}

static ASTNode* parse_if_statement() {
    Token if_keyword_token = current_token;
    advance();

    if (current_token.type != TOKEN_SYMBOL || strcmp(current_token.text, "(") != 0) {
        fprintf(stderr, "Error (L%d:%d): Expected '(' after 'if'.\n", if_keyword_token.line, if_keyword_token.col);
        return NULL;
    }
    advance();

    ASTNode* condition = parse_expression();
    if (!condition) {
        // Error already reported by parse_expression
        return NULL;
    }

    if (current_token.type != TOKEN_SYMBOL || strcmp(current_token.text, ")") != 0) {
        fprintf(stderr, "Error (L%d:%d): Expected ')' after if-condition.\n", current_token.line, current_token.col);
        free_ast(condition); return NULL;
    }
    advance();

    if (current_token.type != TOKEN_SYMBOL || strcmp(current_token.text, "{") != 0) {
        fprintf(stderr, "Error (L%d:%d): Expected '{' to open if-body.\n", current_token.line, current_token.col);
        free_ast(condition); return NULL;
    }
    Token then_body_start_token = current_token;
    advance();

    ASTNode* then_block = parse_block();
    if (!then_block) {
        fprintf(stderr, "Error (L%d:%d): Failed to parse 'then' block for if statement.\n", then_body_start_token.line, then_body_start_token.col);
        free_ast(condition); return NULL;
    }

    if (current_token.type != TOKEN_SYMBOL || strcmp(current_token.text, "}") != 0) {
        fprintf(stderr, "Error (L%d:%d): Expected '}' to close if-body. Got '%s'.\n", current_token.line, current_token.col, current_token.text);
        free_ast(condition); free_ast(then_block); return NULL;
    }
    advance();

    ASTNode* if_node = create_node(AST_IF, "if", if_keyword_token.line, if_keyword_token.col);
    if_node->left = condition;
    if_node->right = then_block;

    if (current_token.type == TOKEN_KEYWORD && strcmp(current_token.text, "else") == 0) {
        Token else_keyword_token = current_token;
        advance();

        ASTNode* else_node_content = NULL;
        if (current_token.type == TOKEN_KEYWORD && strcmp(current_token.text, "if") == 0) {
            else_node_content = parse_if_statement();
            if (!else_node_content) { free_ast(if_node); return NULL; }
        }
        else if (current_token.type == TOKEN_SYMBOL && strcmp(current_token.text, "{") == 0) {
            Token else_body_start_token = current_token;
            advance();
            else_node_content = parse_block();
            if (!else_node_content) {
                fprintf(stderr, "Error (L%d:%d): Failed to parse 'else' block.\n", else_body_start_token.line, else_body_start_token.col);
                free_ast(if_node); return NULL;
            }
            if (current_token.type != TOKEN_SYMBOL || strcmp(current_token.text, "}") != 0) {
                fprintf(stderr, "Error (L%d:%d): Expected '}' to close else-body. Got '%s'.\n", current_token.line, current_token.col, current_token.text);
                free_ast(if_node); free_ast(else_node_content); return NULL;
            }
            advance();
        }
        else {
            fprintf(stderr, "Error (L%d:%d): Expected '{' or 'if' after 'else'.\n", else_keyword_token.line, else_keyword_token.col);
            free_ast(if_node); return NULL;
        }

        ASTNode* else_ast_node = create_node(AST_ELSE, "else", else_keyword_token.line, else_keyword_token.col);
        else_ast_node->left = else_node_content;
        if_node->next = else_ast_node;
    }
    return if_node;
}


static ASTNode* parse_while_statement() {
    Token while_keyword_token = current_token;
    advance();

    if (current_token.type != TOKEN_SYMBOL || strcmp(current_token.text, "(") != 0) {
        fprintf(stderr, "Error (L%d:%d): Expected '(' after 'while'.\n", while_keyword_token.line, while_keyword_token.col);
        return NULL;
    }
    advance();

    ASTNode* condition = parse_expression();
    if (!condition) { return NULL; }

    if (current_token.type != TOKEN_SYMBOL || strcmp(current_token.text, ")") != 0) {
        fprintf(stderr, "Error (L%d:%d): Expected ')' after while-condition.\n", current_token.line, current_token.col);
        free_ast(condition); return NULL;
    }
    advance();

    if (current_token.type != TOKEN_SYMBOL || strcmp(current_token.text, "{") != 0) {
        fprintf(stderr, "Error (L%d:%d): Expected '{' to open while-body.\n", current_token.line, current_token.col);
        free_ast(condition); return NULL;
    }
    Token body_start_token = current_token;
    advance();

    ASTNode* body = parse_block();
    if (!body) {
        fprintf(stderr, "Error (L%d:%d): Failed to parse while-body.\n", body_start_token.line, body_start_token.col);
        free_ast(condition); return NULL;
    }

    if (current_token.type != TOKEN_SYMBOL || strcmp(current_token.text, "}") != 0) {
        fprintf(stderr, "Error (L%d:%d): Expected '}' to close while-body. Got '%s'.\n", current_token.line, current_token.col, current_token.text);
        free_ast(condition); free_ast(body); return NULL;
    }
    advance();

    ASTNode* while_node = create_node(AST_WHILE, "while", while_keyword_token.line, while_keyword_token.col);
    while_node->left = condition;
    while_node->right = body;
    return while_node;
}

static ASTNode* parse_for_statement() {
    Token for_keyword_token = current_token;
    advance();

    if (current_token.type != TOKEN_SYMBOL || strcmp(current_token.text, "(") != 0) {
        fprintf(stderr, "Error (L%d:%d): Expected '(' after 'for'.\n", for_keyword_token.line, for_keyword_token.col);
        return NULL;
    }
    advance();

    ASTNode* init_expr = NULL;
    if (!(current_token.type == TOKEN_SYMBOL && strcmp(current_token.text, ";") == 0)) {
        // Allow full variable declaration (typed or untyped) or an expression.
        // A full var decl statement includes its own ';'. We must NOT consume the loop's ';'.
        Token before_init = current_token;
        if ((current_token.type == TOKEN_KEYWORD && (strcmp(current_token.text, "let") == 0 || strcmp(current_token.text, "var") == 0)) ||
            is_builtin_type_keyword(current_token.text) ||
            (current_token.type == TOKEN_IDENTIFIER && is_builtin_type_keyword(peek_token().text))
            ) {
            // This needs a special "parse_declaration_without_semicolon" or modify existing ones.
            // For now, simplified: parse as statement, then check for semicolon.
            // This is tricky. A common way is to have parse_statement return what it parsed,
            // and then check if current_token is ';'. For for-loop, we expect ';' after init, not within.
            // Let's assume init part can be an expression, which covers `i=0`. `let i=0` needs more.
            // For simplicity, using parse_expression for init for now.
            init_expr = parse_expression();
            if (!init_expr && strcmp(current_token.text, ";") != 0) { // If failed AND not just an empty init
                fprintf(stderr, "Error (L%d:%d): Failed to parse for-loop initializer.\n", before_init.line, before_init.col);
                return NULL;
            }
        }
        else if (strcmp(current_token.text, ";") != 0) { // Not empty, not a declaration keyword -> must be expression
            init_expr = parse_expression();
            if (!init_expr) { // parse_expression would have reported error
                fprintf(stderr, "Error (L%d:%d): Failed to parse for-loop initializer expression.\n", before_init.line, before_init.col);
                return NULL;
            }
        }
    }

    if (current_token.type != TOKEN_SYMBOL || strcmp(current_token.text, ";") != 0) {
        fprintf(stderr, "Error (L%d:%d): Expected ';' after for-loop initializer.\n", current_token.line, current_token.col);
        free_ast(init_expr); return NULL;
    }
    advance();

    ASTNode* cond_expr = NULL;
    if (!(current_token.type == TOKEN_SYMBOL && strcmp(current_token.text, ";") == 0)) {
        cond_expr = parse_expression();
        if (!cond_expr && strcmp(current_token.text, ";") != 0) {
            fprintf(stderr, "Error (L%d:%d): Failed to parse for-loop condition.\n", current_token.line, current_token.col);
            free_ast(init_expr); return NULL;
        }
    }
    if (current_token.type != TOKEN_SYMBOL || strcmp(current_token.text, ";") != 0) {
        fprintf(stderr, "Error (L%d:%d): Expected ';' after for-loop condition.\n", current_token.line, current_token.col);
        free_ast(init_expr); free_ast(cond_expr); return NULL;
    }
    advance();

    ASTNode* incr_expr = NULL;
    if (!(current_token.type == TOKEN_SYMBOL && strcmp(current_token.text, ")") == 0)) {
        incr_expr = parse_expression();
        if (!incr_expr && strcmp(current_token.text, ")") != 0) {
            fprintf(stderr, "Error (L%d:%d): Failed to parse for-loop increment.\n", current_token.line, current_token.col);
            free_ast(init_expr); free_ast(cond_expr); return NULL;
        }
    }
    if (current_token.type != TOKEN_SYMBOL || strcmp(current_token.text, ")") != 0) {
        fprintf(stderr, "Error (L%d:%d): Expected ')' after for-loop increment.\n", current_token.line, current_token.col);
        free_ast(init_expr); free_ast(cond_expr); free_ast(incr_expr); return NULL;
    }
    advance();

    if (current_token.type != TOKEN_SYMBOL || strcmp(current_token.text, "{") != 0) {
        fprintf(stderr, "Error (L%d:%d): Expected '{' to open for-body.\n", current_token.line, current_token.col);
        free_ast(init_expr); free_ast(cond_expr); free_ast(incr_expr); return NULL;
    }
    Token body_start_token = current_token;
    advance();

    ASTNode* body_block = parse_block();
    if (!body_block) {
        fprintf(stderr, "Error (L%d:%d): Failed to parse for-body.\n", body_start_token.line, body_start_token.col);
        free_ast(init_expr); free_ast(cond_expr); free_ast(incr_expr); return NULL;
    }

    if (current_token.type != TOKEN_SYMBOL || strcmp(current_token.text, "}") != 0) {
        fprintf(stderr, "Error (L%d:%d): Expected '}' to close for-body. Got '%s'.\n", current_token.line, current_token.col, current_token.text);
        free_ast(init_expr); free_ast(cond_expr); free_ast(incr_expr); free_ast(body_block); return NULL;
    }
    advance();

    ASTNode* for_node = create_node(AST_FOR, "for", for_keyword_token.line, for_keyword_token.col);

    ASTNode* current_control = NULL;
    if (init_expr) {
        for_node->left = init_expr;
        current_control = init_expr;
    }
    if (cond_expr) {
        if (current_control) current_control->next = cond_expr;
        else for_node->left = cond_expr;
        current_control = cond_expr;
    }
    if (incr_expr) {
        if (current_control) current_control->next = incr_expr;
        else for_node->left = incr_expr;
    }
    if (current_control && !incr_expr && cond_expr) { // e.g. for(init; cond;)
        cond_expr->next = NULL;
    }
    else if (current_control && !cond_expr && init_expr) { // e.g. for(init;;incr) or for(init;;)
        init_expr->next = incr_expr; // incr_expr could be null here too
    }
    // If only init_expr: for_node->left = init_expr, init_expr->next = NULL (implicitly by create_node)
    // If no control parts, for_node->left is NULL.

    for_node->right = body_block;
    return for_node;
}


static ASTNode* parse_return_statement() {
    Token return_keyword_token = current_token;
    advance();
    ASTNode* node = create_node(AST_RETURN, "return", return_keyword_token.line, return_keyword_token.col);

    if (!(current_token.type == TOKEN_SYMBOL && strcmp(current_token.text, ";") == 0)) {
        node->left = parse_expression();
        if (!node->left && !(current_token.type == TOKEN_SYMBOL && strcmp(current_token.text, ";") == 0)) {
            fprintf(stderr, "Error (L%d:%d): Failed to parse return expression.\n", current_token.line, current_token.col);
            free_ast(node); return NULL;
        }
    }
    else {
        node->left = NULL;
    }

    if (current_token.type == TOKEN_SYMBOL && strcmp(current_token.text, ";") == 0) {
        advance();
    }
    else {
        fprintf(stderr, "Warning (L%d:%d): Missing semicolon after return statement.\n", return_keyword_token.line, return_keyword_token.col);
    }
    return node;
}

static ASTNode* parse_function() {
    Token func_keyword_token = current_token;
    advance();

    if (current_token.type != TOKEN_IDENTIFIER) {
        fprintf(stderr, "Error (L%d:%d): Expected function name\n", func_keyword_token.line, func_keyword_token.col);
        return NULL;
    }

    ASTNode* func = create_node(AST_FUNCTION, current_token.text, func_keyword_token.line, func_keyword_token.col);
    Token func_name_token = current_token;
    advance();

    if (current_token.type != TOKEN_SYMBOL || strcmp(current_token.text, "(") != 0) {
        fprintf(stderr, "Error (L%d:%d): Expected '(' after function name '%s'\n", func_name_token.line, func_name_token.col, func_name_token.text);
        free_ast(func); return NULL;
    }
    advance();

    ASTNode* params = NULL;
    ASTNode* last_param = NULL;

    if (!(current_token.type == TOKEN_SYMBOL && strcmp(current_token.text, ")") == 0)) {
        while (1) {
            if (current_token.type != TOKEN_IDENTIFIER) {
                fprintf(stderr, "Error (L%d:%d): Expected parameter name in function '%s'\n", current_token.line, current_token.col, func_name_token.text);
                free_ast(func); free_ast(params); return NULL;
            }

            ASTNode* param = create_node(AST_PARAMETER, current_token.text, current_token.line, current_token.col);
            advance();

            if (params == NULL) params = last_param = param;
            else { last_param->next = param; last_param = param; }

            if (current_token.type == TOKEN_SYMBOL && strcmp(current_token.text, ")") == 0) break;
            if (current_token.type != TOKEN_SYMBOL || strcmp(current_token.text, ",") != 0) {
                fprintf(stderr, "Error (L%d:%d): Expected ',' or ')' in parameter list for '%s'\n", current_token.line, current_token.col, func_name_token.text);
                free_ast(func); free_ast(params); return NULL;
            }
            advance();
        }
    }

    func->left = params;
    if (current_token.type == TOKEN_SYMBOL && strcmp(current_token.text, ")") == 0) {
        advance();
    }
    else {
        fprintf(stderr, "Error (L%d:%d): Expected ')' to close parameter list for '%s'.\n", current_token.line, current_token.col, func_name_token.text);
        free_ast(func); return NULL;
    }

    if (current_token.type != TOKEN_SYMBOL || strcmp(current_token.text, "{") != 0) {
        fprintf(stderr, "Error (L%d:%d): Expected '{' to begin function body for '%s'\n", current_token.line, current_token.col, func_name_token.text);
        free_ast(func); return NULL;
    }
    Token body_start_token = current_token;
    advance();
    func->right = parse_block();
    if (!func->right) {
        fprintf(stderr, "Error (L%d:%d): Failed to parse function body for '%s'\n", body_start_token.line, body_start_token.col, func_name_token.text);
        free_ast(func); return NULL;
    }

    if (current_token.type != TOKEN_SYMBOL || strcmp(current_token.text, "}") != 0) {
        fprintf(stderr, "Error (L%d:%d): Expected '}' to close function body for '%s'. Got '%s'.\n", current_token.line, current_token.col, func_name_token.text, current_token.text);
        free_ast(func); return NULL;
    }
    advance();
    return func;
}

static ASTNode* parse_print_statement() {
    Token print_keyword_token = current_token;
    advance();

    if (current_token.type != TOKEN_SYMBOL || strcmp(current_token.text, "(") != 0) {
        fprintf(stderr, "Error (L%d:%d): Expected '(' after print.\n", print_keyword_token.line, print_keyword_token.col);
        return NULL;
    }
    advance();

    ASTNode* expr = parse_expression();
    if (!expr) {
        fprintf(stderr, "Error (L%d:%d): Expected expression in print statement.\n", current_token.line, current_token.col);
        return NULL;
    }

    if (current_token.type != TOKEN_SYMBOL || strcmp(current_token.text, ")") != 0) {
        fprintf(stderr, "Error (L%d:%d): Expected ')' after print argument\n", current_token.line, current_token.col);
        free_ast(expr); return NULL;
    }
    advance();

    if (current_token.type != TOKEN_SYMBOL || strcmp(current_token.text, ";") != 0) {
        fprintf(stderr, "Error (L%d:%d): Expected ';' after print statement.\n", current_token.line, current_token.col);
        free_ast(expr); return NULL;
    }
    advance();

    ASTNode* print_node = create_node(AST_PRINT, "print", print_keyword_token.line, print_keyword_token.col);
    print_node->left = expr;
    return print_node;
}

static ASTNode* parse_array_literal() {
    Token start_token = current_token; // '['
    advance();

    ASTNode* head_element = NULL;
    ASTNode* tail_element = NULL;

    if (!(current_token.type == TOKEN_SYMBOL && strcmp(current_token.text, "]") == 0)) {
        while (1) {
            ASTNode* elem_expr = parse_expression();
            if (!elem_expr) {
                fprintf(stderr, "Error (L%d:%d): Failed to parse array element.\n", current_token.line, current_token.col);
                free_ast(head_element); return NULL;
            }
            if (!head_element) head_element = tail_element = elem_expr;
            else { tail_element->next = elem_expr; tail_element = elem_expr; }

            if (current_token.type == TOKEN_SYMBOL && strcmp(current_token.text, "]") == 0) break;
            if (current_token.type != TOKEN_SYMBOL || strcmp(current_token.text, ",") != 0) {
                fprintf(stderr, "Error (L%d:%d): Expected ',' or ']' in array literal.\n", current_token.line, current_token.col);
                free_ast(head_element); return NULL;
            }
            advance();
        }
    }

    if (current_token.type != TOKEN_SYMBOL || strcmp(current_token.text, "]") != 0) {
        fprintf(stderr, "Error (L%d:%d): Unterminated array literal, expected ']'.\n", start_token.line, start_token.col);
        free_ast(head_element); return NULL;
    }
    advance();

    ASTNode* arr_node = create_node(AST_ARRAY, "array_literal", start_token.line, start_token.col);
    arr_node->left = head_element;
    strncpy(arr_node->data_type, "array", sizeof(arr_node->data_type) - 1);
    arr_node->data_type[sizeof(arr_node->data_type) - 1] = '\0';
    return arr_node;
}


static ASTNode* parse_new_expression() {
    Token new_keyword_token = current_token;
    advance();
    if (current_token.type != TOKEN_IDENTIFIER) {
        fprintf(stderr, "Error (L%d:%d): Expected class name after 'new'.\n", new_keyword_token.line, new_keyword_token.col);
        return NULL;
    }
    ASTNode* node = create_node(AST_NEW, current_token.text, new_keyword_token.line, new_keyword_token.col);
    strncpy(node->data_type, current_token.text, sizeof(node->data_type) - 1);
    node->data_type[sizeof(node->data_type) - 1] = '\0';
    Token class_name_token = current_token;
    advance();

    if (current_token.type == TOKEN_SYMBOL && strcmp(current_token.text, "(") == 0) {
        advance();

        ASTNode* args = NULL;
        ASTNode* last_arg = NULL;

        if (!(current_token.type == TOKEN_SYMBOL && strcmp(current_token.text, ")") == 0)) {
            while (1) {
                ASTNode* arg = parse_expression();
                if (!arg) {
                    fprintf(stderr, "Error (L%d:%d): Failed to parse constructor argument for 'new %s'.\n", current_token.line, current_token.col, class_name_token.text);
                    free_ast(node); free_ast(args); return NULL;
                }
                if (args == NULL) args = last_arg = arg;
                else { last_arg->next = arg; last_arg = arg; }

                if (current_token.type == TOKEN_SYMBOL && strcmp(current_token.text, ")") == 0) break;
                if (current_token.type != TOKEN_SYMBOL || strcmp(current_token.text, ",") != 0) {
                    fprintf(stderr, "Error (L%d:%d): Expected ',' or ')' in constructor arguments for 'new %s'.\n", current_token.line, current_token.col, class_name_token.text);
                    free_ast(node); free_ast(args); return NULL;
                }
                advance();
            }
        }
        if (current_token.type != TOKEN_SYMBOL || strcmp(current_token.text, ")") != 0) {
            fprintf(stderr, "Error (L%d:%d): Expected ')' to close constructor arguments for 'new %s'.\n", current_token.line, current_token.col, class_name_token.text);
            free_ast(node); free_ast(args); return NULL;
        }
        advance();
        node->left = args;
    }
    else {
        node->left = NULL;
    }
    return node;
}

static ASTNode* parse_this_reference() {
    Token this_token = current_token;
    advance();
    ASTNode* node = create_node(AST_THIS, "this", this_token.line, this_token.col);
    return node;
}

static ASTNode* parse_import() {
    Token import_keyword_token = current_token;
    advance();

    if (current_token.type != TOKEN_STRING) {
        fprintf(stderr, "Error (L%d:%d): Expected string literal for module name after import\n", import_keyword_token.line, import_keyword_token.col);
        return NULL;
    }

    ASTNode* import_node = create_node(AST_IMPORT, current_token.text, import_keyword_token.line, import_keyword_token.col);
    advance();

    if (current_token.type != TOKEN_SYMBOL || strcmp(current_token.text, ";") != 0) {
        fprintf(stderr, "Error (L%d:%d): Expected ';' after import statement\n", current_token.line, current_token.col);
        free_ast(import_node); return NULL;
    }
    advance();
    return import_node;
}