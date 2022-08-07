/**
 * @file hashtable.c
 *
 * @author Erik K. Nyquist
 *
 * @brief Implements a lightweight separate-chaining hashtable designed to be flexible
 *        enough for embedded systems.
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
 * If there is enough space, we will try to size the table array to
 * occupy this percentage of the buffer provided for a hashtable
 */
#define IDEAL_BUFFER_TABLE_PERCENT (12u)


/**
 * @brief Helper macro for getting the size of a _keyval_pair_list_table_t section,
 * given a specific number of array elements
 */
#define ARRAY_SIZE_BYTES(array_count) \
    (((array_count) * sizeof(_keyval_pair_list_t)) + sizeof(_keyval_pair_list_table_t))


static char _error_msg[MAX_ERROR_MSG_SIZE]  = {'\0'};


// Default hash function
static uint32_t _fnv1a_hash(const char *data, const hashtable_size_t size)
{
    // Constants taken from:
    // https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function
    const uint32_t fnv32_prime = 0x01000193u;
    uint32_t hash = 0x811c9dc5u;

    for (hashtable_size_t i = 0u; i < size; i++)
    {
        hash ^= (uint32_t) data[i];
        hash *= fnv32_prime;
    }

    return hash;
}


/**
 * Calculate a hash for the given key data, and return a pointer to the list at the
 * corresponding table index, such that 'table_index := hash (mod) max_array_count'
 *
 * @param table     Pointer to hashtable instance
 * @param key       Pointer to key data
 * @param key_size  Key data size in bytes
 *
 * @return Pointer to list at corresponding table index
 */
static _keyval_pair_list_t *_get_table_list_by_key(hashtable_t *table, const char *key,
                                                   const hashtable_size_t key_size)
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
static _keyval_pair_t *_store_keyval_pair(hashtable_t *table, const char *key, const hashtable_size_t key_size,
                                          const char *value, const hashtable_size_t value_size)
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

    if ((0u < value_size) && (NULL != value))
    {
        (void) memcpy(ret->data + key_size, value, value_size);
    }

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
    size_t min_required_size = HASHTABLE_MIN_BUFFER_SIZE(array_count);

    if (buffer_size < min_required_size)
    {
        return 1;
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
                                           const char *key, const hashtable_size_t key_size,
                                           _keyval_pair_t **previous)
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

    item->next = NULL;

    // Is the list empty now?
    if (list->head == NULL)
    {
        table->array_slots_used -= 1u;
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
static int _insert_keyval_pair(hashtable_t *table, const char *key, const hashtable_size_t key_size,
                               const char *value, const hashtable_size_t value_size)
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
            if ((0u < value_size) && (NULL != value))
            {
                (void) memcpy(pair->data + pair->key_size, value, value_size);
            }

            pair->value_size = value_size;
            return 0;
        }
        else
        {
            // Existing item is too small, need to remove it and insert a new item
            if (_remove_from_table(table, list, pair, prev) < 0)
            {
                ERROR("Item removal failed");
                return -1;
            }
        }
    }

    // No item with this key exists, try to allocate new space
    pair = _store_keyval_pair(table, key, key_size, value, value_size);
    if (NULL == pair)
    {
        return 1;
    }

    // Add new key/val pair into the list at the current slot in the table
    if (NULL == list->head)
    {
        // First item in this slot
        list->head = pair;
        list->tail = pair;
        table->array_slots_used += 1u;
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
#ifndef HASHTABLE_DISABLE_PARAM_VALIDATION
    if (NULL == table)
    {
        ERROR("NULL pointer passed to function");
        return -1;
    }

    if (NULL == config)
    {
        // No config provided, use default config
        if (0 > hashtable_default_config(&table->config, buffer_size))
        {
            return -1;
        }
    }
    else
    {
        if (NULL == config->hash)
        {
            ERROR("NULL function pointer in hashtable_config_t");
            return -1;
        }

        if (0u == config->array_count)
        {
            ERROR("Zero array count in hashtable_config_t");
            return -1;
        }

        (void) memcpy(&table->config, config, sizeof(table->config));
    }
#endif // HASHTABLE_DISABLE_PARAM_VALIDATION

    int ret = _setup_new_table(table, table->config.array_count, buffer, buffer_size);
    if (0 != ret)
    {
        return ret;
    }

    table->array_slots_used = 0u;
    table->entry_count = 0u;
    table->table_data = buffer;
    table->data_size = buffer_size;

    return 0;
}


/**
 * @see hashtable_api.h
 */
int hashtable_insert(hashtable_t *table, const char *key, const hashtable_size_t key_size,
                     const char *value, const hashtable_size_t value_size)
{
#ifndef HASHTABLE_DISABLE_PARAM_VALIDATION
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
#endif // HASHTABLE_DISABLE_PARAM_VALIDATION

    return _insert_keyval_pair(table, key, key_size, value, value_size);
}


/**
 * @see hashtable_api.h
 */
int hashtable_remove(hashtable_t *table, const char *key, const hashtable_size_t key_size)
{
#ifndef HASHTABLE_DISABLE_PARAM_VALIDATION
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
#endif // HASHTABLE_DISABLE_PARAM_VALIDATION

    _keyval_pair_list_t *list = _get_table_list_by_key(table, key, key_size);

    _keyval_pair_t *prev = NULL;
    _keyval_pair_t *pair = _search_list_by_key(table, list, key, key_size, &prev);
    if (NULL == pair)
    {
        // Item does not exist
        return 1;
    }

    return _remove_from_table(table, list, pair, prev);
}


