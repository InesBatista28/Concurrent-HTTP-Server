// Inês Batista, 124877
// Maria Quinteiro, 124996
#include "http.h"           
#include "cache.h"          // Para a Cache de Ficheiros (g_file_cache, cache_get, cache_put)
#include "worker.h"         // Para a função handle_http_request
#include "stats.h"          // Para update_stats
#include "logger.h"         // Para log_request
#include "shared_memory.h"  // Para g_shared_data (acesso à SHM e semáforos)

#include <stdio.h>          // Para snprintf - formatação de strings
#include <string.h>         // Para strstr, strncpy, strcmp - manipulação de strings
#include <unistd.h>         // Para close - fechar descritores de ficheiro, read
#include <sys/socket.h>     // Para send, setsockopt - funções de socket
#include <stdlib.h>         // Para malloc e free - gestão de memória dinâmica
#include <sys/time.h>       // Para struct timeval (necessário para o timeout)
#include <errno.h>          // Para EAGAIN/EWOULDBLOCK (erros de timeout)
#include <pthread.h>        // Para pthread_self()

// Variável global (exclusiva do processo Worker) para a cache (Inicializada no worker_main)
file_cache_t g_file_cache; 

// Mock do Document Root para simular o caminho completo dos ficheiros
#define MOCK_DOC_ROOT "./www"




// Função para analisar um pedido HTTP
// Extrai método, caminho e versão da primeira linha do pedido
int parse_http_request(const char* buffer, http_request_t* req) {
    // Encontrar o fim da primeira linha (termina com \r\n)
    char* line_end = strstr(buffer, "\r\n");
    if (!line_end) return -1;  // Pedido mal formado
    
    // Extrair apenas a primeira linha do pedido HTTP
    char first_line[1024];
    size_t len = line_end - buffer;  // Calcula comprimento da primeira linha
    strncpy(first_line, buffer, len);  // Copia a primeira linha
    first_line[len] = '\0';           // Adiciona terminador de string
    
    // Dividir a primeira linha em três partes: método, caminho, versão
    if (sscanf(first_line, "%s %s %s", req->method, req->path, req->version) != 3) {
        return -1;  // Pedido mal formado
    }
    
    return 0;  // Pedido analisado corretamente
}

// Função para construir e enviar uma resposta HTTP
void send_http_response(int fd, int status, const char* status_msg,
                       const char* content_type, const char* body, size_t body_len) {
    // Buffer para construir o cabeçalho HTTP
    char header[2048];
    
    // Construir o cabeçalho HTTP completo
    int header_len = snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\r\n"           // Linha de status
        "Content-Type: %s\r\n"         // Tipo de conteúdo
        "Content-Length: %zu\r\n"      // Tamanho do conteúdo
        "Server: ConcurrentHTTP/1.0\r\n"    // Identificação do servidor
        "Connection: close\r\n"          // Fechar ligação após resposta
        "\r\n",                             
        status, status_msg, content_type, body_len);
    
    // Enviar o cabeçalho
    send(fd, header, header_len, 0);
    
    // Enviar o conteúdo
    if (body && body_len > 0) {
        send(fd, body, body_len, 0);  
    }
}

// Função para determinar o tipo MIME
const char* get_mime_type(const char* filename) {
    const char* ext = strrchr(filename, '.');
    if (!ext) return "text/plain";  
    
    if (strcmp(ext, ".html") == 0) return "text/html";           
    if (strcmp(ext, ".css") == 0) return "text/css";             
    if (strcmp(ext, ".js") == 0) return "application/javascript"; 
    if (strcmp(ext, ".png") == 0) return "image/png";            
    if (strcmp(ext, ".jpg") == 0) return "image/jpeg";          
    if (strcmp(ext, ".jpeg") == 0) return "image/jpeg";         
    
    return "application/octet-stream";
}

// Função auxiliar para ler um ficheiro de erro da pasta errors/
char* read_error_file(const char* document_root, const char* error_file) {
    char filepath[512];
    
    snprintf(filepath, sizeof(filepath), "%s/errors/%s", document_root, error_file);
    
    FILE* file = fopen(filepath, "rb");
    if (!file) {
        return NULL;
    }
    
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    if (file_size <= 0) {
        fclose(file);
        return NULL;
    }
    
    char* content = malloc(file_size + 1);
    if (!content) {
        fclose(file);
        return NULL;
    }
    
    size_t bytes_read = fread(content, 1, file_size, file);
    fclose(file);
    
    if (bytes_read != (size_t)file_size) {
        free(content);
        return NULL;
    }
    
    content[file_size] = '\0';  
    return content;
}




