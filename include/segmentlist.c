#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>

#include "segmentlist.h"

//region segment

struct segment *create_segment(struct segment *next, size_t count, size_t size) {
	assert(count > 0 && size > 0);

	// allocate segment on heap
	struct segment *new_str = malloc(sizeof(struct segment));
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

bool destroy_segment(struct segment *target) {
	// dealloc array
	if (target != NULL) {
		free(target->array);
	}

	free(target);
	return true;
}

struct segment *append_segment(struct segment_list *target) {
	assert(target != NULL);

	// allocate new segment
	struct segment *new = create_segment(NULL, target->elem_count, target->elem_size);

	// loop to first NULL pointer
	struct segment *tail;
	struct segment **tail_ptr;
	for (tail_ptr = &target->head, tail = target->head; tail != NULL; tail_ptr = &tail->next, tail = tail->next) {
		assert(*tail_ptr == tail);
	}

	// append segment
	*tail_ptr = new;
	target->seg_count++;
	return new;
}

struct segment *insert_segment(struct segment_list *target, size_t index) {
	assert(target != NULL);
	assert(index <= target->seg_count);

	// TODO: call append if insert is equal to groups

	// allocate new segment
	struct segment *new = create_segment(NULL, target->elem_count, target->elem_size);

	// loop to first NULL pointer or index iterations
	struct segment *cur;
	struct segment **cur_ptr;
	for (cur = target->head, cur_ptr = &target->head; index > 0 && cur != NULL; cur = cur->next, cur_ptr = &cur->next, index--) {
		assert(*cur_ptr == cur);
	}

	// insert into linked list
	new->next = cur;
	*cur_ptr = new;
	target->seg_count++;
	return new;
}

bool remove_segment(struct segment_list *target, size_t index) {
	assert(target != NULL);
	assert(index < target->seg_count);

	// loop to first NULL pointer or index iterations
	struct segment *cur;
	struct segment **cur_ptr;
	for (cur = target->head, cur_ptr = &target->head; index > 0 && cur != NULL; cur = cur->next, cur_ptr = &cur->next, index--) {
		assert(*cur_ptr == cur);
	}
	assert(index == 0);

	struct segment *skip = (cur != NULL) ? cur->next : NULL;
	*cur_ptr = skip;
	destroy_segment(cur);
	target->seg_count--;
	return true;
}

struct segment *get_segment(struct segment_list *target, size_t index) {
	assert(target != NULL);
	assert(index < target->seg_count);

	// loop to first NULL pointer or index iterations
	struct segment *cur;
	for (cur = target->head; index > 0 && cur != NULL; cur = cur->next, index--);
	assert(index == 0);

	return cur;
}

//endregion

//region list

struct segment_list *create_list(size_t e_count, size_t e_size, size_t e_max) {
	assert(e_count > 0 && e_size > 0);

	struct segment_list *new = malloc(sizeof(struct segment_list));
	if (new == NULL) {
		return NULL;
	}

	// copy arguments and defaults
	new->elem_count = e_count;
	new->elem_size = e_size;
	new->seg_count = 0;
	new->elem_max = e_max;
	new->cur_elem = 0;
	return new;
}

bool destroy_list(struct segment_list *target) {
	assert(target != NULL);

	// loop through entire segment list, destroying elements first to last
	struct segment *last;
	struct segment *curr;
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
char *get_index_segment(struct segment_list *list, struct segment *segment, size_t index) {
	assert(list != NULL && segment != NULL && index < list->cur_elem); // valid arguments
	size_t array_index = index % list->elem_count;
	char *index_ptr = segment->array + (array_index * list->elem_size);
	assert(index_ptr < (segment->array + list->elem_count * list->elem_size)); // pointer is in buffer
	return index_ptr;
}

/*
 * Get pointer to requested index from given list.
 */
char *get_index_list(struct segment_list *list, size_t index) {
	assert(list != NULL && index < list->cur_elem); // valid arguments
	size_t segment_index = index / list->elem_count;
	assert(segment_index < list->seg_count); // segment exists
	struct segment *target_segment = get_segment(list, segment_index);
	return get_index_segment(list, target_segment, index);
}

/*
 * Get segment in list from given index.
 */
struct segment *get_segment_list(struct segment_list *list, size_t index) {
	assert(list != NULL && index < list->cur_elem); // valid arguments
	size_t segment_index = index / list->elem_count;
	assert(segment_index < list->seg_count); // segment exists
	return get_segment(list, segment_index);
}

/*
 * Get value at requested index in list.
 */
list_word get_value_list(struct segment_list *list, size_t index) {
	assert(list != NULL && index < list->cur_elem); // valid arguments
	list_word out;
	char *pointer = get_index_list(list, index);
	memcpy(&out, pointer, list->elem_size);
	assert(out <= list->elem_max); // value is inside data type
	return out;
}

list_word get_value_segment(struct segment_list *list, struct segment *segment, size_t index) {
	assert(list != NULL && segment != NULL && index < list->cur_elem); // valid arguments
	list_word out;
	char *pointer = get_index_segment(list, segment, index);
	memcpy(&out, pointer, list->elem_size);
	assert(out <= list->elem_max); // value is inside data type
	return out;
}

/*
 * Set value at requested index in list.
 */
bool set_value_list(struct segment_list *list, size_t index, list_word value) {
	assert(list != NULL && index < list->cur_elem);
	char *pointer = get_index_list(list, index);
	memcpy(pointer, &value, list->elem_size);
	return true;
}

/*
 * Set value at requested index in list by segment.
 */
bool set_value_segment(struct segment_list *list, struct segment *segment, size_t index, list_word value) {
	char *pointer = get_index_segment(list, segment, index);
	memcpy(pointer, &value, list->elem_size);
	return true;
}

//endregion

//region interface

/*
 * Sets value at end of list to given, adds new segment if needed.
 */
bool append(struct segment_list *list, list_word value) {
	if (list == NULL) {
		return false;
	}

	// do we need a new segment to hold data
	size_t next = list->cur_elem + 1;
	if (list->cur_elem % list->elem_count == 0) {
		// append segment and set value
		struct segment *buf = append_segment(list);
		set_value_segment(list, buf, next, value);
	} else {
		// set value
		set_value_list(list, next, value);
	}

	list->cur_elem++;
	return true;
}

bool insert(struct segment_list *list, size_t index, list_word value) {
	if (list == NULL) {
		return false;
	}

	// check for valid index
	if (index > list->cur_elem || index < 0) {
		return false;
	}

	return set_value_list(list, index, value);
}

bool remove(struct segment_list *list, size_t index, list_word *dest) {
	if (list == NULL) {
		return false;
	}

	// check for valid index
	if (index > list->cur_elem || index < 0) {
		return false;
	}

	// grab the value being removed
	list_word pick = get_value_list(list, index);

	// move every element back one space to fill the gap
	for (size_t i = index + 1; index < list->cur_elem; i++) {
		list_word ahead = get_value_list(list, i);
		set_value_list(list, i - 1, ahead);
	}

	if (dest != NULL) {
		*dest = pick;
	}

	list->cur_elem--;
	return true;
}

bool get(struct segment_list *list, size_t index, list_word *dest) {
	if (list == NULL) {
		return false;
	}

	// check for valid index
	if (index > list->cur_elem || index < 0) {
		return false;
	}

	list_word grab = get_value_list(list, index);
	if (dest != NULL) {
		*dest = grab;
	}

	return true;
}

bool size(struct segment_list *list, size_t *dest) {
	if (list == NULL) {
		return false;
	}

	if (dest != NULL) {
		*dest = list->cur_elem;
	}

	return true;
}

//endregion