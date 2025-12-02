

// Inês Batista, 124877
// Maria Quinteiro, 124996

#include "shared_memory.h"   // <--- GARANTE SHM_NAME, shared_data_t, e semáforos
#include "worker.h"          // Protótipos das funções Worker (requeridos se existir worker.h)
#include "config.h"          // Estruturas de configuração (requeridos se existir config.h)

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
void process_request(int client_fd) {
    // para já, apenas enviar a resposta de sucesso e atualizar estatísticas
    

    // 1.Enviar resposta 200 OK
    send(client_fd, HTTP_200_RESPONSE, strlen(HTTP_200_RESPONSE), 0);

    // 2. Atualizar estatísticas com o sucesso
    // thread-safe com mutex !!
    if (g_shared_data) {
        // Obter acesso exclusivo às estatísticas e fila.
        sem_wait(&g_shared_data->mutex);
        g_shared_data->stats.total_requests++;
        g_shared_data->stats.status_200++;
        sem_post(&g_shared_data->mutex);
    }

    // 3. fechar conexão
    close(client_fd);
    printf("[Worker %d] Processed request (socket %d). Total 200s: %d\n", 
            getpid(), client_fd, g_shared_data ? g_shared_data->stats.status_200 : 0);
}



int worker_main(server_config_t* config) {
    // 1. configurar handlers de sinal
    // o worker deve responder ao SIGTERM enviado pelo Master
    signal(SIGINT, SIG_IGN);  //Ignora o Ctr+C para deixar o Master tratar do shutdown inicial
    signal(SIGTERM, worker_signal_handler); //terminação pelo Master

    // 2. ligar à memória partilhada (assume que o Master já a criou!!!)
    // O SHM_NAME é uma macro definida em shared_memory.h
    int shm_fd = shm_open(SHM_NAME, O_RDWR, 0666); 
    if (shm_fd < 0) {
        perror("[Worker] shm_open failed");
        return -1;
    }
    // Mapeamento. Assume que o Master já definiu o tamanho.
    g_shared_data = (shared_data_t *)mmap(0, sizeof(shared_data_t), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    close(shm_fd);

    if (g_shared_data == MAP_FAILED) {
        perror("[Worker] mmap failed");
        return -1;
    }

    printf("[Worker %d] Attached to shared memory. Starting work loop...\n", getpid());


    // 3. loop principal do consumidor
    while (worker_keep_running) {
        // A função dequeue_connection bloqueia o Worker se a fila estiver vazia (sem_wait full_slots).
        int client_fd = dequeue_connection();

        // Se o worker recebeu SIGTERM enquanto estava bloqueado em sem_wait,
        // o sem_wait retorna -1 com errno = EINTR. O dequeue_connection trata isso.
        
        if (client_fd > 0) {
            // Conexão recebida, processar.
            process_request(client_fd);
        } else if (client_fd == -1 && worker_keep_running == 0) {
            // Se dequeue falhou (client_fd == -1) E o Master já sinalizou o fim (worker_keep_running = 0).
            break; // Sair do loop.
        } else if (client_fd == -1 && worker_keep_running == 1) {
             // Se dequeue falhou mas ainda está a correr (erro de semáforo).
             fprintf(stderr, "[Worker %d] FATAL ERROR: dequeue failed but worker still running. Retrying in 1s...\n", getpid());
             sleep(1);
        }
    }


    // 4. limpeza final
    if (g_shared_data != NULL) {
        // O Worker APENAS desmapeia (detach). O Master é que desvincula (unlink).
        munmap(g_shared_data, sizeof(shared_data_t)); 
    }
    
    printf("[Worker %d] Terminated gracefully.\n", getpid());
    return 0;
}