/*-------------------------------------------------------------------------
 *
 * pg_popcount_x86_64_accel.c
 *	  Miscellaneous functions for bit-wise operations.
 *
 * Copyright (c) 2019-2024, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *	  src/port/pg_popcount_x86_64_accel.c
 *
 *-------------------------------------------------------------------------
 */
#include "c.h"
#include "port/pg_bitutils.h"

#if defined(HAVE__IMMINTRIN)
#include <immintrin.h>
#endif

#ifdef TRY_POPCNT_FAST
int pg_popcount32_fast(uint32 word);
int pg_popcount64_fast(uint64 word);
uint64 pg_popcount_fast(const char *buf, int bytes);
uint64 pg_popcount512_fast(const char *buf, int bytes);

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
			popcnt += pg_popcount64_fast(*words++);
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
			popcnt += pg_popcount32_fast(*words++);
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

/*
 * Use AVX-512 Intrinsics for supported CPUs or fall back the non-152 fast
 * implem entation and use the best 64 bit fast methods. If no fast
 * methods are used this will fall back to __builtin_* or pure software.
 */
uint64
pg_popcount512_fast(const char *buf, int bytes)
{
	uint64 popcnt = 0;
 #if defined(HAVE__IMMINTRIN) && HAVE__AVX512_POPCNT == 1
	__m512i accumulator = _mm512_setzero_si512();

	while (bytes >= 64)
	{
		const __m512i v = _mm512_loadu_si512((const __m512i *)buf);
		const __m512i p = _mm512_popcnt_epi64(v);

		accumulator = _mm512_add_epi64(accumulator, p);
		bytes -= 64;
		buf += 64;
	}

	popcnt = _mm512_reduce_add_epi64(accumulator);
#endif 				/* defined(HAVE__IMMINTRIN) && HAVE__AVX512_POPCNT == 1 */

	/* Process any remaining bytes */
	return popcnt + pg_popcount_fast(buf, bytes);
}
#endif							/* TRY_POPCNT_FAST */
