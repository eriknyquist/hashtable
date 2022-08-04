#include "unity.h"
#include "hashtable_api.h"

#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>


// Min. size of a randomly-generated key or value
#define MIN_STR_LEN (4u)


// Max. size of a randomly-generated key or value
#define MAX_STR_LEN (24u)


typedef struct
{
    char key[MAX_STR_LEN + 1u];
    char value[MAX_STR_LEN + 1u];
    size_t key_size;
    size_t value_size;
    bool removed;
} _test_keyval_pair_t;


static uint8_t _buffer[1024 * 1024];

static int _rand_range(int lower, int upper)
{
    return (rand() % (upper - lower + 1)) + lower;
}

static void _rand_str(char *output, size_t *num_chars)
{
    *num_chars = _rand_range(MIN_STR_LEN, MAX_STR_LEN);

    for (unsigned int i = 0; i < *num_chars; i++)
    {
        output[i] = (char) _rand_range(0x21, 0x7e);
    }

    output[*num_chars] = '\0';
}


static void _generate_random_items_and_insert(hashtable_t *table, _test_keyval_pair_t *pairs, unsigned int num_items)
{
    for (unsigned int i = 0; i < num_items; i++)
    {
        _rand_str(pairs[i].key, &pairs[i].key_size);
        _rand_str(pairs[i].value, &pairs[i].value_size);
        pairs[i].removed = false;

        TEST_ASSERT_EQUAL_INT(0, hashtable_insert(table, pairs[i].key, pairs[i].key_size,
                                                  pairs[i].value, pairs[i].value_size));
    }
}


static void _verify_table_contents(hashtable_t *table, _test_keyval_pair_t *pairs, unsigned int num_items)
{
    for (unsigned int i = 0; i < num_items; i++)
    {
        int key_expected = pairs[i].removed ? 0 : 1;

        // Verify table has key
        TEST_ASSERT_EQUAL_INT(key_expected, hashtable_has_key(table, pairs[i].key, pairs[i].key_size));

        if (1 == key_expected)
        {
            // Retrieve value and verify it matches what we put in
            char value[MAX_STR_LEN + 1u];
            size_t value_size;

            TEST_ASSERT_EQUAL_INT(0, hashtable_retrieve(table, pairs[i].key, pairs[i].key_size,
                                                    value, &value_size));
            TEST_ASSERT_EQUAL_INT(pairs[i].value_size, value_size);
            TEST_ASSERT_EQUAL_INT(0, memcmp(pairs[i].value, value, value_size));
        }
    }
}


static void _verify_iterated_table_contents(hashtable_t *table, _test_keyval_pair_t *pairs, unsigned int num_items,
                                            unsigned int num_items_removed)
{
    char key[MAX_STR_LEN + 1];
    char value[MAX_STR_LEN + 1];
    size_t key_size;
    size_t value_size;
    int ret;

    unsigned int entry_count = 0u;

    // Reset cursor
    TEST_ASSERT_EQUAL_INT(0, hashtable_reset_cursor(table));

    while(0 == (ret = hashtable_next_item(table, key, &key_size, value, &value_size)))
    {
        // Find the corresponding key/value pair data in the test data array
        _test_keyval_pair_t *test_pair = NULL;
        for (unsigned int i = 0u; i < num_items; i++)
        {
            if (0 != memcmp(pairs[i].key, key, key_size))
            {
                // Key doesn't match, continue to next item
                continue;
            }

            if (0 != memcmp(pairs[i].value, value, value_size))
            {
                // Key doesn't match, continue to next item
                continue;
            }

            // Found matching key/value pair, break out
            test_pair = &pairs[i];
            break;
        }

        // Assert that we found a matching pair
        TEST_ASSERT_FALSE(NULL == test_pair);

        // Assert that it has not been removed, shouldn't be in the table if so
        TEST_ASSERT_FALSE(test_pair->removed);

        entry_count += 1u;
    }

    // Ensure we reached the limit, instead of an error occurring
    TEST_ASSERT_EQUAL_INT(1, ret);

    // Assert number of entries yielded matches expected
    TEST_ASSERT_EQUAL_INT(num_items - num_items_removed, entry_count);
}


