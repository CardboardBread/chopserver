#ifndef CHOPSERVER_SEGMENTLIST_H__
#define CHOPSERVER_SEGMENTLIST_H__

#include <stdbool.h>
#include <stddef.h>

typedef unsigned long long list_word;

struct segment_str {
	struct segment_str *next;
	char *array;
};

struct segment_list_str {
	struct segment_str *head;
	size_t e_size;
	size_t e_count;
	size_t s_count;
	size_t e_max;
	size_t cur_e;
};

bool append(struct segment_list_str *list, list_word value);

bool insert(struct segment_list_str *list, size_t index, list_word value);

bool remove(struct segment_list_str *list, size_t index, list_word *dest);

bool get(struct segment_list_str *list, size_t index, list_word *dest);

#endif // CHOPSERVER_SEGMENTLIST_H__
