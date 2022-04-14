/**
 * @file board_mmx.c
 *
 * MMX translation of some board.c functions for X86-32
 *
 * If both hasMMX and hasSSE2 are undefined, dynamic dispatching code
 * will be generated.  (This setting requires VC or GCC 4.4+)
 *
 * @date 2014 - 2022
 * @author Toshihiko Okuhara
 * @version 4.5
 */

#include "bit.h"
#include "hash.h"
#include "board.h"
#include "move.h"

#if !defined(hasSSE2) && defined(USE_GAS_MMX)
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
	int	flg1, flg2, cpuid_edx, cpuid_ecx;
#ifdef USE_MSVC_X86
	int	cpuinfo[4];

	__asm {
		pushfd
		pop	eax
		mov	flg2, eax
		btc	eax, 21
		push	eax
		popfd
		pushfd
		pop	flg1
	}

	if (flg1 == flg2)	/* CPUID not supported */
		return;

	__cpuid(cpuinfo, 1);
	cpuid_edx = cpuinfo[3];
	cpuid_ecx = cpuinfo[2];

#else
	__asm__ (
		"pushfl\n\t"
		"popl	%0\n\t"
		"movl	%0, %1\n\t"
		"btc	$21, %0\n\t"	/* flip ID bit in EFLAGS */
		"pushl	%0\n\t"
		"popfl\n\t"
		"pushfl\n\t"
		"popl	%0"
	: "=r" (flg1), "=r" (flg2) );

	if (flg1 == flg2)	/* CPUID not supported */
		return;

	__asm__ (
		"movl	$1, %%eax\n\t"
		"cpuid"
	: "=d" (cpuid_edx), "=c" (cpuid_ecx) :: "%eax", "%ebx" );

#endif

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

#ifdef hasMMX
/**
 * @brief Update a board.
 *
 * Update a board by flipping its discs and updating every other data,
 * according to the 'move' description.
 *
 * @param board the board to modify
 * @param move  A Move structure describing the modification.
 */
#if defined(hasSSE2) && !defined(__3dNOW__)	// Faster on CPU with slow emms

void board_update(Board *board, const Move *move)
{
	__m128i	F = _mm_loadl_epi64((__m128i *) &move->flipped);
	__m128i	OP = _mm_loadu_si128((__m128i *) board);
	OP = _mm_xor_si128(OP, _mm_or_si128(_mm_unpacklo_epi64(F, F), _mm_loadl_epi64((__m128i *) &X_TO_BIT[move->x])));
	_mm_storel_pi((__m64 *) &board->opponent, _mm_castsi128_ps(OP));
	_mm_storeh_pi((__m64 *) &board->player, _mm_castsi128_ps(OP));
	board_check(board);
}

#elif defined(USE_MSVC_X86)

void board_update(Board *board, const Move *move)
{
	__m64	F = *(__m64 *) &move->flipped;
	__m64	P = _m_pxor(*(__m64 *) &board->player, _m_por(F, *(__m64 *) &X_TO_BIT[move->x]));
	__m64	O = _m_pxor(*(__m64 *) &board->opponent, F);
	*(__m64 *) &board->player = O;
	*(__m64 *) &board->opponent = P;
	_mm_empty();
	board_check(board);
}

#else

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
	: "m" (move->flipped), "m" (X_TO_BIT[move->x])
	: "mm0", "mm1");
	board_check(board);
}

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
#if defined(hasSSE2) && !defined(__3dNOW__)

void board_restore(Board *board, const Move *move)
{
	__m128i	F = _mm_loadl_epi64((__m128i *) &move->flipped);
	__m128i	OP = _mm_unpacklo_epi64(_mm_loadl_epi64((__m128i *) &board->opponent), _mm_loadl_epi64((__m128i *) &board->player));
	OP = _mm_xor_si128(OP, _mm_or_si128(_mm_unpacklo_epi64(F, F), _mm_loadl_epi64((__m128i *) &X_TO_BIT[move->x])));
	_mm_storeu_si128((__m128i *) board, OP);
	board_check(board);
}

#elif defined(USE_MSVC_X86)