static void _remove_random_items(hashtable_t *table, _test_keyval_pair_t *pairs, unsigned int num_items,
                                 unsigned int num_items_to_remove)
{
    for (unsigned int i = 0; i < num_items_to_remove; i++)
    {
        // Find an item that hasn't been removed yet
        int index = _rand_range(0, num_items);

        while (pairs[index].removed)
        {
            index = _rand_range(0, num_items);
        }

        // Remove item
        TEST_ASSERT_EQUAL_INT(0, hashtable_remove(table, pairs[index].key, pairs[index].key_size));
        pairs[index].removed = true;
    }
}


void setUp(void)
{
}


void tearDown(void)
{
}


// Tests that hashtable_create returns an error when null table is passed
void test_hashtable_create_null_table(void)
{
    TEST_ASSERT_EQUAL_INT(-1, hashtable_create(NULL, hashtable_default_config(), _buffer, sizeof(_buffer)));
}


// Tests that hashtable_create returns an error when null config is passed
void test_hashtable_create_null_config(void)
{
    hashtable_t table;
    TEST_ASSERT_EQUAL_INT(-1, hashtable_create(&table, NULL, _buffer, sizeof(_buffer)));
}


// Tests that hashtable_create returns an error when a config with a null hash function is passed
void test_hashtable_create_null_hash_func(void)
{
    hashtable_t table;
    hashtable_config_t config = {NULL, 32u};
    TEST_ASSERT_EQUAL_INT(-1, hashtable_create(&table, &config, _buffer, sizeof(_buffer)));
}


// Tests that hashtable_create returns an error when a config with zero for the initial array count is passed
void test_hashtable_create_zero_array_count(void)
{
    hashtable_t table;
    hashtable_config_t config;

    // Make a copy of the default config
    (void) memcpy(&config, hashtable_default_config(), sizeof(config));

    config.array_count = 0u;

    TEST_ASSERT_EQUAL_INT(-1, hashtable_create(&table, &config, _buffer, sizeof(_buffer)));
}


// Tests that hashtable_create returns an error when buffer size is too small
void test_hashtable_create_buffer_size_too_small(void)
{
    hashtable_t table;
    TEST_ASSERT_EQUAL_INT(-1, hashtable_create(&table, hashtable_default_config(), _buffer, 2u));
}


// Tests that hashtable_insert returns an error when a NULL table is passed
void test_hashtable_insert_null_table(void)
{
    const char *key = "key1";
    const char *value = "val1";
    TEST_ASSERT_EQUAL_INT(-1, hashtable_insert(NULL, key, 4u, value, 4u));
}


// Tests that hashtable_insert returns an error when a NULL key is passed
void test_hashtable_insert_null_key(void)
{
    hashtable_t table;
    TEST_ASSERT_EQUAL_INT(0, hashtable_create(&table, hashtable_default_config(), _buffer, sizeof(_buffer)));

    const char *value = "val1";
    TEST_ASSERT_EQUAL_INT(-1, hashtable_insert(&table, NULL, 4u, value, 4u));
}


// Tests that hashtable_insert returns an error when a NULL value is passed
void test_hashtable_insert_null_value(void)
{
    hashtable_t table;
    TEST_ASSERT_EQUAL_INT(0, hashtable_create(&table, hashtable_default_config(), _buffer, sizeof(_buffer)));

    const char *key = "key1";
    TEST_ASSERT_EQUAL_INT(-1, hashtable_insert(&table, key, 4u, NULL, 4u));
}


// Tests that hashtable_insert returns an error when zero is given for key size
void test_hashtable_insert_zero_key_size(void)
{
    hashtable_t table;
    TEST_ASSERT_EQUAL_INT(0, hashtable_create(&table, hashtable_default_config(), _buffer, sizeof(_buffer)));

    const char *key = "key1";
    const char *value = "val1";
    TEST_ASSERT_EQUAL_INT(-1, hashtable_insert(&table, key, 0u, value, 4u));
}


