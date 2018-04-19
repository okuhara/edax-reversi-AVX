/**
 * @file bit.h
 *
 * Bitwise operations header file.
 *
 * @date 1998 - 2018
 * @author Richard Delorme
 * @version 4.4
 */

#ifndef EDAX_BIT_H
#define EDAX_BIT_H

#include <stdio.h>
#include <stdbool.h>

struct Random;

/* declaration */
int bit_weighted_count(const unsigned long long);
// int next_bit(unsigned long long*);
void bitboard_write(const unsigned long long, FILE*);
unsigned long long transpose(unsigned long long);
unsigned long long horizontal_mirror(unsigned long long);
int get_rand_bit(unsigned long long, struct Random*);

#ifdef __GNUC__
#define	bswap_short(x)	__builtin_bswap16(x)
#define	bswap_int(x)	__builtin_bswap32(x)
#define	vertical_mirror(x)	__builtin_bswap64(x)
#elif defined(_MSC_VER)
#define	bswap_short(x)	_byteswap_ushort(x)
#define	bswap_int(x)	_byteswap_ulong(x)
#define	vertical_mirror(x)	_byteswap_uint64(x)
#else
unsigned short bswap_short(unsigned short);
unsigned int bswap_int(unsigned int);
unsigned long long vertical_mirror(unsigned long long);
#endif

#ifdef __GNUC__
#define	first_bit(x)	__builtin_ctzll(x)
#define	last_bit(x)	(63 - __builtin_clzll(x))
#else
int first_bit(unsigned long long);
int last_bit(unsigned long long);
#endif

/** Loop over each bit set. */
#define foreach_bit(i, b)	for (i = first_bit(b); b; i = first_bit(b &= (b - 1)))

#ifndef __x86_64__
#ifdef __GNUC__
#define	first_bit_32(x)	__builtin_ctz(x)
#else
int first_bit_32(unsigned int);
#endif
#define foreach_bit_32(i, b)	for (i = first_bit_32(b); b; i = first_bit_32(b &= (b - 1)))
#endif

extern const unsigned long long X_TO_BIT[];
/** Return a bitboard with bit x set. */
#define x_to_bit(x) X_TO_BIT[x]

//#define x_to_bit(x) (1ULL << (x)) // 1% slower on Sandy Bridge

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
	#if defined (USE_MSVC_X64)
		#define	bit_count(x)	__popcnt64(x)
	#elif defined (__x86_64__)
		#define bit_count(x)	__builtin_popcountll(x)
	#else
		#define bit_count(x)	(__builtin_popcount((unsigned int) (x)) + __builtin_popcount((unsigned int) ((x) >> 32)))
	#endif
#else
	int bit_count(unsigned long long);
#endif

#ifdef __x86_64__
	#define hasSSE2	1
#endif
#ifdef hasSSE2
	#define	hasMMX	1
#endif

#ifdef USE_GAS_MMX
#ifndef hasSSE2
	extern bool	hasSSE2;
#endif
#ifndef hasMMX
	extern bool	hasMMX;
#endif
#endif

#endif
