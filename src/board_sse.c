/**
 * @file board_sse.c
 *
 * SSE/AVX translation of some board.c functions
 *
 * @date 2014 - 2018
 * @author Toshihiko Okuhara
 * @version 4.4
 */

#include "bit.h"
#include "hash.h"
#include "board.h"

/**
 * @brief SSE2 translation of board_symetry
 *
 * @param board input board
 * @param s symetry
 * @param sym symetric output board
 */
#ifdef hasSSE2

void board_symetry(const Board *board, const int s, Board *sym)
{
	__m128i	bb = _mm_loadu_si128((__m128i *) board);
	__m128i	tt;
	static const V2DI mask0F0F = {{ 0x0F0F0F0F0F0F0F0FULL, 0x0F0F0F0F0F0F0F0FULL }};
	static const V2DI mask00AA = {{ 0x00AA00AA00AA00AAULL, 0x00AA00AA00AA00AAULL }};
	static const V2DI maskCCCC = {{ 0x0000CCCC0000CCCCULL, 0x0000CCCC0000CCCCULL }};
	static const V2DI mask00F0 = {{ 0x00000000F0F0F0F0ULL, 0x00000000F0F0F0F0ULL }};
#if defined(__SSSE3__) || defined(__AVX__)	// pshufb
	static const V2DI mbswapll = {{ 0x0001020304050607ULL, 0x08090A0B0C0D0E0FULL }};
	static const V2DI mbitrev  = {{ 0x0E060A020C040800ULL, 0x0F070B030D050901ULL }};

	if (s & 1) {	// horizontal_mirror (cf. http://wm.ite.pl/articles/sse-popcount.html)
		bb = _mm_or_si128(_mm_shuffle_epi8(mbitrev.v2, _mm_and_si128(_mm_srli_epi64(bb, 4), mask0F0F.v2)),
			_mm_slli_epi64(_mm_shuffle_epi8(mbitrev.v2, _mm_and_si128(bb, mask0F0F.v2)), 4));
	}

	if (s & 2) {	// vertical_mirror
		bb = _mm_shuffle_epi8(bb, mbswapll.v2);
	}

#else
	static const V2DI mask5555 = {{ 0x5555555555555555ULL, 0x5555555555555555ULL }};
	static const V2DI mask3333 = {{ 0x3333333333333333ULL, 0x3333333333333333ULL }};

	if (s & 1) {	// horizontal_mirror
		bb = _mm_or_si128(_mm_and_si128(_mm_srli_epi64(bb, 1), mask5555.v2), _mm_slli_epi64(_mm_and_si128(bb, mask5555.v2), 1));
		bb = _mm_or_si128(_mm_and_si128(_mm_srli_epi64(bb, 2), mask3333.v2), _mm_slli_epi64(_mm_and_si128(bb, mask3333.v2), 2));
		bb = _mm_or_si128(_mm_and_si128(_mm_srli_epi64(bb, 4), mask0F0F.v2), _mm_slli_epi64(_mm_and_si128(bb, mask0F0F.v2), 4));
	}

	if (s & 2) {	// vertical_mirror
		bb = _mm_or_si128(_mm_srli_epi16(bb, 8), _mm_slli_epi16(bb, 8));
		bb = _mm_shufflehi_epi16(_mm_shufflelo_epi16(bb, 0x1b), 0x1b);
	}
#endif

	if (s & 4) {	// transpose
		tt = _mm_and_si128(_mm_xor_si128(bb, _mm_srli_epi64(bb, 7)), mask00AA.v2);
		bb = _mm_xor_si128(_mm_xor_si128(bb, tt), _mm_slli_epi64(tt, 7));
		tt = _mm_and_si128(_mm_xor_si128(bb, _mm_srli_epi64(bb, 14)), maskCCCC.v2);
		bb = _mm_xor_si128(_mm_xor_si128(bb, tt), _mm_slli_epi64(tt, 14));
		tt = _mm_and_si128(_mm_xor_si128(bb, _mm_srli_epi64(bb, 28)), mask00F0.v2);
		bb = _mm_xor_si128(_mm_xor_si128(bb, tt), _mm_slli_epi64(tt, 28));
	}

#ifdef __clang__
	sym->player = bb[0];
	sym->opponent = bb[1];
#else	// error on clang 3.8
	_mm_storeu_si128((__m128i *) sym, bb);
#endif

	board_check(sym);
}

#endif // hasSSE2

/**
 * @brief X64 optimized get_moves
 *
 * Diag-7 is converted to diag-9 (v.v.) using vertical mirroring
 * in SSE versions.
 *
 * @param P bitboard with player's discs.
 * @param O bitboard with opponent's discs.
 * @return all legal moves in a 64-bit unsigned integer.
 */
#ifdef __AVX2__	// 4 AVX

