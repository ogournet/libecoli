/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2016, Olivier MATZ <zer0@droids-corp.org>
 */

/**
 * Register initialization routines.
 */

#ifndef ECOLI_INIT_
#define ECOLI_INIT_

#include <sys/queue.h>

#include <ecoli_log.h>
#include <ecoli_node.h>

#define EC_INIT_REGISTER(t)						\
	static void ec_init_init_##t(void);				\
	static void __attribute__((constructor, used))			\
	ec_init_init_##t(void)						\
	{								\
		 ec_init_register(&t);					\
	}

/**
 * Type of init function. Return 0 on success, -1 on error.
 */
typedef int (ec_init_t)(void);

TAILQ_HEAD(ec_init_list, ec_init);

/**
 * A structure describing a test case.
 */
struct ec_init {
	TAILQ_ENTRY(ec_init) next;  /**< Next in list. */
	ec_init_t *init;            /**< Init function. */
	unsigned int priority;      /**< Priority (0=first, 99=last) */
};

/**
 * Register an initialization function.
 *
 * @param init
 *   A pointer to a ec_init structure to be registered.
 */
void ec_init_register(struct ec_init *test);

/**
 * Initialize ecoli library
 *
 * Must be called before any other function from libecoli, except
 * ec_malloc_register().
 *
 * @return
 *   0 on success, -1 on error (errno is set).
 */
int ec_init(void);

#endif
