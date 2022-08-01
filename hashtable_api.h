/*! \mainpage Separate-chaining hashtable implementation in C
 *
 * \section intro_sec Introduction
 *
 * See
 * \link hashtable_api.h
 * API documentation for hashtable_api.h.
 * \endlink
 *
 * This module implements a lightweight hashtable that uses separate chaining to resolve collisions.
 *
 * This hashtable is designed to be flexible enough for use on embedded systems that have
 * no dynamic memory, and/or limited memory in general.
 *
 * \section features_sec Features/limitations
 *
 * - Implemented in pure C99, and requires only <code>stdint.h</code> and <code>string.h</code>.
 * - Uses <a href="https://en.wikipedia.org/wiki/Hash_table#Separate_chaining">separate chaining</a> to resolve collisions.
 * - Keys and values are byte streams of arbitrary length/contents, so keys and values can
 *   be any data type.
 * - No memory allocation of any kind, and no table re-sizing. All table data
 *   is stored in a buffer that must be provided by the caller on hashtable creation,
 *   and when there is not enough space remaining in that buffer, insertion of new
 *   items will fail.
 * - Provide/write your own hash function (although a default hash function, optimized
 *   for ASCII strings, is provided).
 */

/**
 * @file hashtable_api.h
 *
 * @author Erik K. Nyquist
 *
 * @brief Implements a lightweight separate-chaining hashtable designed to be flexible and
 *        simple enough for embedded systems.
 */

#ifndef HASHTABLE_API_H
#define HASHTABLE_API_H

#include <stdint.h>


/**
 * @brief Configuration data for a single hashtable instance
 */
typedef struct
{
    /**
     * Pointer to hash function to use for hashing key data
     *
     * @param data   Pointer to key data
     * @param size   Key data size in bytes
     *
     * @return  Computed hash value
     */
    uint32_t (*hash)(const void *data, const size_t size);

    uint32_t initial_array_count; ///< Number of table array slots
} hashtable_config_t;


/**
 * @brief All data for a single hashtable instance
 */
typedef struct
{
    hashtable_config_t config;    ///< Hashtable config data
    uint32_t entry_count;         ///< Number of entries in the table
    size_t data_size;             ///< Size of data section
    void *table_data;             ///< Pointer to buffer for data section
} hashtable_t;


/**
 * Initialize a new hashtable instance
 *
 * @param table        Pointer to hashtable instance
 * @param config       Pointer to hashtable configuration data
 * @param buffer       Pointer to buffer to use for hashtable data
 * @param buffer_size  Size of buffer in bytes
 *
 * @return   0 if successful, -1 if an error occurred. Use #hashtable_error_message
 *           to get an error message.
 */
int hashtable_create(hashtable_t *table, const hashtable_config_t *config,
                     void *buffer, size_t buffer_size);


/**
 * Insert a new key/value pair into a table. If a key/value pair with the
 * given key already exists, then it will be over-written with the new value.
 *
 * @param table       Pointer to hashtable instance
 * @param key         Pointer to key data
 * @param key_size    Key data size in bytes
 * @param value       Pointer to value data
 * @param value_size  Value data size in bytes
 *
 * @return   0 if successful, -1 if an error occurred. Use #hashtable_error_message
 *           to get an error message.
 */
int hashtable_insert(hashtable_t *table, const void *key, const size_t key_size,
                     const void *value, const size_t value_size);


/**
 * Remove a stored value from a table by key. If the given key does not exist in
 * the table, then the return value will indicate success.
 *
 * @param table     Pointer to hashtable instance
 * @param key       Pointer to key data
 * @param key_size  Key data size in bytes
 *
 * @return   0 if successful, -1 if an error occurred. Use #hashtable_error_message
 *           to get an error message.
 */
int hashtable_remove(hashtable_t *table, const void *key, const size_t key_size);


/**
 * Read a stored value from a table by key.
 *
 * @param table       Pointer to hashtable instance
 * @param key         Pointer to key data
 * @param key_size    Key data size in bytes
 * @param value       Pointer to location to store read value data
 * @param value_size  Pointer to location to store number of value bytes read
 *
 * @return   0 if successful, -1 if an error occurred. Use #hashtable_error_message
 *           to get an error message.
 */
int hashtable_retrieve(hashtable_t *table, const void *key, const size_t key_size,
                       void *value, size_t *value_size);


/**
 * Check if a key exists in a table.
 *
 * @param table     Pointer to hashtable instance
 * @param key       Pointer to key data
 * @param key_size  Key data size in bytes
 *
 * @return   1 if key exists, 0 if key does not exist, and -1 if an error occurred.
 *           Use #hashtable_error_message to get an error message.
 */
int hashtable_has_key(hashtable_t *table, const void *key, const size_t key_size);


/**
 * Number of bytes remaining for key/value pair data storage
 *
 * @param table            Pointer to hashtable instance
 * @param bytes_remaining  Pointer to location to store number of bytes remaining
 *
 * @return   0 if successful, -1 if an error occurred. Use #hashtable_error_message
 *           to get an error message.
 */
int hashtable_bytes_remaining(hashtable_t *table, size_t *bytes_remaining);


/**
 * Retrieve pointers to the next key/value pair in the table. This function can
 * be used to iterate over all key/value pairs stored in the table.
 *
 * @param table       Pointer to hashtable instance
 * @param key         Pointer to location to store key data
 * @param key_size    Pointer to location to store key data size in bytes
 * @param value       Pointer to location to store value data
 * @param value_size  Pointer to location to store value data size in bytes
 *
 * @return   0 if next item was read successfully, 1 if no item was read because
 *           all items in the table have been iterated over (in this case you can
             use hashtable_reset_cursor to reset for another iteration if you need to),
             and -1 if an error occurred. Use #hashtable_error_message to get an error
             message.
 */
int hashtable_next_item(hashtable_t *table, void *key, size_t *key_size,
                        void *value, size_t *value_size);


/**
 * Reset the key/value pair cursor, which is used for iteration via #hashtable_next_item.
 * This allows iterating through a table again after #hashtable_next_item has already iterated
 * over the whole table and returned a value of 1.
 *
 * @param table   Pointer to hashtable instance
 *
 * @return   0 if successful, -1 if an error occurred. Use #hashtable_error_message
 *           to get an error message.
 */
int hashtable_reset_cursor(hashtable_t *table);


/**
 * Get a pointer to a default configuration structure, using a basic default hash function
 * optimized for ASCII strings, and an initial array count of 64.
 *
 * @return  Pointer to default configuration data
 */
hashtable_config_t *hashtable_default_config(void);


/**
 * Return a pointer to the last stored error message. When any hashtable function
 * returns -1 to indicate an error, you can call this function to get a pointer to
 * the corresponding error message string. If no error has occurred then this
 * function will return a pointer to an empty string.
 *
 * @return  Pointer to error message string
 */
char *hashtable_error_message(void);

#endif // HASHTABLE_API_H
