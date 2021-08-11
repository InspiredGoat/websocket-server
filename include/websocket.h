#ifndef WEBSOCKET_H
#define WEBSOCKET_H
#include <stdint.h>
// library doesn't deal with clients, leaves that up to you! (you're welcome)

// Type of data being transmitted by websocket
typedef unsigned char byte;

typedef enum {
	CONTINUATION,
	TEXT,
	BINARY
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
