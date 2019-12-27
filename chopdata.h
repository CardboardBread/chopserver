#ifndef __CHOPDATA_H__
#define __CHOPDATA_H__

#include "chopconst.h"

int fill_buf(struct buffer *buffer, const int input);

int read_data(struct client *cli, struct packet *pack, int remaining);

/*
 * Scans the given buffer for a newline sequence '\n' or '\r\n', returning the
 * farthest index in the newline (always returns the index of the '\n'). Returns
 * negative if no newline is found, or the given arguments are invalid.
 */
int find_newline(const char *str, const int len);

/*
 * Scans the given buffer for a newline sequence '\n' or '\r\n', returning the
 * farthest index in the newline (always returns the index of the '\n'). Also
 * replaces the newline with a null byte in order to delimit the buffer. Returns
 * negative if no newline is found, or the given arguments are invalid.
 * This function is generally equivalent to a call to strtok.
 */
int remove_newline(char *str, const int len);

/*
 * Consecutively scans a given buffer for a given symbol, returning the index of
 * the symbol. If no symbol is found, or invalid arguments are provided, a
 * negative value is returned.
 */
int buf_contains_symbol(const char *buf, const int len, const char symbol);

/*
 * Converts the given value into its binary representation within a string.
 * Places the string representation in the given buffer, which is assumed to be
 * at least 9 bytes long, in order to hold all 8 bits in char form, as well as
 * the terminating bit.
 */
void char_to_bin(char value, char *ret);

#endif
