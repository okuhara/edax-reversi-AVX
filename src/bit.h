/**
 * @file bit.h
 *
 * Bitwise operations header file.
 *
 * @date 1998 - 2025
 * @author Richard Delorme
 * @version 4.5
 */

#ifndef EDAX_BIT_H
#define EDAX_BIT_H

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <stdbool.h>

// bit_intrinsics: CPU dependent bit operation intrinsics.

#if defined(__x86_64__) || defined(_M_X64) || defined(__aarch64__) || defined(_M_ARM64)
	#define	HAS_CPU_64	1
#endif

#if defined(__SSE2__) || defined(__AVX__) || defined(_M_X64)
	#define hasSSE2	1
#endif

#ifdef hasSSE2
	#define	hasMMX	1
#endif

#if defined(ANDROID) && defined(__arm__)
  #if __ANDROID_API__ < 21
	#define	DISPATCH_NEON	1
  #else
	#define	__ARM_NEON	1
  #endif
#elif defined(__ARM_NEON__) || defined(__aarch64__) || defined(_M_ARM) || defined(_M_ARM64)
	#define	__ARM_NEON	1
#endif
#ifdef __ARM_NEON
	#include "arm_neon.h"
#endif

#ifdef _MSC_VER
	#include <intrin.h>
  #ifdef _M_IX86
	#define	USE_MSVC_X86	1
  #endif
#elif defined(hasSSE2)
	#include <x86intrin.h>
#endif

#ifndef __has_builtin  // Compatibility with non-clang compilers.
	#define __has_builtin(x) 0
#endif

// mirror byte
#if defined(_M_ARM) // || (defined(_M_ARM64) && _MSC_VER >= 1922)	// https://developercommunity.visualstudio.com/t/ARM64-still-missing-RBIT-intrinsics/10547420
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
	#define	rotl8(x,y)	((unsigned char)(((x)<<(y))|((unsigned char)(x)>>(8-(y)))))
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
	static inline int lzcnt_u32(unsigned int n) {
		unsigned long i;
		if (!_BitScanReverse(&i, n))
			i = 32 ^ 31;
		return i ^ 31;
	}
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
  #if __ARM_ACLE >= 110
	#define	lzcnt_u32(x)	__clz(x)
	#define	lzcnt_u64(x)	__clzll(x)
  #else // assuming CLZ will be used
	#define	lzcnt_u32(x)	__builtin_clz(x)
	#define	lzcnt_u64(x)	__builtin_clzll(x)
  #endif

#else
	static inline int lzcnt_u32(unsigned long x) { return (x ? __builtin_clz(x) : 32); }
	static inline int lzcnt_u64(unsigned long x) { return (x ? __builtin_clzll(x) : 64); }
#endif

#if defined(__BMI__) || defined(__AVX2__)
	#define	tzcnt_u32(x)	_tzcnt_u32(x)
	#define	tzcnt_u64(x)	_tzcnt_u64(x)

#elif defined(_M_ARM64)
	#define tzcnt_u32(x)	_CountTrailingZeros(x)
	#define tzcnt_u64(x)	_CountTrailingZeros64(x)

#elif defined(_M_ARM)
	#define	tzcnt_u32(x)	_arm_clz(_arm_rbit(x))

#elif defined(__ARM_FEATURE_CLZ)
  #if __ARM_ACLE >= 110
	#define	tzcnt_u32(x)	__clz(__rbit(x))
	#define	tzcnt_u64(x)	__clzll(__rbitll(x))
  #endif
#endif

#if defined(__SSE4_2__) || defined(__AVX__)
  #ifdef HAS_CPU_64
	#define	crc32c_u64(crc,d)	_mm_crc32_u64((crc),(d))
  #else
	#define	crc32c_u64(crc,d)	_mm_crc32_u32(_mm_crc32_u32((crc),(d)),((d)>>32))
  #endif
	#define	crc32c_u8(crc,d)	_mm_crc32_u8((crc),(d))

#elif defined(__ARM_FEATURE_CRC32)
  #ifndef _MSC_VER
	#include "arm_acle.h"
  #endif
	#define	crc32c_u64(crc,d)	__crc32cd((crc),(d))
	#define crc32c_u8(crc,d)	__crc32cb((crc),(d))

#else
	unsigned int crc32c_u64(unsigned int crc, unsigned long long data);
	unsigned int crc32c_u8(unsigned int crc, unsigned int data);
#endif

