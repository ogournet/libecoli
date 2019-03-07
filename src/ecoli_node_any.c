/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2016, Olivier MATZ <zer0@droids-corp.org>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <ecoli_malloc.h>
#include <ecoli_log.h>
#include <ecoli_test.h>
#include <ecoli_strvec.h>
#include <ecoli_node.h>
#include <ecoli_parse.h>
#include <ecoli_complete.h>
#include <ecoli_keyval.h>
#include <ecoli_config.h>
#include <ecoli_node_any.h>

EC_LOG_TYPE_REGISTER(node_any);

struct ec_node_any {
	struct ec_node gen;
	char *attr_name;
};

static int ec_node_any_parse(const struct ec_node *gen_node,
			struct ec_parse *state,
			const struct ec_strvec *strvec)
{
	struct ec_node_any *node = (struct ec_node_any *)gen_node;
	const struct ec_keyval *attrs;

	(void)state;

	if (ec_strvec_len(strvec) == 0)
		return EC_PARSE_NOMATCH;
	if (node->attr_name != NULL) {
		attrs = ec_strvec_get_attrs(strvec, 0);
		if (attrs == NULL || !ec_keyval_has_key(attrs, node->attr_name))
			return EC_PARSE_NOMATCH;
	}

	return 1;
}

static void ec_node_any_free_priv(struct ec_node *gen_node)
{
	struct ec_node_any *node = (struct ec_node_any *)gen_node;

	ec_free(node->attr_name);
}

static const struct ec_config_schema ec_node_any_schema[] = {
	{
		.key = "attr",
		.desc = "The optional attribute name to attach.",
		.type = EC_CONFIG_TYPE_STRING,
	},
	{
		.type = EC_CONFIG_TYPE_NONE,
	},
};

static int ec_node_any_set_config(struct ec_node *gen_node,
				const struct ec_config *config)
{
	struct ec_node_any *node = (struct ec_node_any *)gen_node;
	const struct ec_config *value = NULL;
	char *s = NULL;

	value = ec_config_dict_get(config, "attr");
	if (value != NULL) {
		s = ec_strdup(value->string);
		if (s == NULL)
			goto fail;
	}

	ec_free(node->attr_name);
	node->attr_name = s;

	return 0;

fail:
	ec_free(s);
	return -1;
}

static struct ec_node_type ec_node_any_type = {
	.name = "any",
	.schema = ec_node_any_schema,
	.set_config = ec_node_any_set_config,
	.parse = ec_node_any_parse,
	.complete = ec_node_complete_unknown,
	.size = sizeof(struct ec_node_any),
	.free_priv = ec_node_any_free_priv,
};

EC_NODE_TYPE_REGISTER(ec_node_any_type);

struct ec_node *
ec_node_any(const char *id, const char *attr)
{
	struct ec_config *config = NULL;
	struct ec_node *gen_node = NULL;
	int ret;

	gen_node = ec_node_from_type(&ec_node_any_type, id);
	if (gen_node == NULL)
		return NULL;

	config = ec_config_dict();
	if (config == NULL)
		goto fail;

	ret = ec_config_dict_set(config, "attr", ec_config_string(attr));
	if (ret < 0)
		goto fail;

	ret = ec_node_set_config(gen_node, config);
	config = NULL;
	if (ret < 0)
		goto fail;

	return gen_node;

fail:
	ec_config_free(config);
	ec_node_free(gen_node);
	return NULL;
}

/* LCOV_EXCL_START */
static int ec_node_any_testcase(void)
{
	struct ec_node *node;
	int testres = 0;

	node = ec_node("any", EC_NO_ID);
	if (node == NULL) {
		EC_LOG(EC_LOG_ERR, "cannot create node\n");
		return -1;
	}
	testres |= EC_TEST_CHECK_PARSE(node, 1, "foo");
	testres |= EC_TEST_CHECK_PARSE(node, 1, "foo", "bar");
	testres |= EC_TEST_CHECK_PARSE(node, -1);
	ec_node_free(node);

	/* never completes */
	node = ec_node("any", EC_NO_ID);
	if (node == NULL) {
		EC_LOG(EC_LOG_ERR, "cannot create node\n");
		return -1;
	}
	testres |= EC_TEST_CHECK_COMPLETE(node,
		"", EC_NODE_ENDLIST,
		EC_NODE_ENDLIST);
	testres |= EC_TEST_CHECK_COMPLETE(node,
		"foo", EC_NODE_ENDLIST,
		EC_NODE_ENDLIST);
	ec_node_free(node);

	return testres;
}
/* LCOV_EXCL_STOP */

static struct ec_test ec_node_any_test = {
	.name = "node_any",
	.test = ec_node_any_testcase,
};

EC_TEST_REGISTER(ec_node_any_test);