unsigned long long get_moves(const unsigned long long P, const unsigned long long O)
{
	__m256i	PP, mOO, MM, flip_l, flip_r, pre_l, pre_r, shift2;
	__m128i	M;
	static const V4DI shift1897 = {{ 1, 8, 9, 7 }};
	static const V4DI mflipH = {{ 0x7e7e7e7e7e7e7e7e, 0xffffffffffffffff, 0x7e7e7e7e7e7e7e7e, 0x7e7e7e7e7e7e7e7e }};

	PP = _mm256_broadcastq_epi64(_mm_cvtsi64_si128(P));
	mOO = _mm256_and_si256(_mm256_broadcastq_epi64(_mm_cvtsi64_si128(O)), mflipH.v4);

	flip_l = _mm256_and_si256(mOO, _mm256_sllv_epi64(PP, shift1897.v4));
	flip_r = _mm256_and_si256(mOO, _mm256_srlv_epi64(PP, shift1897.v4));
	flip_l = _mm256_or_si256(flip_l, _mm256_and_si256(mOO, _mm256_sllv_epi64(flip_l, shift1897.v4)));
	flip_r = _mm256_or_si256(flip_r, _mm256_and_si256(mOO, _mm256_srlv_epi64(flip_r, shift1897.v4)));
	pre_l = _mm256_and_si256(mOO, _mm256_sllv_epi64(mOO, shift1897.v4));
	pre_r = _mm256_srlv_epi64(pre_l, shift1897.v4);
	shift2 = _mm256_add_epi64(shift1897.v4, shift1897.v4);
	flip_l = _mm256_or_si256(flip_l, _mm256_and_si256(pre_l, _mm256_sllv_epi64(flip_l, shift2)));
	flip_r = _mm256_or_si256(flip_r, _mm256_and_si256(pre_r, _mm256_srlv_epi64(flip_r, shift2)));
	flip_l = _mm256_or_si256(flip_l, _mm256_and_si256(pre_l, _mm256_sllv_epi64(flip_l, shift2)));
	flip_r = _mm256_or_si256(flip_r, _mm256_and_si256(pre_r, _mm256_srlv_epi64(flip_r, shift2)));
	MM = _mm256_sllv_epi64(flip_l, shift1897.v4);
	MM = _mm256_or_si256(MM, _mm256_srlv_epi64(flip_r, shift1897.v4));

	M = _mm_or_si128(_mm256_castsi256_si128(MM), _mm256_extracti128_si256(MM, 1));
	M = _mm_or_si128(M, _mm_unpackhi_epi64(M, M));
	return _mm_cvtsi128_si64(M) & ~(P|O);	// mask with empties
}

#elif defined(__x86_64__) || defined(_M_X64)	// 2 SSE, 2 CPU

unsigned long long get_moves(const unsigned long long P, const unsigned long long O)
{
	unsigned long long moves, mO, flip1, pre1, flip8, pre8;
	__m128i	PP, mOO, MM, flip, pre;

	mO = O & 0x7e7e7e7e7e7e7e7eULL;
	PP  = _mm_set_epi64x(vertical_mirror(P), P);
	mOO = _mm_set_epi64x(vertical_mirror(mO), mO);
		/* shift=-9:+7 */								/* shift=+1 */			/* shift = +8 */
	flip = _mm_and_si128(mOO, _mm_slli_epi64(PP, 7));				flip1  = mO & (P << 1);		flip8  = O & (P << 8);
	flip = _mm_or_si128(flip, _mm_and_si128(mOO, _mm_slli_epi64(flip, 7)));		flip1 |= mO & (flip1 << 1);	flip8 |= O & (flip8 << 8);
	pre  = _mm_and_si128(mOO, _mm_slli_epi64(mOO, 7));				pre1   = mO & (mO << 1);	pre8   = O & (O << 8);
	flip = _mm_or_si128(flip, _mm_and_si128(pre, _mm_slli_epi64(flip, 14)));	flip1 |= pre1 & (flip1 << 2);	flip8 |= pre8 & (flip8 << 16);
	flip = _mm_or_si128(flip, _mm_and_si128(pre, _mm_slli_epi64(flip, 14)));	flip1 |= pre1 & (flip1 << 2);	flip8 |= pre8 & (flip8 << 16);
	MM = _mm_slli_epi64(flip, 7);							moves = flip1 << 1;		moves |= flip8 << 8;
		/* shift=-7:+9 */								/* shift=-1 */			/* shift = -8 */
	flip = _mm_and_si128(mOO, _mm_slli_epi64(PP, 9));				flip1  = mO & (P >> 1);		flip8  = O & (P >> 8);
	flip = _mm_or_si128(flip, _mm_and_si128(mOO, _mm_slli_epi64(flip, 9)));		flip1 |= mO & (flip1 >> 1);	flip8 |= O & (flip8 >> 8);
	pre = _mm_and_si128(mOO, _mm_slli_epi64(mOO, 9));				pre1 >>= 1;			pre8 >>= 8;
	flip = _mm_or_si128(flip, _mm_and_si128(pre, _mm_slli_epi64(flip, 18)));	flip1 |= pre1 & (flip1 >> 2);	flip8 |= pre8 & (flip8 >> 16);
	flip = _mm_or_si128(flip, _mm_and_si128(pre, _mm_slli_epi64(flip, 18)));	flip1 |= pre1 & (flip1 >> 2);	flip8 |= pre8 & (flip8 >> 16);
	MM = _mm_or_si128(MM, _mm_slli_epi64(flip, 9));					moves |= flip1 >> 1;		moves |= flip8 >> 8;

	moves |= _mm_cvtsi128_si64(MM) | vertical_mirror(_mm_cvtsi128_si64(_mm_unpackhi_epi64(MM, MM)));
	return moves & ~(P|O);	// mask with empties
}

#elif 0	// 4 CPU

