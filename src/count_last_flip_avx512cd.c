/**
 * @file count_last_flip_avx512cd.c
 *
 * A function is provided to count the number of fipped disc of the last move.
 *
 * Count last flip using the flip_avx512cd way.
 * For optimization purpose, the value returned is twice the number of flipped
 * disc, to facilitate the computation of disc difference.
 *
 * @date 2023 - 2025
 * @author Toshihiko Okuhara
 * @version 4.5
 * 
 */

#include "bit.h"

extern	const V8DI lrmask[66];	// in flip_avx512cd.c

#ifdef AVX512_PREFER512
	#define	SI256(x)	_mm512_castsi512_si256(x)
#else
	#define	SI256(x)	x
#endif

/**
 * Count last flipped discs when playing on the last empty.
 *
 * @param pos the last empty square.
 * @param P player's disc pattern.
 * @return flipped disc count.
 */

int last_flip(int pos, unsigned long long P)
{
	__m256i PP = _mm256_set1_epi64x(P);
	__m256i minusone = _mm256_set1_epi64x(-1);
	__m256i one = _mm256_srli_epi64(minusone, 63);	// better than _mm256_set1_epi64(1) or _mm256_abs_epi64(minusone) on MSVC
	__m256i mask3F = _mm256_srli_epi64(minusone, 58);
	__m256i flip, outflank, eraser, rmask, lmask;
	__m128i flip2;

		// left: look for player LS1B
	lmask = lrmask[pos].v4[1];
	outflank = _mm256_and_si256(PP, lmask);
		// set below LS1B if P is in lmask
	flip = _mm256_sub_epi64(outflank, _mm256_min_epu64(outflank, one));
	// flip = _mm256_and_si256(_mm256_andnot_si256(outflank, flip), lmask);
	flip = _mm256_ternarylogic_epi64(outflank, flip, lmask, 0x08);

		// right: look for player bit with lzcnt
	rmask = lrmask[pos].v4[0];
	eraser = _mm256_srlv_epi64(minusone, _mm256_and_si256(_mm256_lzcnt_epi64(_mm256_and_si256(PP, rmask)), mask3F));
	// flip = _mm256_or_si256(flip, _mm256_andnot_si256(eraser, rmask));
	flip = _mm256_ternarylogic_epi64(flip, eraser, rmask, 0xf2);

	flip2 = _mm_or_si128(_mm256_castsi256_si128(flip), _mm256_extracti128_si256(flip, 1));
	return 2 * bit_count(_mm_cvtsi128_si64(_mm_or_si128(flip2, _mm_unpackhi_epi64(flip2, flip2))));
}

/**
 * @brief Get the final score.
 *
 * Get the final score, when 1 empty square remain.
 * The original code has been adapted from Zebra by Gunnar Anderson.
 *
 * @param OP     Board to evaluate. (O ignored)
 * @param alpha  Alpha bound. (beta - 1)
 * @param pos    Last empty square to play.
 * @return       The final score, as a disc difference.
 */
#ifdef LASTFLIP_HIGHCUT

