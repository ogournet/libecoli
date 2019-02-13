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
#include <ecoli_node_option.h>
#include <ecoli_config.h>
#include <ecoli_node_many.h>

EC_LOG_TYPE_REGISTER(node_many);

struct ec_node_many {
	struct ec_node gen;
	unsigned int min;
	unsigned int max;
	struct ec_node *child;
};

static int ec_node_many_parse(const struct ec_node *gen_node,
			struct ec_parse *state,
			const struct ec_strvec *strvec)
{
	struct ec_node_many *node = (struct ec_node_many *)gen_node;
	struct ec_parse *child_parse;
	struct ec_strvec *childvec = NULL;
	size_t off = 0, count;
	int ret;

	for (count = 0; node->max == 0 || count < node->max; count++) {
		childvec = ec_strvec_ndup(strvec, off,
			ec_strvec_len(strvec) - off);
		if (childvec == NULL)
			goto fail;

		ret = ec_node_parse_child(node->child, state, childvec);
		if (ret < 0)
			goto fail;

		ec_strvec_free(childvec);
		childvec = NULL;

		if (ret == EC_PARSE_NOMATCH)
			break;

		/* it matches an empty strvec, no need to continue */
		if (ret == 0) {
			child_parse = ec_parse_get_last_child(state);
			ec_parse_unlink_child(state, child_parse);
			ec_parse_free(child_parse);
			break;
		}

		off += ret;
	}

	if (count < node->min) {
		ec_parse_free_children(state);
		return EC_PARSE_NOMATCH;
	}

	return off;

fail:
	ec_strvec_free(childvec);
	return -1;
}

static int
__ec_node_many_complete(struct ec_node_many *node, unsigned int max,
			struct ec_comp *comp,
			const struct ec_strvec *strvec)
{
	struct ec_parse *parse = ec_comp_get_state(comp);
	struct ec_strvec *childvec = NULL;
	unsigned int i;
	int ret;

	/* first, try to complete with the child node */
	ret = ec_node_complete_child(node->child, comp, strvec);
	if (ret < 0)
		goto fail;

	/* we're done, we reached the max number of nodes */
	if (max == 1)
		return 0;

	/* if there is a maximum, decrease it before recursion */
	if (max != 0)
		max--;

	/* then, if the node matches the beginning of the strvec, try to
	 * complete the rest */
	for (i = 0; i < ec_strvec_len(strvec); i++) {
		childvec = ec_strvec_ndup(strvec, 0, i);
		if (childvec == NULL)
			goto fail;

		ret = ec_node_parse_child(node->child, parse, childvec);
		if (ret < 0)
			goto fail;

		ec_strvec_free(childvec);
		childvec = NULL;

		if ((unsigned int)ret != i) {
			if (ret != EC_PARSE_NOMATCH)
				ec_parse_del_last_child(parse);
			continue;
		}

		childvec = ec_strvec_ndup(strvec, i, ec_strvec_len(strvec) - i);
		if (childvec == NULL) {
			ec_parse_del_last_child(parse);
			goto fail;
		}

		ret = __ec_node_many_complete(node, max, comp, childvec);
		ec_parse_del_last_child(parse);
		ec_strvec_free(childvec);
		childvec = NULL;

		if (ret < 0)
			goto fail;
	}

	return 0;

fail:
	ec_strvec_free(childvec);
	return -1;
}

static int
ec_node_many_complete(const struct ec_node *gen_node,
		struct ec_comp *comp,
		const struct ec_strvec *strvec)
{
	struct ec_node_many *node = (struct ec_node_many *)gen_node;

	return __ec_node_many_complete(node, node->max,	comp,
				strvec);
}

static void ec_node_many_free_priv(struct ec_node *gen_node)
{
	struct ec_node_many *node = (struct ec_node_many *)gen_node;

	ec_node_free(node->child);
}

static size_t
ec_node_many_get_children_count(const struct ec_node *gen_node)
{
	struct ec_node_many *node = (struct ec_node_many *)gen_node;

	if (node->child)
		return 1;
	return 0;
}

