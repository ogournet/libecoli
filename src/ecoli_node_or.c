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
#include <ecoli_strvec.h>
#include <ecoli_node.h>
#include <ecoli_parse.h>
#include <ecoli_complete.h>
#include <ecoli_node.h>
#include <ecoli_config.h>
#include <ecoli_node_helper.h>
#include <ecoli_node_or.h>
#include <ecoli_node_str.h>
#include <ecoli_test.h>

EC_LOG_TYPE_REGISTER(node_or);

struct ec_node_or {
	struct ec_node **table;
	size_t len;
};

static int
ec_node_or_parse(const struct ec_node *node,
		struct ec_parse *state,
		const struct ec_strvec *strvec)
{
	struct ec_node_or *priv = ec_node_priv(node);
	unsigned int i;
	int ret;

	for (i = 0; i < priv->len; i++) {
		ret = ec_node_parse_child(priv->table[i], state, strvec);
		if (ret == EC_PARSE_NOMATCH)
			continue;
		return ret;
	}

	return EC_PARSE_NOMATCH;
}

static int
ec_node_or_complete(const struct ec_node *node,
		struct ec_comp *comp,
		const struct ec_strvec *strvec)
{
	struct ec_node_or *priv = ec_node_priv(node);
	int ret;
	size_t n;

	for (n = 0; n < priv->len; n++) {
		ret = ec_node_complete_child(priv->table[n],
					comp, strvec);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static void ec_node_or_free_priv(struct ec_node *node)
{
	struct ec_node_or *priv = ec_node_priv(node);
	size_t i;

	for (i = 0; i < priv->len; i++)
		ec_node_free(priv->table[i]);
	ec_free(priv->table);
	priv->table = NULL;
	priv->len = 0;
}

static const struct ec_config_schema ec_node_or_subschema[] = {
	{
		.desc = "A child node which is part of the choice.",
		.type = EC_CONFIG_TYPE_NODE,
	},
	{
		.type = EC_CONFIG_TYPE_NONE,
	},
};

static const struct ec_config_schema ec_node_or_schema[] = {
	{
		.key = "children",
		.desc = "The list of children nodes defining the choice "
		"elements.",
		.type = EC_CONFIG_TYPE_LIST,
		.subschema = ec_node_or_subschema,
	},
	{
		.type = EC_CONFIG_TYPE_NONE,
	},
};

static int ec_node_or_set_config(struct ec_node *node,
				const struct ec_config *config)
{
	struct ec_node_or *priv = ec_node_priv(node);
	struct ec_node **table = NULL;
	size_t i, len = 0;

	table = ec_node_config_node_list_to_table(
		ec_config_dict_get(config, "children"), &len);
	if (table == NULL)
		goto fail;

	for (i = 0; i < priv->len; i++)
		ec_node_free(priv->table[i]);
	ec_free(priv->table);
	priv->table = table;
	priv->len = len;

	return 0;

fail:
	for (i = 0; i < len; i++)
		ec_node_free(table[i]);
	ec_free(table);
	return -1;
}

static size_t
ec_node_or_get_children_count(const struct ec_node *node)
{
	struct ec_node_or *priv = ec_node_priv(node);
	return priv->len;
}

static int
ec_node_or_get_child(const struct ec_node *node, size_t i,
		struct ec_node **child, unsigned int *refs)
{
	struct ec_node_or *priv = ec_node_priv(node);

	if (i >= priv->len)
		return -1;

	*child = priv->table[i];
	/* each child node is referenced twice: once in the config and
	 * once in the priv->table[] */
	*refs = 2;
	return 0;
}

static struct ec_node_type ec_node_or_type = {
	.name = "or",
	.schema = ec_node_or_schema,
	.set_config = ec_node_or_set_config,
	.parse = ec_node_or_parse,
	.complete = ec_node_or_complete,
	.size = sizeof(struct ec_node_or),
	.free_priv = ec_node_or_free_priv,
	.get_children_count = ec_node_or_get_children_count,
	.get_child = ec_node_or_get_child,
};

EC_NODE_TYPE_REGISTER(ec_node_or_type);

int ec_node_or_add(struct ec_node *node, struct ec_node *child)
{
	const struct ec_config *cur_config = NULL;
	struct ec_config *config = NULL, *children;
	int ret;

	assert(node != NULL);

	/* XXX factorize this code in a helper */

	if (ec_node_check_type(node, &ec_node_or_type) < 0)
		goto fail;

	cur_config = ec_node_get_config(node);
	if (cur_config == NULL)
		config = ec_config_dict();
	else
		config = ec_config_dup(cur_config);
	if (config == NULL)
		goto fail;

	children = ec_config_dict_get(config, "children");
	if (children == NULL) {
		children = ec_config_list();
		if (children == NULL)
			goto fail;

		if (ec_config_dict_set(config, "children", children) < 0)
			goto fail; /* children list is freed on error */
	}

	if (ec_config_list_add(children, ec_config_node(child)) < 0) {
		child = NULL;
		goto fail;
	}

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

struct ec_node *__ec_node_or(const char *id, ...)
{
	struct ec_config *config = NULL, *children = NULL;
	struct ec_node *node = NULL;
	struct ec_node *child;
	va_list ap;
	int ret;

	va_start(ap, id);
	child = va_arg(ap, struct ec_node *);

	node = ec_node_from_type(&ec_node_or_type, id);
	if (node == NULL)
		goto fail_free_children;

	config = ec_config_dict();
	if (config == NULL)
		goto fail_free_children;

	children = ec_config_list();
	if (children == NULL)
		goto fail_free_children;

	for (; child != EC_NODE_ENDLIST; child = va_arg(ap, struct ec_node *)) {
		if (child == NULL)
			goto fail_free_children;

		if (ec_config_list_add(children, ec_config_node(child)) < 0) {
			child = NULL;
			goto fail_free_children;
		}
	}

	if (ec_config_dict_set(config, "children", children) < 0) {
		children = NULL; /* freed */
		goto fail;
	}
	children = NULL;

	ret = ec_node_set_config(node, config);
	config = NULL; /* freed */
	if (ret < 0)
		goto fail;

	va_end(ap);

	return node;

fail_free_children:
	for (; child != EC_NODE_ENDLIST; child = va_arg(ap, struct ec_node *))
		ec_node_free(child);
fail:
	ec_node_free(node); /* will also free added children */
	ec_config_free(children);
	ec_config_free(config);
	va_end(ap);

	return NULL;
}

/* LCOV_EXCL_START */
static int ec_node_or_testcase(void)
{
	struct ec_node *node;
	int testres = 0;

	node = EC_NODE_OR(EC_NO_ID,
		ec_node_str(EC_NO_ID, "foo"),
		ec_node_str(EC_NO_ID, "bar")
	);
	if (node == NULL) {
		EC_LOG(EC_LOG_ERR, "cannot create node\n");
		return -1;
	}
	testres |= EC_TEST_CHECK_PARSE(node, 1, "foo");
	testres |= EC_TEST_CHECK_PARSE(node, 1, "bar");
	testres |= EC_TEST_CHECK_PARSE(node, 1, "foo", "bar");
	testres |= EC_TEST_CHECK_PARSE(node, -1, " ");
	testres |= EC_TEST_CHECK_PARSE(node, -1, "foox");
	testres |= EC_TEST_CHECK_PARSE(node, -1, "toto");
	testres |= EC_TEST_CHECK_PARSE(node, -1, "");
	ec_node_free(node);

	/* test completion */
	node = EC_NODE_OR(EC_NO_ID,
		ec_node_str(EC_NO_ID, "foo"),
		ec_node_str(EC_NO_ID, "bar"),
		ec_node_str(EC_NO_ID, "bar2"),
		ec_node_str(EC_NO_ID, "toto"),
		ec_node_str(EC_NO_ID, "titi")
	);
	if (node == NULL) {
		EC_LOG(EC_LOG_ERR, "cannot create node\n");
		return -1;
	}
	testres |= EC_TEST_CHECK_COMPLETE(node,
		"", EC_NODE_ENDLIST,
		"foo", "bar", "bar2", "toto", "titi", EC_NODE_ENDLIST);
	testres |= EC_TEST_CHECK_COMPLETE(node,
		"f", EC_NODE_ENDLIST,
		"foo", EC_NODE_ENDLIST);
	testres |= EC_TEST_CHECK_COMPLETE(node,
		"b", EC_NODE_ENDLIST,
		"bar", "bar2", EC_NODE_ENDLIST);
	testres |= EC_TEST_CHECK_COMPLETE(node,
		"bar", EC_NODE_ENDLIST,
		"bar", "bar2", EC_NODE_ENDLIST);
	testres |= EC_TEST_CHECK_COMPLETE(node,
		"t", EC_NODE_ENDLIST,
		"toto", "titi", EC_NODE_ENDLIST);
	testres |= EC_TEST_CHECK_COMPLETE(node,
		"to", EC_NODE_ENDLIST,
		"toto", EC_NODE_ENDLIST);
	testres |= EC_TEST_CHECK_COMPLETE(node,
		"x", EC_NODE_ENDLIST,
		EC_NODE_ENDLIST);
	ec_node_free(node);

	return testres;
}
/* LCOV_EXCL_STOP */

static struct ec_test ec_node_or_test = {
	.name = "node_or",
	.test = ec_node_or_testcase,
};

EC_TEST_REGISTER(ec_node_or_test);
