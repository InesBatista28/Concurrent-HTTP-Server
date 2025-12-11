#ifndef CONFIG_H
#define CONFIG_H // I'm using include guards to prevent this header from being included multiple times.

#define MAX_PATH_LEN 256 // I'm defining a maximum path length that I'll use throughout my server.

// This structure holds all my server configuration settings.
// I need to keep all these settings together so I can pass them around easily.
typedef struct
{
    int port;                   // I'm storing the port number the server will listen on.
    int num_workers;            // I need to know how many worker processes to create.
    int threads_per_worker;     // Each worker can have multiple threads - this controls that.
    int max_queue_size;         // I'm limiting how many pending connections I'll queue up.
    char document_root[MAX_PATH_LEN]; // This is where I'll look for files to serve.
    char log_file[MAX_PATH_LEN];      // I need to know where to write my log messages.
    int cache_size_mb;          // I'm controlling how much memory the cache can use (in MB).
    int timeout_seconds;        // I'm setting a timeout for idle connections.
    int keep_alive_timeout;     // This controls how long I keep HTTP keep-alive connections open.
} server_config_t;

// Function prototypes - I'm declaring these here so other files know they exist.

// I need a function to load configuration from a file.
// This will parse a simple key=value format file.
int load_config(const char *filename, server_config_t *config);

// I also want to support environment variables for configuration.
// This lets users override file settings with environment variables.
void parse_env_vars(server_config_t *config);

// Finally, I want to support command-line arguments.
// This gives users the most direct way to override settings.
void parse_arguments(int argc, char *argv[], server_config_t *config);

#endif // CONFIG_H