#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>
#include <stdarg.h>

#include "hashtable_api.h"


#define BUFFER_SIZE (1024 * 1024 * 100) // 10MB

#define MAX_LOG_MSG_SIZE (256u)       // Log messages printed to stdout can't be larger than this


// Min. size of a randomly-generated key or value
#define MIN_STR_LEN (16u)

// Max. size of a randomly-generated key or value
#define MAX_STR_LEN (32u)

// Number of randomly-generated items to insert into table
#define ITEM_INSERT_COUNT (1000000u)

// Number of slots in the hashtable
#define INITIAL_ARRAY_COUNT (100000u)


#if defined(_WIN32)
#include <Windows.h>
static uint64_t _perf_freq;
#elif defined(__linux__)
#include <sys/time.h>
#else
#error "Platform not supported"
#endif // _WIN32


typedef struct
{
    char key[MAX_STR_LEN + 1u];
    char value[MAX_STR_LEN + 1u];
    hashtable_size_t key_size;
    hashtable_size_t value_size;
} _test_keyval_pair_t;


// Test data for insertion/removal
static _test_keyval_pair_t _test_pairs[ITEM_INSERT_COUNT];

// Buffer for hashtable
static uint8_t _buffer[BUFFER_SIZE];

// Hashtable instance
static hashtable_t _table;

// Microseconds timestamp of program start time (used to show elapsed time in log message timestamps)
static uint64_t _start_us = 0u;


// Utility function for string-ifiyig a size in bytes in a human-readable format
#define NUMSUFFIXES (7)

static const char *size_suffixes[NUMSUFFIXES] =
{
    "EB", "PB", "TB", "GB", "MB", "KB", "B"
};

#define EXABYTES                (1024ULL * 1024ULL * 1024ULL * 1024ULL * \
                                 1024ULL * 1024ULL)

int sizesprint(size_t size, char *buf, unsigned int bufsize)
{
    int ret = 0;
    uint64_t mult = EXABYTES;

    for (int i = 0; i < NUMSUFFIXES; i++, mult /= 1024ULL)
    {
        if (size < mult)
        {
            continue;
        }

        if (mult && (size % mult) == 0)
        {
            ret = snprintf(buf, bufsize, "%"PRIu64"%s", size / mult, size_suffixes[i]);
        }
        else
        {
            ret = snprintf(buf, bufsize, "%.2f%s", (float) size / mult, size_suffixes[i]);
        }

        break;
    }

    return ret;
}


// Utility function for getting a system timestamp in microseconds
uint64_t timing_usecs_elapsed(void)
{
#if defined(_WIN32)
    LARGE_INTEGER tcounter = {0};
    uint64_t tick_value = 0u;
    if (QueryPerformanceCounter(&tcounter) != 0)
    {
        tick_value = tcounter.QuadPart;
    }

    return (uint64_t) (tick_value / (_perf_freq / 1000000ULL));
#elif defined(__linux__)
    struct timeval timer = {.tv_sec=0LL, .tv_usec=0LL};
    (void) gettimeofday(&timer, NULL);
    return (uint64_t) ((timer.tv_sec * 1000000LL) + timer.tv_usec);
#else
#error "Platform not supported"
#endif // _WIN32
}


// Utility function to log a message to stdout with a timestamp
static void _log(const char *fmt, ...)
{
    char buf[MAX_LOG_MSG_SIZE];
    uint64_t usecs = timing_usecs_elapsed() - _start_us;
    uint32_t secs = (uint32_t) (usecs / 1000000u);
    uint32_t msecs_remaining = ((usecs % 1000000u) / 1000u);
    int written = snprintf(buf, sizeof(buf), "[%05us %03ums] ", secs, msecs_remaining);

    va_list args;
    va_start(args, fmt);
    (void) vsnprintf(buf + written, sizeof(buf) - written, fmt, args);
    va_end(args);

    printf("%s", buf);
}


