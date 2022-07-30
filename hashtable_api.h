#include <stdint.h>


typedef struct
{
    void* (*alloc)(size_t size);
    void (*free)(void *ptr);
    uint32_t (*hash)(const void *data, const size_t size);
} hashtable_config_t;

typedef struct
{
    hashtable_config_t config;    ///< Hashtable config data
    uint32_t entry_count;         ///< Number of entries in the table
    size_t data_size;             ///< Size of data section
    void *table_data;             ///< Pointer to data section (size not known at compile time)
} hashtable_t;

int hashtable_create(hashtable_t *table, const hashtable_config_t *config);

int hashtable_insert(hashtable_t *table, const void *key, const size_t key_size,
                     const void *value, const size_t value_size);

int hashtable_remove(hashtable_t *table, const void *key, const size_t key_size);

int hashtable_retrieve(hashtable_t *table, const void *key, const size_t key_size,
                       void *value, size_t *value_size);

int hashtable_has_key(hashtable_t *table, const void *key, const size_t key_size);

int hashtable_destroy(hashtable_t *table);

char *hashtable_error_message(void);