// Servir página de erro 404
void send_404_response(int client_fd, const char* document_root) {
    char* error_content = read_error_file(document_root, "404.html");
    
    if (error_content) {
        send_http_response(client_fd, 404, "Not Found", "text/html", 
                        error_content, strlen(error_content));
        free(error_content);
    } else {
        const char* fallback_html = 
            "<!DOCTYPE html>\n<html>\n<head><title>404 Not Found</title></head>\n"
            "<body>\n<h1>404 Not Found</h1>\n<p>The requested resource was not found on this server.</p>\n"
            "</body>\n</html>";
        send_http_response(client_fd, 404, "Not Found", "text/html", 
                        fallback_html, strlen(fallback_html));
    }
}

// Servir página de erro 500
void send_500_response(int client_fd, const char* document_root) {
    char* error_content = read_error_file(document_root, "500.html");
    
    if (error_content) {
        send_http_response(client_fd, 500, "Internal Server Error", "text/html", 
                        error_content, strlen(error_content));
        free(error_content);
    } else {
        const char* fallback_html = 
            "<!DOCTYPE html>\n<html>\n<head><title>500 Internal Server Error</title></head>\n"
            "<body>\n<h1>500 Internal Server Error</h1>\n<p>Something went wrong on the server.</p>\n"
            "</body>\n</html>";
        send_http_response(client_fd, 500, "Internal Server Error", "text/html", 
                        fallback_html, strlen(fallback_html));
    }
}

// Servir página de erro 403
void send_403_response(int client_fd, const char* document_root) {
    char* error_content = read_error_file(document_root, "403.html");
    
    if (error_content) {
        send_http_response(client_fd, 403, "Forbidden", "text/html", 
                        error_content, strlen(error_content));
        free(error_content);
    } else {
        const char* fallback_html = 
            "<!DOCTYPE html>\n<html>\n<head><title>403 Forbidden</title></head>\n"
            "<body>\n<h1>403 Forbidden</h1>\n<p>You don't have permission to access this resource.</p>\n"
            "</body>\n</html>";
        send_http_response(client_fd, 403, "Forbidden", "text/html", 
                        fallback_html, strlen(fallback_html));
    }
}

// Servir página de erro 503
void send_503_response(int client_fd, const char* document_root) {
    char* error_content = read_error_file(document_root, "503.html");
    
    if (error_content) {
        send_http_response(client_fd, 503, "Service Unavailable", "text/html", 
                        error_content, strlen(error_content));
        free(error_content);
    } else {
        const char* fallback_html = 
            "<!DOCTYPE html>\n<html>\n<head><title>503 Service Unavailable</title></head>\n"
            "<body>\n<h1>503 Service Unavailable</h1>\n<p>Server is currently overloaded. Please try again later.</p>\n"
            "</body>\n</html>";
        send_http_response(client_fd, 503, "Service Unavailable", "text/html", 
                        fallback_html, strlen(fallback_html));
    }
}

// NOVO: Função para servir página de erro 408 (Request Timeout)
void send_408_response(int client_fd, const char* document_root) {
    char* error_content = read_error_file(document_root, "408.html");
    
    if (error_content) {
        send_http_response(client_fd, 408, "Request Timeout", "text/html", 
                        error_content, strlen(error_content));
        free(error_content);
    } else {
        const char* fallback_html = 
            "<!DOCTYPE html>\n<html>\n<head><title>408 Request Timeout</title></head>\n"
            "<body>\n<h1>408 Request Timeout</h1>\n<p>The server timed out waiting for the full request.</p>\n"
            "</body>\n</html>";
        send_http_response(client_fd, 408, "Request Timeout", "text/html", 
                        fallback_html, strlen(fallback_html));
    }
}





// Configura o timeout de leitura (SO_RCVTIMEO) para evitar threads bloqueadas.
static int set_socket_timeout(int client_fd, int seconds) {
    struct timeval tv;
    tv.tv_sec = seconds; // Segundos de espera
    tv.tv_usec = 0;      // Microsegundos

    // Configura a opção SO_RCVTIMEO no nível SOL_SOCKET
    if (setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv) < 0) {
        perror("[HTTP] ERRO: Nao foi possivel setar SO_RCVTIMEO");
        return -1;
    }
    return 0;
}




// Lê o conteúdo de um ficheiro do disco. Retorna um ponteiro alocado que DEVE ser libertado.
static char* read_file_from_disk(const char* filepath, size_t *out_size) {
    FILE* file = fopen(filepath, "rb");
    if (!file) {
        return NULL; // Ficheiro não encontrado ou erro de permissão
    }
    
    // Obter tamanho do ficheiro
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    if (file_size <= 0) {
        fclose(file);
        return NULL;
    }
    
    // Alocar memória para o conteúdo
    char* content = malloc(file_size + 1);
    if (!content) {
        fclose(file);
        return NULL;
    }
    
    // Ler conteúdo
    size_t bytes_read = fread(content, 1, file_size, file);
    fclose(file);
    
    if (bytes_read != (size_t)file_size) {
        free(content);
        return NULL;
    }
    
    content[file_size] = '\0';  
    *out_size = file_size;
    return content;
}


