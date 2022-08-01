/**
 * @file hashtable.c
 *
 * @author Erik K. Nyquist
 *
 * @brief Implements a separate-chaining hashtable that accepts any/all types of data for keys and values.
 */


#include <string.h>
#include "hashtable_api.h"


/**
 * @brief Max. size for an error message string
 */
#define MAX_ERROR_MSG_SIZE  (256u)

/**
 * @brief Internal error message macro
 */
#define ERROR(msg) ((void) strncpy(_error_msg, msg, sizeof(_error_msg)))

/**
 * New tables must have at least this much space available for data
 */
#define MIN_REQUIRED_DATA_SIZE (128u)


/**
 * @brief Helper macro for getting the size of a _keyval_pair_list_table_t section,
 * given a specific number of array elements
 */
#define ARRAY_SIZE_BYTES(array_count) \
    (((array_count) * sizeof(_keyval_pair_list_t)) + sizeof(_keyval_pair_list_table_t))


/**
 * Holds information about a single key/value pair stored in the table.
 * Also represents a single node in a single-linked list of key/value pairs.
 */
typedef struct _keyval_pair
{
    struct _keyval_pair *next;    ///< Pointer to next key/val pair in the list
    size_t key_size;              ///< Size of key data in bytes
    size_t value_size;            ///< Size of value data in bytes
    uint8_t data[];               ///< Start of key + value data packed together
} _keyval_pair_t;


/**
 * Represents a singly-linked list of key/value pairs
 */
typedef struct
{
    _keyval_pair_t *head;  ///< Head (first) item
    _keyval_pair_t *tail;  ///< Tail (last) item
} _keyval_pair_list_t;


/**
 * Represents the area where key/val pair data is stored
 */
typedef struct
{
    _keyval_pair_list_t freelist;  ///< List of freed key/value pairs
    size_t total_bytes;            ///< Total bytes available for key/value pair data
    size_t bytes_used;             ///< Total bytes used (including freed) by key/value pair data
    uint8_t data[];                ///< Pointer to key/value data section, size not known at compile time
} _keyval_pair_data_block_t;


/**
 * Represents a table of singly-linked lists of key-value pair
 */
typedef struct
{
    uint32_t array_count;          ///< Number of _keyval_pair_list_t slots in the array
    _keyval_pair_list_t table[];   ///< Pointer to first slot in table
} _keyval_pair_list_table_t;


/**
 * First section in the table->table_data field, holds some misc. housekeeping data.
 *
 * table->table_data layout/format:
 *
 * The table->table_data field points to the buffer area passed to 'hashtable_create',
 * and contains the following data:
 *
 *  +-----------------------------+ <--- Lowest address of table->table_data
 *  |                             |
 *  | _keyval_pair_table_data_t   |
 *  |                             |
 *  +-----------------------------+
 *  |                             |
 *  | _keyval_pair_list_table_t   |
 *  |                             |
 *  +-----------------------------+
 *  |                             |
 *  | _keyval_pair_list_t table[] |
 *  | array items                 |
 *  |                             |
 *  +-----------------------------+
 *  |                             |
 *  | _keyval_pair_data_block_t   |
 *  |                             |
 *  +-----------------------------+
 *  |                             |
 *  | data block data[] section   |
 *  |                             |
 *  +-----------------------------+
 */
typedef struct
{
    _keyval_pair_list_table_t *list_table;  ///< Convenience pointer to table array
    _keyval_pair_data_block_t *data_block;  ///< Convenience pointer to key/val data block
    uint32_t cursor_array_index;            ///< Cursor current table index for iteration
    uint32_t cursor_items_traversed;        ///< Number of items traversed by cursor
    _keyval_pair_t *cursor_item;            ///< Cursor current item pointer for iteration
    uint8_t cursor_limit;                   ///< Set to 1 when all items have been iterated through
} _keyval_pair_table_data_t;


static char _error_msg[MAX_ERROR_MSG_SIZE]  = {'\0'};


// Default hash function
static uint32_t _default_hash(const void *data, const size_t size)
{
    static const uint32_t magic_multiplier = 37u;
    uint8_t *u8_data = (uint8_t *) data;
    uint32_t ret = 0u;

    for (size_t i = 0u; i < size; i++)
    {
        ret = magic_multiplier * ret + u8_data[i];
    }

    ret += (ret >> 5u);
    return ret;
}

