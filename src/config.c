#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>
#include "config.h"

// Remove whitespace from beginning and end of a string
static char* trim_whitespace(char *str) {
    if (!str) return NULL;
    char *end;
    // Remove leading whitespace
    while (isspace((unsigned char)*str)) str++;
    // If string became empty, return
    if (*str == 0) return str;
    // Remove trailing whitespace
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

// Check if a directory exists in the filesystem
static int directory_exists(const char *path) {
    struct stat statbuf;
    return (stat(path, &statbuf) == 0 && S_ISDIR(statbuf.st_mode));
}

// Initialize configuration structure with default values
void config_init_defaults(server_config_t *config) {
    if (!config) return;
    config->port = 8080;
    strcpy(config->document_root, "/var/www/html");
    config->num_workers = 4;
    config->threads_per_worker = 10;
    config->max_queue_size = 100;
    strcpy(config->log_file, "access.log");
    config->cache_size_mb = 10;
    config->timeout_seconds = 30;
}

// Load configuration from a file
int config_load_from_file(const char *filename, server_config_t *config) {
    if (!filename || !config) return -1;
    
    // Open configuration file
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Failed to open config file");
        return -1;
    }

    // Start with default values
    config_init_defaults(config);
    char line[MAX_CONFIG_LINE];
    int line_num = 0;

    // Read file line by line
    while (fgets(line, sizeof(line), file)) {
        line_num++;
        // Remove newline from end
        line[strcspn(line, "\n")] = 0;
        // Trim spaces and check if empty line or comment
        char *trimmed_line = trim_whitespace(line);
        if (trimmed_line[0] == '#' || trimmed_line[0] == '\0') continue;

        // Find the '=' symbol that separates key=value
        char *equals = strchr(trimmed_line, '=');
        if (!equals) {
            fprintf(stderr, "Invalid config line %d: %s\n", line_num, trimmed_line);
            continue;
        }

        // Split key from value
        *equals = '\0';
        char *key = trim_whitespace(trimmed_line);
        char *value = trim_whitespace(equals + 1);

        // Process each configuration option
        if (strcmp(key, "PORT") == 0) {
            config_set_port(config, atoi(value));
        }
        else if (strcmp(key, "DOCUMENT_ROOT") == 0) {
            config_set_document_root(config, value);
        }
        else if (strcmp(key, "NUM_WORKERS") == 0) {
            config_set_num_workers(config, atoi(value));
        }
        else if (strcmp(key, "THREADS_PER_WORKER") == 0) {
            config_set_threads_per_worker(config, atoi(value));
        }
        else if (strcmp(key, "MAX_QUEUE_SIZE") == 0) {
            int queue_size = atoi(value);
            if (queue_size > 0) config->max_queue_size = queue_size;
        }
        else if (strcmp(key, "LOG_FILE") == 0) {
            config_set_log_file(config, value);
        }
        else if (strcmp(key, "CACHE_SIZE_MB") == 0) {
            int cache_size = atoi(value);
            if (cache_size > 0) config->cache_size_mb = cache_size;
        }
        else if (strcmp(key, "TIMEOUT_SECONDS") == 0) {
            int timeout = atoi(value);
            if (timeout > 0) config->timeout_seconds = timeout;
        }
        else {
            fprintf(stderr, "Unknown config option: %s\n", key);
        }
    }

    fclose(file);
    
    // Validate final configuration
    if (config_validate(config) != 0) {
        fprintf(stderr, "Configuration validation failed\n");
        return -1;
    }
    
    return 0;
}




// Create a new configuration (allocated on heap)
server_config_t* config_create(const char *filename) {
    // Allocate memory for the structure
    server_config_t *config = malloc(sizeof(server_config_t));
    if (!config) {
        perror("Failed to allocate config");
        return NULL;
    }
    
    // Load from file or use default values
    if (filename) {
        if (config_load_from_file(filename, config) != 0) {
            free(config);
            return NULL;
        }
    } else {
        config_init_defaults(config);
    }
    
    return config;
}

// Free memory allocated by config_create()
void config_destroy(server_config_t *config) {
    free(config);
}

