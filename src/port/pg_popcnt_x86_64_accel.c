/*-------------------------------------------------------------------------
 *
 * pg_popcnt_x86_64_accel.c
 *	  Fast POPCNT methods for x86_64.
 *
 * Copyright (c) 2019-2024, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *	  src/port/pg_popcnt_x86_64_accel.c
 *
 *-------------------------------------------------------------------------
 */
#include "c.h"

#include "port/pg_bitutils.h"

extern int pg_popcount32_fast(uint32 word);
extern int pg_popcount64_fast(uint64 word);

#ifdef TRY_POPCNT_FAST
/*
 * pg_popcount32_fast
 *		Return the number of 1 bits set in word
 */
int
pg_popcount32_fast(uint32 word)
{
#ifdef _MSC_VER
	return __popcnt(word);
#else
	uint32		res;

__asm__ __volatile__(" popcntl %1,%0\n":"=q"(res):"rm"(word):"cc");
	return (int) res;
#endif
}

/*
 * pg_popcount64_fast
 *		Return the number of 1 bits set in word
 */
int
pg_popcount64_fast(uint64 word)
{
#ifdef _MSC_VER
	return __popcnt64(word);
#else
	uint64		res;

__asm__ __volatile__(" popcntq %1,%0\n":"=q"(res):"rm"(word):"cc");
	return (int) res;
#endif
}

#endif							/* TRY_POPCNT_FAST */