// Tests that hashtable_insert returns an error when zero is given for value size
void test_hashtable_insert_zero_value_size(void)
{
    hashtable_t table;
    TEST_ASSERT_EQUAL_INT(0, hashtable_create(&table, hashtable_default_config(), _buffer, sizeof(_buffer)));

    const char *key = "key1";
    const char *value = "val1";
    TEST_ASSERT_EQUAL_INT(-1, hashtable_insert(&table, key, 4u, value, 0u));
}


// Tests that hashtable_remove returns an error when a null table is passed
void test_hashtable_remove_null_table(void)
{
    const char *key = "key1";
    TEST_ASSERT_EQUAL_INT(-1, hashtable_remove(NULL, key, 4u));
}


// Tests that hashtable_remove returns an error when a null key is passed
void test_hashtable_remove_null_key(void)
{
    hashtable_t table;
    TEST_ASSERT_EQUAL_INT(0, hashtable_create(&table, hashtable_default_config(), _buffer, sizeof(_buffer)));

    TEST_ASSERT_EQUAL_INT(-1, hashtable_remove(&table, NULL, 4u));
}


// Tests that hashtable_remove returns an error when zero is passed for key size
void test_hashtable_remove_zero_key_size(void)
{
    hashtable_t table;
    TEST_ASSERT_EQUAL_INT(0, hashtable_create(&table, hashtable_default_config(), _buffer, sizeof(_buffer)));

    const char *key = "key1";
    TEST_ASSERT_EQUAL_INT(-1, hashtable_remove(&table, key, 0u));
}


// Tests that hashtable_retrieve returns an error when a null table is passed
void test_hashtable_retrieve_null_table(void)
{
    const char *key = "key1";
    uint8_t value[4u];
    size_t value_size;
    TEST_ASSERT_EQUAL_INT(-1, hashtable_retrieve(NULL, key, 4u, value, &value_size));
}


// Tests that hashtable_retrieve returns an error when a null key is passed
void test_hashtable_retrieve_null_key(void)
{
    hashtable_t table;
    TEST_ASSERT_EQUAL_INT(0, hashtable_create(&table, hashtable_default_config(), _buffer, sizeof(_buffer)));

    uint8_t value[4u];
    size_t value_size;
    TEST_ASSERT_EQUAL_INT(-1, hashtable_retrieve(&table, NULL, 4u, value, &value_size));
}


// Tests that hashtable_retrieve returns an error when a null value is passed
void test_hashtable_retrieve_null_value(void)
{
    hashtable_t table;
    TEST_ASSERT_EQUAL_INT(0, hashtable_create(&table, hashtable_default_config(), _buffer, sizeof(_buffer)));

    const char *key = "key1";
    size_t value_size;
    TEST_ASSERT_EQUAL_INT(-1, hashtable_retrieve(&table, key, 4u, NULL, &value_size));
}


// Tests that hashtable_has_key returns an error when a null table is passed
void test_hashtable_has_key_null_table(void)
{
    const char *key = "key1";
    TEST_ASSERT_EQUAL_INT(-1, hashtable_has_key(NULL, key, 4u));
}


// Tests that hashtable_has_key returns an error when a null key is passed
void test_hashtable_has_key_null_key(void)
{
    hashtable_t table;
    TEST_ASSERT_EQUAL_INT(0, hashtable_create(&table, hashtable_default_config(), _buffer, sizeof(_buffer)));

    TEST_ASSERT_EQUAL_INT(-1, hashtable_has_key(&table, NULL, 4u));
}


// Tests that hashtable_bytes_remaining returns an error when a null table is pased
void test_hashtable_bytes_remaining_null_table(void)
{
    size_t remaining;
    TEST_ASSERT_EQUAL_INT(-1, hashtable_bytes_remaining(NULL, &remaining));
}


// Tests that hashtable_bytes_remaining returns an error when a null key is pased
void test_hashtable_bytes_remaining_null_key(void)
{
    hashtable_t table;
    TEST_ASSERT_EQUAL_INT(0, hashtable_create(&table, hashtable_default_config(), _buffer, sizeof(_buffer)));

    TEST_ASSERT_EQUAL_INT(-1, hashtable_bytes_remaining(&table, NULL));
}


