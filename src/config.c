#include "config.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// I'm loading server configuration from a file.
// This function reads a simple key=value format and fills in the config structure.
// I need to handle comments (lines starting with #) and ignore empty lines.
int load_config(const char *filename, server_config_t *config)
{
    // I'm opening the configuration file for reading.
    FILE *fp = fopen(filename, "r");
    if (!fp)
        return -1; // I couldn't open the file, so I return an error.

    // I'll use these buffers to store each line, key, and value as I parse.
    char line[512], key[128], value[256];
    
    // I'm reading the file line by line until I reach the end.
    while (fgets(line, sizeof(line), fp))
    {
        // If the line starts with '#' or is empty, I skip it.
        // These are comments or blank lines that don't contain configuration.
        if (line[0] == '#' || line[0] == '\n')
            continue;

        // I'm parsing each line with a special format string.
        // %[^=] means "read everything up to the equals sign" into key.
        // = means "match the equals sign exactly".
        // %s means "read the rest as a string" into value.
        if (sscanf(line, "%[^=]=%s", key, value) == 2)
        {
            // Now I need to figure out which configuration setting this is.
            // I compare the key against all the possible settings I know about.
            
            if (strcmp(key, "PORT") == 0)
                config->port = atoi(value); // I convert the string to an integer.
            else if (strcmp(key, "NUM_WORKERS") == 0)
                config->num_workers = atoi(value);
            else if (strcmp(key, "THREADS_PER_WORKER") == 0)
                config->threads_per_worker = atoi(value);
            else if (strcmp(key, "DOCUMENT_ROOT") == 0)
                // For strings, I use strncpy to avoid buffer overflows.
                strncpy(config->document_root, value, sizeof(config->document_root));
            else if (strcmp(key, "MAX_QUEUE_SIZE") == 0)
                config->max_queue_size = atoi(value);
            else if (strcmp(key, "LOG_FILE") == 0)
                strncpy(config->log_file, value, sizeof(config->log_file));
            else if (strcmp(key, "CACHE_SIZE_MB") == 0)
                config->cache_size_mb = atoi(value);
            else if (strcmp(key, "TIMEOUT_SECONDS") == 0)
                config->timeout_seconds = atoi(value);
            else if (strcmp(key, "KEEP_ALIVE_TIMEOUT") == 0)
                config->keep_alive_timeout = atoi(value);
            // If the key doesn't match any known setting, I just ignore it.
        }
    }
    fclose(fp); // I'm done with the file, so I close it.
    return 0; // Success!
}

// I also want to support configuration through environment variables.
// This gives users flexibility - they can override file settings with env vars.
void parse_env_vars(server_config_t *config) {
    char *val; // I'll use this to temporarily hold environment variable values.

    // For each environment variable, I check if it exists and parse it if it does.
    // The pattern is: if the env var exists, convert it and store it in the config.
    
    if ((val = getenv("HTTP_PORT"))) config->port = atoi(val);
    if ((val = getenv("HTTP_WORKERS"))) config->num_workers = atoi(val);
    if ((val = getenv("HTTP_THREADS"))) config->threads_per_worker = atoi(val);
    if ((val = getenv("HTTP_ROOT"))) {
        // For strings, I'm careful about buffer boundaries.
        strncpy(config->document_root, val, sizeof(config->document_root) - 1);
        config->document_root[sizeof(config->document_root) - 1] = '\0'; // Ensure null termination.
    }
    if ((val = getenv("HTTP_QUEUE"))) config->max_queue_size = atoi(val);
    if ((val = getenv("HTTP_CACHE"))) config->cache_size_mb = atoi(val);
    if ((val = getenv("HTTP_LOG"))) {
        strncpy(config->log_file, val, sizeof(config->log_file) - 1);
        config->log_file[sizeof(config->log_file) - 1] = '\0';
    }
    if ((val = getenv("HTTP_TIMEOUT"))) config->timeout_seconds = atoi(val);
    // I don't check every possible env var - just the most important ones.
    // Users can still use the config file for other settings.
}