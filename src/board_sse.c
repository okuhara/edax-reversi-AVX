/**
 * @file board_sse.c
 *
 * SSE/AVX translation of some board.c functions
 *
 * @date 2014 - 2022
 * @author Toshihiko Okuhara
 * @version 4.5
 */

#include "bit.h"
#include "hash.h"
#include "board.h"

#if defined(ANDROID) && !defined(hasNeon) && !defined(hasSSE2)
#include "android/cpu-features.h"

bool	hasSSE2 = false;

void init_neon (void)
{
#ifdef __arm__
	if (android_getCpuFeatures() & ANDROID_CPU_ARM_FEATURE_NEON) {
	#if (MOVE_GENERATOR == MOVE_GENERATOR_BITSCAN)
		extern unsigned long long (*flip_neon[66])(const unsigned long long, const unsigned long long);
		memcpy(flip, flip_neon, sizeof(flip_neon));
	#endif
		hasSSE2 = true;
	}
#else	// android x86 w/o SSE2 - uncommon and not tested
	int	cpuid_edx, cpuid_ecx;
	__asm__ (
		"movl	$1, %%eax\n\t"
		"cpuid"
	: "=d" (cpuid_edx), "=c" (cpuid_ecx) :: "%eax", "%ebx" );
	if ((cpuid_edx & 0x04000000u) != 0)
		hasSSE2 = true;
#endif
}
#endif

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
	const __m128i mask0F0F = _mm_set1_epi16(0x0F0F);
	const __m128i mask00AA = _mm_set1_epi16(0x00AA);
	const __m128i maskCCCC = _mm_set1_epi32(0x0000CCCC);
	const __m128i mask00F0 = _mm_set1_epi64x(0x00000000F0F0F0F0);
#if defined(__SSSE3__) || defined(__AVX__)	// pshufb
	const __m128i mbswapll = _mm_set_epi8(8, 9, 10, 11, 12, 13, 14, 15, 0, 1, 2, 3, 4, 5, 6, 7);
	const __m128i mbitrev  = _mm_set_epi8(15, 7, 11, 3, 13, 5, 9, 1, 14, 6, 10, 2, 12, 4, 8, 0);

	if (s & 1) {	// horizontal_mirror (cf. http://wm.ite.pl/articles/sse-popcount.html)
		bb = _mm_or_si128(_mm_shuffle_epi8(mbitrev, _mm_and_si128(_mm_srli_epi64(bb, 4), mask0F0F)),
			_mm_slli_epi64(_mm_shuffle_epi8(mbitrev, _mm_and_si128(bb, mask0F0F)), 4));
	}

	if (s & 2) {	// vertical_mirror
		bb = _mm_shuffle_epi8(bb, mbswapll);
	}

#else
	const __m128i mask5555 = _mm_set1_epi16(0x5555);
	const __m128i mask3333 = _mm_set1_epi16(0x3333);

	if (s & 1) {	// horizontal_mirror
		bb = _mm_or_si128(_mm_and_si128(_mm_srli_epi64(bb, 1), mask5555), _mm_slli_epi64(_mm_and_si128(bb, mask5555), 1));
		bb = _mm_or_si128(_mm_and_si128(_mm_srli_epi64(bb, 2), mask3333), _mm_slli_epi64(_mm_and_si128(bb, mask3333), 2));
		bb = _mm_or_si128(_mm_and_si128(_mm_srli_epi64(bb, 4), mask0F0F), _mm_slli_epi64(_mm_and_si128(bb, mask0F0F), 4));
	}

	if (s & 2) {	// vertical_mirror
		bb = _mm_or_si128(_mm_srli_epi16(bb, 8), _mm_slli_epi16(bb, 8));
		bb = _mm_shufflehi_epi16(_mm_shufflelo_epi16(bb, 0x1b), 0x1b);
	}
