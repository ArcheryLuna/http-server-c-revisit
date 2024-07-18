#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>

#define BUFFER_SIZE 1024
char *OK_RESPONSE = "HTTP/1.1 200 OK\r\n\r\n";
char *NOT_FOUND_RESPONSE = "HTTP/1.1 404 Not Found\r\n\r\n";

int main() {
	// Disable output buffering
	setbuf(stdout, NULL);
 	setbuf(stderr, NULL);

	// You can use print statements as follows for debugging, they'll be visible when running tests.
	printf("Logs from your program will appear here!\n");

	// Uncomment this block to pass the first stage
	//
	int server_fd, client_addr_len;
	struct sockaddr_in client_addr;
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
	
	printf("Waiting for a client to connect...\n");
	client_addr_len = sizeof(client_addr);
    
    int fd = accept(server_fd, (struct sockaddr *) &client_addr, &client_addr_len);
    
    if ( fd < 0 ) {
        printf("Connection Failed %s \n", strerror(errno));
        return 1;
    }

	printf("Client connected\n");
    
    char request_buffer[BUFFER_SIZE];

    if (read(fd, request_buffer, BUFFER_SIZE) < 0) {
        printf("Read Failed: %s \n", strerror(errno));
        return 1;
    } else {
        printf("Request from client %s \n", request_buffer);
    }

    

    char *path = strtok(request_buffer, " ");
    path = strtok(NULL, " ");

    char *response = (strcmp(path, "/") == 0) ? OK_RESPONSE : NOT_FOUND_RESPONSE;

    if (send(fd, response, strlen(response), 0) < 0) {
        printf("Error: %s \n", strerror(errno));
    }


	close(server_fd);

	return 0;
}
