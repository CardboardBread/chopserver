#ifndef CHOPSERVER_LOCKING_H
#define CHOPSERVER_LOCKING_H

#include <pthread.h>

/*
 * Macros that expand into block that wraps the given code in locking function calls.
 * Blocks must not execute any jump instructions such as return, break, continue, etc.
 */

#define MUTEX_BLOCK(mutex, args) pthread_mutex_lock(mutex);{args}pthread_mutex_unlock(mutex);

#define RDLOCK_BLOCK(lock, args) pthread_rwlock_rdlock(lock);{args}pthread_rwlock_unlock(lock);
#define WRLOCK_BLOCK(lock, args) pthread_rwlock_wrlock(lock);{args}pthread_rwlock_unlock(lock);

#define SPIN_BLOCK(lock, args) pthread_spin_lock(lock);{args}pthread_spin_unlock(lock);

#endif //CHOPSERVER_LOCKING_H
