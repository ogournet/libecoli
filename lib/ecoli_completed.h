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

#ifndef ECOLI_COMPLETED_
#define ECOLI_COMPLETED_

#include <sys/queue.h>
#include <sys/types.h>
#include <stdio.h>

struct ec_node;

struct ec_completed_elt {
	TAILQ_ENTRY(ec_completed_elt) next;
	const struct ec_node *node;
	char *add;

	/* reverse order: [0] = last parsed, [len-1] = root */
	const struct ec_node **path;
	size_t pathlen;
};

TAILQ_HEAD(ec_completed_elt_list, ec_completed_elt);

struct ec_completed {
	struct ec_completed_elt_list elts;
	unsigned count;
	unsigned count_match;
	char *smallest_start;
};

/*
 * return a completed object filled with elts
 * return NULL on error (nomem?)
 */
struct ec_completed *ec_node_complete(struct ec_node *node,
	const char *str);
struct ec_completed *ec_node_complete_strvec(struct ec_node *node,
	const struct ec_strvec *strvec);

/* internal: used by nodes */
struct ec_completed *ec_node_complete_child(struct ec_node *node,
					struct ec_parsed *state,
					const struct ec_strvec *strvec);

struct ec_completed *ec_completed(void);

/* XXX add completion type: full, partial, none */
int ec_completed_add_elt(struct ec_completed *completed,
			struct ec_parsed *parsed,
			const struct ec_node *node, const char *add);
void ec_completed_elt_free(struct ec_completed_elt *elt);
void ec_completed_merge(struct ec_completed *completed1,
	struct ec_completed *completed2);
void ec_completed_free(struct ec_completed *completed);
void ec_completed_dump(FILE *out,
	const struct ec_completed *completed);
struct ec_completed *ec_node_default_complete(const struct ec_node *gen_node,
					struct ec_parsed *state,
					const struct ec_strvec *strvec);

/* cannot return NULL */
const char *ec_completed_smallest_start(
	const struct ec_completed *completed);

enum ec_completed_filter_flags {
	EC_MATCH = 1,
	EC_NO_MATCH = 2,
};

unsigned int ec_completed_count(
	const struct ec_completed *completed,
	enum ec_completed_filter_flags flags);

struct ec_completed_iter {
	enum ec_completed_filter_flags flags;
	const struct ec_completed *completed;
	const struct ec_completed_elt *cur;
};

struct ec_completed_iter *
ec_completed_iter(struct ec_completed *completed,
	enum ec_completed_filter_flags flags);

const struct ec_completed_elt *ec_completed_iter_next(
	struct ec_completed_iter *iter);

void ec_completed_iter_free(struct ec_completed_iter *iter);


#endif
