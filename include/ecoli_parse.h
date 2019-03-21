/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2016, Olivier MATZ <zer0@droids-corp.org>
 */

/**
 * Node parse API.
 *
 * The parse operation is to check if an input (a string or vector of
 * strings) matches the node tree. On success, the result is stored in a
 * tree that describes which part of the input matches which node.
 */

#ifndef ECOLI_PARSE_
#define ECOLI_PARSE_

#include <sys/queue.h>
#include <sys/types.h>
#include <limits.h>
#include <stdio.h>
#include <stdbool.h>

struct ec_node;
struct ec_parse;

/**
 * Create an empty parse tree.
 *
 * @return
 *   The empty parse tree.
 */
struct ec_parse *ec_parse(const struct ec_node *node);

/**
 *
 *
 *
 */
void ec_parse_free(struct ec_parse *parse);

/**
 *
 *
 *
 */
void ec_parse_free_children(struct ec_parse *parse);

/**
 *
 *
 *
 */
struct ec_parse *ec_parse_dup(const struct ec_parse *parse);

/**
 *
 *
 *
 */
// _get_ XXX
const struct ec_strvec *ec_parse_strvec(const struct ec_parse *parse);

/* a NULL return value is an error, with errno set
  ENOTSUP: no ->parse() operation
*/
/**
 *
 *
 *
 */
struct ec_parse *ec_node_parse(const struct ec_node *node, const char *str);

/**
 *
 *
 *
 */
struct ec_parse *ec_node_parse_strvec(const struct ec_node *node,
				const struct ec_strvec *strvec);

/**
 *
 *
 *
 */
#define EC_PARSE_NOMATCH INT_MAX

/* internal: used by nodes
 *
 * state is the current parse tree, which is built piece by piece while
 *   parsing the node tree: ec_node_parse_child() creates a new child in
 *   this state parse tree, and calls the parse() method for the child
 *   node, with state pointing to this new child. If it does not match,
 *   the child is removed in the state, else it is kept, with its
 *   possible descendants.
 *
 * return:
 * the number of matched strings in strvec on success
 * EC_PARSE_NOMATCH (positive) if it does not match
 * -1 on error, and errno is set
 */
int ec_node_parse_child(const struct ec_node *node,
			struct ec_parse *state,
			const struct ec_strvec *strvec);

/**
 *
 *
 *
 */
void ec_parse_link_child(struct ec_parse *parse,
			struct ec_parse *child);
/**
 *
 *
 *
 */
void ec_parse_unlink_child(struct ec_parse *parse,
			struct ec_parse *child);

/* keep the const */
#define ec_parse_get_root(parse) ({				\
	const struct ec_parse *p_ = parse; /* check type */	\
	struct ec_parse *parse_ = (struct ec_parse *)parse;	\
	typeof(parse) res_;					\
	(void)p_;						\
	res_ = __ec_parse_get_root(parse_);			\
	res_;							\
})

/**
 *
 *
 *
 */
struct ec_parse *__ec_parse_get_root(struct ec_parse *parse);

/**
 *
 *
 *
 */
struct ec_parse *ec_parse_get_parent(const struct ec_parse *parse);

/**
 * Get the first child of a tree.
 *
 */
struct ec_parse *ec_parse_get_first_child(const struct ec_parse *parse);

/**
 *
 *
 *
 */
struct ec_parse *ec_parse_get_last_child(const struct ec_parse *parse);

/**
 *
 *
 *
 */
struct ec_parse *ec_parse_next(const struct ec_parse *parse);

/**
 *
 *
 *
 */
#define EC_PARSE_FOREACH_CHILD(child, parse)			\
	for (child = ec_parse_get_first_child(parse);		\
	     child != NULL;					\
	     child = ec_parse_next(child))			\

/**
 *
 *
 *
 */
bool ec_parse_has_child(const struct ec_parse *parse);

/**
 *
 *
 *
 */
const struct ec_node *ec_parse_get_node(const struct ec_parse *parse);

/**
 *
 *
 *
 */
void ec_parse_del_last_child(struct ec_parse *parse);

/**
 *
 *
 *
 */
struct ec_keyval *ec_parse_get_attrs(struct ec_parse *parse);

/**
 *
 *
 *
 */
void ec_parse_dump(FILE *out, const struct ec_parse *parse);

/**
 *
 *
 *
 */
struct ec_parse *ec_parse_find(struct ec_parse *parse,
	const char *id);

/**
 *
 *
 *
 */
struct ec_parse *ec_parse_find_next(struct ec_parse *root,
				struct ec_parse *start,
				const char *id, bool iter_children);

/**
 * Iterate among parse tree
 *
 * Use it with:
 * for (iter = state; iter != NULL; iter = EC_PARSE_ITER_NEXT(state, iter, 1))
 */
struct ec_parse *__ec_parse_iter_next(const struct ec_parse *root,
				struct ec_parse *parse, bool iter_children);

/* keep the const if any */
#define EC_PARSE_ITER_NEXT(root, parse, iter_children) ({		\
	const struct ec_parse *p_ = parse; /* check type */		\
	struct ec_parse *parse_ = (struct ec_parse *)parse;		\
	typeof(parse) res_;						\
	(void)p_;							\
	res_ = __ec_parse_iter_next(root, parse_, iter_children);	\
	res_;								\
})

/**
 *
 *
 *
 */
size_t ec_parse_len(const struct ec_parse *parse);

/**
 *
 *
 *
 */
size_t ec_parse_matches(const struct ec_parse *parse);

#endif
