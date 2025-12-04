// Inês Batista, 124877
// Maria Quinteiro, 124996

#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <pthread.h>

// Tipo de função que as threads irão executar (aponta para a função de tratamento de sockets)
// A função de tratamento de sockets (ex: handle_http_request) receberá o client_fd.
typedef void (*task_func_t)(int); 

// Estrutura que representa uma tarefa
typedef struct {
    task_func_t function; // Ponteiro para a função a executar (e.g., handle_request)
    int arg_fd;           // Argumento da função (o socket FD do cliente)
} task_t;

// Capacidade máxima da fila de tarefas interna de cada Worker
#define MAX_TASK_QUEUE_SIZE 256 

// Estrutura principal da Thread Pool
typedef struct {
    pthread_t *threads;          // Array de IDs das threads
    int num_threads;             // Número total de threads na pool
    
    // Fila Circular de Tarefas (Protegida por queue_mutex)
    task_t task_queue[MAX_TASK_QUEUE_SIZE]; 
    int head;                               // Índice de remoção (Consumidor/Thread)
    int tail;                               // Índice de inserção (Produtor/Worker Principal)
    int count;                              // Contagem atual de tarefas na fila

    pthread_mutex_t queue_mutex;    // Mutex: Protege o acesso à fila e à flag 'shutdown'
    pthread_cond_t work_available;  // Cond Var: Threads esperam por trabalho (count > 0)
    
    int shutdown;                   // Flag: 0=running, 1=shutdown requested
    task_func_t handler_func;       // Função a ser executada pelas threads
} thread_pool_t;

// Funções públicas
// task_handler é a função que as threads irão executar quando tiverem um socket
thread_pool_t* create_thread_pool(int num_threads, task_func_t task_handler);
void destroy_thread_pool(thread_pool_t *pool);
// Adiciona uma nova tarefa (socket FD) à fila
int add_task(thread_pool_t *pool, int client_fd);

#endif