#ifndef __CHOP_HELPER_H__
#define __CHOP_HELPER_H__

/*
 * Macros and Constants
 */

#define PACKET_LEN 4
#define TEXT_LEN 255

/// Header Packet bytes
#define PACKET_HEAD 0 // currently no purpose
#define PACKET_STATUS 1 // defines what this packet means
#define PACKET_CONTROL1 2 // parameter 1 for packet type
#define PACKET_CONTROL2 3 // parameter 2 for packet type

/// status bytes
#define NULL_BYTE 0 // basically a no-operation
#define START_HEADER 1 // control1 indicates number of extra bytes
#define START_TEXT 2 // control1 - num of elements, control2 - size of each element
#define END_TEXT 3 // used in conjuction with variable length START_TEXT
#define END_TRANSMISSION 4 // TODO
#define ENQUIRY 5 // basically a ping
#define ACKNOWLEDGE 6 // signal was received, control1 is recieved status
#define WAKEUP 7 // wake sleeping connection
#define SHIFT_OUT 14 // TODO
#define SHIFT_IN 15 // TODO
#define START_DATA 16 // TODO
#define CONTROL_KEY 17 // special action 1
#define CONTROL_END 20 // special action 4
#define NEG_ACKNOWLEDGE 21 // received status/message is incorrect/invalid, control1 is status
#define IDLE 22 // go to sleep, only accept wakeup or escape as signals
#define END_TRANSMISSION_BLOCK 23 // TODO
#define CANCEL 24 // TODO
#define END_OF_MEDIUM 25 // TODO
#define SUBSTITUTE 26 // TODO
#define ESCAPE 27 // Disconnect, waits for acknowledge (useful for cleanup)
#define FILE_SEPARATOR 28 // TODO
#define GROUP_SEPARATOR 29 // TODO
#define RECORD_SEPARATOR 30 // TODO
#define UNIT_SEPARATOR 31 // TODO

#define CONNECTION_QUEUE 5
#define MAX_CONNECTIONS 20

#define PIPE_READ 0
#define PIPE_WRITE 1

#define MIN_FD 0

static const int debug_fd = STDERR_FILENO;
static const char debug_header[] = "[DEBUG] ";

/*
 * Structures and Types
 */

struct socket_buffer {
	char buf[TEXT_LEN];
	int consumed;
	int inbuf;
};
typedef struct socket_buffer Buffer;

struct client {
	int socket_fd;
  int inc_flag;
  int out_flag;
	struct socket_buffer *buffer;
};
typedef struct client Client;

struct packet {
  char head[PACKET_LEN];
	char buf[TEXT_LEN];
  int inbuf;
};
typedef struct packet Packet;

/*
 * Client Management functions
 */

/*
 * Blocks until a client connects to a server already running on listen_fd.
 * Length of clients array is assumed to be MAX_CONNECTIONS.
 * Returns 0 on success, -1 on error and 1 if the list of clients is full.
 */
int setup_new_client(const int listen_fd, Client *clients[]);

int establish_server_connection(const int port, const char *address, Client *cli);

/*
 * Frees the client at the given index in the given array.
 * Length of client array is assumed to be MAX_CONNECTIONS.
 * Returns 0 on success, -1 on error and 1 if the target client doesn't exist.
 */
int remove_client(const int client_index, Client *clients[]);

/*
 * Sending functions
 */

/*
 * Writes data to the given socket, assuming msg is the beginning of an array
 * of msg_len length.
 * Automatically packages the given data into a text style packet.
 * Refuses to send messages that are too large to fit in a single text packet.
 * Returns 0 on success, -1 on error, and 1 on an imcomplete/failed write.
 * ERRNO will be preserved from the write call.
 */
int write_buf_to_client(Client *cli, const char *msg, const int msg_len);

/*
 * Writes the given packet to the given socket.
 * Returns 0 on success, -1 on error, and 1 on an imcomplete/failed write.
 * ERRNO will be preserved from the write call.
 */
int write_packet_to_client(Client *cli, Packet *pack);

/*
 * Writes the given string to the given client.
 * Assumes the string is null-terminated, unpredictable behaviour otherwise.
 * Returns 0 on success, -1 on error, and 1 on an imcomplete/failed write.
 * ERRNO will be preserved from the write call.
 */
int send_str_to_client(Client *cli, const char *str);

/*
 * Writes a given format string to the given socket.
 * Functions similarly to dprintf, with a different return behaviour
 * Returns 0 on success, -1 on error, and 1 on an imcomplete/failed write.
 * ERRNO will be preserved from the write call.
 */
int send_fstr_to_client(Client *cli, const char *format, ...);

/*
 * Receiving functions
 */

int read_header(Client *cli);

int parse_ext_header(Client *cli, char header[PACKET_LEN]);

int parse_text(Client *cli, const int control1, const int control2);

int parse_enquiry(Client *cli, const int control1);

int parse_acknowledge(Client *cli, const int control1);

int parse_neg_acknowledge(Client *cli, const int control1);

int parse_escape(Client *cli);


/*
 * Utility Functions
 */

/*
 * Copies given data into a given packet struct
 */
int assemble_packet(Packet *pack, char header[PACKET_LEN], const char *buf, const int buf_len);

/*
 * Prints requested format string into stderr, prefixing properly
 */
void debug_print(const char *format, ...);

/*
 * Zeroes out all the fields of a given client
 */
int reset_client_struct(Client *client);

/*
 * Zeroes out all the fields of a given buffer.
 */
int reset_buffer_struct(Buffer *buffer);

/*
 * Zeroes out all the fields of a given segment structure.
 */
int reset_packet_struct(Packet *pack);

#endif
