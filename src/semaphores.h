#ifndef SEMAPHORES_H
#define SEMAPHORES_H // I'm using include guards to prevent multiple inclusion.

#include <pthread.h>  // I need pthread_mutex_t for mutual exclusion.
#include <semaphore.h> // I need sem_t for counting semaphores.

// This function initializes all my synchronization primitives.
// I need to call this before starting any producer-consumer operations.
void init_semaphores(int max_queue_size);

#endif 