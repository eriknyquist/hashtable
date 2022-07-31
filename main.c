#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "hashtable_api.h"


#define MAX_STRING_SIZE (32u)


static void _dump_table(hashtable_t *table)
{
    uint8_t key[MAX_STRING_SIZE + 1u];
    size_t key_size;

    uint8_t value[MAX_STRING_SIZE + 1];
    size_t value_size;

    (void) hashtable_reset_cursor(table);

    while (hashtable_next_item(table, &key, &key_size, &value, &value_size) == 0)
    {
        // NULL-terminate the strings and print them
        key[key_size] = '\0';
        value[value_size] = '\0';
        printf("%s: %s\n", key, value);
    }
}


int main(void)
{
    uint8_t buf[4096];
    hashtable_t table;

    int ret = hashtable_create(&table, hashtable_default_config(), buf, sizeof(buf));
    if (ret < 0)
    {
        printf("%s\n", hashtable_error_message());
        return -1;
    }

    ret = hashtable_insert(&table, "key1", 4u, "val1", 4u);
    if (ret < 0)
    {
        printf("%s\n", hashtable_error_message());
        return -1;
    }

    ret = hashtable_insert(&table, "key2", 4u, "val2", 4u);
    if (ret < 0)
    {
        printf("%s\n", hashtable_error_message());
        return -1;
    }

    ret = hashtable_insert(&table, "key3", 4u, "val3", 4u);
    if (ret < 0)
    {
        printf("%s\n", hashtable_error_message());
        return -1;
    }

    ret = hashtable_insert(&table, "key4", 4u, "val4", 4u);
    if (ret < 0)
    {
        printf("%s\n", hashtable_error_message());
        return -1;
    }

    _dump_table(&table);

    return 0;
}
