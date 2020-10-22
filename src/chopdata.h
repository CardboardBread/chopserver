#ifndef __CHOPDATA_H__
#define __CHOPDATA_H__

#include "chopconst.h"

/*
 * Given a buffer and a fd to read from, attempts to fill the remaining space
 * of the buffer with as much data as can be gathered in a single call.
 */
int fill_buf(struct buffer *buffer, int input_fd);

/*
 * Given a client to read from and a packet to place the data in, attempts to
 * read as much data as required into buffers that fit the requirements of the
 * client.
 */
int read_data(struct client *cli, struct packet *pack, size_t remaining);

/*
 * Given a client to read from and an empty packet as a container, reads in
 * exactly enough data to fill a packet header.
 */
int read_header(struct client *cli, struct packet *pack);

/*
 * Writes the entire given packet to the given client, returns once the entire
 * packet is sent.
 */
int write_packet(struct client *cli, struct packet *pack);

/*
 * Scans the given buffer for a newline sequence '\n' or '\r\n', returning the
 * farthest index in the newline (always returns the index of the '\n').
 * Returns negative if no newline is found, or the given arguments are invalid.
 */
int find_newline(const char *str, size_t len);

/*
 * Scans the given buffer for a newline sequence '\n' or '\r\n', returning the
 * farthest index in the newline (always returns the index of the '\n'). Also
 * replaces the newline with a null byte in order to delimit the buffer. Returns
 * negative if no newline is found, or the given arguments are invalid.
 * This function is generally equivalent to a call to strtok.
 */
int remove_newline(char *str, size_t len);

/*
 * Consecutively scans a given buffer for a given symbol, returning the index of
 * the symbol. If no symbol is found, or invalid arguments are provided, a
 * negative value is returned.
 */
int buf_contains_symbol(const char *buf, size_t len, char symbol);

/*
 * Converts the given value into its binary representation within a string.
 * Places the string representation in the given buffer, which is assumed to be
 * at least 9 bytes long, in order to hold all 8 bits in char form, as well as
 * the terminating bit.
 */
void char_to_bin(char value, char *ret);

/*
 * Takes the data in the given buffer and places it into the given packet,
 * segmenting it according to the given limit
 */
int assemble_data(struct packet *pack, const char *buf, size_t buf_len, size_t fragment_size);

/*
 * Replaces the given packet's data segments with one continuous data segment.
 * Takes a given packet's data segments and copies it into a single space in
 * memory, by looking through all the segments and allocating a single chunk of
 * memory that can hold all of the data.
 */
int consolidate_packet(struct packet *pack);

int fragment_packet(struct packet *pack);

/*
 * Allocates a new buffer of given size at the end of a given packet. If out is
 * provided, it will point to the new buffer.
 */
int append_buffer(struct packet *pack, size_t buffer_len, struct buffer **out);

/*
 * Loops until all the data is read from the given fd into the given buffer.
 */
int force_read(int input_fd, char *buffer, size_t incoming);

/*
 * Loops until all the data in the given buffer is written into the given fd.
 */
int force_write(int output_fd, const char *buffer, size_t outgoing);

/*
 * Converts the header of a given packet to a single number
 */
int packet_style(struct packet *pack);

#endif