int vectorcall board_score_sse_1(__m128i OP, const int alpha, const int pos)
{
	int score = 2 * bit_count(_mm_cvtsi128_si64(OP)) - 64 + 2;	// = (bit_count(P) + 1) - (SCORE_MAX - 1 - bit_count(P))
		// if player can move, final score > this score.
		// if player pass then opponent play, final score < score - 1 (cancel P) - 1 (last O).
		// if both pass, score - 1 (cancel P) - 1 (empty for O) <= final score <= score (empty for P).
	int nflip;
	__m256i F4, eraser, mask3F;
	__m128i F2;
  #ifdef AVX512_PREFER512
	__m512i P8 = _mm512_broadcastq_epi64(OP);
	__m512i minusone = _mm512_set1_epi64(-1);
	__m512i mask0 = lrmask[pos].v8;
	__mmask8 fP0 = _mm512_test_epi64_mask(P8, mask0);	// P exists on mask

	if (score > alpha) {	// if player can move, high cut-off will occur regardress of n_flips.
		__m512i mO0 = _mm512_andnot_epi64(P8, mask0);
		__mmask8 fO0 = _mm512_test_epi64_mask(mO0, _mm512_broadcastq_epi64(_mm_cvtsi64_si128(NEIGHBOUR[pos])));	// MSVC fails to use BCST for _mm512_set1_epi64
		if (_ktestz_mask8_u8(fP0, fO0)) {	// pass (16%)
			// nflip = last_flip(pos, ~P);
				// left: set below LS1B if O is in lmask (in upper i64x4)
			__m512i one = _mm512_srli_epi64(minusone, 63);	// better than _mm512_set1_epi64(1) or _mm512_abs_epi64(minusone) on MSVC
			__m512i F8 = _mm512_sub_epi64(mO0, _mm512_min_epu64(mO0, one));
			// F8 = _mm512_and_si512(_mm512_andnot_si512(mO0, F8), mask0);
			F8 = _mm512_ternarylogic_epi64(mO0, F8, mask0, 0x08);
			F4 = _mm512_extracti64x4_epi64(F8, 1);

  #else
	__m256i P4 = _mm256_broadcastq_epi64(OP);
	__m256i minusone = _mm256_set1_epi64x(-1);
	__m256i mask0 = lrmask[pos].v4[0];
	__m256i mask1 = lrmask[pos].v4[1];
	__mmask8 fP0 = _mm256_test_epi64_mask(P4, mask0);	// P exists on mask
	__mmask8 fP1 = _mm256_test_epi64_mask(P4, mask1);

	if (score > alpha) {	// if player can move, high cut-off will occur regardress of n_flips.
		__m256i mO0 = _mm256_maskz_andnot_epi64(fP0, P4, mask0);	// masked O, clear if all O
		__m256i mO1 = _mm256_maskz_andnot_epi64(fP1, P4, mask1);	// (all O = all P = 0 flip)
		if (_mm256_testz_si256(_mm256_or_si256(mO1, mO0), _mm256_set1_epi64x(NEIGHBOUR[pos]))) {	// pass (16%)
			// nflip = last_flip(pos, ~P);
				// left: set below LS1B if O is in lmask
			__m256i one = _mm256_srli_epi64(minusone, 63);
			F4 = _mm256_sub_epi64(mO1, _mm256_min_epu64(mO1, one));
			// F4 = _mm256_and_si256(_mm256_andnot_si256(mO1, F4), mask1);
			F4 = _mm256_ternarylogic_epi64(mO1, F4, mask1, 0x08);
  #endif
				// right: clear all bits lower than outflank
			mask3F = _mm256_srli_epi64(SI256(minusone), 58);
			eraser = _mm256_srlv_epi64(SI256(minusone), _mm256_and_si256(_mm256_lzcnt_epi64(SI256(mO0)), mask3F));
			// F4 = _mm256_or_si256(F4, _mm256_andnot_si256(eraser, SI256(mask0)));
			F4 = _mm256_ternarylogic_epi64(F4, eraser, SI256(mask0), 0xf2);

			F2 = _mm_or_si128(_mm256_castsi256_si128(F4), _mm256_extracti128_si256(F4, 1));
			nflip = bit_count(_mm_cvtsi128_si64(_mm_or_si128(F2, _mm_unpackhi_epi64(F2, F2))));
				// last square for O if O can move or score <= 0
			score -= (nflip + (int)((-nflip | (score - 1)) < 0)) * 2;

		} else	score += 2;	// lazy high cut-off, return min flip

	} else {	// if player cannot move, low cut-off will occur whether opponent can move.
  #ifdef AVX512_PREFER512
			// left: set below LS1B (in upper i64x4)
		__m512i mP = _mm512_and_si512(P8, mask0);
		__m512i F8 = _mm512_maskz_add_epi64(fP0, mP, minusone);
		// F8 = _mm512_and_si512(_mm512_andnot_si512(mP, F8), mask0);
		F8 = _mm512_ternarylogic_epi64(mP, F8, mask0, 0x08);
		F4 = _mm512_extracti64x4_epi64(F8, 1);
  #else
			// left: set below LS1B
		__m256i mP = _mm256_and_si256(P4, mask1);
		F4 = _mm256_maskz_add_epi64(fP1, mP, minusone);
		// F4 = _mm256_and_si256(_mm256_andnot_si256(mP, F4), mask1);
		F4 = _mm256_ternarylogic_epi64(mP, F4, mask1, 0x08);

		mP = _mm256_and_si256(P4, mask0);
  #endif
			// right: clear all bits lower than outflank
		eraser = _mm256_srlv_epi64(SI256(minusone), _mm256_maskz_lzcnt_epi64(fP0, SI256(mP)));
		// F4 = _mm256_or_si256(F4, _mm256_andnot_si256(eraser, SI256(mask0)));
		F4 = _mm256_ternarylogic_epi64(F4, eraser, SI256(mask0), 0xf2);

		F2 = _mm_or_si128(_mm256_castsi256_si128(F4), _mm256_extracti128_si256(F4, 1));
		nflip = bit_count(_mm_cvtsi128_si64(_mm_or_si128(F2, _mm_unpackhi_epi64(F2, F2))));
		score += nflip * 2;

		// if nflip == 0, score <= alpha so lazy low cut-off
	}

	return score;
}

#elif defined(LASTFLIP_LOWCUT)

int vectorcall board_score_sse_1(__m128i OP, const int alpha, const int pos)
{
	__m256i P4 = _mm256_broadcastq_epi64(OP);
	__m256i minusone = _mm256_set1_epi64x(-1);
	__m256i one = _mm256_srli_epi64(minusone, 63);
	__m256i mask3F = _mm256_srli_epi64(minusone, 58);
	__m256i rmask = lrmask[pos].v4[0];
	__m256i lmask = lrmask[pos].v4[1];
	__m256i flip, outflank, eraser;
	__m128i flip2;
	int	score, score2, n_flips;

	SEARCH_STATS(++statistics.n_solve_1);

		// left: look for player LS1B
	outflank = _mm256_and_si256(P4, lmask);
		// set below LS1B if P is in lmask
	flip = _mm256_sub_epi64(outflank, _mm256_min_epu64(outflank, one));
	// flip = _mm256_and_si256(_mm256_andnot_si256(outflank, flip), lmask);
	flip = _mm256_ternarylogic_epi64(outflank, flip, lmask, 0x08);

		// right: look for player bit with lzcnt
	eraser = _mm256_srlv_epi64(minusone, _mm256_and_si256(_mm256_lzcnt_epi64(_mm256_and_si256(P4, rmask)), mask3F));
	// flip = _mm256_or_si256(flip, _mm256_andnot_si256(eraser, rmask));
	flip = _mm256_ternarylogic_epi64(flip, eraser, rmask, 0xf2);

	flip2 = _mm_or_si128(_mm256_castsi256_si128(flip), _mm256_extracti128_si256(flip, 1));
	flip2 = _mm_or_si128(flip2, _mm_unpackhi_epi64(flip2, flip2));
	score = 2 * bit_count(_mm_cvtsi128_si64(_mm_or_si128(_mm256_castsi256_si128(P4), flip2))) - 64 + 2;

	if (_mm_testz_si128(flip2, flip2)) {	// (23%)
		score2 = score - 2;	// empty for opponent
		if (score <= 0)
			score = score2;
		if (score > alpha) {	// lazy cut-off (40%)
			outflank = _mm256_andnot_si256(P4, lmask);
				// set below LS1B if P is in lmask
			flip = _mm256_sub_epi64(outflank, _mm256_min_epu64(outflank, _mm256_set1_epi64x(1)));
			// flip = _mm256_and_si256(_mm256_andnot_si256(outflank, flip), lmask);
			flip = _mm256_ternarylogic_epi64(outflank, flip, lmask, 0x08);

				// right: look for player bit with lzcnt
			eraser = _mm256_srlv_epi64(minusone, _mm256_and_si256(_mm256_lzcnt_epi64(_mm256_andnot_si256(P4, rmask)), mask3F));
			// flip = _mm256_or_si256(flip, _mm256_andnot_si256(eraser, rmask));
			flip = _mm256_ternarylogic_epi64(flip, eraser, rmask, 0xf2);

			flip2 = _mm_or_si128(_mm256_castsi256_si128(flip), _mm256_extracti128_si256(flip, 1));
			n_flips = 2 * bit_count(_mm_cvtsi128_si64(_mm_or_si128(flip2, _mm_unpackhi_epi64(flip2, flip2))));
			if (n_flips != 0)	// (98%)
				score = score2 - n_flips;
		}
	}

	return score;
}

#else	// P & O simul flip

int vectorcall board_score_sse_1(__m128i OP, const int alpha, const int pos)
{
	int	score;
	__m256i p_flip, o_flip, opop_flip;
	__m128i P2;
	__mmask8 op_pass;
  #ifdef AVX512_PREFER512
  // branchless AVX512(512) lastflip (2.71s on skylake, 2.48 on icelake, 2.15s on Zen4)
	__m512i minusone = _mm512_set1_epi64(-1);
	__m512i one = _mm512_srli_epi64(minusone, 63);
	__m512i mask3F = _mm512_srli_epi64(minusone, 58);
	__m512i po_outflank, po_flip, po_eraser, mask;
	__m512i P8 = _mm512_broadcastq_epi64(OP);
	__m512i P4O4 = _mm512_xor_si512(P8, _mm512_zextsi256_si512(_mm512_castsi512_si256(minusone)));

		// left: look for player LS1B
	mask = _mm512_broadcast_i64x4(lrmask[pos].v4[1]);
	po_outflank = _mm512_and_si512(P4O4, mask);
		// set below LS1B if P is in lmask
	po_flip = _mm512_sub_epi64(po_outflank, _mm512_min_epu64(po_outflank, one));
	// po_flip = _mm512_and_si512(_mm512_andnot_si512(po_outflank, po_flip), mask);
	po_flip = _mm512_ternarylogic_epi64(po_outflank, po_flip, mask, 0x08);

		// right: clear all bits lower than outflank
	mask = _mm512_broadcast_i64x4(lrmask[pos].v4[0]);
	po_outflank = _mm512_and_si512(P4O4, mask);
	po_eraser = _mm512_srlv_epi64(minusone, _mm512_and_si512(_mm512_lzcnt_epi64(po_outflank), mask3F));
	// po_flip = _mm512_or_si512(po_flip, _mm512_andnot_si512(po_eraser, mask));
	po_flip = _mm512_ternarylogic_epi64(po_flip, po_eraser, mask, 0xf2);

	p_flip = _mm512_extracti64x4_epi64(po_flip, 1);
	o_flip = _mm512_castsi512_si256(po_flip);
	P2 = _mm512_castsi512_si128(P8);

  #else
  // branchless AVX512(256) lastflip (2.61s on skylake, 2.38 on icelake, 2.13s on Zen4)
	__m256i minusone = _mm256_set1_epi64x(-1);
	__m256i one = _mm256_srli_epi64(minusone, 63);
	__m256i mask3F = _mm256_srli_epi64(minusone, 58);
	__m256i p_outflank, o_outflank, p_eraser, o_eraser, mask;
	__m256i P4 = _mm256_broadcastq_epi64(OP);

		// left: look for player LS1B
	mask = lrmask[pos].v4[1];
	p_outflank = _mm256_and_si256(P4, mask);	o_outflank = _mm256_andnot_si256(P4, mask);
		// set below LS1B if P is in lmask
	p_flip = _mm256_sub_epi64(p_outflank, _mm256_min_epu64(p_outflank, one));
		// set below LS1B if O is in lmask
	o_flip = _mm256_sub_epi64(o_outflank, _mm256_min_epu64(o_outflank, one));
	// p_flip = _mm256_and_si256(_mm256_andnot_si256(p_outflank, p_flip), mask);
	p_flip = _mm256_ternarylogic_epi64(p_outflank, p_flip, mask, 0x08);
	// o_flip = _mm256_and_si256(_mm256_andnot_si256(o_outflank, o_flip), mask);
	o_flip = _mm256_ternarylogic_epi64(o_outflank, o_flip, mask, 0x08);

		// right: clear all bits lower than outflank
	mask = lrmask[pos].v4[0];
	p_outflank = _mm256_and_si256(P4, mask);	o_outflank = _mm256_andnot_si256(P4, mask);
	p_eraser = _mm256_srlv_epi64(minusone, _mm256_and_si256(_mm256_lzcnt_epi64(p_outflank), mask3F));
	o_eraser = _mm256_srlv_epi64(minusone, _mm256_and_si256(_mm256_lzcnt_epi64(o_outflank), mask3F));
	// p_flip = _mm256_or_si256(p_flip, _mm256_andnot_si256(p_eraser, mask));
	p_flip = _mm256_ternarylogic_epi64(p_flip, p_eraser, mask, 0xf2);
	// o_flip = _mm256_or_si256(o_flip, _mm256_andnot_si256(o_eraser, mask));
	o_flip = _mm256_ternarylogic_epi64(o_flip, o_eraser, mask, 0xf2);

	P2 = _mm256_castsi256_si128(P4);
  #endif
	opop_flip = _mm256_or_si256(_mm256_unpacklo_epi64(p_flip, o_flip), _mm256_unpackhi_epi64(p_flip, o_flip));
	// OP = _mm_xor_si128(_mm_or_si128(_mm256_castsi256_si128(opop_flip), _mm256_extracti128_si256(opop_flip, 1)), P2);
	OP = _mm_ternarylogic_epi64(_mm256_castsi256_si128(opop_flip), _mm256_extracti128_si256(opop_flip, 1), P2, 0x56);
	op_pass = _mm_cmpeq_epi64_mask(OP, P2);
	OP = _mm_mask_unpackhi_epi64(OP, op_pass, OP, OP);	// use O if p_pass
	score = bit_count(_mm_cvtsi128_si64(OP));
		// last square for P if not P pass or (O pass and score >= 32)
	// score += ((~op_pass & 1) | ((op_pass >> 1) & (score >= 32)));
	score += (~op_pass | ((op_pass >> 1) & (score >> 5))) & 1;
	(void) alpha;	// no lazy cut-off
	return score * 2 - 64;	// = bit_count(P) - (SCORE_MAX - bit_count(P))
}

#endif

int board_score_1(unsigned long long player, int alpha, int x)
{
	return board_score_sse_1(_mm_cvtsi64_si128(player), alpha, x);
}
