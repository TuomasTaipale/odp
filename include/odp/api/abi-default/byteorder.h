/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2015-2018 Linaro Limited
 */

/**
 * @file
 *
 * ODP byteorder
 */

#ifndef ODP_ABI_BYTEORDER_H_
#define ODP_ABI_BYTEORDER_H_

#include <odp/api/std_types.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __BYTE_ORDER__
#error __BYTE_ORDER__ not defined!
#endif

#ifndef __ORDER_BIG_ENDIAN__
#error __ORDER_BIG_ENDIAN__ not defined!
#endif

#ifndef __ORDER_LITTLE_ENDIAN__
#error __ORDER_LITTLE_ENDIAN__ not defined!
#endif

/** @addtogroup odp_compiler_optim
 *  @{
 */
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
	#define ODP_LITTLE_ENDIAN           1
	#define ODP_BIG_ENDIAN              0
	#define ODP_BYTE_ORDER              ODP_LITTLE_ENDIAN
	#define ODP_LITTLE_ENDIAN_BITFIELD  1
	#define ODP_BIG_ENDIAN_BITFIELD     0
	#define ODP_BITFIELD_ORDER          ODP_LITTLE_ENDIAN_BITFIELD
#else
	#define ODP_LITTLE_ENDIAN           0
	#define ODP_BIG_ENDIAN              1
	#define	ODP_BYTE_ORDER              ODP_BIG_ENDIAN
	#define ODP_LITTLE_ENDIAN_BITFIELD  0
	#define ODP_BIG_ENDIAN_BITFIELD     1
	#define ODP_BITFIELD_ORDER          ODP_BIG_ENDIAN_BITFIELD
#endif

typedef uint16_t odp_u16le_t;
typedef uint16_t odp_u16be_t;

typedef uint32_t odp_u32le_t;
typedef uint32_t odp_u32be_t;

typedef uint64_t odp_u64le_t;
typedef uint64_t odp_u64be_t;

typedef uint16_t odp_u16sum_t;
typedef uint32_t odp_u32sum_t;

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif
