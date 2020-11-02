#include <stdbool.h>
#include <assert.h>
#include <math.h>

#include "prime.h"

bool is_prime(LARGE value) {
	assert(value != 0);

	LARGE factor;
	LARGEF limit = sqrtl(value);
	for (factor = 2; factor <= limit; factor++) {
		if (value % factor == 0) {
			return false;
		}
	}

	return true;
}

LARGE next_prime(LARGE value, LARGE type_max) {

	LARGE next;
	for (next = value; next < type_max; next++) {
		if (is_prime(next)) {
			return next;
		}
	}

	return 0;
}