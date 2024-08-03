#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>

#define BUFFER_SIZE 1024

char *OK_RESPONSE = "HTTP/1.1 200 OK\r\n\r\n";
char *NOT_FOUND_RESPONSE = "HTTP/1.1 404 Not Found\r\n\r\n";
char *BAD_REQUEST_RESPONSE = "HTTP/1.1 400 Bad Request\r\n\r\n";

char directory_path[BUFFER_SIZE] = "."; // Default directory path

void handle_client(int client_socket) {
    char buffer[BUFFER_SIZE];
    int bytes_read = read(client_socket, buffer, BUFFER_SIZE);
    if (bytes_read < 0) {
        perror("read");
        close(client_socket);
        return;
    }
    
    buffer[bytes_read] = '\0';
    
    // Simple request parsing to extract the string from URL
    char *start = strstr(buffer, "GET ");
    if (start == NULL) {
        write(client_socket, BAD_REQUEST_RESPONSE, strlen(BAD_REQUEST_RESPONSE));
        close(client_socket);
        return;
    }
    
    start += 4; // Move past "GET "
    char *end = strchr(start, ' ');
    if (end == NULL) {
        write(client_socket, BAD_REQUEST_RESPONSE, strlen(BAD_REQUEST_RESPONSE));
        close(client_socket);
        return;
    }
    
    int length = end - start;
    char path[length + 1];
    strncpy(path, start, length);
    path[length] = '\0';

    if (strcmp(path, "/") == 0) {
        // Handle root path
        write(client_socket, OK_RESPONSE, strlen(OK_RESPONSE));
    } else if (strncmp(path, "/files/", 7) == 0) {
        // Handle /files/{filename} path
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
        int response_length = snprintf(response, sizeof(response),
                                       "HTTP/1.1 200 OK\r\n"
                                       "Content-Type: text/plain\r\n"
                                       "Content-Length: %ld\r\n"
                                       "\r\n"
                                       "%s",
                                       strlen(echo_string), echo_string);

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

    close(client_socket);
}

int main(int argc, char *argv[]) {
    // Parse command-line arguments to get the directory path
    if (argc == 3 && strcmp(argv[1], "--directory") == 0) {
        strncpy(directory_path, argv[2], sizeof(directory_path) - 1);
        directory_path[sizeof(directory_path) - 1] = '\0';
    } else if (argc != 1) {  // Only print usage if arguments are provided incorrectly
        fprintf(stderr, "Usage: %s [--directory <path>]\n", argv[0]);
        return 1;
    }

    // Disable output buffering
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);

    // You can use print statements as follows for debugging, they'll be visible when running tests.
    printf("Logs from your program will appear here!\n");

    int server_fd, client_socket;
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        printf("Socket creation failed: %s...\n", strerror(errno));
        return 1;
    }

    int reuse = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        printf("SO_REUSEADDR failed: %s \n", strerror(errno));
        return 1;
    }

    struct sockaddr_in serv_addr = { .sin_family = AF_INET,
                                     .sin_port = htons(4221),
                                     .sin_addr = { htonl(INADDR_ANY) },
                                   };
    
    if (bind(server_fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) != 0) {
        printf("Bind failed: %s \n", strerror(errno));
        return 1;
    }

    int connection_backlog = 5;
    if (listen(server_fd, connection_backlog) != 0) {
        printf("Listen failed: %s \n", strerror(errno));
        return 1;
    }
    
    while (1) {
        client_socket = accept(server_fd, (struct sockaddr*)&client_addr, &client_addr_len);

        if (client_socket < 0) {
            perror("accept");
            continue;
        }

        if (fork() == 0) {
            // Child process
            close(server_fd);
            handle_client(client_socket);
            exit(0);
        } else {
            // Parent process
            close(client_socket);
        }
    }

    close(server_fd);

    return 0;
}

