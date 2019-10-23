#ifndef __CHOPDATA_H__
#define __CHOPDATA_H__

int read_to_buf(struct buffer *buffer, const int input, int *received);

int find_newline(char *str, const int len, int *location);

int remove_newline(char *str, const int len, int *location);

int buf_contains_symbol(const char *buf, const int len, const int symbol, int *symbol_index);

#endif
