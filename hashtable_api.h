/*! \mainpage A lightweight separate-chaining hashtable in C
 *
 * \section intro_sec Introduction
 *
 * See \link hashtable_api.h API documentation for hashtable_api.h \endlink .
 *
 * This module implements a lightweight hashtable that uses separate chaining to resolve collisions.
 *
 * This hashtable is designed to be flexible enough for use on embedded systems that have
 * no dynamic memory, and/or limited memory in general.
 *
 * \section codesample_sec Example program
 *
 * The following sample program creates a hashtable instance, inserts a single key/value pair,
 * and the retrieves the stored value with the same key, and prints it to stdout.
 *
 * See \link hashtable_api.h API documentation for hashtable_api.h \endlink for comprehensive details about
 * all available functions.
 *
 * \include example_main.c
 *
 * \section features_sec Features/limitations
 *
 * - Implemented in pure C99, and requires only `stdint.h` and `string.h`.
 * - Uses <a href="https://en.wikipedia.org/wiki/Hash_table#Separate_chaining">separate chaining</a> to resolve collisions.
 * - Keys and values are byte streams of arbitrary length/contents, so keys and values can
 *   be any data type.
 * - No dynamic memory allocation, and no table re-sizing. All table data is stored
 *   in a buffer that must be provided by the caller on hashtable creation,
 *   and when there is not enough space remaining in that buffer, insertion of new
 *   items will fail.
 * - Provide/write your own hash function (FNV-1a is used by default if you don't provide one).
 *
 * \section buildopts_sec Build/compile options
 *
 * There are a number of preprocessor symbols you can define to change certain things,
 * here are the details about those symbols.
 *
 * \subsection ht_size_sec Datatype used for key/value sizes
 *
 *  By default, `size_t` is used to hold the sizes of keys/values. If you
 *  know that all your keys/values are below a certain size, however, and you want to save
 *  some memory, then you can define one of the following options to set the datatype used
 *  to hold key/value sizes:
 *
 *  Symbol name                          | Effect
 *  -------------------------------------|-------------------------------------------
 *  `HASHTABLE_SIZE_T_UINT16` | hashtable_size_t is `uint16_t`
 *  `HASHTABLE_SIZE_T_UINT32` | hashtable_size_t is `uint32_t`
 *  `HASHTABLE_SIZE_T_SYS`    | hashtable_size_t is `size_t` <b>(default)</b>
 *
 * \subsection disable_paramval_sec Disable function parameter validation
 *
 *  By default, all function parameters are checked for validity (e.g. no NULL pointers,
 *  no size values of 0). However, if you have a stable system and have worked most of
 *  those types of bugs out, then you may want to compile these checks out for a performance gain.
 *  Define the following option to compile out all function parameter validation checks:
 *
 *  Symbol name                          | Effect
 *  -------------------------------------|---------------------------------------------------
 *  `HASHTABLE_DISABLE_PARAM_VALIDATION` | All function param. validation checks compiled out
 *
 * \subsection packed_struct_sec Enable packed struct option
 *
 *  Use `__attribute__((packed))` for key/value pair struct definition, may save
 *  some extra space for stored key/value pair data:
 *
 *  Symbol name               | Effect
 *  --------------------------|-----------------------------------------------------
 *  `HASHTABLE_PACKED_STRUCT` | Key/value pair struct uses `__attribute__((packed))`
 *
 */


/**
 * @file hashtable_api.h
 *
 * @author Erik K. Nyquist
 *
 * @brief Implements a lightweight separate-chaining hashtable designed to be flexible
 *        enough for embedded systems.
 */

#ifndef HASHTABLE_API_H
#define HASHTABLE_API_H

#include <stdint.h>


#define HASHTABLE_LIB_VERSION "v0.1.0"


#if !defined(HASHTABLE_SIZE_T_UINT16) && !defined(HASHTABLE_SIZE_T_UINT32) && !defined(HASHTABLE_SIZE_T_SYS)
#define HASHTABLE_SIZE_T_SYS
#endif // if !defined..


#if defined(HASHTABLE_SIZE_T_UINT16)
typedef uint16_t hashtable_size_t;
#elif defined(HASHTABLE_SIZE_T_UINT32)
typedef uint32_t hashtable_size_t;
#elif defined(HASHTABLE_SIZE_T_SYS)
typedef size_t hashtable_size_t;
#else
#error("HASHTABLE_SIZE_T option not defined")
#endif // if defined....


/**
 * @brief Minimum number of slots in the table array
 */
#define HASHTABLE_MIN_ARRAY_COUNT (10u)


/**
 * @brief Helper macro, gets the min. required buffer size for a specific array count.
 *        When creating a hashtable with a specific array count, this macro will tell
 *        you how much memory is required at a minimum to hold the 'housekeeping' data
 *        for that table. Any remaining space is used for key/value pair data storage.
 */
#define HASHTABLE_MIN_BUFFER_SIZE(array_count)                                 \
    (sizeof(_keyval_pair_table_data_t) + sizeof(_keyval_pair_list_table_t) +   \
    ((array_count) * sizeof(_keyval_pair_list_t)) +                            \
    sizeof(_keyval_pair_data_block_t))


/**
 * Hash function used for hashing key data
 *
 * @param data   Pointer to key data
 * @param size   Key data size in bytes
 *
 * @return  Computed hash value
 */
