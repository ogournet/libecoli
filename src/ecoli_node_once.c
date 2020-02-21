/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2016, Olivier MATZ <zer0@droids-corp.org>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>
#include <errno.h>

#include <ecoli_malloc.h>
#include <ecoli_log.h>
#include <ecoli_test.h>
#include <ecoli_strvec.h>
#include <ecoli_node.h>
#include <ecoli_parse.h>
#include <ecoli_complete.h>
#include <ecoli_node_str.h>
#include <ecoli_node_or.h>
#include <ecoli_node_many.h>
#include <ecoli_config.h>
#include <ecoli_node_once.h>

EC_LOG_TYPE_REGISTER(node_once);

struct ec_node_once {
	struct ec_node *child;
};

static unsigned int
count_node(struct ec_pnode *parse, const struct ec_node *node)
{
	struct ec_pnode *child;
	unsigned int count = 0;

	if (parse == NULL)
		return 0;

	if (ec_pnode_get_node(parse) == node)
		count++;

	EC_PNODE_FOREACH_CHILD(child, parse)
		count += count_node(child, node);

	return count;
}

static int
ec_node_once_parse(const struct ec_node *node,
		struct ec_pnode *state,
		const struct ec_strvec *strvec)
{
	struct ec_node_once *priv = ec_node_priv(node);
	unsigned int count;

	/* count the number of occurences of the node: if already parsed,
	 * do not match
	 */
	count = count_node(ec_pnode_get_root(state), priv->child);
	if (count > 0)
		return EC_PARSE_NOMATCH;

	return ec_parse_child(priv->child, state, strvec);
}

static int
ec_node_once_complete(const struct ec_node *node,
		struct ec_comp *comp,
		const struct ec_strvec *strvec)
{
	struct ec_node_once *priv = ec_node_priv(node);
	struct ec_pnode *parse = ec_comp_get_cur_pstate(comp);
	unsigned int count;
	int ret;

	/* count the number of occurences of the node: if already parsed,
	 * do not match
	 */
	count = count_node(ec_pnode_get_root(parse), priv->child);
	if (count > 0)
		return 0;

	ret = ec_complete_child(priv->child, comp, strvec);
	if (ret < 0)
		return ret;

	return 0;
}

static void ec_node_once_free_priv(struct ec_node *node)
{
	struct ec_node_once *priv = ec_node_priv(node);

	ec_node_free(priv->child);
}

static size_t
ec_node_once_get_children_count(const struct ec_node *node)
{
	struct ec_node_once *priv = ec_node_priv(node);

	if (priv->child)
		return 1;
	return 0;
}

static int
ec_node_once_get_child(const struct ec_node *node, size_t i,
		struct ec_node **child, unsigned int *refs)
{
	struct ec_node_once *priv = ec_node_priv(node);

	if (i >= 1)
		return -1;

	*child = priv->child;
	*refs = 2;
	return 0;
}

static const struct ec_config_schema ec_node_once_schema[] = {
	{
		.key = "child",
		.desc = "The child node.",
		.type = EC_CONFIG_TYPE_NODE,
	},
	{
		.type = EC_CONFIG_TYPE_NONE,
	},
};

static int ec_node_once_set_config(struct ec_node *node,
				const struct ec_config *config)
{
	struct ec_node_once *priv = ec_node_priv(node);
	const struct ec_config *child;

	child = ec_config_dict_get(config, "child");
	if (child == NULL)
		goto fail;
	if (ec_config_get_type(child) != EC_CONFIG_TYPE_NODE) {
		errno = EINVAL;
		goto fail;
	}

	if (priv->child != NULL)
		ec_node_free(priv->child);
	priv->child = ec_node_clone(child->node);

	return 0;

fail:
	return -1;
}