/**
 * @see hashtable_api.h
 */
int hashtable_retrieve(hashtable_t *table, const char *key, const hashtable_size_t key_size,
                       char **value, hashtable_size_t *value_size)
{
#ifndef HASHTABLE_DISABLE_PARAM_VALIDATION
    if ((NULL == table) || (NULL == key))
    {
        ERROR("NULL pointer passed to function");
        return -1;
    }
#endif // HASHTABLE_DISABLE_PARAM_VALIDATION

    _keyval_pair_list_t *list = _get_table_list_by_key(table, key, key_size);

    _keyval_pair_t *pair = _search_list_by_key(table, list, key, key_size, NULL);
    if (NULL == pair)
    {
        // Item does not exist
        return 1;
    }

    if ((NULL != value) && (0u < pair->value_size))
    {
        *value = (char *) (pair->data + pair->key_size);
    }

    if (NULL != value_size)
    {
        *value_size = pair->value_size;
    }

    return 0;
}


/**
 * @see hashtable_api.h
 */
int hashtable_has_key(hashtable_t *table, const char *key, const hashtable_size_t key_size)
{
#ifndef HASHTABLE_DISABLE_PARAM_VALIDATION
    if ((NULL == table) || (NULL == key))
    {
        ERROR("NULL pointer passed to function");
        return -1;
    }
#endif // HASHTABLE_DISABLE_PARAM_VALIDATION

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
#ifndef HASHTABLE_DISABLE_PARAM_VALIDATION
    if ((NULL == table) || (NULL == bytes_remaining))
    {
        ERROR("NULL pointer passed to function");
        return -1;
    }
#endif // HASHTABLE_DISABLE_PARAM_VALIDATION

    _keyval_pair_table_data_t *td = (_keyval_pair_table_data_t *) table->table_data;
    *bytes_remaining = td->data_block->total_bytes - td->data_block->bytes_used;

    return 0;
}


/**
 * @see hashtable_api.h
 */
int hashtable_next_item(hashtable_t *table, char **key, hashtable_size_t *key_size,
                        char **value, hashtable_size_t *value_size)
{
#ifndef HASHTABLE_DISABLE_PARAM_VALIDATION
    if ((NULL == table) || (NULL == key))
    {
        ERROR("NULL pointer passed to function");
        return -1;
    }
#endif // HASHTABLE_DISABLE_PARAM_VALIDATION

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

        // Copy out pointers to the next non-NULL item in the list
        if (NULL != td->cursor_item)
        {
            *key = (char *) td->cursor_item->data;

            if (0u < td->cursor_item->value_size)
            {
                *value = (char *) td->cursor_item->data + td->cursor_item->key_size;
            }

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
#ifndef HASHTABLE_DISABLE_PARAM_VALIDATION
    if (NULL == table)
    {
        ERROR("NULL pointer passed to function");
        return -1;
    }
#endif // HASHTABLE_DISABLE_PARAM_VALIDATION

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
 int hashtable_clear(hashtable_t *table)
 {
#ifndef HASHTABLE_DISABLE_PARAM_VALIDATION
    if (NULL == table)
    {
        ERROR("NULL pointer passed to function");
        return -1;
    }
#endif // HASHTABLE_DISABLE_PARAM_VALIDATION

    _keyval_pair_table_data_t *td = (_keyval_pair_table_data_t *) table->table_data;

    // NULL-ify all the array entries
    (void) memset(td->list_table->table, 0, td->list_table->array_count * sizeof(_keyval_pair_list_t));

    // Reset cursor values
    td->cursor_array_index = 0u;
    td->cursor_items_traversed = 0u;
    td->cursor_item = td->list_table->table[0].head;
    td->cursor_limit = 0u;

    // Reset key/pair value data block
    td->data_block->freelist.head = NULL;
    td->data_block->freelist.tail = NULL;
    td->data_block->total_bytes = table->data_size - HASHTABLE_MIN_BUFFER_SIZE(td->list_table->array_count);
    td->data_block->bytes_used = 0u;

    return 0;
 }


/**
 * @see hashtable_api.h
 */
int hashtable_default_config(hashtable_config_t *config, size_t buffer_size)
{
#ifndef HASHTABLE_DISABLE_PARAM_VALIDATION
    if (NULL == config)
    {
        ERROR("NULL pointer passed to function");
        return -1;
    }
#endif // HASHTABLE_DISABLE_PARAM_VALIDATION

    config->hash = _fnv1a_hash;

    /* We either want an array count that results in a table that takes up
     * roughly 10% of the buffer size, or an array count of at least 10-- whichever
     * takes up the most bytes in the buffer. */
    size_t buf_min_size = (buffer_size * IDEAL_BUFFER_TABLE_PERCENT) / 100u; // Ideal % of buffer size
    size_t array_min_size = ARRAY_SIZE_BYTES(HASHTABLE_MIN_ARRAY_COUNT);               // Size of an array with min. elements

    if (buf_min_size > array_min_size)
    {
        // Figure out the array count that is closest to the ideal % of the buffer size
        config->array_count = ((buf_min_size - sizeof(_keyval_pair_list_table_t)) / sizeof(_keyval_pair_list_t)) + 1u;
    }
    else
    {
        // Best array size is HASHTABLE_MIN_ARRAY_COUNT
        config->array_count = HASHTABLE_MIN_ARRAY_COUNT;
    }

    return 0;
}


/**
 * @see hashtable_api.h
 */
char *hashtable_error_message(void)
{
    return _error_msg;
}
