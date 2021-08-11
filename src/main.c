#include <errno.h>
#include <stdlib.h>
#include <stdio.h>

#include <string.h>
#include <sys/time.h>

#include <netdb.h>
#include <unistd.h>
#include <netinet/in.h>

#include "../include/websocket.h"

#define PORT 8000
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

	FILE* log_file;

	// accept loop
	for(;;) {
		int client_fd;
		struct sockaddr_in client_address;
		socklen_t client_address_length = sizeof(client_address);

		client_fd = accept(server_fd, (struct sockaddr*) &client_address, &client_address_length);
		printf("Client found!\n");

		struct timeval start, stop;
		gettimeofday(&start, NULL);
		int res = ws_handshake(client_fd);

		if(!res) {
			printf("Handshake failed\n");
			close(server_fd);
			close(client_fd);
			exit(EXIT_FAILURE);
		}

		Dataframe dataframe;
		ws_read(client_fd, &dataframe);
		log_file = fopen("logs.txt", "a");

		printf("message: %s\n", dataframe.payload);
		printf("payload length: %lu\n", dataframe.payload_length);

		gettimeofday(&stop, NULL);
		float time_taken = ((stop.tv_sec - start.tv_sec) + (stop.tv_usec - start.tv_usec) / 1000000.f);
		printf("took: %f seconds\n", time_taken);

		close(client_fd);
		fprintf(log_file, "%f\n", time_taken);

		Dataframe_free(&dataframe);
		fclose(log_file);
	}

	close(server_fd);

	return 0;
}