// Utility functions for generating random numbers / ASCII strings
static int _rand_range(int lower, int upper)
{
    return (rand() % ((upper - lower) + 1u)) + lower;
}


static void _rand_str(char *output, size_t *num_chars)
{
    *num_chars = (size_t) _rand_range(MIN_STR_LEN, MAX_STR_LEN);

    for (unsigned int i = 0; i < *num_chars; i++)
    {
        output[i] = (char) _rand_range(0x21, 0x7e);
    }

    output[*num_chars] = '\0';
}


int main(void)
{
    // Setup timing function
#if defined(_WIN32)
    LARGE_INTEGER tcounter = {0};
    if (QueryPerformanceFrequency(&tcounter) != 0)
    {
        _perf_freq = tcounter.QuadPart;
    }
#elif defined(__linux__)
    // Nothing to do
#else
#error "Platform not supported"
#endif // _WIN32

    _start_us = timing_usecs_elapsed();
    srand((unsigned int) _start_us);

    hashtable_config_t config;
    (void) memcpy(&config, hashtable_default_config(), sizeof(config));
    config.initial_array_count = INITIAL_ARRAY_COUNT;

    if (hashtable_create(&_table, &config, _buffer, sizeof(_buffer)) < 0)
    {
        printf("%s\n", hashtable_error_message());
        return -1;
    }

    size_t bytes_available = 0u;
    if (hashtable_bytes_remaining(&_table, &bytes_available) < 0)
    {
        printf("%s\n", hashtable_error_message());
        return -1;
    }

    char bufsize_buf[32];
    (void) sizesprint(sizeof(_buffer), bufsize_buf, sizeof(bufsize_buf));

    char rmsize_buf[32];
    (void) sizesprint(bytes_available, rmsize_buf, sizeof(rmsize_buf));

    _log("Total buffer size is %s, %s remaining for key/value pair data\n",
         bufsize_buf, rmsize_buf);

    _log("Generating %u random key/value pairs, all keys and values are between %u-%u bytes in size\n",
         ITEM_INSERT_COUNT, MIN_STR_LEN, MAX_STR_LEN);

    for (uint32_t i = 0u; i < ITEM_INSERT_COUNT; i++)
    {
        _rand_str(_test_pairs[i].key, &_test_pairs[i].key_size);
        _rand_str(_test_pairs[i].value, &_test_pairs[i].value_size);
    }

    _log("Inserting all %u items into the table\n", ITEM_INSERT_COUNT);

    for (uint32_t i = 0u; i < ITEM_INSERT_COUNT; i++)
    {
        if (0 > hashtable_insert(&_table, _test_pairs[i].key, _test_pairs[i].key_size,
                                _test_pairs[i].value, _test_pairs[i].value_size))
         {
            printf("%s\n", hashtable_error_message());
            return -1;
         }
    }

    // Check bytes available after inserting everything
    bytes_available = 0u;
    if (hashtable_bytes_remaining(&_table, &bytes_available) < 0)
    {
        printf("%s\n", hashtable_error_message());
        return -1;
    }

    (void) sizesprint(bytes_available, rmsize_buf, sizeof(rmsize_buf));

    _log("All items inserted, %s remaining, %u/%u array slots used\n",
         rmsize_buf, _table.array_slots_used, INITIAL_ARRAY_COUNT);

    // Retrieve all stored items and verify they match expected
    for (uint32_t i = 0u; i < ITEM_INSERT_COUNT; i++)
    {
        char value[MAX_STR_LEN];
        hashtable_size_t value_size;

        if (0 > hashtable_retrieve(&_table, _test_pairs[i].key, _test_pairs[i].key_size,
                                   value, &value_size))
        {
            printf("%s\n", hashtable_error_message());
            return -1;
        }

        if (0 != memcmp(_test_pairs[i].value, value, value_size))
        {
            printf("Error, retrieved value #%u did not match\n", i);
            return -1;
        }
    }

    _log("All %u items retrieved and contents verified\n", ITEM_INSERT_COUNT);

    return 0;
}