void board_restore(Board *board, const Move *move)
{
	__m64	F = *(__m64 *) &move->flipped;
	__m64	P = *(__m64 *) &board->opponent;
	__m64	O = *(__m64 *) &board->player;
	*(__m64 *) &board->player = _m_pxor(P, _m_por(F, *(__m64 *) &X_TO_BIT[move->x]));
	*(__m64 *) &board->opponent = _m_pxor(O, F);
	_mm_empty();
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
	: "m" (move->flipped), "m" (X_TO_BIT[move->x]))
	: "mm0", "mm1");
	board_check(board);
}

#endif
#endif // hasMMX

/**
 * @brief MMX translation of get_moves
 *
 * x 2 faster bench mobility on 32-bit x86.
 *
 */
#ifdef USE_MSVC_X86

unsigned long long get_moves_mmx(unsigned long long P_, unsigned long long O_)
{
	unsigned int movesL, movesH, mO1, flip1, pre1;
	__m64	P, O, M, mO, flip, pre;

	P = _m_punpckldq(_m_from_int(P_), _m_from_int(P_ >> 32));
	O = _m_punpckldq(_m_from_int(O_), _m_from_int(O_ >> 32));	mO1 = (unsigned int) O_ & 0x7e7e7e7e;
		/* shift = +8 */						/* shift = +1 */
	flip = _m_pand(O, _m_psllqi(P, 8));				flip1  = mO1 & ((unsigned int) P_ << 1);
	flip = _m_por(flip, _m_pand(O, _m_psllqi(flip, 8)));		flip1 |= mO1 & (flip1 << 1);
	pre  = _m_pand(O, _m_psllqi(O, 8));				pre1   = mO1 & (mO1 << 1);
	flip = _m_por(flip, _m_pand(pre, _m_psllqi(flip, 16)));		flip1 |= pre1 & (flip1 << 2);
	flip = _m_por(flip, _m_pand(pre, _m_psllqi(flip, 16)));		flip1 |= pre1 & (flip1 << 2);
	M = _m_psllqi(flip, 8);						movesL = flip1 << 1;
		/* shift = -8 */						/* shift = -1 */
	flip = _m_pand(O, _m_psrlqi(P, 8));				flip1  = mO1 & ((unsigned int) P_ >> 1);
	flip = _m_por(flip, _m_pand(O, _m_psrlqi(flip, 8)));		flip1 |= mO1 & (flip1 >> 1);
	pre  = _m_psrlqi(pre, 8);					pre1 >>= 1;
	flip = _m_por(flip, _m_pand(pre, _m_psrlqi(flip, 16)));		flip1 |= pre1 & (flip1 >> 2);
	flip = _m_por(flip, _m_pand(pre, _m_psrlqi(flip, 16)));		flip1 |= pre1 & (flip1 >> 2);
	M = _m_por(M, _m_psrlqi(flip, 8));				movesL |= flip1 >> 1;
		/* shift = +7 */
	mO = _m_pand(O, *(__m64 *) &mask_7e);				mO1 = (unsigned int)(O_ >> 32) & 0x7e7e7e7e;
	flip = _m_pand(mO, _m_psllqi(P, 7));
	flip = _m_por(flip, _m_pand(mO, _m_psllqi(flip, 7)));
	pre  = _m_pand(mO, _m_psllqi(mO, 7));
	flip = _m_por(flip, _m_pand(pre, _m_psllqi(flip, 14)));
	flip = _m_por(flip, _m_pand(pre, _m_psllqi(flip, 14)));
	M = _m_por(M, _m_psllqi(flip, 7));
		/* shift = -7 */						/* shift = +1 */
	flip = _m_pand(mO, _m_psrlqi(P, 7));				flip1  = mO1 & ((unsigned int)(P_ >> 32) << 1);
	flip = _m_por(flip, _m_pand(mO, _m_psrlqi(flip, 7)));		flip1 |= mO1 & (flip1 << 1);
	pre  = _m_psrlqi(pre, 7);					pre1   = mO1 & (mO1 << 1);
	flip = _m_por(flip, _m_pand(pre, _m_psrlqi(flip, 14)));		flip1 |= pre1 & (flip1 << 2);
	flip = _m_por(flip, _m_pand(pre, _m_psrlqi(flip, 14)));		flip1 |= pre1 & (flip1 << 2);
	M = _m_por(M, _m_psrlqi(flip, 7));				movesH = flip1 << 1;
		/* shift = +9 */						/* shift = -1 */
	flip = _m_pand(mO, _m_psllqi(P, 9));				flip1  = mO1 & ((unsigned int)(P_ >> 32) >> 1);
	flip = _m_por(flip, _m_pand(mO, _m_psllqi(flip, 9)));		flip1 |= mO1 & (flip1 >> 1);
	pre  = _m_pand(mO, _m_psllqi(mO, 9));				pre1 >>= 1;
	flip = _m_por(flip, _m_pand(pre, _m_psllqi(flip, 18)));		flip1 |= pre1 & (flip1 >> 2);
	flip = _m_por(flip, _m_pand(pre, _m_psllqi(flip, 18)));		flip1 |= pre1 & (flip1 >> 2);
	M = _m_por(M, _m_psllqi(flip, 9));				movesH |= flip1 >> 1;
		/* shift = -9 */
	flip = _m_pand(mO, _m_psrlqi(P, 9));
	flip = _m_por(flip, _m_pand(mO, _m_psrlqi(flip, 9)));
	pre  = _m_psrlqi(pre, 9);
	flip = _m_por(flip, _m_pand(pre, _m_psrlqi(flip, 18)));
	flip = _m_por(flip, _m_pand(pre, _m_psrlqi(flip, 18)));
	M = _m_por(M, _m_psrlqi(flip, 9));

	movesL |= _m_to_int(M);
	movesH |= _m_to_int(_m_punpckhdq(M, M));
	_mm_empty();
	return (((unsigned long long) movesH << 32) | movesL) & ~(P_|O_);	// mask with empties
}