static int
ec_node_many_get_child(const struct ec_node *gen_node, size_t i,
		struct ec_node **child, unsigned int *refs)
{
	struct ec_node_many *node = (struct ec_node_many *)gen_node;

	if (i >= 1)
		return -1;

	*child = node->child;
	*refs = 2;
	return 0;
}

static const struct ec_config_schema ec_node_many_schema[] = {
	{
		.key = "child",
		.desc = "The child node.",
		.type = EC_CONFIG_TYPE_NODE,
	},
	{
		.key = "min",
		.desc = "The minimum number of matches (default = 0).",
		.type = EC_CONFIG_TYPE_UINT64,
	},
	{
		.key = "max",
		.desc = "The maximum number of matches. If 0, there is "
		"no maximum (default = 0).",
		.type = EC_CONFIG_TYPE_UINT64,
	},
	{
		.type = EC_CONFIG_TYPE_NONE,
	},
};

static int ec_node_many_set_config(struct ec_node *gen_node,
				const struct ec_config *config)
{
	struct ec_node_many *node = (struct ec_node_many *)gen_node;
	const struct ec_config *child, *min, *max;

	child = ec_config_dict_get(config, "child");
	if (child == NULL)
		goto fail;
	if (ec_config_get_type(child) != EC_CONFIG_TYPE_NODE) {
		errno = EINVAL;
		goto fail;
	}
	min = ec_config_dict_get(config, "min");
	if (min != NULL && (ec_config_get_type(min) != EC_CONFIG_TYPE_UINT64 ||
				min->u64 >= UINT_MAX)) {
		errno = EINVAL;
		goto fail;
	}
	max = ec_config_dict_get(config, "max");
	if (max != NULL && (ec_config_get_type(max) != EC_CONFIG_TYPE_UINT64 ||
				max->u64 >= UINT_MAX)) {
		errno = EINVAL;
		goto fail;
	}

	if (node->child != NULL)
		ec_node_free(node->child);
	node->child = ec_node_clone(child->node);
	if (min == NULL)
		node->min = 0;
	else
		node->min = min->u64;
	if (max == NULL)
		node->max = 0;
	else
		node->max = max->u64;

	return 0;

fail:
	return -1;
}

static struct ec_node_type ec_node_many_type = {
	.name = "many",
	.schema = ec_node_many_schema,
	.set_config = ec_node_many_set_config,
	.parse = ec_node_many_parse,
	.complete = ec_node_many_complete,
	.size = sizeof(struct ec_node_many),
	.free_priv = ec_node_many_free_priv,
	.get_children_count = ec_node_many_get_children_count,
	.get_child = ec_node_many_get_child,
};

EC_NODE_TYPE_REGISTER(ec_node_many_type);

int
ec_node_many_set_params(struct ec_node *gen_node, struct ec_node *child,
	unsigned int min, unsigned int max)
{
	const struct ec_config *cur_config = NULL;
	struct ec_config *config = NULL;
	int ret;

	if (ec_node_check_type(gen_node, &ec_node_many_type) < 0)
		goto fail;

	cur_config = ec_node_get_config(gen_node);
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

	if (ec_config_dict_set(config, "min", ec_config_u64(min)) < 0)
		goto fail;
	if (ec_config_dict_set(config, "max", ec_config_u64(max)) < 0)
		goto fail;

	ret = ec_node_set_config(gen_node, config);
	config = NULL; /* freed */
	if (ret < 0)
		goto fail;

	return 0;

fail:
	ec_config_free(config);
	ec_node_free(child);
	return -1;
}

struct ec_node *ec_node_many(const char *id, struct ec_node *child,
	unsigned int min, unsigned int max)
{
	struct ec_node *gen_node = NULL;

	if (child == NULL)
		return NULL;

	gen_node = ec_node_from_type(&ec_node_many_type, id);
	if (gen_node == NULL)
		goto fail;

	if (ec_node_many_set_params(gen_node, child, min, max) < 0) {
		child = NULL;
		goto fail;
	}
	child = NULL;

	return gen_node;

fail:
	ec_node_free(child);
	return NULL;
}

