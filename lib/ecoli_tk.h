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

#ifndef ECOLI_TK_
#define ECOLI_TK_

#include <sys/queue.h>
#include <sys/types.h>
#include <stdio.h>

#define EC_TK_ENDLIST ((void *)1)

struct ec_tk;
struct ec_parsed_tk;
struct ec_strvec;

typedef struct ec_parsed_tk *(*ec_tk_parse_t)(const struct ec_tk *tk,
	const struct ec_strvec *strvec);
typedef struct ec_completed_tk *(*ec_tk_complete_t)(const struct ec_tk *tk,
	const struct ec_strvec *strvec);
typedef void (*ec_tk_free_priv_t)(struct ec_tk *);

struct ec_tk_ops {
	const char *typename;
	ec_tk_parse_t parse;
	ec_tk_complete_t complete;
	ec_tk_free_priv_t free_priv;
};

struct ec_tk {
	const struct ec_tk_ops *ops;
	char *id;
};

struct ec_tk *ec_tk_new(const char *id, const struct ec_tk_ops *ops,
	size_t priv_size);
void ec_tk_free(struct ec_tk *tk);


TAILQ_HEAD(ec_parsed_tk_list, ec_parsed_tk);

/*
  tk == NULL + empty children list means "no match"
*/
struct ec_parsed_tk {
	TAILQ_ENTRY(ec_parsed_tk) next;
	struct ec_parsed_tk_list children;
	const struct ec_tk *tk;
	struct ec_strvec *strvec;
};

struct ec_parsed_tk *ec_parsed_tk_new(void);

void ec_parsed_tk_set_match(struct ec_parsed_tk *parsed_tk,
	const struct ec_tk *tk, struct ec_strvec *strvec);

/* XXX we could use a cache to store possible completions or match: the
 * cache would be per-node, and would be reset for each call to parse()
 * or complete() ? */
/* a NULL return value is an error, with errno set
  ENOTSUP: no ->parse() operation
*/
struct ec_parsed_tk *ec_tk_parse(const struct ec_tk *token, const char *str);

/* mostly internal to tokens */
/* XXX it should not reset cache
 * ... not sure... it is used by tests */
struct ec_parsed_tk *ec_tk_parse_tokens(const struct ec_tk *token,
	const struct ec_strvec *strvec);

void ec_parsed_tk_add_child(struct ec_parsed_tk *parsed_tk,
	struct ec_parsed_tk *child);
void ec_parsed_tk_free_children(struct ec_parsed_tk *parsed_tk);
void ec_parsed_tk_dump(FILE *out, const struct ec_parsed_tk *parsed_tk);
void ec_parsed_tk_free(struct ec_parsed_tk *parsed_tk);

struct ec_parsed_tk *ec_parsed_tk_find_first(struct ec_parsed_tk *parsed_tk,
	const char *id);

const char *ec_parsed_tk_to_string(const struct ec_parsed_tk *parsed_tk);
size_t ec_parsed_tk_len(const struct ec_parsed_tk *parsed_tk);
size_t ec_parsed_tk_matches(const struct ec_parsed_tk *parsed_tk);

struct ec_completed_tk_elt {
	TAILQ_ENTRY(ec_completed_tk_elt) next;
	const struct ec_tk *tk;
	char *add;
};

TAILQ_HEAD(ec_completed_tk_elt_list, ec_completed_tk_elt);


struct ec_completed_tk {
	struct ec_completed_tk_elt_list elts;
	unsigned count;
	unsigned count_match;
	char *smallest_start;
};

/*
 * return a completed_tk object filled with elts
 * return NULL on error (nomem?)
 */
struct ec_completed_tk *ec_tk_complete(const struct ec_tk *token,
	const char *str);
struct ec_completed_tk *ec_tk_complete_tokens(const struct ec_tk *token,
	const struct ec_strvec *strvec);
struct ec_completed_tk *ec_completed_tk_new(void);
struct ec_completed_tk_elt *ec_completed_tk_elt_new(const struct ec_tk *tk,
	const char *add);
void ec_completed_tk_add_elt(struct ec_completed_tk *completed_tk,
	struct ec_completed_tk_elt *elt);
void ec_completed_tk_elt_free(struct ec_completed_tk_elt *elt);
void ec_completed_tk_merge(struct ec_completed_tk *completed_tk1,
	struct ec_completed_tk *completed_tk2);
void ec_completed_tk_free(struct ec_completed_tk *completed_tk);
void ec_completed_tk_dump(FILE *out,
	const struct ec_completed_tk *completed_tk);
struct ec_completed_tk *ec_tk_default_complete(const struct ec_tk *gen_tk,
	const struct ec_strvec *strvec);

/* cannot return NULL */
const char *ec_completed_tk_smallest_start(
	const struct ec_completed_tk *completed_tk);

unsigned int ec_completed_tk_count_match(
	const struct ec_completed_tk *completed_tk);

enum ec_completed_tk_filter_flags {
	ITER_MATCH = 1,
	ITER_NO_MATCH,
};

struct ec_completed_tk_iter {
	enum ec_completed_tk_filter_flags flags;
	const struct ec_completed_tk *completed_tk;
	const struct ec_completed_tk_elt *cur;
};

struct ec_completed_tk_iter *
ec_completed_tk_iter_new(struct ec_completed_tk *completed_tk,
	enum ec_completed_tk_filter_flags flags);

const struct ec_completed_tk_elt *ec_completed_tk_iter_next(
	struct ec_completed_tk_iter *iter);

void ec_completed_tk_iter_free(struct ec_completed_tk_iter *iter);
#endif
