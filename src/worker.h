#ifndef WORKER_H
#define WORKER_H // I'm using include guards to prevent multiple inclusion.

#include <stddef.h> // I need size_t for buffer lengths.
#include <time.h>   // I need struct timespec for timing measurements.
#include <pthread.h> // I need pthread types for thread operations.

// This function calculates the time difference between two timestamps in milliseconds.
// I use it to measure how long it takes to handle each request.
long get_time_diff_ms(struct timespec start, struct timespec end);

// This function extracts the client's IP address from a socket file descriptor.
// I need this for logging who made each request.
void get_client_ip(int client_fd, char *ip_buffer, size_t buffer_len);

// This function determines the MIME type based on a file's extension.
// I use this to set the correct Content-Type header in HTTP responses.
const char *get_mime_type(const char *path);

// This is the main function that handles a client connection.
// It processes HTTP requests from start to finish.
void handle_client(int client_socket);

// This is the entry point for a worker process.
// The master process calls fork() and the child executes this function.
void start_worker_process(int ipc_socket);

#endif 