static struct ec_node_type ec_node_once_type = {
	.name = "once",
	.schema = ec_node_once_schema,
	.set_config = ec_node_once_set_config,
	.parse = ec_node_once_parse,
	.complete = ec_node_once_complete,
	.size = sizeof(struct ec_node_once),
	.free_priv = ec_node_once_free_priv,
	.get_children_count = ec_node_once_get_children_count,
	.get_child = ec_node_once_get_child,
};

EC_NODE_TYPE_REGISTER(ec_node_once_type);

int
ec_node_once_set_child(struct ec_node *node, struct ec_node *child)
{
	const struct ec_config *cur_config = NULL;
	struct ec_config *config = NULL;
	int ret;

	if (ec_node_check_type(node, &ec_node_once_type) < 0)
		goto fail;

	cur_config = ec_node_get_config(node);
	if (cur_config == NULL)
		config = ec_config_dict();
	else
		config = ec_config_dup(cur_config);
	if (config == NULL)
		goto fail;

	if (ec_config_dict_set(config, "child", ec_config_node(child)) < 0) {
		child = NULL; /* freed */
		goto fail;
	}
	child = NULL; /* freed */

	ret = ec_node_set_config(node, config);
	config = NULL; /* freed */
	if (ret < 0)
		goto fail;

	return 0;

fail:
	ec_config_free(config);
	ec_node_free(child);
	return -1;
}

struct ec_node *ec_node_once(const char *id, struct ec_node *child)
{
	struct ec_node *node = NULL;

	if (child == NULL)
		return NULL;

	node = ec_node_from_type(&ec_node_once_type, id);
	if (node == NULL)
		goto fail;

	ec_node_once_set_child(node, child);
	child = NULL;

	return node;

fail:
	ec_node_free(child);
	return NULL;
}

/* LCOV_EXCL_START */
static int ec_node_once_testcase(void)
{
	struct ec_node *node;
	int testres = 0;

	node = ec_node_many(EC_NO_ID,
			EC_NODE_OR(EC_NO_ID,
				ec_node_once(EC_NO_ID, ec_node_str(EC_NO_ID, "foo")),
				ec_node_str(EC_NO_ID, "bar")
				), 0, 0
		);
	if (node == NULL) {
		EC_LOG(EC_LOG_ERR, "cannot create node\n");
		return -1;
	}
	testres |= EC_TEST_CHECK_PARSE(node, 0);
	testres |= EC_TEST_CHECK_PARSE(node, 1, "foo");
	testres |= EC_TEST_CHECK_PARSE(node, 1, "bar");
	testres |= EC_TEST_CHECK_PARSE(node, 2, "foo", "bar");
	testres |= EC_TEST_CHECK_PARSE(node, 3, "foo", "bar", "bar");
	testres |= EC_TEST_CHECK_PARSE(node, 3, "bar", "foo", "bar");
	testres |= EC_TEST_CHECK_PARSE(node, 2, "bar", "foo", "foo");
	testres |= EC_TEST_CHECK_PARSE(node, 1, "foo", "foo");
	testres |= EC_TEST_CHECK_PARSE(node, 0, "foox");

	testres |= EC_TEST_CHECK_COMPLETE(node,
		"", EC_VA_END,
		"foo", "bar", EC_VA_END);
	testres |= EC_TEST_CHECK_COMPLETE(node,
		"f", EC_VA_END,
		"foo", EC_VA_END);
	testres |= EC_TEST_CHECK_COMPLETE(node,
		"b", EC_VA_END,
		"bar", EC_VA_END);
	testres |= EC_TEST_CHECK_COMPLETE(node,
		"foo", "", EC_VA_END,
		"bar", EC_VA_END);
	testres |= EC_TEST_CHECK_COMPLETE(node,
		"bar", "", EC_VA_END,
		"foo", "bar", EC_VA_END);
	ec_node_free(node);

	return testres;
}
/* LCOV_EXCL_STOP */

static struct ec_test ec_node_once_test = {
	.name = "node_once",
	.test = ec_node_once_testcase,
};

EC_TEST_REGISTER(ec_node_once_test);
