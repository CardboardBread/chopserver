#ifndef CHOPSERVER_SEGMENTLIST_H__
#define CHOPSERVER_SEGMENTLIST_H__

#include <stdbool.h>
#include <stddef.h>

typedef unsigned long long list_word;

struct segment {
	struct segment *next;
	char *array;
};

struct segment_list {
	struct segment *head;
	size_t elem_size;
	size_t elem_count;
	size_t seg_count;
	size_t elem_max;
	size_t cur_elem;
};

bool append(struct segment_list *list, list_word value);

bool insert(struct segment_list *list, size_t index, list_word value);

bool remove(struct segment_list *list, size_t index, list_word *dest);

bool get(struct segment_list *list, size_t index, list_word *dest);

bool size(struct segment_list *list, size_t *dest);

#endif // CHOPSERVER_SEGMENTLIST_H__
