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
#define CONTROL_ONE 17 // special action 1
#define CONTROL_TWO 18 // special action 2
#define CONTROL_THREE 19 // special action 3
#define CONTROL_FOUR 20 // special action 4
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

/*
 * Immediately connects to a server at the given address and port.
 * Does not block and will error if no server is available.
 * Address is assumed to be properly null terminated.
 * Returns 0 on success, -1 on error.
 */
int establish_server_connection(const char *address, const int port, Client **client);

/*
 * Frees the client at the given index in the given array.
 * Length of client array is assumed to be MAX_CONNECTIONS.
 * Returns 0 on success, -1 on error and 1 if the target client doesn't exist.
 */
int remove_client_index(const int client_index, Client *clients[]);

/*
 * Frees the client that the given pointer is pointing to.
 * Returns 0 on success, -1 on error and 1 if the pointer has no client.
 */
int remove_client_address(const int client_index, Client **client);

int read_flags(Client *cli, fd_set *listen_fds);

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

int parse_header(Client *cli, const char header[PACKET_LEN]);

int parse_text(Client *cli, const int control1, const int control2);

char *read_long_text(Client *cli, int *len_ptr);

int parse_enquiry(Client *cli, const int control1);

int parse_acknowledge(Client *cli, const int control1);

int parse_wakeup(Client *cli);

int parse_neg_acknowledge(Client *cli, const int control1);

int parse_idle(Client *cli);

int parse_escape(Client *cli);

/*
 * Data Layer functions
 */

 /*
  * Replaces the first '\n' or '\r\n' found in str with a null terminator.
  * Returns the index of the new first null terminator if found, or -1 if
  * not found.
  */
 int remove_newline(char *str, int len);

/*
 * Utility Functions
 */

int is_client_status(Client *cli, const int status);

/*
 * Finds symbol within buf, under the constrictions of buf_len.
 * If symbol_index is a valid address, saves the index of the symbol.
 * Returns 0 if the symbol isn't found, 1 if the symbol is found.

 */
int buf_contains_symbol(const char *buf, const int buf_len, int symbol, int *symbol_index);

/*
 * Copies given data into a given packet struct
 */
int assemble_packet(Packet *pack, const char header[PACKET_LEN], const char *buf, const int buf_len);

/*
 * Prints requested format string into stderr, prefixing properly
 */
void debug_print(const char *format, ...);

int setup_client_struct(Client **client, int socket_fd);

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

int destroy_client_struct(Client **client);

#endif
