/*-------------------------------------------------------------------------
 *
 * pg_hw_feat_check.c
 *    Test for hardware features at runtime on x86_64 platforms.
 *
 * Copyright (c) 2024, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *    src/port/pg_hw_feat_check.c
 *
 *-------------------------------------------------------------------------
 */
#include "c.h"

#if defined(__x86_64__)

#if defined(HAVE__GET_CPUID) || defined(HAVE__GET_CPUID_COUNT)
#include <cpuid.h>
#endif

#include <immintrin.h>

#if defined(HAVE__CPUID) || defined(HAVE__CPUIDEX)
#include <intrin.h>
#endif

#include "port/pg_hw_feat_check.h"

/* Define names for EXX registers to avoid hard to see bugs in code below. */
typedef unsigned int exx_t;
typedef enum
{
    EAX = 0,
    EBX = 1,
    ECX = 2,
    EDX = 3
} reg_name;

/*
 * Helper function.
 * Test for a bit being set in a exx_t register.
 */
inline bool is_bit_set_in_exx(exx_t* regs, reg_name ex, int bit)
{
    return ((regs[ex] & (1 << bit)) != 0);
}

/*
 * x86_64 Platform CPUID check for Linux and Visual Studio platforms.
 */
inline static void
pg_getcpuid(unsigned int leaf, exx_t *exx)
{
#if defined(HAVE__GET_CPUID)
    __get_cpuid(leaf, &exx[0], &exx[1], &exx[2], &exx[3]);
#elif defined(HAVE__CPUID)
    __cpuid(exx, 1);
#else
#error cpuid instruction not available
#endif
}

/*
 * x86_64 Platform CPUIDEX check for Linux and Visual Studio platforms.
 */
inline static void
pg_getcpuidex(unsigned int leaf, unsigned int subleaf, exx_t *exx)
{
#if defined(HAVE__GET_CPUID_COUNT)
    __get_cpuid_count(leaf, subleaf, &exx[0], &exx[1], &exx[2], &exx[3]);
#elif defined(HAVE__CPUIDEX)
    __cpuidex(exx, 7, 0);
#else
#error cpuid instruction not available
#endif
}

/*
 * x86_64 Platform CPUIDEX_COUNT check for Linux and Visual Studio platforms.
 */
inline static void
pg_getcpuid_count(unsigned int leaf, exx_t *exx)
{
#if defined(HAVE__GET_CPUID_COUNT)
    __get_cpuid_count(leaf, 0, &exx[0], &exx[1], &exx[2], &exx[3]);
#elif defined(HAVE__CPUIDEX)
    __cpuidex(exx, leaf, 0);
#else
#error cpuid instruction not available
#endif
}

/*
 * Check for CPU support for CPUID: osxsave
 */
inline static bool
osxsave_available(void)
{
#if defined(HAVE_XSAVE_INTRINSICS)
    exx_t exx[4] = {0, 0, 0, 0};

    pg_getcpuid(1, exx);

    return is_bit_set_in_exx(exx, ECX, 27); /* osxsave */
#else
    return false;
#endif
}

/*
 * Does XGETBV say the ZMM registers are enabled?
 *
 * NB: Caller is responsible for verifying that osxsave_available() returns true
 * before calling this.
 */
inline static bool
zmm_regs_available(void)
{
    return (_xgetbv(0) & 0xe6) == 0xe6;
}

/*
 * Does CPUID say there's support for AVX-512 popcount and byte-and-word
 * instructions?
 */
inline static bool
avx512_popcnt_available(void)
{
    exx_t exx[4] = {0, 0, 0, 0};

    pg_getcpuid_count(7, exx);

    return is_bit_set_in_exx(exx, ECX, 14) && is_bit_set_in_exx(exx, EBX, 30);
}

/*
 * Return true if CPUID indicates that the POPCNT instruction is available.
 */
inline bool pg_popcount_available(void)
{
    exx_t exx[4] = {0, 0, 0, 0};

    pg_getcpuid(1, exx);

    return is_bit_set_in_exx(exx, ECX, 23);
 }

 /*
  * Returns true if the CPU supports the instructions required for the AVX-512
  * pg_popcount() implementation.
  *
  * PA: The call to 'osxsave_available' MUST preceed the call to
  *     'zmm_regs_available' function per NB above.
  */
 inline bool pg_popcount_avx512_available(void)
 {
     return osxsave_available() &&
            zmm_regs_available() &&
            avx512_popcnt_available();
}

/*
 * Does CPUID say there's support for SSE 4.2?
 */
inline bool pg_crc32c_sse42_available(void)
{
    exx_t exx[4] = {0, 0, 0, 0};

    pg_getcpuid(1, exx);

    return is_bit_set_in_exx(exx, ECX, 20);
}

#endif                              /* __x86_64__ */