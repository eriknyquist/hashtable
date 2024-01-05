#ifndef TESTING_UTILS_H
#define TESTING_UTILS_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>

#include "hashtable_api.h"


// Min. size of a randomly-generated key or value
#define MIN_STR_LEN (2u)

// Max. size of a randomly-generated key or value
#define MAX_STR_LEN (16u)


int sizesprint(size_t size, char *buf, unsigned int bufsize);

void timing_init(void);

uint64_t timing_usecs_elapsed(void);

void test_log(const char *fmt, ...);

int rand_range(int lower, int upper);

void rand_str(char *output, hashtable_size_t *num_chars, bool ascii_only);


#endif // TESTING_UTILS_H
