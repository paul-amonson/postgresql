/*-------------------------------------------------------------------------
 *
 * pg_crc32c_avx512_choose.c
 *	  Choose between Intel AVX-512 and software CRC-32C implementation.
 *
 * On first call, checks if the CPU we're running on supports Intel AVX-
 * 512. If it does, use the special AVX-512 instructions for CRC-32C
 * computation. Otherwise, fall back to the pure software implementation
 * (slicing-by-8).
 *
 * Portions Copyright (c) 1996-2024, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 * Portions Copyright (c) 2024, Intel(r) Corp.
 *
 *
 * IDENTIFICATION
 *	  src/port/pg_crc32c_avx512_choose.c
 *
 *-------------------------------------------------------------------------
 */

#include "c.h"

#if defined(HAVE__GET_CPUID) || defined(HAVE__GET_CPUID_COUNT)
#include <cpuid.h>
#endif

#ifdef HAVE_XSAVE_INTRINSICS
#include <immintrin.h>
#endif

#if defined(HAVE__CPUID) || defined(HAVE__CPUIDEX)
#include <intrin.h>
#endif

#include "port/pg_crc32c.h"

typedef unsigned int exx_t;

/*
 * Helper function.
 * Test for a bit being set in a exx_t field.
 */
inline
static
bool
is_bit_set(exx_t reg, int bit)
{
	return (reg & (1 << bit)) != 0;
}

/*
 * Intel Platform CPUID check for Linux and Visual Studio platforms.
 */
inline
static
void
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
 * Intel Platform CPUIDEX check for Linux and Visual Studio platforms.
 */
inline
static
void
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
 * Check for CPU supprt for CPUID: sse4.2
 */
inline
static
bool
sse42_available(void)
{
	exx_t exx[4] = {0, 0, 0, 0};

	pg_getcpuid(1, exx);
	return is_bit_set(exx[2], 20); /* sse4.2 */
}

/*
 * Check for CPU supprt for CPUID: osxsave
 */
inline
static
bool
osxsave_available(void)
{
	exx_t exx[4] = {0, 0, 0, 0};

	pg_getcpuid(1, exx);
	return is_bit_set(exx[2], 27); /* osxsave */
}

/*
 * Check for CPU supprt for CPUIDEX: avx512-f
 */
inline
static
bool
avx512f_available(void)
{
	exx_t exx[4] = {0, 0, 0, 0};

	pg_getcpuidex(7, 0, exx);
	return is_bit_set(exx[1], 16); /* avx512-f */
}

/*
 * Check for CPU supprt for CPUIDEX: vpclmulqdq
 */
inline
static
bool
vpclmulqdq_available(void)
{
	exx_t exx[4] = {0, 0, 0, 0};

	pg_getcpuidex(7, 0, exx);
	return is_bit_set(exx[1], 10); /* vpclmulqdq */
}

/*
 * Check for CPU supprt for CPUIDEX: vpclmulqdq
 */
inline
static
bool
avx512vl_available(void)
{
	exx_t exx[4] = {0, 0, 0, 0};

	pg_getcpuidex(7, 0, exx);
	return is_bit_set(exx[1], 31); /* avx512-vl */
}

/*
 * Returns true if the CPU supports the instructions required for the AVX-512
 * pg_crc32c implementation.
 */
inline
static
bool
pg_crc32c_avx512_available(void)
{
	return sse42_available() && osxsave_available() &&
		   avx512f_available() && vpclmulqdq_available() &&
		   avx512vl_available();
}

/*
 * This gets called on the first call. It replaces the function pointer
 * so that subsequent calls are routed directly to the chosen implementation.
 */
static
pg_crc32c
pg_comp_avx512_choose(pg_crc32c crc, const void *data, size_t len)
{
	if (pg_crc32c_avx512_available())
		pg_comp_crc32c = pg_comp_crc32c_avx512;
	else
		pg_comp_crc32c = pg_comp_crc32c_sb8;

	return pg_comp_crc32c(crc, data, len);
}

pg_crc32c	(*pg_comp_crc32c) (pg_crc32c crc, const void *data, size_t len) = pg_comp_avx512_choose;
