//
// Created by mike_ on 11/2/2020.
//

#ifndef CHOPSERVER_PRIME_H
#define CHOPSERVER_PRIME_H

#include <stdbool.h>

// should hold any datatype
#define LARGE unsigned long long
#define LARGEF long double

/*
 * Brute-force tests if the given value is prime.
 */
bool is_prime(LARGE value);

/*
 * Attempts to find a prime greater than or equal to the given value, within
 * the limits of a specified datatype's maximum value.
 * Returns 0 if no prime is greater than value and within the given datatype.
 */
LARGE next_prime(LARGE value, LARGE type_max);

#endif //CHOPSERVER_PRIME_H
