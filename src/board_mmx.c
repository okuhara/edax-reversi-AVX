/**
 * @file board_mmx.c
 *
 * MMX translation of some board.c functions
 *
 * If both hasMMX and hasSSE2 are undefined, dynamic dispatching code
 * will be generated.  (This setting requires GCC 4.4+)
 *
 * Parameters are passed as 4 ints instead of 2 longlongs to avoid
 * alignment ajust and to enable store to load forwarding.
 *
 * @date 2014
 * @author Toshihiko Okuhara
 * @version 4.4
 */

#include "bit.h"
#include "hash.h"
#include "board.h"
#include "move.h"

#ifdef hasSSE2
	#include <emmintrin.h>
#else
#ifndef hasMMX
	#pragma GCC push_options
	#pragma GCC target ("mmx")
#endif
	#include <mmintrin.h>
#endif

static const unsigned long long mask_7e = 0x7e7e7e7e7e7e7e7eULL;
#ifndef POPCOUNT
static const unsigned long long mask_55 = 0x5555555555555555ULL;
static const unsigned long long mask_33 = 0x3333333333333333ULL;
static const unsigned long long mask_0F = 0x0f0f0f0f0f0f0f0fULL;
#endif

#ifndef hasSSE2

#ifndef hasMMX
bool	hasMMX = false;
#endif
bool	hasSSE2 = false;

void init_mmx (void)
{
	unsigned int cpuid_edx, cpuid_ecx;

	unsigned int flg1, flg2;

	__asm__ (
		"pushf\n\t"
		"popl	%0\n\t"
		"movl	%0, %1\n\t"
		"btc	$21, %0\n\t"	/* flip ID bit in EFLAGS */
		"pushl	%0\n\t"
		"popf\n\t"
		"pushf\n\t"
		"popl	%0"
	: "=r" (flg1), "=r" (flg2) );

	if (flg1 == flg2)	/* CPUID not supported */
		return;

	__asm__ (
		"movl	$1, %%eax\n\t"
		"cpuid"
	: "=d" (cpuid_edx), "=c" (cpuid_ecx) :: "%eax", "%ebx" );

#ifndef hasMMX
	hasMMX  = ((cpuid_edx & 0x00800000u) != 0);
#endif
	hasSSE2 = ((cpuid_edx & 0x04000000u) != 0);
	// hasPOPCNT = ((cpuid_ecx & 0x00800000u) != 0);

#if (MOVE_GENERATOR == MOVE_GENERATOR_32)
	if (hasSSE2)
		init_flip_sse();
#endif
}
#endif	// hasSSE2

/**
 * @brief Update a board.
 *
 * Update a board by flipping its discs and updating every other data,
 * according to the 'move' description.
 *
 * @param board the board to modify
 * @param move  A Move structure describing the modification.
 */
#ifdef hasMMX
#if defined(hasSSE2) && !defined(__3dNOW__)

void board_update(Board *board, const Move *move)
{
	__v2di	OP = _mm_loadu_si128((__m128i *) board);
	OP ^= (_mm_set1_epi64x(move->flipped) | _mm_loadl_epi64((__m128i *) &X_TO_BIT[move->x]));
	_mm_storel_pi((__m64 *) &board->opponent, (__m128) OP);
	_mm_storeh_pi((__m64 *) &board->player, (__m128) OP);
	board_check(board);
}

#else	// Faster on AMD but not for CPU with slow emms

void board_update(Board *board, const Move *move)
{
	__asm__ (
		"movq	%2, %%mm1\n\t"
		"movq	%3, %%mm0\n\t"
		"por	%%mm1, %%mm0\n\t"
		"pxor	%0, %%mm0\n\t"
		"pxor	%1, %%mm1\n\t"
		"movq	%%mm0, %1\n\t"
		"movq	%%mm1, %0\n\t"
		"emms"
	: "=m" (board->player), "=m" (board->opponent)
	: "m" (move->flipped), "m" (x_to_bit(move->x))
	: "mm0", "mm1");
	board_check(board);
}

#endif
#endif


/**
 * @brief Restore a board.
 *
 * Restore a board by un-flipping its discs and restoring every other data,
 * according to the 'move' description, in order to cancel a board_update_move.
 *
 * @param board board to restore.
 * @param move  a Move structure describing the modification.
 */
#ifdef hasMMX
#if defined(hasSSE2) && !defined(__3dNOW__)

