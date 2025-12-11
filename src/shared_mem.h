#ifndef SHARED_MEM_H
#define SHARED_MEM_H // I'm using include guards to prevent multiple inclusion.

#include <semaphore.h> // I need semaphores for synchronization.
#include <pthread.h>   // I need pthread_mutex_t for mutual exclusion.

// This structure represents my shared connection queue.
// It's a circular buffer that lives in shared memory so all processes can access it.
typedef struct
{
    int *connections;        // I store client file descriptors in this array.
    int head;               // This is where I take connections from (consumer side).
    int tail;               // This is where I add new connections (producer side).
    int max_size;           // I need to know how many connections I can hold.

    // These are my synchronization primitives:
    sem_t empty_slots;      // I count how many empty slots are available.
    sem_t filled_slots;     // I count how many slots have connections waiting.
    pthread_mutex_t mutex;  // I protect the head/tail indices from concurrent access.
    sem_t log_mutex;        // I need a separate lock for logging operations.
    int shutting_down;      // This flag tells workers when it's time to stop.
} connection_queue_t;

// This structure holds server statistics that all workers update.
// I keep these in shared memory so I can monitor server performance.
typedef struct
{
    long total_requests;           // I count all HTTP requests processed.
    long bytes_transferred;        // I track total data sent to clients.
    long status_200;               // I count successful responses.
    long status_404;               // I count "Not Found" responses.
    long status_500;               // I count server error responses.
    int active_connections;       // I track how many clients are connected right now.
    int average_response_time;    // I could calculate average response time here.
    sem_t mutex;                  // I need a lock to protect these counters.
} server_stats_t;

// I'm declaring these as extern so other files can access them.
// They're defined in shared_mem.c and point to the shared memory regions.
extern connection_queue_t *queue;
extern server_stats_t *stats;

// These are my function declarations:

// I need to initialize the shared queue before using it.
void init_shared_queue(int max_queue_size);

// I need to initialize the shared statistics structure.
void init_shared_stats();

// I use this to add a client connection to the queue.
int enqueue(int client_socket);

// I use this to take a client connection from the queue.
int dequeue();

#endif 