#endif

	if (s & 4) {	// transpose
		tt = _mm_and_si128(_mm_xor_si128(bb, _mm_srli_epi64(bb, 7)), mask00AA);
		bb = _mm_xor_si128(_mm_xor_si128(bb, tt), _mm_slli_epi64(tt, 7));
		tt = _mm_and_si128(_mm_xor_si128(bb, _mm_srli_epi64(bb, 14)), maskCCCC);
		bb = _mm_xor_si128(_mm_xor_si128(bb, tt), _mm_slli_epi64(tt, 14));
		tt = _mm_and_si128(_mm_xor_si128(bb, _mm_srli_epi64(bb, 28)), mask00F0);
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

#elif defined(hasNeon)

void board_symetry(const Board *board, const int s, Board *sym)
{
	uint64x2_t bb = vld1q_u64((uint64_t *) board);
	uint64x2_t tt;

	if (s & 1) {	// horizontal_mirror
#ifdef HAS_CPU_64
		bb = vreinterpretq_u64_u8(vrbitq_u8(vreinterpretq_u8_u64(bb)));
#else
		bb = vbslq_u64(vdupq_n_u64(0x5555555555555555), vshrq_n_u64(bb, 1), vshlq_n_u64(bb, 1));
		bb = vbslq_u64(vdupq_n_u64(0x3333333333333333), vshrq_n_u64(bb, 2), vshlq_n_u64(bb, 2));
		bb = vreinterpretq_u64_u8(vsliq_n_u8(vshrq_n_u8(vreinterpretq_u8_u64(bb), 4), vreinterpretq_u8_u64(bb), 4));
#endif
	}

	if (s & 2) {	// vertical_mirror
		bb = vreinterpretq_u64_u8(vrev64q_u8(vreinterpretq_u8_u64(bb)));
	}

	if (s & 4) {	// transpose
		tt = vandq_u64(veorq_u64(bb, vshrq_n_u64(bb, 7)), vdupq_n_u64(0x00AA00AA00AA00AA));
		bb = veorq_u64(veorq_u64(bb, tt), vshlq_n_u64(tt, 7));
		tt = vandq_u64(veorq_u64(bb, vshrq_n_u64(bb, 14)), vdupq_n_u64(0x0000CCCC0000CCCC));
		bb = veorq_u64(veorq_u64(bb, tt), vshlq_n_u64(tt, 14));
		tt = vandq_u64(veorq_u64(bb, vshrq_n_u64(bb, 28)), vdupq_n_u64(0x00000000F0F0F0F0));
		bb = veorq_u64(veorq_u64(bb, tt), vshlq_n_u64(tt, 28));
	}

	vst1q_u64((uint64_t *) sym, bb);
	board_check(sym);
}

#endif // hasSSE2/Neon

/**
 * @brief Compute a board resulting of a move played on a previous board.
 *
 * @param board board to play the move on.
 * @param x move to play.
 * @param next resulting board.
 * @return flipped discs.
 */
#if (MOVE_GENERATOR == MOVE_GENERATOR_AVX) || (MOVE_GENERATOR == MOVE_GENERATOR_SSE)

unsigned long long board_next(const Board *board, const int x, Board *next)
{
	__m128i OP = _mm_loadu_si128((__m128i *) board);
	__m128i flipped = mm_Flip(OP, x);

	OP = _mm_xor_si128(OP, _mm_or_si128(flipped, _mm_loadl_epi64((__m128i *) &X_TO_BIT[x])));
	_mm_storeu_si128((__m128i *) next, _mm_shuffle_epi32(OP, 0x4e));

	return _mm_cvtsi128_si64(flipped);
}

#elif MOVE_GENERATOR == MOVE_GENERATOR_NEON

unsigned long long board_next(const Board *board, const int x, Board *next)
{
	uint64x2_t OP = vld1q_u64((uint64_t *) board);
	uint64x2_t flipped = mm_Flip(OP, x);
#ifdef HAS_CPU_64	// vld1q_lane_u64
	OP = veorq_u64(OP, vorrq_u64(flipped, vld1q_lane_u64((uint64_t *) &X_TO_BIT[x], flipped, 0)));
	vst1q_u64((uint64_t *) next, vextq_u64(OP, OP, 1));
#else
	OP = veorq_u64(OP, flipped);
	OP = vcombine_u64(vget_high_u64(OP), vorr_u64(vget_low_u64(OP), vld1_u64(&X_TO_BIT[x])));
	vst1q_u64((uint64_t *) next, OP);
#endif
	return vgetq_lane_u64(flipped, 0);
}
#endif

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
	const __m256i shift1897 = _mm256_set_epi64x(7, 9, 8, 1);
	const __m256i mflipH = _mm256_set_epi64x(0x7e7e7e7e7e7e7e7e, 0x7e7e7e7e7e7e7e7e, -1, 0x7e7e7e7e7e7e7e7e);

	PP = _mm256_broadcastq_epi64(_mm_cvtsi64_si128(P));
	mOO = _mm256_and_si256(_mm256_broadcastq_epi64(_mm_cvtsi64_si128(O)), mflipH);

	flip_l = _mm256_and_si256(mOO, _mm256_sllv_epi64(PP, shift1897));
	flip_r = _mm256_and_si256(mOO, _mm256_srlv_epi64(PP, shift1897));
	flip_l = _mm256_or_si256(flip_l, _mm256_and_si256(mOO, _mm256_sllv_epi64(flip_l, shift1897)));
	flip_r = _mm256_or_si256(flip_r, _mm256_and_si256(mOO, _mm256_srlv_epi64(flip_r, shift1897)));
	pre_l = _mm256_and_si256(mOO, _mm256_sllv_epi64(mOO, shift1897));
	pre_r = _mm256_srlv_epi64(pre_l, shift1897);
	shift2 = _mm256_add_epi64(shift1897, shift1897);
	flip_l = _mm256_or_si256(flip_l, _mm256_and_si256(pre_l, _mm256_sllv_epi64(flip_l, shift2)));
	flip_r = _mm256_or_si256(flip_r, _mm256_and_si256(pre_r, _mm256_srlv_epi64(flip_r, shift2)));
	flip_l = _mm256_or_si256(flip_l, _mm256_and_si256(pre_l, _mm256_sllv_epi64(flip_l, shift2)));
	flip_r = _mm256_or_si256(flip_r, _mm256_and_si256(pre_r, _mm256_srlv_epi64(flip_r, shift2)));
	MM = _mm256_sllv_epi64(flip_l, shift1897);
	MM = _mm256_or_si256(MM, _mm256_srlv_epi64(flip_r, shift1897));

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

#elif defined(__aarch64__) || defined(_M_ARM64)	// 4 CPU

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

#elif defined(__ARM_NEON__)	// 3 Neon, 1 CPU(32)

#ifdef hasNeon
#define	get_moves_sse	get_moves	// no dispatch
#endif

unsigned long long get_moves_sse(unsigned long long P, unsigned long long O)
{
	unsigned int	mO, movesL, movesH, flip1, pre1;
	uint64x1_t	rP, rO;
	uint64x2_t	PP, OO, MM, flip, pre;

		/* vertical_mirror in PP[1], OO[1] */				mO = (unsigned int) O & 0x7e7e7e7e;
	rP = vreinterpret_u64_u8(vrev64_u8(vcreate_u8(P)));			flip1  = mO & ((unsigned int) P << 1);
	PP = vcombine_u64(vcreate_u64(P), rP);					flip1 |= mO & (flip1 << 1);
										pre1   = mO & (mO << 1);
	rO = vreinterpret_u64_u8(vrev64_u8(vcreate_u8(O)));			flip1 |= pre1 & (flip1 << 2);
	OO = vcombine_u64(vcreate_u64(O), rO);					flip1 |= pre1 & (flip1 << 2);
										movesL = flip1 << 1;

	flip = vandq_u64(OO, vshlq_n_u64(PP, 8));				flip1  = mO & ((unsigned int) P >> 1);
	flip = vorrq_u64(flip, vandq_u64(OO, vshlq_n_u64(flip, 8)));		flip1 |= mO & (flip1 >> 1);
	pre  = vandq_u64(OO, vshlq_n_u64(OO, 8));				pre1 >>= 1;
	flip = vorrq_u64(flip, vandq_u64(pre, vshlq_n_u64(flip, 16)));		flip1 |= pre1 & (flip1 >> 2);
	flip = vorrq_u64(flip, vandq_u64(pre, vshlq_n_u64(flip, 16)));		flip1 |= pre1 & (flip1 >> 2);
	MM = vshlq_n_u64(flip, 8);						movesL |= flip1 >> 1;

	OO = vandq_u64(OO, vdupq_n_u64(0x7e7e7e7e7e7e7e7e));			mO = (unsigned int) (O >> 32) & 0x7e7e7e7e;
	flip = vandq_u64(OO, vshlq_n_u64(PP, 7));				flip1  = mO & ((unsigned int) (P >> 32) << 1);
	flip = vorrq_u64(flip, vandq_u64(OO, vshlq_n_u64(flip, 7)));		flip1 |= mO & (flip1 << 1);
	pre  = vandq_u64(OO, vshlq_n_u64(OO, 7));				pre1   = mO & (mO << 1);
	flip = vorrq_u64(flip, vandq_u64(pre, vshlq_n_u64(flip, 14)));		flip1 |= pre1 & (flip1 << 2);
	flip = vorrq_u64(flip, vandq_u64(pre, vshlq_n_u64(flip, 14)));		flip1 |= pre1 & (flip1 << 2);
	MM = vorrq_u64(MM, vshlq_n_u64(flip, 7));				movesH = flip1 << 1;

	flip = vandq_u64(OO, vshlq_n_u64(PP, 9));				flip1  = mO & ((unsigned int) (P >> 32) >> 1);
	flip = vorrq_u64(flip, vandq_u64(OO, vshlq_n_u64(flip, 9)));		flip1 |= mO & (flip1 >> 1);
	pre  = vandq_u64(OO, vshlq_n_u64(OO, 9));				pre1 >>= 1;
	flip = vorrq_u64(flip, vandq_u64(pre, vshlq_n_u64(flip, 18)));		flip1 |= pre1 & (flip1 >> 2);
	flip = vorrq_u64(flip, vandq_u64(pre, vshlq_n_u64(flip, 18)));		flip1 |= pre1 & (flip1 >> 2);
	MM = vorrq_u64(MM, vshlq_n_u64(flip, 9));				movesH |= flip1 >> 1;

	movesL |= vgetq_lane_u32(vreinterpretq_u32_u64(MM), 0) | bswap_int(vgetq_lane_u32(vreinterpretq_u32_u64(MM), 3));
	movesH |= vgetq_lane_u32(vreinterpretq_u32_u64(MM), 1) | bswap_int(vgetq_lane_u32(vreinterpretq_u32_u64(MM), 2));
	return (movesL | ((unsigned long long) movesH << 32)) & ~(P|O);	// mask with empties
}

#else // AVX/x86_64/arm
/**
 * @brief SSE optimized get_moves for x86 - 3 SSE, 1 CPU(32)
 *
 */
#if defined(hasSSE2) || defined(USE_MSVC_X86) || defined(ANDROID)

#ifdef hasSSE2
#define	get_moves_sse	get_moves	// no dispatch
#endif

unsigned long long get_moves_sse(const unsigned long long P, const unsigned long long O)
{
	unsigned int	mO, movesL, movesH, flip1, pre1;
	__m128i	OP, rOP, PP, OO, MM, flip, pre;

		// vertical_mirror in PP[1], OO[1]
	OP  = _mm_unpacklo_epi64(_mm_cvtsi64_si128(P), _mm_cvtsi64_si128(O));		mO = (unsigned int) O & 0x7e7e7e7eU;
	rOP = _mm_shufflelo_epi16(OP, 0x1B);						flip1  = mO & ((unsigned int) P << 1);
	rOP = _mm_shufflehi_epi16(rOP, 0x1B);						flip1 |= mO & (flip1 << 1);
											pre1   = mO & (mO << 1);
	rOP = _mm_or_si128(_mm_srli_epi16(rOP, 8), _mm_slli_epi16(rOP, 8));
	    										flip1 |= pre1 & (flip1 << 2);
	PP  = _mm_unpacklo_epi64(OP, rOP);						flip1 |= pre1 & (flip1 << 2);
	OO  = _mm_unpackhi_epi64(OP, rOP);						movesL = flip1 << 1;

	flip = _mm_and_si128(OO, _mm_slli_epi64(PP, 8));				flip1  = mO & ((unsigned int) P >> 1);
	flip = _mm_or_si128(flip, _mm_and_si128(OO, _mm_slli_epi64(flip, 8)));		flip1 |= mO & (flip1 >> 1);
	pre = _mm_and_si128(OO, _mm_slli_epi64(OO, 8));					pre1 >>= 1;
	flip = _mm_or_si128(flip, _mm_and_si128(pre, _mm_slli_epi64(flip, 16)));	flip1 |= pre1 & (flip1 >> 2);
	flip = _mm_or_si128(flip, _mm_and_si128(pre, _mm_slli_epi64(flip, 16)));	flip1 |= pre1 & (flip1 >> 2);
	MM = _mm_slli_epi64(flip, 8);							movesL |= flip1 >> 1;

	OO = _mm_and_si128(OO, _mm_set1_epi8(0x7e));					mO = (unsigned int) (O >> 32) & 0x7e7e7e7eU;
	flip = _mm_and_si128(OO, _mm_slli_epi64(PP, 7));				flip1  = mO & ((unsigned int) (P >> 32) << 1);
	flip = _mm_or_si128(flip, _mm_and_si128(OO, _mm_slli_epi64(flip, 7)));		flip1 |= mO & (flip1 << 1);
	pre = _mm_and_si128(OO, _mm_slli_epi64(OO, 7));					pre1   = mO & (mO << 1);
	flip = _mm_or_si128(flip, _mm_and_si128(pre, _mm_slli_epi64(flip, 14)));	flip1 |= pre1 & (flip1 << 2);
	flip = _mm_or_si128(flip, _mm_and_si128(pre, _mm_slli_epi64(flip, 14)));	flip1 |= pre1 & (flip1 << 2);
	MM = _mm_or_si128(MM, _mm_slli_epi64(flip, 7));					movesH = flip1 << 1;

	flip = _mm_and_si128(OO, _mm_slli_epi64(PP, 9));				flip1  = mO & ((unsigned int) (P >> 32) >> 1);
	flip = _mm_or_si128(flip, _mm_and_si128(OO, _mm_slli_epi64(flip, 9)));		flip1 |= mO & (flip1 >> 1);
	pre = _mm_and_si128(OO, _mm_slli_epi64(OO, 9));					pre1 >>= 1;
	flip = _mm_or_si128(flip, _mm_and_si128(pre, _mm_slli_epi64(flip, 18)));	flip1 |= pre1 & (flip1 >> 2);
	flip = _mm_or_si128(flip, _mm_and_si128(pre, _mm_slli_epi64(flip, 18)));	flip1 |= pre1 & (flip1 >> 2);
	MM = _mm_or_si128(MM, _mm_slli_epi64(flip, 9));					movesH |= flip1 >> 1;

	movesL |= _mm_cvtsi128_si32(MM);	MM = _mm_srli_si128(MM, 4);
	movesH |= _mm_cvtsi128_si32(MM);	MM = _mm_srli_si128(MM, 4);
	movesH |= bswap_int(_mm_cvtsi128_si32(MM));
	movesL |= bswap_int(_mm_cvtsi128_si32(_mm_srli_si128(MM, 4)));
	return (movesL | ((unsigned long long) movesH << 32)) & ~(P|O);	// mask with empties
}

#else // non-VEX asm

unsigned long long get_moves_sse(const unsigned long long P, const unsigned long long O)
{
	unsigned long long moves;
	static const V2DI mask7e = {{ 0x7e7e7e7e7e7e7e7eULL, 0x7e7e7e7e7e7e7e7eULL }};

	__asm__ (
		"movl	%1, %%ebx\n\t"
		"movl	%3, %%edi\n\t"
		"andl	$0x7e7e7e7e, %%edi\n\t"
				/* shift=-1 */			/* vertical mirror in PP[1], OO[1] */
		"movl	%%ebx, %%eax\n\t"	"movd	%1, %%xmm4\n\t"		// (movd for store-forwarding)
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
	: "m" (P), "m" (((unsigned int *)&P)[1]), "m" (O), "m" (((unsigned int *)&O)[1]), "m" (mask7e)
	: "ebx", "ecx", "esi", "edi" );

	return moves;
}

#endif // hasSSE2
#endif // x86

#if defined(hasSSE2) || defined(hasNeon)	// no dispatch

/**
 * @brief SSE optimized get_stable_edge
 *
 * @param P bitboard with player's discs.
 * @param O bitboard with opponent's discs.
 * @return a bitboard with (some of) player's stable discs.
 *
 */
#if defined(__aarch64__) || defined(_M_ARM64)	// for vaddvq
unsigned long long get_stable_edge(unsigned long long P, unsigned long long O)
{	// compute the exact stable edges (from precomputed tables)
	// const int16x8_t shiftv = { 0, 1, 2, 3, 4, 5, 6, 7 };	// error on MSVC
	const uint64x2_t shiftv = { 0x0003000200010000, 0x0007000600050004 };
	uint8x16_t PO = vzip1q_u8(vreinterpretq_u8_u64(vdupq_n_u64(O)), vreinterpretq_u8_u64(vdupq_n_u64(P)));
	unsigned int a1a8 = edge_stability[vaddvq_u16(vshlq_u16(vreinterpretq_u16_u8(vandq_u8(PO, vdupq_n_u8(1))), vreinterpretq_s16_u64(shiftv)))];
	unsigned int h1h8 = edge_stability[vaddvq_u16(vshlq_u16(vreinterpretq_u16_u8(vshrq_n_u8(PO, 7)), vreinterpretq_s16_u64(shiftv)))];
	return edge_stability[vgetq_lane_u16(vreinterpretq_u16_u8(PO), 0)]
	    |  (unsigned long long) edge_stability[vgetq_lane_u16(vreinterpretq_u16_u8(PO), 7)] << 56
	    |  unpackA1A8(a1a8) | unpackH1H8(h1h8);
}

#elif defined(__ARM_NEON__) // Neon kindergarten
unsigned long long get_stable_edge(unsigned long long P, unsigned long long O)
{	// compute the exact stable edges (from precomputed tables)
	const uint64x2_t kMul  = { 0x1020408001020408, 0x1020408001020408 };
	uint64x2_t PP = vcombine_u64(vshl_n_u64(vcreate_u64(P), 7), vcreate_u64(P));
	uint64x2_t OO = vcombine_u64(vshl_n_u64(vcreate_u64(O), 7), vcreate_u64(O));
	uint32x4_t QP = vmulq_u32(vreinterpretq_u32_u64(kMul), vreinterpretq_u32_u8(vshrq_n_u8(vreinterpretq_u8_u64(PP), 7)));
	uint32x4_t QO = vmulq_u32(vreinterpretq_u32_u64(kMul), vreinterpretq_u32_u8(vshrq_n_u8(vreinterpretq_u8_u64(OO), 7)));
	uint32x2_t DP = vpadd_u32(vget_low_u32(QP), vget_high_u32(QP));	// P_h1h8 * * * P_a1a8 * * *
	uint32x2_t DO = vpadd_u32(vget_low_u32(QO), vget_high_u32(QO));	// O_h1h8 * * * O_a1a8 * * *
	uint8x8_t DB = vtrn_u8(vreinterpret_u8_u32(DO), vreinterpret_u8_u32(DP)).val[1];	// P_h1h8 O_h1h8 * * P_a1a8 O_a1a8 * *
	unsigned int a1a8 = edge_stability[vget_lane_u16(vreinterpret_u16_u8(DB), 1)];
	unsigned int h1h8 = edge_stability[vget_lane_u16(vreinterpret_u16_u8(DB), 3)];
	uint8x16_t PO = vzipq_u8(vreinterpretq_u8_u64(OO), vreinterpretq_u8_u64(PP)).val[1];
	return edge_stability[vgetq_lane_u16(vreinterpretq_u16_u8(PO), 0)]
	    |  (unsigned long long) edge_stability[vgetq_lane_u16(vreinterpretq_u16_u8(PO), 7)] << 56
	    |  unpackA1A8(a1a8) | unpackH1H8(h1h8);
}

#elif defined(hasSSE2) || defined(USE_MSVC_X86)
unsigned long long get_stable_edge(const unsigned long long P, const unsigned long long O)
{
	// compute the exact stable edges (from precomputed tables)
	unsigned int a1a8, h1h8;
	unsigned long long stable_edge;

	__m128i	P0 = _mm_cvtsi64_si128(P);
	__m128i	O0 = _mm_cvtsi64_si128(O);
	__m128i	PO = _mm_unpacklo_epi8(O0, P0);
	stable_edge = edge_stability[_mm_extract_epi16(PO, 0)]
		| ((unsigned long long) edge_stability[_mm_extract_epi16(PO, 7)] << 56);

	PO = _mm_unpacklo_epi64(O0, P0);
	a1a8 = edge_stability[_mm_movemask_epi8(_mm_slli_epi64(PO, 7))];
	h1h8 = edge_stability[_mm_movemask_epi8(PO)];
	stable_edge |= unpackA1A8(a1a8) | unpackH1H8(h1h8);

	return stable_edge;
}
#endif

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
unsigned long long get_all_full_lines(const unsigned long long disc, V4DI *full)
{
	unsigned long long l8;
	__m128i l81, l79;
	__m256i	v4_disc, lr79;
	const __m128i kff  = _mm_set1_epi64x(0xffffffffffffffff);
#if 0 // PCMPEQQ
	static const V4DI m791 = {{ 0x0402010000804020, 0x2040800000010204, 0x0804020180402010, 0x1020408001020408 }};	// V8SI
	static const V4DI m792 = {{ 0x0000008040201008, 0x0000000102040810, 0x1008040201000000, 0x0810204080000000 }};
	static const V4DI m793 = {{ 0x0000804020100804, 0x0000010204081020, 0x2010080402010000, 0x0408102040800000 }};
	static const V4DI m794 = {{ 0x0080402010080402, 0x0001020408102040, 0x4020100804020100, 0x0204081020408000 }};
	static const V2DI m795 = {{ 0x8040201008040201, 0x0102040810204080 }};

	l81 = _mm_cvtsi64_si128(disc);		v4_disc = _mm256_broadcastq_epi64(l81);
	l81 = _mm_cmpeq_epi8(kff, l81);		lr79 = _mm256_and_si256(_mm256_cmpeq_epi32(_mm256_and_si256(v4_disc, m791.v4), m791.v4), m791.v4);
						lr79 = _mm256_or_si256(lr79, _mm256_and_si256(_mm256_cmpeq_epi64(_mm256_and_si256(v4_disc, m792.v4), m792.v4), m792.v4));
	l8 = disc;				lr79 = _mm256_or_si256(lr79, _mm256_and_si256(_mm256_cmpeq_epi64(_mm256_and_si256(v4_disc, m793.v4), m793.v4), m793.v4));
	l8 &= (l8 >> 8) | (l8 << 56);		lr79 = _mm256_or_si256(lr79, _mm256_and_si256(_mm256_cmpeq_epi64(_mm256_and_si256(v4_disc, m794.v4), m794.v4), m794.v4));
	l8 &= (l8 >> 16) | (l8 << 48);		l79 = _mm_and_si128(_mm_cmpeq_epi64(_mm_and_si128(_mm256_castsi256_si128(v4_disc), m795.v2), m795.v2), m795.v2);
	l8 &= (l8 >> 32) | (l8 << 32);		l79 = _mm_or_si128(l79, _mm_or_si128(_mm256_extracti128_si256(lr79, 1), _mm256_castsi256_si128(lr79)));

#elif 0 // PCMPEQD
	__m256i lm79;
	static const V4DI m790 = {{ 0x80c0e0f0783c1e0f, 0x0103070f1e3c78f0, 0x70381c0e07030100, 0x0e1c3870e0c08000 }};
	static const V4DI m791 = {{ 0x0402010000804020, 0x2040800000010204, 0x0804020180402010, 0x1020408001020408 }};	// V8SI
	static const V4DI m792 = {{ 0x2010884440201088, 0x0408112202040811, 0x2211080411080402, 0x4488102088102040 }};	// V8SI
	static const V4DI m793 = {{ 0x8844221110884422, 0x1122448808112244, 0x0000000044221108, 0x0000000022448810 }};	// V8SI

	l81 = _mm_cvtsi64_si128(disc);		v4_disc = _mm256_broadcastq_epi64(l81);
	l81 = _mm_cmpeq_epi8(kff, l81);		lm79 = _mm256_and_si256(v4_disc, m790.v4);
						lm79 = _mm256_or_si256(lm79, _mm256_shuffle_epi32(lm79, 0xb1));
	l8 = disc;				lr79 = _mm256_and_si256(_mm256_cmpeq_epi32(_mm256_and_si256(lm79, m792.v4), m792.v4), m792.v4);
	l8 &= (l8 >> 8) | (l8 << 56);		lr79 = _mm256_or_si256(lr79, _mm256_and_si256(_mm256_cmpeq_epi32(_mm256_and_si256(lm79, m793.v4), m793.v4), m793.v4));
	l8 &= (l8 >> 16) | (l8 << 48);		lr79 = _mm256_and_si256(_mm256_or_si256(lr79, _mm256_shuffle_epi32(lr79, 0xb1)), m790.v4);
	l8 &= (l8 >> 32) | (l8 << 32);		lr79 = _mm256_or_si256(lr79, _mm256_and_si256(_mm256_cmpeq_epi32(_mm256_and_si256(v4_disc, m791.v4), m791.v4), m791.v4));
						l79 = _mm_or_si128(_mm256_extracti128_si256(lr79, 1), _mm256_castsi256_si128(lr79));

#else // Kogge-Stone
	const __m128i mcpyswap = _mm_set_epi8(0, 1, 2, 3, 4, 5, 6, 7, 7, 6, 5, 4, 3, 2, 1, 0);
	const __m128i mbswapll = _mm_set_epi8(8, 9, 10, 11, 12, 13, 14, 15, 0, 1, 2, 3, 4, 5, 6, 7);
	static const V4DI shiftlr[] = {{{ 9, 7, 7, 9 }}, {{ 18, 14, 14, 18 }}, {{ 36, 28, 28, 36 }}};
	static const V4DI e790 = {{ 0xff80808080808080, 0xff01010101010101, 0xff01010101010101, 0xff80808080808080 }};
	static const V4DI e791 = {{ 0xffffc0c0c0c0c0c0, 0xffff030303030303, 0xffff030303030303, 0xffffc0c0c0c0c0c0 }};
	static const V4DI e792 = {{ 0xfffffffff0f0f0f0, 0xffffffff0f0f0f0f, 0xffffffff0f0f0f0f, 0xfffffffff0f0f0f0 }};

	l81 = _mm_cvtsi64_si128(disc);		v4_disc = _mm256_castsi128_si256(_mm_shuffle_epi8(l81, mcpyswap));
	l81 = _mm_cmpeq_epi8(kff, l81);		lr79 = _mm256_permute4x64_epi64(v4_disc, 0x50);	// disc, disc, rdisc, rdisc
						lr79 = _mm256_and_si256(lr79, _mm256_or_si256(e790.v4, _mm256_srlv_epi64(lr79, shiftlr[0].v4)));
	l8 = disc;				lr79 = _mm256_and_si256(lr79, _mm256_or_si256(e791.v4, _mm256_srlv_epi64(lr79, shiftlr[1].v4)));
	l8 &= (l8 >> 8) | (l8 << 56);		lr79 = _mm256_and_si256(lr79, _mm256_or_si256(e792.v4, _mm256_srlv_epi64(lr79, shiftlr[2].v4)));
	l8 &= (l8 >> 16) | (l8 << 48);		l79 = _mm_shuffle_epi8(_mm256_extracti128_si256(lr79, 1), mbswapll);
	l8 &= (l8 >> 32) | (l8 << 32);		l79 = _mm_and_si128(l79, _mm256_castsi256_si128(lr79));

#endif
	l81 = _mm_insert_epi64(l81, l8, 1);
	full->v4 = _mm256_insertf128_si256(_mm256_castsi128_si256(l81), l79, 1);
	l81 = _mm_and_si128(l81, l79);
	return _mm_cvtsi128_si64(l81) & _mm_extract_epi64(l81, 1);
}

int get_stability_fulls_given(unsigned long long P, unsigned long long O, unsigned long long allfull, V4DI *full)
{
	unsigned long long stable, P_central;
	__m128i	v2_stable, v2_old_stable, v2_P_central;
	__m256i	v4_stable;
	const __m256i shift1897 = _mm256_set_epi64x(7, 9, 8, 1);

	// compute the exact stable edges (from precomputed tables)
	stable = get_stable_edge(P, O);

	// add full lines
	P_central = (P & 0x007e7e7e7e7e7e00);
	stable |= (allfull & P_central);

	if (stable == 0)
		return 0;

	// now compute the other stable discs (ie discs touching another stable disc in each flipping direction).
	v2_stable = _mm_cvtsi64_si128(stable);
	v2_P_central = _mm_cvtsi64_si128(P_central);
	do {
		v2_old_stable = v2_stable;
		v4_stable = _mm256_broadcastq_epi64(v2_stable);
		v4_stable = _mm256_or_si256(_mm256_or_si256(_mm256_srlv_epi64(v4_stable, shift1897), _mm256_sllv_epi64(v4_stable, shift1897)), full->v4);
		v2_stable = _mm_and_si128(_mm256_castsi256_si128(v4_stable), _mm256_extracti128_si256(v4_stable, 1));
		v2_stable = _mm_and_si128(v2_stable, _mm_unpackhi_epi64(v2_stable, v2_stable));
		v2_stable = _mm_or_si128(v2_old_stable, _mm_and_si128(v2_stable, v2_P_central));
	} while (!_mm_testc_si128(v2_old_stable, v2_stable));

	return bit_count(_mm_cvtsi128_si64(v2_stable));
}

#elif defined(hasNeon)
unsigned long long get_all_full_lines(const unsigned long long disc, V4DI *full)
{
	unsigned long long l8;
	uint8x8_t l01;
	uint64x2_t l79, r79;
	const uint64x2_t e790 = vdupq_n_u64(0x007f7f7f7f7f7f7f);
	const uint64x2_t e791 = vdupq_n_u64(0xfefefefefefefe00);
	const uint64x2_t e792 = vdupq_n_u64(0x00003f3f3f3f3f3f);
	const uint64x2_t e793 = vdupq_n_u64(0x0f0f0f0ff0f0f0f0);

	l01 = vcreate_u8(disc);			l79 = r79 = vreinterpretq_u64_u8(vcombine_u8(l01, vrev64_u8(l01)));
	l01 = vceq_u8(l01, vdup_n_u8(0xff));	l79 = vandq_u64(l79, vornq_u64(vshrq_n_u64(l79, 9), e790));
	full->ull[0] = vget_lane_u64(vreinterpret_u64_u8(l01), 0);
						r79 = vandq_u64(r79, vornq_u64(vshlq_n_u64(r79, 9), e791));
	l8 = disc;				l79 = vbicq_u64(l79, vbicq_u64(e792, vshrq_n_u64(l79, 18)));	// De Morgan
	l8 &= (l8 >> 8) | (l8 << 56);		r79 = vbicq_u64(r79, vshlq_n_u64(vbicq_u64(e792, r79), 18));
	l8 &= (l8 >> 16) | (l8 << 48);		l79 = vandq_u64(vandq_u64(l79, r79), vorrq_u64(e793, vsliq_n_u64(vshrq_n_u64(l79, 36), r79, 36)));
	l8 &= (l8 >> 32) | (l8 << 32);		full->ull[2] = vgetq_lane_u64(l79, 0);
	full->ull[1] = l8;			full->ull[3] = vertical_mirror(vgetq_lane_u64(l79, 1));

	return full->ull[0] & l8 & full->ull[2] & full->ull[3];
}

#else	// 1 CPU, 3 SSE
unsigned long long get_all_full_lines(const unsigned long long disc, V4DI *full)
{
	unsigned long long l8;
	__m128i l01, l79, r79;	// full lines
	const __m128i kff  = _mm_set1_epi64x(0xffffffffffffffff);
	const __m128i e790 = _mm_set1_epi64x(0xff80808080808080);
	const __m128i e791 = _mm_set1_epi64x(0x01010101010101ff);
	const __m128i e792 = _mm_set1_epi64x(0x00003f3f3f3f3f3f);
	const __m128i e793 = _mm_set1_epi64x(0x0f0f0f0ff0f0f0f0);

	l01 = l79 = _mm_cvtsi64_si128(disc);	l79 = r79 = _mm_unpacklo_epi64(l79, _mm_cvtsi64_si128(vertical_mirror(disc)));
	l01 = _mm_cmpeq_epi8(kff, l01);		l79 = _mm_and_si128(l79, _mm_or_si128(e790, _mm_srli_epi64(l79, 9)));
	_mm_storel_epi64(&full->v2[0], l01);	r79 = _mm_and_si128(r79, _mm_or_si128(e791, _mm_slli_epi64(r79, 9)));
						l79 = _mm_andnot_si128(_mm_andnot_si128(_mm_srli_epi64(l79, 18), e792), l79);	// De Morgan
	l8 = disc;				r79 = _mm_andnot_si128(_mm_slli_epi64(_mm_andnot_si128(r79, e792), 18), r79);
	l8 &= (l8 >> 8) | (l8 << 56);		l79 = _mm_and_si128(_mm_and_si128(l79, r79), _mm_or_si128(e793, _mm_or_si128(_mm_srli_epi64(l79, 36), _mm_slli_epi64(r79, 36))));
	l8 &= (l8 >> 16) | (l8 << 48);		_mm_storel_epi64(&full->v2[1], l79);	// full->ull[2]
	l8 &= (l8 >> 32) | (l8 << 32);		full->ull[3] = vertical_mirror(_mm_cvtsi128_si64(_mm_unpackhi_epi64(l79, l79)));
	full->ull[1] = l8;

	return full->ull[0] & l8 & full->ull[2] & full->ull[3];
}

#endif
#endif // hasSSE2/hasNeon