void board_restore(Board *board, const Move *move)
{
	__v2di	OP = _mm_loadu_si128((__m128i *) board);
	OP ^= (_mm_set1_epi64x(move->flipped) | _mm_set_epi64x(x_to_bit(move->x), 0));
	_mm_storel_pi((__m64 *) &board->opponent, (__m128) OP);
	_mm_storeh_pi((__m64 *) &board->player, (__m128) OP);
	board_check(board);
}

#else

void board_restore(Board *board, const Move *move)
{
	__asm__ (
		"movq	%2, %%mm1\n\t"
		"movq	%3, %%mm0\n\t"
		"por	%%mm1, %%mm0\n\t"
		"pxor	%1, %%mm0\n\t"
		"pxor	%0, %%mm1\n\t"
		"movq	%%mm0, %0\n\t"
		"movq	%%mm1, %1\n\t"
		"emms"
	: "=m" (board->player), "=m" (board->opponent)
	: "m" (move->flipped), "m" (x_to_bit(move->x))
	: "mm0", "mm1");
	board_check(board);
}

#endif
#endif

/**
 * @brief MMX translation of get_moves
 *
 * x 2 faster bench mobility on 32-bit x86.
 *
 */
unsigned long long get_moves_mmx(unsigned int PL, unsigned int PH, unsigned int OL, unsigned int OH)
{
	unsigned long long moves;
	__asm__ (
		"movl	%1, %%ebx\n\t"		"movd	%1, %%mm4\n\t"
		"movl	%3, %%edi\n\t"		"movd	%3, %%mm5\n\t"
		"andl	$0x7e7e7e7e, %%edi\n\t"	"punpckldq %2, %%mm4\n\t"
						"punpckldq %4, %%mm5\n\t"
				/* shift=-1 */			/* shift=-8 */
		"movl	%%ebx, %%eax\n\t"	"movq	%%mm4, %%mm0\n\t"
		"shrl	$1, %%eax\n\t"		"psrlq	$8, %%mm0\n\t"
		"andl	%%edi, %%eax\n\t"	"pand	%%mm5, %%mm0\n\t"	// 0 m7&o6 m6&o5 .. m1&o0
		"movl	%%eax, %%edx\n\t"	"movq	%%mm0, %%mm1\n\t"
		"shrl	$1, %%eax\n\t"		"psrlq	$8, %%mm0\n\t"
		"movl	%%edi, %%ecx\n\t"	"movq	%%mm5, %%mm3\n\t"
		"andl	%%edi, %%eax\n\t"	"pand	%%mm5, %%mm0\n\t"	// 0 0 m7&o6&o5 .. m2&o1&o0
		"shrl	$1, %%ecx\n\t"		"psrlq	$8, %%mm3\n\t"
		"orl	%%edx, %%eax\n\t"	"por	%%mm1, %%mm0\n\t"	// 0 m7&o6 (m6&o5)|(m7&o6&o5) .. (m1&o0)
		"andl	%%edi, %%ecx\n\t"	"pand	%%mm5, %%mm3\n\t"	// 0 o7&o6 o6&o5 o5&o4 o4&o3 ..
		"movl	%%eax, %%edx\n\t"	"movq	%%mm0, %%mm2\n\t"
		"shrl	$2, %%eax\n\t"		"psrlq	$16, %%mm0\n\t"
		"andl	%%ecx, %%eax\n\t"	"pand	%%mm3, %%mm0\n\t"	// 0 0 0 m7&o6&o5&o4 (m6&o5&o4&o3)|(m7&o6&o5&o4&o3) ..
		"orl	%%eax, %%edx\n\t"	"por	%%mm0, %%mm2\n\t"
		"shrl	$2, %%eax\n\t"		"psrlq	$16, %%mm0\n\t"
		"andl	%%ecx, %%eax\n\t"	"pand	%%mm3, %%mm0\n\t"	// 0 0 0 0 0 m7&o6&..&o2 (m6&o5&..&o1)|(m7&o6&..&o1) ..
		"orl	%%edx, %%eax\n\t"	"por	%%mm0, %%mm2\n\t"
		"shrl	$1, %%eax\n\t"		"psrlq	$8, %%mm2\n\t"
				/* shift=+1 */			/* shift=+8 */
						"movq	%%mm4, %%mm0\n\t"
		"addl	%%ebx, %%ebx\n\t"	"psllq	$8, %%mm0\n\t"
		"andl	%%edi, %%ebx\n\t"	"pand	%%mm5, %%mm0\n\t"
		"movl	%%ebx, %%edx\n\t"	"movq	%%mm0, %%mm1\n\t"
		"addl	%%ebx, %%ebx\n\t"	"psllq	$8, %%mm0\n\t"
		"andl	%%edi, %%ebx\n\t"	"pand	%%mm5, %%mm0\n\t"
		"orl	%%ebx, %%edx\n\t"	"por	%%mm1, %%mm0\n\t"
		"addl	%%ecx, %%ecx\n\t"	"psllq	$8, %%mm3\n\t"
						"movq	%%mm0, %%mm1\n\t"
		"leal	(,%%edx,4), %%ebx\n\t"	"psllq	$16, %%mm0\n\t"
		"andl	%%ecx, %%ebx\n\t"	"pand	%%mm3, %%mm0\n\t"
		"orl	%%ebx, %%edx\n\t"	"por	%%mm0, %%mm1\n\t"
		"shll	$2, %%ebx\n\t"		"psllq	$16, %%mm0\n\t"
		"andl	%%ecx, %%ebx\n\t"	"pand	%%mm3, %%mm0\n\t"
		"orl	%%edx, %%ebx\n\t"	"por	%%mm1, %%mm0\n\t"
		"addl	%%ebx, %%ebx\n\t"	"psllq	$8, %%mm0\n\t"
		"orl	%%eax, %%ebx\n\t"	"por	%%mm0, %%mm2\n\t"
								/* shift=+7 */
						"pand	%5, %%mm5\n\t"
						"movq	%%mm4, %%mm0\n\t"
						"psrlq	$7, %%mm0\n\t"
						"pand	%%mm5, %%mm0\n\t"
						"movq	%%mm0, %%mm1\n\t"
						"psrlq	$7, %%mm0\n\t"
						"pand	%%mm5, %%mm0\n\t"
						"movq	%%mm5, %%mm3\n\t"
						"por	%%mm1, %%mm0\n\t"
						"psrlq	$7, %%mm3\n\t"
						"movq	%%mm0, %%mm1\n\t"
						"pand	%%mm5, %%mm3\n\t"
						"psrlq	$14, %%mm0\n\t"
						"pand	%%mm3, %%mm0\n\t"
		"movl	%2, %%esi\n\t"		"por	%%mm0, %%mm1\n\t"
		"movl	%4, %%edi\n\t"		"psrlq	$14, %%mm0\n\t"
		"andl	$0x7e7e7e7e,%%edi\n\t"	"pand	%%mm3, %%mm0\n\t"
		"movl	%%edi, %%ecx\n\t"	"por	%%mm1, %%mm0\n\t"
		"shrl	$1, %%ecx\n\t"		"psrlq	$7, %%mm0\n\t"
		"andl	%%edi, %%ecx\n\t"	"por	%%mm0, %%mm2\n\t"
				/* shift=-1 */			/* shift=+7 */
		"movl	%%esi, %%eax\n\t"	"movq	%%mm4, %%mm0\n\t"
		"shrl	$1, %%eax\n\t"		"psllq	$7, %%mm0\n\t"
		"andl	%%edi, %%eax\n\t"	"pand	%%mm5, %%mm0\n\t"
		"movl	%%eax, %%edx\n\t"	"movq	%%mm0, %%mm1\n\t"
		"shrl	$1, %%eax\n\t"		"psllq	$7, %%mm0\n\t"
		"andl	%%edi, %%eax\n\t"	"pand	%%mm5, %%mm0\n\t"
		"orl	%%edx, %%eax\n\t"	"por	%%mm1, %%mm0\n\t"
						"psllq	$7, %%mm3\n\t"
		"movl	%%eax, %%edx\n\t"	"movq	%%mm0, %%mm1\n\t"
		"shrl	$2, %%eax\n\t"		"psllq	$14, %%mm0\n\t"
		"andl	%%ecx, %%eax\n\t"	"pand	%%mm3, %%mm0\n\t"
		"orl	%%eax, %%edx\n\t"	"por	%%mm0, %%mm1\n\t"
		"shrl	$2, %%eax\n\t"		"psllq	$14, %%mm0\n\t"
		"andl	%%ecx, %%eax\n\t"	"pand	%%mm3, %%mm0\n\t"
		"orl	%%edx, %%eax\n\t"	"por	%%mm1, %%mm0\n\t"
		"shrl	$1, %%eax\n\t"		"psllq	$7, %%mm0\n\t"
						"por	%%mm0, %%mm2\n\t"
				/* shift=+1 */			/* shift=-9 */
						"movq	%%mm4, %%mm0\n\t"
		"addl	%%esi, %%esi\n\t"	"psrlq	$9, %%mm0\n\t"
		"andl	%%edi, %%esi\n\t"	"pand	%%mm5, %%mm0\n\t"
		"movl	%%esi, %%edx\n\t"	"movq	%%mm0, %%mm1\n\t"
		"addl	%%esi, %%esi\n\t"	"psrlq	$9, %%mm0\n\t"
		"andl	%%edi, %%esi\n\t"	"pand	%%mm5, %%mm0\n\t"
						"movq	%%mm5, %%mm3\n\t"
		"orl	%%esi, %%edx\n\t"	"por	%%mm1, %%mm0\n\t"
						"psrlq	$9, %%mm3\n\t"
						"movq	%%mm0, %%mm1\n\t"
		"addl	%%ecx, %%ecx\n\t"	"pand	%%mm5, %%mm3\n\t"
		"leal	(,%%edx,4), %%esi\n\t"	"psrlq	$18, %%mm0\n\t"
		"andl	%%ecx, %%esi\n\t"	"pand	%%mm3, %%mm0\n\t"
		"orl	%%esi, %%edx\n\t"	"por	%%mm0, %%mm1\n\t"
		"shll	$2, %%esi\n\t"		"psrlq	$18, %%mm0\n\t"
		"andl	%%ecx, %%esi\n\t"	"pand	%%mm3, %%mm0\n\t"
		"orl	%%edx, %%esi\n\t"	"por	%%mm1, %%mm0\n\t"
		"addl	%%esi, %%esi\n\t"	"psrlq	$9, %%mm0\n\t"
		"orl	%%eax, %%esi\n\t"	"por	%%mm0, %%mm2\n\t"
								/* shift=+9 */
						"movq	%%mm4, %%mm0\n\t"
						"psllq	$9, %%mm0\n\t"
						"pand	%%mm5, %%mm0\n\t"
						"movq	%%mm0, %%mm1\n\t"
						"psllq	$9, %%mm0\n\t"
						"pand	%%mm5, %%mm0\n\t"
						"por	%%mm1, %%mm0\n\t"
						"psllq	$9, %%mm3\n\t"
						"movq	%%mm0, %%mm1\n\t"
						"psllq	$18, %%mm0\n\t"
						"pand	%%mm3, %%mm0\n\t"
		"movl	%1, %%eax\n\t"		"por	%%mm0, %%mm1\n\t"
		"movl	%2, %%edx\n\t"		"psllq	$18, %%mm0\n\t"
		"orl	%3, %%eax\n\t"		"pand	%%mm3, %%mm0\n\t"
		"orl	%4, %%edx\n\t"		"por	%%mm1, %%mm0\n\t"
		"notl	%%eax\n\t"		"psllq	$9, %%mm0\n\t"
		"notl	%%edx\n\t"		"por	%%mm0, %%mm2\n\t"
		/* mm2|(esi:ebx) is the pseudo-feasible moves at this point. */
		/* Let edx:eax be the feasible moves, i.e., mm2 restricted to empty squares. */
		"movd	%%mm2, %%ecx\n\t"	"punpckhdq %%mm2, %%mm2\n\t"
		"orl	%%ecx, %%ebx\n\t"
		"movd	%%mm2, %%ecx\n\t"
		"orl	%%ecx, %%esi\n\t"
		"andl	%%ebx, %%eax\n\t"
		"andl	%%esi, %%edx\n\t"
		"emms"		/* Reset the FP/MMX unit. */
	: "=&A" (moves)
	: "m" (PL), "m" (PH), "m" (OL), "m" (OH), "m" (mask_7e)
	: "ebx", "ecx", "esi", "edi", "mm0", "mm1", "mm2", "mm3", "mm4", "mm5" );

	return moves;
}

