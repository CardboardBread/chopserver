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

typedef struct hash_table_str hash_table_t;

/*
 * Functions
 */

struct hash_table_str *table_create(size_t size);

bool table_destroy(struct hash_table_str *target, size_t *size_dest);

bool table_get(struct hash_table_str *target, hash_key key, hash_value *dest);

bool table_put(struct hash_table_str *target, hash_key key, hash_value value);

bool table_remove(struct hash_table_str *target, hash_key key, hash_value *dest);

#endif //CHOPSERVER_HASHTABLE_H
