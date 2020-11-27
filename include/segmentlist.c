#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>

#include "segmentlist.h"

struct segment_str *create_segment(struct segment_str *next, size_t count, size_t size) {
	assert(count > 0 && size > 0);

	// allocate segment on heap
	struct segment_str *new_str = malloc(sizeof(struct segment_str));
	if (new_str == NULL) {
		return NULL;
	}

	// allocate segment memory
	char *new_arr = calloc(count, size);
	if (new_arr == NULL) {
		free(new_str);
		return NULL;
	}

	new_str->next = next;
	new_str->array = new_arr;
	return new_str;
}

bool destroy_segment(struct segment_str *target) {
	// dealloc array
	if (target != NULL) {
		free(target->array);
	}

	free(target);
	return true;
}

struct segment_str *append_segment(struct segment_list_str *target) {
	assert(target != NULL);

	// allocate new segment
	struct segment_str *new = create_segment(NULL, target->e_count, target->e_size);

	// loop to first NULL pointer
	struct segment_str *tail;
	struct segment_str **tail_ptr;
	for (tail_ptr = &target->head, tail = target->head; tail != NULL; tail_ptr = &tail->next, tail = tail->next) {
		assert(*tail_ptr == tail);
	}

	// append segment
	*tail_ptr = new;
	target->s_count++;
	return new;
}

struct segment_str *insert_segment(struct segment_list_str *target, size_t index) {
	assert(target != NULL);
	assert(index <= target->s_count);

	// TODO: call append if insert is equal to groups

	// allocate new segment
	struct segment_str *new = create_segment(NULL, target->e_count, target->e_size);

	// loop to first NULL pointer or index iterations
	struct segment_str *cur;
	struct segment_str **cur_ptr;
	for (cur = target->head, cur_ptr = &target->head; index > 0 && cur != NULL; cur = cur->next, cur_ptr = &cur->next, index--) {
		assert(*cur_ptr == cur);
	}

	// insert into linked list
	new->next = cur;
	*cur_ptr = new;
	target->s_count++;
	return new;
}

bool remove_segment(struct segment_list_str *target, size_t index) {
	assert(target != NULL);
	assert(index < target->s_count);

	// loop to first NULL pointer or index iterations
	struct segment_str *cur;
	struct segment_str **cur_ptr;
	for (cur = target->head, cur_ptr = &target->head; index > 0 && cur != NULL; cur = cur->next, cur_ptr = &cur->next, index--) {
		assert(*cur_ptr == cur);
	}
	assert(index == 0);

	struct segment_str *skip = (cur != NULL) ? cur->next : NULL;
	*cur_ptr = skip;
	destroy_segment(cur);
	target->s_count--;
	return true;
}

struct segment_str *get_segment(struct segment_list_str *target, size_t index) {
	assert(target != NULL);
	assert(index < target->s_count);

	// loop to first NULL pointer or index iterations
	struct segment_str *cur;
	for (cur = target->head; index > 0 && cur != NULL; cur = cur->next, index--);
	assert(index == 0);

	return cur;
}

struct segment_list_str *create_list(size_t e_count, size_t e_size, size_t e_max) {
	assert(e_count > 0 && e_size > 0);

	struct segment_list_str *new = malloc(sizeof(struct segment_list_str));
	if (new == NULL) {
		return NULL;
	}

	// copy arguments and defaults
	new->e_count = e_count;
	new->e_size = e_size;
	new->s_count = 0;
	new->e_max = e_max;
	new->cur_e = 0;
	return new;
}

bool destroy_list(struct segment_list_str *target) {
	assert(target != NULL);

	// loop through entire segment list, destroying elements first to last
	struct segment_str *last;
	struct segment_str *curr;
	for (curr = target->head, last = NULL; curr != NULL; curr = curr->next) {
		destroy_segment(last);
		last = curr;
	}

	// destroy last element
	destroy_segment(last);

	return true;
}

/*
 * Get pointer to requested index from given list and list segment.
 */