/**
 * @brief MMX translation of get_stability()
 *
 * x 1.5 faster bench stability on 32-bit x86.
 *
 */
#if !(defined(__clang__) && (__clang__major__ < 3))
// LLVM ERROR: Unsupported asm: input constraint with a matching output constraint of incompatible type!

#define	get_full_lines_mmx(result,disc,dir,edge)	__asm__ (\
		"movq	%2, %%mm0\n\t"		"movq	%2, %%mm1\n\t"\
		"psrlq	%3, %%mm0\n\t"		"psllq	%3, %%mm1\n\t"\
		"por	%6, %%mm0\n\t"		"por	%6, %%mm1\n\t"\
		"pand	%2, %%mm0\n\t"		"pand	%2, %%mm1\n\t"\
		"movq	%%mm0, %%mm2\n\t"	"movq	%%mm1, %%mm3\n\t"\
		"psrlq	%4, %%mm0\n\t"		"psllq	%4, %%mm1\n\t"\
		"por	%7, %%mm0\n\t"		"por	%9, %%mm1\n\t"\
		"pand	%%mm2, %%mm0\n\t"	"pand	%%mm3, %%mm1\n\t"\
		"movq	%%mm0, %%mm2\n\t"	"pand	%%mm1, %%mm0\n\t"\
		"psrlq	%5, %%mm2\n\t"		"psllq	%5, %%mm1\n\t"\
		"por	%8, %%mm2\n\t"		"por	%10, %%mm1\n\t"\
		"pand	%%mm2, %%mm0\n\t"	"pand	%%mm1, %%mm0\n\t"\
		"movq	%%mm0, %0\n\t"\
		"pand	%%mm0, %1"\
	: "=m" (result), "+y" (stable)\
	: "y" (disc), "i" (dir), "i" (dir * 2), "i" (dir * 4),\
	  "my" (e0), "m" (edge[0]), "m" (edge[1]), "m" (edge[2]), "m" (edge[3])\
	: "mm0", "mm1", "mm2", "mm3");