// Validate if all configuration values are valid
int config_validate(const server_config_t *config) {
    if (!config) return -1;
    // Validate port (1-65535)
    if (config->port < 1 || config->port > 65535) {
        fprintf(stderr, "Invalid port: %d\n", config->port);
        return -1;
    }
    // Validate number of workers
    if (config->num_workers < 1) {
        fprintf(stderr, "Invalid number of workers: %d\n", config->num_workers);
        return -1;
    }
    // Validate threads per worker
    if (config->threads_per_worker < 1) {
        fprintf(stderr, "Invalid threads per worker: %d\n", config->threads_per_worker);
        return -1;
    }
    // Validate queue size
    if (config->max_queue_size < 1) {
        fprintf(stderr, "Invalid max queue size: %d\n", config->max_queue_size);
        return -1;
    }
    // Check if document root exists (warning only)
    if (!directory_exists(config->document_root)) {
        fprintf(stderr, "Document root does not exist: %s\n", config->document_root);
    }
    return 0;
}

// Print current configuration for debug
void config_print(const server_config_t *config) {
    if (!config) {
        printf("Configuration: NULL\n");
        return;
    }
    printf("Server Configuration:\n");
    printf("Port: %d\n", config->port);
    printf("Document Root: %s\n", config->document_root);
    printf("Number of Workers: %d\n", config->num_workers);
    printf("Threads per Worker: %d\n", config->threads_per_worker);
    printf("Max Queue Size: %d\n", config->max_queue_size);
    printf("Log File: %s\n", config->log_file);
    printf("Cache Size: %d MB\n", config->cache_size_mb);
    printf("Timeout: %d seconds\n", config->timeout_seconds);
}



// GETTERS IMPLEMENTATION
// Return server port
int config_get_port(const server_config_t *config) {
    return config ? config->port : 0;
}

// Return document root directory
const char* config_get_document_root(const server_config_t *config) {
    return config ? config->document_root : NULL;
}

// Return number of worker processes
int config_get_num_workers(const server_config_t *config) {
    return config ? config->num_workers : 0;
}

// Return number of threads per worker
int config_get_threads_per_worker(const server_config_t *config) {
    return config ? config->threads_per_worker : 0;
}

// Return maximum request queue size
int config_get_max_queue_size(const server_config_t *config) {
    return config ? config->max_queue_size : 0;
}

// Return log file path
const char* config_get_log_file(const server_config_t *config) {
    return config ? config->log_file : NULL;
}

// Return cache size in MB
megabytes_t config_get_cache_size(const server_config_t *config) {
    return config ? config->cache_size_mb : 0;
}

// Return timeout in seconds
seconds_t config_get_timeout(const server_config_t *config) {
    return config ? config->timeout_seconds : 0;
}



//SETTERS IMPLEMENTATION
// Set server port with validation
int config_set_port(server_config_t *config, int port) {
    if (!config || port < 1 || port > 65535) return -1;
    config->port = port;
    return 0;
}

// Set document root directory
int config_set_document_root(server_config_t *config, const char *document_root) {
    if (!config || !document_root || strlen(document_root) >= MAX_PATH_LENGTH) return -1;
    strncpy(config->document_root, document_root, MAX_PATH_LENGTH - 1);
    config->document_root[MAX_PATH_LENGTH - 1] = '\0';
    return 0;
}

// Set number of worker processes
int config_set_num_workers(server_config_t *config, int num_workers) {
    if (!config || num_workers < 1) return -1;
    config->num_workers = num_workers;
    return 0;
}

// Set number of threads per worker
int config_set_threads_per_worker(server_config_t *config, int threads_per_worker) {
    if (!config || threads_per_worker < 1) return -1;
    config->threads_per_worker = threads_per_worker;
    return 0;
}

// Set log file path
int config_set_log_file(server_config_t *config, const char *log_file) {
    if (!config || !log_file || strlen(log_file) >= MAX_PATH_LENGTH) return -1;
    strncpy(config->log_file, log_file, MAX_PATH_LENGTH - 1);
    config->log_file[MAX_PATH_LENGTH - 1] = '\0';
    return 0;
}