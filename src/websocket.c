#include <stdlib.h>
#include <stdio.h>

#ifdef __WIN32
#include <winsock.h>
#include <winsock2.h>
#endif

#ifdef __unix
#include <errno.h>
#include <netdb.h>
#include <unistd.h>
#include <netinet/in.h>
#include <endian.h>
#endif

#include <openssl/sha.h>
#include <string.h>

#include "../include/websocket.h"


//------------------------------------------------------------------------------------------------------------------------


// YOyoyo wassup man, so the problem is probably related to the way that websocket fragmentation works,
// I think maybe the first payload length shouldn't be read or decoded until the entire sequence is finished.
// But the documentation I read isn't even specific, it just says that I'm supposed to "concadenate" the payload
// no idea what to do


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
	char dummy_buf;
	if(!recv(socket_fd, &dummy_buf, 1, MSG_PEEK)) {
		perror("Client not connected, on ws_read function");
		return -1;
	}

	byte header[2];
	byte* payload = NULL;
	uint64_t payload_length = 0;
	byte opcode;
	byte mask_key[4];

	do {
		header[0] = 0;
		header[1] = 0;
		recv(socket_fd, &header, 2, 0);
		uint64_t length = header[1] & 127;

		printf("Opcode is: %i\n", header[0] & 15);

		// find length of payload
		if(length == 126) {
			length = 0;
			recv(socket_fd, &length, 2, 0);
			
			length = ntohs(length);
		}
		else if(length == 127) {
			length = 0;
			recv(socket_fd, &length, 8, 0);

#ifdef __WIN32
			length = ntohll(length);
#endif
#ifdef __unix
			length = be64toh(length);
#endif
		}

		// extract mask key
		if(header[1] >> 7 & 1) {
			recv(socket_fd, &mask_key, 4, 0);
			printf("masked payload\n");
		}

		// extract payload
		if(payload == NULL) {
			payload = (byte*) malloc(sizeof(byte) * length);
			opcode = header[0] & 15;
		}
		else {
			printf("Fragmented message\n");
			payload = (byte*) realloc(payload, sizeof(byte) * (payload_length + length));
		}

		recv(socket_fd, &payload[payload_length], length, 0);

		printf("local payload: %lu\n", length);
		printf("total payload: %lu\n", payload_length + length);

		// if the payload is masked do the conversion
		if(header[1] >> 7 & 1)
			for(uint64_t i = 0; i < length; i++)
				payload[payload_length + i] = payload[payload_length + i] ^ mask_key[i % 4];

		// pings have to be ANSWERED with pongs
		if(opcode == OPCODE_PING)
			ws_send(socket_fd, OPCODE_PONG, payload, length);
		payload_length += length;
	}
	// if FIN bit was set, then the message is fragmented (composed of multiple frames)
	while(!((header[0] >> 7) & 1));

	data->payload_length = payload_length;
	data->payload = payload;
	return 1;
}


//------------------------------------------------------------------------------------------------------------------------


int ws_send(int socket_fd, Opcode type, byte* data, uint64_t bytes) {
	byte header[2];
	header[0] = 0x80;
	header[0] = header[0] | (byte) type;

	// assumes message is never fragmented

	uint64_t payload_length_bytes = 0;
	payload_length_bytes += (bytes > 125 && bytes <= 0xffff) * 0xffff;
	payload_length_bytes += (bytes > 0xffff && bytes <= 0xffffffffffffffff) * 0xffffffffffffffff;

	byte payload[payload_length_bytes + bytes];

	if(bytes < 126) {
		header[1] = bytes;
	}
	else if(bytes < 65536) {
		header[1] = 126;
		payload[0] = bytes;
		payload_length_bytes = 2;
	}
	else {
		header[1] = 127;
		payload[0] = bytes;
		payload_length_bytes = 8;
	}

	for(uint64_t i = 0; i < bytes; i++)
		payload[payload_length_bytes + i] = data[i];

	uint64_t payload_length_buf = 0;

#ifdef __WIN32
	payload_length_buf = htonll(bytes);
#endif
#ifdef __unix
	payload_length_buf = htobe64(bytes);
#endif

	printf("header: %x, %x\n", header[0], header[1]);
	printf("Here is payload length: %lu, byte amount: %lu\n", payload_length_bytes, bytes);
	send(socket_fd, header, 2, MSG_NOSIGNAL);
	send(socket_fd, &payload_length_buf, payload_length_bytes, MSG_NOSIGNAL);
	send(socket_fd, payload, bytes, MSG_NOSIGNAL);

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
