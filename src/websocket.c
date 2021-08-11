#include <errno.h>
#include <stdlib.h>
#include <stdio.h>

#include <string.h>

#include <netdb.h>
#include <unistd.h>
#include <netinet/in.h>

#include <openssl/sha.h>

#include "../include/websocket.h"


//------------------------------------------------------------------------------------------------------------------------


int power(int x, int power) {
	int res = x;

	for(int i = 0; i < power; i++)
		res *= x;

	return res;
}


//------------------------------------------------------------------------------------------------------------------------


char base64_alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
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


//------------------------------------------------------------------------------------------------------------------------


void handshake_key(const char* key, char* output) {
	char magic_string[60];
	strcpy(magic_string, key);
	strcat(magic_string, "258EAFA5-E914-47DA-95CA-C5AB0DC85B11");

	unsigned char hash[20];
	SHA1((const unsigned char*) magic_string, 60, hash);

	hash_to_base64(hash, output);
}


//------------------------------------------------------------------------------------------------------------------------


int ws_read(int socket_fd, Dataframe* data) {
	if(!recv(socket_fd, NULL, 1, MSG_PEEK)) {
		perror("Client not connected, on ws_read function");
		return -1;
	}

	byte header[2];
	recv(socket_fd, &header, 2, 0);

	if(header[1] >> 7) {
		byte opcode = header[0] & 15;
		uint64_t payload_length = header[1] & 127;

		// find length of payload
		if(payload_length == 126) {
			payload_length = 0;

			recv(socket_fd, &payload_length, 16, 0);
			payload_length = payload_length << 16;
			printf("payload: %lu\n", payload_length);
		}
		else if(payload_length == 127) {
			payload_length = 0;

			recv(socket_fd, &payload_length, 64, 0);
		}

		// extract mask key
		byte mask_key[4];
		recv(socket_fd, &mask_key, sizeof(byte) * 4, 0);

		// extract payload
		byte* payload = (byte*) malloc(sizeof(byte) * payload_length);
		recv(socket_fd, payload, sizeof(byte) * payload_length, 0);

		// decifer payload
		for(int i = 0; i < payload_length; i++) {
			payload[i] = payload[i] ^ mask_key[i % 4];
		}

		// if FIN bit is enabled this a fragmented message
		while(!header[0] >> 7) {
			recv(socket_fd, &header, 2, 0);
			
			uint64_t length = header[1] & 127;

			if(length == 126) {
				length = 0;

				recv(socket_fd, &length, 16, 0);
				length = length >> 48;
			}
			else if(length == 127) {
				length = 0;

				recv(socket_fd, &length, 64, 0);
			}

			recv(socket_fd, &mask_key, sizeof(byte) * 4, 0);

			payload = (byte*) realloc(payload, sizeof(byte) * (payload_length * length));
			recv(socket_fd, payload + payload_length, length, 0);

			for(int i = 0; i < length; i++) {
				payload[payload_length + i] = payload[payload_length + i] ^ mask_key[i % 4];
			}

			payload_length += length;
		}

		data->opcode = opcode;
		data->payload_length = payload_length;
		data->payload = payload;
		return 1;
	}
	else
		return -1;
}


//------------------------------------------------------------------------------------------------------------------------


int ws_send(int socket_fd, Opcode type, byte* data, uint64_t bytes) {
	byte header[2];
	header[0] = (byte) type;

	uint64_t payload_length_bytes = 0;
	payload_length_bytes += (bytes > 125 && bytes <= 0xffff) * 0xffff;
	payload_length_bytes += (bytes > 0xffff && bytes <= 0xffffffffffffffff) * 0xffffffffffffffff;

	byte payload[payload_length_bytes + bytes];

	if(bytes < 126) {
		header[1] = bytes;
	}
	else if(bytes < 65536) {
		header[1] = 126;
		payload[0] = bytes << bytes;
	}
	else {
		header[1] = 127;
		payload[0] = bytes << bytes;
	}

	for(uint64_t i = 0; i < bytes; i++)
		payload[payload_length_bytes + i] = data[i];

	printf("Here is payload length: %lu, byte amount: %lu\n", payload_length_bytes, bytes);
	send(socket_fd, payload, payload_length_bytes + bytes, MSG_NOSIGNAL);

	return 1;
}


//------------------------------------------------------------------------------------------------------------------------


void Dataframe_free(Dataframe* dataframe) {
	free(dataframe->payload);
}


//------------------------------------------------------------------------------------------------------------------------


int ws_handshake(int socket_fd) {
	if(errno > 0) {
		printf("%s\n", strerror(errno));
		printf("error before handshake\n");
		exit(-1);
	}

	printf("handshake initiated!\n");

	char request[500];

	recv(socket_fd, &request, 500, 0);
	if(errno > 0) {
		printf("%s\n", strerror(errno));
		exit(-1);
	}

	printf("REQUEST:\n");
	printf("%s\n", request);
	char upgrade_accept[] = "Upgrade: websocket\0";
	char* res = strstr(request, upgrade_accept);

	if(!res) {
		send(socket_fd, "Sorry lad, websockets ONLY!", 26, MSG_NOSIGNAL);
		return -1;
	}

	else {
		char request_key_accept[] = "Sec-WebSocket-Key: ";
		char* request_key_loc = strstr(request, request_key_accept) + 19;

		char request_key[25];

		strncpy(request_key, request_key_loc, 24);
		request_key[24] = '\0';

		char response_key[29];
		handshake_key(request_key, response_key);
		response_key[28] = '\0';

		char response[137]; 
		bzero(response, sizeof(response));
		strcat(response, "HTTP/1.1 101 Switching Protocols\r\n");
		strcat(response, "Upgrade: websocket\r\n");
		strcat(response, "Connection: Upgrade\r\n");
		strcat(response, "Sec-WebSocket-Accept: ");
		strcat(response, response_key);
		strcat(response, "\r\n");
		strcat(response, "\r\n");

		printf("RESPONSE:\n%s\n", response);
		printf("response key: %s\n", response_key);
		
		send(socket_fd, response, 129, MSG_NOSIGNAL);

		printf("handshake completed!\n");

		return 1;
	}
}