int get_stability_mmx(unsigned int PL, unsigned int PH, unsigned int OL, unsigned int OH)
{
	__m64	P, O, P_central, disc, full_h, full_v, full_d7, full_d9, stable;
#ifdef hasSSE2
	__v2di	PO;
#endif
	int	t, a1a8po, h1h8po;
	static const unsigned long long e0 = 0xff818181818181ffULL;
	// static const unsigned long long e1[] = { 0xffc1c1c1c1c1c1ffULL, 0xfff1f1f1f1f1f1ffULL, 0xff838383838383ffULL, 0xff8f8f8f8f8f8fffULL };
	// static const unsigned long long e8[] = { 0xffff8181818181ffULL, 0xffffffff818181ffULL, 0xff8181818181ffffULL, 0xff818181ffffffffULL };
	static const unsigned long long e7[] = { 0xffff8383838383ffULL, 0xffffffff8f8f8fffULL, 0xffc1c1c1c1c1ffffULL, 0xfff1f1f1ffffffffULL };
	static const unsigned long long e9[] = { 0xffffc1c1c1c1c1ffULL, 0xfffffffff1f1f1ffULL, 0xff8383838383ffffULL, 0xff8f8f8fffffffffULL };
#ifndef POPCOUNT
	static const unsigned long long M55 = 0x5555555555555555ULL;
	static const unsigned long long M33 = 0x3333333333333333ULL;
	static const unsigned long long M0F = 0x0f0f0f0f0f0f0f0fULL;
#endif

	__asm__ (
		"movd	%2, %0\n\t"		"movd	%4, %1\n\t"
		"punpckldq %3, %0\n\t"		"punpckldq %5, %1"
	: "=&y" (P), "=&y" (O) : "m" (PL), "m" (PH), "m" (OL), "m" (OH));
#ifdef hasSSE2
	PO = _mm_unpacklo_epi64(_mm_movpi64_epi64(O), _mm_movpi64_epi64(P));
#endif
	__asm__ (
		"por	%3, %0\n\t"
		"pandn	%3, %1\n\t"
		"movq	%1, %2"
	: "=y" (disc), "=y" (stable), "=m" (P_central)
	: "y" (P), "0" (O), "1" (e0));

	// get full lines and set intersection of them to stable
	// get_full_lines_mmx(full_h, disc, 1, e1);
	__asm__ (
		"pcmpeqb %%mm0, %%mm0\n\t"
		"pcmpeqb %2, %%mm0\n\t"
		"movq	%%mm0, %0\n\t"
		"pand	%%mm0, %1"
	: "=m" (full_h), "+y" (stable) : "y" (disc) : "mm0");
	// get_full_lines_mmx(full_v, disc, 8, e8);
	__asm__ (
		"movq	%2, %%mm0\n\t"		"movq	%2, %%mm1\n\t"
		"punpcklbw %%mm0, %%mm0\n\t"	"punpckhbw %%mm1, %%mm1\n\t"
		"pand	%%mm1, %%mm0\n\t"	// (d,d,c,c,b,b,a,a) & (h,h,g,g,f,f,e,e)
		"pshufw	$177, %%mm0, %%mm1\n\t"
		"pand	%%mm1, %%mm0\n\t"	// (cg,cg,dh,dh,ae,ae,bf,bf) & (dh,dh,cg,cg,bf,bf,ae,ae)
		"pshufw	$78, %%mm0, %%mm1\n\t"
		"pand	%%mm1, %%mm0\n\t"	// (abef*4, cdgh*4) & (cdgh*4, abef*4)
		"movq	%%mm0, %0\n\t"
		"pand	%%mm0, %1"
	: "=m" (full_v), "+y" (stable) : "y" (disc) : "mm0", "mm1");
	get_full_lines_mmx(full_d7, disc, 7, e7);
	get_full_lines_mmx(full_d9, disc, 9, e9);

	// compute the exact stable edges (from precomputed tables)
#ifdef hasSSE2
	a1a8po = _mm_movemask_epi8(_mm_slli_epi64(PO, 7));
	h1h8po = _mm_movemask_epi8(PO);
#else
	a1a8po = ((((PL & 0x01010101u) + ((PH & 0x01010101u) << 4)) * 0x01020408u) >> 24) * 256
		+ ((((OL & 0x01010101u) + ((OH & 0x01010101u) << 4)) * 0x01020408u) >> 24);
	h1h8po = ((((PH & 0x80808080u) + ((PL & 0x80808080u) >> 4)) * 0x00204081u) >> 24) * 256
		+ ((((OH & 0x80808080u) + ((OL & 0x80808080u) >> 4)) * 0x00204081u) >> 24);
#endif
	__asm__(
		"movd	%1, %%mm0\n\t"		"por	%3, %0\n\t"
		"movd	%2, %%mm1\n\t"
		"punpckldq %%mm1, %%mm0\n\t"	"movq	%4, %%mm1\n\t"
		"por	%%mm0, %0\n\t"		"psllq	$7, %%mm1\n\t"
						"por	%%mm1, %0"
	: "+y" (stable)
	: "g" ((int) edge_stability[(PL & 0xff) * 256 + (OL & 0xff)]),
	  "g" (edge_stability[((PH >> 16) & 0xff00) + (OH >> 24)] << 24),
	  "m" (A1_A8[edge_stability[a1a8po]]),
	  "m" (A1_A8[edge_stability[h1h8po]])
	: "mm0", "mm1");

	// now compute the other stable discs (ie discs touching another stable disc in each flipping direction).
	__asm__ (
		"movq	%1, %%mm0\n\t"
		"packsswb %%mm0, %%mm0\n\t"
		"movd	%%mm0, %0\n\t"
	: "=g" (t) : "y" (stable) : "mm0" );

	if (t == 0) {
		__asm__ ( "emms" );
		return 0;
	}

	do {
		__asm__ (
			"movq	%1, %%mm3\n\t"
			"movq	%6, %1\n\t"
			"movq	%%mm3, %%mm0\n\t"	"movq	%%mm3, %%mm1\n\t"
			"psrlq	$1, %%mm0\n\t"		"psllq	$1, %%mm1\n\t"		"movq	%%mm3, %%mm2\n\t"
			"por	%%mm1, %%mm0\n\t"	"movq	%%mm3, %%mm1\n\t"	"psrlq	$7, %%mm2\n\t"
			"por	%2, %%mm0\n\t"		"psllq	$7, %%mm1\n\t"		"por	%%mm1, %%mm2\n\t"
			"pand	%%mm0, %1\n\t"						"por	%4, %%mm2\n\t"
			"movq	%%mm3, %%mm0\n\t"	"movq	%%mm3, %%mm1\n\t"	"pand	%%mm2, %1\n\t"
			"psrlq	$8, %%mm0\n\t"		"psllq	$8, %%mm1\n\t"		"movq	%%mm3, %%mm2\n\t"
			"por	%%mm1, %%mm0\n\t"	"movq	%%mm3, %%mm1\n\t"	"psrlq	$9, %%mm2\n\t"
			"por	%3, %%mm0\n\t"		"psllq	$9, %%mm1\n\t"		"por	%%mm1, %%mm2\n\t"
			"pand	%%mm0, %1\n\t"						"por	%5, %%mm2\n\t"
											"pand	%%mm2, %1\n\t"
			"por	%%mm3, %1\n\t"
			"pxor	%1, %%mm3\n\t"
			"packsswb %%mm3, %%mm3\n\t"
			"movd	%%mm3, %0"
		: "=g" (t), "+y" (stable)
		: "m" (full_h), "m" (full_v), "m" (full_d7), "m" (full_d9), "m" (P_central)
		: "mm0", "mm1", "mm2", "mm3");
	} while (t);

	// bit_count(stable)
#ifdef POPCOUNT
	__asm__ (
		"movd	%1, %0\n\t"
		"psrlq	$32, %1\n\t"
		"movd	%1, %%edx\n\t"
		"popcntl %0, %0\n\t"
		"popcntl %%edx, %%edx\n\t"
		"addl	%%edx, %0\n\t"
		"emms"
	: "=&a" (t) : "y" (stable) : "edx");
#else
	__asm__ (
 		"movq	%1, %%mm0\n\t"
		"psrlq	$1, %1\n\t"
		"pand	%2, %1\n\t"
		"psubd	%1, %%mm0\n\t"

		"movq	%%mm0, %%mm1\n\t"
		"psrlq	$2, %%mm0\n\t"
		"pand	%3, %%mm1\n\t"
		"pand	%3, %%mm0\n\t"
		"paddd	%%mm1, %%mm0\n\t"

		"movq	%%mm0, %%mm1\n\t"
		"psrlq	$4, %%mm0\n\t"
		"paddd	%%mm1, %%mm0\n\t"
		"pand	%4, %%mm0\n\t"
	#ifdef hasSSE2
		"pxor	%%mm1, %%mm1\n\t"
		"psadbw	%%mm1, %%mm0\n\t"
		"movd	%%mm0, %0\n\t"
	#else
		"movq	%%mm0, %%mm1\n\t"
		"psrlq	$32, %%mm0\n\t"
		"paddb	%%mm1, %%mm0\n\t"

		"movd	%%mm0, %0\n\t"
		"imull	$0x01010101, %0, %0\n\t"
		"shrl	$24, %0\n\t"
	#endif
		"emms"
	: "=a" (t) : "y" (stable), "m" (M55), "my" (M33), "m" (M0F) : "mm0", "mm1");
#endif

	return t;
}
#endif // clang