unsigned long long get_moves(const unsigned long long P, const unsigned long long O)
{
	unsigned long long moves, mO;
	unsigned long long flip1, flip7, flip9, flip8, pre1, pre7, pre9, pre8;

	mO = O & 0x7e7e7e7e7e7e7e7eULL;
	flip1  = mO & (P << 1);		flip7  = mO & (P << 7);		flip9  = mO & (P << 9);		flip8  = O & (P << 8);
	flip1 |= mO & (flip1 << 1);	flip7 |= mO & (flip7 << 7);	flip9 |= mO & (flip9 << 9);	flip8 |= O & (flip8 << 8);
	pre1 = mO & (mO << 1);		pre7 = mO & (mO << 7);		pre9 = mO & (mO << 9);		pre8 = O & (O << 8);
	flip1 |= pre1 & (flip1 << 2);	flip7 |= pre7 & (flip7 << 14);	flip9 |= pre9 & (flip9 << 18);	flip8 |= pre8 & (flip8 << 16);
	flip1 |= pre1 & (flip1 << 2);	flip7 |= pre7 & (flip7 << 14);	flip9 |= pre9 & (flip9 << 18);	flip8 |= pre8 & (flip8 << 16);
	moves = flip1 << 1;		moves |= flip7 << 7;		moves |= flip9 << 9;		moves |= flip8 << 8;
	flip1  = mO & (P >> 1);		flip7  = mO & (P >> 7);		flip9  = mO & (P >> 9);		flip8  = O & (P >> 8);
	flip1 |= mO & (flip1 >> 1);	flip7 |= mO & (flip7 >> 7);	flip9 |= mO & (flip9 >> 9);	flip8 |= O & (flip8 >> 8);
	pre1 >>= 1;			pre7 >>= 7;			pre9 >>= 9;			pre8 >>= 8;
	flip1 |= pre1 & (flip1 >> 2);	flip7 |= pre7 & (flip7 >> 14);	flip9 |= pre9 & (flip9 >> 18);	flip8 |= pre8 & (flip8 >> 16);
	flip1 |= pre1 & (flip1 >> 2);	flip7 |= pre7 & (flip7 >> 14);	flip9 |= pre9 & (flip9 >> 18);	flip8 |= pre8 & (flip8 >> 16);
	moves |= flip1 >> 1;		moves |= flip7 >> 7;		moves |= flip9 >> 9;		moves |= flip8 >> 8;

	return moves & ~(P|O);	// mask with empties
}

#else // __x86_64__
/**
 * @brief SSE optimized get_moves for x86 (3 SSE, 1 CPU)
 *
 */
#if defined(hasSSE2) || defined(USE_MSVC_X86)

unsigned long long get_moves_sse(unsigned int PL, unsigned int PH, unsigned int OL, unsigned int OH)
{
	unsigned int	mO, movesL, movesH, flip1, pre1;
	__m128i	OP, rOP, PP, OO, MM, flip, pre;
	static const V2DI mask7e = {{ 0x7e7e7e7e7e7e7e7eULL, 0x7e7e7e7e7e7e7e7eULL }};

		// vertical_mirror in PP[1], OO[1]
	OP  = _mm_set_epi32(OH, OL, PH, PL);						mO = OL & 0x7e7e7e7eU;
	rOP = _mm_shufflelo_epi16(OP, 0x1B);						flip1  = mO & (PL << 1);
	rOP = _mm_shufflehi_epi16(rOP, 0x1B);						flip1 |= mO & (flip1 << 1);
											pre1   = mO & (mO << 1);
	rOP = _mm_or_si128(_mm_srli_epi16(rOP, 8), _mm_slli_epi16(rOP, 8));
	    										flip1 |= pre1 & (flip1 << 2);
	PP  = _mm_unpacklo_epi64(OP, rOP);						flip1 |= pre1 & (flip1 << 2);
	OO  = _mm_unpackhi_epi64(OP, rOP);						movesL = flip1 << 1;

	flip = _mm_and_si128(OO, _mm_slli_epi64(PP, 8));				flip1  = mO & (PL >> 1);
	flip = _mm_or_si128(flip, _mm_and_si128(OO, _mm_slli_epi64(flip, 8)));		flip1 |= mO & (flip1 >> 1);
	pre = _mm_and_si128(OO, _mm_slli_epi64(OO, 8));					pre1 >>= 1;
	flip = _mm_or_si128(flip, _mm_and_si128(pre, _mm_slli_epi64(flip, 16)));	flip1 |= pre1 & (flip1 >> 2);
	flip = _mm_or_si128(flip, _mm_and_si128(pre, _mm_slli_epi64(flip, 16)));	flip1 |= pre1 & (flip1 >> 2);
	MM = _mm_slli_epi64(flip, 8);							movesL |= flip1 >> 1;

	OO = _mm_and_si128(OO, mask7e.v2);						mO = OH & 0x7e7e7e7eU;
	flip = _mm_and_si128(OO, _mm_slli_epi64(PP, 7));				flip1  = mO & (PH << 1);
	flip = _mm_or_si128(flip, _mm_and_si128(OO, _mm_slli_epi64(flip, 7)));		flip1 |= mO & (flip1 << 1);
	pre = _mm_and_si128(OO, _mm_slli_epi64(OO, 7));					pre1   = mO & (mO << 1);
	flip = _mm_or_si128(flip, _mm_and_si128(pre, _mm_slli_epi64(flip, 14)));	flip1 |= pre1 & (flip1 << 2);
	flip = _mm_or_si128(flip, _mm_and_si128(pre, _mm_slli_epi64(flip, 14)));	flip1 |= pre1 & (flip1 << 2);
	MM = _mm_or_si128(MM, _mm_slli_epi64(flip, 7));					movesH = flip1 << 1;

	flip = _mm_and_si128(OO, _mm_slli_epi64(PP, 9));				flip1  = mO & (PH >> 1);
	flip = _mm_or_si128(flip, _mm_and_si128(OO, _mm_slli_epi64(flip, 9)));		flip1 |= mO & (flip1 >> 1);
	pre = _mm_and_si128(OO, _mm_slli_epi64(OO, 9));					pre1 >>= 1;
	flip = _mm_or_si128(flip, _mm_and_si128(pre, _mm_slli_epi64(flip, 18)));	flip1 |= pre1 & (flip1 >> 2);
	flip = _mm_or_si128(flip, _mm_and_si128(pre, _mm_slli_epi64(flip, 18)));	flip1 |= pre1 & (flip1 >> 2);
	MM = _mm_or_si128(MM, _mm_slli_epi64(flip, 9));					movesH |= flip1 >> 1;

	movesL |= _mm_cvtsi128_si32(MM);	MM = _mm_srli_si128(MM, 4);
	movesH |= _mm_cvtsi128_si32(MM);	MM = _mm_srli_si128(MM, 4);
	movesH |= bswap_int(_mm_cvtsi128_si32(MM));
	movesL |= bswap_int(_mm_cvtsi128_si32(_mm_srli_si128(MM, 4)));
	movesL &= ~(PL|OL);	// mask with empties
	movesH &= ~(PH|OH);
	return movesL | ((unsigned long long) movesH << 32);
}