// Esta é a função que as threads da Thread Pool executam.
void handle_http_request(int client_fd) {
    char buffer[2048];
    int bytes_read = 0;
    int status_code = 500; // Estado inicial (erro interno)
    size_t bytes_sent = 0;
    http_request_t req = {0};
    const int TIMEOUT_SECONDS = 10; // Timeout de 10 segundos
    const char *remote_ip = "127.0.0.1"; // IP mock para logging
    
    // 1. CONFIGURAR TIMEOUT (NOVIDADE DIA 11)
    if (set_socket_timeout(client_fd, TIMEOUT_SECONDS) != 0) {
        fprintf(stderr, "[HTTP] AVISO: Thread %lu falhou ao configurar timeout.\n", pthread_self());
    }

    // 2. Tentar ler o pedido do cliente (sujeito ao timeout)
    bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);

    if (bytes_read <= 0) {
        // Erro de leitura, timeout ou fecho do cliente
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            // Se o erro foi timeout (408)
            status_code = 408;
            send_408_response(client_fd, MOCK_DOC_ROOT); // Enviar resposta 408
            fprintf(stderr, "[HTTP] TIMEOUT: Conexao excedeu %d segundos.\n", TIMEOUT_SECONDS);
        } else {
            // Outro erro (e.g., cliente fechou a conexão antes de enviar dados)
            status_code = 499; // Cliente fechou a conexão (não oficial, mas útil)
            bytes_sent = 0;
        }
        goto cleanup; // Ir para o passo final (Stats/Log/Close)
    }

    // Sucesso na leitura: terminar string e analisar
    buffer[bytes_read] = '\0'; 
    if (parse_http_request(buffer, &req) != 0) {
        status_code = 400; // Bad Request
        send_http_response(client_fd, 400, "Bad Request", "text/plain", "Bad Request", 11);
        goto cleanup;
    }

    // 3. TENTAR LER DA CACHE (Leitor - usa rdlock)
    
    // Normalizar o path (se for "/", servir index.html)
    const char *path_to_serve = strcmp(req.path, "/") == 0 ? "/index.html" : req.path;
    
    size_t cached_size = 0;
    // cache_get adquire o Read Lock internamente
    const char *cached_content = cache_get(&g_file_cache, path_to_serve, &cached_size);
    
    if (cached_content) {
        // CACHE HIT: Enviar o conteúdo diretamente da cache
        const char* mime_type = get_mime_type(path_to_serve);
        send_http_response(client_fd, 200, "OK", mime_type, cached_content, cached_size);
        bytes_sent = cached_size;
        status_code = 200;
        printf("[HTTP] CACHE HIT para: %s\n", path_to_serve);
    } else {
        // CACHE MISS: Tentar ler o ficheiro do disco
        printf("[HTTP] CACHE MISS para: %s - Tentando ler do disco.\n", path_to_serve);
        char full_path[1024];
        // Construir caminho completo: Exemplo: ./www/index.html
        snprintf(full_path, sizeof(full_path), "%s%s", MOCK_DOC_ROOT, path_to_serve);

        size_t file_size = 0;
        char* file_content = read_file_from_disk(full_path, &file_size);
        
        if (file_content) {
            // SUCESSO: Servir e Adicionar à Cache
            const char* mime_type = get_mime_type(path_to_serve);
            send_http_response(client_fd, 200, "OK", mime_type, file_content, file_size);
            bytes_sent = file_size;
            status_code = 200;
            
            // Adicionar à cache (Escritor - usa wrlock)
            cache_put(&g_file_cache, path_to_serve, file_content, file_size);
            free(file_content); // Libertar a cópia lida do disco
        } else {
            // FALHA: Ficheiro não encontrado (404)
            send_404_response(client_fd, MOCK_DOC_ROOT);
            status_code = 404;
            bytes_sent = 0; // O tamanho exato do 404 será contabilizado no Master se necessário
        }
    }

    // 4. ATUALIZAÇÕES E LIMPEZA
cleanup:
    // 4.1. Atualizar Estatísticas e Log (Protegido por semáforos na SHM)
    if (g_shared_data) {
        // Atualizar estatísticas (protegido por semáforo mutex)
        update_stats(&g_shared_data->stats, status_code, bytes_sent, &g_shared_data->mutex);
        // Registar o pedido no log (protegido por semáforo log_mutex)
        log_request(remote_ip, &req, status_code, bytes_sent, &g_shared_data->log_mutex);
    }
    
    // 4.2. Fechar conexão
    close(client_fd);
}