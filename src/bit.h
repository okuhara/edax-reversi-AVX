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

extern const unsigned long long X_TO_BIT[];
/** Return a bitboard with bit x set. */
#define x_to_bit(x) X_TO_BIT[x]

//#define x_to_bit(x) (1ULL << (x)) // 1% slower on Sandy Bridge

#ifndef __has_builtin
	#define __has_builtin(x) 0  // Compatibility with non-clang compilers.
#endif

// mirror byte
#if defined(_M_ARM) // || defined(_M_ARM64) // https://developercommunity.visualstudio.com/content/problem/498995/arm64-missing-rbit-intrinsics.html
#define mirror_byte(b)	(_arm_rbit(b) >> 24)
#elif defined(__ARM_ACLE)
#include <arm_acle.h>
#define mirror_byte(b)	(__rbit(b) >> 24)
#elif defined(HAS_CPU_64)
// http://graphics.stanford.edu/~seander/bithacks.html
#define mirror_byte(b)	(unsigned char)((((b) * 0x80200802ULL) & 0x0884422110ULL) * 0x0101010101ULL >> 32)
#else
static inline unsigned char mirror_byte(unsigned int b) { return ((((b * 0x200802) & 0x4422110) + ((b << 7) & 0x880)) * 0x01010101 >> 24); }
#endif

// rotl8
#if __has_builtin(__builtin_rotateleft8)
	#define rotl8(x,y)	__builtin_rotateleft8((x),(y))
#elif defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 5)) && (defined(__x86_64__) || defined(__i386__))
	#define rotl8(x,y)	__builtin_ia32_rolqi((x),(y))
#elif defined(_MSC_VER)
	#define	rotl8(x,y)	_rotl8((x),(y))
#else	// may not compile into 8-bit rotate
	#define	rotl8(x,y)	((unsigned char)(((x)<<(y))|((unsigned)(x)>>(8-(y)))))
#endif

// bswap
#ifdef _MSC_VER
	#define	bswap_short(x)	_byteswap_ushort(x)
	#define	bswap_int(x)	_byteswap_ulong(x)
	#define	vertical_mirror(x)	_byteswap_uint64(x)
#else
	#if (defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 8))) || __has_builtin(__builtin_bswap16)
		#define	bswap_short(x)	__builtin_bswap16(x)
	#else
		#define bswap_short(x)	(((unsigned short) (x) >> 8) | ((unsigned short) (x) << 8))
	#endif
	#if (defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 3))) || __has_builtin(__builtin_bswap64)
		#define	bswap_int(x)	__builtin_bswap32(x)
		#define	vertical_mirror(x)	__builtin_bswap64(x)
	#else
		unsigned int bswap_int(unsigned int);
		unsigned long long vertical_mirror(unsigned long long);
	#endif
#endif

// ctz / clz
#if (defined(__GNUC__) && __GNUC__ >= 4) || __has_builtin(__builtin_ctzll)
	#define	first_bit(x)	__builtin_ctzll(x)
	#define	last_bit(x)	(63 - __builtin_clzll(x))
#elif defined(__AVX2__) && (defined(__x86_64__) || defined(_M_X64))
	#define	first_bit(x)	_tzcnt_u64(x)
	#define	last_bit(x)	(63 - _lzcnt_u64(x))
#else
	int first_bit(unsigned long long);
	int last_bit(unsigned long long);
#endif

/** Loop over each bit set. */
#define foreach_bit(i, b)	for (i = first_bit(b); b; i = first_bit(b &= (b - 1)))

#ifdef HAS_CPU_64
	typedef unsigned long long	widest_register;
	#define foreach_bit_r(i, f, b)	b = (widest_register) f;\
		foreach_bit(i, b)
#else
	typedef unsigned int	widest_register;
	#if (defined(__GNUC__) && __GNUC__ >= 4) || __has_builtin(__builtin_ctz)
		#define	first_bit_32(x)	__builtin_ctz(x)
	#elif defined(_M_ARM) || defined(_M_ARM64)
		#define first_bit_32(x) _arm_clz(_arm_rbit(x))
	#else
		int first_bit_32(unsigned int);
	#endif
	#define foreach_bit_r(i, f, b)	b = (widest_register) f;\
		f >>= sizeof(widest_register) * CHAR_BIT;\
		for (i = first_bit_32(b); b; i = first_bit_32(b &= (b - 1)))
