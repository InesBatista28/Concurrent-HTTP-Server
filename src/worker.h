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

// Função que cada thread na pool irá executar para tratar um pedido HTTP.
// AQUI O nome foi alterado de 'process_request' para 'handle_http_request' !!
void handle_http_request(int client_fd);

#endif 