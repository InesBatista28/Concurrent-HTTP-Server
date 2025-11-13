// HEADER FILE - INTERFACE
#ifndef CONFIG_H
#define CONFIG_H

#define MAX_CONFIG_LINE 256
#define MAX_PATH_LENGTH 1024

typedef int megabytes_t;
typedef int seconds_t;


typedef struct {
    int port;
    char document_root[MAX_PATH_LENGTH];
    int num_workers;
    int threads_per_worker;
    int max_queue_size;
    char log_file[MAX_PATH_LENGTH];
    megabytes_t cache_size_mb;
    seconds_t timeout_seconds;
} server_config_t;


//API CORE CONFIGURATIONS 
// Initialize configuration with default values
void config_init_defaults(server_config_t *config);
// Load configuration from file
int config_load_from_file(const char *filename, server_config_t *config);
// Create and initialize config with file loading
server_config_t* config_create(const char *filename);

// Free resources allocated by config_create()
void config_destroy(server_config_t *config);

// Validate configuration values
int config_validate(const server_config_t *config);

// Print configuration for debugging
void config_print(const server_config_t *config);


//API GETTERS
// Get port number
int config_get_port(const server_config_t *config);
// Get document root path
const char* config_get_document_root(const server_config_t *config);
// Get number of worker processes
int config_get_num_workers(const server_config_t *config);
// Get threads per worker process
int config_get_threads_per_worker(const server_config_t *config);
// Get maximum queue size
int config_get_max_queue_size(const server_config_t *config);
// Get log file path
const char* config_get_log_file(const server_config_t *config);
// Get cache size in megabytes
megabytes_t config_get_cache_size(const server_config_t *config);
// Get timeout in seconds
seconds_t config_get_timeout(const server_config_t *config);


//API SETTERS
// Set port with validation
int config_set_port(server_config_t *config, int port);
// Set document root path
int config_set_document_root(server_config_t *config, const char *document_root);
// Set number of worker processes
int config_set_num_workers(server_config_t *config, int num_workers);
// Set threads per worker
int config_set_threads_per_worker(server_config_t *config, int threads_per_worker);
// Set log file path
int config_set_log_file(server_config_t *config, const char *log_file);




#endif

