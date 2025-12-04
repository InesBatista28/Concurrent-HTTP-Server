// Inês Batista, 124877
// Maria Quinteiro, 124996

#ifndef HTTP_H
#define HTTP_H

#include <sys/types.h>

// Estrutura para guardar os dados essenciais de um pedido HTTP
typedef struct {
    char method[16];   // Ex: GET, POST
    char path[256];    // Ex: /index.html
    char version[16];  // Ex: HTTP/1.1
} http_request_t;

// Variável global para a Cache de Ficheiros (será definida no Worker)
// A definição completa de file_cache_t deve estar em cache.h
typedef struct file_cache_s file_cache_t;
extern file_cache_t g_file_cache; 

// Mock do Document Root (se não for passado via argumento de worker_main)
#define MOCK_DOC_ROOT "./www"

// Funções de parsing e resposta
int parse_http_request(const char* buffer, http_request_t* req);
void send_http_response(int fd, int status, const char* status_msg,
                       const char* content_type, const char* body, size_t body_len);
const char* get_mime_type(const char* filename);

// Funções de resposta a erros
void send_404_response(int client_fd, const char* document_root);
void send_500_response(int client_fd, const char* document_root);
void send_403_response(int client_fd, const char* document_root);
void send_503_response(int client_fd, const char* document_root);
void send_408_response(int client_fd, const char* document_root); // NOVO: Request Timeout

// Função principal de tratamento de pedidos (implementada em http.c)
void handle_http_request(int client_fd);

#endif 