#else // non-VEX asm

unsigned long long get_moves_sse(unsigned int PL, unsigned int PH, unsigned int OL, unsigned int OH)
{
	unsigned long long moves;
	static const V2DI mask7e = {{ 0x7e7e7e7e7e7e7e7eULL, 0x7e7e7e7e7e7e7e7eULL }};

	__asm__ (
		"movl	%1, %%ebx\n\t"
		"movl	%3, %%edi\n\t"
		"andl	$0x7e7e7e7e, %%edi\n\t"
				/* shift=-1 */			/* vertical mirror in PP[1], OO[1] */
		"movl	%%ebx, %%eax\n\t"	"movd	%1, %%xmm4\n\t"
		"shrl	$1, %%eax\n\t"		"movd	%2, %%xmm0\n\t"
		"andl	%%edi, %%eax\n\t"	"movd	%3, %%xmm5\n\t"
		"movl	%%eax, %%edx\n\t"	"movd	%4, %%xmm1\n\t"
		"shrl	$1, %%eax\n\t"		"punpckldq %%xmm0, %%xmm4\n\t"		// P
		"movl	%%edi, %%ecx\n\t"	"punpckldq %%xmm1, %%xmm5\n\t"		// O
		"andl	%%edi, %%eax\n\t"	"punpcklqdq %%xmm5, %%xmm4\n\t"		// OP
		"shrl	$1, %%ecx\n\t"		"pshuflw $0x1b, %%xmm4, %%xmm0\n\t"
		"orl	%%edx, %%eax\n\t"	"pshufhw $0x1b, %%xmm0, %%xmm0\n\t"
		"andl	%%edi, %%ecx\n\t"	"movdqa	%%xmm0, %%xmm1\n\t"
		"movl	%%eax, %%edx\n\t"	"psllw	$8, %%xmm0\n\t"
		"shrl	$2, %%eax\n\t"		"psrlw	$8, %%xmm1\n\t"
		"andl	%%ecx, %%eax\n\t"	"por	%%xmm1, %%xmm0\n\t"		// rOP
		"orl	%%eax, %%edx\n\t"
		"shrl	$2, %%eax\n\t"		"movdqa	%%xmm4, %%xmm5\n\t"
		"andl	%%ecx, %%eax\n\t"	"punpcklqdq %%xmm0, %%xmm4\n\t"		// PP
		"orl	%%edx, %%eax\n\t"	"punpckhqdq %%xmm0, %%xmm5\n\t"		// OO
		"shrl	$1, %%eax\n\t"
				/* shift=+1 */			/* shift=-8:+8 */
						"movdqa	%%xmm4, %%xmm0\n\t"
		"addl	%%ebx, %%ebx\n\t"	"psllq	$8, %%xmm0\n\t"
		"andl	%%edi, %%ebx\n\t"	"pand	%%xmm5, %%xmm0\n\t"	// 0 m7&o6 m6&o5 .. m1&o0
		"movl	%%ebx, %%edx\n\t"	"movdqa	%%xmm0, %%xmm1\n\t"
		"addl	%%ebx, %%ebx\n\t"	"psllq	$8, %%xmm0\n\t"
						"movdqa	%%xmm5, %%xmm3\n\t"
		"andl	%%edi, %%ebx\n\t"	"pand	%%xmm5, %%xmm0\n\t"	// 0 0 m7&o6&o5 .. m2&o1&o0
						"psllq	$8, %%xmm3\n\t"
		"orl	%%ebx, %%edx\n\t"	"por	%%xmm1, %%xmm0\n\t"	// 0 m7&o6 (m6&o5)|(m7&o6&o5) .. (m1&o0)
		"addl	%%ecx, %%ecx\n\t"	"pand	%%xmm5, %%xmm3\n\t"	// 0 o7&o6 o6&o5 o5&o4 o4&o3 ..
						"movdqa	%%xmm0, %%xmm2\n\t"
		"leal	(,%%edx,4), %%ebx\n\t"	"psllq	$16, %%xmm0\n\t"
		"andl	%%ecx, %%ebx\n\t"	"pand	%%xmm3, %%xmm0\n\t"	// 0 0 0 m7&o6&o5&o4 (m6&o5&o4&o3)|(m7&o6&o5&o4&o3) ..
		"orl	%%ebx, %%edx\n\t"	"por	%%xmm0, %%xmm2\n\t"
		"shll	$2, %%ebx\n\t"		"psllq	$16, %%xmm0\n\t"
		"andl	%%ecx, %%ebx\n\t"	"pand	%%xmm3, %%xmm0\n\t"	// 0 0 0 0 0 m7&o6&..&o2 (m6&o5&..&o1)|(m7&o6&..&o1) ..
		"orl	%%edx, %%ebx\n\t"	"por	%%xmm0, %%xmm2\n\t"
		"addl	%%ebx, %%ebx\n\t"	"psllq	$8, %%xmm2\n\t"
		"orl	%%eax, %%ebx\n\t"

		"movl	%2, %%esi\n\t"
		"movl	%4, %%edi\n\t"
				/* shift=-1 */			/* shift=-9:+7 */
		"andl	$0x7e7e7e7e,%%edi\n\t"	"pand	%5, %%xmm5\n\t"
		"movl	%%esi, %%eax\n\t"	"movdqa	%%xmm4, %%xmm0\n\t"
		"shrl	$1, %%eax\n\t"		"psllq	$7, %%xmm0\n\t"
		"andl	%%edi, %%eax\n\t"	"pand	%%xmm5, %%xmm0\n\t"
		"movl	%%eax, %%edx\n\t"	"movdqa	%%xmm0, %%xmm1\n\t"
		"shrl	$1, %%eax\n\t"		"psllq	$7, %%xmm0\n\t"
		"andl	%%edi, %%eax\n\t"	"pand	%%xmm5, %%xmm0\n\t"
		"movl	%%edi, %%ecx\n\t"	"movdqa	%%xmm5, %%xmm3\n\t"
		"orl	%%edx, %%eax\n\t"	"por	%%xmm1, %%xmm0\n\t"
		"shrl	$1, %%ecx\n\t"		"psllq	$7, %%xmm3\n\t"
		"movl	%%eax, %%edx\n\t"	"movdqa	%%xmm0, %%xmm1\n\t"
		"andl	%%edi, %%ecx\n\t"	"pand	%%xmm5, %%xmm3\n\t"
		"shrl	$2, %%eax\n\t"		"psllq	$14, %%xmm0\n\t"
		"andl	%%ecx, %%eax\n\t"	"pand	%%xmm3, %%xmm0\n\t"
		"orl	%%eax, %%edx\n\t"	"por	%%xmm0, %%xmm1\n\t"
		"shrl	$2, %%eax\n\t"		"psllq	$14, %%xmm0\n\t"
		"andl	%%ecx, %%eax\n\t"	"pand	%%xmm3, %%xmm0\n\t"
		"orl	%%edx, %%eax\n\t"	"por	%%xmm1, %%xmm0\n\t"
		"shrl	$1, %%eax\n\t"		"psllq	$7, %%xmm0\n\t"
						"por	%%xmm0, %%xmm2\n\t"
				/* shift=+1 */			/* shift=-7:+9 */
						"movdqa	%%xmm4, %%xmm0\n\t"
		"addl	%%esi, %%esi\n\t"	"psllq	$9, %%xmm0\n\t"
		"andl	%%edi, %%esi\n\t"	"pand	%%xmm5, %%xmm0\n\t"
		"movl	%%esi, %%edx\n\t"	"movdqa	%%xmm0, %%xmm1\n\t"
		"addl	%%esi, %%esi\n\t"	"psllq	$9, %%xmm0\n\t"
		"andl	%%edi, %%esi\n\t"	"pand	%%xmm5, %%xmm0\n\t"
						"movdqa	%%xmm5, %%xmm3\n\t"
		"orl	%%esi, %%edx\n\t"	"por	%%xmm1, %%xmm0\n\t"
						"psllq	$9, %%xmm3\n\t"
						"movdqa	%%xmm0, %%xmm1\n\t"
		"addl	%%ecx, %%ecx\n\t"	"pand	%%xmm5, %%xmm3\n\t"
		"leal	(,%%edx,4), %%esi\n\t"	"psllq	$18, %%xmm0\n\t"
		"andl	%%ecx, %%esi\n\t"	"pand	%%xmm3, %%xmm0\n\t"
		"orl	%%esi, %%edx\n\t"	"por	%%xmm0, %%xmm1\n\t"
		"shll	$2, %%esi\n\t"		"psllq	$18, %%xmm0\n\t"
		"andl	%%ecx, %%esi\n\t"	"pand	%%xmm3, %%xmm0\n\t"
		"orl	%%edx, %%esi\n\t"	"por	%%xmm1, %%xmm0\n\t"
		"addl	%%esi, %%esi\n\t"	"psllq	$9, %%xmm0\n\t"
		"orl	%%eax, %%esi\n\t"	"por	%%xmm0, %%xmm2\n\t"

		"movl	%1, %%eax\n\t"		"movhlps %%xmm2, %%xmm3\n\t"
		"movl	%2, %%edx\n\t"		"movd	%%xmm3, %%edi\n\t"	"movd	%%xmm2, %%ecx\n\t"
						"psrlq	$32, %%xmm3\n\t"	"psrlq	$32, %%xmm2\n\t"
						"bswapl	%%edi\n\t"		"orl	%%ecx, %%ebx\n\t"
		"orl	%3, %%eax\n\t"		"orl	%%edi, %%esi\n\t"
		"orl	%4, %%edx\n\t"		"movd	%%xmm3, %%edi\n\t"	"movd	%%xmm2, %%ecx\n\t"
		"notl	%%eax\n\t"		"bswapl	%%edi\n\t"
		"notl	%%edx\n\t"		"orl	%%edi, %%ebx\n\t"	"orl	%%ecx, %%esi\n\t"
		"andl	%%esi, %%edx\n\t"
		"andl	%%ebx, %%eax"
	: "=&A" (moves)
	: "m" (PL), "m" (PH), "m" (OL), "m" (OH), "m" (mask7e)
	: "ebx", "ecx", "esi", "edi" );

	return moves;
}