#ifdef __GNUC__
	#define UNREACHABLE	__builtin_unreachable()
#elif defined(_MSC_VER)
	#define UNREACHABLE	__assume(0)
#else
	#define	UNREACHABLE
#endif

struct Random;

// end of bit_intrinsics

/* declaration */
void bit_init(void);
// int next_bit(unsigned long long*);
void bitboard_write(unsigned long long, FILE*);
unsigned long long transpose(unsigned long long);
unsigned int horizontal_mirror_32(unsigned int b);
unsigned long long horizontal_mirror(unsigned long long);
int get_rand_bit(unsigned long long, struct Random*);

#if !defined(__AVX2__) && defined(hasSSE2) && !defined(POPCOUNT)
	__m128i bit_weighted_count_sse(unsigned long long, unsigned long long);
#elif defined (__ARM_NEON)
	uint64x2_t bit_weighted_count_neon(unsigned long long, unsigned long long);
#else
	int bit_weighted_count(unsigned long long);
#endif

extern unsigned long long X_TO_BIT[];
extern const unsigned long long NEIGHBOUR[];

/** Return a bitboard with bit x set. */
// https://eukaryote.hateblo.jp/entry/2020/04/12/054905
#ifdef HAS_CPU_64 // 1% slower on Sandy Bridge
	#define x_to_bit(x) (1ULL << (x))
#else
	#define x_to_bit(x) X_TO_BIT[x]
#endif

/** Loop over each bit set. */
#if defined(tzcnt_u64)
	#define	first_bit(x)	tzcnt_u64(x)
	#define	last_bit(x)	(63 - lzcnt_u64(x))
#elif ((defined(__GNUC__) && (__GNUC__ >= 4)) || __has_builtin(__builtin_ctzll)) && !defined(__INTEL_COMPILER)
	#define	first_bit(x)	__builtin_ctzll(x)
	#define	last_bit(x)	(63 - __builtin_clzll(x))
#else
	int first_bit(unsigned long long);
	int last_bit(unsigned long long);
#endif

#if defined(HAS_CPU_64) || !defined(__STDC_HOSTED__)	// __STDC_HOSTED__ (C99) to declare var in for statement
	#define foreach_bit(i, b)	for (i = first_bit(b); b; i = first_bit(b &= (b - 1)))
#else
  #ifdef tzcnt_u32
	#define	first_bit_32(x)	tzcnt_u32(x)
  #else
	int first_bit_32(unsigned int);
  #endif
	#define foreach_bit(i, b)	(void) i; for (unsigned int _j = 0; _j < sizeof(b) * CHAR_BIT; _j += sizeof(int) * CHAR_BIT) \
		for (int _r = (b >> _j), i = first_bit_32(_r) + _j; _r; i = first_bit_32(_r &= (_r - 1)) + _j)
#endif

// popcount
#ifdef __ARM_NEON
  #ifdef HAS_CPU_64
	#define bit_count(x)	vaddv_u8(vcnt_u8(vcreate_u8(x)))
	#define bit_count_32(x)	vaddv_u8(vcnt_u8(vcreate_u8((unsigned int) x)))
  #else
	#define bit_count(x)	vget_lane_u32(vreinterpret_u32_u64(vpaddl_u32(vpaddl_u16(vpaddl_u8(vcnt_u8(vcreate_u8(x)))))), 0)
	#define bit_count_32(x)	vget_lane_u32(vpaddl_u16(vpaddl_u8(vcnt_u8(vcreate_u8(x)))), 0)
  #endif

#elif defined(POPCOUNT)
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
	#define bit_count_32(x)	_CountOneBits(x)
    #elif defined(_M_X64)
	#define bit_count(x)	((int) __popcnt64(x))
	#define bit_count_32(x)	__popcnt(x)
    #else
	#define bit_count(x)	(__popcnt((unsigned int) (x)) + __popcnt((unsigned int) ((x) >> 32)))
	#define bit_count_32(x)	__popcnt(x)
    #endif
  #else
	#define bit_count(x)	__builtin_popcountll(x)
	#define bit_count_32(x)	__builtin_popcount(x)
  #endif
	#define bit_count_si64(x)	bit_count(_mm_cvtsi128_si64(x))

