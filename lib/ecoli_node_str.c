/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2016, Olivier MATZ <zer0@droids-corp.org>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <ecoli_log.h>
#include <ecoli_malloc.h>
#include <ecoli_test.h>
#include <ecoli_strvec.h>
#include <ecoli_node.h>
#include <ecoli_config.h>
#include <ecoli_parse.h>
#include <ecoli_complete.h>
#include <ecoli_node_str.h>

EC_LOG_TYPE_REGISTER(node_str);

struct ec_node_str {
	struct ec_node gen;
	char *string;
	unsigned len;
};

static int
ec_node_str_parse(const struct ec_node *gen_node,
		struct ec_parse *state,
		const struct ec_strvec *strvec)
{
	struct ec_node_str *node = (struct ec_node_str *)gen_node;
	const char *str;

	(void)state;

	if (node->string == NULL) {
		errno = EINVAL;
		return -1;
	}

	if (ec_strvec_len(strvec) == 0)
		return EC_PARSE_NOMATCH;

	str = ec_strvec_val(strvec, 0);
	if (strcmp(str, node->string) != 0)
		return EC_PARSE_NOMATCH;

	return 1;
}

static int
ec_node_str_complete(const struct ec_node *gen_node,
		struct ec_comp *comp,
		const struct ec_strvec *strvec)
{
	struct ec_node_str *node = (struct ec_node_str *)gen_node;
	const char *str;
	size_t n = 0;

	if (ec_strvec_len(strvec) != 1)
		return 0;

	str = ec_strvec_val(strvec, 0);
	for (n = 0; n < node->len; n++) {
		if (str[n] != node->string[n])
			break;
	}

	/* no completion */
	if (str[n] != '\0')
		return EC_PARSE_NOMATCH;

	if (ec_comp_add_item(comp, gen_node, NULL, EC_COMP_FULL,
					str, node->string) < 0)
		return -1;

	return 0;
}

static const char *ec_node_str_desc(const struct ec_node *gen_node)
{
	struct ec_node_str *node = (struct ec_node_str *)gen_node;

	return node->string;
}

static void ec_node_str_free_priv(struct ec_node *gen_node)
{
	struct ec_node_str *node = (struct ec_node_str *)gen_node;

	ec_free(node->string);
}

static const struct ec_config_schema ec_node_str_schema[] = {
	{
		.key = "string",
		.desc = "The string to match.",
		.type = EC_CONFIG_TYPE_STRING,
	},
	{
		.type = EC_CONFIG_TYPE_NONE,
	},
};

static int ec_node_str_set_config(struct ec_node *gen_node,
				const struct ec_config *config)
{
	struct ec_node_str *node = (struct ec_node_str *)gen_node;
	const struct ec_config *value = NULL;
	char *s = NULL;

	value = ec_config_dict_get(config, "string");
	if (value == NULL) {
		errno = EINVAL;
		goto fail;
	}

	s = ec_strdup(value->string);
	if (s == NULL)
		goto fail;

	ec_free(node->string);
	node->string = s;
	node->len = strlen(node->string);

	return 0;

fail:
	ec_free(s);
	return -1;
}

static struct ec_node_type ec_node_str_type = {
	.name = "str",
	.schema = ec_node_str_schema,
	.set_config = ec_node_str_set_config,
	.parse = ec_node_str_parse,
	.complete = ec_node_str_complete,
	.desc = ec_node_str_desc,
	.size = sizeof(struct ec_node_str),
	.free_priv = ec_node_str_free_priv,
};

EC_NODE_TYPE_REGISTER(ec_node_str_type);

int ec_node_str_set_str(struct ec_node *gen_node, const char *str)
{
	struct ec_config *config = NULL;
	int ret;

	if (ec_node_check_type(gen_node, &ec_node_str_type) < 0)
		goto fail;

	if (str == NULL) {
		errno = EINVAL;
		goto fail;
	}

	config = ec_config_dict();
	if (config == NULL)
		goto fail;

	ret = ec_config_dict_set(config, "string", ec_config_string(str));
	if (ret < 0)
		goto fail;

	ret = ec_node_set_config(gen_node, config);
	config = NULL;
	if (ret < 0)
		goto fail;

	return 0;

fail:
	ec_config_free(config);
	return -1;
}

struct ec_node *ec_node_str(const char *id, const char *str)
{
	struct ec_node *gen_node = NULL;

	gen_node = ec_node_from_type(&ec_node_str_type, id);
	if (gen_node == NULL)
		goto fail;

	if (ec_node_str_set_str(gen_node, str) < 0)
		goto fail;

	return gen_node;

fail:
	ec_node_free(gen_node);
	return NULL;
}

/* LCOV_EXCL_START */
static int ec_node_str_testcase(void)
{
	struct ec_node *node;
	int testres = 0;

	node = ec_node_str(EC_NO_ID, "foo");
	if (node == NULL) {
		EC_LOG(EC_LOG_ERR, "cannot create node\n");
		return -1;
	}
	testres |= EC_TEST_CHECK(!strcmp(ec_node_desc(node), "foo"),
		"Invalid node description.");
	testres |= EC_TEST_CHECK_PARSE(node, 1, "foo");
	testres |= EC_TEST_CHECK_PARSE(node, 1, "foo", "bar");
	testres |= EC_TEST_CHECK_PARSE(node, -1, "foobar");
	testres |= EC_TEST_CHECK_PARSE(node, -1, " foo");
	testres |= EC_TEST_CHECK_PARSE(node, -1, "");
	ec_node_free(node);

	node = ec_node_str(EC_NO_ID, "Здравствуйте");
	if (node == NULL) {
		EC_LOG(EC_LOG_ERR, "cannot create node\n");
		return -1;
	}
	testres |= EC_TEST_CHECK_PARSE(node, 1, "Здравствуйте");
	testres |= EC_TEST_CHECK_PARSE(node, 1, "Здравствуйте",
		"John!");
	testres |= EC_TEST_CHECK_PARSE(node, -1, "foo");
	testres |= EC_TEST_CHECK_PARSE(node, -1, "");
	ec_node_free(node);

	/* an empty string node always matches */
	node = ec_node_str(EC_NO_ID, "");
	if (node == NULL) {
		EC_LOG(EC_LOG_ERR, "cannot create node\n");
		return -1;
	}
	testres |= EC_TEST_CHECK_PARSE(node, 1, "");
	testres |= EC_TEST_CHECK_PARSE(node, 1, "", "foo");
	testres |= EC_TEST_CHECK_PARSE(node, -1, "foo");
	ec_node_free(node);

	/* test completion */
	node = ec_node_str(EC_NO_ID, "foo");
	if (node == NULL) {
		EC_LOG(EC_LOG_ERR, "cannot create node\n");
		return -1;
	}
	testres |= EC_TEST_CHECK_COMPLETE(node,
		EC_NODE_ENDLIST,
		EC_NODE_ENDLIST);
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
		"x", EC_NODE_ENDLIST,
		EC_NODE_ENDLIST);
	ec_node_free(node);

	return testres;
}
/* LCOV_EXCL_STOP */

static struct ec_test ec_node_str_test = {
	.name = "node_str",
	.test = ec_node_str_testcase,
};

EC_TEST_REGISTER(ec_node_str_test);
