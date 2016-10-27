/*
 * Copyright (c) 2016, Olivier MATZ <zer0@droids-corp.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the University of California, Berkeley nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE REGENTS AND CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>

#include <ecoli_malloc.h>
#include <ecoli_tk.h>
#include <ecoli_tk_or.h>
#include <ecoli_tk_str.h>
#include <ecoli_test.h>

static struct ec_parsed_tk *parse(const struct ec_tk *gen_tk,
	const char *str)
{
	struct ec_tk_or *tk = (struct ec_tk_or *)gen_tk;
	struct ec_parsed_tk *parsed_tk, *child_parsed_tk;
	unsigned int i;

	parsed_tk = ec_parsed_tk_new(gen_tk);
	if (parsed_tk == NULL)
		return NULL;

	for (i = 0; i < tk->len; i++) {
		child_parsed_tk = ec_tk_parse(tk->table[i], str);
		if (child_parsed_tk != NULL)
			break;
	}

	if (child_parsed_tk == NULL)
		goto fail;

	ec_parsed_tk_add_child(parsed_tk, child_parsed_tk);

	parsed_tk->str = ec_strndup(child_parsed_tk->str,
		strlen(child_parsed_tk->str));

	return parsed_tk;

 fail:
	ec_parsed_tk_free(parsed_tk);
	return NULL;
}

static struct ec_completed_tk *complete(const struct ec_tk *gen_tk,
	const char *str)
{
	struct ec_tk_or *tk = (struct ec_tk_or *)gen_tk;
	struct ec_completed_tk *completed_tk = NULL, *child_completed_tk;
	size_t n;

	for (n = 0; n < tk->len; n++) {
		child_completed_tk = ec_tk_complete(tk->table[n], str);

		if (child_completed_tk == NULL)
			continue;

		completed_tk = ec_completed_tk_merge(completed_tk,
			child_completed_tk);
	}

	return completed_tk;
}

static void free_priv(struct ec_tk *gen_tk)
{
	struct ec_tk_or *tk = (struct ec_tk_or *)gen_tk;

	ec_free(tk->table);
}

static struct ec_tk_ops or_ops = {
	.parse = parse,
	.complete = complete,
	.free_priv = free_priv,
};

struct ec_tk *ec_tk_or_new(const char *id)
{
	struct ec_tk_or *tk = NULL;

	tk = (struct ec_tk_or *)ec_tk_new(id, &or_ops, sizeof(*tk));
	if (tk == NULL)
		goto fail;

	tk->table = NULL;
	tk->len = 0;

	return &tk->gen;

fail:
	ec_free(tk);
	return NULL;
}

struct ec_tk *ec_tk_or_new_list(const char *id, ...)
{
	struct ec_tk_or *tk = NULL;
	struct ec_tk *child;
	va_list ap;

	va_start(ap, id);

	tk = (struct ec_tk_or *)ec_tk_or_new(id);
	if (tk == NULL)
		goto fail;

	for (child = va_arg(ap, struct ec_tk *);
	     child != EC_TK_ENDLIST;
	     child = va_arg(ap, struct ec_tk *)) {
		if (child == NULL)
			goto fail;

		ec_tk_or_add(&tk->gen, child);
	}

	va_end(ap);
	return &tk->gen;

fail:
	ec_free(tk); // XXX use tk_free? we need to delete all child on error
	va_end(ap);
	return NULL;
}

int ec_tk_or_add(struct ec_tk *tk, struct ec_tk *child)
{
	struct ec_tk_or *or = (struct ec_tk_or *)tk;
	struct ec_tk **table;

	assert(tk != NULL);
	assert(child != NULL);

	table = realloc(or->table, (or->len + 1) * sizeof(*or->table));
	if (table == NULL)
		return -1;

	or->table = table;
	table[or->len] = child;
	or->len ++;

	return 0;
}

static int testcase(void)
{
	struct ec_tk *tk;
	int ret = 0;

	/* all inputs starting with foo should match */
	tk = ec_tk_or_new_list(NULL,
		ec_tk_str_new(NULL, "foo"),
		ec_tk_str_new(NULL, "bar"),
		EC_TK_ENDLIST);
	if (tk == NULL) {
		printf("cannot create tk\n");
		return -1;
	}
	ret |= EC_TEST_CHECK_TK_PARSE(tk, "foo", "foo");
	ret |= EC_TEST_CHECK_TK_PARSE(tk, "fooxxx", "foo");
	ret |= EC_TEST_CHECK_TK_PARSE(tk, "bar", "bar");
	ret |= EC_TEST_CHECK_TK_PARSE(tk, "oo", NULL);
	ec_tk_free(tk);

	/* test completion */
	tk = ec_tk_or_new_list(NULL,
		ec_tk_str_new(NULL, "foo"),
		ec_tk_str_new(NULL, "bar"),
		ec_tk_str_new(NULL, "bar2"),
		ec_tk_str_new(NULL, "toto"),
		ec_tk_str_new(NULL, "titi"),
		EC_TK_ENDLIST);
	if (tk == NULL) {
		printf("cannot create tk\n");
		return -1;
	}
	ret |= EC_TEST_CHECK_TK_COMPLETE(tk, "", "");
	ret |= EC_TEST_CHECK_TK_COMPLETE(tk, "f", "oo");
	ret |= EC_TEST_CHECK_TK_COMPLETE(tk, "b", "ar");
	ret |= EC_TEST_CHECK_TK_COMPLETE(tk, "t", "");
	ret |= EC_TEST_CHECK_TK_COMPLETE(tk, "to", "to");
	ret |= EC_TEST_CHECK_TK_COMPLETE(tk, "x", NULL);
	ec_tk_free(tk);

	return ret;
}

static struct ec_test test = {
	.name = "tk_or",
	.test = testcase,
};

EC_REGISTER_TEST(test);