/* LCOV_EXCL_START */
static int ec_node_many_testcase(void)
{
	struct ec_node *node;
	int testres = 0;

	node = ec_node_many(EC_NO_ID, ec_node_str(EC_NO_ID, "foo"), 0, 0);
	if (node == NULL) {
		EC_LOG(EC_LOG_ERR, "cannot create node\n");
		return -1;
	}
	testres |= EC_TEST_CHECK_PARSE(node, 0);
	testres |= EC_TEST_CHECK_PARSE(node, 0, "bar");
	testres |= EC_TEST_CHECK_PARSE(node, 1, "foo", "bar");
	testres |= EC_TEST_CHECK_PARSE(node, 2, "foo", "foo", "bar");
	testres |= EC_TEST_CHECK_PARSE(node, 0);
	ec_node_free(node);

	node = ec_node_many(EC_NO_ID, ec_node_str(EC_NO_ID, "foo"), 1, 0);
	if (node == NULL) {
		EC_LOG(EC_LOG_ERR, "cannot create node\n");
		return -1;
	}
	testres |= EC_TEST_CHECK_PARSE(node, -1, "bar");
	testres |= EC_TEST_CHECK_PARSE(node, 1, "foo", "bar");
	testres |= EC_TEST_CHECK_PARSE(node, 2, "foo", "foo", "bar");
	testres |= EC_TEST_CHECK_PARSE(node, -1);
	ec_node_free(node);

	node = ec_node_many(EC_NO_ID, ec_node_str(EC_NO_ID, "foo"), 1, 2);
	if (node == NULL) {
		EC_LOG(EC_LOG_ERR, "cannot create node\n");
		return -1;
	}
	testres |= EC_TEST_CHECK_PARSE(node, -1, "bar");
	testres |= EC_TEST_CHECK_PARSE(node, 1, "foo", "bar");
	testres |= EC_TEST_CHECK_PARSE(node, 2, "foo", "foo", "bar");
	testres |= EC_TEST_CHECK_PARSE(node, 2, "foo", "foo", "foo");
	testres |= EC_TEST_CHECK_PARSE(node, -1);
	ec_node_free(node);

	/* test completion */
	node = ec_node_many(EC_NO_ID, ec_node_str(EC_NO_ID, "foo"), 2, 4);
	if (node == NULL) {
		EC_LOG(EC_LOG_ERR, "cannot create node\n");
		return -1;
	}
	testres |= EC_TEST_CHECK_COMPLETE(node,
		"", EC_NODE_ENDLIST,
		"foo", EC_NODE_ENDLIST);
	testres |= EC_TEST_CHECK_COMPLETE(node,
		"f", EC_NODE_ENDLIST,
		"foo", EC_NODE_ENDLIST);
	testres |= EC_TEST_CHECK_COMPLETE(node,
		"foo", EC_NODE_ENDLIST,
		"foo", EC_NODE_ENDLIST);
	testres |= EC_TEST_CHECK_COMPLETE(node,
		"foo", "", EC_NODE_ENDLIST,
		"foo", EC_NODE_ENDLIST);
	testres |= EC_TEST_CHECK_COMPLETE(node,
		"foo", "foo", "", EC_NODE_ENDLIST,
		"foo", EC_NODE_ENDLIST);
	testres |= EC_TEST_CHECK_COMPLETE(node,
		"foo", "foo", "foo", "", EC_NODE_ENDLIST,
		"foo", EC_NODE_ENDLIST);
	testres |= EC_TEST_CHECK_COMPLETE(node,
		"foo", "foo", "foo", "foo", "", EC_NODE_ENDLIST,
		EC_NODE_ENDLIST);
	ec_node_free(node);

	return testres;
}
/* LCOV_EXCL_STOP */

static struct ec_test ec_node_many_test = {
	.name = "node_many",
	.test = ec_node_many_testcase,
};

EC_TEST_REGISTER(ec_node_many_test);
