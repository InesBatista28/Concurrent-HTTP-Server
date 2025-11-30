// Inês Batista, 124877
// Maria Quinteiro, 124996

#include "master.h"
#include "config.h"
#include "shared_memory.h"
#include "semaphores.h"
#include <stdio.h>          // Para printf, perror
#include <stdlib.h>         // Para exit
#include <unistd.h>         // Para close, fork, getpid
#include <sys/socket.h>     // Para socket, setsockopt, bind, listen, accept
#include <netinet/in.h>     // Para sockaddr_in
#include <signal.h>         // Para signal, SIGINT, SIGTERM, kill
#include <sys/wait.h>       // Para waitpid
#include <string.h>         // Para memset

// Variável global para controlar o loop principal do master
// volatile: garante que o compilador não optimiza o acesso a esta variável
// sig_atomic_t: tipo que garante acesso atómico em handlers de sinal
volatile sig_atomic_t keep_running = 1;

// Array para guardar os PIDs dos processos worker
// Permite ao master controlar e terminar os workers graciosamente
static pid_t worker_pids[100];  // Assume máximo de 100 workers
static int num_workers = 0;

// Handler para sinais (Ctrl+C)
// Permite parar o servidor graciosamente
void signal_handler(int signum) {
    keep_running = 0;  // Altera a flag para sair do loop principal
    printf("\nReceived signal %d. Gracefully stopping server...\n", signum);
}

// Função para criar o socket do servidor (do template do professor)
int create_server_socket(int port) {
    // Criar socket TCP
    // AF_INET = IPv4, SOCK_STREAM = TCP, 0 = protocolo padrão
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket failed");
        return -1;
    }
    
    // Configurar opção para reutilizar o porto imediatamente
    // SO_REUSEADDR evita "Address already in use" ao reiniciar o servidor
    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    // Configurar endereço do servidor
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;           // Família IPv4
    addr.sin_addr.s_addr = INADDR_ANY;   // Aceitar ligações de qualquer interface
    addr.sin_port = htons(port);         // Porto (convertido para network byte order)
    
    // Associar o socket ao endereço e porto
    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind failed");
        close(sockfd);
        return -1;
    }
    
    // Colocar o socket em modo escuta
    // 128 = tamanho da fila de ligações pendentes (backlog)
    if (listen(sockfd, 128) < 0) {
        perror("listen failed");
        close(sockfd);
        return -1;
    }
    
    return sockfd;  // Retorna o socket criado com sucesso
}

// Função para colocar uma ligação na fila partilhada (do template do professor)
// Implementa o padrão produtor-consumidor com semáforos
void enqueue_connection(shared_data_t* data, semaphores_t* sems, int client_fd) {
    // PASSO 1: Esperar por um slot vazio na fila
    // sem_wait decrementa empty_slots - bloqueia se não houver slots vazios
    // Isto garante que não excedemos o tamanho máximo da fila
    sem_wait(sems->empty_slots);
    
    // PASSO 2: Obter acesso exclusivo à fila
    // sem_wait no mutex garante que só um processo acede à fila de cada vez
    sem_wait(sems->queue_mutex);
    
    // PASSO 3: SECÇÃO CRÍTICA - Adicionar ligação à fila
    // Adicionar o socket do cliente na próxima posição disponível
    data->queue.sockets[data->queue.rear] = client_fd;
    
    // Atualizar o índice rear (fim da fila) usando aritmética modular
    // % MAX_QUEUE_SIZE garante que a fila é circular
    data->queue.rear = (data->queue.rear + 1) % MAX_QUEUE_SIZE;
    
    // Incrementar o contador de elementos na fila
    data->queue.count++;
    
    // PASSO 4: Libertar o mutex da fila
    sem_post(sems->queue_mutex);
    
    // PASSO 5: Sinalizar que há um novo elemento na fila
    // sem_post incrementa filled_slots - acorda workers à espera de trabalho
    sem_post(sems->filled_slots);
}

// Função para criar processos worker usando fork()
// config: configurações do servidor
// shared_data: memória partilhada com a fila
// sems: semáforos para sincronização
// Retorna: número de workers criados com sucesso
int create_worker_processes(server_config_t* config, shared_data_t* shared_data, semaphores_t* sems) {
    printf("Creating %d worker processes...\n", config->num_workers);
    
    int workers_created = 0;
    
    for (int i = 0; i < config->num_workers; i++) {
        // fork() cria um novo processo (cópia do processo atual)
        pid_t pid = fork();
        
        if (pid == -1) {
            // Erro no fork()
            perror("fork failed");
            continue;  // Tenta criar os workers restantes
        }
        else if (pid == 0) {
            // CÓDIGO EXECUTADO APENAS NO PROCESSO WORKER (filho)
            printf("Worker %d (PID: %d) created\n", i, getpid());
            
            // TODO: Implementar worker_main quando worker.c estiver pronto
            // Por agora, workers apenas imprimem uma mensagem e terminam
            printf("Worker %d simulating work...\n", i);
            sleep(10);  // Simula trabalho por 10 segundos
            printf("Worker %d finished\n", i);
            exit(0);  // Worker termina
        }
        else {
            // CÓDIGO EXECUTADO APENAS NO PROCESSO MASTER (pai)
            // Guardar o PID do worker para podermos controlá-lo depois
            worker_pids[workers_created] = pid;
            workers_created++;
            
            printf("Worker %d created with PID: %d\n", i, pid);
        }
    }
    
    return workers_created;
}