// Tests that hashtable_next_item returns an error when a null table is passed
void test_hashtable_next_item_null_table(void)
{
    uint8_t key[4];
    uint8_t value[4];
    size_t key_size;
    size_t value_size;
    TEST_ASSERT_EQUAL_INT(-1, hashtable_next_item(NULL, &key, &key_size, &value, &value_size));
}


// Tests that hashtable_next_item returns an error when a null key is passed
void test_hashtable_next_item_null_key(void)
{
    hashtable_t table;
    TEST_ASSERT_EQUAL_INT(0, hashtable_create(&table, hashtable_default_config(), _buffer, sizeof(_buffer)));

    uint8_t value[4];
    size_t key_size;
    size_t value_size;
    TEST_ASSERT_EQUAL_INT(-1, hashtable_next_item(&table, NULL, &key_size, value, &value_size));
}


// Tests that hashtable_next_item returns an error when a null value is passed
void test_hashtable_next_item_null_value(void)
{
    hashtable_t table;
    TEST_ASSERT_EQUAL_INT(0, hashtable_create(&table, hashtable_default_config(), _buffer, sizeof(_buffer)));

    uint8_t key[4];
    size_t key_size;
    size_t value_size;
    TEST_ASSERT_EQUAL_INT(-1, hashtable_next_item(&table, key, &key_size, NULL, &value_size));
}


// Tests that hashtable_reset_cursor returns an error when a null table is passed
void test_hashtable_reset_cursor_null_table(void)
{
    TEST_ASSERT_EQUAL_INT(-1, hashtable_reset_cursor(NULL));
}


// Tests that hashtable_insert returns 1 when no space is available in the buffer
void test_hashtable_insert_buffer_full(void)
{
    uint8_t test_buf[512];

    hashtable_config_t config;
    (void) memcpy(&config, hashtable_default_config(), sizeof(config));
    config.array_count = 1u;

    hashtable_t table;
    TEST_ASSERT_EQUAL_INT(0, hashtable_create(&table, &config, test_buf, sizeof(test_buf)));

    uint8_t key1[128u];
    uint8_t key2[128u];
    uint8_t value[128u];

    // Just need 2 unique keys
    (void) memset(key1, 0xaa, sizeof(key1));
    (void) memset(key2, 0xbb, sizeof(key2));

    size_t bytes_remaining_1 = 0u;
    TEST_ASSERT_EQUAL_INT(0, hashtable_bytes_remaining(&table, &bytes_remaining_1));

    // First insertion should succeed
    TEST_ASSERT_EQUAL_INT(0, hashtable_insert(&table, key1, sizeof(key1), value, sizeof(value)));

    // Bytes remaining should have decreased
    size_t bytes_remaining_2 = 0u;
    TEST_ASSERT_EQUAL_INT(0, hashtable_bytes_remaining(&table, &bytes_remaining_2));
    TEST_ASSERT_TRUE(bytes_remaining_2 < bytes_remaining_1);

    // Second insertion should fail with return value of 1
    TEST_ASSERT_EQUAL_INT(1, hashtable_insert(&table, key2, sizeof(key2), value, sizeof(value)));

    // Bytes remaining should have stayed the same
    size_t bytes_remaining_3 = 0u;
    TEST_ASSERT_EQUAL_INT(0, hashtable_bytes_remaining(&table, &bytes_remaining_3));
    TEST_ASSERT_EQUAL_INT(bytes_remaining_2, bytes_remaining_3);
}


// Tests that hashtable_retrieve returns 1 when the requested key does not exist
void test_hashtable_retrieve_no_such_key(void)
{
    hashtable_t table;
    TEST_ASSERT_EQUAL_INT(0, hashtable_create(&table, hashtable_default_config(), _buffer, sizeof(_buffer)));

    uint8_t key1[128u];  // This key will be inserted
    uint8_t key2[128u];  // This key will not be inserted
    uint8_t value[128u];

    // Just need 2 unique keys
    (void) memset(key1, 0xaa, sizeof(key1));
    (void) memset(key2, 0xbb, sizeof(key2));

    // Insert value with key1
    TEST_ASSERT_EQUAL_INT(0, hashtable_insert(&table, key1, sizeof(key1), value, sizeof(value)));

    // Try to retrieve key2, should fail with return value of 1
    uint8_t read_value[128u];
    size_t read_value_size;
    TEST_ASSERT_EQUAL_INT(1, hashtable_retrieve(&table, key2, sizeof(key2), read_value, &read_value_size));
}


