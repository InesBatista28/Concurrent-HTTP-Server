// Inês Batista, 124877
// Maria Quinteiro, 124996

#include "http.h"           
#include <stdio.h>          // Para snprintf - formatação de strings
#include <string.h>         // Para strstr, strncpy, strcmp - manipulação de strings
#include <unistd.h>         // Para close - fechar descritores de ficheiro
#include <sys/socket.h>     // Para send - enviar dados através do socket
#include <stdlib.h>         // Para malloc e free - gestão de memória dinâmica !!! ISTO ESTAVA A DAR ERRO


// Função para analisar um pedido HTTP
// Extrai método, caminho e versão da primeira linha do pedido
int parse_http_request(const char* buffer, http_request_t* req) {
    // Encontrar o fim da primeira linha (termina com \r\n)
    // strstr procura a primeira ocorrência de "\r\n" no buffer
    char* line_end = strstr(buffer, "\r\n");
    if (!line_end) return -1;  // Se não encontrar, pedido mal formado

    
    // Extrair apenas a primeira linha do pedido HTTP
    char first_line[1024];
    size_t len = line_end - buffer;  // Calcula comprimento da primeira linha
    strncpy(first_line, buffer, len);  // Copia a primeira linha
    first_line[len] = '\0';           // Adiciona terminador de string
    

    // Dividir a primeira linha em três partes: método, caminho, versão
    // sscanf lê três strings separadas por espaços
    if (sscanf(first_line, "%s %s %s", req->method, req->path, req->version) != 3) {
        return -1;  // Se não conseguir ler três partes, pedido mal formado
    }
    
    return 0;  // Pedido analisado corretamente
}




// Função para construir e enviar uma resposta HTTP
// Cria os cabeçalhos HTTP e envia para o cliente
void send_http_response(int fd, int status, const char* status_msg,
                       const char* content_type, const char* body, size_t body_len) {
    // Buffer para construir o cabeçalho HTTP
    char header[2048];
    
    // Construir o cabeçalho HTTP completo usando snprintf que evita buffer overflow
    int header_len = snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\r\n"           // Linha de status: versão + código + mensagem
        "Content-Type: %s\r\n"         // Tipo de conteúdo (MIME type) !!!
        "Content-Length: %zu\r\n"      // Tamanho do conteúdo em bytes
        "Server: ConcurrentHTTP/1.0\r\n"    // Identificação do servidor
        "Connection: close\r\n"          // Fechar ligação após resposta
        "\r\n",                             
        status, status_msg, content_type, body_len);
    

    // Enviar o cabeçalho para o cliente através do socket (0 é flags)
    send(fd, header, header_len, 0);
    
    // Se existir conteúdo, enviá-lo também
    // Verifica se body não é NULL e tem tamanho maior que 0
    if (body && body_len > 0) {
        send(fd, body, body_len, 0);  // Envia o conteúdo do mesmo
    }
}


// Função para determinar o tipo MIME baseado na extensão do ficheiro (IMPORTANTE!!)
// Usa a extensão do ficheiro para identificar o tipo de conteúdo
const char* get_mime_type(const char* filename) {
    // strrchr encontra a última ocorrência do caractere '.' na string
    // Isto dá-nos a extensão do ficheiro
    const char* ext = strrchr(filename, '.');
    if (!ext) return "text/plain";  // Sem extensão = texto simples
    
    // Comparar a extensão com tipos conhecidos
    // strcmp compara duas strings, retorna 0 se forem iguais
    
    if (strcmp(ext, ".html") == 0) return "text/html";           
    if (strcmp(ext, ".css") == 0) return "text/css";             
    if (strcmp(ext, ".js") == 0) return "application/javascript"; 
    if (strcmp(ext, ".png") == 0) return "image/png";            
    if (strcmp(ext, ".jpg") == 0) return "image/jpeg";          
    if (strcmp(ext, ".jpeg") == 0) return "image/jpeg";         
    
    // Tipo genérico para extensões desconhecidas
    // "application/octet-stream" = ficheiro binário genérico
    return "application/octet-stream";
}

