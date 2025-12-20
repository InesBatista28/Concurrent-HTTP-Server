#include <stdio.h> // I need this for printing to the console (printf).
#include <stdlib.h> // This gives me memory allocation and process control (exit).
#include <string.h> // I use this for string manipulation functions like strcmp.
#include <unistd.h> // This provides access to the POSIX operating system API (fork, close).
#include <getopt.h> // I use this to parse command-line arguments easily.
#include <sys/types.h> // I need this for system data types like pid_t.
#include <sys/stat.h> // This lets me change file permissions (umask).
#include <fcntl.h> // I use this for file control options (open).
#include "master.h" // I need to know about the master process functions.
#include "config.h" // I need to know how to handle configuration.
#include <signal.h> // I need this for signal handling (SIGPIPE).
#include "shared_mem.h" // I need to set up shared memory for statistics.

// I'm declaring the configuration structure globally so I can access it from anywhere.
server_config_t config;

// This is a helper function to tell the user how to interact with me.
// If they run me with -h or make a mistake, I'll show them this message.
void print_usage(const char *prog_name) {
    printf("Usage: %s [OPTIONS]\n", prog_name);
    printf("Options:\n");
    printf("  -c, --config PATH    Configuration file path (default: ./server.conf)\n");
    printf("  -p, --port PORT      Port to listen on (default: 8080)\n");
    printf("  -w, --workers NUM    Number of worker processes (default: 4)\n");
    printf("  -t, --threads NUM    Threads per worker (default: 10)\n");
    printf("  -d, --daemon         Run in background\n");
    printf("  -v, --verbose        Enable verbose logging\n");
    printf("  -h, --help           Show this help message\n");
    printf("  --version            Show version information\n");
}

// This function transforms me into a daemon process.
// It allows me to run in the background, detached from the terminal, like a ghost!
void daemonize() {
    pid_t pid;

    // First, I fork the process.
    pid = fork();
    // If the fork fails, I can't continue, so I exit with an error.
    if (pid < 0) exit(EXIT_FAILURE);
    // If I am the parent process, I exit successfully.
    // This leaves the child process running in the background.
    if (pid > 0) exit(EXIT_SUCCESS);

    // Now, as the child, I create a new session.
    // This makes me the session leader and detaches me from the controlling terminal.
    if (setsid() < 0) exit(EXIT_FAILURE);

    // I fork again to ensure I can never acquire a controlling terminal.
    // It's a double-fork trick!
    pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS);

    // I set the file mode creation mask to 0 so I have full control over permissions.
    umask(0);
    // I change my working directory to the root directory.
    // This prevents me from locking any mounted filesystems.
    if (chdir("/") < 0) {
        perror("chdir");
        exit(EXIT_FAILURE);
    }

    // I close the standard file descriptors (stdin, stdout, stderr)
    // because a daemon shouldn't be reading from or writing to the terminal.
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    // I redirect them to /dev/null just in case some code tries to use them.
    open("/dev/null", O_RDONLY);
    open("/dev/null", O_RDWR);
    open("/dev/null", O_RDWR);
}

// Here is the main entry point where my life begins!
int main(int argc, char *argv[]) {
    int opt;
    int daemon_mode = 0;
    char config_file[256] = "server.conf";

    // I'll start by setting up some sensible default values.
    // This way, I can run even if the user doesn't tell me anything.
    config.port = 8080; // I'll listen on port 8080 by default.
    config.num_workers = 4; // I'll use 4 worker processes.
    config.threads_per_worker = 10; // Each worker will have 10 threads.
    config.max_queue_size = 100; // I can hold 100 pending connections.
    config.cache_size_mb = 10; // I'll give each worker 10MB of cache.
    config.timeout_seconds = 30; // Connections will time out after 30 seconds of silence.
    config.keep_alive_timeout = 5; // Keep-alive connections get 5 seconds.
    strncpy(config.document_root, "./www", sizeof(config.document_root)); // I'll serve files from ./www.
    strncpy(config.log_file, "access.log", sizeof(config.log_file)); // I'll log everything to access.log.

    // I'm defining the command-line options I understand.
    static struct option long_options[] = {
        {"config", required_argument, 0, 'c'}, // -c or --config for config file
        {"port", required_argument, 0, 'p'},   // -p or --port for port
        {"workers", required_argument, 0, 'w'}, // -w or --workers for worker count
        {"threads", required_argument, 0, 't'}, // -t or --threads for thread count
        {"daemon", no_argument, 0, 'd'},       // -d or --daemon to run in background
        {"verbose", no_argument, 0, 'v'},      // -v or --verbose for chatty logs
        {"help", no_argument, 0, 'h'},         // -h or --help for help
        {"version", no_argument, 0, 0},        // --version to see who I am
        {0, 0, 0, 0}
    };

    // First, I'll quickly scan the arguments to see if a config file was specified.
    // I need to do this first because the config file might change my defaults.
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--config") == 0) {
            if (i + 1 < argc) {
                // Found it! I'll copy the path safely.
                strncpy(config_file, argv[i+1], sizeof(config_file) - 1);
                config_file[sizeof(config_file) - 1] = '\0';
            }
        }
    }

    // Now I'll load the configuration from the file.
    load_config(config_file, &config);

    // Next, I'll check if any environment variables are set.
    // These override the config file settings.
    parse_env_vars(&config);

    // Finally, I'll process the command-line arguments again.
    // These have the highest priority, so they override everything else.
    optind = 1; // I need to reset getopt index to start over.
    while ((opt = getopt_long(argc, argv, "c:p:w:t:dvh", long_options, NULL)) != -1) {
        switch (opt) {
            case 'c':
                // I already handled the config file, so I'll skip this.
                break;
            case 'p':
                // The user wants a specific port. Got it.
                config.port = atoi(optarg);
                break;
            case 'w':
                // Setting the number of workers as requested.
                config.num_workers = atoi(optarg);
                break;
            case 't':
                // Setting the number of threads per worker.
                config.threads_per_worker = atoi(optarg);
                break;
            case 'd':
                // The user wants me to run as a daemon.
                daemon_mode = 1;
                break;
            case 'v':
                // Verbose logging requested. I'll remember that (implementation dependent).
                break;
            case 'h':
                // The user needs help, so I'll print the usage and exit.
                print_usage(argv[0]);
                return 0;
            case 0:
                // Handling long options without short equivalents (like --version).
                if (strcmp(argv[optind-1], "--version") == 0) {
                    printf("Concurrent HTTP Server v1.0\n");
                    return 0;
                }
                break;
            default:
                // I don't recognize this option, so I'll show the usage.
                print_usage(argv[0]);
                return 1;
        }
    }

    // If daemon mode was requested, I'll detach myself now.
    if (daemon_mode) {
        daemonize();
    }

    // Ignore SIGPIPE to prevent crash on client disconnect
    signal(SIGPIPE, SIG_IGN);

    // I'm initializing the shared memory for statistics.
    init_shared_stats();

    // Everything is set up! I'm handing control over to the master server logic.
    return start_master_server();
}