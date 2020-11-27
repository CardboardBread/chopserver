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
#define SUBSTITUTE 26 // Error, notifying a client/server of an error on the sender's end
#define ESCAPE 27 // Disconnect, waits for acknowledge (useful for cleanup)
#define FILE_SEPARATOR 28 // TODO
#define GROUP_SEPARATOR 29 // TODO
#define RECORD_SEPARATOR 30 // TODO
#define UNIT_SEPARATOR 31 // TODO

/*
 * General Macros
 */

#define MIN_FD 0
#define MAX_STATUS 32
#define MAX_ENQUIRY 4

// Macros for type maximums, sourced from stackoverflow
#define ISSIGNED(t) (((t)(-1)) < ((t) 0))
#define UMAXOF(t) (((0x1ULL << ((sizeof(t) * 8ULL) - 1ULL)) - 1ULL) | (0xFULL << ((sizeof(t) * 8ULL) - 4ULL)))
#define SMAXOF(t) (((0x1ULL << ((sizeof(t) * 8ULL) - 1ULL)) - 1ULL) | (0x7ULL << ((sizeof(t) * 8ULL) - 4ULL)))
#define MAXOF(t) ((unsigned long long) (ISSIGNED(t) ? SMAXOF(t) : UMAXOF(t)))

/*
 * Type Definitions // TODO: create shortcut to insert proper specifier for printing
 * Recommended these are unsigned types
 */

typedef uint8_t pack_head;
typedef uint8_t pack_stat;
typedef uint8_t pack_con1;
typedef uint8_t pack_con2;

static const size_t pack_head_max = UMAXOF(pack_head);
static const size_t pack_stat_max = UMAXOF(pack_stat);
static const size_t pack_con1_max = UMAXOF(pack_con1);
static const size_t pack_con2_max = UMAXOF(pack_con2);

/*
 * Structures
 */

struct buffer {
	char *buf;
	size_t inbuf;
	size_t bufsize;
	struct buffer *next;
};

struct packet_header {
	pack_head head;
	pack_stat status;
	pack_con1 control1;
	pack_con2 control2;
};

struct packet {
	struct packet_header header;
	struct buffer *data;
	size_t datalen;
};

struct server {
	struct sockaddr_in address;
	int server_fd;
	int server_port;
	struct client **clients; // array of client pointers
	size_t max_connections;
	size_t cur_connections;
	size_t connect_queue;
	size_t window;
};

struct client {
	struct sockaddr_in address;
	int socket_fd; // fd of the client
	int server_fd; // fd of the server this client is attached to, -1 if client
	pack_stat inc_flag; // what the client is receiving
	pack_stat out_flag; // what the client is sending
	size_t window; // how much data the client can pass at once
};

struct status {
	pack_stat value;
	const char *name;
};

/*
 * Shortcut Type Definitions
 */

typedef int (*status_function)(struct client *, struct packet *);

/*
 * Variables
 */

extern const struct status status_str_arr[MAX_STATUS];

extern const struct status enquiry_str_arr[MAX_ENQUIRY];

/*
 * Structure Management Functions
 */

/*
 * Allocates and initializes a buffer in heap memory, pointing the given ptr at it
 * Intended for fragmented allocation
 */
int create_buffer(struct buffer **target, size_t size);

/*
 * Readies the buffer structure at the given location for use
 * Intended for array allocation or stack allocation
 */
int init_buffer(struct buffer *target, size_t size);

/*
 * Resizes the given buffer to a new size, does not move contents between buffers
 */
int realloc_buffer(struct buffer *target, size_t size);

/*
 * Frees all memory related to the target buffer
 */
int destroy_buffer(struct buffer **target);

/*
 * Allocates and initializes a packet in heap memory, pointing the given ptr at it
 * Intended for fragmented allocation
 */
int create_packet(struct packet **target);

/*
 * Readies the packet structure at the given location for use
 * Intended for array allocation or stack allocation
 */
int init_packet(struct packet *target);

/*
 * Destroys each buffer within the given packet
 */
int empty_packet(struct packet *target);

/*
 * Frees all memory related to the target packet
 */
int destroy_packet(struct packet **target);

int create_server(struct server **target, int port, size_t max_conns, size_t queue_len, size_t window);

int destroy_server(struct server **target);

int create_client(struct client **target, size_t window);

int destroy_client(struct client **target);

#endif