void *get_index_segment(struct segment_list_str *list, struct segment_str *segment, size_t index) {
	assert(list != NULL && segment != NULL && index < list->cur_e); // valid arguments
	size_t array_index = index % list->e_count;
	void *index_ptr = segment->array + (array_index * list->e_size);
	assert(index_ptr < ((void *) segment->array + list->e_count * list->e_size)); // pointer is in buffer
	return index_ptr;
}

/*
 * Get pointer to requested index from given list.
 */
void *get_index_list(struct segment_list_str *list, size_t index) {
	assert(list != NULL && index < list->cur_e); // valid arguments
	size_t segment_index = index / list->e_count;
	assert(segment_index < list->s_count); // segment exists
	struct segment_str *target_segment = get_segment(list, segment_index);
	return get_index_segment(list, target_segment, index);
}

/*
 * Get segment in list from given index.
 */
struct segment_str *get_segment_list(struct segment_list_str *list, size_t index) {
	assert(list != NULL && index < list->cur_e); // valid arguments
	size_t segment_index = index / list->e_count;
	assert(segment_index < list->s_count); // segment exists
	return get_segment(list, segment_index);
}

/*
 * Get value at requested index in list.
 */
list_word get_value_list(struct segment_list_str *list, size_t index) {
	assert(list != NULL && index < list->cur_e); // valid arguments
	list_word out;
	void *pointer = get_index_list(list, index);
	memcpy(&out, pointer, list->e_size);
	assert(out <= list->e_max); // value is inside data type
	return out;
}

list_word get_value_segment(struct segment_list_str *list, struct segment_str *segment, size_t index) {
	assert(list != NULL && segment != NULL && index < list->cur_e); // valid arguments
	list_word out;
	void *pointer = get_index_segment(list, segment, index);
	memcpy(&out, pointer, list->e_size);
	assert(out <= list->e_max); // value is inside data type
	return out;
}

/*
 * Set value at requested index in list.
 */
bool set_value_list(struct segment_list_str *list, size_t index, list_word value) {
	assert(list != NULL && index < list->cur_e);
	void *pointer = get_index_list(list, index);
	memcpy(pointer, &value, list->e_size);
	return true;
}

/*
 * Set value at requested index in list by segment.
 */
bool set_value_segment(struct segment_list_str *list, struct segment_str *segment, size_t index, list_word value) {
	void *pointer = get_index_segment(list, segment, index);
	memcpy(pointer, &value, list->e_size);
	return true;
}

/*
 * Sets value at end of list to given, adds new segment if needed.
 */
bool append(struct segment_list_str *list, list_word value) {
	if (list == NULL) {
		return false;
	}

	// do we need a new segment to hold data
	size_t next = list->cur_e + 1;
	if (list->cur_e % list->e_count == 0) {
		// append segment and set value
		struct segment_str *buf = append_segment(list);
		set_value_segment(list, buf, next, value);
	} else {
		// set value
		set_value_list(list, next, value);
	}

	list->cur_e++;
	return true;
}

bool insert(struct segment_list_str *list, size_t index, list_word value) {
	if (list == NULL) {
		return false;
	}

	// check for valid index
	if (index > list->cur_e || index < 0) {
		return false;
	}

	return set_value_list(list, index, value);
}

bool remove(struct segment_list_str *list, size_t index, list_word *dest) {
	if (list == NULL) {
		return false;
	}

	// check for valid index
	if (index > list->cur_e || index < 0) {
		return false;
	}

	// grab the value being removed
	list_word pick = get_value_list(list, index);

	// move every element back on space to fill the gap
	for (size_t i = index + 1; index < list->cur_e; i++) {
		list_word ahead = get_value_list(list, i);
		set_value_list(list, i - 1, ahead);
	}

	if (dest != NULL) {
		*dest = pick;
	}

	list->cur_e--;
	return true;
}

bool get(struct segment_list_str *list, size_t index, list_word *dest) {
	if (list == NULL) {
		return false;
	}

	// check for valid index
	if (index > list->cur_e || index < 0) {
		return false;
	}

	list_word grab = get_value_list(list, index);
	if (dest != NULL) {
		*dest = grab;
	}

	return true;
}