// Tests that hashtable_remove returns 1 when the requested key does not exist
void test_hashtable_remove_no_such_key(void)
{
    hashtable_t table;
    TEST_ASSERT_EQUAL_INT(0, hashtable_create(&table, hashtable_default_config(), _buffer, sizeof(_buffer)));

    uint8_t key1[128u];  // This key will be inserted
    uint8_t key2[128u];  // This key will not be inserted
    uint8_t value[128u];

    // Just need 2 unique keys
    (void) memset(key1, 0xaa, sizeof(key1));
    (void) memset(key2, 0xbb, sizeof(key2));

    // Insert value with key1
    TEST_ASSERT_EQUAL_INT(0, hashtable_insert(&table, key1, sizeof(key1), value, sizeof(value)));

    // Try to remove key2, should fail with return value of 1
    TEST_ASSERT_EQUAL_INT(1, hashtable_remove(&table, key2, sizeof(key2)));
}


// Tests that all items exist in the table and can be retrieved, after 1000 items are inserted
void test_hashtable_insert1000items(void)
{
    hashtable_t table;
    TEST_ASSERT_EQUAL_INT(0, hashtable_create(&table, hashtable_default_config(), _buffer, sizeof(_buffer)));

    const unsigned int num_items = 1000;
    _test_keyval_pair_t pairs[num_items];

    _generate_random_items_and_insert(&table, pairs, num_items);

    _verify_table_contents(&table, pairs, num_items);
}


// Tests that all expected items exist in the table and can be retrieved, after 1000 items are inserted
// and then 500 items are removed
void test_hashtable_insert1000items_remove500(void)
{
    hashtable_t table;
    TEST_ASSERT_EQUAL_INT(0, hashtable_create(&table, hashtable_default_config(), _buffer, sizeof(_buffer)));

    const unsigned int num_items = 1000;
    _test_keyval_pair_t pairs[num_items];

    _generate_random_items_and_insert(&table, pairs, num_items);

    _remove_random_items(&table, pairs, num_items, 500);

    _verify_table_contents(&table, pairs, num_items);
}


// Tests that iterating through all items via hashtable_next_item yields the same
// data as retrieving items via hashtable_retrieve, after inserting 1000 items
void test_hashtable_next_item_iterate1000items(void)
{
    hashtable_t table;
    TEST_ASSERT_EQUAL_INT(0, hashtable_create(&table, hashtable_default_config(), _buffer, sizeof(_buffer)));

    const unsigned int num_items = 1000;
    _test_keyval_pair_t pairs[num_items];

    _generate_random_items_and_insert(&table, pairs, num_items);

    _verify_table_contents(&table, pairs, num_items);

    _verify_iterated_table_contents(&table, pairs, num_items, 0);
}


// Tests that iterating through all items via hashtable_next_item yields the same
// data as retrieving items via hashtable_retrieve, after inserting 1000 items and removing 500
void test_hashtable_next_item_iterate1000items_remove500(void)
{
    hashtable_t table;
    TEST_ASSERT_EQUAL_INT(0, hashtable_create(&table, hashtable_default_config(), _buffer, sizeof(_buffer)));

    const unsigned int num_items = 1000;
    _test_keyval_pair_t pairs[num_items];

    _generate_random_items_and_insert(&table, pairs, num_items);

    _verify_table_contents(&table, pairs, num_items);
    _verify_iterated_table_contents(&table, pairs, num_items, 0);

    _remove_random_items(&table, pairs, num_items, 500);

    _verify_table_contents(&table, pairs, num_items);
    _verify_iterated_table_contents(&table, pairs, num_items, 500);
}


