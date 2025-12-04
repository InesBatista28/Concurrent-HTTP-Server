// Inês Batista, 124877
// Maria Quinteiro, 124996



#include "thread_pool.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>


// A rotina de execução de cada thread na pool. (função principal que cada thread executa)
// Fica num ciclo infinito à espera de tarefas da fila interna.
static void *worker_thread_routine(void *arg) {
    thread_pool_t *pool = (thread_pool_t *)arg;
    task_t task;
    pthread_t thread_id = pthread_self();

    printf("[Pool Thread %lu] Worker thread started.\n", thread_id);

    while (1) {
        // 1. Esperar pelo Mutex (Entra na Secção Crítica)
        if (pthread_mutex_lock(&pool->queue_mutex) != 0) {
            perror("ERRO: pthread_mutex_lock na thread routine");
            break; 
        }

        // 2. Esperar por Trabalho (Bloqueio Condicional)
        while (pool->count == 0 && pool->shutdown == 0) {
            if (pthread_cond_wait(&pool->work_available, &pool->queue_mutex) != 0) {
                perror("ERRO: pthread_cond_wait falhou");
                pthread_mutex_unlock(&pool->queue_mutex);
                goto exit_thread; 
            }
        }
        
        // 3. Verificar o pedido de encerramento
        if (pool->shutdown) {
            pthread_mutex_unlock(&pool->queue_mutex);
            goto exit_thread;
        }

        // --- SECÇÃO CRÍTICA: Consumo da Tarefa ---
        
        // 4. Remover Tarefa da Fila (Dequeue)
        task = pool->task_queue[pool->head];
        pool->head = (pool->head + 1) % MAX_TASK_QUEUE_SIZE;
        pool->count--;
        
        // 5. Sai da Secção Crítica (Liberta o Mutex)
        pthread_mutex_unlock(&pool->queue_mutex);


        // 6. Executar a Tarefa (Fora da Secção Crítica!)
        if (task.function != NULL) {
            task.function(task.arg_fd);
        }
    }

exit_thread:
    printf("[Pool Thread %lu] Thread exiting gracefully.\n", thread_id);
    // IMPORTANTE: Se uma thread termina, o monitor deve ser informado.
    // pthread_exit() permite que a thread termine.
    pthread_exit(NULL); 
    return NULL; 
}





// Esta thread monitoriza o array de workers e reinicia as threads que falham.
static void *monitor_thread_routine(void *arg) {
    thread_pool_t *pool = (thread_pool_t *)arg;
    int i;
    
    printf("[Pool Monitor] Monitor thread started.\n");

    while (!pool->shutdown) {
        // Iterar sobre todas as threads worker
        for (i = 0; i < pool->num_threads; i++) {
            
            // Tenta juntar a thread (non-blocking: usamos pthread_tryjoin_np ou um método similar)
            // Em ambientes GNU, pthread_tryjoin_np(pool->threads[i], NULL) != EBUSY indica que a thread terminou.
            // Para portabilidade, vamos usar a solução mais robusta:
            // A thread monitor usa pthread_join, mas apenas se a thread worker tiver sido detached.
            
            // Contudo, dado que as threads são criadas como joinable, vamos usar um hack 
            // comum em exercícios C: tentar juntar e, se tiver terminado (0), reiniciar.
            // Nota: Este loop vai bloquear (join) se a thread estiver ativa, o que é um
            // design pobre, mas demonstra a monitorização.
            
            int join_result = 0;
            
            // Tenta juntar. Se a thread já tiver morrido, retorna 0 (SUCCESS).
            // Se estiver ativa e joinable, bloqueia.
            // Para evitar o bloqueio no monitor, a thread worker deveria ser detached (pthread_detach).
            // MAS, se a thread worker termina *inesperadamente* (seg fault),
            // a próxima chamada a pthread_join() nesse thread ID retorna 0 e permite o cleanup.

            // 1. Tentar juntar a thread (assume-se que só é chamada se houver suspeita de morte)
            // Usaremos uma solução mais limpa: a main thread faz o join e restart, não o monitor.
            // A thread monitor é usada apenas para sinalizar a monitorização externa.
            
            // *CORREÇÃO DE DESIGN:* Vamos manter o Monitor simples e o loop de monitorização 
            // será integrado na função que o Worker Principal executa após a inicialização da Pool.

            // Se for necessário reiniciar a thread:
            // if (join_result == 0) {
            //     fprintf(stderr, "[Monitor] AVISO: Thread %d terminou. REINICIANDO...\n", i);
            //     
            //     // 2. Recriar a thread
            //     if (pthread_create(&pool->threads[i], NULL, worker_thread_routine, (void *)pool) != 0) {
            //         fprintf(stderr, "[Monitor] ERRO FATAL: Falha ao recriar a thread %d.\n", i);
            //     } else {
            //         fprintf(stdout, "[Monitor] Thread %d recriada com sucesso.\n", i);
            //     }
            // }

            // O monitor apenas dorme e verifica se o pool deve ser encerrado.
        }
        
        // Descansar para evitar consumo excessivo de CPU
        usleep(100000); // 100ms
    }

    printf("[Pool Monitor] Monitor thread exiting.\n");
    pthread_exit(NULL);
    return NULL;
}



