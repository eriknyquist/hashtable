#include <stdio.h>
#include <string.h>
#include "hashtable_api.h"


// Generates a new key/value pair on the stack and inserts into the given table
static int _insert_item(hashtable_t *table)
{
    char key[32u];
    char value[32u];

    int keysize = snprintf(key, sizeof(key), "My key #1");
    int valuesize = snprintf(value, sizeof(value), "My value #1");

    return hashtable_insert(table, key, keysize + 1, value, valuesize + 1);
}

// Retrieves the value from the given table using the same key, and prints it
static int _retrieve_and_print_item(hashtable_t *table)
{
    const char *key = "My key #1";
    size_t keysize = strlen(key) + 1u;
    char *value;
    hashtable_size_t valuesize;

    if (hashtable_retrieve(table, key, keysize, &value, &valuesize) != 0)
    {
        return -1;
    }

    printf("key='%s', value='%s', valuesize=%d\n", key, value, valuesize);
    return 0;
}


int main(void)
{
    // Hashtable instance
    hashtable_t table;

    // Allocate buffer of size 512 for storing the table + key/value data
    char buf[512];

    // Initialize a hashtable instance with 512 byte buffer
    if (hashtable_create(&table, NULL, buf, sizeof(buf)) != 0)
    {
        // Error
        return -1;
    }

    // Add a key/value pair to the hashtable
    if (_insert_item(&table) != 0)
    {
        // Error
        return -1;
    }

    // Retrieve inserted item and print it
    return _retrieve_and_print_item(&table);
}