// Tests that the value reported by hashtable_bytes_remaining does not change
// after inserting the same 1000 items that were previously inserted and removed
void test_hashtable_bytes_remaining_unchanged_after_reinserting_removed_items(void)
{
    hashtable_t table;
    TEST_ASSERT_EQUAL_INT(0, hashtable_create(&table, hashtable_default_config(), _buffer, sizeof(_buffer)));

    const unsigned int num_items = 1000;
    _test_keyval_pair_t pairs[num_items];

    _generate_random_items_and_insert(&table, pairs, num_items);

    size_t bytes_remaining_1 = 0u;
    TEST_ASSERT_EQUAL_INT(0, hashtable_bytes_remaining(&table, &bytes_remaining_1));

    // Remove all items
    for (unsigned int i = 0; i < num_items; i++)
    {
        TEST_ASSERT_EQUAL_INT(0, hashtable_remove(&table, pairs[i].key, pairs[i].key_size));
    }

    size_t bytes_remaining_2 = 0u;
    TEST_ASSERT_EQUAL_INT(0, hashtable_bytes_remaining(&table, &bytes_remaining_2));

    // Bytes remaining should not have changed after removing all items
    TEST_ASSERT_EQUAL_INT(bytes_remaining_1, bytes_remaining_2);

    // Re-insert all items
    for (unsigned int i = 0; i < num_items; i++)
    {
        TEST_ASSERT_EQUAL_INT(0, hashtable_insert(&table, pairs[i].key, pairs[i].key_size,
                                                  pairs[i].value, pairs[i].value_size));
    }

    size_t bytes_remaining_3 = 0u;
    TEST_ASSERT_EQUAL_INT(0, hashtable_bytes_remaining(&table, &bytes_remaining_3));

    // Bytes remaining should not have changed after re-inserting all items,
    // since all the space we need should be in the free list
    TEST_ASSERT_EQUAL_INT(bytes_remaining_2, bytes_remaining_3);
}


// Tests that the value reported by hashtable_bytes_remaining does not change after
// overwriting an existing key with a value of the same size
void test_hashtable_bytes_remaining_overwrite_samesizevalue(void)
{
    hashtable_t table;
    TEST_ASSERT_EQUAL_INT(0, hashtable_create(&table, hashtable_default_config(), _buffer, sizeof(_buffer)));

    // Get bytes remaining before inserting anything
    size_t bytes_remaining_1 = 0u;
    TEST_ASSERT_EQUAL_INT(0, hashtable_bytes_remaining(&table, &bytes_remaining_1));

    char key[MAX_STR_LEN + 1];
    char value1[MAX_STR_LEN + 1];
    size_t key_size = 0u;
    size_t value1_size = 0u;

    // Generate initial key/value pair and insert it
    _rand_str(key, &key_size);
    _rand_str(value1, &value1_size);

    TEST_ASSERT_EQUAL_INT(0, hashtable_insert(&table, key, key_size, value1, value1_size));
    TEST_ASSERT_EQUAL_INT(1u, table.entry_count);

    size_t bytes_remaining_2 = 0u;
    TEST_ASSERT_EQUAL_INT(0, hashtable_bytes_remaining(&table, &bytes_remaining_2));

    // Should be fewer bytes remaining after inserting first item
    TEST_ASSERT_TRUE(bytes_remaining_2 < bytes_remaining_1);

    // Generate a new value (all 0xff this time) and insert it with the same key,
    // just has to be the same size as the original value
    char value2[MAX_STR_LEN + 1];
    (void) memset(value2, 0xff, sizeof(value2));
    size_t value2_size = value1_size;

    TEST_ASSERT_EQUAL_INT(0, hashtable_insert(&table, key, key_size, value2, value2_size));
    TEST_ASSERT_EQUAL_INT(1u, table.entry_count);

    size_t bytes_remaining_3 = 0u;
    TEST_ASSERT_EQUAL_INT(0, hashtable_bytes_remaining(&table, &bytes_remaining_3));

    // Bytes remaining should be unchanged, since the new value was the same size
    TEST_ASSERT_EQUAL_INT(bytes_remaining_2, bytes_remaining_3);

    // Retrieve item and verify value matches new value
    char read_value[MAX_STR_LEN + 1];
    size_t read_size = 0u;
    TEST_ASSERT_EQUAL_INT(0, hashtable_retrieve(&table, key, key_size, read_value, &read_size));

    TEST_ASSERT_EQUAL_INT(value2_size, read_size);
    TEST_ASSERT_EQUAL_INT(0, memcmp(value2, read_value, read_size));
}


