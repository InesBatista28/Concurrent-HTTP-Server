#include "shared_mem.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>        
#include <fcntl.h>           

// * Global pointers to shared memory regions
// These pointers will be accessible from all processes (master and workers).
connection_queue_t *queue = NULL; // I store client connections here.
server_stats_t *stats = NULL;     // I keep server statistics here.

// I need to initialize the shared connection queue.
// This creates a circular buffer in shared memory that all processes can access.
void init_shared_queue(int max_queue_size)
{
    // First, I calculate how much memory I need.
    // I need space for the queue structure PLUS space for the actual connections array.
    size_t queue_data_size = sizeof(int) * max_queue_size;
    size_t total_size = sizeof(connection_queue_t) + queue_data_size;

    // I use mmap with MAP_ANONYMOUS to allocate shared memory that isn't backed by a file.
    void *mem_block = mmap(NULL, total_size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);

    if (mem_block == MAP_FAILED) {
        perror("mmap failed");
        exit(1); // If I can't allocate shared memory, I can't continue.
    }

    // Now I set up my queue structure.
    queue = (connection_queue_t *)mem_block;
    // The connections array starts right after the queue structure in memory.
    queue->connections = (int *)(queue + 1);

    // I initialize the circular buffer indices.
    queue->head = 0;  // This is where I'll take connections from.
    queue->tail = 0;  // This is where I'll add new connections.
    queue->max_size = max_queue_size; // I remember my capacity.
    queue->shutting_down = 0; // I start with the queue active.
    
    // Now I need to initialize a mutex that works across processes.
    pthread_mutexattr_t mutex_attr;
    pthread_mutexattr_init(&mutex_attr);
    pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED); // This is key!
    if (pthread_mutex_init(&queue->mutex, &mutex_attr) != 0) {
        perror("mutex init");
        exit(1);
    }
    pthread_mutexattr_destroy(&mutex_attr);

    // I also need a semaphore for logging synchronization.
    if (sem_init(&queue->log_mutex, 1, 1) != 0) {
        perror("sem init log_mutex");
        exit(1);
    }
    
    // I set up the producer-consumer semaphores.
    // empty_slots starts at max_size (all slots are empty).
    // filled_slots starts at 0 (no connections yet).
    if (sem_init(&queue->empty_slots, 1, max_queue_size) != 0 ||
        sem_init(&queue->filled_slots, 1, 0) != 0) {
        perror("sem init");
        exit(1);
    }
}

// I also need shared memory for server statistics.
// All workers will update these stats, and the master can read them.
void init_shared_stats()
{
    // I allocate shared memory for the stats structure.
    void *mem_block = mmap(NULL, sizeof(server_stats_t), 
                           PROT_READ | PROT_WRITE, 
                           MAP_SHARED | MAP_ANONYMOUS, -1, 0);

    if (mem_block == MAP_FAILED) {
        perror("mmap stats failed");
        exit(1);
    }

    stats = (server_stats_t *)mem_block;

    // I initialize all counters to zero.
    stats->total_requests = 0;
    stats->bytes_transferred = 0;
    stats->status_200 = 0;
    stats->status_404 = 0;
    stats->status_500 = 0;
    stats->active_connections = 0;
    stats->average_response_time = 0;

    // I need a semaphore to protect these stats from concurrent updates.
    if (sem_init(&stats->mutex, 1, 1) != 0) {
        perror("sem init stats");
        exit(1);
    }
}

// This function adds a client connection to the shared queue.
// I'm the producer in the producer-consumer pattern.
int enqueue(int client_socket) {
    // First, I check if we're shutting down.
    if (queue->shutting_down) {
        return -1; // If we are, I reject new connections.
    }

    // I try to get an empty slot without blocking.
    // If the queue is full, I return immediately.
    if (sem_trywait(&queue->empty_slots) != 0) {
        if (errno == EAGAIN) {
            return -1; // Queue is full.
        }
        perror("sem_trywait");
        return -1; 
    }

    // Now I have permission to add to the queue.
    pthread_mutex_lock(&queue->mutex);

    // I add the client socket to the tail position.
    queue->connections[queue->tail] = client_socket;
    // I move the tail forward, wrapping around if needed.
    queue->tail = (queue->tail + 1) % queue->max_size;

    pthread_mutex_unlock(&queue->mutex);
    
    // I signal that there's now a new item in the queue.
    sem_post(&queue->filled_slots);
    
    return 0; // Success!
}

// This function takes a client connection from the shared queue.
// I'm the consumer in the producer-consumer pattern.
int dequeue() {
    // I wait for a filled slot. This will block if the queue is empty.
    sem_wait(&queue->filled_slots);
    
    // Now I can safely modify the queue.
    pthread_mutex_lock(&queue->mutex);

    // I check if I woke up because we're shutting down.
    if (queue->shutting_down && queue->head == queue->tail) {
        pthread_mutex_unlock(&queue->mutex);
        return -1; // Queue is empty and we're shutting down.
    }

    // I take the connection from the head position.
    int client_socket = queue->connections[queue->head];
    // I move the head forward, wrapping around if needed.
    queue->head = (queue->head + 1) % queue->max_size;

    pthread_mutex_unlock(&queue->mutex);
    
    // I signal that there's now an empty slot available.
    sem_post(&queue->empty_slots);
    
    return client_socket; // Here's the connection!
}