#else

unsigned long long get_moves_mmx(unsigned long long P, unsigned long long O)
{
	unsigned long long moves;
	__asm__ (
		"movl	%1, %%ebx\n\t"		"movd	%1, %%mm4\n\t"		// (movd for store-forwarding)
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
								/* shift=-7 */
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
	: "m" (P), "m" (((unsigned int *)&P)[1]), "m" (O), "m" (((unsigned int *)&O)[1]), "m" (mask_7e)
	: "ebx", "ecx", "esi", "edi", "mm0", "mm1", "mm2", "mm3", "mm4", "mm5" );

	return moves;
}
#endif

/**
 * @brief MMX translation of get_stability()
 *
 * x 1.5 faster bench stability on 32-bit x86.
 *
 */
#ifdef USE_MSVC_X86

unsigned long long get_all_full_lines_mmx(const unsigned long long disc_, V4DI *full)
{
	__m64	disc = *(__m64 *) &disc_;
	__m64	full_v, full_l, full_r;
	static const unsigned long long MFF = 0xffffffffffffffff;
	static const unsigned long long edge = 0xff818181818181ffULL;
	static const unsigned long long e7[] = { 0xffff030303030303, 0xc0c0c0c0c0c0ffff, 0xffffffff0f0f0f0f, 0xf0f0f0f0ffffffff };
	static const unsigned long long e9[] = { 0xffffc0c0c0c0c0c0, 0x030303030303ffff, 0x0f0f0f0ff0f0f0f0 };

	// get_full_lines_mmx(full_h, disc, 1, e1);
	full->v1[0] = _m_pcmpeqb(*(__m64 *) &MFF, disc);

	// get_full_lines_mmx(full_v, disc, 8, e8);
	full_v = _m_pand(_m_punpcklbw(disc, disc), _m_punpckhbw(disc, disc));	//  (d,d,c,c,b,b,a,a) & (h,h,g,g,f,f,e,e)
	full_v = _m_pand(_m_punpcklwd(full_v, full_v), _m_punpckhwd(full_v, full_v));	// (dh,dh,dh,dh,cg,cg,cg,cg) & (bf,bf,bf,bf,ae,ae,ae,ae)
	full->v1[1] = _m_pand(_m_punpckldq(full_v, full_v), _m_punpckhdq(full_v, full_v));	// (bdfh*4, bdfh*4) & (aceg*4, aceg*4)

	// get_full_lines_mmx(full_d7, disc, 7, e7);
	full_l = _m_pand(disc, _m_por(*(__m64 *) &edge, _m_psrlqi(disc, 7)));
	full_r = _m_pand(disc, _m_por(*(__m64 *) &edge, _m_psllqi(disc, 7)));
	full_l = _m_pand(full_l, _m_por(*(__m64 *) &e7[0], _m_psrlqi(full_l, 14)));
	full_r = _m_pand(full_r, _m_por(*(__m64 *) &e7[1], _m_psllqi(full_r, 14)));
	full_l = _m_pand(full_l, _m_por(*(__m64 *) &e7[2], _m_psrlqi(full_l, 28)));
	full_r = _m_pand(full_r, _m_por(*(__m64 *) &e7[3], _m_psllqi(full_r, 28)));
	full->v1[3] = _m_pand(full_l, full_r);

	// get_full_lines_mmx(full_d9, disc, 9, e9);
	full_l = _m_pand(disc, _m_por(*(__m64 *) &edge, _m_psrlqi(disc, 9)));
	full_r = _m_pand(disc, _m_por(*(__m64 *) &edge, _m_psllqi(disc, 9)));
	full_l = _m_pand(full_l, _m_por(*(__m64 *) &e9[0], _m_psrlqi(full_l, 18)));
	full_r = _m_pand(full_r, _m_por(*(__m64 *) &e9[1], _m_psllqi(full_r, 18)));
	full->v1[2] = _m_pand(_m_pand(full_l, full_r), _m_por(*(__m64 *) &e9[2], _m_por(_m_psrlqi(full_l, 36), _m_psllqi(full_r, 36))));
	_mm_empty();

	return full->ull[0] & full->ull[1] & full->ull[2] & full->ull[3];
}

