/* Copyright (c) 2015-2018, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

/**
 * @file
 *
 * ODP debug
 */

#ifndef ODP_ABI_DEBUG_H_
#define ODP_ABI_DEBUG_H_

#include <odp/autoheader_external.h>
#include <odp/api/init.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @internal Compile time assertion macro. Fails compilation and outputs 'msg'
 * if condition 'cond' is false. Macro definition is empty when compiler is not
 * supported or the compiler does not support static assertion.
 */
#ifndef __cplusplus
#define ODP_STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)
#else
#define ODP_STATIC_ASSERT(cond, msg) static_assert(cond, msg)
#endif

/**
 * @def ODP_DEBUG
 * Defines inclusion of additional debug code
 */
#ifndef ODP_DEBUG
#define ODP_DEBUG 0
#endif

/**
 * @internal Runtime assertion-macro, calls configured ODP abort function if
 * 'cond' is false.
 */
#define ODP_ASSERT(cond) \
	do { if ((ODP_DEBUG == 1) && (!(cond))) { \
		odp_log_func_t log_fn = odp_get_log_fn(); \
		odp_abort_func_t abort_fn = odp_get_abort_fn(); \
		\
		if (log_fn != NULL) \
			log_fn(ODP_LOG_ERR, "%s\n", #cond); \
		\
		if (abort_fn != NULL) \
			abort_fn(); } \
	} while (0)

#ifdef __cplusplus
}
#endif

#endif
