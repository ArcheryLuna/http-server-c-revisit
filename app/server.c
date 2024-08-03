#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#define BUFFER_SIZE 1024
char *OK_RESPONSE = "HTTP/1.1 200 OK\r\n\r\n";
char *NOT_FOUND_RESPONSE = "HTTP/1.1 404 Not Found\r\n\r\n";

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
        close(client_socket);
        return;
    }
    
    start += 4; // Move past "GET "
    char *end = strchr(start, ' ');
    if (end == NULL) {
        close(client_socket);
        return;
    }
    
    int length = end - start;
    char path[length + 1];
    strncpy(path, start, length);
    path[length] = '\0';
    
    char response[BUFFER_SIZE];
    if (strcmp(path, "/") == 0) {
        // Handle root path
        sprintf(response, 
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: 12\r\n"
            "\r\n"
            "Hello, world");
    } else if (strncmp(path, "/echo/", 6) == 0) {
        // Handle /echo/{str} path
        char *echo_string = path + 6;
        sprintf(response, 
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: %ld\r\n"
            "\r\n"
            "%s",
            strlen(echo_string), echo_string);
    } else if (strcmp(path, "/user-agent") == 0) {
        // Handle /user-agent path
        char *user_agent = strstr(buffer, "User-Agent: ");
        if (user_agent != NULL) {
            user_agent += 12; // Move past "User-Agent: "
            char *ua_end = strstr(user_agent, "\r\n");
            if (ua_end != NULL) {
                int ua_length = ua_end - user_agent;
                char ua_value[ua_length + 1];
                strncpy(ua_value, user_agent, ua_length);
                ua_value[ua_length] = '\0';
                
                sprintf(response,
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Type: text/plain\r\n"
                    "Content-Length: %d\r\n"
                    "\r\n"
                    "%s",
                    ua_length, ua_value);
            } else {
                sprintf(response, 
                    "HTTP/1.1 400 Bad Request\r\n"
                    "Content-Type: text/plain\r\n"
                    "Content-Length: 15\r\n"
                    "\r\n"
                    "400 Bad Request");
            }
        } else {
            sprintf(response, 
                "HTTP/1.1 400 Bad Request\r\n"
                "Content-Type: text/plain\r\n"
                "Content-Length: 15\r\n"
                "\r\n"
                "400 Bad Request");
        }
    } else {
        // Handle other paths
        sprintf(response, 
            "HTTP/1.1 404 Not Found\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: 13\r\n"
            "\r\n"
            "404 Not Found");
    }
    
    write(client_socket, response, strlen(response));
    close(client_socket);
}

int main() {
    // Disable output buffering
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);

    // You can use print statements as follows for debugging, they'll be visible when running tests.
    printf("Logs from your program will appear here!\n");

    // Uncomment this block to pass the first stage
    //
    int server_fd, client_socket;
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    //
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        printf("Socket creation failed: %s...\n", strerror(errno));
        return 1;
    }
    //
    // // Since the tester restarts your program quite often, setting SO_REUSEADDR
    // // ensures that we don't run into 'Address already in use' errors
    int reuse = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        printf("SO_REUSEADDR failed: %s \n", strerror(errno));
        return 1;
    }

    struct sockaddr_in serv_addr = { .sin_family = AF_INET ,
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