/**
 * @brief MMX translation of get_potential_mobility
 *
 * @param P bitboard with player's discs.
 * @param O bitboard with opponent's discs.
 * @return a count of potential moves.
 */
int get_potential_mobility_mmx(unsigned long long P, unsigned long long O)
{
	int count;
	static const unsigned long long mask_v = 0x00ffffffffffff00ULL;
	// static const unsigned long long mask_d = 0x007e7e7e7e7e7e00ULL;	// = mask_7e & mask_v
#ifndef POPCOUNT
	static const unsigned long long mask_15 = 0x1555555555555515ULL;
	static const unsigned long long mask_01 = 0x0100000000000001ULL;
#endif

	__asm__ (
		"movq	%3, %%mm2\n\t"		"movq	%4, %%mm5\n\t"
		"pand	%2, %%mm2\n\t"		"pand	%2, %%mm5\n\t"		"movq	%%mm2, %%mm3\n\t"
		"movq	%%mm2, %%mm4\n\t"	"movq	%%mm5, %%mm6\n\t"	"pand	%%mm5, %%mm3\n\t"
		"psllq	$1, %%mm2\n\t"		"psllq	$8, %%mm5\n\t"
		"psrlq	$1, %%mm4\n\t"		"psrlq	$8, %%mm6\n\t"
		"por	%%mm4, %%mm2\n\t"	"por	%%mm6, %%mm5\n\t"
		"por	%%mm5, %%mm2\n\t"
		"movq	%%mm3, %%mm5\n\t"
		"movq	%%mm3, %%mm4\n\t"	"movq	%%mm5, %%mm6\n\t"
		"psllq	$7, %%mm3\n\t"		"psllq	$9, %%mm5\n\t"
		"psrlq	$7, %%mm4\n\t"		"psrlq	$9, %%mm6\n\t"
		"por	%%mm4, %%mm3\n\t"	"por	%%mm6, %%mm5\n\t"
		"por	%%mm3, %%mm2\n\t"	"por	%%mm5, %%mm2\n\t"
		"por	%1, %2\n\t"
		"pandn	%%mm2, %2\n\t"

#ifdef POPCOUNT
		"movd	%2, %%ecx\n\t"
		"popcntl %%ecx, %0\n\t"		"andl	$0x00000081, %%ecx\n\t"
		"psrlq	$32, %2\n\t"		"popcntl %%ecx, %%ecx\n\t"
		"movd	%2, %%edx\n\t"		"addl	%%ecx, %0\n\t"
		"popcntl %%edx, %%ecx\n\t"	"andl	$0x81000000, %%edx\n\t"
		"addl	%%ecx, %0\n\t"		"popcntl %%edx, %%edx\n\t"
						"addl	%%edx, %0\n\t"
		"emms"
	: "=g" (count) : "y" (P), "y" (O), "m" (mask_7e), "m" (mask_v)
	: "ecx", "edx", "mm2", "mm3", "mm4", "mm5", "mm6");

#else
		"movq	%2, %1\n\t"		"movq	%2, %%mm2\n\t"
		"psrlq	$1, %2\n\t"
		"pand	%5, %2\n\t"		"pand	%6, %%mm2\n\t"
		"psubd	%2, %1\n\t"		"paddd	%%mm2, %1\n\t"

		"movq	%1, %2\n\t"
		"psrlq	$2, %1\n\t"
		"pand	%7, %2\n\t"
		"pand	%7, %1\n\t"
		"paddd	%2, %1\n\t"

		"movq	%1, %2\n\t"
		"psrlq	$4, %1\n\t"
		"paddd	%2, %1\n\t"
		"pand	%8, %1\n\t"
	#ifdef hasSSE2
		"pxor	%2, %2\n\t"
		"psadbw	%2, %1\n\t"
		"movd	%1, %0\n\t"
	#else
		"movq	%1, %2\n\t"
		"psrlq	$32, %1\n\t"
		"paddb	%2, %1\n\t"

		"movd	%1, %0\n\t"
		"imull	$0x01010101, %0, %0\n\t"
		"shrl	$24, %0\n\t"
	#endif
		"emms"
	: "=g" (count)
	: "y" (P), "y" (O), "m" (mask_7e), "m" (mask_v),
	  "m" (mask_15), "m" (mask_01), "m" (mask_33), "m" (mask_0F)
	: "mm2", "mm3", "mm4", "mm5", "mm6");
