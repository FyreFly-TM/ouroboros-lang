#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lexer.h"
#include "parser.h"
#include "ast_types.h"
#include "semantic.h"
#include "ir.h"
#include "vm.h"
#include "stdlib.h"
#include "module.h"

// Direct ASCII voxel renderer
void render_ascii_voxel_world() {
    printf("\n");
    printf("=================================================\n");
    printf("          OUROBOROS ASCII VOXEL ENGINE           \n");
    printf("=================================================\n");
    printf("\n");
    
    // Create a simple voxel world - 16x8x16
    // 0 = air, 1 = dirt, 2 = stone, 3 = grass, 4 = gold, 5 = wood, 6 = leaves
    int world[16][8][16] = {0};
    
    // Generate terrain
    for (int x = 0; x < 16; x++) {
        for (int z = 0; z < 16; z++) {
            // Base terrain height
            int height = 1 + ((x + z) % 3);
            
            // Create hills
            if (x > 3 && x < 12 && z > 3 && z < 12) {
                height += 1;
            }
            
            // Create mountain
            if (x > 6 && x < 10 && z > 6 && z < 10) {
                height += 2;
            }
            
            // Fill the terrain
            for (int y = 0; y < height; y++) {
                if (y == height - 1) {
                    world[x][y][z] = 3; // Grass on top
                } else if (y > height - 3) {
                    world[x][y][z] = 1; // Dirt
                } else {
                    world[x][y][z] = 2; // Stone
                }
            }
        }
    }
    
    // Add a tree
    int tree_x = 8;
    int tree_z = 8;
    int tree_base = 0;
    
    // Find ground level
    for (int y = 7; y >= 0; y--) {
        if (world[tree_x][y][tree_z] != 0) {
            tree_base = y;
            break;
        }
    }
    
    // Plant tree
    world[tree_x][tree_base+1][tree_z] = 5; // Tree trunk
    world[tree_x][tree_base+2][tree_z] = 5; // Tree trunk
    
    // Leaves
    world[tree_x][tree_base+3][tree_z] = 6;
    world[tree_x+1][tree_base+2][tree_z] = 6;
    world[tree_x-1][tree_base+2][tree_z] = 6;
    world[tree_x][tree_base+2][tree_z+1] = 6;
    world[tree_x][tree_base+2][tree_z-1] = 6;
    
    // Add some gold
    world[3][0][3] = 4;
    world[4][0][3] = 4;
    world[3][0][4] = 4;
    
    // Display top-down view (top layer)
    printf("Top-down view (y=7):\n");
    printf("+--------------------------------+\n");
    
    for (int z = 0; z < 16; z++) {
        printf("|");
        for (int x = 0; x < 16; x++) {
            char block_char = ' ';
            
            switch (world[x][7][z]) {
                case 0: block_char = ' '; break; // Air
                case 1: block_char = '#'; break; // Dirt
                case 2: block_char = '%'; break; // Stone
                case 3: block_char = '^'; break; // Grass
                case 4: block_char = '*'; break; // Gold
                case 5: block_char = '|'; break; // Wood
                case 6: block_char = '@'; break; // Leaves
            }
            
            printf("%c", block_char);
        }
        printf("|\n");
    }
    
    printf("+--------------------------------+\n\n");
    
    // Display side view
    printf("Side view (z=8):\n");
    printf("+--------------------------------+\n");
    
    for (int y = 7; y >= 0; y--) {
        printf("|");
        for (int x = 0; x < 16; x++) {
            char block_char = ' ';
            
            switch (world[x][y][8]) {
                case 0: block_char = ' '; break; // Air
                case 1: block_char = '#'; break; // Dirt
                case 2: block_char = '%'; break; // Stone
                case 3: block_char = '^'; break; // Grass
                case 4: block_char = '*'; break; // Gold
                case 5: block_char = '|'; break; // Wood
                case 6: block_char = '@'; break; // Leaves
            }
            
            printf("%c", block_char);
        }
        printf("|\n");
    }
    
    printf("+--------------------------------+\n\n");
    
    // Display front view
    printf("Front view (x=8):\n");
    printf("+--------------------------------+\n");
    
    for (int y = 7; y >= 0; y--) {
        printf("|");
        for (int z = 0; z < 16; z++) {
            char block_char = ' ';
            
            switch (world[8][y][z]) {
                case 0: block_char = ' '; break; // Air
                case 1: block_char = '#'; break; // Dirt
                case 2: block_char = '%'; break; // Stone
                case 3: block_char = '^'; break; // Grass
                case 4: block_char = '*'; break; // Gold
                case 5: block_char = '|'; break; // Wood
                case 6: block_char = '@'; break; // Leaves
            }
            
            printf("%c", block_char);
        }
        printf("|\n");
    }
    
    printf("+--------------------------------+\n\n");
    
    printf("First-person view from (7,2,7) looking north:\n");
    printf("+--------------------------------+\n");
    
    // Simple first-person view (just show what's directly in front)
    for (int y = 5; y >= 0; y--) {
        printf("|");
        for (int x = 0; x < 16; x++) {
            int view_x = x - 8 + 7; // Center view on player position
            int view_z = 6; // Looking north (z-1)
            
            // Skip out of bounds coordinates
            if (view_x < 0 || view_x >= 16 || view_z < 0 || view_z >= 16) {
                printf(" ");
                continue;
            }
            
            char block_char = ' ';
            
            switch (world[view_x][y][view_z]) {
                case 0: block_char = ' '; break; // Air
                case 1: block_char = '#'; break; // Dirt
                case 2: block_char = '%'; break; // Stone
                case 3: block_char = '^'; break; // Grass
                case 4: block_char = '*'; break; // Gold
                case 5: block_char = '|'; break; // Wood
                case 6: block_char = '@'; break; // Leaves
            }
            
            printf("%c", block_char);
        }
        printf("|\n");
    }
    
    printf("+--------------------------------+\n\n");
    
    printf("Thank you for using Ouroboros ASCII Voxel Engine!\n");
}