// Default hashtable_config_t data
static hashtable_config_t _default_config = {_default_hash, 64u};


/**
 * Calculate a has for the given key data, and return a pointer to the list at the
 * corresponding table index, such that 'table_index := hash (mod) max_array_count'
 *
 * @param table     Pointer to hashtable instance
 * @param key       Pointer to key data
 * @param key_size  Key data size in bytes
 *
 * @return Pointer to list at corresponding table index
 */
static _keyval_pair_list_t *_get_table_list_by_key(hashtable_t *table, const void *key, const size_t key_size)
{
    uint32_t hash = table->config.hash(key, key_size);

    _keyval_pair_table_data_t *td = (_keyval_pair_table_data_t *) table->table_data;
    uint32_t table_index = hash % td->list_table->array_count;
    return &td->list_table->table[table_index];
}


/**
 * Search the list of freed key/value pairs, for one that is the same size or larger than
 * a specific size. If found, the pair will be removed from the free list and a pointer
 * to the pair will be returned.
 *
 * @param table          Pointer to hashtable instance
 * @param size_required  Number of bytes needed, look for a freed pair equal to or larger than this
 *
 * @return Pointer to pair that satisfies size requirement, or NULL if none was found
 */
static _keyval_pair_t *_search_free_list(hashtable_t *table, size_t size_required)
{
    _keyval_pair_table_data_t *td = (_keyval_pair_table_data_t *) table->table_data;
    _keyval_pair_t *curr = td->data_block->freelist.head;
    _keyval_pair_t *prev = NULL;

    while (NULL != curr)
    {
        size_t size_available = sizeof(_keyval_pair_t) + curr->key_size + curr->value_size;
        if (size_available >= size_required)
        {
            /* Found a freed pair that is the same size or larger than what we need,
             * so remove it from the free list and return a pointer */
            if (curr == td->data_block->freelist.head)
            {
                td->data_block->freelist.head = curr->next;
            }

            if (curr == td->data_block->freelist.tail)
            {
                td->data_block->freelist.tail = prev;
            }

            if (NULL != prev)
            {
                prev->next = curr->next;
            }

            curr->next = NULL;
            return curr;
        }

        // Save pointer from previous iteration, in case we need to remove the next item
        prev = curr;
        curr = curr->next;
    }

    return NULL;
}


/**
 * Store a new key/value pair in the table->table_data section of a hashtable.
 *
 * This function will first try to find a suitable existing key/value pair in the
 * free list (data_block->freelist). If there is none, it will try to carve out the
 * required space in data_block->data. If data_block->data doesn't have the required
 * space, then a NULL pointer is returned.
 *
 * @param table       Pointer to hashtable instance
 * @param key         Pointer to key data
 * @param key_size    Key data size in bytes
 * @param value       Pointer to value data
 * @param value_size  Value data size in bytes
 *
 * @return Pointer to stored key/value pair, or NULL if there was not sufficient space to store
 */
static _keyval_pair_t *_store_keyval_pair(hashtable_t *table, const void *key, const size_t key_size,
                                          const void *value, const size_t value_size)
{
    _keyval_pair_t *ret = NULL;
    size_t size_required = sizeof(_keyval_pair_t) + key_size + value_size;

    // Prefer finding a suitable block in the free list, so check there first
    ret = _search_free_list(table, size_required);
    if (NULL == ret)
    {
        // Nothing suitable in the free list, see if we can carve out space in the data block
        _keyval_pair_table_data_t *td = (_keyval_pair_table_data_t *) table->table_data;
        size_t size_remaining = td->data_block->total_bytes - td->data_block->bytes_used;

        if (size_required > size_remaining)
        {
            // Not enough space
            return NULL;
        }

        // There is space in the data block
        ret = (_keyval_pair_t *) (td->data_block->data + td->data_block->bytes_used);

        // Increment bytes used
        td->data_block->bytes_used += size_required;
    }

    // Populate new entry
    ret->next = NULL;
    ret->key_size = key_size;
    ret->value_size = value_size;
    (void) memcpy(ret->data, key, key_size);
    (void) memcpy(ret->data + key_size, value, value_size);

    return ret;
}


