/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018, Olivier MATZ <zer0@droids-corp.org>
 */

#ifndef ECOLI_CONFIG_
#define ECOLI_CONFIG_

#include <sys/queue.h>
#include <stdbool.h>
#include <stdint.h>

#ifndef EC_COUNT_OF //XXX
#define EC_COUNT_OF(x) ((sizeof(x)/sizeof(0[x])) / \
		((size_t)(!(sizeof(x) % sizeof(0[x])))))
#endif

struct ec_config;
struct ec_keyval;

/**
 * The type identifier for a config value.
 */
enum ec_config_type {
	EC_CONFIG_TYPE_NONE = 0,
	EC_CONFIG_TYPE_BOOL,
	EC_CONFIG_TYPE_INT64,
	EC_CONFIG_TYPE_UINT64,
	EC_CONFIG_TYPE_STRING,
	EC_CONFIG_TYPE_NODE,
	EC_CONFIG_TYPE_LIST,
	EC_CONFIG_TYPE_DICT,
};

/**
 * Structure describing the format of a configuration value.
 *
 * This structure is used in a const array which is referenced by a
 * struct ec_config. Each entry of the array represents a key/value
 * storage of the configuration dictionary.
 */
struct ec_config_schema {
	const char *key;          /**< The key string (NULL for list elts). */
	const char *desc;         /**< A description of the value. */
	enum ec_config_type type; /**< Type of the value */

	/** If type is dict or list, the schema of the dict or list
	 * elements. Else must be NULL. */
	const struct ec_config_schema *subschema;

	/** The subschema array len in case of dict (> 0) or list (set
	 * to 1). Else must be 0. */
	size_t subschema_len;

};

TAILQ_HEAD(ec_config_list, ec_config);

/**
 * Structure storing the configuration data.
 */
struct ec_config {
	/** type of value stored in the union */
	enum ec_config_type type;

	union {
		bool boolean;   /** Boolean value */
		int64_t i64;    /** Signed integer value */
		uint64_t u64;   /** Unsigned integer value */
		char *string;   /** String value */
		struct ec_node *node;       /** Node value */
		struct ec_keyval *dict;     /** Hash table value */
		struct ec_config_list list; /** List value */
	};

	/**
	 * Next in list, only valid if type is list.
	 */
	TAILQ_ENTRY(ec_config) next;
};

/* schema */

/**
 * Validate a configuration schema array.
 *
 * @param schema
 *   Pointer to the first element of the schema array.
 * @param schema_len
 *   Length of the schema array.
 * @return
 *   0 if the schema is valid, or -1 on error (errno is set).
 */
int ec_config_schema_validate(const struct ec_config_schema *schema,
			size_t schema_len);

/**
 * Dump a configuration schema array.
 *
 * @param out
 *   Output stream on which the dump will be sent.
 * @param schema
 *   Pointer to the first element of the schema array.
 * @param schema_len
 *   Length of the schema array.
 */
void ec_config_schema_dump(FILE *out, const struct ec_config_schema *schema,
			size_t schema_len);


/* config */

/**
 * Create a boolean configuration value.
 *
 * @param boolean
 *   The boolean value to be set.
 * @return
 *   The configuration object, or NULL on error (errno is set).
 */
struct ec_config *ec_config_bool(bool boolean);

/**
 * Create a signed integer configuration value.
 *
 * @param i64
 *   The signed integer value to be set.
 * @return
 *   The configuration object, or NULL on error (errno is set).
 */
struct ec_config *ec_config_i64(int64_t i64);

/**
 * Create an unsigned configuration value.
 *
 * @param u64
 *   The unsigned integer value to be set.
 * @return
 *   The configuration object, or NULL on error (errno is set).
 */
struct ec_config *ec_config_u64(uint64_t u64);

/**
 * Create a string configuration value.
 *
 * @param string
 *   The string value to be set. The string is copied into the
 *   configuration object.
 * @return
 *   The configuration object, or NULL on error (errno is set).
 */
struct ec_config *ec_config_string(const char *string);

/**
 * Create a node configuration value.
 *
 * @param node
 *   The node pointer to be set. The node is "consumed" by
 *   the function and should not be used by the caller, even
 *   on error. The caller can use ec_node_clone() to keep a
 *   reference on the node.
 * @return
 *   The configuration object, or NULL on error (errno is set).
 */
