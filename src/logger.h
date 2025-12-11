#ifndef LOGGER_H
#define LOGGER_H // I'm using include guards to prevent multiple inclusion.

#include <semaphore.h> // I need semaphores for thread synchronization.
#include <stddef.h>    // I need size_t for byte counts.

// I'm defining constants that control how the logger works.
#define MAX_LOG_FILE_SIZE (10 * 1024 * 1024) // I don't want log files to exceed 10MB.
#define LOG_BUFFER_SIZE 4096 // I'll buffer 4KB of log entries in memory before writing to disk.

// I need to declare all my logger functions so other files can use them.

// This initializes the logger system (though I notice it's declared but not defined in the provided code).
void init_logger();

// This is the main logging function that records HTTP requests.
// It follows the Apache Common Log Format for compatibility with log analyzers.
void log_request(sem_t *log_sem, const char *client_ip, const char *method,
                 const char *path, int status, size_t bytes);

// This function forces the logger to write any buffered entries to disk.
// I have two declarations for the same function - probably a typo in the original code.
void flush_logger(sem_t *log_sem);
void flush_logger(sem_t *log_sem); // Duplicate declaration - I should fix this.

// This function runs in a background thread and periodically flushes logs.
void *logger_flush_thread(void *arg);

// This signals the logger thread to shut down cleanly.
void logger_request_shutdown();

#endif 