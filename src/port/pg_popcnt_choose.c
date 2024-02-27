/*-------------------------------------------------------------------------
 *
 * pg_popcnt_x86_64_choose.c
 *	  Miscellaneous functions for bit-wise operations.
 *
 * Copyright (c) 2024, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *	  src/port/pg_popcnt_x86_64_choose.c
 *
 *-------------------------------------------------------------------------
 */
#include "c.h"

#include "port/pg_bitutils.h"

#ifdef TRY_POPCNT_FAST

#ifdef HAVE__GET_CPUID
#include <cpuid.h>
#endif

#ifdef HAVE__CPUID
#include <intrin.h>
#endif

static bool pg_popcount_available(void);
int pg_popcount32_choose(uint32 word);
int pg_popcount64_choose(uint64 word);
uint64 pg_popcount_choose(const char *buf, int bytes);

extern int pg_popcount32_fast(uint32 word);
extern int pg_popcount64_fast(uint64 word);
extern int pg_popcount32_slow(uint32 word);
extern int pg_popcount64_slow(uint64 word);
extern uint64 pg_popcount512_fast(const char *buf, int bytes);
extern uint64 pg_popcount_slow(const char *buf, int bytes);
extern uint64 (*pg_popcount_indirect)(const char *buf, int bytes);

extern int (*pg_popcount32)(uint32 word);
extern int (*pg_popcount64)(uint64 word);

/*
 * Return true if CPUID indicates that the POPCNT instruction is available.
 */
static bool
pg_popcount_available(void)
{
    unsigned int exx[4] = {0, 0, 0, 0};

#if defined(HAVE__GET_CPUID)
    __get_cpuid(1, &exx[0], &exx[1], &exx[2], &exx[3]);
#elif defined(HAVE__CPUID)
    __cpuid(exx, 1);
#else
#error cpuid instruction not available
#endif

    return (exx[2] & (1 << 23)) != 0; /* POPCNT */
}

/*
 * Return true if CPUID indicates that the AVX512_POPCNT instruction is
 * available. This is similar to the method above; see
 * https://en.wikipedia.org/wiki/CPUID#EAX=7,_ECX=0:_Extended_Features
 *
 * Finally, we make sure the xgetbv result is consistent with the CPUID
 * results.
 */
static bool
pg_popcount512_available(void)
{
    unsigned int exx[4] = {0, 0, 0, 0};

    /* Check for AVX512VPOPCNTDQ and AVX512F */
#if defined(HAVE__GET_CPUID_COUNT)
    __get_cpuid_count(7, 0, &exx[0], &exx[1], &exx[2], &exx[3]);
#elif defined(HAVE__CPUIDEX)
    __cpuidex(exx, 7, 0);
#endif

    if ((exx[2] & (0x00004000)) != 0 && (exx[1] & (0x00010000)) != 0)
    {
        /*
         * CPUID succeeded, does the current running OS support the
         * ZMM registers which are required for AVX512? This check is
         * required to make sure an old OS on a new CPU is correctly
         * checked or a VM hypervisor is not excluding AVX512 ZMM
         * support in the VM; see "5.1.9 Detection of AVX Instructions"
         * https://www.intel.com/content/www/us/en/content-details/671488/intel-64-and-ia-32-architectures-optimization-reference-manual-volume-1.html
         */
        uint64 xcr = 0;
#ifdef _MSC_VER
        uint64 highlow = _xgetbv(xcr);

        return (highlow & 0xE0) != 0;
#else
        uint32 high;
        uint32 low;
        
        __asm__ __volatile__("xgetbv\t\n" : "=a"(low), "=d"(high) : "c"(xcr));
        return (low & 0xE0) != 0;
#endif
    } /* POPCNT 512 */
    return false;
}

/*
 * These functions get called on the first call to pg_popcount32 etc.
 * They detect whether we can use the asm implementations, and replace
 * the function pointers so that subsequent calls are routed directly to
 * the chosen implementation.
 */
static void set_up_function_pointers()
{
    if (pg_popcount512_available())
    {
#if defined(_MSC_VER)
        pg_popcount_indirect = pg_popcount512_fast;
#else
        pg_popcount = pg_popcount512_fast;
#endif
    }
    else
    {
#if defined(_MSC_VER)
        pg_popcount_indirect = pg_popcount_slow;
#else
        pg_popcount = pg_popcount_slow;
#endif
    }
    if (pg_popcount_available())
    {
        pg_popcount32 = pg_popcount32_fast;
        pg_popcount64 = pg_popcount64_fast;
    }
    else
    {
        pg_popcount32 = pg_popcount32_slow;
        pg_popcount64 = pg_popcount64_slow;
    }
}

int pg_popcount32_choose(uint32 word)
{
    set_up_function_pointers();
    return pg_popcount32(word);
}

int
pg_popcount64_choose(uint64 word)
{
    set_up_function_pointers();
    return pg_popcount64(word);
}

uint64
pg_popcount_choose(const char *buf, int bytes)
{
    set_up_function_pointers();
#if defined(_MSC_VER)
    return pg_popcount_indirect(buf, bytes);
#else
    return pg_popcount(buf, bytes);
#endif
}

#endif                                          /* TRY_POPCNT_FAST */
