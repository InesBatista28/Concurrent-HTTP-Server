#include "shared_mem.h"
#include "config.h"
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

// I need to access the global server configuration to know my reporting interval.
extern server_config_t config;

// This is my statistics monitor thread.
// It runs in the background and periodically shows how the server is performing.
void *stats_monitor_thread(void *arg) {
    (void)arg; // I don't use this parameter, but I need to declare it for pthread compatibility.
    
    // I run forever in a loop, showing statistics at regular intervals.
    while (1) {
        // I wait for the configured interval before showing stats again.
        // The timeout_seconds setting tells me how often to report.
        sleep(config.timeout_seconds);

        // Now I need to read the shared statistics safely.
        // I acquire the mutex so workers don't modify the stats while I'm reading them.
        sem_wait(&stats->mutex);
        
        // I need to calculate the average response time.
        // I have to be careful not to divide by zero.
        double avg_time = 0.0;
        if (stats->total_requests > 0) {
            // I calculate the average by dividing total time by number of requests.
            avg_time = (double)stats->average_response_time / stats->total_requests;
        }

        // Now I print a nice dashboard of server statistics.
        printf("\n SERVER STATISTICS \n");
        printf("Active Connections: %d\n", stats->active_connections);
        printf("Total Requests:     %ld\n", stats->total_requests);
        printf("Bytes Transferred:  %ld\n", stats->bytes_transferred);
        printf("Avg Response Time:  %.2f ms\n", avg_time);
        printf("Status 200 (OK):    %ld\n", stats->status_200);
        printf("Status 404 (NF):    %ld\n", stats->status_404);
        printf("Status 500 (Err):   %ld\n", stats->status_500);

        // I'm done reading, so I release the mutex.
        // Now workers can update the statistics again.
        sem_post(&stats->mutex);
    }
    return NULL; // I never actually return, but I need to declare a return value.
}