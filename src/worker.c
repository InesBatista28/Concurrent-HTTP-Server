#define _POSIX_C_SOURCE 199309L // I need this for clock_gettime and other POSIX features.

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h> 
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <pthread.h>
#include <time.h> 
#include <sys/time.h> 

#include "http.h"
#include "config.h"
#include "shared_mem.h"
#include "thread_pool.h"
#include <sys/uio.h>
#include "logger.h"
#include "worker.h"
#include "cache.h"

// I need to access the global server configuration and shared queue.
extern server_config_t config;
extern connection_queue_t *queue;

// This helper calculates the time difference between two timestamps in milliseconds.
// I use this to measure how long it takes to handle each request.
long get_time_diff_ms(struct timespec start, struct timespec end) {
    return (end.tv_sec - start.tv_sec) * 1000 + (end.tv_nsec - start.tv_nsec) / 1000000;
}

// This function extracts the client's IP address from the socket.
// I need this for logging purposes.
void get_client_ip(int client_fd, char *ip_buffer, size_t buffer_len) {
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    if (getpeername(client_fd, (struct sockaddr *)&addr, &addr_len) == 0) {
        inet_ntop(AF_INET, &addr.sin_addr, ip_buffer, buffer_len);
    } else {
        strncpy(ip_buffer, "unknown", buffer_len);
    }
}

// I need to determine the MIME type based on file extension.
// This helps browsers understand what kind of file I'm sending.
const char *get_mime_type(const char *path)
{
    const char *ext = strrchr(path, '.'); // I find the last dot in the path.
    if (!ext)
        return "application/octet-stream"; // Default for unknown types.
    
    // I compare extensions to known MIME types.
    if (strcmp(ext, ".html") == 0)
        return "text/html";
    if (strcmp(ext, ".css") == 0)
        return "text/css";
    if (strcmp(ext, ".js") == 0)
        return "application/javascript";
    if (strcmp(ext, ".png") == 0)
        return "image/png";
    if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0)
        return "image/jpeg";
    if (strcmp(ext, ".pdf") == 0)
        return "application/pdf";
    
    return "application/octet-stream"; // Fallback for other types.
}

// This helper function sends custom error pages.
// If there's an error HTML file in www/errors/, I send that.
// Otherwise, I send a simple hardcoded error message.
static void send_error_page(int client_fd, int status_code, const char *status_text, long *bytes_sent)
{
    char filepath[1024];
    snprintf(filepath, sizeof(filepath), "%s/errors/%d.html", config.document_root, status_code);

    struct stat st;
    if (stat(filepath, &st) == 0) {
        // I found a custom error page!
        FILE *fp = fopen(filepath, "rb");
        if (fp) {
            char *buf = malloc(st.st_size);
            if (buf) {
                size_t rb = fread(buf, 1, st.st_size, fp);
                if (rb == (size_t)st.st_size) {
                    send_http_response(client_fd, status_code, status_text, "text/html", buf, st.st_size);
                    *bytes_sent = st.st_size;
                    free(buf);
                    fclose(fp);
                    return;
                }
                free(buf);
            }
            fclose(fp);
        }
    }

    // Fallback: I send a simple HTML error message.
    char body[512];
    snprintf(body, sizeof(body), "<h1>%d %s</h1>", status_code, status_text);
    size_t len = strlen(body);
    send_http_response(client_fd, status_code, status_text, "text/html", body, len);
    *bytes_sent = len;
}

