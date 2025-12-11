#ifndef HTTP_H
#define HTTP_H // I'm using include guards to prevent multiple inclusions.

#include <stddef.h> // I need size_t from here.

// This structure represents an HTTP request from a client.
// I'm keeping it simple with just the essentials for my static file server.
typedef struct
{
    char method[16];   // I store the HTTP method like "GET", "POST", or "HEAD".
    char path[512];    // I need enough space for file paths like "/folder/file.html".
    char version[16];  // I store the HTTP version like "HTTP/1.0" or "HTTP/1.1".
} http_request_t;

// I need a function to parse raw HTTP request data.
// This takes the buffer received from the client socket and extracts the request details.
int parse_http_request(const char *buffer, http_request_t *req);

// I need a function to send HTTP responses back to clients.
// This builds proper HTTP headers and sends the response body.
void send_http_response(int fd, int status, const char *status_msg, 
                        const char *content_type, const char *body, size_t body_len);

#endif // HTTP_H