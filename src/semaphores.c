#include "semaphores.h"
#include <stdio.h>
#include <stdlib.h>

// * Global Synchronization Primitives
// These are the semaphores and mutex I use to coordinate between producers and consumers.
// I'm implementing the classic Producer-Consumer pattern for my worker thread pool.
sem_t empty_slots;      // I count how many empty slots are in the queue.
sem_t filled_slots;     // I count how many filled slots are in the queue.
pthread_mutex_t mutex;  // I protect the critical section where I modify the queue.

// I need to initialize all my synchronization primitives before using them.
// This function sets them up with the right starting values for my queue size.
void init_semaphores(int max_queue_size)
{
    // I initialize empty_slots to the full queue size because initially, all slots are empty.
    if (sem_init(&empty_slots, 1, max_queue_size) != 0 ||
        // I initialize filled_slots to 0 because there's nothing in the queue yet.
        sem_init(&filled_slots, 1, 0) != 0 ||
        // I initialize the mutex with default attributes.
        pthread_mutex_init(&mutex, NULL) != 0)
    {
        // If anything fails, I print an error and exit because I can't work without synchronization.
        perror("Semaphore or mutex init failed");
        exit(1);
    }
}