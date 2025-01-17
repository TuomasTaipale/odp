/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023-2025 Nokia
 */

/**
 * @file
 *
 * Common helper macros
 */

#ifndef ODPH_MACROS_H_
#define ODPH_MACROS_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup odph_macros ODPH MACROS
 * Helper macros
 *
 * @{
 */

/**
 * Return number of elements in array
 */
#define ODPH_ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

/**
 * Return minimum of two numbers
 */
#define ODPH_MIN(a, b)				\
	__extension__ ({			\
		__typeof__(a) min_a = (a);	\
		__typeof__(b) min_b = (b);	\
		min_a < min_b ? min_a : min_b;	\
	})

/**
 * Return maximum of two numbers
 */
#define ODPH_MAX(a, b)				\
	__extension__ ({			\
		__typeof__(a) max_a = (a);	\
		__typeof__(b) max_b = (b);	\
		max_a > max_b ? max_a : max_b;	\
	})

/**
 * Return absolute value of a variable
 */
#define ODPH_ABS(v)				\
	__extension__ ({			\
		__typeof__(v) abs_v = (v);	\
		abs_v > 0 ? abs_v : -abs_v;	\
	})

/**
 * Return division of two numbers rounded up to the closest integer
 */
#define ODPH_ROUNDUP_DIV(numer, denom)			\
	__extension__ ({				\
		__typeof__(numer) n = (numer);		\
		__typeof__(denom) d = (denom);		\
		((n) > 0) != ((d) > 0) ?		\
			(n) / (d) :			\
			((n) + ((d) > 0 ?		\
				(d) - 1 :		\
				(d) + 1)) / (d);	\
	})

/**
 * Round up 'x' to alignment 'align'
 */
#define ODPH_ROUNDUP_ALIGN(x, align) ((ODPH_ABS(align)) * (ODPH_ROUNDUP_DIV((x), (align))))

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* ODPH_MACROS_H_ */
