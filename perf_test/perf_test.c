#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <inttypes.h>
#include <stdarg.h>

#include "testing_utils.h"
#include "hashtable_api.h"

// Size of statically-allocated buffer passed to hashtable_create
#define BUFFER_SIZE (1024 * 1024 * 512)

// Number of randomly-generated items to insert into table
#define ITEM_INSERT_COUNT (1000000u)


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
    unsigned char key[MAX_STR_LEN + 1u];
    unsigned char value[MAX_STR_LEN + 1u];
    hashtable_size_t key_size;
    hashtable_size_t value_size;
} _test_keyval_pair_t;


// Test data for insertion/removal
static _test_keyval_pair_t _test_pairs[ITEM_INSERT_COUNT];

// Buffer for hashtable
static uint8_t _buffer[BUFFER_SIZE];

// Hashtable instance
static hashtable_t _table;


static void _fmt_int_with_commas(long x, char *output)
{
    char buf[64u];
	char *intstr = buf;

    int len = snprintf(buf, sizeof(buf), "%ld", x);

    if (*intstr == '-')
	{
        *output++ = *intstr++;
        len--;
    }

    switch (len % 3)
	{
        do
		{
            *output++ = ',';
            case 0: *output++ = *intstr++;
            case 2: *output++ = *intstr++;
            case 1: *output++ = *intstr++;
        } while (*intstr);
    }

    *output = '\0';
}


static void _fmt_bytes_as_hex(unsigned char *bytes, size_t num_bytes, char *output)
{
    for (int i = 0; i < num_bytes; i++)
    {
        int printed = snprintf(output, 4u, "%02x ", (uint8_t) bytes[i]);
        output += printed;
    }
}





static int _run_perf_test(bool ascii_only)
{
    hashtable_config_t config;
    (void) hashtable_default_config(&config, sizeof(_buffer));
    config.array_count = 4026571;

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

    char tablesize_buf[32];
    (void) sizesprint(sizeof(_buffer) - bytes_available, tablesize_buf, sizeof(tablesize_buf));

    test_log("Buffer size %s\n", bufsize_buf);
    test_log("%s of buffer is used for table array\n", tablesize_buf);
    test_log("%s of buffer remains for key/value data\n", rmsize_buf);

	char itemcount_str[64u];
	_fmt_int_with_commas((long) ITEM_INSERT_COUNT, itemcount_str);

    test_log("Generating %s random key/value pairs, all keys/values are %u-%u bytes in size\n",
        itemcount_str, MIN_STR_LEN, MAX_STR_LEN);

    for (uint32_t i = 0u; i < ITEM_INSERT_COUNT; i++)
    {
        rand_str(_test_pairs[i].key, &_test_pairs[i].key_size, ascii_only);
        rand_str(_test_pairs[i].value, &_test_pairs[i].value_size, ascii_only);
    }

    unsigned char *firstkey = _test_pairs[0].key;
    unsigned char *firstvalue = _test_pairs[0].value;
    unsigned char *lastkey = _test_pairs[ITEM_INSERT_COUNT - 1].key;
    unsigned char *lastvalue = _test_pairs[ITEM_INSERT_COUNT - 1].value;

    char firstkeybuf[128u];
    char lastkeybuf[128u];
    char firstvaluebuf[128u];
    char lastvaluebuf[128u];

    if (!ascii_only)
    {
        hashtable_size_t firstkeysize = _test_pairs[0].key_size;
        hashtable_size_t firstvaluesize = _test_pairs[0].value_size;
        hashtable_size_t lastkeysize = _test_pairs[ITEM_INSERT_COUNT - 1].key_size;
        hashtable_size_t lastvaluesize = _test_pairs[ITEM_INSERT_COUNT - 1].value_size;

        _fmt_bytes_as_hex(firstkey, firstkeysize, firstkeybuf);
        _fmt_bytes_as_hex(firstvalue, firstvaluesize, firstvaluebuf);
        _fmt_bytes_as_hex(lastkey, lastkeysize, lastkeybuf);
        _fmt_bytes_as_hex(lastvalue, lastvaluesize, lastvaluebuf);

        firstkey = (unsigned char *) firstkeybuf;
        firstvalue = (unsigned char *) firstvaluebuf;
        lastkey = (unsigned char *) lastkeybuf;
        lastvalue = (unsigned char *) lastvaluebuf;
    }

    test_log("first key   : %s\n", firstkey);
    test_log("first value : %s\n", firstvalue);
    test_log("last key    : %s\n", lastkey);
    test_log("last value  : %s\n", lastvalue);

    test_log("Inserting all %s key/value pairs into the table\n", itemcount_str);

    uint64_t total_insert_us = 0u;
    uint64_t longest_insert_us = 0u;

    for (uint32_t i = 0u; i < ITEM_INSERT_COUNT; i++)
    {
        // Verify key is not already in table
        if (hashtable_has_key(&_table, (char *) _test_pairs[i].key, _test_pairs[i].key_size))
        {
            int index = -1;
            // Find the matching key
            for (int j = 0u; j < i; j++)
            {
                if ((_test_pairs[j].key_size == _test_pairs[i].key_size) &&
                    (0 == memcmp(_test_pairs[j].key, _test_pairs[i].key, _test_pairs[i].key_size)))
                {
                    index = j;
                    break;
                }
            }

            if (index < 0)
            {
                printf("Error inserting key #%u, table reports key exists, but it's not in the test data\n", i);
            }
            else
            {
                printf("Error inserting key #%u, key #%u matches\n", i, index);
            }

            return -1;
        }

        uint64_t startus = timing_usecs_elapsed();
        int ret = hashtable_insert(&_table, (char *) _test_pairs[i].key, _test_pairs[i].key_size,
                                   (char *) _test_pairs[i].value, _test_pairs[i].value_size);
        uint64_t time_us = timing_usecs_elapsed() - startus;

        if (-1 == ret)
        {
            printf("%s\n", hashtable_error_message());
            return -1;
        }
        else if (1 == ret)
        {
            printf("No more space in buffer\n");
            return -1;
        }

        total_insert_us += time_us;

        if (time_us > longest_insert_us)
        {
            longest_insert_us = time_us;
        }
    }

    double avg_insert_us = ((double) total_insert_us) / ((double) ITEM_INSERT_COUNT);

    // Check bytes available after inserting everything
    bytes_available = 0u;
    if (hashtable_bytes_remaining(&_table, &bytes_available) < 0)
    {
        printf("%s\n", hashtable_error_message());
        return -1;
    }

    (void) sizesprint(bytes_available, rmsize_buf, sizeof(rmsize_buf));

    char slotsused_str[64u];
    char totalslots_str[64u];
    _fmt_int_with_commas((long) _table.array_slots_used, slotsused_str);
    _fmt_int_with_commas((long) _table.config.array_count, totalslots_str);
    test_log("All items inserted, %s remaining, %s/%s array slots used\n",
             rmsize_buf, slotsused_str, totalslots_str);

    uint64_t total_retrieve_us = 0u;
    uint64_t longest_retrieve_us = 0u;

    // Retrieve all stored items and verify they match expected
    for (uint32_t i = 0u; i < ITEM_INSERT_COUNT; i++)
    {
        char *value;
        hashtable_size_t value_size;

        uint64_t startus = timing_usecs_elapsed();
        if (0 > hashtable_retrieve(&_table, (char *) _test_pairs[i].key, _test_pairs[i].key_size,
                                   &value, &value_size))
        {
            printf("%s\n", hashtable_error_message());
            return -1;
        }

        uint64_t time_us = timing_usecs_elapsed() - startus;
        total_retrieve_us += time_us;

        if (time_us > longest_retrieve_us)
        {
            longest_retrieve_us = time_us;
        }

        if (_test_pairs[i].value_size != value_size)
        {
            printf("Error, retrieved value #%u size did not match (inserted %u, table had %u)\n",
                   i, _test_pairs[i].value_size, value_size);
            return -1;
        }

        if (0 != memcmp(_test_pairs[i].value, value, value_size))
        {
            printf("Error, retrieved value #%u contents did not match\n", i);
            return -1;
        }
    }

    double avg_retrieve_us = ((double) total_retrieve_us) / ((double) ITEM_INSERT_COUNT);

    test_log("All %s items retrieved & verified via hashtable_retrieve\n", itemcount_str);

    uint64_t total_remove_us = 0u;
    uint64_t longest_remove_us = 0u;

    // Remove all stored items
    for (uint32_t i = 0u; i < ITEM_INSERT_COUNT; i++)
    {
        uint64_t startus = timing_usecs_elapsed();
        if (0 > hashtable_remove(&_table, (char *) _test_pairs[i].key, _test_pairs[i].key_size))
        {
            printf("%s\n", hashtable_error_message());
            return -1;;
        }

        uint64_t time_us = timing_usecs_elapsed() - startus;
        total_remove_us += time_us;

        if (time_us > longest_remove_us)
        {
            longest_remove_us = time_us;
        }
    }

    double avg_remove_us = ((double) total_remove_us) / ((double) ITEM_INSERT_COUNT);

    _fmt_int_with_commas((long) _table.array_slots_used, slotsused_str);
    test_log("All items removed via hashtable_remove, %s/%s array slots used\n", slotsused_str, totalslots_str);

    // Verify all remove items are indeed removed, according to hashtable_has_key
    for (uint32_t i = 0u; i < ITEM_INSERT_COUNT; i++)
    {
        if (hashtable_has_key(&_table, (char *) _test_pairs[i].key, _test_pairs[i].key_size))
        {
            printf("Item #%u has been removed, but apparently is still in the table\n", i);
            return -1;
        }
    }

    test_log("Removal of all items verified via hashtable_has_key\n");

    test_log("Inserting all %s items into the table again\n", itemcount_str);

    uint64_t total_after_insert_us = 0u;
    uint64_t longest_after_insert_us = 0u;

    for (uint32_t i = 0u; i < ITEM_INSERT_COUNT; i++)
    {
        uint64_t startus = timing_usecs_elapsed();
        if (0 > hashtable_insert(&_table, (char *) _test_pairs[i].key, _test_pairs[i].key_size,
                                (char *) _test_pairs[i].value, _test_pairs[i].value_size))
        {
            printf("%s\n", hashtable_error_message());
            return -1;
        }

        uint64_t time_us = timing_usecs_elapsed() - startus;
        total_after_insert_us += time_us;

        if (time_us > longest_after_insert_us)
        {
            longest_after_insert_us = time_us;
        }
    }

    double avg_after_insert_us = ((double) total_after_insert_us) / ((double) ITEM_INSERT_COUNT);

    _fmt_int_with_commas((long) _table.array_slots_used, slotsused_str);
    test_log("All items re-inserted, %s/%s array slots used\n", slotsused_str, totalslots_str);
    test_log("Done\n");

    printf("\n");
    printf("Longest initial hashtable_insert time, microsecs    : %"PRIu64"\n", longest_insert_us);
    printf("Avg. initial hashtable_insert time, microsecs       : %.2f\n", avg_insert_us);

    printf("Longest hashtable_retrieve time, microsecs          : %"PRIu64"\n", longest_retrieve_us);
    printf("Avg. hashtable_retrieve time, microsecs             : %.2f\n", avg_retrieve_us);

    printf("Longest hashtable_remove time, microsecs            : %"PRIu64"\n", longest_remove_us);
    printf("Avg. hashtable_remove time, microsecs               : %.2f\n\n", avg_remove_us);

    printf("Longest secondary hashtable_insert time, microsecs  : %"PRIu64"\n", longest_after_insert_us);
    printf("Avg. secondary hashtable_insert time, microsecs     : %.2f\n", avg_after_insert_us);
    printf("\n");

    return 0;
}


int main(void)
{
    timing_init();
    srand((unsigned int) timing_usecs_elapsed());

    printf("\nhashtable performance smoke test (hashtable "HASHTABLE_LIB_VERSION")\n\n");

    test_log("Running test with randomly-generated ASCII key/value data\n");
    if (_run_perf_test(true) < 0)
    {
        return -1;
    }

    printf("\n");
    test_log("Running test with randomly-generated binary key/value data\n");
    return _run_perf_test(false);
}
