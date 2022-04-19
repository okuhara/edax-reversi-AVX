/**
 * @file bit.h
 *
 * Bitwise operations header file.
 *
 * @date 1998 - 2020
 * @author Richard Delorme
 * @version 4.4
 */

#ifndef EDAX_BIT_H
#define EDAX_BIT_H

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <stdbool.h>

#include "bit_intrinsics.h"

struct Random;

/* declaration */
void bit_init(void);
int bit_weighted_count(unsigned long long);
// int next_bit(unsigned long long*);
void bitboard_write(unsigned long long, FILE*);
unsigned long long transpose(unsigned long long);
unsigned int horizontal_mirror_32(unsigned int b);
unsigned long long horizontal_mirror(unsigned long long);
int get_rand_bit(unsigned long long, struct Random*);

extern unsigned long long X_TO_BIT[];
extern const unsigned long long NEIGHBOUR[];

/** Return a bitboard with bit x set. */
// https://eukaryote.hateblo.jp/entry/2020/04/12/054905
#if 1 // 1% slower on Sandy Bridge
#define x_to_bit(x) (1ULL << (x))
#else
#define x_to_bit(x) X_TO_BIT[x]
#endif

/** Loop over each bit set. */
#if (defined(__GNUC__) && __GNUC__ >= 4) || __has_builtin(__builtin_ctzll)
	#define	first_bit(x)	__builtin_ctzll(x)
	#define	last_bit(x)	(63 - __builtin_clzll(x))
#elif defined(tzcnt_u64)
	#define	first_bit(x)	tzcnt_u64(x)
	#define	last_bit(x)	(63 - lzcnt_u64(x))
#else
	int first_bit(unsigned long long);
	int last_bit(unsigned long long);
#endif

#define foreach_bit(i, b)	for (i = first_bit(b); b; i = first_bit(b &= (b - 1)))

#ifdef HAS_CPU_64
	typedef unsigned long long	widest_register;
	#define foreach_bit_r(i, f, b)	b = (widest_register) f;\
		foreach_bit(i, b)
#else
	typedef unsigned int	widest_register;
	#ifdef tzcnt_u32
		#define	first_bit_32(x)	tzcnt_u32(x)
	#else
		int first_bit_32(unsigned int);
	#endif
	#define foreach_bit_r(i, f, b)	b = (widest_register) f;\
		f >>= (sizeof(widest_register) % sizeof(f)) * CHAR_BIT;\
		for (i = first_bit_32(b); b; i = first_bit_32(b &= (b - 1)))
#endif

// popcount
#if !defined(POPCOUNT) && defined(hasNeon)
	#define	POPCOUNT	1
#endif

#ifdef POPCOUNT
	/*
	#if defined (USE_GAS_X64)
		static inline int bit_count (unsigned long long x) {
			long long	y;
			__asm__ ( "popcntq %1,%0" : "=r" (y) : "rm" (x));
			return y;
		}
	#elif defined (USE_GAS_X86)
		static inline int bit_count (unsigned long long x) {
			unsigned int	y0, y1;
			__asm__ ( "popcntl %2,%0\n\t"
				"popcntl %3,%1"
				: "=&r" (y0), "=&r" (y1)
				: "rm" ((unsigned int) x), "rm" ((unsigned int) (x >> 32)));
			return y0 + y1;
		}
	*/
	#ifdef _MSC_VER
		#if defined(_M_ARM) || defined(_M_ARM64)
			#define bit_count(x)	_CountOneBits64(x)
		#elif defined(_M_X64)
			#define	bit_count(x)	((int) __popcnt64(x))
		#else
			#define bit_count(x)	(__popcnt((unsigned int) (x)) + __popcnt((unsigned int) ((x) >> 32)))
		#endif
	#else
		#define bit_count(x)	__builtin_popcountll(x)
	#endif
#else
	extern unsigned char PopCnt16[1 << 16];
	static inline int bit_count(unsigned long long b) {
		union { unsigned long long bb; unsigned short u[4]; } v = { b };
		return (unsigned char)(PopCnt16[v.u[0]] + PopCnt16[v.u[1]] + PopCnt16[v.u[2]] + PopCnt16[v.u[3]]);
	}
#endif

#if defined(USE_GAS_MMX) || defined(USE_MSVC_X86)
	#ifndef hasSSE2
		extern bool	hasSSE2;
	#endif
	#ifndef hasMMX
		extern bool	hasMMX;
	#endif
#endif

#if defined(ANDROID) && ((defined(__arm__) && !defined(hasNeon)) || (defined(__i386__) && !defined(hasSSE2)))
extern bool	hasSSE2;
#endif

typedef union {
	unsigned long long	ull[2];
#if defined(hasSSE2) || defined(USE_MSVC_X86)
	__m128i	v2;
	__m128d	d2;
#endif
}
#if defined(__GNUC__) && !defined(hasSSE2)
__attribute__ ((aligned (16)))
#endif
V2DI;

#ifdef hasSSE2
typedef union {
	unsigned long long	ull[4];
	#ifdef __AVX2__
		__m256i	v4;
	#endif
	__m128i	v2[2];
} V4DI;
#endif

/* Define function attributes directive when available */

#if defined(_MSC_VER)	// including clang-win
#define	vectorcall	__vectorcall
#elif defined(__GNUC__) && !defined(__clang__) && defined(__i386__)
#define	vectorcall	__attribute__((sseregparm))
#elif 0 // defined(__GNUC__)	// erroreous result on pgo-build
#define	vectorcall	__attribute__((sysv_abi))
#else
#define	vectorcall
#endif

// X64 compatibility sims for X86
#ifndef HAS_CPU_64
#if defined(hasSSE2) || defined(USE_MSVC_X86)
static inline __m128i _mm_cvtsi64_si128(const unsigned long long x) {
	return _mm_unpacklo_epi32(_mm_cvtsi32_si128(x), _mm_cvtsi32_si128(x >> 32));
}
#endif

// Double casting (unsigned long long) (unsigned int) improves MSVC code
#ifdef __AVX2__
static inline unsigned long long _mm_cvtsi128_si64(__m128i x) {
	return ((unsigned long long) (unsigned int) _mm_extract_epi32(x, 1) << 32)
		| (unsigned int) _mm_cvtsi128_si32(x);
}
#elif defined(hasSSE2) || defined(USE_MSVC_X86)
static inline unsigned long long _mm_cvtsi128_si64(__m128i x) {
	return ((unsigned long long) (unsigned int) _mm_cvtsi128_si32(_mm_shuffle_epi32(x, 0xb1)) << 32)
		| (unsigned int) _mm_cvtsi128_si32(x);
}
#endif
#endif // !HAS_CPU_64

#endif // EDAX_BIT_H
