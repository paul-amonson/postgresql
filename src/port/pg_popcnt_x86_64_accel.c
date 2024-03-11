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

#if defined(HAVE__IMMINTRIN)
#include <immintrin.h>
#endif

extern int pg_popcount32_fast(uint32 word);
extern int pg_popcount64_fast(uint64 word);
extern uint64 pg_popcount_fast(const char *buf, int bytes);
extern uint64 pg_popcount_slow(const char *buf, int bytes);

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

/*
 * Use AVX-512 Intrinsics for supported Intel CPUs or fall back the the software
 * loop in pg_bunutils.c and use the best 32 or 64 bit fast methods. If no fast
 * methods are used this will fall back to __builtin_* or pure software.
 */
uint64
pg_popcount_fast(const char *buf, int bytes)
{
#if defined(HAVE__IMMINTRIN) && HAVE__AVX512_POPCNT == 1
    uint64 popcnt = 0;
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
    return popcnt + pg_popcount_slow(buf, bytes);
}
#endif                              /* TRY_POPCNT_FAST */
