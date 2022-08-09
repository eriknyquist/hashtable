#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <inttypes.h>
#include <stdarg.h>

#include "testing_utils.h"
#include "hashtable_api.h"


#if defined(_WIN32)
#include <Windows.h>
static uint64_t _perf_freq;
#elif defined(__linux__)
#include <sys/time.h>
#else
#error "Platform not supported"
#endif // _WIN32


// Size of statically-allocated buffer passed to hashtable_create
#define BUFFER_SIZE (1024 * 1024 * 16)


static uint8_t _buffer[BUFFER_SIZE];
static hashtable_t _table;
static uint32_t _insert_counter = 0u;


static float _load_factor(hashtable_t *table)
{
    return ((float) table->entry_count) / ((float) table->config.array_count);
}


int _retrieve_all_and_time(hashtable_t *table, uint64_t *avg_retrieve_ns)
{
    uint64_t start_us = timing_usecs_elapsed();
    for (uint32_t i = 0u; i < table->entry_count; i++)
    {
        int ret = hashtable_retrieve(table, (char *) &i, sizeof(i), NULL, NULL);
        if (0 != ret)
        {
            return ret;
        }
    }
    uint64_t us_elapsed = timing_usecs_elapsed() - start_us;
    *avg_retrieve_ns = (us_elapsed * 1000u) / table->entry_count;

    return 0;
}


int _check_for_1k_bad_keys(hashtable_t *table, uint64_t *avg_badkey_ns)
{
    uint64_t start_us = timing_usecs_elapsed();

    for (uint32_t i = table->entry_count; i < (table->entry_count + 1000u); i++)
    {
        if (1 == hashtable_has_key(table, (char *) &i, sizeof(i)))
        {
            printf("Error, bad key exists!\n");
            return -1;
        }
    }

    *avg_badkey_ns = timing_usecs_elapsed() - start_us;
    return 0;
}


static int _insert_2k_items(hashtable_t *table)
{
    // Insert 2000 items
    uint64_t before_insert = timing_usecs_elapsed();
    for (int i = 0; i < 2000; i++)
    {
        int ret = hashtable_insert(table, (char *) &_insert_counter, sizeof(_insert_counter),
                                   NULL, 0u);

        if (ret != 0)
        {
            return ret;
        }

        _insert_counter += 1u;
    }

    uint64_t avg_insert_ns = (timing_usecs_elapsed() - before_insert) / 2;
    uint64_t avg_retrieve_ns = 0u;

    int ret = _retrieve_all_and_time(table, &avg_retrieve_ns);
    if (0 != ret)
    {
        return ret;
    }

    uint64_t avg_badkey_ns = 0u;
    ret = _check_for_1k_bad_keys(table, &avg_badkey_ns);
    if (0 != ret)
    {
        return ret;
    }

    test_log("entries=%u, lf=%.2f, insrtns=%u, rtrv_ns=%u, badkeyns=%u\n",
             table->entry_count, _load_factor(table), avg_insert_ns, avg_retrieve_ns,
             avg_badkey_ns);

    return 0;
}


int main(void)
{
    timing_init();

    if (0 != hashtable_create(&_table, NULL, _buffer, sizeof(_buffer)))
    {
        printf("hashtable_create failed\n");
        return -1;
    }

    // Insert 2k items and print status, until the table is full
    while (_insert_2k_items(&_table) == 0);

    return 0;
}