// Função para terminar todos os processos worker graciosamente
// Envia sinal SIGTERM a todos os workers e espera que terminem
void terminate_worker_processes() {
    printf("Terminating worker processes...\n");
    
    for (int i = 0; i < num_workers; i++) {
        if (worker_pids[i] > 0) {
            printf("   - Sending SIGTERM to worker PID: %d\n", worker_pids[i]);
            
            // Enviar sinal SIGTERM para o worker
            // SIGTERM é um sinal de terminação graciosa
            kill(worker_pids[i], SIGTERM);
            
            // Esperar que o worker termine
            // waitpid bloqueia até o processo filho terminar
            // NULL = não nos interessa o status de saída
            waitpid(worker_pids[i], NULL, 0);
            
            printf("   Worker PID: %d terminated\n", worker_pids[i]);
        }
    }
}

// Função principal do processo master
// Configura o servidor, cria workers e gere ligações
int master_main(server_config_t* config) {
    printf("MASTER PROCESS (PID: %d) - Starting...\n", getpid());
    
    // 1. CONFIGURAR HANDLERS DE SINAL (versão simplificada)
    // Usar signal() em vez de sigaction para simplificar
    signal(SIGINT, signal_handler);   // Ctrl+C
    signal(SIGTERM, signal_handler);  // Sinal de terminação
    
    // 2. CRIAR MEMÓRIA PARTILHADA E SEMÁFOROS
    printf("Initializing shared resources...\n");
    
    shared_data_t* shared_data = create_shared_memory();
    if (!shared_data) {
        printf("ERROR: Could not create shared memory\n");
        return -1;
    }
    printf("Shared memory created\n");
    
    semaphores_t sems;
    if (init_semaphores(&sems, config->max_queue_size) != 0) {
        printf("ERROR: Could not create semaphores\n");
        destroy_shared_memory(shared_data);
        return -1;
    }
    printf("Semaphores initialized\n");
    
    // 3. CRIAR SOCKET DO SERVIDOR
    int server_fd = create_server_socket(config->port);
    if (server_fd < 0) {
        printf("ERROR: Could not create server socket\n");
        destroy_semaphores(&sems);
        destroy_shared_memory(shared_data);
        return -1;
    }
    printf("Server socket created on port %d\n", config->port);
    
    // 4. CRIAR PROCESSOS WORKER
    num_workers = create_worker_processes(config, shared_data, &sems);
    if (num_workers == 0) {
        printf("ERROR: Could not create any workers\n");
        close(server_fd);
        destroy_semaphores(&sems);
        destroy_shared_memory(shared_data);
        return -1;
    }
    printf("%d worker processes created successfully\n", num_workers);
    
    // 5. LOOP PRINCIPAL DO MASTER - ACEITAR LIGAÇÕES
    printf("\nMaster ready to accept connections...\n");
    printf("Server running at: http://localhost:%d\n", config->port);
    printf("Press Ctrl+C to stop the server\n\n");
    
    int connection_count = 0;
    
    while (keep_running) {
        // Aceitar uma nova ligação de cliente
        // accept() bloqueia até que um cliente se ligue
        int client_fd = accept(server_fd, NULL, NULL);
        
        if (client_fd < 0) {
            // Se accept falhar (mas não for porque estamos a parar), mostra erro
            if (keep_running) {
                perror("accept failed");
            }
            continue;  // Continua para a próxima iteração
        }
        
        connection_count++;
        
        // Colocar a ligação na fila partilhada para os workers processarem
        // enqueue_connection usa semáforos para sincronização thread-safe
        enqueue_connection(shared_data, &sems, client_fd);
        
        // Mostrar estatísticas (para debug)
        printf("Connection %d accepted (socket %d) - In queue: %d/%d\n", 
               connection_count, client_fd, shared_data->queue.count, config->max_queue_size);
    }
    
    // 6. LIMPEZA FINAL - executado quando o servidor para
    printf("\nMASTER PROCESS - Performing cleanup...\n");
    
    // Fechar socket do servidor - não aceita mais ligações
    close(server_fd);
    printf("Server socket closed\n");
    
    // Terminar workers graciosamente
    terminate_worker_processes();
    
    // Destruir semáforos e memória partilhada
    destroy_semaphores(&sems);
    destroy_shared_memory(shared_data);
    
    printf("Master terminated gracefully\n");
    return 0;
}