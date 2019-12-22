#ifndef __CHOPDATA_H__
#define __CHOPDATA_H__

#include "chopconst.h"

int read_to_buf(struct buffer *buffer, const int input, int *received);

int read_data(struct client *cli, struct packet *pack, int remaining, int *received);

int find_newline(const char *str, const int len, int *location);

int remove_newline(char *str, const int len, int *location);

int buf_contains_symbol(const char *buf, const int len, const char symbol, int *symbol_index);

/*
 * Converts the given value into its binary representation within a string.
 * Places the string representation in the given buffer, which is assumed to be
 * at least 9 bytes long, in order to hold all 8 bits in char form, as well as
 * the terminating bit.
 */
void char_to_bin(char value, char *ret);

#endif
