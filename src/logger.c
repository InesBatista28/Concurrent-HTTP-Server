//LOGGER COM VERIFICAÇÃO snprintf

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <pthread.h>
#include <errno.h>
#include "logger.h"
#include "config.h"

// Global logger instance
static logger_t logger;
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

// Initialize logger with configuration
int logger_init(const server_config_t *config) {
    if (!config) {
        return -1;
    }

    // Initialize logger structure
    strncpy(logger.log_file, config_get_log_file(config), sizeof(logger.log_file) - 1);
    logger.log_file[sizeof(logger.log_file) - 1] = '\0';
    logger.max_file_size = 10 * 1024 * 1024; // 10MB default
    logger.current_file_size = 0;
    logger.rotation_count = 0;

    // Open log file in append mode
    logger.log_fp = fopen(logger.log_file, "a");
    if (!logger.log_fp) {
        perror("Failed to open log file");
        return -1;
    }

    // Get current file size
    struct stat st;
    if (stat(logger.log_file, &st) == 0) {
        logger.current_file_size = st.st_size;
    }

    // Log startup message
    logger_log(LOG_INFO, "Logger initialized - Log file: %s", logger.log_file);
    
    return 0;
}

// Close logger and free resources
void logger_close(void) {
    // Log final message before closing
    logger_log(LOG_INFO, "Logger shutting down");
    
    pthread_mutex_lock(&log_mutex);
    
    if (logger.log_fp) {
        fflush(logger.log_fp);  // Force flush any buffered data
        fclose(logger.log_fp);
        logger.log_fp = NULL;
    }
    
    pthread_mutex_unlock(&log_mutex);
    
    // Small delay to ensure all threads finish
    usleep(100000); // 100ms delay
}

// Get current timestamp for logging
void logger_get_timestamp(char *buffer, size_t buffer_size) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(buffer, buffer_size, "%d/%b/%Y:%H:%M:%S %z", tm_info);
}

// Get log level as string
const char* logger_level_to_string(log_level_t level) {
    switch (level) {
        case LOG_DEBUG:   return "DEBUG";
        case LOG_INFO:    return "INFO";
        case LOG_WARNING: return "WARNING";
        case LOG_ERROR:   return "ERROR";
        case LOG_ACCESS:  return "ACCESS";
        default:          return "UNKNOWN";
    }
}

// Rotate log file if it exceeds maximum size
void logger_rotate_if_needed(void) {
    if (logger.current_file_size < logger.max_file_size) {
        return;
    }

    // Close current log file
    if (logger.log_fp) {
        fclose(logger.log_fp);
        logger.log_fp = NULL;
    }

    // Create backup filename with safe snprintf
    char backup_file[1024];
    int written = snprintf(backup_file, sizeof(backup_file), "%s.%d", 
                          logger.log_file, logger.rotation_count);
    
    // Check if snprintf was successful
    if (written < 0 || written >= (int)sizeof(backup_file)) {
        // Fallback: use a simple backup name
        strncpy(backup_file, "access.log.backup", sizeof(backup_file) - 1);
        backup_file[sizeof(backup_file) - 1] = '\0';
    }

    // Rename current log file to backup
    if (rename(logger.log_file, backup_file) != 0) {
        fprintf(stderr, "Failed to rotate log file: %s\n", strerror(errno));
        // Continue anyway - we'll try to create a new file
    }

    // Open new log file
    logger.log_fp = fopen(logger.log_file, "a");
    if (!logger.log_fp) {
        fprintf(stderr, "Failed to create new log file after rotation: %s\n", strerror(errno));
        return;
    }

    logger.current_file_size = 0;
    logger.rotation_count++;
    logger_log(LOG_INFO, "Log file rotated to %s", backup_file);
}

// Main logging function - thread safe
void logger_log(log_level_t level, const char *format, ...) {
    pthread_mutex_lock(&log_mutex);

    if (!logger.log_fp) {
        pthread_mutex_unlock(&log_mutex);
        return;
    }

    // Rotate log if needed
    logger_rotate_if_needed();

    // Get timestamp
    char timestamp[64];
    logger_get_timestamp(timestamp, sizeof(timestamp));

    // Format log message
    va_list args;
    
    // Write to log file
    fprintf(logger.log_fp, "[%s] [%s] ", timestamp, logger_level_to_string(level));
    va_start(args, format);
    vfprintf(logger.log_fp, format, args);
    va_end(args);
    fprintf(logger.log_fp, "\n");
    fflush(logger.log_fp);

    // Also print to console for debug
    printf("[%s] [%s] ", timestamp, logger_level_to_string(level));
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");

    // Update file size (estimate)
    logger.current_file_size += 100; // Rough estimate

    pthread_mutex_unlock(&log_mutex);
}

// Log HTTP access in Apache Combined Log Format
void logger_log_access(const char *client_ip, const char *method, 
                       const char *url, int status_code, 
                       size_t response_size, const char *referer, 
                       const char *user_agent) {
    
    pthread_mutex_lock(&log_mutex);

    if (!logger.log_fp) {
        pthread_mutex_unlock(&log_mutex);
        return;
    }

    // Rotate log if needed
    logger_rotate_if_needed();

    // Get timestamp
    char timestamp[64];
    logger_get_timestamp(timestamp, sizeof(timestamp));

    // Default values for missing fields
    const char *safe_referer = referer ? referer : "-";
    const char *safe_user_agent = user_agent ? user_agent : "-";

    // Log in Apache Combined Log Format
    fprintf(logger.log_fp, "%s - - [%s] \"%s %s HTTP/1.1\" %d %zu \"%s\" \"%s\"\n",
            client_ip, timestamp, method, url, status_code, 
            response_size, safe_referer, safe_user_agent);
    fflush(logger.log_fp);

    // Also print to console
    printf("%s - - [%s] \"%s %s HTTP/1.1\" %d %zu \"%s\" \"%s\"\n",
           client_ip, timestamp, method, url, status_code, 
           response_size, safe_referer, safe_user_agent);

    // Update file size (estimate)
    logger.current_file_size += 200; // Rough estimate

    pthread_mutex_unlock(&log_mutex);
}