// Tests that the value reported by hashtable_bytes_remaining does not change after
// overwriting an existing key with a value of a smaller size
void test_hashtable_bytes_remaining_overwrite_smallervalue(void)
{
    hashtable_t table;
    TEST_ASSERT_EQUAL_INT(0, hashtable_create(&table, hashtable_default_config(), _buffer, sizeof(_buffer)));

    // Get bytes remaining before inserting anything
    size_t bytes_remaining_1 = 0u;
    TEST_ASSERT_EQUAL_INT(0, hashtable_bytes_remaining(&table, &bytes_remaining_1));

    char key[MAX_STR_LEN + 1];
    char value1[MAX_STR_LEN + 1];
    size_t key_size = 0u;
    size_t value1_size = 0u;

    // Generate initial key/value pair and insert it
    _rand_str(key, &key_size);
    _rand_str(value1, &value1_size);

    TEST_ASSERT_EQUAL_INT(0, hashtable_insert(&table, key, key_size, value1, value1_size));
    TEST_ASSERT_EQUAL_INT(1u, table.entry_count);

    size_t bytes_remaining_2 = 0u;
    TEST_ASSERT_EQUAL_INT(0, hashtable_bytes_remaining(&table, &bytes_remaining_2));

    // Should be fewer bytes remaining after inserting first item
    TEST_ASSERT_TRUE(bytes_remaining_2 < bytes_remaining_1);

    // Generate a new value (all 0xff this time) and insert it with the same key,
    // just has to be the same size as the original value
    char value2[MAX_STR_LEN + 1];
    (void) memset(value2, 0xff, sizeof(value2));
    size_t value2_size = value1_size - 1u;

    TEST_ASSERT_EQUAL_INT(0, hashtable_insert(&table, key, key_size, value2, value2_size));
    TEST_ASSERT_EQUAL_INT(1u, table.entry_count);

    size_t bytes_remaining_3 = 0u;
    TEST_ASSERT_EQUAL_INT(0, hashtable_bytes_remaining(&table, &bytes_remaining_3));

    // Bytes remaining should be unchanged, since the new value was smaller and can fit in the old slot
    TEST_ASSERT_EQUAL_INT(bytes_remaining_2, bytes_remaining_3);

    // Retrieve item and verify value matches new value
    char read_value[MAX_STR_LEN + 1];
    size_t read_size = 0u;
    TEST_ASSERT_EQUAL_INT(0, hashtable_retrieve(&table, key, key_size, read_value, &read_size));

    TEST_ASSERT_EQUAL_INT(value2_size, read_size);
    TEST_ASSERT_EQUAL_INT(0, memcmp(value2, read_value, read_size));
}