#endif // hasSSE2
#endif // x86

#if defined(__x86_64__) || defined(_M_X64)
/**
 * @brief SSE optimized get_stable_edge
 *
 * @param P bitboard with player's discs.
 * @param O bitboard with opponent's discs.
 * @return a bitboard with (some of) player's stable discs.
 *
 */
static unsigned long long get_stable_edge(const unsigned long long P, const unsigned long long O)
{
	// compute the exact stable edges (from precomputed tables)
	unsigned int a1a8po, h1h8po;
	unsigned long long stable_edge;

	__m128i	P0 = _mm_cvtsi64_si128(P);
	__m128i	O0 = _mm_cvtsi64_si128(O);
	__m128i	PO = _mm_unpacklo_epi8(O0, P0);
	stable_edge = edge_stability[_mm_extract_epi16(PO, 0)]
		| ((unsigned long long) edge_stability[_mm_extract_epi16(PO, 7)] << 56);

	PO = _mm_unpacklo_epi64(O0, P0);
	a1a8po = _mm_movemask_epi8(_mm_slli_epi64(PO, 7));
	h1h8po = _mm_movemask_epi8(PO);
#if 0 // def __BMI2__ // pdep is slow on AMD
	stable_edge |= _pdep_u64(edge_stability[a1a8po], 0x0101010101010101ULL)
		| _pdep_u64(edge_stability[h1h8po], 0x8080808080808080ULL);
#else
	stable_edge |= A1_A8[edge_stability[a1a8po]] | (A1_A8[edge_stability[h1h8po]] << 7);
#endif
	return stable_edge;
}

