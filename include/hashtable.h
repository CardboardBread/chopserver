#ifndef CHOPSERVER_HASHTABLE_H
#define CHOPSERVER_HASHTABLE_H

#include <stddef.h>
#include <stdbool.h>

/*
 * Data Types
 */

// should be able to hold any data type
typedef unsigned long long hash_value;
typedef unsigned long long hash_key;

/*
 * Structures
 */

struct hash_entry_str {
	struct hash_entry_str *next;
	hash_key key;
	hash_value value;
};

struct hash_bucket_str {
	struct hash_entry_str *head;
	size_t count;
	pthread_rwlock_t lock;
};

struct hash_table_str {
	size_t size;
	struct hash_bucket_str *buckets;
	pthread_rwlock_t lock;
};

typedef struct hash_table_str hash_table_t;

/*
 * Functions
 */

hash_table_t *table_create(size_t size);

bool table_destroy(hash_table_t *target, size_t *size_dest);

bool table_get(hash_table_t *target, hash_key key, hash_value *dest);

bool table_put(hash_table_t *target, hash_key key, hash_value value);

bool table_remove(hash_table_t *target, hash_key key, hash_value *dest);

#endif //CHOPSERVER_HASHTABLE_H