// Function to read file content
char* read_file_content(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Error: Cannot open file %s\n", filename);
        return NULL;
    }
    
    // Get file size
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    // Allocate buffer
    char* buffer = (char*)malloc(size + 1);
    if (!buffer) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        fclose(file);
        return NULL;
    }
    
    // Read file content
    size_t read_size = fread(buffer, 1, size, file);
    buffer[read_size] = '\0';
    
    fclose(file);
    return buffer;
}

int main(int argc, char *argv[]) {
    // Check if a filename is provided
    if (argc < 2) {
        printf("Usage: %s <filename> [additional files...]\n", argv[0]);
        printf("       %s -m <module-path> <filename> [additional files...]\n", argv[0]);
        return 1;
    }
    
    int arg_index = 1;
    
    // Initialize module manager
    module_manager_init();
    
    // Check for module path option
    while (arg_index < argc && argv[arg_index][0] == '-') {
        if (strcmp(argv[arg_index], "-m") == 0 && arg_index + 1 < argc) {
            module_manager_add_search_path(argv[arg_index + 1]);
            arg_index += 2;
        } else {
            arg_index++;
        }
    }
    
    if (arg_index >= argc) {
        printf("Error: No input files specified\n");
        return 1;
    }
    
    // Initialize stdlib functions
    register_stdlib_functions();
    
    ASTNode* ast = NULL;
    
    // Check if we have multiple files
    if (argc - arg_index > 1) {
        // Multiple file compilation
        printf("Compiling multiple files...\n");
        char **filenames = &argv[arg_index];
        int file_count = argc - arg_index;
        
        ast = compile_multiple_files(filenames, file_count);
        if (!ast) {
            fprintf(stderr, "Error: Multi-file compilation failed\n");
            module_manager_cleanup();
            return 1;
        }
    } else {
        // Single file compilation (original behavior)
        const char* filename = argv[arg_index];
        
        // Read file content
        char* source = read_file_content(filename);
        if (!source) {
            module_manager_cleanup();
            return 1;
        }
        
        printf("Compiling file: %s\n", filename);
        
        // Lexical analysis
        printf("==== Lexical Analysis ====\n");
        Token* tokens = lex(source);
        if (!tokens) {
            fprintf(stderr, "Error: Lexical analysis failed\n");
            free(source);
            module_manager_cleanup();
            return 1;
        }
        
        // Print tokens
        int i = 0;
        while (tokens[i].type != TOKEN_EOF) {
            printf("Token: Type=%d, Text='%s', Line=%d, Col=%d\n", 
                   tokens[i].type, tokens[i].text, tokens[i].line, tokens[i].col);
            i++;
        }
        
        // Parsing
        ast = parse(tokens);
        if (!ast) {
            fprintf(stderr, "Error: Parsing failed\n");
            free(source);
            module_manager_cleanup();
            return 1;
        }
        
        free(source);
    }
    
    // Print AST
    printf("\n==== Abstract Syntax Tree ====\n");
    print_ast(ast, 0);
    
    // Semantic analysis
    printf("\n==== Semantic Analysis ====\n");
    analyze_program(ast);
    check_semantics(ast);
    
    // Execute the program
    vm_init();
    run_vm(ast);
    
    printf("\nCompilation and execution completed successfully!\n");
    
    // Clean up
    free_ast(ast);
    module_manager_cleanup();
    
    return 0;
}