/**
 * @brief X64 optimized get_stability
 *
 * SSE pcmpeqb for horizontal get_full_lines.
 * CPU rotate for vertical get_full_lines.
 * Diag-7 is converted to diag-9 using vertical mirroring.
 * 
 * @param P bitboard with player's discs.
 * @param O bitboard with opponent's discs.
 * @return the number of stable discs.
 */
#ifdef __AVX2__

int get_stability(const unsigned long long P, const unsigned long long O)
{
	unsigned long long disc = (P | O);
	unsigned long long l8, stable;
	__m128i	l01, l79, v2_stable, v2_old_stable, v2_P_central;
	__m256i	lr79, v4_stable, v4_full;
	static const V2DI kff = {{ 0xffffffffffffffff, 0xffffffffffffffff }};
	static const V2DI mcpyswap = {{ 0x0706050403020100, 0x0001020304050607 }};
	static const V2DI mbswapll = {{ 0x0001020304050607, 0x08090A0B0C0D0E0F }};
	static const V4DI shift1897 = {{ 1, 8, 9, 7 }};
	static const V4DI shiftlr[] = {{{ 9, 7, 7, 9 }}, {{ 18, 14, 14, 18 }}, {{ 36, 28, 28, 36 }}};
	static const V4DI e0 = {{ 0xff818181818181ff, 0xff818181818181ff, 0xff818181818181ff, 0xff818181818181ff }};
	static const V4DI e791 = {{ 0xffffc1c1c1c1c1ff, 0xffff8383838383ff, 0xffff8383838383ff, 0xffffc1c1c1c1c1ff }};
	static const V4DI e792 = {{ 0xfffffffff1f1f1ff, 0xffffffff8f8f8fff, 0xffffffff8f8f8fff, 0xfffffffff1f1f1ff }};

	l01 = _mm_cvtsi64_si128(disc);		lr79 = _mm256_castsi128_si256(_mm_shuffle_epi8(l01, mcpyswap.v2));
	l01 = _mm_cmpeq_epi8(kff.v2, l01);	lr79 = _mm256_permute4x64_epi64(lr79, 0x50);	// disc, disc, rdisc, rdisc
						lr79 = _mm256_and_si256(lr79, _mm256_or_si256(e0.v4, _mm256_srlv_epi64(lr79, shiftlr[0].v4)));
	l8 = disc;				lr79 = _mm256_and_si256(lr79, _mm256_or_si256(e791.v4, _mm256_srlv_epi64(lr79, shiftlr[1].v4)));
	l8 &= (l8 >> 8) | (l8 << 56);		lr79 = _mm256_and_si256(lr79, _mm256_or_si256(e792.v4, _mm256_srlv_epi64(lr79, shiftlr[2].v4)));
	l8 &= (l8 >> 16) | (l8 << 48);		l79 = _mm_shuffle_epi8(_mm256_extracti128_si256(lr79, 1), mbswapll.v2);
	l8 &= (l8 >> 32) | (l8 << 32);		l79 = _mm_and_si128(l79, _mm256_castsi256_si128(lr79));

	// compute the exact stable edges (from precomputed tables)
	stable = get_stable_edge(P, O);

	// add full lines
	v2_P_central = _mm_andnot_si128(_mm256_castsi256_si128(e0.v4), _mm_cvtsi64_si128(P));
	stable |= l8 & _mm_extract_epi64(l79, 1) & _mm_cvtsi128_si64(_mm_and_si128(_mm_and_si128(l01, l79), v2_P_central));

	if (stable == 0)
		return 0;

	// now compute the other stable discs (ie discs touching another stable disc in each flipping direction).
	v4_full = _mm256_insertf128_si256(_mm256_castsi128_si256(_mm_insert_epi64(l01, l8, 1)), l79, 1);
	v2_stable = _mm_cvtsi64_si128(stable);
	do {
		v2_old_stable = v2_stable;
		v4_stable = _mm256_broadcastq_epi64(v2_stable);
		v4_stable = _mm256_or_si256(_mm256_or_si256(_mm256_srlv_epi64(v4_stable, shift1897.v4), _mm256_sllv_epi64(v4_stable, shift1897.v4)), v4_full);
		v2_stable = _mm_and_si128(_mm256_castsi256_si128(v4_stable), _mm256_extracti128_si256(v4_stable, 1));
		v2_stable = _mm_and_si128(v2_stable, _mm_unpackhi_epi64(v2_stable, v2_stable));
		v2_stable = _mm_or_si128(v2_old_stable, _mm_and_si128(v2_stable, v2_P_central));
	} while (!_mm_testc_si128(v2_old_stable, v2_stable));

	return bit_count(_mm_cvtsi128_si64(v2_stable));
}

#else

