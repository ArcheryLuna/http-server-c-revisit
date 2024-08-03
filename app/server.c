#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include "handler.h"

#define BUFFER_SIZE 1024

char *OK_RESPONSE = "HTTP/1.1 200 OK\r\n\r\n";
char *CREATED_RESPONSE = "HTTP/1.1 201 Created\r\n\r\n";
char *NOT_FOUND_RESPONSE = "HTTP/1.1 404 Not Found\r\n\r\n";
char *BAD_REQUEST_RESPONSE = "HTTP/1.1 400 Bad Request\r\n\r\n";

char directory_path[BUFFER_SIZE] = "."; // Default directory path

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

