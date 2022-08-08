#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "testing_utils.h"


#if defined(_WIN32)
#include <Windows.h>
static uint64_t _perf_freq;
#elif defined(__linux__)
#include <sys/time.h>
#else
#error "Platform not supported"
#endif // _WIN32


// Log messages printed to stdout can't be larger than this
#define MAX_LOG_MSG_SIZE (256u)


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


void timing_init(void)
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
void test_log(const char *fmt, ...)
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
int rand_range(int lower, int upper)
{
    return (rand() % ((upper - lower) + 1)) + lower;
    //return (rand() % (upper - lower)) + lower;
}


void rand_str(unsigned char *output, hashtable_size_t *num_chars, bool ascii_only)
{
    *num_chars = (hashtable_size_t) rand_range(MIN_STR_LEN, MAX_STR_LEN);

    // By default, generate from all printable chars
    int low = 0x21;
    int high = 0x7e;

    if (!ascii_only)
    {
        // Unless ascii_only flag is unset, in which case all bytes values are used
        low = 0x0;
        high = 0xfe;
    }

    for (unsigned int i = 0; i < *num_chars; i++)
    {
        output[i] = (unsigned char) rand_range(low, high);
    }

    output[*num_chars] = '\0';
}