/**
 * Initialize the buffer for a new table structure
 *
 * @param table        Pointer to hashtable instance
 * @param array_count  Key/value pair list table array count
 * @param buffer       Pointer to location to buffer area
 * @param buffer_size  Buffer area size in bytes
 *
 * @return 0 if successful, -1 if an error occurred
 */
static int _setup_new_table(hashtable_t *table, uint32_t array_count, void *buffer, size_t buffer_size)
{
    size_t array_size = ARRAY_SIZE_BYTES(array_count);
    size_t min_required_size = sizeof(_keyval_pair_table_data_t) + array_size +
                               sizeof(_keyval_pair_data_block_t) + MIN_REQUIRED_DATA_SIZE;

    if (buffer_size < min_required_size)
    {
        ERROR("Allocated size is too small");
        return -1;
    }

    uint8_t *u8_ret = (uint8_t *) buffer;

    // Populate convenience pointers
    _keyval_pair_table_data_t *td = (_keyval_pair_table_data_t *) buffer;
    td->list_table = (_keyval_pair_list_table_t *) (u8_ret + sizeof(_keyval_pair_table_data_t));
    td->data_block = (_keyval_pair_data_block_t *) (u8_ret + sizeof(_keyval_pair_table_data_t) + array_size);

    // NULL-ify all the array entries
    (void) memset(td->list_table, 0, array_size);

    // Initialize cursor values
    td->cursor_array_index = 0u;
    td->cursor_items_traversed = 0u;
    td->cursor_item = td->list_table->table[0].head;
    td->cursor_limit = 0u;

    td->list_table->array_count = array_count;

    // Initialize key/pair value data block
    td->data_block->freelist.head = NULL;
    td->data_block->freelist.tail = NULL;
    td->data_block->total_bytes = buffer_size - min_required_size;
    td->data_block->bytes_used = 0u;

    return 0;
}


/**
 * Search a single key/pair list for match key data
 *
 * @param table      Pointer to hashtable instance
 * @param list       Pointer to key/val pair list to search
 * @param key        Pointer to key data
 * @param key_size   Size of key data in bytes
 * @param previous   Pointer to location to store pointer to the item before the
 *                   matching item. Will only be populated if a matching item is
 *                   found (this may be needed if the caller wants to unlink the
 *                   matching item from the list).
 *
 * @return Pointer to key/val pair with matching key data, or NULL if none was found
 */
static _keyval_pair_t *_search_list_by_key(hashtable_t *table, _keyval_pair_list_t *list,
                                           const void *key, const size_t key_size, _keyval_pair_t **previous)
{
    _keyval_pair_t *curr = list->head;
    _keyval_pair_t *prev = NULL;

    while (NULL != curr)
    {
        if (curr->key_size == key_size)
        {
            if (0 == memcmp(key, curr->data, key_size))
            {
                if (NULL != previous)
                {
                    *previous = prev;
                }

                return curr;
            }
        }

        prev = curr;
        curr = curr->next;
    }

    return NULL;
}


/**
 * Unlink a stored key/val pair from a specific key/val list, and add it to the free list
 *
 * @param table   Pointer to hashtable instance
 * @param list    Pointer to list to remove key/val pair list from
 * @param item    Pointer to key/val pair to remove
 * @param prev    Pointer to item before the key/val pair to be removed
 *
 * @return 0 if successful, -1 if an error occurred
 */
static int _remove_from_table(hashtable_t *table, _keyval_pair_list_t *list,
                              _keyval_pair_t *item, _keyval_pair_t *prev)
{
    // Remove item from table list
    if (item == list->head)
    {
        list->head = item->next;
    }

    if (item == list->tail)
    {
        list->tail = prev;
    }

    if (NULL != prev)
    {
        prev->next = item->next;
    }

    // Add item to free list
    _keyval_pair_table_data_t *td = (_keyval_pair_table_data_t *) table->table_data;
    _keyval_pair_list_t *freelist = &td->data_block->freelist;

    if (NULL == freelist->head)
    {
        freelist->head = item;
        freelist->tail = item;
    }
    else
    {
        freelist->tail->next = item;
        freelist->tail = item;
    }

    table->entry_count -= 1u;
    return 0;
}