struct ec_config *ec_config_node(struct ec_node *node);

/**
 * Create a hash table configuration value.
 *
 * @return
 *   A configuration object containing an empty hash table, or NULL on
 *   error (errno is set).
 */
struct ec_config *ec_config_dict(void);

/**
 * Create a list configuration value.
 *
 * @return
 *   The configuration object containing an empty list, or NULL on
 *   error (errno is set).
 */
struct ec_config *ec_config_list(void);

/**
 * Add a config object into a list.
 *
 * @param list
 *   The list configuration in which the value will be added.
 * @param value
 *   The value configuration to add in the list. The value object
 *   will be freed when freeing the list object. On error, the
 *   value object is also freed.
 * @return
 *   0 on success, else -1 (errno is set).
 */
int ec_config_list_add(struct ec_config *list, struct ec_config *value);

/**
 * Remove an element from a list.
 *
 * The element is freed and should not be accessed.
 *
 * @param list
 *   The list configuration.
 * @param config
 *   The element to remove from the list.
 * @return
 *   0 on success, -1 on error (errno is set).
 */
int ec_config_list_del(struct ec_config *list, struct ec_config *config);

/**
 * Validate a configuration.
 *
 * @param dict
 *   A hash table configuration to validate.
 * @param schema
 *   Pointer to the first element of the schema array.
 * @param schema_len
 *   Length of the schema array.
 * @return
 *   0 on success, -1 on error (errno is set).
 */
int ec_config_validate(const struct ec_config *dict,
		const struct ec_config_schema *schema,
		size_t schema_len);

/**
 * Set a value in a hash table configuration
 *
 * @param dict
 *   A hash table configuration to validate.
 * @param key
 *   The key to update.
 * @param value
 *   The value to set. The value object will be freed when freeing the
 *   dict object. On error, the value object is also freed.
 * @return
 *   0 on success, -1 on error (errno is set).
 */
int ec_config_dict_set(struct ec_config *dict, const char *key,
		struct ec_config *value);

/**
 * Remove an element from a hash table configuration.
 *
 * The element is freed and should not be accessed.
 *
 * @param dict
 *   A hash table configuration to validate.
 * @param key
 *   The key of the configuration to delete.
 * @return
 *   0 on success, -1 on error (errno is set).
 */
int ec_config_dict_del(struct ec_config *config, const char *key);

/**
 * Compare two configurations.
 */
int ec_config_cmp(const struct ec_config *config1,
		const struct ec_config *config2);

/**
 * Get configuration value.
 */
struct ec_config *ec_config_dict_get(const struct ec_config *config,
				const char *key);

/**
 * Get the first element of a list.
 *
 * Example of use:
 * for (config = ec_config_list_iter(list);
 *	config != NULL;
 *	config = ec_config_list_next(list, config)) {
 *		...
 * }
 *
 * @param list
 *   The list configuration to iterate.
 * @return
 *   The first configuration element, or NULL on error (errno is set).
 */
struct ec_config *ec_config_list_first(struct ec_config *list);

/**
 * Get next element in list.
 *
 * @param list
 *   The list configuration beeing iterated.
 * @param config
 *   The current configuration element.
 * @return
 *   The next configuration element, or NULL if there is no more element.
 */
struct ec_config *
ec_config_list_next(struct ec_config *list, struct ec_config *config);

/**
 * Free a configuration.
 *
 * @param config
 *   The element to free.
 */
void ec_config_free(struct ec_config *config);

/**
 * Compare two configurations.
 *
 * @return
 *   0 if the configurations are equal, else -1.
 */
int ec_config_cmp(const struct ec_config *value1,
		const struct ec_config *value2);

/**
 * Duplicate a configuration.
 *
 * @param config
 *   The configuration to duplicate.
 * @return
 *   The duplicated configuration, or NULL on error (errno is set).
 */
struct ec_config *
ec_config_dup(const struct ec_config *config);

/**
 * Dump a configuration.
 *
 * @param out
 *   Output stream on which the dump will be sent.
 * @param config
 *   The configuration to dump.
 */
void ec_config_dump(FILE *out, const struct ec_config *config);

#endif