#endif

// popcount
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
		#ifdef _M_ARM
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
		return PopCnt16[v.u[0]] + PopCnt16[v.u[1]] + PopCnt16[v.u[2]] + PopCnt16[v.u[3]];
	}
#endif

#if defined(__x86_64__) || defined(_M_X64) || defined(__AVX2__)
	#define hasSSE2	1
#endif

#ifdef _MSC_VER
	#include <intrin.h>
	#ifdef _M_IX86
		#define	USE_MSVC_X86	1
	#endif
#elif defined(hasSSE2)
	#include <x86intrin.h>
#endif

#ifdef hasSSE2
	#define	hasMMX	1
#endif

#if defined(USE_GAS_MMX) || defined(USE_MSVC_X86)
	#ifndef hasSSE2
		extern bool	hasSSE2;
	#endif
	#ifndef hasMMX
		extern bool	hasMMX;
	#endif
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
#elif defined(__GNUC__) && defined(__i386__)
#define	vectorcall	__attribute__((sseregparm))
#elif 0 // defined(__GNUC__)	// erroreous result on pgo-build
#define	vectorcall	__attribute__((sysv_abi))
#else
#define	vectorcall
#endif

// X64 compatibility sims for X86
#if !defined(__x86_64__) && !defined(_M_X64)
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
#endif

// lzcnt / tzcnt (0 allowed)
#ifdef USE_GAS_X86
#ifdef __LZCNT__
static inline int _lzcnt_u64(unsigned long long x) {
	int	y;
	__asm__ (
		"lzcntl	%1, %0\n\t"
		"lzcntl	%2, %2\n\t"
		"leal	(%0, %2), %0\n\t"
		"cmovnc	%2, %0"
	: "=&r" (y) : "0" ((unsigned int) x), "r" ((unsigned int) (x >> 32)) );
	return y;
}
#endif
#ifdef __BMI__
static inline int _tzcnt_u64(unsigned long long x) {
	int	y;
	__asm__ (
		"tzcntl	%1, %0\n\t"
		"tzcntl	%2, %2\n\t"
		"leal	(%0, %2), %0\n\t"
		"cmovnc	%2, %0"
	: "=&r" (y) : "0" ((unsigned int) (x >> 32)), "r" ((unsigned int) x) );
	return y;
}
#endif
#elif defined(USE_MSVC_X86) && (defined(__AVX2__) || defined(__LZCNT__))
static inline int _lzcnt_u64(unsigned long long x) {
	__asm {
		lzcnt	eax, dword ptr x
		lzcnt	edx, dword ptr x+4
		lea	eax, [eax+edx]
		cmovnc	eax, edx
	}
}

static inline int _tzcnt_u64(unsigned long long x) {
	__asm {
		tzcnt	eax, dword ptr x+4
		tzcnt	edx, dword ptr x
		lea	eax, [eax+edx]
		cmovnc	eax, edx
	}
}
#endif

#if defined(__AVX2__) || defined(__LZCNT__)
	#define	lzcnt_u32(x)	_lzcnt_u32(x)
	#define	lzcnt_u64(x)	_lzcnt_u64(x)

#elif defined(_M_ARM) || defined(_M_ARM64)
	#define lzcnt_u32(x)	_CountLeadingZeros(x)
	#define lzcnt_u64(x)	_CountLeadingZeros64(x)

#elif defined(_MSC_VER)
	#ifdef _M_X64
		static inline int lzcnt_u64(unsigned long long n) {
			unsigned long i;
			if (!_BitScanReverse64(&i, n))
				i = 64 ^ 63;
			return i ^ 63;
		}
	#else
		static inline int lzcnt_u64(unsigned long long n) {
			unsigned long i;
			if (_BitScanReverse(&i, n >> 32))
				return i ^ 31;
			if (!_BitScanReverse(&i, (unsigned int) n))
				i = 64 ^ 63;
			return i ^ 63;
		}
	#endif

#elif defined(__ARM_FEATURE_CLZ)
	#define	lzcnt_u32(x)	__clz(x)
	#define	lzcnt_u64(x)	__clzll(x)

#else
	static inline int lzcnt_u32(unsigned long x) { return (x ? __builtin_clz(x) : 32); }
	static inline int lzcnt_u64(unsigned long x) { return (x ? __builtin_clzll(x) : 64); }
#endif

#endif // EDAX_BIT_H
