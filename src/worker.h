// Inês Batista, 124877
// Maria Quinteiro, 124996
#ifndef WORKER_H
#define WORKER_H

#include "config.h"

// Flag controlada pelo signal handler para terminação graciosa.
extern volatile sig_atomic_t worker_keep_running;

// Função principal do Worker.
int worker_main(server_config_t* config);

// Handler de sinal para terminação.
void worker_signal_handler(int signum);

// Simula o processamento de um pedido e atualiza estatísticas.
void process_request(int client_fd);

#endif 