int get_stability_mmx(unsigned long long P, unsigned long long O)
{
	V4DI	full;
	unsigned long long allfull;
	__m64	P_central, stable, stable_h, stable_v, stable_d7, stable_d9, old_stable, m;
	unsigned int	OL, OH, PL, PH, t, a1a8, h1h8, SL, SH;

	allfull = get_all_full_lines_mmx(P | O, &full);

	// compute the exact stable edges (from precomputed tables)
	OL = (unsigned int) O;	OH = (unsigned int)(O >> 32);
	PL = (unsigned int) P;	PH = (unsigned int)(P >> 32);
	a1a8 = edge_stability[((((PL & 0x01010101) + ((PH & 0x01010101) << 4)) * 0x01020408) >> 24) * 256
		+ ((((OL & 0x01010101) + ((OH & 0x01010101) << 4)) * 0x01020408) >> 24)];
	h1h8 = edge_stability[((((PH & 0x80808080) + ((PL & 0x80808080) >> 4)) * 0x00204081) >> 24) * 256
		+ ((((OH & 0x80808080) + ((OL & 0x80808080) >> 4)) * 0x00204081) >> 24)];
	SL = edge_stability[(PL & 0xff) * 256 + (OL & 0xff)]
		| (((a1a8 & 0x0f) * 0x00204081) & 0x01010101)
		| (((h1h8 & 0x0f) * 0x10204080) & 0x80808080);
	SH = (edge_stability[((PH >> 16) & 0xff00) + (OH >> 24)] << 24)
		| (((a1a8 >> 4) * 0x00204081) & 0x01010101)
		| (((h1h8 >> 4) * 0x10204080) & 0x80808080);

	PL &= 0x7f7f7f00;
	PH &= 0x007f7f7f;
	P_central = _m_punpckldq(_m_from_int(PL), _m_from_int(PH));

	SL |= (unsigned int) allfull & PL;
	SH |= (unsigned int)(allfull >> 32) & PH;
	stable = _m_punpckldq(_m_from_int(SL), _m_from_int(SH));

	// now compute the other stable discs (ie discs touching another stable disc in each flipping direction).
	t = SL | SH;
	if (t) {
		do {
			old_stable = stable;
			stable_h = _m_por(_m_por(_m_psrlqi(stable, 1), _m_psllqi(stable, 1)), full.v1[0]);
			stable_v = _m_por(_m_por(_m_psrlqi(stable, 8), _m_psllqi(stable, 8)), full.v1[1]);
			stable_d7 = _m_por(_m_por(_m_psrlqi(stable, 7), _m_psllqi(stable, 7)), full.v1[3]);
			stable_d9 = _m_por(_m_por(_m_psrlqi(stable, 9), _m_psllqi(stable, 9)), full.v1[2]);
			stable = _m_por(stable, _m_pand(_m_pand(_m_pand(_m_pand(stable_h, stable_v), stable_d7), stable_d9), P_central));
			m = _m_pxor(stable, old_stable);
		} while (_m_to_int(_m_packsswb(m, m)) != 0);

#ifdef POPCOUNT
		t = __popcnt(_m_to_int(stable)) + __popcnt(_m_to_int(_m_psrlqi(stable, 32)));
#else
		m = _m_psubd(stable, _m_pand(_m_psrlqi(stable, 1), *(__m64 *) &mask_55));
		m = _m_paddd(_m_pand(m, *(__m64 *) &mask_33), _m_pand(_m_psrlqi(m, 2), *(__m64 *) &mask_33));
		m = _m_pand(_m_paddd(m, _m_psrlqi(m, 4)), *(__m64 *) &mask_0F);
		t = ((unsigned int) _m_to_int(_m_paddb(m, _m_psrlqi(m, 32))) * 0x01010101u) >> 24;
#endif
	}
	_mm_empty();
	return t;
}

