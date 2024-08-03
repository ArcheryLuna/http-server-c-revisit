// handler.c
#include "handle_client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>

#define BUFFER_SIZE 1024

extern char *OK_RESPONSE;
extern char *CREATED_RESPONSE;
extern char *NOT_FOUND_RESPONSE;
extern char *BAD_REQUEST_RESPONSE;
extern char directory_path[BUFFER_SIZE];

void handle_client(int client_socket) {
    char buffer[BUFFER_SIZE];
    int bytes_read = read(client_socket, buffer, BUFFER_SIZE);
    if (bytes_read < 0) {
        perror("read");
        close(client_socket);
        return;
    }
    
    buffer[bytes_read] = '\0';
    
    // Simple request parsing to extract the method and URL
    char *method_end = strchr(buffer, ' ');
    if (method_end == NULL) {
        write(client_socket, BAD_REQUEST_RESPONSE, strlen(BAD_REQUEST_RESPONSE));
        close(client_socket);
        return;
    }
    
    int method_length = method_end - buffer;
    char method[method_length + 1];
    strncpy(method, buffer, method_length);
    method[method_length] = '\0';

    char *url_start = method_end + 1;
    char *url_end = strchr(url_start, ' ');
    if (url_end == NULL) {
        write(client_socket, BAD_REQUEST_RESPONSE, strlen(BAD_REQUEST_RESPONSE));
        close(client_socket);
        return;
    }

    int url_length = url_end - url_start;
    char path[url_length + 1];
    strncpy(path, url_start, url_length);
    path[url_length] = '\0';

    // Check for Accept-Encoding header
    char *accept_encoding = strstr(buffer, "Accept-Encoding: ");
    int supports_gzip = 0;
    if (accept_encoding) {
        accept_encoding += strlen("Accept-Encoding: ");
        char *accept_end = strstr(accept_encoding, "\r\n");
        if (accept_end) {
            int accept_length = accept_end - accept_encoding;
            char encoding[accept_length + 1];
            strncpy(encoding, accept_encoding, accept_length);
            encoding[accept_length] = '\0';

            if (strstr(encoding, "gzip")) {
                supports_gzip = 1;
            }
        }
    }

    if (strcmp(method, "GET") == 0) {
        if (strcmp(path, "/") == 0) {
            // Handle root path
            write(client_socket, OK_RESPONSE, strlen(OK_RESPONSE));
        } else if (strncmp(path, "/files/", 7) == 0) {
            // Handle /files/{filename} path for GET method
            char *filename = path + 7;
            char full_path[BUFFER_SIZE];
            snprintf(full_path, sizeof(full_path), "%s/%s", directory_path, filename);

            struct stat st;
            if (stat(full_path, &st) != 0) {
                // File does not exist
                write(client_socket, NOT_FOUND_RESPONSE, strlen(NOT_FOUND_RESPONSE));
                close(client_socket);
                return;
            }

            FILE *file = fopen(full_path, "rb");
            if (!file) {
                // Could not open file
                write(client_socket, NOT_FOUND_RESPONSE, strlen(NOT_FOUND_RESPONSE));
                close(client_socket);
                return;
            }

            // Read file contents
            char *file_contents = malloc(st.st_size);
            if (!file_contents) {
                // Memory allocation failed
                fclose(file);
                write(client_socket, NOT_FOUND_RESPONSE, strlen(NOT_FOUND_RESPONSE));
                close(client_socket);
                return;
            }

            fread(file_contents, 1, st.st_size, file);
            fclose(file);

            // Prepare the HTTP response
            char response[BUFFER_SIZE];
            int response_length = snprintf(response, sizeof(response),
                                           "HTTP/1.1 200 OK\r\n"
                                           "Content-Type: application/octet-stream\r\n"
                                           "Content-Length: %ld\r\n"
                                           "\r\n",
                                           st.st_size);

            // Send the headers
            write(client_socket, response, response_length);
            // Send the file contents
            write(client_socket, file_contents, st.st_size);

            free(file_contents);
        } else if (strncmp(path, "/echo/", 6) == 0) {
            // Handle /echo/{str} path
            char *echo_string = path + 6;
            char response[BUFFER_SIZE];
            int response_length;

            if (supports_gzip) {
                response_length = snprintf(response, sizeof(response),
                                           "HTTP/1.1 200 OK\r\n"
                                           "Content-Type: text/plain\r\n"
                                           "Content-Encoding: gzip\r\n"
                                           "Content-Length: %ld\r\n"
                                           "\r\n"
                                           "%s",
                                           strlen(echo_string), echo_string);
            } else {
                response_length = snprintf(response, sizeof(response),
                                           "HTTP/1.1 200 OK\r\n"
                                           "Content-Type: text/plain\r\n"
                                           "Content-Length: %ld\r\n"
                                           "\r\n"
                                           "%s",
                                           strlen(echo_string), echo_string);
            }

            write(client_socket, response, response_length);
        } else if (strcmp(path, "/user-agent") == 0) {
            // Handle /user-agent path
            char *user_agent_start = strstr(buffer, "User-Agent: ");
            if (user_agent_start != NULL) {
                user_agent_start += strlen("User-Agent: ");
                char *user_agent_end = strstr(user_agent_start, "\r\n");
                if (user_agent_end != NULL) {
                    int user_agent_length = user_agent_end - user_agent_start;
                    char user_agent[user_agent_length + 1];
                    strncpy(user_agent, user_agent_start, user_agent_length);
                    user_agent[user_agent_length] = '\0';

                    char response[BUFFER_SIZE];
                    int response_length = snprintf(response, sizeof(response),
                                                   "HTTP/1.1 200 OK\r\n"
                                                   "Content-Type: text/plain\r\n"
                                                   "Content-Length: %d\r\n"
                                                   "\r\n"
                                                   "%s",
                                                   user_agent_length, user_agent);

                    write(client_socket, response, response_length);
                } else {
                    write(client_socket, BAD_REQUEST_RESPONSE, strlen(BAD_REQUEST_RESPONSE));
                }
            } else {
                write(client_socket, BAD_REQUEST_RESPONSE, strlen(BAD_REQUEST_RESPONSE));
            }
        } else {
            // Handle other paths
            write(client_socket, NOT_FOUND_RESPONSE, strlen(NOT_FOUND_RESPONSE));
        }
    } else if (strcmp(method, "POST") == 0) {
        if (strncmp(path, "/files/", 7) == 0) {
            // Handle /files/{filename} path for POST method
            char *filename = path + 7;
            char full_path[BUFFER_SIZE];
            snprintf(full_path, sizeof(full_path), "%s/%s", directory_path, filename);

            // Find Content-Length
            char *content_length_start = strstr(buffer, "Content-Length: ");
            if (content_length_start == NULL) {
                write(client_socket, BAD_REQUEST_RESPONSE, strlen(BAD_REQUEST_RESPONSE));
                close(client_socket);
                return;
            }

            content_length_start += strlen("Content-Length: ");
            char *content_length_end = strstr(content_length_start, "\r\n");
            if (content_length_end == NULL) {
                write(client_socket, BAD_REQUEST_RESPONSE, strlen(BAD_REQUEST_RESPONSE));
                close(client_socket);
                return;
            }

            int content_length = atoi(content_length_start);

            // Find the start of the body
            char *body_start = strstr(buffer, "\r\n\r\n");
            if (body_start == NULL) {
                write(client_socket, BAD_REQUEST_RESPONSE, strlen(BAD_REQUEST_RESPONSE));
                close(client_socket);
                return;
            }
            body_start += 4; // Move past \r\n\r\n

            // Write the body to the file
            FILE *file = fopen(full_path, "wb");
            if (!file) {
                write(client_socket, BAD_REQUEST_RESPONSE, strlen(BAD_REQUEST_RESPONSE));
                close(client_socket);
                return;
            }

            fwrite(body_start, 1, content_length, file);
            fclose(file);

            // Respond with 201 Created
            write(client_socket, CREATED_RESPONSE, strlen(CREATED_RESPONSE));
        } else {
            write(client_socket, NOT_FOUND_RESPONSE, strlen(NOT_FOUND_RESPONSE));
        }
    } else {
        write(client_socket, BAD_REQUEST_RESPONSE, strlen(BAD_REQUEST_RESPONSE));
    }

    close(client_socket);
}