// Tests that the value reported by hashtable_bytes_remaining decreases after
// overwriting an existing key with a value of a larger size
void test_hashtable_bytes_remaining_overwrite_largervalue(void)
{
    hashtable_t table;
    TEST_ASSERT_EQUAL_INT(0, hashtable_create(&table, hashtable_default_config(), _buffer, sizeof(_buffer)));

    // Get bytes remaining before inserting anything
    size_t bytes_remaining_1 = 0u;
    TEST_ASSERT_EQUAL_INT(0, hashtable_bytes_remaining(&table, &bytes_remaining_1));

    char key[MAX_STR_LEN + 1];
    char value1[MAX_STR_LEN + 1];
    size_t key_size = 5u;
    size_t value1_size = 5u;

    // Generate initial key/value pair and insert it
    (void) memset(key, 0xaa, key_size);
    (void) memset(value1, 0xbb, value1_size);

    TEST_ASSERT_EQUAL_INT(0, hashtable_insert(&table, key, key_size, value1, value1_size));
    TEST_ASSERT_EQUAL_INT(1u, table.entry_count);

    size_t bytes_remaining_2 = 0u;
    TEST_ASSERT_EQUAL_INT(0, hashtable_bytes_remaining(&table, &bytes_remaining_2));

    // Should be fewer bytes remaining after inserting first item
    TEST_ASSERT_TRUE(bytes_remaining_2 < bytes_remaining_1);

    // Generate a new value (all 0xff this time) and insert it with the same key,
    // just has to be the same size as the original value
    char value2[MAX_STR_LEN + 1];
    size_t value2_size = value1_size + 1;

    (void) memset(value2, 0xff, sizeof(value2_size));

    TEST_ASSERT_EQUAL_INT(0, hashtable_insert(&table, key, key_size, value2, value2_size));
    TEST_ASSERT_EQUAL_INT(1u, table.entry_count);

    size_t bytes_remaining_3 = 0u;
    TEST_ASSERT_EQUAL_INT(0, hashtable_bytes_remaining(&table, &bytes_remaining_3));

    // Bytes remaining should be decreased, since the new value was larger
    TEST_ASSERT_TRUE(bytes_remaining_3 < bytes_remaining_2);

    // Retrieve item and verify value matches new value
    char read_value[MAX_STR_LEN + 1];
    size_t read_size = 0u;
    TEST_ASSERT_EQUAL_INT(0, hashtable_retrieve(&table, key, key_size, read_value, &read_size));

    TEST_ASSERT_EQUAL_INT(value2_size, read_size);
    TEST_ASSERT_EQUAL_INT(0, memcmp(value2, read_value, read_size));
}


int main(void)
{
    UNITY_BEGIN();

    // Boring/necessary input validation tests
    RUN_TEST(test_hashtable_create_null_table);
    RUN_TEST(test_hashtable_create_null_config);
    RUN_TEST(test_hashtable_create_null_hash_func);
    RUN_TEST(test_hashtable_create_zero_array_count);
    RUN_TEST(test_hashtable_create_buffer_size_too_small);
    RUN_TEST(test_hashtable_insert_null_table);
    RUN_TEST(test_hashtable_insert_null_key);
    RUN_TEST(test_hashtable_insert_null_value);
    RUN_TEST(test_hashtable_insert_zero_key_size);
    RUN_TEST(test_hashtable_insert_zero_value_size);
    RUN_TEST(test_hashtable_insert_null_table);
    RUN_TEST(test_hashtable_insert_null_key);
    RUN_TEST(test_hashtable_insert_zero_key_size);
    RUN_TEST(test_hashtable_retrieve_null_table);
    RUN_TEST(test_hashtable_retrieve_null_key);
    RUN_TEST(test_hashtable_retrieve_null_value);
    RUN_TEST(test_hashtable_has_key_null_table);
    RUN_TEST(test_hashtable_has_key_null_key);
    RUN_TEST(test_hashtable_bytes_remaining_null_table);
    RUN_TEST(test_hashtable_bytes_remaining_null_key);
    RUN_TEST(test_hashtable_next_item_null_table);
    RUN_TEST(test_hashtable_next_item_null_key);
    RUN_TEST(test_hashtable_next_item_null_value);
    RUN_TEST(test_hashtable_reset_cursor_null_table);

    // Woohoo now the more fun tests
    RUN_TEST(test_hashtable_insert_buffer_full);
    RUN_TEST(test_hashtable_retrieve_no_such_key);
    RUN_TEST(test_hashtable_remove_no_such_key);
    RUN_TEST(test_hashtable_insert1000items);
    RUN_TEST(test_hashtable_insert1000items_remove500);
    RUN_TEST(test_hashtable_next_item_iterate1000items);
    RUN_TEST(test_hashtable_next_item_iterate1000items_remove500);
    RUN_TEST(test_hashtable_bytes_remaining_unchanged_after_reinserting_removed_items);
    RUN_TEST(test_hashtable_bytes_remaining_overwrite_samesizevalue);
    RUN_TEST(test_hashtable_bytes_remaining_overwrite_smallervalue);
    RUN_TEST(test_hashtable_bytes_remaining_overwrite_largervalue);

    return UNITY_END();
}

