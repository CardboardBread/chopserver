#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <unistd.h>
#include <assert.h>
#include <math.h>
#include <pthread.h>

#include "hashtable.h"

static bool is_prime(size_t value) {
	// only positive primes
	assert(value > 0);

	// mod by all values that could divide evenly
	for (size_t factor = 2; factor <= sqrt(value); factor++) {
		if (value % factor == 0) {
			return false;
		}
	}

	return true;
}

/*
 * Attempts to find a prime greater than or equal to the given value.
 * Returns 0 if no prime is greater (value overflow).
 */
static size_t next_prime(size_t value) {
	// loop upwards until prime is found
	size_t next;
	for (next = value; next >= value; next++) {
		if (is_prime(next)) {
			return next;
		}
	}

	// no prime above value (within data type)
	return 0;
}

struct hash_entry_str *entry_create(struct hash_entry_str *next, hash_key key, hash_value value) {
	// allocate memory for new entry
	struct hash_entry_str *new = malloc(sizeof(struct hash_entry_str));
	if (new == NULL) {
		return NULL;
	}

	// copy values into memory
	new->next = next;
	new->key = key;
	new->value = value;

	return new;
}

bool entry_destroy(struct hash_entry_str *target, hash_key *dest_key, hash_value *dest_value) {
	// copy values out before destruction
	if (target != NULL) {
		if (dest_key != NULL) {
			*dest_key = target->key;
		}

		if (dest_value != NULL) {
			*dest_value = target->value;
		}
	}

	free(target);
	return true;
}

struct hash_bucket_str *bucket_create(struct hash_bucket_str *target) {
	assert(target != NULL);

	// init access-aware sync structure
	if (pthread_rwlock_init(&target->lock, NULL) != 0) {
		return NULL;
	}

	// copy default values into memory
	target->head = NULL;
	target->count = 0;
	return target;
}

bool bucket_empty(struct hash_bucket_str *target, size_t *count_dest) {
	assert(target != NULL);

	// loop through entire entry list, destroying elements first to last
	struct hash_entry_str *last;
	struct hash_entry_str *current;
	for (current = target->head, last = NULL; current != NULL; current = current->next) {
		entry_destroy(last, NULL, NULL);
		last = current;
	}

	// destroy last element
	entry_destroy(last, NULL, NULL);

	// copy value out
	if (count_dest != NULL) {
		*count_dest = target->count;
	}

	return true;
}

bool bucket_destroy(struct hash_bucket_str *target, size_t *count_dest) {
	// empty bucket and destroy sync structure
	bucket_empty(target, count_dest);
	pthread_rwlock_destroy(&target->lock);
	return true;
}

struct hash_table_str *table_create(size_t size) {
	assert(size > 0);

	// align buckets to prime number
	if (!is_prime(size)) {
		size = next_prime(size);
	}

	// allocate memory for table
	struct hash_table_str *new = malloc(sizeof(struct hash_table_str));
	if (new == NULL) {
		return NULL;
	}

	// allocate memory for buckets
	struct hash_bucket_str *new_arr = calloc(size, sizeof(struct hash_bucket_str));
	if (new_arr == NULL) {
		free(new);
		return NULL;
	}

	// init access-aware sync structure
	if (pthread_rwlock_init(&new->lock, NULL) != 0) {
		free(new_arr);
		free(new);
		return NULL;
	}

	// init each bucket
	for (size_t i = 0; i < size; i++) {
		if (bucket_create(new_arr + i) == NULL) {
			free(new_arr);
			free(new);
			return NULL;
		}
	}

	// copy default values into memory
	new->size = size;
	new->buckets = new_arr;
	return new;
}

bool table_destroy(struct hash_table_str *target, size_t *size_dest) {
	if (target != NULL) {
		// copy value out before destruction
		if (size_dest != NULL) {
			*size_dest = target->size;
		}

		// destroy inner properties
		for (size_t i = 0; i < target->size; i++) {
			bucket_destroy(target->buckets + i, NULL);
		}
		pthread_rwlock_destroy(&target->lock);
		free(target->buckets);
	}

	free(target);
	return true;
}

struct hash_entry_str *bucket_put(struct hash_bucket_str *target, hash_key key, hash_value value) {
	struct hash_entry_str *append;
	assert(target != NULL);