/**
 * Store a new key/value pair and insert references into the list table.
 * Uses the following steps:
 *
 * 1. First, look up the given key to see if it already exists in the hashtable.
 *    If it *does* already exist, and the space allocated is large enough for the
 *    new value data being inserted, then all we have to do is write the new value
 *    data in-place to the already-stored key/value. This would be optimal, both
 *    in terms of execution time and memory efficiency.
 *
 *    If the given key *did* exist, but the available size was not large enough for the
 *    new value data, then we need to remove the existing key/pair data for the given
 *    key, and add it to the free list, since the next step involves storing new
 *    key/pair data with the given key.
 *
 * 2. If the previous step failed, invoke _store_keyval_pair to find storage (either
 *    from the free list, or by allocating new space in the data block) for the new
 *    key/val pair.
 *
 * @param table       Pointer to hashtable instance
 * @param key         Pointer to key data
 * @param key_size    Key data size in bytes
 * @param value       Pointer to value data
 * @param value_size  Value data size in bytes
 *
 * @return 0 if successful, -1 if enough space was not available
 */
static int _insert_keyval_pair(hashtable_t *table, const void *key, const size_t key_size,
                               const void *value, const size_t value_size)
{
    _keyval_pair_list_t *list = _get_table_list_by_key(table, key, key_size);

    _keyval_pair_t *prev = NULL;
    _keyval_pair_t *pair = _search_list_by_key(table, list, key, key_size, &prev);
    if (NULL != pair)
    {
        // Item with this key already exists, check if new item can fit in existing slot
        if (value_size <= pair->value_size)
        {
            // New value is the same size or smaller than existing, easy/quick update
            (void) memcpy(pair->data + pair->key_size, value, value_size);
            pair->value_size = value_size;
            return 0;
        }
        else
        {
            // Existing item is too small, need to remove it and insert a new item
            if (_remove_from_table(table, list, pair, prev) < 0)
            {
                return -1;
            }
        }
    }

    // No item with this key exists, try to allocate new space
    pair = _store_keyval_pair(table, key, key_size, value, value_size);
    if (NULL == pair)
    {
        return -1;
    }

    // Add new key/val pair into the list at the current slot in the table
    if (NULL == list->head)
    {
        list->head = pair;
        list->tail = pair;
    }
    else
    {
        list->tail->next = pair;
        list->tail = pair;
    }

    pair->next = NULL;
    table->entry_count += 1u;

    return 0;
}


/**
 * @see hashtable_api.h
 */
int hashtable_create(hashtable_t *table, const hashtable_config_t *config,
                     void *buffer, size_t buffer_size)
{
    if ((NULL == table) || (NULL == config))
    {
        ERROR("NULL pointer passed to function");
        return -1;
    }

    if (NULL == config->hash)
    {
        ERROR("NULL function pointer in hashtable_config_t");
        return -1;
    }

    if (0u == config->initial_array_count)
    {
        ERROR("Zero array count in hashtable_config_t");
        return -1;
    }

    (void) memcpy(&table->config, config, sizeof(table->config));

    if (_setup_new_table(table, config->initial_array_count, buffer, buffer_size) < 0)
    {
        return -1;
    }

    table->entry_count = 0u;
    table->table_data = buffer;
    return 0;
}


/**
 * @see hashtable_api.h
 */
int hashtable_insert(hashtable_t *table, const void *key, const size_t key_size,
                     const void *value, const size_t value_size)
{
    if ((NULL == table) || (NULL == key) || (NULL == value))
    {
        ERROR("NULL pointer passed to function");
        return -1;
    }

    if ((0u == key_size) || (0u == value_size))
    {
        ERROR("Invalid size value passed to function");
        return -1;
    }

    return _insert_keyval_pair(table, key, key_size, value, value_size);
}


/**
 * @see hashtable_api.h
 */
int hashtable_remove(hashtable_t *table, const void *key, const size_t key_size)
{
    if ((NULL == table) || (NULL == key))
    {
        ERROR("NULL pointer passed to function");
        return -1;
    }

    if (0u == key_size)
    {
        ERROR("Invalid size value passed to function");
        return -1;
    }

    _keyval_pair_list_t *list = _get_table_list_by_key(table, key, key_size);

    _keyval_pair_t *prev = NULL;
    _keyval_pair_t *pair = _search_list_by_key(table, list, key, key_size, &prev);
    if (NULL == pair)
    {
        // Item does not exist
        return 0;
    }

    return _remove_from_table(table, list, pair, prev);
}


/**
 * @see hashtable_api.h
 */
