/*-------------------------------------------------------------------------
 *
 * pg_popcount_x86_64_choose.c
 *	  Miscellaneous functions for bit-wise operations.
 *
 * Copyright (c) 2019-2024, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *	  src/port/pg_popcount_x86_64_choose.c
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
static int	pg_popcount32_choose(uint32 word);
static int	pg_popcount64_choose(uint64 word);
static uint64 pg_popcount_choose(const char *buf, int bytes);
extern int pg_popcount32_fast(uint32 word);
extern int pg_popcount64_fast(uint64 word);
extern uint64 pg_popcount_fast(const char *buf, int bytes);
extern int pg_popcount32_slow(uint32 word);
extern int pg_popcount64_slow(uint64 word);
extern uint64 pg_popcount_slow(const char *buf, int bytes);

int			(*pg_popcount32) (uint32 word) = pg_popcount32_choose;
int			(*pg_popcount64) (uint64 word) = pg_popcount64_choose;
uint64		(*pg_popcount) (const char *buf, int bytes) = pg_popcount_choose;

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
static inline void set_function_pointers()
{
	if (pg_popcount_available())
	{
		pg_popcount32 = pg_popcount32_fast;
		pg_popcount64 = pg_popcount64_fast;
		pg_popcount = pg_popcount_fast;
	}
	else
	{
		pg_popcount32 = pg_popcount32_slow;
		pg_popcount64 = pg_popcount64_slow;
		pg_popcount = pg_popcount_slow;
	}
}

static inline int
pg_popcount32_choose(uint32 word)
{
	set_function_pointers();
	return pg_popcount32(word);
}

static inline int
pg_popcount64_choose(uint64 word)
{
	set_function_pointers();
	return pg_popcount64(word);
}

static inline uint64
pg_popcount_choose(const char *buf, int bytes)
{
	set_function_pointers();
	return pg_popcount(buf, bytes);
}

#endif							/* TRY_POPCNT_FAST */