// This is the main function that handles each client connection.
// It processes HTTP requests from start to finish.
void handle_client(int client_socket)
{
    struct timespec start_time, end_time;
    
    // 1. I increment the active connections counter.
    // This needs to be thread-safe, so I use a semaphore.
    sem_wait(&stats->mutex);
    stats->active_connections++;
    sem_post(&stats->mutex);

    char client_ip[INET_ADDRSTRLEN];
    get_client_ip(client_socket, client_ip, sizeof(client_ip));

    // I set a timeout on the socket for keep-alive connections.
    struct timeval tv;
    tv.tv_sec = config.keep_alive_timeout > 0 ? config.keep_alive_timeout : 5;
    tv.tv_usec = 0;
    setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);

    // I can handle multiple requests on the same connection (keep-alive).
    while (1) {
        clock_gettime(CLOCK_MONOTONIC, &start_time);

        // I read the request from the client.
        char buffer[2048];
        ssize_t bytes = recv(client_socket, buffer, sizeof(buffer) - 1, 0);

        int status_code = 0;
        long bytes_sent = 0;
        http_request_t req = {0}; 

        if (bytes <= 0)
        {
            // Connection closed or timeout - I break out of the loop.
            break; 
        }
        buffer[bytes] = '\0';

    if (parse_http_request(buffer, &req) != 0)
    {
        status_code = 400;
        send_error_page(client_socket, 400, "Bad Request", &bytes_sent);
        status_code = 400;
        goto update_stats_and_log; 
    }

    // I only support GET and HEAD methods.
    int is_head = (strcmp(req.method, "HEAD") == 0);
    if (strcmp(req.method, "GET") != 0 && strcmp(req.method, "HEAD") != 0)
    {
        status_code = 405;
        send_error_page(client_socket, 405, "Method Not Allowed", &bytes_sent);
        status_code = 405;
        goto update_stats_and_log;
    }

    // Security check: I prevent directory traversal attacks.
    if (strstr(req.path, ".."))
    {
        status_code = 403;
        send_error_page(client_socket, 403, "Forbidden", &bytes_sent);
        status_code = 403;
        goto update_stats_and_log;
    }

    // Special endpoint: /stats returns server statistics as JSON.
    if (strcmp(req.path, "/stats") == 0)
    {
        sem_wait(&stats->mutex);
        char json_body[1024];
        snprintf(json_body, sizeof(json_body),
            "{"
            "\"active_connections\": %d,"
            "\"total_requests\": %ld,"
            "\"bytes_transferred\": %ld,"
            "\"status_200\": %ld,"
            "\"status_404\": %ld,"
            "\"status_500\": %ld,"
            "\"avg_response_time_ms\": %ld"
            "}",
            stats->active_connections,
            stats->total_requests,
            stats->bytes_transferred,
            stats->status_200,
            stats->status_404,
            stats->status_500,
            (stats->total_requests > 0) ? (stats->average_response_time / stats->total_requests) : 0
        );
        sem_post(&stats->mutex);

        size_t len = strlen(json_body);
        send_http_response(client_socket, 200, "OK", "application/json", json_body, len);
        bytes_sent = len;
        status_code = 200;
        goto update_stats_and_log;
    }

    // I check for HTTP Range requests (for partial file downloads).
    long range_start = -1;
    long range_end = -1;
    char *range_header = strstr(buffer, "Range: bytes=");
    if (range_header) {
        range_header += 13; // Skip "Range: bytes="
        char *dash = strchr(range_header, '-');
        if (dash) {
            *dash = '\0';
            range_start = atol(range_header);
            if (*(dash + 1) != '\r' && *(dash + 1) != '\n') {
                range_end = atol(dash + 1);
            }
            *dash = '-'; // Restore the buffer
        }
    }

    // I handle virtual hosts: check if there's a directory matching the Host header.
    char full_path[2048];
    char vhost_path[1024];
    int vhost_found = 0;

    // Parse the Host header from the request.
    char *host_header = strstr(buffer, "Host: ");
    if (host_header) {
        host_header += 6; // Skip "Host: "
        char *end = strchr(host_header, '\r');
        if (!end) end = strchr(host_header, '\n');
        if (end) {
            char host[256];
            size_t len = end - host_header;
            if (len > 255) len = 255;
            strncpy(host, host_header, len);
            host[len] = '\0';
            
            // Remove port if present
            char *colon = strchr(host, ':');
            if (colon) *colon = '\0';

            // Check if directory exists: www/host
            snprintf(vhost_path, sizeof(vhost_path), "%s/%s", config.document_root, host);
            struct stat st_vhost;
            if (stat(vhost_path, &st_vhost) == 0 && S_ISDIR(st_vhost.st_mode)) {
                snprintf(full_path, sizeof(full_path), "%s%s", vhost_path, req.path);
                vhost_found = 1;
            }
        }
    }

    if (!vhost_found) {
        snprintf(full_path, sizeof(full_path), "%s%s", config.document_root, req.path);
    }

    // If the path is a directory, I serve index.html.
    struct stat st;
    if (stat(full_path, &st) == 0 && S_ISDIR(st.st_mode))
    {
        strncat(full_path, "/index.html", sizeof(full_path) - strlen(full_path) - 1);
    }

    // Check if the file exists.
    if (stat(full_path, &st) != 0) {
        status_code = 404;
        send_error_page(client_socket, 404, "Not Found", &bytes_sent);
        goto update_stats_and_log;
    }

    long fsize = st.st_size;
    char *content = NULL;
    size_t read_bytes = 0;

    // * CACHING LOGIC
    // I only cache files smaller than 1MB to save memory.
    if (fsize > 0 && fsize < (1 * 1024 * 1024)) {
        // First, I try to get the file from cache.
        if (cache_get(full_path, &content, &read_bytes) == 0) {
            // Cache HIT! 'content' now has a copy of the cached data.
        } else {
            // Cache MISS: I need to read from disk.
            FILE *fp = fopen(full_path, "rb");
            if (!fp) {
                status_code = 404;
                send_error_page(client_socket, 404, "Not Found", &bytes_sent);
                goto update_stats_and_log;
            }
            char *buf = malloc(fsize);
            if (!buf) {
                fclose(fp);
                status_code = 500;
                send_error_page(client_socket, 500, "Internal Server Error", &bytes_sent);
                goto update_stats_and_log;
            }
            size_t rb = fread(buf, 1, fsize, fp);
            fclose(fp);
            
            if (rb != (size_t)fsize) {
                free(buf);
                status_code = 500;
                send_error_page(client_socket, 500, "Internal Server Error", &bytes_sent);
                goto update_stats_and_log;
            }
            read_bytes = rb;
            content = buf;

            // I update the cache for next time (best effort).
            cache_put(full_path, content, read_bytes);
        }
    } else {
        // Large files: I read directly from disk without caching.
        FILE *fp = fopen(full_path, "rb");
        if (!fp) {
            status_code = 404;
            send_error_page(client_socket, 404, "Not Found", &bytes_sent);
            goto update_stats_and_log;
        }
        char *buf = malloc(fsize);
        if (!buf) {
            fclose(fp);
            status_code = 500;
            send_error_page(client_socket, 500, "Internal Server Error", &bytes_sent);
            goto update_stats_and_log;
        }
        size_t rb = fread(buf, 1, fsize, fp);
        fclose(fp);
        if (rb != (size_t)fsize) {
            free(buf);
            status_code = 500;
            // ... send 500 ...
            close(client_socket);
            goto update_stats_and_log;
        }
        content = buf;
        read_bytes = rb;
    }

    // Determine the MIME type for the response header.
    const char *mime = get_mime_type(full_path);
    
    // Handle Range requests (partial content).
    if (range_start != -1) {
        // Partial Content response (206)
        if (range_end == -1 || range_end >= fsize) range_end = fsize - 1;
        long content_length = range_end - range_start + 1;
        
        status_code = 206;
        
        // Construct Content-Range header
        char extra_headers[128];
        snprintf(extra_headers, sizeof(extra_headers), "Content-Range: bytes %ld-%ld/%ld\r\n", range_start, range_end, fsize);
        
        // Send the header
        char header[1024];
        snprintf(header, sizeof(header), 
            "HTTP/1.1 206 Partial Content\r\n"
            "Content-Type: %s\r\n"
            "Content-Length: %ld\r\n"
            "%s"
            "Connection: keep-alive\r\n"
            "\r\n", mime, content_length, extra_headers);
        send(client_socket, header, strlen(header), 0);
        
        // Send the body (or just header for HEAD requests)
        if (!is_head) {
            if (content) {
                // From cache or full read
                send(client_socket, content + range_start, content_length, 0);
            } else {
                // For large files, I need to read just the requested range
                FILE *fp = fopen(full_path, "rb");
                 if (fp) {
                     fseek(fp, range_start, SEEK_SET);
                     char *chunk = malloc(content_length);
                     if (chunk) {
                         size_t rb = fread(chunk, 1, content_length, fp);
                         if (rb > 0) {
                            send(client_socket, chunk, rb, 0);
                         }
                         free(chunk);
                     }
                     fclose(fp);
                 }
            }
        }
        bytes_sent = content_length;
    }
    else {
        // Normal 200 OK response
        status_code = 200;
        if (is_head) // HEAD request: send headers only
        {
            send_http_response(client_socket, 200, "OK", mime, NULL, fsize);
            bytes_sent = 0;
        }
        else // GET request: send headers and body
        {
            send_http_response(client_socket, 200, "OK", mime, content, fsize);
            bytes_sent = fsize;
        }
    }

    free(content);

// * Cleanup Label: I use goto to handle errors and normal completion in one place.
update_stats_and_log:
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    long elapsed_ms = get_time_diff_ms(start_time, end_time);

    // Update shared statistics (thread-safe)
    sem_wait(&stats->mutex);
    stats->total_requests++;
    stats->bytes_transferred += bytes_sent;
    stats->average_response_time += elapsed_ms;

    if (status_code == 200) stats->status_200++;
    else if (status_code == 404) stats->status_404++;
    else if (status_code == 500) stats->status_500++;
    
    sem_post(&stats->mutex);

    // Log the request in Apache format
    const char *log_method = (req.method[0] != '\0') ? req.method : "-";
    const char *log_path = (req.path[0] != '\0') ? req.path : "-";
    
    log_request(&queue->log_mutex, client_ip, log_method, log_path, status_code, bytes_sent);

    } // End of while(1) keep-alive loop

    // Connection is closing, so I clean up.
    close(client_socket);
    sem_wait(&stats->mutex);
    stats->active_connections--; // Decrement active connections
    sem_post(&stats->mutex);
}

