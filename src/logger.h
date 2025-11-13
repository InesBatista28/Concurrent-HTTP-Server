// INTERFACE LOGGER


#ifndef LOGGER_H
#define LOGGER_H


#include <stdio.h>
#include <time.h>
#include "config.h"


//Log levels for different types of messages
typedef enum {
    LOG_DEBUG,      // Debug information
    LOG_INFO,       // General information
    LOG_WARNING,    // Warning messages
    LOG_ERROR,      // Error conditions
    LOG_ACCESS      // HTTP access logs
} log_level_t;


//Logger levels for different types of messages 
typedef struct {
    char log_file[1024];    // Log file path
    FILE *log_fp;           // Log file pointer
    int max_file_size;      // Maximum log file size in bytes
    int current_file_size;  // Current log file size
    int rotation_count;     // Number of log rotations
} logger_t;



//LOGGER API
// Initialize logger with configuration
int logger_init(const server_config_t *config);

// Close logger and free resources
void logger_close(void);

// Main logging function - thread safe
void logger_log(log_level_t level, const char *format, ...);

// Log HTTP access in Apache Combined Log Format
void logger_log_access(const char *client_ip, const char *method, const char *url, int status_code, size_t response_size, const char *referer, const char *user_agent);

// Rotate log file if it exceeds maximum size
void logger_rotate_if_needed(void);

// Get current timestamp for logging
void logger_get_timestamp(char *buffer, size_t buffer_size);

// Get log level as string
const char* logger_level_to_string(log_level_t level);




#endif