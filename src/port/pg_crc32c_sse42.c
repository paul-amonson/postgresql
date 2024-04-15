/*-------------------------------------------------------------------------
 *
 * pg_crc32c_sse42.c
 *	  Compute CRC-32C checksum using Intel SSE 4.2 instructions.
 *
 * Portions Copyright (c) 1996-2024, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 * Portions Copyright (c) 2024, Intel(r) Corporation
 * 
 * IDENTIFICATION
 *	  src/port/pg_crc32c_sse42.c
 *
 *-------------------------------------------------------------------------
 */

#define USE_AVX

#include "c.h"

#include <nmmintrin.h>

#include "port/pg_crc32c.h"

#ifdef USE_AVX
#include <immintrin.h>
#include <smmintrin.h>
#include <wmmintrin.h>
#include <emmintrin.h>
#endif

//#define CRC_DEBUG
#if defined(CRC_DEBUG)
#include <stdio.h>
void dump(const char *prefix, size_t len);
void dump_hex(const char *prefix, size_t len);
void dump(const char *prefix, size_t len) {
	static const char* format="%s: %ld\n";
	FILE* fd = fopen("/home/paul/postgresql-fork/postgres.out", "a");
	fprintf(fd, format, prefix, len);
	fclose(fd);
}
void dump_hex(const char *prefix, size_t len) {
	static const char* format="%s: 0x%016lx\n";
	FILE* fd = fopen("/home/paul/postgresql-fork/postgres.out", "a");
	fprintf(fd, format, prefix, len);
	fclose(fd);
}
#else
#define dump(x,y)
#define dump_hex(x,y)
#endif

#ifndef USE_AVX
pg_attribute_no_sanitize_alignment()
inline 
pg_crc32c
pg_comp_crc32c_sse42(pg_crc32c crc, const void *data, size_t len)
{
	const unsigned char *p = data;
	const unsigned char *pend = p + len;

	/*
	 * Process eight bytes of data at a time.
	 *
	 * NB: We do unaligned accesses here. The Intel architecture allows that,
	 * and performance testing didn't show any performance gain from aligning
	 * the begin address.
	 */
#ifdef __x86_64__
	while (p + 8 <= pend)
	{
		crc = (uint32) _mm_crc32_u64(crc, *((const uint64 *) p));
		p += 8;
	}

	/* Process remaining full four bytes if any */
	if (p + 4 <= pend)
	{
		crc = _mm_crc32_u32(crc, *((const unsigned int *) p));
		p += 4;
	}
#else

	/*
	 * Process four bytes at a time. (The eight byte instruction is not
	 * available on the 32-bit x86 architecture).
	 */
	while (p + 4 <= pend)
	{
		crc = _mm_crc32_u32(crc, *((const unsigned int *) p));
		p += 4;
	}
#endif							/* __x86_64__ */

	/* Process any remaining bytes one at a time. */
	while (p < pend)
	{
		crc = _mm_crc32_u8(crc, *p);
		p++;
	}

	return crc;
}
#endif

#ifdef USE_AVX
/*
 * Process eight bytes of data at a time.
 *
 * NB: We do unaligned accesses here. The Intel architecture allows that,
 * and performance testing didn't show any performance gain from aligning
 * the begin address.
 */
pg_attribute_no_sanitize_alignment()
static inline
pg_crc32c
crc32c_fallback(pg_crc32c crc, const uint8 *input, size_t length)
{
	const unsigned char *pend = input + length;
	while (input + 8 <= pend)
	{
		crc = (uint32) _mm_crc32_u64(crc, *((const uint64 *) input));
		input += 8;
	}

	/* Process any remaining bytes one at a time. */
	while (input < pend)
	{
		crc = _mm_crc32_u8(crc, *(input++));
	}
	return crc;
}

/*
 * pg_crc32c_avx512(): compute the crc32c of the buffer, where the buffer
 * length must be at least 256, and a multiple of 64. Based on:
 *
 * "Fast CRC Computation for Generic Polynomials Using PCLMULQDQ Instruction"
 *  V. Gopal, E. Ozturk, et al., 2009,
 *  https://www.researchgate.net/publication/263424619_Fast_CRC_computation#full-text
 * 
 * This Function:
 * Copyright (c) 2024, Intel(r) Corporation
  * SPDX: BSD-3-Clause
 */
