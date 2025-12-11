#ifndef THREAD_POOL_H
#define THREAD_POOL_H // I'm using include guards to prevent multiple inclusion.

#include <pthread.h> // I need pthread types for thread synchronization.

// This structure represents a local queue for a worker process.
// Each worker has its own queue that feeds its thread pool.
typedef struct local_queue {
    int *fds;              // I store client file descriptors in this array.
    int head;             // This is where I take connections from (consumer side).
    int tail;             // This is where I add new connections (producer side).
    int max_size;         // I need to know how many connections I can hold.
    int shutting_down;    // This flag tells threads when to stop.
    
    // I need synchronization primitives for my queue:
    pthread_mutex_t mutex; // I protect the queue data from concurrent access.
    pthread_cond_t cond;   // I signal waiting threads when work is available.
} local_queue_t;

// I need to initialize the local queue before using it.
int local_queue_init(local_queue_t *q, int max_size);

// I need to clean up the local queue when I'm done with it.
void local_queue_destroy(local_queue_t *q);

// This adds a client connection to the queue (producer operation).
int local_queue_enqueue(local_queue_t *q, int client_fd);

// This takes a client connection from the queue (consumer operation).
int local_queue_dequeue(local_queue_t *q);

// This is the main function for worker threads in the pool.
void *worker_thread(void *arg);

#endif 