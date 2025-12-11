#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include "config.h"
#include "logger.h"
#include "shared_mem.h"
#include "worker.h"
#include "thread_pool.h"
#include "http.h"
#include "cache.h"

// I need to access the global server configuration and shared queue.
extern server_config_t config;
extern connection_queue_t *queue;

// This is where I initialize my local queue for this worker process.
// Each worker has its own queue that feeds its thread pool.
int local_queue_init(local_queue_t *q, int max_size)
{
    // I allocate memory for the file descriptor array.
    q->fds = malloc(sizeof(int) * max_size);
    if (!q->fds) return -1; // If allocation fails, I return an error.
    
    // I set up the circular buffer indices.
    q->head = 0;  // This is where I'll take connections from.
    q->tail = 0;  // This is where I'll add new connections.
    q->max_size = max_size; // I remember my capacity.
    q->shutting_down = 0; // I start with the queue active.
    
    // I need to initialize the mutex and condition variable for synchronization.
    if (pthread_mutex_init(&q->mutex, NULL) != 0) return -1;
    if (pthread_cond_init(&q->cond, NULL) != 0) return -1;
    
    return 0; // Success!
}

// When the worker shuts down, I need to clean up the local queue.
void local_queue_destroy(local_queue_t *q)
{
    if (!q) return; // If there's no queue, I have nothing to do.
    
    free(q->fds); // I free the array of file descriptors.
    pthread_mutex_destroy(&q->mutex); // I destroy the mutex.
    pthread_cond_destroy(&q->cond); // I destroy the condition variable.
}

// This function adds a client connection to the worker's local queue.
// I'm the producer (worker main thread adds, worker threads consume).
int local_queue_enqueue(local_queue_t *q, int client_fd)
{
    pthread_mutex_lock(&q->mutex); // I need exclusive access to modify the queue.
    
    // First, I check if the queue is full.
    int next = (q->tail + 1) % q->max_size;
    if (next == q->head) {
        // The queue is full! I can't accept this connection.
        pthread_mutex_unlock(&q->mutex);
        return -1; // I return -1 to signal "queue full".
    }
    
    // There's space, so I add the connection.
    q->fds[q->tail] = client_fd;
    q->tail = next; // I move the tail forward.
    
    // Now I signal any waiting worker threads that there's work to do.
    pthread_cond_signal(&q->cond);
    
    pthread_mutex_unlock(&q->mutex);
    return 0; // Success!
}

// This function takes a client connection from the worker's local queue.
// Worker threads call this to get work to do.
int local_queue_dequeue(local_queue_t *q)
{
    pthread_mutex_lock(&q->mutex);
    
    // If the queue is empty AND we're not shutting down, I wait.
    while (q->head == q->tail && !q->shutting_down) {
        pthread_cond_wait(&q->cond, &q->mutex); // I sleep until there's work.
    }
    
    // When I wake up, I check why.
    if (q->head == q->tail && q->shutting_down) {
        // The queue is empty AND we're shutting down.
        pthread_mutex_unlock(&q->mutex);
        return -1; // This signals the thread to exit.
    }
    
    // There's work to do! I take a connection from the head.
    int fd = q->fds[q->head];
    q->head = (q->head + 1) % q->max_size; // I move the head forward.
    
    pthread_mutex_unlock(&q->mutex);
    return fd; // Here's the connection to handle!
}

// This is the entry point for each worker thread in the pool.
// These threads continuously process client connections.
void *worker_thread(void *arg)
{
    local_queue_t *q = (local_queue_t *)arg; // I get my queue from the argument.
    
    while (1) // I keep running until told to stop.
    {
        // I wait for a connection to become available.
        int client_socket = local_queue_dequeue(q);
        
        if (client_socket < 0) {
            break; // A negative value means "shutdown", so I exit the loop.
        }

        // I have a connection! Now I handle the client request.
        handle_client(client_socket);
    }
    
    return NULL; // The thread exits cleanly.
}