// This function receives a file descriptor from another process via UNIX socket.
// It's the counterpart to send_fd() in master.c.
static int recv_fd(int socket)
{
    struct msghdr msg = {0};

    // I need to receive at least one byte of data.
    char buf[1] = {0};
    struct iovec io = {.iov_base = buf, .iov_len = 1};

    // I use a union for proper alignment of the control buffer.
    union
    {
        char buf[CMSG_SPACE(sizeof(int))];
        struct cmsghdr align;
    } u;

    memset(&u, 0, sizeof(u));

    msg.msg_iov = &io;
    msg.msg_iovlen = 1;
    msg.msg_control = u.buf;
    msg.msg_controllen = sizeof(u.buf);

    // Perform the receive operation
    if (recvmsg(socket, &msg, 0) < 0)
        return -1;

    // Extract the file descriptor from the control message
    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
    
    // Verify it's the right type of message
    if (cmsg && cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_RIGHTS)
    {
        return *((int *)CMSG_DATA(cmsg)); // Here's the file descriptor!
    }
    
    return -1; // Failed to receive a valid FD
}

// This is the main entry point for a worker process.
// The master process calls fork() and then the child executes this function.
void start_worker_process(int ipc_socket)
{
    printf("Worker (PID: %d) started\n", getpid());

    // Initialize time zone for proper logging timestamps
    tzset();
    
    // Initialize shared queue structures
    init_shared_queue(config.max_queue_size);

    // Start the logger flush thread
    pthread_t flush_tid;
    if (pthread_create(&flush_tid, NULL, logger_flush_thread, (void *)&queue->log_mutex) != 0) {
        perror("Failed to create logger flush thread");
    }

    // Initialize the local queue for this worker's thread pool
    local_queue_t local_q;
    if (local_queue_init(&local_q, config.max_queue_size) != 0) {
        perror("local_queue_init");
    }
    
    // Initialize the file cache
    size_t cache_bytes = (size_t)config.cache_size_mb * 1024 * 1024;
    if (cache_init(cache_bytes) != 0) {
        perror("cache_init");
    }

    // Create the thread pool
    int thread_count = config.threads_per_worker > 0 ? config.threads_per_worker : 0;
    pthread_t *threads = NULL;
    if (thread_count > 0) {
        threads = malloc(sizeof(pthread_t) * thread_count);
        if (!threads) {
            perror("Failed to allocate worker threads array");
            thread_count = 0;
        }
    }

    int created = 0;
    for (int i = 0; i < thread_count; i++) {
        if (pthread_create(&threads[i], NULL, worker_thread, &local_q) != 0) {
            perror("pthread_create");
            break;
        }
        created++;
    }

    // * Main Loop: Receive and dispatch connections from master
    while (1)
    {
        int client_fd = recv_fd(ipc_socket);
        if (client_fd < 0) {
            // IPC socket closed or error - time to shut down
            break;
        }

        // Try to add the client to the local queue
        if (local_queue_enqueue(&local_q, client_fd) != 0) {
            fprintf(stderr, "[Worker %d] Queue full! Rejecting client.\n", getpid());
            
            long bytes_sent = 0;
            send_error_page(client_fd, 503, "Service Unavailable", &bytes_sent);

            close(client_fd);
        }
    }

    // * === Graceful Shutdown Sequence === 
    
    // 1. Signal worker threads to stop
    pthread_mutex_lock(&local_q.mutex); 
    local_q.shutting_down = 1;
    pthread_cond_broadcast(&local_q.cond);
    pthread_mutex_unlock(&local_q.mutex);

    // 2. Stop logger thread
    logger_request_shutdown();
    pthread_join(flush_tid, NULL);

    // 3. Join worker threads
    for (int i = 0; i < created; i++) {
        pthread_join(threads[i], NULL);
    }

    // 4. Cleanup resources
    if (threads) free(threads);
    local_queue_destroy(&local_q);
    cache_destroy();
    
    close(ipc_socket);
}