// Cria a pool: inicializa estruturas, mutex, cond var e lança as threads.
// Cria a pool e inicia o Monitor.
thread_pool_t* create_thread_pool(int num_threads, task_func_t task_handler) {
    if (num_threads <= 0 || task_handler == NULL) {
        fprintf(stderr, "ERRO: create_thread_pool: Número de threads inválido ou handler nulo.\n");
        return NULL;
    }

    thread_pool_t *pool = (thread_pool_t*)malloc(sizeof(thread_pool_t));
    if (pool == NULL) {
        perror("ERRO: malloc pool");
        return NULL;
    }

    // Inicializar variáveis e arrays
    pool->num_threads = num_threads;
    pool->shutdown = 0;
    pool->head = 0;
    pool->tail = 0;
    pool->count = 0;
    pool->handler_func = task_handler;
    // Alocar espaço para as threads worker + 1 para a thread monitor
    pool->threads = (pthread_t*)malloc(sizeof(pthread_t) * (num_threads + 1)); 
    if (pool->threads == NULL) {
        perror("ERRO: malloc threads array");
        free(pool);
        return NULL;
    }

    // Inicializar Semáforos/Cond Vars
    if (pthread_mutex_init(&pool->queue_mutex, NULL) != 0 ||
        pthread_cond_init(&pool->work_available, NULL) != 0) {
        perror("ERRO: Falha na inicialização do mutex/cond var");
        free(pool->threads);
        free(pool);
        return NULL;
    }

    // Lançar as threads WORKER
    for (int i = 0; i < num_threads; i++) {
        if (pthread_create(&pool->threads[i], NULL, worker_thread_routine, (void*)pool) != 0) {
            perror("ERRO: Falha na criação da thread worker");
            pool->num_threads = i; // Atualiza o contador para saber quantas fazer join
            destroy_thread_pool(pool); 
            return NULL;
        }
    }
    
    // Lançar a THREAD MONITOR (Índice num_threads)
    if (pthread_create(&pool->threads[num_threads], NULL, monitor_thread_routine, (void*)pool) != 0) {
        perror("ERRO: Falha na criação da thread monitor");
        // Se o monitor falhar, encerramos a pool
        destroy_thread_pool(pool); 
        return NULL;
    }
    
    printf("[THREAD POOL] Pool criada com sucesso: %d workers ativas.\n", num_threads);
    return pool;
}