typedef uint32_t (*hashtable_hashfunc_t)(const char *data, const hashtable_size_t size);


/**
 * @brief Configuration data for a single hashtable instance
 */
typedef struct
{
    hashtable_hashfunc_t hash;    ///< Hash function to use, must not be NULL
    uint32_t array_count;         ///< Number of table array slots, must not be 0
} hashtable_config_t;


/**
 * @brief All data for a single hashtable instance
 */
typedef struct
{
    hashtable_config_t config;    ///< Hashtable config data
    uint32_t entry_count;         ///< Number of entries in the table
    uint32_t array_slots_used;    ///< Number of array slots with one or more items in the list
    size_t data_size;             ///< Size of data section
    void *table_data;             ///< Pointer to buffer for data section
} hashtable_t;


/**
 * Initialize a new hashtable instance
 *
 * @param table        Pointer to hashtable instance
 * @param config       Pointer to hashtable configuration data. May be NULL.
 *                     If NULL, a default general-purpose configuration will be used.
 * @param buffer       Pointer to buffer to use for hashtable data
 * @param buffer_size  Size of buffer in bytes
 *
 * @return   0 if successful, 1 if buffer size is not large enough, and -1 if an
 *           error occurred. Use #hashtable_error_message to get an error message
 *           if -1 is returned.
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
 * @param value       Pointer to value data, may be NULL
 * @param value_size  Value data size in bytes, may be 0
 *
 * @return   0 if successful, 1 if there is not enough space left in the buffer for
             key/value pair data, and -1 if an error occurred. Use #hashtable_error_message
 *           to get an error message if -1 is returned.
 */
int hashtable_insert(hashtable_t *table, const char *key, const hashtable_size_t key_size,
                     const char *value, const hashtable_size_t value_size);


/**
 * Remove a stored value from a table by key. If the given key does not exist in
 * the table, then the return value will indicate success.
 *
 * @param table     Pointer to hashtable instance
 * @param key       Pointer to key data
 * @param key_size  Key data size in bytes
 *
 * @return   0 if successful, 1 if the key does not exist, and -1 if an error occurred.
 *           Use #hashtable_error_message to get an error message if -1 is returned.
 */
int hashtable_remove(hashtable_t *table, const char *key, const hashtable_size_t key_size);


/**
 * Retrieve a pointer to a value stored in a table by key.
 *
 * @param table       Pointer to hashtable instance
 * @param key         Pointer to key data
 * @param key_size    Key data size in bytes
 * @param value       Pointer to location to store value pointer, may be NULL
 * @param value_size  Pointer to location to store value size, may be NULL
 *
 * @return   0 if successful, 1 if the key does not exist, and -1 if an error occurred.
 *           Use #hashtable_error_message to get an error message if -1 is returned.
 */
int hashtable_retrieve(hashtable_t *table, const char *key, const hashtable_size_t key_size,
                       char **value, hashtable_size_t *value_size);


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
int hashtable_has_key(hashtable_t *table, const char *key, const hashtable_size_t key_size);


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
 * @param key         Pointer to location to store key pointer
 * @param key_size    Pointer to location to store key data size in bytes
 * @param value       Pointer to location to store value pointer, may be NULL
 * @param value_size  Pointer to location to store value data size in bytes, may be NULL
 *
 * @return   0 if next item was read successfully, 1 if no item was read because
 *           all items in the table have been iterated over (in this case you can
             use hashtable_reset_cursor to reset for another iteration if you need to),
             and -1 if an error occurred. Use #hashtable_error_message to get an error
             message.
 */
int hashtable_next_item(hashtable_t *table, char **key, hashtable_size_t *key_size,
                        char **value, hashtable_size_t *value_size);


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
 * Populate a configuration structure with the default hash function (FNV-1a), and
 * an array count optimized for the given buffer size.
 *
 * @param config       Pointer to configuration data structure to populate
 * @param buffer_size  Buffer size to generate configuration for
 *
 * @return   0 if successful, -1 if an error occurred. Use #hashtable_error_message
 *           to get an error message.
 */
int hashtable_default_config(hashtable_config_t *config, size_t buffer_size);


/**
 * Return a pointer to the last stored error message. When any hashtable function
 * returns -1 to indicate an error, you can call this function to get a pointer to
 * the corresponding error message string. If no error has occurred then this
 * function will return a pointer to an empty string.
 *
 * @return  Pointer to error message string
 */
char *hashtable_error_message(void);


/**
 * Private definitions-- not strictly needed in the public API, but required for
 * the #HASHTABLE_MIN_BUFFER_SIZE macro definition.
 */
#ifdef HASHTABLE_PACKED_STRUCT
#define _HASHTABLE_PACKED __attribute__((packed))
#else
#define _HASHTABLE_PACKED
#endif // HASHTABLE_PACKED_STRUCT


/**
 * Represents a single key/value pair stored in the data block area of a table instance.
 * Also represents a single node in a singly-linked list of key/value pairs.
 */
typedef struct _keyval_pair
{
    struct _keyval_pair *next;    ///< Pointer to next key/val pair in the list
    hashtable_size_t key_size;    ///< Size of key data in bytes
    hashtable_size_t value_size;  ///< Size of value data in bytes
    uint8_t data[];               ///< Start of key + value data packed together
} _HASHTABLE_PACKED _keyval_pair_t;


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

#endif // HASHTABLE_API_H