#endif

	return count;
}

/**
 * @brief MMX translation of board_get_hash_code.
 *
 * @param p pointer to 16 bytes to hash.
 * @return the hash code of the bitboard
 */
#ifdef __3dNOW__

unsigned long long board_get_hash_code_mmx(const unsigned char *p)
{
	unsigned long long h;

	__asm__ volatile (
		"movq	%0, %%mm0\n\t"		"movq	%1, %%mm1"
	: : "m" (hash_rank[0][p[0]]), "m" (hash_rank[1][p[1]]));
	__asm__ volatile (
		"pxor	%0, %%mm0\n\t"		"pxor	%1, %%mm1"
	: : "m" (hash_rank[2][p[2]]), "m" (hash_rank[3][p[3]]));
	__asm__ volatile (
		"pxor	%0, %%mm0\n\t"		"pxor	%1, %%mm1"
	: : "m" (hash_rank[4][p[4]]), "m" (hash_rank[5][p[5]]));
	__asm__ volatile (
		"pxor	%0, %%mm0\n\t"		"pxor	%1, %%mm1"
	: : "m" (hash_rank[6][p[6]]), "m" (hash_rank[7][p[7]]));
	__asm__ volatile (
		"pxor	%0, %%mm0\n\t"		"pxor	%1, %%mm1"
	: : "m" (hash_rank[8][p[8]]), "m" (hash_rank[9][p[9]]));
	__asm__ volatile (
		"pxor	%0, %%mm0\n\t"		"pxor	%1, %%mm1"
	: : "m" (hash_rank[10][p[10]]), "m" (hash_rank[11][p[11]]));
	__asm__ volatile (
		"pxor	%0, %%mm0\n\t"		"pxor	%1, %%mm1"
	: : "m" (hash_rank[12][p[12]]), "m" (hash_rank[13][p[13]]));
	__asm__ volatile (
		"pxor	%1, %%mm0\n\t"		"pxor	%2, %%mm1\n\t"
		"pxor	%%mm1, %%mm0\n\t"
		"movd	%%mm0, %%eax\n\t"
		"punpckhdq %%mm0, %%mm0\n\t"
		"movd	%%mm0, %%edx\n\t"
		"emms"
	: "=A" (h)
	: "m" (hash_rank[14][p[14]]), "m" (hash_rank[15][p[15]])
	: "mm0", "mm1");

	return h;
}

#endif

#ifndef hasMMX
	#pragma GCC pop_options
#endif