// MAIN APENAS RELATIVA AO TESTE DA NOVA API

#include <stdio.h>
#include <stdlib.h>
#include "config.h"

// Test program for configuration system
int main() {
    printf("=== Testing Complete Config API ===\n\n");
    
    // Test 1: Create config with file loading
    printf("1. Loading config from file:\n");
    server_config_t *config = config_create("server.conf");
    if (config) {
        config_print(config);
        config_destroy(config);
    }
    
    // Test 2: Create config with defaults
    printf("\n2. Default configuration:\n");
    config = config_create(NULL);
    if (config) {
        config_print(config);
        
        // Test getters
        printf("\n3. Testing getters:\n");
        printf("Port: %d\n", config_get_port(config));
        printf("Document Root: %s\n", config_get_document_root(config));
        printf("Workers: %d\n", config_get_num_workers(config));
        
        // Test setters
        printf("\n4. Testing setters:\n");
        config_set_port(config, 9090);
        config_set_document_root(config, "/custom/path");
        config_print(config);
        
        config_destroy(config);
    }
    
    // Test 3: Stack allocation
    printf("\n5. Stack-allocated config:\n");
    server_config_t stack_config;
    config_init_defaults(&stack_config);
    config_load_from_file("server.conf", &stack_config);
    config_print(&stack_config);
    
    printf("\nAll tests completed!\n");
    return 0;
}