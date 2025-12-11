#define _XOPEN_SOURCE 700 // I need this for some advanced POSIX features.

#include "master.h"    
#include "shared_mem.h" 
#include "config.h"     
#include <sys/uio.h>    
#include <string.h>     
#include "worker.h"     
#include "stats.h"      
#include "thread_pool.h" 
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <unistd.h>     
#include <stdio.h>      
#include <stdlib.h>     
#include <sys/wait.h>   
#include <pthread.h>    
#include <signal.h>     
#include <errno.h>      

// I'm grabbing the global config that was loaded in main.c.
extern server_config_t config;

// This little flag controls my main loop.
// If it's 1, I keep running. If it's 0, I start packing up.
// I make it volatile because it can change at any time from a signal handler.
static volatile sig_atomic_t server_running = 1;

// This is my signal handler. When you press Ctrl+C, the OS calls this.
// I just flip the switch to stop the server.
void handle_sigint(int sig) {
    (void)sig; // I don't care about the signal number, I just know it's time to stop.
    server_running = 0; 
}

// I'm sending a file descriptor to another process.
// You see, file descriptors are just numbers local to my process.
// If I just send the number "5", it means nothing to the worker.
// So I use a special UNIX socket message (SCM_RIGHTS) to tell the kernel:
// "Hey, please copy this file descriptor into the worker's process table!"
static int send_fd(int socket, int fd_to_send)
{
    struct msghdr msg = {0};

    // I need to send at least one byte of real data for this to work.
    // So I'll just send a dummy '0'.
    char buf[1] = {0}; 
    struct iovec io = {.iov_base = buf, .iov_len = 1};

    // I need a buffer for the control message (the FD).
    // I use a union to make sure it's properly aligned in memory.
    union
    {
        char buf[CMSG_SPACE(sizeof(int))]; // Enough space for one integer.
        struct cmsghdr align;              // Force alignment.
    } u;
    
    // I'll clear the buffer to avoid sending garbage.
    memset(&u, 0, sizeof(u)); 

    // Now I set up the message header.
    msg.msg_iov = &io;          // Here's my dummy data.
    msg.msg_iovlen = 1;         // Just one chunk.
    msg.msg_control = u.buf;    // Here's my control buffer.
    msg.msg_controllen = sizeof(u.buf); // This is how big it is.

    // I'm filling in the control message header.
    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;  // It's a socket-level message.
    cmsg->cmsg_type = SCM_RIGHTS;   // I'm sending rights (a file descriptor).
    cmsg->cmsg_len = CMSG_LEN(sizeof(int)); // It's the size of an int.

    // Finally, I put the file descriptor into the data part of the message.
    *((int *)CMSG_DATA(cmsg)) = fd_to_send;

    return sendmsg(socket, &msg, 0);
}

// This is the main event! I'm starting the master server.
// I'll set up everything, spawn the workers, and then just sit there accepting connections.
int start_master_server()
{
    // 1. First, I need to handle signals.
    struct sigaction sa;
    sa.sa_handler = handle_sigint; // Call this function when SIGINT happens.
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0; // I don't want SA_RESTART because I want accept() to fail when interrupted.
    sigaction(SIGINT, &sa, NULL);

    // 2. Now I'm creating the server socket.
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    // I'm setting SO_REUSEADDR so I can restart the server immediately without waiting for the OS to release the port.
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY; // I'll listen on all available network interfaces.
    address.sin_port = htons(config.port); // I'm converting the port number to network byte order. 

    // I'm binding the socket to the address and port.
    if (bind(server_socket, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        return 1;
    }
    
    // Now I start listening! I can handle a backlog of 128 pending connections.
    listen(server_socket, 128);

    printf("Master (PID: %d) listening on port %d.\n", getpid(), config.port);

    // 3. I'm starting a background thread to print statistics every now and then.
    pthread_t stats_tid;
    pthread_create(&stats_tid, NULL, stats_monitor_thread, NULL);

    // 4. Time to spawn my minions (worker processes)!
    int *worker_pipes = malloc(sizeof(int) * config.num_workers);
    for (int i = 0; i < config.num_workers; i++)
    {
        // I'm creating a pair of connected sockets for IPC.
        // sv[0] is for me (Master), sv[1] is for the Worker.
        int sv[2]; 
        if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) < 0) {
            perror("socketpair");
            exit(1);
        }

        pid_t pid = fork();
        if (pid == 0) {
            // === CHILD PROCESS (WORKER) ===
            // I'm the worker now!
            close(server_socket); // I don't need the listening socket.
            close(sv[0]);         // I don't need the master's end of the pipe.
            
            // I'm ignoring SIGINT because I want to finish my current job before dying.
            // The master will tell me when to stop by closing the pipe.
            signal(SIGINT, SIG_IGN); 
            
            start_worker_process(sv[1]); // I'm starting my shift!
            exit(0);
        }
        
        // === PARENT PROCESS (MASTER) ===
        close(sv[1]); // I don't need the worker's end.
        worker_pipes[i] = sv[0]; // I'll keep my end safe.
    }

    // 5. Main Loop: This is where I spend most of my time.
    int current_worker = 0;
    
    while (server_running) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        // I'm blocking here until a client connects.
        int client_fd = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
        
        // If accept failed, I need to check why.
        if (client_fd < 0) {
            // If it was just a signal (like Ctrl+C), I'll loop back and check server_running.
            if (errno == EINTR) continue; 
            perror("accept");
            continue;
        }

        // Now I'm handing off the connection to a worker.
        // I use Round-Robin scheduling to be fair.
        send_fd(worker_pipes[current_worker], client_fd);
        
        // CRITICAL: I must close my copy of the file descriptor.
        // If I don't, I'll run out of file descriptors and the connection will never close.
        close(client_fd);
        current_worker = (current_worker + 1) % config.num_workers;
    }

    // 6. Shutdown Sequence
    printf("\nShutting down server...\n");

    // I'm closing the pipes. This sends an EOF to the workers, telling them to quit.
    for (int i = 0; i < config.num_workers; i++) {
        close(worker_pipes[i]);
    }

    // I'm waiting for all my children to finish their homework and go to bed.
    while (wait(NULL) > 0);

    // I need to stop the stats thread too.
    // Since it's probably sleeping, I'll have to cancel it.
    pthread_cancel(stats_tid);
    pthread_join(stats_tid, NULL);

    // Cleaning up the last bits of memory.
    free(worker_pipes);
    close(server_socket);

    printf("Server stopped cleanly.\n");
    return 0;
}