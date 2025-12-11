#include <stdio.h>      
#include <string.h>     
#include <sys/socket.h> 
#include <time.h>
#include "http.h"

// I'm parsing an HTTP request from a client.
// This function takes the raw request buffer and extracts the important parts.
int parse_http_request(const char *buffer, http_request_t *req)
{
    // First, I need to find where the first line of the request ends.
    // HTTP uses \r\n (carriage return + line feed) to separate lines.
    char *line_end = strstr(buffer, "\r\n");

    if (!line_end)
        return -1; // If there's no \r\n, the request is incomplete or malformed.

    // I'll copy just the first line to a temporary buffer so I can work with it safely.
    char first_line[1024];
    size_t len = line_end - buffer; // This calculates how long the first line is.
    
    // I need to make sure I don't overflow my buffer.
    if (len >= sizeof(first_line))
        len = sizeof(first_line) - 1; // Leave room for the null terminator.
        
    strncpy(first_line, buffer, len);
    first_line[len] = '\0'; // Always null-terminate!

    // Now I need to extract the three parts of the request line:
    // 1. Method (like GET, POST, HEAD)
    // 2. Path (like /index.html or /images/photo.jpg)
    // 3. HTTP Version (like HTTP/1.1)
    // I use sscanf because it's perfect for this simple format.
    if (sscanf(first_line, "%s %s %s", req->method, req->path, req->version) != 3)
    {
        return -1; // If I couldn't get all three parts, the request is malformed.
    }

    return 0; // Success!
}

// I'm sending an HTTP response back to the client.
// This function builds a proper HTTP response with headers and body.
void send_http_response(int fd, int status, const char *status_msg, const char *content_type, const char *body, size_t body_len)
{
    // 1. First, I need to generate the current date in the format HTTP requires.
    // HTTP responses must include a Date header in GMT timezone.
    time_t now = time(NULL);
    struct tm tm_data;
    gmtime_r(&now, &tm_data); // I use gmtime_r because it's thread-safe.

    char date_str[128];
    // I format the date according to RFC 1123, which is what HTTP expects.
    strftime(date_str, sizeof(date_str), "%a, %d %b %Y %H:%M:%S GMT", &tm_data);

    // 2. Now I'll build the complete HTTP response header.
    // I'm using snprintf because it's safe - it won't overflow my buffer.
    char header[2048];
    int header_len = snprintf(header, sizeof(header),
                              "HTTP/1.1 %d %s\r\n"          // Status line
                              "Date: %s\r\n"               // Current date
                              "Content-Type: %s\r\n"       // What kind of data I'm sending
                              "Content-Length: %zu\r\n"    // How many bytes are in the body
                              "Server: ConcurrentHTTP/1.0\r\n" // My server name
                              "Connection: close\r\n"      // I'll close the connection after this
                              "\r\n",                      // Empty line marks end of headers
                              status, status_msg, 
                              date_str,                     
                              content_type, body_len);

    // 3. Send the header to the client.
    send(fd, header, header_len, 0);

    // 4. If there's a body (and it's not a HEAD request), send it too.
    // The body_len check ensures I don't try to send empty data.
    if (body && body_len > 0)
    {
        send(fd, body, body_len, 0);
    }
}