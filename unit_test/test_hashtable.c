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

        TEST_ASSERT_EQUAL_INT(0, hashtable_insert(table, pairs[i].key, pairs[i].key_size,
                                                  pairs[i].value, pairs[i].value_size));
    }
}


static void _verify_table_contents(hashtable_t *table, _test_keyval_pair_t *pairs, unsigned int num_items)
{
    for (unsigned int i = 0; i < num_items; i++)
    {
        // Verify table has key
        TEST_ASSERT_EQUAL_INT(1, hashtable_has_key(table, pairs[i].key, pairs[i].key_size));

        // Retrieve value and verify it matches what we put in
        char value[MAX_STR_LEN + 1u];
        size_t value_size;

        TEST_ASSERT_EQUAL_INT(0, hashtable_retrieve(table, pairs[i].key, pairs[i].key_size,
                                                    value, &value_size));
        TEST_ASSERT_EQUAL_INT(pairs[i].value_size, value_size);
        TEST_ASSERT_EQUAL_INT(0, memcmp(pairs[i].value, value, value_size));
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
void test_hashtable_create_zero_initial_array_count(void)
{
    hashtable_t table;
    hashtable_config_t config;

    // Make a copy of the default config
    (void) memcpy(&config, hashtable_default_config(), sizeof(config));

    config.initial_array_count = 0u;

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


// Tests that all items exist in the table and can be retrieved, after 10 items are inserted
void test_hashtable_insert_100items(void)
{
    hashtable_t table;
    TEST_ASSERT_EQUAL_INT(0, hashtable_create(&table, hashtable_default_config(), _buffer, sizeof(_buffer)));

    const unsigned int num_items = 10;
    _test_keyval_pair_t pairs[num_items];

    _generate_random_items_and_insert(&table, pairs, num_items);

    _verify_table_contents(&table, pairs, num_items);
}


int main(void)
{
    UNITY_BEGIN();

    // Boring/necessary input validation tests
    RUN_TEST(test_hashtable_create_null_table);
    RUN_TEST(test_hashtable_create_null_config);
    RUN_TEST(test_hashtable_create_null_hash_func);
    RUN_TEST(test_hashtable_create_zero_initial_array_count);
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
    RUN_TEST(test_hashtable_insert_100items);

    return UNITY_END();
}

