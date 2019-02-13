/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2016, Olivier MATZ <zer0@droids-corp.org>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <regex.h>

#include <ecoli_log.h>
#include <ecoli_malloc.h>
#include <ecoli_test.h>
#include <ecoli_strvec.h>
#include <ecoli_node.h>
#include <ecoli_parse.h>
#include <ecoli_complete.h>
#include <ecoli_config.h>
#include <ecoli_node_re.h>

EC_LOG_TYPE_REGISTER(node_re);

struct ec_node_re {
	struct ec_node gen;
	char *re_str;
	regex_t re;
};

static int
ec_node_re_parse(const struct ec_node *gen_node,
		struct ec_parse *state,
		const struct ec_strvec *strvec)
{
	struct ec_node_re *node = (struct ec_node_re *)gen_node;
	const char *str;
	regmatch_t pos;

	(void)state;

	if (ec_strvec_len(strvec) == 0)
		return EC_PARSE_NOMATCH;

	str = ec_strvec_val(strvec, 0);
	if (regexec(&node->re, str, 1, &pos, 0) != 0)
		return EC_PARSE_NOMATCH;
	if (pos.rm_so != 0 || pos.rm_eo != (int)strlen(str))
		return EC_PARSE_NOMATCH;

	return 1;
}

static void ec_node_re_free_priv(struct ec_node *gen_node)
{
	struct ec_node_re *node = (struct ec_node_re *)gen_node;

	if (node->re_str != NULL) {
		ec_free(node->re_str);
		regfree(&node->re);
	}
}

static const struct ec_config_schema ec_node_re_schema[] = {
	{
		.key = "pattern",
		.desc = "The pattern to match.",
		.type = EC_CONFIG_TYPE_STRING,
	},
	{
		.type = EC_CONFIG_TYPE_NONE,
	},
};

static int ec_node_re_set_config(struct ec_node *gen_node,
				const struct ec_config *config)
{
	struct ec_node_re *node = (struct ec_node_re *)gen_node;
	const struct ec_config *value = NULL;
	char *s = NULL;
	regex_t re;
	int ret;

	value = ec_config_dict_get(config, "pattern");
	if (value == NULL) {
		errno = EINVAL;
		goto fail;
	}

	s = ec_strdup(value->string);
	if (s == NULL)
		goto fail;

	ret = regcomp(&re, s, REG_EXTENDED);
	if (ret != 0) {
		if (ret == REG_ESPACE)
			errno = ENOMEM;
		else
			errno = EINVAL;
		goto fail;
	}

	if (node->re_str != NULL) {
		ec_free(node->re_str);
		regfree(&node->re);
	}
	node->re_str = s;
	node->re = re;

	return 0;

fail:
	ec_free(s);
	return -1;
}

static struct ec_node_type ec_node_re_type = {
	.name = "re",
	.schema = ec_node_re_schema,
	.set_config = ec_node_re_set_config,
	.parse = ec_node_re_parse,
	.complete = ec_node_complete_unknown,
	.size = sizeof(struct ec_node_re),
	.free_priv = ec_node_re_free_priv,
};

EC_NODE_TYPE_REGISTER(ec_node_re_type);

int ec_node_re_set_regexp(struct ec_node *gen_node, const char *str)
{
	struct ec_config *config = NULL;
	int ret;

	EC_CHECK_ARG(str != NULL, -1, EINVAL);

	if (ec_node_check_type(gen_node, &ec_node_re_type) < 0)
		goto fail;

	config = ec_config_dict();
	if (config == NULL)
		goto fail;

	ret = ec_config_dict_set(config, "pattern", ec_config_string(str));
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

struct ec_node *ec_node_re(const char *id, const char *re_str)
{
	struct ec_node *gen_node = NULL;

	gen_node = ec_node_from_type(&ec_node_re_type, id);
	if (gen_node == NULL)
		goto fail;

	if (ec_node_re_set_regexp(gen_node, re_str) < 0)
		goto fail;

	return gen_node;

fail:
	ec_node_free(gen_node);
	return NULL;
}

/* LCOV_EXCL_START */
static int ec_node_re_testcase(void)
{
	struct ec_node *node;
	int testres = 0;

	node = ec_node_re(EC_NO_ID, "fo+|bar");
	if (node == NULL) {
		EC_LOG(EC_LOG_ERR, "cannot create node\n");
		return -1;
	}
	testres |= EC_TEST_CHECK_PARSE(node, 1, "foo");
	testres |= EC_TEST_CHECK_PARSE(node, 1, "foo", "bar");
	testres |= EC_TEST_CHECK_PARSE(node, 1, "bar");
	testres |= EC_TEST_CHECK_PARSE(node, -1, "foobar");
	testres |= EC_TEST_CHECK_PARSE(node, -1, " foo");
	testres |= EC_TEST_CHECK_PARSE(node, -1, "");
	ec_node_free(node);

	return testres;
}
/* LCOV_EXCL_STOP */

static struct ec_test ec_node_re_test = {
	.name = "node_re",
	.test = ec_node_re_testcase,
};

EC_TEST_REGISTER(ec_node_re_test);
