#ifndef __CHOPDATA_H__
#define __CHOPDATA_H__

#include "chopconst.h"

int read_to_buf(struct buffer *buffer, const int input, int *received);

int find_newline(const char *str, const int len, int *location);

int remove_newline(char *str, const int len, int *location);

int buf_contains_symbol(const char *buf, const int len, const char symbol, int *symbol_index);

#endif