#elif defined(USE_GAS_MMX) && !(defined(__clang__) && (__clang__major__ < 3))
// LLVM ERROR: Unsupported asm: input constraint with a matching output constraint of incompatible type!

#define	get_full_lines_mmx(result,disc,dir,edge)	__asm__ (\
		"movq	%1, %%mm0\n\t"		"movq	%1, %%mm1\n\t"\
		"psrlq	%2, %%mm0\n\t"		"psllq	%2, %%mm1\n\t"\
		"por	%5, %%mm0\n\t"		"por	%5, %%mm1\n\t"\
		"pand	%1, %%mm0\n\t"		"pand	%1, %%mm1\n\t"\
		"movq	%%mm0, %%mm2\n\t"	"movq	%%mm1, %%mm3\n\t"\
		"psrlq	%3, %%mm0\n\t"		"psllq	%3, %%mm1\n\t"\
		"por	%6, %%mm0\n\t"		"por	%7, %%mm1\n\t"\
		"pand	%%mm2, %%mm0\n\t"	"pand	%%mm3, %%mm1\n\t"\
		"movq	%%mm0, %%mm2\n\t"	"pand	%%mm1, %%mm0\n\t"\
		"psrlq	%4, %%mm2\n\t"		"psllq	%4, %%mm1\n\t"\
		"por	%8, %%mm2\n\t"		"por	%9, %%mm1\n\t"\
		"pand	%%mm2, %%mm0\n\t"	"pand	%%mm1, %%mm0\n\t"\
		"movq	%%mm0, %0"\
	: "=m" (result)\
	: "y" (disc), "i" (dir), "i" (dir * 2), "i" (dir * 4),\
	  "my" (e0), "m" (edge[0]), "m" (edge[1]), "m" (edge[2]), "m" (edge[3])\
	: "mm0", "mm1", "mm2", "mm3");