int hashtable_retrieve(hashtable_t *table, const void *key, const size_t key_size,
                       void *value, size_t *value_size)
{
    if ((NULL == table) || (NULL == key) || (NULL == value))
    {
        ERROR("NULL pointer passed to function");
        return -1;
    }

    _keyval_pair_list_t *list = _get_table_list_by_key(table, key, key_size);

    _keyval_pair_t *pair = _search_list_by_key(table, list, key, key_size, NULL);
    if (NULL == pair)
    {
        // Item does not exist
        ERROR("Requested key does not exist");
        return -1;
    }

    (void) memcpy(value, pair->data + pair->key_size, pair->value_size);

    if (NULL != value_size)
    {
        *value_size = pair->value_size;
    }

    return 0;
}


/**
 * @see hashtable_api.h
 */
int hashtable_has_key(hashtable_t *table, const void *key, const size_t key_size)
{
    if ((NULL == table) || (NULL == key))
    {
        ERROR("NULL pointer passed to function");
        return -1;
    }

    _keyval_pair_list_t *list = _get_table_list_by_key(table, key, key_size);

    _keyval_pair_t *pair = _search_list_by_key(table, list, key, key_size, NULL);
    if (NULL == pair)
    {
        // Item does not exist
        return 0;
    }

    return 1;
}


/**
 * @see hashtable_api.h
 */
int hashtable_bytes_remaining(hashtable_t *table, size_t *bytes_remaining)
{
    if ((NULL == table) || (NULL == bytes_remaining))
    {
        ERROR("NULL pointer passed to function");
        return -1;
    }


    _keyval_pair_table_data_t *td = (_keyval_pair_table_data_t *) table->table_data;
    *bytes_remaining = td->data_block->total_bytes - td->data_block->bytes_used;

    return 0;
}


/**
 * @see hashtable_api.h
 */
int hashtable_next_item(hashtable_t *table, void *key, size_t *key_size,
                        void *value, size_t *value_size)
{
    if ((NULL == table) || (NULL == key) || (NULL == value))
    {
        ERROR("NULL pointer passed to function");
        return -1;
    }

    _keyval_pair_table_data_t *td = (_keyval_pair_table_data_t *) table->table_data;

    if (td->cursor_limit)
    {
        // Cursor limit reached
        return 1;
    }

    // Look through lists until the last index, or until we've traversed all stored items
    while ((td->cursor_array_index < td->list_table->array_count) &&
           (td->cursor_items_traversed < table->entry_count))
    {
        _keyval_pair_list_t *list = &td->list_table->table[td->cursor_array_index];

        if (NULL == td->cursor_item)
        {
            /* If item pointer is null, we just moved to a new slot, so
             * set to the head of the current list */
            td->cursor_item = list->head;
        }

        // Copy out the next non-NULL item in the list
        if (NULL != td->cursor_item)
        {
            (void) memcpy(key, td->cursor_item->data, td->cursor_item->key_size);
            (void) memcpy(value, td->cursor_item->data + td->cursor_item->key_size,
                          td->cursor_item->value_size);

            if (NULL != key_size)
            {
                *key_size = td->cursor_item->key_size;
            }

            if (NULL != value_size)
            {
                *value_size = td->cursor_item->value_size;
            }

            td->cursor_item = td->cursor_item->next;
            if (NULL == td->cursor_item)
            {
                td->cursor_array_index += 1u;
            }

            td->cursor_items_traversed += 1u;
            return 0;
        }

        td->cursor_array_index += 1u;
    }

    td->cursor_limit = 1u;
    ERROR("Cursor limit reached");
    return 1;
}


/**
 * @see hashtable_api.h
 */
int hashtable_reset_cursor(hashtable_t *table)
{
    if (NULL == table)
    {
        ERROR("NULL pointer passed to function");
        return -1;
    }

    _keyval_pair_table_data_t *td = (_keyval_pair_table_data_t *) table->table_data;
    td->cursor_array_index = 0u;
    td->cursor_items_traversed = 0u;
    td->cursor_item = td->list_table->table[0].head;
    td->cursor_limit = 0u;

    return 0;
}


/**
 * @see hashtable_api.h
 */
hashtable_config_t *hashtable_default_config(void)
{
    return &_default_config;
}


/**
 * @see hashtable_api.h
 */
char *hashtable_error_message(void)
{
    return _error_msg;
}
