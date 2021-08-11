#ifndef WEBSOCKET_H
#define WEBSOCKET_H
#include <stdint.h>
// library doesn't deal with clients, leaves that up to you! (you're welcome)

// Type of data being transmitted by websocket
typedef unsigned char byte;

typedef enum {
	OPCODE_CONTINUATION=0,
	OPCODE_TEXT=1,
	OPCODE_BINARY=2,
	OPCODE_PING=9,
	OPCODE_PONG=10
} Opcode;


// Structure of websocket messages
typedef struct {
	Opcode opcode;

	uint64_t payload_length;
	byte* payload;
} Dataframe;


void Dataframe_free(Dataframe* dataframe);

// performs standard websocket handshake, takes tcp client as input
// returns -1 on failure
int ws_handshake(int socket_fd);

int ws_read(int socket_fd, Dataframe* dataframe);
// print info
int ws_read_debug(int socket_fd, Dataframe* dataframe);
int ws_send(int socket_fd, Opcode message_type, byte* payload, uint64_t bytes);
#endif