unsigned long long get_all_full_lines_mmx(const unsigned long long disc_, V4DI *full)
{
	__m64	disc;
	static const unsigned long long e0 = 0xff818181818181ffULL;
	static const unsigned long long e7[] = { 0xffff030303030303, 0xc0c0c0c0c0c0ffff, 0xffffffff0f0f0f0f, 0xf0f0f0f0ffffffff };
	static const unsigned long long e9[] = { 0xffffc0c0c0c0c0c0, 0x030303030303ffff, 0xfffffffff0f0f0f0, 0x0f0f0f0fffffffff };

	__asm__ (
		"movd	%1, %0\n\t"
		"punpckldq %2, %0\n\t"
	: "=&y" (disc) : "m" (disc_), "m" (((unsigned int *)&disc_)[1]));

	get_full_lines_mmx(full->ull[3], disc, 7, e7);
	get_full_lines_mmx(full->ull[2], disc, 9, e9);

	// get_full_lines_mmx(full_h, disc, 1, e1);
	__asm__ (
		"pcmpeqb %%mm0, %%mm0\n\t"
		"pcmpeqb %1, %%mm0\n\t"
		"movq	%%mm0, %0"
	: "=m" (full->ull[0]) : "y" (disc) : "mm0");

	// get_full_lines_mmx(full_v, disc, 8, e8);
	__asm__ (
		"movq	%1, %%mm0\n\t"		"movq	%1, %%mm1\n\t"
		"punpcklbw %%mm0, %%mm0\n\t"	"punpckhbw %%mm1, %%mm1\n\t"
		"pand	%%mm1, %%mm0\n\t"	// (d,d,c,c,b,b,a,a) & (h,h,g,g,f,f,e,e)
		"movq	%%mm0, %%mm1\n\t"
		"punpcklwd %%mm0, %%mm0\n\t"	"punpckhwd %%mm1, %%mm1\n\t"
		"pand	%%mm1, %%mm0\n\t"	// (dh,dh,dh,dh,cg,cg,cg,cg) & (bf,bf,bf,bf,ae,ae,ae,ae)
		"movq	%%mm0, %%mm1\n\t"
		"punpckldq %%mm0, %%mm0\n\t"	"punpckhdq %%mm1, %%mm1\n\t"
		"pand	%%mm1, %%mm0\n\t"	// (bdfh*4, bdfh*4) & (aceg*4, aceg*4)
		"movq	%%mm0, %0\n\t"
		"emms"
	: "=m" (full->ull[1]) : "y" (disc) : "mm0", "mm1");

	return full->ull[0] & full->ull[1] & full->ull[2] & full->ull[3];
}

int get_stability_mmx(unsigned long long P, unsigned long long O)
{
	V4DI	full;
	unsigned long long allfull;
	__m64	P_central, stable;
	unsigned int	OL, OH, PL, PH, t, a1a8, h1h8, SL, SH;

	allfull = get_all_full_lines_mmx(P | O, &full);

	// compute the exact stable edges (from precomputed tables)
	OL = (unsigned int) O;	OH = (unsigned int)(O >> 32);
	PL = (unsigned int) P;	PH = (unsigned int)(P >> 32);
	a1a8 = edge_stability[((((PL & 0x01010101u) + ((PH & 0x01010101u) << 4)) * 0x01020408u) >> 24) * 256
		+ ((((OL & 0x01010101u) + ((OH & 0x01010101u) << 4)) * 0x01020408u) >> 24)];
	h1h8 = edge_stability[((((PH & 0x80808080u) + ((PL & 0x80808080u) >> 4)) * 0x00204081u) >> 24) * 256
		+ ((((OH & 0x80808080u) + ((OL & 0x80808080u) >> 4)) * 0x00204081u) >> 24)];
	SL = edge_stability[(PL & 0xff) * 256 + (OL & 0xff)]
		| (((a1a8 & 0x0f) * 0x00204081) & 0x01010101)
		| (((h1h8 & 0x0f) * 0x10204080) & 0x80808080);
	SH = (edge_stability[((PH >> 16) & 0xff00) + (OH >> 24)] << 24)
		| (((a1a8 >> 4) * 0x00204081) & 0x01010101)
		| (((h1h8 >> 4) * 0x10204080) & 0x80808080);

	PL &= 0x7f7f7f00;
	PH &= 0x007f7f7f;

	__asm__(
		"movd	%2, %0\n\t"		"movd	%4, %1\n\t"
		"movd	%3, %%mm0\n\t"		"movd	%5, %%mm1\n\t"
		"punpckldq %%mm0, %0\n\t"	"punpckldq %%mm1, %1\n\t"
	: "=y" (P_central), "=y" (stable)
	: "g" (PL), "g" (PH), "g" (SL | ((unsigned int) allfull & PL)), "g" (SH | ((unsigned int)(allfull >> 32) & PH))
	: "mm0", "mm1" );

	// now compute the other stable discs (ie discs touching another stable disc in each flipping direction).
	__asm__ (
		"movq	%1, %%mm0\n\t"
		"packsswb %%mm0, %%mm0\n\t"
		"movd	%%mm0, %0\n\t"
	: "=g" (t) : "y" (stable) : "mm0" );

	if (t) {
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
			: "m" (full.ull[0]), "m" (full.ull[1]), "m" (full.ull[3]), "m" (full.ull[2]), "m" (P_central)
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
			"addl	%%edx, %0"
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
			"shrl	$24, %0"
	#endif
		: "=a" (t) : "y" (stable), "m" (mask_55), "my" (mask_33), "m" (mask_0F) : "mm0", "mm1");
#endif
	}
	__asm__ ( "emms" );
	return t;
}
#endif // USE_MSVC_X86

