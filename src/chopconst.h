#ifndef __CHOPCONST_H__
#define __CHOPCONST_H__

#include <netinet/in.h>

/*
 * Packet Macros
 */

/// Header Packet bytes
#define PACKET_HEAD 0 // currently no purpose
#define PACKET_STATUS 1 // defines what this packet means
#define PACKET_CONTROL1 2 // parameter 1 for packet type
#define PACKET_CONTROL2 3 // parameter 2 for packet type

/// Status Bytes
#define NULL_BYTE 0 // basically a no-operation
#define START_HEADER 1 // control1 indicates number of extra header bytes
#define START_TEXT 2 // control1 - num of elements, control2 - size of each element
// if both control signals are 0, unknown text length is specified
// to send an empty message, send 1 message of 0 length or 1,0 on control signals
#define END_TEXT 3 // used in conjuction with variable length START_TEXT
#define END_TRANSMISSION 4 // TODO
#define ENQUIRY 5 // basically a ping
#define ENQUIRY_NORMAL 0 // just acknowledge
#define ENQUIRY_RETURN 1 // return enquiry signal1=0
#define ENQUIRY_TIME 2 // sent time in data section
#define ENQUIRY_RTIME 3 // return time in data section
#define ACKNOWLEDGE 6 // signal was received, control1 is recieved status
#define WAKEUP 7 // wake sleeping connection

#define SHIFT_OUT 14 // TODO
#define SHIFT_IN 15 // TODO
#define START_DATA 16 // TODO
#define CONTROL_ONE 17 // special action 1
#define CONTROL_TWO 18 // special action 2
#define CONTROL_THREE 19 // special action 3
#define CONTROL_FOUR 20 // special action 4
#define NEG_ACKNOWLEDGE 21 // received status/message is incorrect/invalid, control1 is status
#define IDLE 22 // go to sleep, only accept wakeup or escape as signals
#define END_TRANSMISSION_BLOCK 23 // TODO
#define CANCEL 24 // flag marker for closing connections, should not be sent in a packet
#define END_OF_MEDIUM 25 // TODO
#define SUBSTITUTE 26 // TODO
#define ESCAPE 27 // Disconnect, waits for acknowledge (useful for cleanup)
#define FILE_SEPARATOR 28 // TODO
#define GROUP_SEPARATOR 29 // TODO
#define RECORD_SEPARATOR 30 // TODO
#define UNIT_SEPARATOR 31 // TODO

/*
 * General Macros
 */

#define MIN_FD 0

/*
 * Type Definitions
 */

typedef unsigned char pack_head;
typedef unsigned char pack_stat;
typedef unsigned char pack_con1;
typedef unsigned char pack_con2;

/*
 * Structures
 */

struct buffer {
	char *buf;
	int inbuf;
	int bufsize;
	struct buffer *next;
};

struct packet {
	pack_head head;
	pack_stat status;
	pack_con1 control1;
	pack_con2 control2;
	struct buffer *data;
	int datalen;
};

struct server {
	int server_fd;
	int server_port;
	struct sockaddr_in address;
	struct client **clients; // array of client pointers
	int max_connections;
	int cur_connections;
	int connect_queue;
};

struct client {
	struct sockaddr_in address;
	int socket_fd; // fd of the client
	int server_fd; // fd of the server this client is attached to, -1 if client
	pack_stat inc_flag; // what the client is receiving
	pack_stat out_flag; // what the client is sending
	int window; // how much data the client can pass at once
};

/*
 * Structure-Relevant Macros
 */

#define HEADER_LEN sizeof(pack_head) + sizeof(pack_stat) + sizeof(pack_con1) + sizeof(pack_con2)

/*
 * Structure Management Functions
 */

int init_buffer_struct(struct buffer **target, const int size);

int init_packet_struct(struct packet **target);

int init_server_struct(struct server **target, const int port, const int max_conns, const int queue_len);

int init_client_struct(struct client **target, const int size);

int destroy_buffer_struct(struct buffer **target);

int destroy_packet_struct(struct packet **target);

int destroy_server_struct(struct server **target);

int destroy_client_struct(struct client **target);

#endif
