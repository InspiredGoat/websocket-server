#include <stdlib.h>
#include <stdio.h>

#include <string.h>

#include <netdb.h>
#include <unistd.h>
#include <netinet/in.h>

#define PORT 3000
#define MAX_CLIENTS 10

int main() {
	int server_fd;
	struct sockaddr_in address;

	server_fd = socket(AF_INET, SOCK_STREAM, 0);

	bzero(&address, sizeof(struct sockaddr_in));
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(PORT);

	int opt = 1;

	// make port reusable
	setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));

	// make sure sockets are connected
	setsockopt(server_fd, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(opt));

	// bind
	if(bind(server_fd, (struct sockaddr*) &address, sizeof(address)) != 0) {
		perror("failed to bind to port");
		exit(1);
	}
	
	// listen
	if(listen(server_fd, MAX_CLIENTS) != 0) {
		perror("failed to listen to port");
		exit(1);
	}
	
	printf("Started server, listening at port: %i\n", PORT);

	// accept loop
	int client_fd;
	struct sockaddr_in client_address;
	unsigned int client_address_length;

	client_fd = accept(server_fd, (struct sockaddr*) &client_address, &client_address_length);

	char buf[10000];
	if(!read(client_fd, &buf, 10000))
		printf("reached end of message\n");

	printf("Received:\n %s\n", buf);

	close(client_fd);

	// actual websocket part starts
	
	char message[] = "hello\0";
	send(client_fd, message, sizeof(char) * 6, MSG_NOSIGNAL);
	
	close(server_fd);

	return 0;
}
