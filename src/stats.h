#ifndef STATS_H
#define STATS_H // I'm using include guards to prevent multiple inclusion.

// This function runs in a background thread and monitors server statistics.
// I declare it here so other files can create this monitoring thread.
void *stats_monitor_thread(void *arg);

#endif 