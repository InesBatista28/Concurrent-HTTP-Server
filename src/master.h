// Inês Batista, 124877
// Maria Quinteiro, 124996

#ifndef MASTER_H
#define MASTER_H

#include  "config.h" // Para server_cofig_t

#include "shared_memory.h"     // Para shared_data_t
#include "semaphores.h"     // Para semaphores_t

// Função para criar o socket do servidor
// port: número do porto onde o servidor vai escutar
// Retorna: descritor do socket ou -1 em caso de erro
int create_server_socket(int port);

// Função para colocar uma ligação na fila partilhada
// data: ponteiro para a memória partilhada com a fila
// sems: semáforos para sincronização
// client_fd: descritor do socket do cliente a colocar na fila
void enqueue_connection(shared_data_t* data, semaphores_t* sems, int client_fd);

// Função principal do processo master
// config: configurações do servidor
// Retorna: 0 em sucesso, -1 em erro
int master_main(server_config_t* config);

#endif