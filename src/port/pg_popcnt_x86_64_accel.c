/*-------------------------------------------------------------------------
 *
 * pg_popcnt_x86_64_accel.c
 *	  Miscellaneous functions for bit-wise operations.
 *
 * Copyright (c) 2024, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *	  src/port/pg_popcnt_x86_64_accel.c
 *
 *-------------------------------------------------------------------------
 */
#include "c.h"

#if defined(HAVE__IMMINTRIN)
#include <immintrin.h>
#endif

#include "port/pg_bitutils.h"

#ifdef _MSC_VER
#define __asm__ __asm
#endif

#ifdef TRY_POPCNT_FAST
extern const uint8 pg_number_of_ones[256];
extern uint64 pg_popcount_slow(const char *buf, int bytes);
uint64 pg_popcount512_fast(const char *buf, int bytes);
int pg_popcount32_fast(uint32 word);
int pg_popcount64_fast(uint64 word);

/*
 * pg_popcount32_fast
 *		Return the number of 1 bits set in word
 */
int pg_popcount32_fast(uint32 word)
{
#ifdef _MSC_VER
    return __popcnt(word);
#else
    uint32 res;
    
    __asm__ __volatile__(" popcntl %1,%0\n" : "=q"(res) : "rm"(word) : "cc");
    return (int)res;
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
    uint64 res;

    __asm__ __volatile__(" popcntq %1,%0\n" : "=q"(res) : "rm"(word) : "cc");
    return (int)res;
#endif
}

/*
 * Use AVX-512 Intrinsics for supported Intel CPUs or fall back the the software
 * loop in pg_bunutils.c and use the best 32 or 64 bit fast methods. If no fast
 * methods are used this will fall back to __builtin_* or pure software.
 */
uint64
pg_popcount512_fast(const char *buf, int bytes)
{
#if defined(HAVE__IMMINTRIN) && USE_AVX512_POPCNT_WITH_RUNTIME_CHECK == 1
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

    /* Process any remaining bytes */
    while (bytes--)
        popcnt += pg_number_of_ones[(unsigned char)*buf++];
    return popcnt;
#else
    return pg_popcount_slow(buf, bytes);
#endif /* USE_AVX512_CODE */
}
#endif                              /* TRY_POPCNT_FAST */
