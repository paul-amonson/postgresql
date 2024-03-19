/*-------------------------------------------------------------------------
 *
 * pg_popcount_x86_64_accel.c
 *	  Miscellaneous functions for bit-wise operations.
 *
 * Copyright (c) 2024, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *	  src/port/pg_popcount_x86_64_accel.c
 *
 *-------------------------------------------------------------------------
 */
#include "c.h"
#include "port/pg_bitutils.h"

#ifdef TRY_POPCNT_FAST
int pg_popcount32_fast(uint32 word);
int pg_popcount64_fast(uint64 word);
uint64 pg_popcount_fast(const char *buf, int bytes);

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

/*
 * pg_popcount_fast
 *		Returns the number of 1-bits in buf
 */
uint64
pg_popcount_fast(const char *buf, int bytes)
{
	uint64		popcnt = 0;

#if SIZEOF_VOID_P >= 8
	/* Process in 64-bit chunks if the buffer is aligned. */
	if (buf == (const char *) TYPEALIGN(8, buf))
	{
		const uint64 *words = (const uint64 *) buf;

		while (bytes >= 8)
		{
			popcnt += PG_POPCOUNT64(*words++);
			bytes -= 8;
		}

		buf = (const char *) words;
	}
#else
	/* Process in 32-bit chunks if the buffer is aligned. */
	if (buf == (const char *) TYPEALIGN(4, buf))
	{
		const uint32 *words = (const uint32 *) buf;

		while (bytes >= 4)
		{
			popcnt += PG_POPCOUNT32(*words++);
			bytes -= 4;
		}

		buf = (const char *) words;
	}
#endif

	/* Process any remaining bytes */
	while (bytes--)
		popcnt += pg_number_of_ones[(unsigned char) *buf++];

	return popcnt;
}

#endif							/* TRY_POPCNT_FAST */