int get_stability(const unsigned long long P, const unsigned long long O)
{
	unsigned long long disc = (P | O);
	unsigned long long P_central = (P & 0x007e7e7e7e7e7e00ULL);
	unsigned long long l8, full_h, full_v, full_d7, full_d9, stable;
	unsigned long long stable_h, stable_v, stable_d7, stable_d9, old_stable;
#if 1	// 1 CPU, 3 SSE
	__m128i l01, l79, r79;	// full lines
	static const V2DI kff =  {{ 0xffffffffffffffff, 0xffffffffffffffff }};
	static const V2DI e0 =   {{ 0xff818181818181ff, 0xff818181818181ff }};
	static const V2DI e790 = {{ 0xffffc1c1c1c1c1ff, 0xffffc1c1c1c1c1ff }};
	static const V2DI e791 = {{ 0xff8f8f8ff1f1f1ff, 0xff8f8f8ff1f1f1ff }};
	static const V2DI e792 = {{ 0xff8383838383ffff, 0xff8383838383ffff }};

	l01 = l79 = _mm_cvtsi64_si128(disc);	r79 = _mm_cvtsi64_si128(vertical_mirror(disc));
	l01 = _mm_cmpeq_epi8(kff.v2, l01);	l79 = r79 = _mm_unpacklo_epi64(l79, r79);
	full_h = _mm_cvtsi128_si64(l01);	l79 = _mm_and_si128(l79, _mm_or_si128(e0.v2, _mm_srli_epi64(l79, 9)));
						r79 = _mm_and_si128(r79, _mm_or_si128(e0.v2, _mm_slli_epi64(r79, 9)));
	l8 = disc;				l79 = _mm_and_si128(l79, _mm_or_si128(e790.v2, _mm_srli_epi64(l79, 18)));
	l8 &= (l8 >> 8) | (l8 << 56);		r79 = _mm_and_si128(r79, _mm_or_si128(e792.v2, _mm_slli_epi64(r79, 18)));
	l8 &= (l8 >> 16) | (l8 << 48);		l79 = _mm_and_si128(_mm_and_si128(l79, r79), _mm_or_si128(_mm_or_si128(e791.v2, _mm_srli_epi64(l79, 36)), _mm_slli_epi64(r79, 36)));
	l8 &= (l8 >> 32) | (l8 << 32);		full_d9 = _mm_cvtsi128_si64(l79);
	full_v = l8;				full_d7 = vertical_mirror(_mm_cvtsi128_si64(_mm_unpackhi_epi64(l79, l79)));

#else	// 4 CPU
	unsigned long long l1, l7, l9, r7, r9;	// full lines
	static const unsigned long long edge = 0xff818181818181ffULL;
	static const unsigned long long k01 = 0x0101010101010101ULL;
	// static const unsigned long long e1[] = { 0xffc1c1c1c1c1c1ffULL, 0xfff1f1f1f1f1f1ffULL, 0xff838383838383ffULL, 0xff8f8f8f8f8f8fffULL }; 
	// static const unsigned long long e8[] = { 0xffff8181818181ffULL, 0xffffffff818181ffULL, 0xff8181818181ffffULL, 0xff818181ffffffffULL };
	static const unsigned long long e7[] = { 0xffff8383838383ffULL, 0xffffffff8f8f8fffULL, 0xffc1c1c1c1c1ffffULL, 0xfff1f1f1ffffffffULL };
	static const unsigned long long e9[] = { 0xffffc1c1c1c1c1ffULL, 0xff8f8f8ff1f1f1ffULL, 0xff8383838383ffffULL };

	l1 = l7 = r7 = disc;
	l1 &= l1 >> 1;				l7 &= edge | (l7 >> 7);		r7 &= edge | (r7 << 7);
	l1 &= l1 >> 2;				l7 &= e7[0] | (l7 >> 14);	r7 &= e7[2] | (r7 << 14);
	l1 &= l1 >> 4;				l7 &= e7[1] | (l7 >> 28);	r7 &= e7[3] | (r7 << 28);
	full_h = ((l1 & k01) * 0xff);		full_d7 = l7 & r7;

	l8 = l9 = r9 = disc;
	l8 &= (l8 >> 8) | (l8 << 56);		l9 &= edge | (l9 >> 9);		r9 &= edge | (r9 << 9);
	l8 &= (l8 >> 16) | (l8 << 48);		l9 &= e9[0] | (l9 >> 18);	r9 &= e9[2] | (r9 << 18);
	l8 &= (l8 >> 32) | (l8 << 32);		full_d9 = l9 & r9 & (e9[1] | (l9 >> 36) | (r9 << 36));
	full_v = l8;

#endif
	// compute the exact stable edges (from precomputed tables)
	stable = get_stable_edge(P, O);

	// add full lines
	stable |= (full_h & full_v & full_d7 & full_d9 & P_central);

	if (stable == 0)
		return 0;

	// now compute the other stable discs (ie discs touching another stable disc in each flipping direction).
	do {
		old_stable = stable;
		stable_h = ((stable >> 1) | (stable << 1) | full_h);
		stable_v = ((stable >> 8) | (stable << 8) | full_v);
		stable_d7 = ((stable >> 7) | (stable << 7) | full_d7);
		stable_d9 = ((stable >> 9) | (stable << 9) | full_d9);
		stable |= (stable_h & stable_v & stable_d7 & stable_d9 & P_central);
	} while (stable != old_stable);

	return bit_count(stable);
}

#endif // __AVX2__
#endif // __x86_64__

/**
 * @brief SSE translation of board_get_hash_code.
 *
 * Too many dependencies, effective only on 32bit build.
 * For AMD, MMX version in board_mmx.c is faster.
 *
 * @param p pointer to 16 bytes to hash.
 * @return the hash code of the bitboard
 */
#if (defined(USE_GAS_MMX) && !defined(__3dNOW__)) || defined(USE_MSVC_X86) // || defined(__x86_64__)

