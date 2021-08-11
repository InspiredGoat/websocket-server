#include <stdlib.h>
#include <stdio.h>

#include <string.h>

#include <netdb.h>
#include <unistd.h>
#include <netinet/in.h>

#include <openssl/sha.h>

#define PORT 3000
#define MAX_CLIENTS 10

typedef unsigned char byte;

char base64_alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

int power(int x, int power) {
	int res = x;

	for(int i = 0; i < power; i++)
		res *= x;

	return res;
}

// output needs to be 28 bytes, data needs to be 20 bytes
void hash_to_base64(byte* data, char* output) {
	byte current_bit = 0;
	byte group_count = 1;
	byte index = 0;

	// there's probably a better way to implement this that is way more clever but its 11
	while(current_bit < 156) {
		byte current_byte = current_bit / 8;
		byte bit_value = (data[current_byte] >> (7 - (current_bit % 8))) & 1;

		index <<= 1;
		index |= bit_value;

		// reached end of group
		if(group_count == 6) {
			if(index > 64)
				printf("biggest value: %i, at %i, %i\n", index, current_byte, current_bit);

			output[current_bit / 6] = base64_alphabet[index];
			group_count = 0;
			index = 0;
		}

		group_count++;
		current_bit++;
	}

	// do the special 16 case

	// 0000 1111
	index = (data[19] & 15) << 2;
	output[26] = base64_alphabet[index];
	output[27] = '=';
}

void handshake_key(const char* key, char* output) {
	char magic_string[60];
	strcpy(magic_string, key);
	strcat(magic_string, "258EAFA5-E914-47DA-95CA-C5AB0DC85B11");

	printf("magic string: %s\n", magic_string);

	unsigned char hash[20];
	SHA1((const unsigned char*) magic_string, 60, hash);

	hash_to_base64(hash, output);
}

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

	char request[1500];
	read(client_fd, &request, 1500);

	printf("Received:\n\n%s\n", request);

	
	char upgrade_accept[] = "Upgrade: websocket\0";
	char* res = strstr(request, upgrade_accept);

	// not a websocket connection
	if(!res) {
		printf("client is a total poser\n");
		char sass[] = "websockets only fucko";
		send(client_fd, &sass, 21, MSG_NOSIGNAL);
	}
	else {
		printf("New connection\n");
		// find keys and shit

		char request_key_accept[] = "Sec-WebSocket-Key: ";
		char* request_key_loc = strstr(request, request_key_accept) + 19;

		char request_key[25];

		strncpy(request_key, request_key_loc, 24);
		request_key[24] = '\0';

		printf("Request key: %s\n", request_key);

		char response_key[29];
		handshake_key(request_key, response_key);
		response_key[28] = '\0';

		printf("Response key: %s\n", response_key);
	}
	
	close(client_fd);
	close(server_fd);

	return 0;
}