// Função auxiliar para ler um ficheiro de erro da pasta errors/
// Esta função lê o conteúdo completo de um ficheiro HTML de erro
char* read_error_file(const char* document_root, const char* error_file) {
    char filepath[512];
    
    // Construir caminho completo para o ficheiro de erro
    // Exemplo: document_root = "./www", error_file = "404.html"
    // Resultado: "./www/errors/404.html"
    snprintf(filepath, sizeof(filepath), "%s/errors/%s", document_root, error_file);
    
    // Tentar abrir o ficheiro de erro
    FILE* file = fopen(filepath, "rb");
    if (!file) {
        // Se o ficheiro de erro personalizado não existir, usar fallback
        return NULL;
    }
    
    // Obter tamanho do ficheiro
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    if (file_size <= 0) {
        fclose(file);
        return NULL;
    }
    
    // Alocar memória para o conteúdo do ficheiro
    char* content = malloc(file_size + 1);
    if (!content) {
        fclose(file);
        return NULL;
    }
    
    // Ler conteúdo do ficheiro
    size_t bytes_read = fread(content, 1, file_size, file);
    fclose(file);
    
    if (bytes_read != (size_t)file_size) {
        free(content);
        return NULL;
    }
    
    content[file_size] = '\0';  // Adicionar terminador de string
    return content;
}

// Função para servir página de erro 404
// Primeiro tenta ler 404.html personalizado, depois usa fallback
void send_404_response(int client_fd, const char* document_root) {
    char* error_content = read_error_file(document_root, "404.html");
    
    if (error_content) {
        // Usar página de erro personalizada
        send_http_response(client_fd, 404, "Not Found", "text/html", 
                        error_content, strlen(error_content));
        free(error_content);
    } else {
        // Fallback: página de erro simples embutida no código
        const char* fallback_html = 
            "<!DOCTYPE html>\n"
            "<html>\n"
            "<head><title>404 Not Found</title></head>\n"
            "<body>\n"
            "<h1>404 Not Found</h1>\n"
            "<p>The requested resource was not found on this server.</p>\n"
            "</body>\n"
            "</html>";
        send_http_response(client_fd, 404, "Not Found", "text/html", 
                        fallback_html, strlen(fallback_html));
    }
}

// Função para servir página de erro 500
// Primeiro tenta ler 500.html personalizado, depois usa fallback
void send_500_response(int client_fd, const char* document_root) {
    char* error_content = read_error_file(document_root, "500.html");
    
    if (error_content) {
        // Usar página de erro personalizada
        send_http_response(client_fd, 500, "Internal Server Error", "text/html", 
                        error_content, strlen(error_content));
        free(error_content);
    } else {
        // Fallback: página de erro simples embutida no código
        const char* fallback_html = 
            "<!DOCTYPE html>\n"
            "<html>\n"
            "<head><title>500 Internal Server Error</title></head>\n"
            "<body>\n"
            "<h1>500 Internal Server Error</h1>\n"
            "<p>Something went wrong on the server.</p>\n"
            "</body>\n"
            "</html>";
        send_http_response(client_fd, 500, "Internal Server Error", "text/html", 
                        fallback_html, strlen(fallback_html));
    }
}

// Função para servir página de erro 403
// Primeiro tenta ler 403.html personalizado, depois usa fallback
void send_403_response(int client_fd, const char* document_root) {
    char* error_content = read_error_file(document_root, "403.html");
    
    if (error_content) {
        // Usar página de erro personalizada
        send_http_response(client_fd, 403, "Forbidden", "text/html", 
                        error_content, strlen(error_content));
        free(error_content);
    } else {
        // Fallback: página de erro simples embutida no código
        const char* fallback_html = 
            "<!DOCTYPE html>\n"
            "<html>\n"
            "<head><title>403 Forbidden</title></head>\n"
            "<body>\n"
            "<h1>403 Forbidden</h1>\n"
            "<p>You don't have permission to access this resource.</p>\n"
            "</body>\n"
            "</html>";
        send_http_response(client_fd, 403, "Forbidden", "text/html", 
                        fallback_html, strlen(fallback_html));
    }
}

// Função para servir página de erro 503
// Primeiro tenta ler 503.html personalizado, depois usa fallback
void send_503_response(int client_fd, const char* document_root) {
    char* error_content = read_error_file(document_root, "503.html");
    
    if (error_content) {
        // Usar página de erro personalizada
        send_http_response(client_fd, 503, "Service Unavailable", "text/html", 
                        error_content, strlen(error_content));
        free(error_content);
    } else {
        // Fallback: página de erro simples embutida no código
        const char* fallback_html = 
            "<!DOCTYPE html>\n"
            "<html>\n"
            "<head><title>503 Service Unavailable</title></head>\n"
            "<body>\n"
            "<h1>503 Service Unavailable</h1>\n"
            "<p>Server is currently overloaded. Please try again later.</p>\n"
            "</body>\n"
            "</html>";
        send_http_response(client_fd, 503, "Service Unavailable", "text/html", 
                        fallback_html, strlen(fallback_html));
    }
}