/**
 * @brief MMX translation of get_potential_mobility
 *
 * @param P bitboard with player's discs.
 * @param O bitboard with opponent's discs.
 * @return a count of potential moves.
 */
#ifdef USE_MSVC_X86

int get_potential_mobility_mmx(unsigned long long P, unsigned long long O)
{
	__m64	m, mO;
	int	count;
	static const unsigned long long mask_v = 0x00ffffffffffff00ULL;
	// static const unsigned long long mask_d = 0x007e7e7e7e7e7e00ULL;	// = mask_7e & mask_v
#ifdef POPCOUNT
	int	mh, ml;
#else
	static const unsigned long long mask_15 = 0x1555555555555515ULL;
	static const unsigned long long mask_01 = 0x0100000000000001ULL;
#endif

	mO = _m_pand(*(__m64 *) &O, *(__m64 *) &mask_7e);
	m = _m_por(_m_psllqi(mO, 1), _m_psrlqi(mO, 1));
	mO = _m_pand(*(__m64 *) &O, *(__m64 *) &mask_v);
	m = _m_por(m, _m_por(_m_psllqi(mO, 8), _m_psrlqi(mO, 8)));
	mO = _m_pand(mO, *(__m64 *) &mask_7e);
	m = _m_por(m, _m_por(_m_psllqi(mO, 7), _m_psrlqi(mO, 7)));
	m = _m_por(m, _m_por(_m_psllqi(mO, 9), _m_psrlqi(mO, 9)));
	m = _m_pandn(_m_por(*(__m64 *) &O, *(__m64 *) &P), m);

#ifdef POPCOUNT
	ml = _m_to_int(m);
	mh = _m_to_int(_m_psrlqi(m, 32));
	count = __popcnt(ml) + __popcnt(mh) + __popcnt((ml & 0x00000081) + (mh & 0x81000000));
#else
	m = _m_paddd(_m_psubd(m, _m_pand(_m_psrlqi(m, 1), *(__m64 *) &mask_15)), _m_pand(m, *(__m64 *) &mask_01));
	m = _m_paddd(_m_pand(m, *(__m64 *) &mask_33), _m_pand(_m_psrlqi(m, 2), *(__m64 *) &mask_33));
	m = _m_pand(_m_paddd(m, _m_psrlqi(m, 4)), *(__m64 *) &mask_0F);
	count = ((unsigned int) _m_to_int(_m_paddb(m, _m_psrlqi(m, 32))) * 0x01010101u) >> 24;
#endif
	_mm_empty();
	return count;
}

#elif defined(USE_GAS_MMX)

int get_potential_mobility_mmx(unsigned long long P, unsigned long long O)
{
	int	count;
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
#endif

#if !defined(hasMMX) && defined(USE_GAS_MMX)
	#pragma GCC pop_options
#endif
