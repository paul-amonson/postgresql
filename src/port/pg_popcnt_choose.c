/*-------------------------------------------------------------------------
 *
 * pg_popcnt_choose.c
 *	  For FAST operations, these methods do runtime checks and set the
 *    appropriate function pointers.
 *
 * Copyright (c) 2019-2024, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *	  src/port/pg_popcnt_choose.c
 *
 *-------------------------------------------------------------------------
 */
#include "c.h"

#ifdef HAVE__GET_CPUID
#include <cpuid.h>
#endif
#ifdef HAVE__CPUID
#include <intrin.h>
#endif

#include "port/pg_bitutils.h"


/* In pg_bitutils.c file */
extern int	pg_popcount32_slow(uint32 word);
extern int	pg_popcount64_slow(uint64 word);

#ifdef TRY_POPCNT_FAST
static bool pg_popcount_available(void);
static int	pg_popcount32_choose(uint32 word);
static int	pg_popcount64_choose(uint64 word);

/* In pg_popcnt_*_accel source file. */
extern int	pg_popcount32_fast(uint32 word);
extern int	pg_popcount64_fast(uint64 word);

int			(*pg_popcount32) (uint32 word) = pg_popcount32_choose;
int			(*pg_popcount64) (uint64 word) = pg_popcount64_choose;
#endif							/* TRY_POPCNT_FAST */

#ifdef TRY_POPCNT_FAST

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

	return (exx[2] & (1 << 23)) != 0;	/* POPCNT */
}

/*
 * These functions get called on the first call to pg_popcount32 etc.
 * They detect whether we can use the asm implementations, and replace
 * the function pointers so that subsequent calls are routed directly to
 * the chosen implementation.
 */
static int
pg_popcount32_choose(uint32 word)
{
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

	return pg_popcount32(word);
}

static int
pg_popcount64_choose(uint64 word)
{
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

	return pg_popcount64(word);
}
#endif							/* TRY_POPCNT_FAST */