#define AVX_ALIGN 32
pg_attribute_no_sanitize_alignment()
inline
pg_crc32c
pg_comp_crc32c_sse42(pg_crc32c crc, const void *data, size_t length)
{
	const uint8 *input = (const uint8 *)data;

	dump("Request Length", length);
	if (length >= 288) {
		static const uint64 k1k2[8] = {
			0xdcb17aa4, 0xb9e02b86, 0xdcb17aa4, 0xb9e02b86, 0xdcb17aa4,
			0xb9e02b86, 0xdcb17aa4, 0xb9e02b86};
		static const uint64 k3k4[8] = {
			0x740eef02, 0x9e4addf8, 0x740eef02, 0x9e4addf8, 0x740eef02,
			0x9e4addf8, 0x740eef02, 0x9e4addf8};
		static const uint64 k9k10[8] = {
			0x6992cea2, 0x0d3b6092, 0x6992cea2, 0x0d3b6092, 0x6992cea2,
			0x0d3b6092, 0x6992cea2, 0x0d3b6092};
		static const uint64 k1k4[8] =  {
			0x1c291d04, 0xddc0152b, 0x3da6d0cb, 0xba4fc28e, 0xf20c0dfe,
			0x493c7d27, 0x00000000, 0x00000000};

		uint64 val;

		__m512i x0, x1, x2, x3, x4, x5, x6, x7, x8, y5, y6, y7, y8;
		__m128i a1, a2;
		
		/* Leading bytes need alignment? */
		if ((uint64)input % AVX_ALIGN != 0)
		{
			size_t slop = AVX_ALIGN - ((uint64)input % AVX_ALIGN);
			dump("Leading Length", slop);
			dump_hex("Raw Address", (size_t)input);
			crc = crc32c_fallback(crc, input, slop);
			input += slop;
			length -= slop;
		}
		dump("Post-Leading Length", length);
		dump_hex("Aligned Address", (size_t)input);

		/*
		* AVX-512 Optimized crc32c algorithm with mimimum of 256 bytes aligned
		* to 32 bytes.
		* >>> BEGIN
		*/
		// if (length >= 256)
		// {
			dump("Using AVX Code on Length", length);
			/*
			* There's at least one block of 256.
			*/
			x1 = _mm512_loadu_si512((__m512i *)(input + 0x00));
			x2 = _mm512_loadu_si512((__m512i *)(input + 0x40));
			x3 = _mm512_loadu_si512((__m512i *)(input + 0x80));
			x4 = _mm512_loadu_si512((__m512i *)(input + 0xC0));

			x1 = _mm512_xor_si512(x1, _mm512_castsi128_si512(_mm_cvtsi32_si128(crc)));

			x0 = _mm512_load_si512((__m512i *)k1k2);

			input += 256;
			length -= 256;

			/*
			* Parallel fold blocks of 256, if any.
			*/
			while (length >= 256) {
				x5 = _mm512_clmulepi64_epi128(x1, x0, 0x00);
				x6 = _mm512_clmulepi64_epi128(x2, x0, 0x00);
				x7 = _mm512_clmulepi64_epi128(x3, x0, 0x00);
				x8 = _mm512_clmulepi64_epi128(x4, x0, 0x00);
			
				x1 = _mm512_clmulepi64_epi128(x1, x0, 0x11);
				x2 = _mm512_clmulepi64_epi128(x2, x0, 0x11);
				x3 = _mm512_clmulepi64_epi128(x3, x0, 0x11);
				x4 = _mm512_clmulepi64_epi128(x4, x0, 0x11);
			
				y5 = _mm512_loadu_si512((__m512i *)(input + 0x00));
				y6 = _mm512_loadu_si512((__m512i *)(input + 0x40));
				y7 = _mm512_loadu_si512((__m512i *)(input + 0x80));
				y8 = _mm512_loadu_si512((__m512i *)(input + 0xC0));

				x1 = _mm512_ternarylogic_epi64(x1, x5, y5, 0x96);
				x2 = _mm512_ternarylogic_epi64(x2, x6, y6, 0x96);
				x3 = _mm512_ternarylogic_epi64(x3, x7, y7, 0x96);
				x4 = _mm512_ternarylogic_epi64(x4, x8, y8, 0x96);

				input += 256;
				length -= 256;
			}

			/*
			* Fold 256 bytes into 64 bytes.
			*/
			x0 = _mm512_load_si512((__m512i *)k9k10);
			x5 = _mm512_clmulepi64_epi128(x1, x0, 0x00);
			x6 = _mm512_clmulepi64_epi128(x1, x0, 0x11);
			x3 = _mm512_ternarylogic_epi64(x3, x5, x6, 0x96);

			x7 = _mm512_clmulepi64_epi128(x2, x0, 0x00);
			x8 = _mm512_clmulepi64_epi128(x2, x0, 0x11);
			x4 = _mm512_ternarylogic_epi64(x4, x7, x8, 0x96);

			x0 = _mm512_load_si512((__m512i *)k3k4);
			y5 = _mm512_clmulepi64_epi128(x3, x0, 0x00);
			y6 = _mm512_clmulepi64_epi128(x3, x0, 0x11);
			x1 = _mm512_ternarylogic_epi64(x4, y5, y6, 0x96);

			/*
			* Single fold blocks of 64, if any.
			*/
			while (length >= 64) {
				x2 = _mm512_loadu_si512((__m512i *)input);

				x5 = _mm512_clmulepi64_epi128(x1, x0, 0x00);
				x1 = _mm512_clmulepi64_epi128(x1, x0, 0x11);
				x1 = _mm512_ternarylogic_epi64(x1, x2, x5, 0x96);

				input += 64;
				length -= 64;
			}

			/*
			* Fold 512-bits to 128-bits.
			*/
			x0 = _mm512_loadu_si512((__m512i *)k1k4);

			a2 = _mm512_extracti32x4_epi32(x1, 3);
			x5 = _mm512_clmulepi64_epi128(x1, x0, 0x00);
			x1 = _mm512_clmulepi64_epi128(x1, x0, 0x11);
			x1 = _mm512_ternarylogic_epi64(x1, x5, _mm512_castsi128_si512(a2), 0x96);

			x0 = _mm512_shuffle_i64x2(x1, x1, 0x4E);
			x0 = _mm512_xor_epi64(x1, x0);
			a1 = _mm512_extracti32x4_epi32(x0, 1);
			a1 = _mm_xor_epi64(a1, _mm512_castsi512_si128(x0));

			/*
			* Fold 128-bits to 32-bits.
			*/
			val = _mm_crc32_u64(0, _mm_extract_epi64(a1, 0));
			crc = (uint32_t) _mm_crc32_u64(val, _mm_extract_epi64(a1, 1));
		// }
		/*
		* AVX-512 Optimized crc32c algorithm with mimimum of 256 bytes aligned
		* to 32 bytes. 
		* <<< END
		*/
	}

	/*
	 * Finish any remaining bytes.
	 */
	dump("Remaining Length", length);
	return crc32c_fallback(crc, input, length);
}
#endif
