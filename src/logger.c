#include "logger.h"
#include "config.h" 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>

// I need to access the global server configuration to know where to write logs.
extern server_config_t config;

// * In-Memory Log Buffer
// I'm keeping logs in memory first to reduce disk writes.
// This improves performance because writing to disk is slow.
static char log_buffer[LOG_BUFFER_SIZE];
static size_t buffer_offset = 0;

// * Shutdown Flag
// I need a way to tell the logger thread when to stop.
// I use volatile and atomic operations so the main thread can signal shutdown safely.
static volatile int logger_shutting_down = 0;

// I need to rotate log files when they get too big.
// If a log file exceeds the maximum size, I rename it to ".old" and start fresh.
void check_and_rotate_log()
{
    struct stat st;
    // I check if the log file exists and how big it is.
    if (stat(config.log_file, &st) == 0)
    {
        if (st.st_size >= MAX_LOG_FILE_SIZE)
        {
            // The file is too big! I'll rename it to archive it.
            char old_log_name[512]; 
            
            snprintf(old_log_name, sizeof(old_log_name), "%s.old", config.log_file);
            rename(config.log_file, old_log_name);
            // Now the next write will create a new, empty log file.
        }
    }
}

// This is the internal, unsafe version of buffer flushing.
// I only call this from functions that already hold the semaphore lock.
void flush_buffer_to_disk_internal()
{
    if (buffer_offset == 0) return; // If there's nothing to write, I just return.

    check_and_rotate_log(); // Check if I need to rotate logs first.

    // I open the log file in append mode.
    FILE *fp = fopen(config.log_file, "a");
    if (fp)
    {
        // I write everything from the buffer to the file.
        fwrite(log_buffer, 1, buffer_offset, fp);
        fclose(fp);
    }

    // Now that I've written everything, I reset the buffer.
    buffer_offset = 0;
}

// This is the public, thread-safe version of buffer flushing.
// Any thread can call this safely because it uses the semaphore.
void flush_logger(sem_t *log_sem)
{
    sem_wait(log_sem); // I acquire the lock.
    flush_buffer_to_disk_internal(); // Do the actual writing.
    sem_post(log_sem); // I release the lock.
}

// This is the main logging function that other parts of the server call.
// It formats log entries in Apache Common Log Format.
void log_request(sem_t *log_sem, const char *client_ip, const char *method,
                 const char *path, int status, size_t bytes)
{
    // 1. First, I generate a timestamp for this request.
    time_t now = time(NULL);
    struct tm tm_info;
    localtime_r(&now, &tm_info); // I use localtime_r because it's thread-safe.
    
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%d/%b/%Y:%H:%M:%S %z", &tm_info);

    // 2. Now I format the log entry like Apache does.
    char entry[512];
    int len = snprintf(entry, sizeof(entry), "%s - - [%s] \"%s %s HTTP/1.1\" %d %zu\n",
                       client_ip, timestamp, method, path, status, bytes);

    if (len < 0) return; // If snprintf failed, I give up.

    // 3. This is the critical section - I need to add this entry to the buffer.
    sem_wait(log_sem); // I acquire the lock.

    // If the buffer is almost full, I flush it to disk first.
    if (buffer_offset + len >= LOG_BUFFER_SIZE)
    {
        flush_buffer_to_disk_internal();
    }

    // Now I can safely add the new entry to the buffer.
    memcpy(log_buffer + buffer_offset, entry, len);
    buffer_offset += len;

    sem_post(log_sem); // I release the lock.
}

// This is a compatibility wrapper - some parts of the code might call this.
void flush_buffer_to_disk(sem_t *log_sem)
{
    flush_logger(log_sem);
}

// This is the background thread that periodically flushes the buffer.
// Even if the buffer isn't full, I want to make sure logs get written to disk regularly.
void *logger_flush_thread(void *arg)
{
    sem_t *log_sem = (sem_t *)arg;

    // I keep running until someone tells me to stop.
    while (!__atomic_load_n(&logger_shutting_down, __ATOMIC_SEQ_CST))
    {
        // Instead of sleeping for 5 seconds straight, I sleep in 1-second chunks.
        // This way I can check the shutdown flag more often.
        for (int i = 0; i < 5; i++) {
             if (__atomic_load_n(&logger_shutting_down, __ATOMIC_SEQ_CST)) break;
             sleep(1);
        }

        // Time to flush the buffer to disk.
        flush_logger(log_sem);
    }

    // Before I exit, I make sure to write any remaining logs.
    flush_logger(log_sem);
    return NULL;
}

// This function is called when the server is shutting down.
// It tells the logger thread to stop running.
void logger_request_shutdown()
{
    __atomic_store_n(&logger_shutting_down, 1, __ATOMIC_SEQ_CST);
}