	// loop through bucket, maintaining current and previous elements
	struct hash_entry_str *prev;
	struct hash_entry_str *tail;
	for (tail = target->head, prev = NULL; tail != NULL; tail = tail->next) {

		// remap existing entry to new value
		if (tail->key == key) {
			append = tail;
			tail->value = value;
			break;
		}

		prev = tail;
	}
	// loop ends with prev at last element and tail null

	// existing entry not remapped
	if (append == NULL) {
		// create new entry for key-value pair
		append = entry_create(NULL, key, value);
		if (append == NULL) {
			return NULL;
		}

		// determine if list empty or end reached
		if (tail == NULL && prev == NULL) {
			target->head = append;
		} else {
			prev->next = append;
		}

		target->count++;
	}

	return append;
}

struct hash_entry_str *bucket_get_key(struct hash_bucket_str *target, hash_key key) {
	assert(target != NULL);

	// loop through bucket for target key
	struct hash_entry_str *current;
	for (current = target->head; current != NULL; current = current->next) {
		if (current->key == key) {
			return current;
		}
	}

	return NULL;
}

struct hash_entry_str *bucket_get_value(struct hash_bucket_str *target, hash_value value) {
	assert(target != NULL);

	// loop through bucket for target value
	struct hash_entry_str *current;
	for (current = target->head; current != NULL; current = current->next) {
		if (current->value == value) {
			return current;
		}
	}

	return NULL;
}

bool bucket_remove(struct hash_bucket_str *target, hash_key key, hash_value *dest) {
	hash_value copy;
	assert(target != NULL);

	// loop through bucket, keeping the previous pointer (or the head pointer)
	struct hash_entry_str *cur;
	struct hash_entry_str **cur_pte;
	for (cur_pte = &target->head, cur = target->head; cur != NULL; cur_pte = &cur->next, cur = cur->next) {
		if (cur->key == key) {
			// point over current to next element, destroy current and update bucket
			*cur_pte = cur->next;
			copy = entry_destroy(cur, NULL, dest);
			target->count--;

			// return copy to caller
			if (dest != NULL) {
				*dest = copy;
			}
			return true;
		}
	}

	return false;
}

size_t bucket_index(struct hash_table_str *target, hash_key key) {
	return key % target->size;
}

bool table_get(struct hash_table_str *target, hash_key key, hash_value *dest) {
	struct hash_entry_str *target_entry;
	assert(target != NULL);

	// lock read access to table
	pthread_rwlock_rdlock(&target->lock);
	{
		// find key-specific bucket
		size_t index = bucket_index(target, key);
		struct hash_bucket_str *target_bucket = target->buckets + index;

		// lock bucket read access
		pthread_rwlock_rdlock(&target_bucket->lock);
		{
			target_entry = bucket_get_key(target_bucket, key);
		}
		pthread_rwlock_unlock(&target_bucket->lock);
	}
	pthread_rwlock_unlock(&target->lock);

	if (target_entry != NULL) {
		if (dest != NULL) {
			*dest = target_entry->value;
		}
		return true;
	} else {
		return false;
	}
}

bool table_put(struct hash_table_str *target, hash_key key, hash_value value) {
	struct hash_entry_str *target_entry;
	assert(target != NULL);

	// lock read access to table
	pthread_rwlock_rdlock(&target->lock);
	{
		// find key-specific bucket
		size_t index = bucket_index(target, key);
		struct hash_bucket_str *target_bucket = target->buckets + index;

		// lock bucket write access
		pthread_rwlock_wrlock(&target_bucket->lock);
		{
			target_entry = bucket_put(target_bucket, key, value);
		}
		pthread_rwlock_unlock(&target_bucket->lock);
	}
	pthread_rwlock_unlock(&target->lock);

	if (target_entry != NULL) {
		return true;
	} else {
		return false;
	}
}

bool table_remove(struct hash_table_str *target, hash_key key, hash_value *dest) {
	bool ret;
	assert(target != NULL);

	// lock read access to table
	pthread_rwlock_rdlock(&target->lock);
	{
		// find key-specific bucket
		size_t index = bucket_index(target, key);
		struct hash_bucket_str *target_bucket = target->buckets + index;

		// lock bucket write access
		pthread_rwlock_wrlock(&target_bucket->lock);
		{
			ret = bucket_remove(target_bucket, key, dest);
		}
		pthread_rwlock_unlock(&target_bucket->lock);
	}
	pthread_rwlock_unlock(&target->lock);

	return ret;
}
