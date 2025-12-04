

// Inês Batista, 124877
// Maria Quinteiro, 124996

#include "shared_memory.h"   // Acesso à SHM, semáforos e shared_data_t
#include "worker.h"          // Protótipos das funções Worker
#include "config.h"          // Estruturas de configuração
#include "stats.h"           // função update_stats
#include "logger.h"          // função log_request
#include "http.h"            // estrutura http_request_t (assumida)
#include "thread_pool.h"     // <<< NOVO: Para a Thread Pool

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>      // Funções de rede (send, close).
#include <string.h>          // memset, strlen.
#include <signal.h>          // Para o signal_handler.
#include <sys/mman.h>        // munmap (para detach da SHM).
#include <fcntl.h>           // shm_open flags.
#include <sys/stat.h>        // shm_open mode.

// flag de controlo para terminação
volatile sig_atomic_t worker_keep_running = 1;

// Variável global para a Thread Pool interna
thread_pool_t *g_pool = NULL;

// resposta de HTTP simpes 200 OK (APENAS PARA TESTES INICIAIS)
static const char* HTTP_200_RESPONSE =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/plain\r\n"
    "Connection: close\r\n"
    "Content-Length: 17\r\n"
    "\r\n"
    "Worker Process OK";

    
void worker_signal_handler(int signum) {
    worker_keep_running = 0; // altera a flag para conseguir sair do loop principal
    printf("[Worker %d] Received signal %d. Shutting down...\n", getpid(), signum);
}

// simula o processamento de um pedido (ENVIA UM 200 OK SIMPLES)
// nova executada agora por uma thread da pool 
void handle_http_request(int client_fd) {
    size_t bytes_sent = strlen(HTTP_200_RESPONSE);
    int status_code = 200;
    

    // Vamos simular uma estrutura de pedido para o log.
    http_request_t mock_req = {0};
    strcpy(mock_req.method, "GET");
    strcpy(mock_req.path, "/test");
    strcpy(mock_req.version, "HTTP/1.1");
    // O IP remoto não está disponível aqui, usamos mock.
    const char *mock_ip = "127.0.0.1"; 


    // 1. Enviar resposta 200 OK
    send(client_fd, HTTP_200_RESPONSE, bytes_sent, 0);

    // 2. Atualizar estatísticas (USANDO A FUNÇÃO CENTRALIZADA)
    if (g_shared_data) {
        // Usamos a função que faz o sem_wait/post internamente
        update_stats(&g_shared_data->stats, status_code, bytes_sent, &g_shared_data->mutex);
        
        // 3. REGISTAR O PEDIDO (LOG)
        log_request(mock_ip, &mock_req, status_code, bytes_sent, &g_shared_data->mutex);
    }
    
    // 4. fechar conexão
    close(client_fd);
    printf("[Worker %d | Thread %lu] Processed request (socket %d). Status: %d\n", 
           getpid(), pthread_self(), client_fd, status_code);
}



int worker_main(server_config_t* config) {
    int exit_code = 0;
    int worker_pid = getpid();
    
    // 1. configurar handlers de sinal
    signal(SIGINT, SIG_IGN);  
    signal(SIGTERM, worker_signal_handler); 
    signal(SIGPIPE, SIG_IGN); // Ignora SIGPIPE, permite que o send/write retorne um erro

    // 2. ligar à memória partilhada (assume que o Master já a criou!!!)
    int shm_fd = shm_open(SHM_NAME, O_RDWR, 0666); 
    if (shm_fd < 0) {
        perror("[Worker] shm_open failed");
        return -1;
    }
    g_shared_data = (shared_data_t *)mmap(0, sizeof(shared_data_t), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    close(shm_fd);

    if (g_shared_data == MAP_FAILED) {
        perror("[Worker] mmap failed");
        return -1;
    }

    printf("[Worker %d] Attached to shared memory.\n", worker_pid);

    // 3. INICIALIZAR A THREAD POOL 
    // Cada Worker cria M threads para lidar com os sockets que recebe.
    g_pool = create_thread_pool(config->num_threads, handle_http_request);
    if (g_pool == NULL) {
        fprintf(stderr, "[Worker %d] ERRO: Falha ao criar a Thread Pool. A sair.\n", worker_pid);
        munmap(g_shared_data, sizeof(shared_data_t));
        return 1;
    }
    
    printf("[Worker %d] Thread Pool criada com %d threads. Starting work loop...\n", worker_pid, config->num_threads);


    // 4. loop principal do Consumidor da SHM (Produtor para a Pool)
    while (worker_keep_running) {
        // O Worker PRINCIPAL bloqueia aqui à espera de um socket do Master
        int client_fd = dequeue_connection();

        if (client_fd > 0) {
            // SUCESSO: Conexão recebida. O Worker delega à Thread Pool.
            
            // ESTE É O PONTO CRÍTICO DA CONCORRÊNCIA HÍBRIDA!
            if (add_task(g_pool, client_fd) != 0) {
                // AVISO: A fila interna da Thread Pool está cheia.
                fprintf(stderr, "[Worker %d] AVISO: Thread Pool Task Queue cheia. Fechando socket %d.\n", worker_pid, client_fd);
                close(client_fd);
            }
            
        } else if (client_fd == -1 && worker_keep_running == 0) {
            // Saída graciosa (SIGTERM apanhou o sem_wait/dequeue)
            break; 
        } else if (client_fd == -1 && worker_keep_running == 1) {
             // Erro inesperado no dequeue_connection (pode ser problema de semáforo)
             fprintf(stderr, "[Worker %d] FATAL ERROR: dequeue failed but worker still running. Retrying in 1s...\n", worker_pid);
             sleep(1);
        }
    }


    // 5. LIMPEZA FINAL
    printf("[Worker %d] Iniciando limpeza da Thread Pool...\n", worker_pid);
    
    // Destrói a pool (espera por todas as M threads)
    if (g_pool != NULL) {
        destroy_thread_pool(g_pool);
        g_pool = NULL;
    }
    
    // O Worker APENAS desmapeia (detach).
    if (g_shared_data != NULL) {
        munmap(g_shared_data, sizeof(shared_data_t)); 
    }
    
    printf("[Worker %d] Terminated gracefully.\n", worker_pid);
    return exit_code;
}