unsigned long long board_get_hash_code_sse(const unsigned char *p)
{
	unsigned long long h;
#if defined(hasSSE2) || defined(USE_MSVC_X86)
	__m128	h0, h1, h2, h3;

	h0 = _mm_loadh_pi(_mm_castsi128_ps(_mm_loadl_epi64((__m128i *) &hash_rank[0][p[0]])), (__m64 *) &hash_rank[4][p[4]]);
	h1 = _mm_loadh_pi(_mm_castsi128_ps(_mm_loadl_epi64((__m128i *) &hash_rank[1][p[1]])), (__m64 *) &hash_rank[5][p[5]]);
	h2 = _mm_loadh_pi(_mm_castsi128_ps(_mm_loadl_epi64((__m128i *) &hash_rank[2][p[2]])), (__m64 *) &hash_rank[6][p[6]]);
	h3 = _mm_loadh_pi(_mm_castsi128_ps(_mm_loadl_epi64((__m128i *) &hash_rank[3][p[3]])), (__m64 *) &hash_rank[7][p[7]]);
	h0 = _mm_xor_ps(h0, h2);	h1 = _mm_xor_ps(h1, h3);
	h2 = _mm_loadh_pi(_mm_castsi128_ps(_mm_loadl_epi64((__m128i *) &hash_rank[8][p[8]])), (__m64 *) &hash_rank[10][p[10]]);
	h3 = _mm_loadh_pi(_mm_castsi128_ps(_mm_loadl_epi64((__m128i *) &hash_rank[9][p[9]])), (__m64 *) &hash_rank[11][p[11]]);
	h0 = _mm_xor_ps(h0, h2);	h1 = _mm_xor_ps(h1, h3);
	h2 = _mm_loadh_pi(_mm_castsi128_ps(_mm_loadl_epi64((__m128i *) &hash_rank[12][p[12]])), (__m64 *) &hash_rank[14][p[14]]);
	h3 = _mm_loadh_pi(_mm_castsi128_ps(_mm_loadl_epi64((__m128i *) &hash_rank[13][p[13]])), (__m64 *) &hash_rank[15][p[15]]);
	h0 = _mm_xor_ps(h0, h2);	h1 = _mm_xor_ps(h1, h3);
	h0 = _mm_xor_ps(h0, h1);
	h0 = _mm_xor_ps(h0, _mm_movehl_ps(h1, h0));
	h = _mm_cvtsi128_si64(_mm_castps_si128(h0));

#else
	__asm__ volatile (
		"movq	%0, %%xmm0\n\t"		"movq	%1, %%xmm1"
	: : "m" (hash_rank[0][p[0]]), "m" (hash_rank[1][p[1]]));
	__asm__ volatile (
		"movq	%0, %%xmm2\n\t"		"movq	%1, %%xmm3"
	: : "m" (hash_rank[2][p[2]]), "m" (hash_rank[3][p[3]]));
	__asm__ volatile (
		"movhps	%0, %%xmm0\n\t"		"movhps	%1, %%xmm1"
	: : "m" (hash_rank[4][p[4]]), "m" (hash_rank[5][p[5]]));
	__asm__ volatile (
		"movhps	%0, %%xmm2\n\t"		"movhps	%1, %%xmm3"
	: : "m" (hash_rank[6][p[6]]), "m" (hash_rank[7][p[7]]));
	__asm__ volatile (
		"xorps	%%xmm2, %%xmm0\n\t"	"xorps	%%xmm3, %%xmm1\n\t"
		"movq	%0, %%xmm2\n\t"		"movq	%1, %%xmm3"
	: : "m" (hash_rank[8][p[8]]), "m" (hash_rank[9][p[9]]));
	__asm__ volatile (
		"movhps	%0, %%xmm2\n\t"		"movhps	%1, %%xmm3"
	: : "m" (hash_rank[10][p[10]]), "m" (hash_rank[11][p[11]]));
	__asm__ volatile (
		"xorps	%%xmm2, %%xmm0\n\t"	"xorps	%%xmm3, %%xmm1\n\t"
		"movq	%0, %%xmm2\n\t"		"movq	%1, %%xmm3"
	: : "m" (hash_rank[12][p[12]]), "m" (hash_rank[13][p[13]]));
	__asm__ volatile (
		"movhps	%1, %%xmm2\n\t"		"movhps	%2, %%xmm3\n\t"
		"xorps	%%xmm2, %%xmm0\n\t"	"xorps	%%xmm3, %%xmm1\n\t"
		"xorps	%%xmm1, %%xmm0\n\t"
		"movhlps %%xmm0, %%xmm1\n\t"
		"xorps	%%xmm1, %%xmm0\n\t"
		"movd	%%xmm0, %%eax\n\t"
		"punpckhdq %%xmm0, %%xmm0\n\t"
		"movd	%%xmm0, %%edx"
	: "=A" (h) : "m" (hash_rank[14][p[14]]), "m" (hash_rank[15][p[15]]));
#endif

	return h;
}

#endif // USE_GAS_MMX

#if 0 // def __AVX2__	// experimental - too many instructions

unsigned long long board_get_hash_code_avx2(const unsigned char *p)
{
	__m128i	ix0, ix8, hh;
	__m256i	hhh;
	static const __v16qi rank = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };

	ix0 = _mm_loadu_si128((__m128i *) p);
	ix8 = _mm_unpackhi_epi8(ix0, (__m128i) rank);
	ix0 = _mm_unpacklo_epi8(ix0, (__m128i) rank);

	hhh  = _mm256_i32gather_epi64((long long *) hash_rank[0], _mm_blend_epi16(_mm_setzero_si128(), ix0, 0x55), 8);
	hhh ^= _mm256_i32gather_epi64((long long *) hash_rank[0], _mm_blend_epi16(_mm_setzero_si128(), ix8, 0x55), 8);
	hhh ^= _mm256_i32gather_epi64((long long *) hash_rank[0], _mm_srli_epi32(ix0, 16), 8);
	hhh ^= _mm256_i32gather_epi64((long long *) hash_rank[0], _mm_srli_epi32(ix8, 16), 8);

	hh = _mm256_castsi256_si128(hhh) ^ _mm256_extracti128_si256(hhh, 1);
	hh ^= _mm_shuffle_epi32(hh, 0x4e);
	return hh[0];
}

#endif