#else
	extern unsigned char PopCnt16[1 << 16];
	static inline int bit_count(unsigned long long b) {
		union { unsigned long long bb; unsigned short u[4]; } v = { b };
		return (unsigned char)(PopCnt16[v.u[0]] + PopCnt16[v.u[1]] + PopCnt16[v.u[2]] + PopCnt16[v.u[3]]);
	}
	static inline int bit_count_32(unsigned int b) {
		union { unsigned int bb; unsigned short u[2]; } v = { b };
		return (unsigned char)(PopCnt16[v.u[0]] + PopCnt16[v.u[1]]);
	}
	#define bit_count_si64(x)	((unsigned char)(PopCnt16[_mm_extract_epi16((x), 0)] + PopCnt16[_mm_extract_epi16((x), 1)] + PopCnt16[_mm_extract_epi16((x), 2)] + PopCnt16[_mm_extract_epi16((x), 3)]))
#endif

#if defined(USE_GAS_MMX) || defined(USE_MSVC_X86)
  #ifndef hasSSE2
	extern bool	hasSSE2;
  #endif
  #ifndef hasMMX
	extern bool	hasMMX;
  #endif
#endif

#if defined(ANDROID) && ((defined(__arm__) && !defined(__ARM_NEON)) || (defined(__i386__) && !defined(hasSSE2)))
extern bool	hasSSE2;
#endif

/** Board : board representation */
typedef struct Board {
	unsigned long long player, opponent;     /**< bitboard representation */
} Board;

typedef union {
	unsigned long long	ull[2];
	Board	board;	// for vboard optimization in search
  #ifdef __ARM_NEON
	uint64x2_t	v2;
  #elif defined(hasSSE2) || defined(USE_MSVC_X86)
	__m128i	v2;
	__m128d	d2;	// used in flip_carry_sse_32.c
  #endif
}
#if defined(__GNUC__) && !defined(hasSSE2)
__attribute__ ((aligned (16)))
#endif
V2DI;

typedef union {
	unsigned long long	ull[4];
  #ifdef __AVX2__
	__m256i	v4;
  #endif
  #ifdef hasSSE2
	__m128i	v2[2];
  #endif
  #ifdef USE_MSVC_X86
	__m64	v1[4];
  #endif
} V4DI;

typedef union {
	unsigned long long	ull[8];
  #ifdef __AVX512VL__
	__m512i	v8;
  #endif
  #ifdef __AVX2__
	__m256i	v4[2];
  #endif
} V8DI;

#ifdef hasSSE2
typedef union {
	unsigned int	ui[4];
	__m128i	v4;
} V4SI;
#endif

/* Define function attributes directive when available */

#if (defined(_MSC_VER) || defined(__clang__)) && defined(hasSSE2)
	#define	vectorcall	__vectorcall
#elif defined(__GNUC__) && defined(__i386__)
	#define	vectorcall	__attribute__((sseregparm))
#elif 0 // defined(__GNUC__)	// erroreous result on pgo-build
	#define	vectorcall	__attribute__((sysv_abi))
#else
	#define	vectorcall
#endif

// X64 compatibility sims for X86
#if !defined(HAS_CPU_64) && (defined(hasSSE2) || defined(USE_MSVC_X86))
	static inline __m128i _mm_cvtsi64_si128(const unsigned long long x) {
		return _mm_unpacklo_epi32(_mm_cvtsi32_si128(x), _mm_cvtsi32_si128(x >> 32));
	}
		// better code but requires lvalue
	// #define	_mm_cvtsi64_si128(x)	_mm_loadl_epi64((__m128i *) &(x))

	static inline unsigned long long vectorcall _mm_cvtsi128_si64(__m128i x) {
		return *(unsigned long long *) &x;
	}
	static inline unsigned long long vectorcall _mm_extract_epi64(__m128i x, int i) {
		return ((unsigned long long *) &x)[i];
	}

  #if defined(_MSC_VER) && _MSC_VER<1900
	static inline __m128i _mm_set_epi64x(unsigned long long b, unsigned long long a) {
		return _mm_unpacklo_epi64(_mm_cvtsi64_si128(b), _mm_cvtsi64_si128(a));
	}
	static inline __m128i _mm_set1_epi64x(unsigned long long x) {
		__m128i t = _mm_cvtsi64_si128(x);
		return _mm_unpacklo_epi64(t, t);
	}
  #endif
#endif // !HAS_CPU_64

#if __clang_major__ == 3	// undefined reference to `llvm.x86.avx.storeu.dq.256'
	#define	_mm_storeu_si128(a,b)	*(__m128i *)(a) = (b)
	#define	_mm256_storeu_si256(a,b)	*(__m256i *)(a) = (b)
#endif

#endif // EDAX_BIT_H