// ** NOVO: Função para monitorizar e reiniciar (Chamada pelo Main Worker Process) **
void run_thread_pool_manager(thread_pool_t *pool) {
    if (pool == NULL) return;

    printf("[MANAGER] Monitorizacao de threads iniciada.\n");

    // Loop de monitorização (permanece aqui até o Master sinalizar o encerramento)
    while (!pool->shutdown) {
        for (int i = 0; i < pool->num_threads; i++) {
            // Tenta juntar-se (join) à thread worker. 
            // Se a thread tiver terminado (inesperadamente), join retorna 0 imediatamente.
            int join_result = pthread_join(pool->threads[i], NULL); 

            if (join_result == 0) {
                // SUCESSO NO JOIN: A thread terminou. É necessário reiniciá-la.
                fprintf(stderr, "[MANAGER] AVISO: Thread %lu terminou inesperadamente. REINICIANDO...\n", (unsigned long)pool->threads[i]);
                
                // Recria a thread na mesma posição do array
                if (pthread_create(&pool->threads[i], NULL, worker_thread_routine, (void *)pool) != 0) {
                    fprintf(stderr, "[MANAGER] ERRO FATAL: Falha ao recriar a thread %d.\n", i);
                    // Aqui, deveria haver um mecanismo de alerta mais robusto
                } else {
                    fprintf(stdout, "[MANAGER] Thread %d recriada com sucesso.\n", i);
                }
            } else if (join_result == ESRCH) {
                // ESRCH: No such process. O ID de thread é inválido ou já foi limpo.
                // Esta condição é mais para depuração, mas indica um problema no ID.
                // Não deve ocorrer se o join for feito corretamente.
            } else if (join_result != EBUSY) {
                // EBUSY (ou outro código de erro): significa que a thread está a correr.
                // O loop continua sem bloqueio.
            }
        }
        usleep(100000); // Esperar 100ms antes de verificar as threads novamente
    }
}


// Encerra a pool de forma graciosa: sinaliza shutdown, acorda threads e espera.
void destroy_thread_pool(thread_pool_t *pool) {
    if (pool == NULL) return;

    // 1. Sinalizar o encerramento (Dentro do Mutex)
    pthread_mutex_lock(&pool->queue_mutex);
    pool->shutdown = 1;
    pthread_mutex_unlock(&pool->queue_mutex);

    // 2. Acordar todas as threads
    pthread_cond_broadcast(&pool->work_available);
    
    printf("[THREAD POOL] Iniciando shutdown e aguardando threads...\n");

    // 3. Esperar que todas as threads terminem (Worker e Monitor)
    // O array tem 'num_threads' workers + 1 monitor
    for (int i = 0; i < pool->num_threads + 1; i++) {
        pthread_join(pool->threads[i], NULL);
    }
    
    // 4. Limpeza de recursos
    pthread_mutex_destroy(&pool->queue_mutex);
    pthread_cond_destroy(&pool->work_available);
    free(pool->threads);
    free(pool);

    printf("[THREAD POOL] Shutdown completo.\n");
}



// Adiciona uma nova tarefa (socket FD) à fila interna (Produtor).
int add_task(thread_pool_t *pool, int client_fd) {
    if (pool == NULL || pool->shutdown) {
        return -1;
    }

    // 1. Bloquear o Mutex
    if (pthread_mutex_lock(&pool->queue_mutex) != 0) {
        perror("ERRO: pthread_mutex_lock em add_task");
        return -1;
    }

    // 2. Verificar se a fila está cheia
    if (pool->count == MAX_TASK_QUEUE_SIZE) {
        fprintf(stderr, "AVISO: Fila interna do Worker cheia. Tarefa %d descartada.\n", client_fd);
        pthread_mutex_unlock(&pool->queue_mutex);
        return -1; // Fila cheia
    }

    // 3. Inserir a tarefa
    pool->task_queue[pool->tail].function = pool->handler_func;
    pool->task_queue[pool->tail].arg_fd = client_fd;
    
    // 4. Mover o tail (fila circular)
    pool->tail = (pool->tail + 1) % MAX_TASK_QUEUE_SIZE;
    
    // 5. Incrementar a contagem
    pool->count++;
    
    // 6. Sinalizar uma thread consumidora que há trabalho
    pthread_cond_signal(&pool->work_available); 

    // 7. Desbloquear o Mutex
    pthread_mutex_unlock(&pool->queue_mutex);
    
    return 0; // Sucesso
}