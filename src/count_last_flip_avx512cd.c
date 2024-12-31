/**
 * @file count_last_flip_avx512cd.c
 *
 * A function is provided to count the number of fipped disc of the last move.
 *
 * Count last flip using the flip_avx512cd way.
 * For optimization purpose, the value returned is twice the number of flipped
 * disc, to facilitate the computation of disc difference.
 *
 * @date 2023 - 2024
 * @author Toshihiko Okuhara
 * @version 4.5
 * 
 */

#include "bit.h"

extern	const V8DI lrmask[66];	// in flip_avx512cd.c

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
	__m256i	flip, outflank, eraser, rmask, lmask;
	__m128i	flip2;

		// left: look for player LS1B
	lmask = lrmask[pos].v4[0];
	outflank = _mm256_and_si256(PP, lmask);
		// set below LS1B if P is in lmask
	flip = _mm256_maskz_add_epi64(_mm256_test_epi64_mask(PP, lmask), outflank, _mm256_set1_epi64x(-1));
	// flip = _mm256_and_si256(_mm256_andnot_si256(outflank, flip), lmask);
	flip = _mm256_ternarylogic_epi64(outflank, flip, lmask, 0x08);

		// right: look for player bit with lzcnt
	rmask = lrmask[pos].v4[1];
	eraser = _mm256_srlv_epi64(_mm256_set1_epi64x(-1),
		_mm256_maskz_lzcnt_epi64(_mm256_test_epi64_mask(PP, rmask), _mm256_and_si256(PP, rmask)));
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
 * @param PO     Board to evaluate. (O ignored)
 * @param alpha  Alpha bound. (beta - 1)
 * @param pos    Last empty square to play.
 * @return       The final score, as a disc difference.
 */
#ifdef SIMULLASTFLIP512
// branchless AVX512(512) lastflip (2.71s on skylake, 2.48 on icelake, 2.15s on Zen4)

inline int vectorcall board_score_sse_1(__m128i OP, const int alpha, const int pos)
{
	int	score;
	__m512i	op_outflank, op_flip, op_eraser, mask;
	__m256i	o_flip, opop_flip;
	__mmask8 op_pass;
	__m512i	O4P4 = _mm512_xor_si512(_mm512_broadcastq_epi64(OP),
		 _mm512_set_epi64(-1, -1, -1, -1, 0, 0, 0, 0));

		// left: look for player LS1B
	mask = _mm512_broadcast_i64x4(lrmask[pos].v4[0]);
	op_outflank = _mm512_and_si512(O4P4, mask);
		// set below LS1B if P is in lmask
	op_flip = _mm512_maskz_add_epi64(_mm512_test_epi64_mask(op_outflank, op_outflank),
		op_outflank, _mm512_set1_epi64(-1));
	// op_flip = _mm512_and_si512(_mm512_andnot_si512(op_outflank, op_flip), mask);
	op_flip = _mm512_ternarylogic_epi64(op_outflank, op_flip, mask, 0x08);

		// right: clear all bits lower than outflank
	mask = _mm512_broadcast_i64x4(lrmask[pos].v4[1]);
	op_outflank = _mm512_and_si512(O4P4, mask);
	op_eraser = _mm512_srlv_epi64(_mm512_set1_epi64(-1),
		_mm512_maskz_lzcnt_epi64(_mm512_test_epi64_mask(op_outflank, op_outflank), op_outflank));
	// op_flip = _mm512_or_si512(op_flip, _mm512_andnot_si512(op_eraser, mask));
	op_flip = _mm512_ternarylogic_epi64(op_flip, op_eraser, mask, 0xf2);

	o_flip = _mm512_extracti64x4_epi64(op_flip, 1);
	opop_flip = _mm256_or_si256(_mm256_unpacklo_epi64(_mm512_castsi512_si256(op_flip), o_flip),
		_mm256_unpackhi_epi64(_mm512_castsi512_si256(op_flip), o_flip));
	OP = _mm_xor_si128(_mm512_castsi512_si128(O4P4),
		_mm_or_si128(_mm256_castsi256_si128(opop_flip), _mm256_extracti128_si256(opop_flip, 1)));
	op_pass = _mm_cmpeq_epi64_mask(OP, _mm512_castsi512_si128(O4P4));
	OP = _mm_mask_unpackhi_epi64(OP, op_pass, OP, OP);	// use O if p_pass
	score = bit_count(_mm_cvtsi128_si64(OP));
		// last square for P if not P pass or (O pass and score >= 32)
	// score += ((~op_pass & 1) | ((op_pass >> 1) & (score >= 32)));
	score += (~op_pass | ((op_pass >> 1) & (score >> 5))) & 1;
	(void) alpha;	// no lazy cut-off
	return score * 2 - SCORE_MAX;	// = bit_count(P) - (SCORE_MAX - bit_count(P))
}

#elif defined(SIMULLASTFLIP)
// branchless AVX512(256) lastflip (2.61s on skylake, 2.38 on icelake, 2.13s on Zen4)

inline int vectorcall board_score_sse_1(__m128i OP, const int alpha, const int pos)
{
	int	score;
	__m256i	p_flip, o_flip, p_outflank, o_outflank, p_eraser, o_eraser, mask, opop_flip;
	__mmask8 op_pass;
	__m256i	P4 = _mm256_broadcastq_epi64(OP);

		// left: look for player LS1B
	mask = lrmask[pos].v4[0];
	p_outflank = _mm256_and_si256(P4, mask);	o_outflank = _mm256_andnot_si256(P4, mask);
		// set below LS1B if P is in lmask
	p_flip = _mm256_maskz_add_epi64(_mm256_test_epi64_mask(P4, mask), p_outflank, _mm256_set1_epi64x(-1));
		// set below LS1B if O is in lmask
	o_flip = _mm256_maskz_add_epi64(_mm256_test_epi64_mask(o_outflank, o_outflank), o_outflank, _mm256_set1_epi64x(-1));
	// p_flip = _mm256_and_si256(_mm256_andnot_si256(p_outflank, p_flip), mask);
	p_flip = _mm256_ternarylogic_epi64(p_outflank, p_flip, mask, 0x08);
	// o_flip = _mm256_and_si256(_mm256_andnot_si256(o_outflank, o_flip), mask);
	o_flip = _mm256_ternarylogic_epi64(o_outflank, o_flip, mask, 0x08);

		// right: clear all bits lower than outflank
	mask = lrmask[pos].v4[1];
	p_outflank = _mm256_and_si256(P4, mask);	o_outflank = _mm256_andnot_si256(P4, mask);
	p_eraser = _mm256_srlv_epi64(_mm256_set1_epi64x(-1),
		_mm256_maskz_lzcnt_epi64(_mm256_test_epi64_mask(P4, mask), p_outflank));
	o_eraser = _mm256_srlv_epi64(_mm256_set1_epi64x(-1),
		_mm256_maskz_lzcnt_epi64(_mm256_test_epi64_mask(o_outflank, o_outflank), o_outflank));
	// p_flip = _mm256_or_si256(p_flip, _mm256_andnot_si256(p_eraser, mask));
	p_flip = _mm256_ternarylogic_epi64(p_flip, p_eraser, mask, 0xf2);
	// o_flip = _mm256_or_si256(o_flip, _mm256_andnot_si256(o_eraser, mask));
	o_flip = _mm256_ternarylogic_epi64(o_flip, o_eraser, mask, 0xf2);

	opop_flip = _mm256_or_si256(_mm256_unpacklo_epi64(p_flip, o_flip), _mm256_unpackhi_epi64(p_flip, o_flip));
	OP = _mm_xor_si128(_mm256_castsi256_si128(P4),
		_mm_or_si128(_mm256_castsi256_si128(opop_flip), _mm256_extracti128_si256(opop_flip, 1)));
	op_pass = _mm_cmpeq_epi64_mask(OP, _mm256_castsi256_si128(P4));
	OP = _mm_mask_unpackhi_epi64(OP, op_pass, OP, OP);	// use O if p_pass
	score = bit_count(_mm_cvtsi128_si64(OP));
		// last square for P if not P pass or (O pass and score >= 32)
	// score += ((~op_pass & 1) | ((op_pass >> 1) & (score >= 32)));
	score += (~op_pass | ((op_pass >> 1) & (score >> 5))) & 1;
	(void) alpha;	// no lazy cut-off
	return score * 2 - SCORE_MAX;	// = bit_count(P) - (SCORE_MAX - bit_count(P))
}

#elif defined(LASTFLIP_HIGHCUT)
// AVX512(256) NWS lazy high cut-off version (2.63s on skylake, 2.33 on icelake, 2.14s on Zen4)

inline int vectorcall board_score_sse_1(__m128i OP, const int alpha, const int pos)
{
	int score = 2 * bit_count(_mm_cvtsi128_si64(OP)) - SCORE_MAX + 2;	// = (bit_count(P) + 1) - (SCORE_MAX - 1 - bit_count(P))
		// if player can move, final score > this score.
		// if player pass then opponent play, final score < score - 1 (cancel P) - 1 (last O).
		// if both pass, score - 1 (cancel P) - 1 (empty for O) <= final score <= score (empty for P).
	__m256i P4 = _mm256_broadcastq_epi64(OP);
	__m256i lmask = lrmask[pos].v4[0];
	__m256i rmask = lrmask[pos].v4[1];
	__mmask16 lp = _mm256_test_epi64_mask(P4, lmask);	// P exists on mask
	__mmask16 rp = _mm256_test_epi64_mask(P4, rmask);
	__m256i F4, outflank, eraser, lmO, rmO;
	__m128i F2;
	int nflip;

	if (score > alpha) {	// if player can move, high cut-off will occur regardress of n_flips.
		lmO = _mm256_maskz_andnot_epi64(lp, P4, lmask);	// masked O, clear if all O
		rmO = _mm256_maskz_andnot_epi64(rp, P4, rmask);	// (all O = all P = 0 flip)
		if (_mm256_testz_si256(_mm256_or_si256(lmO, rmO), _mm256_set1_epi64x(NEIGHBOUR[pos]))) {
			// nflip = last_flip(pos, ~P);
				// left: set below LS1B if O is in lmask
			F4 = _mm256_maskz_add_epi64(_mm256_test_epi64_mask(lmO, lmO), lmO, _mm256_set1_epi64x(-1));
			// F4 = _mm256_and_si256(_mm256_andnot_si256(lmO, F4), lmask);
			F4 = _mm256_ternarylogic_epi64(lmO, F4, lmask, 0x08);

				// right: clear all bits lower than outflank
			eraser = _mm256_srlv_epi64(_mm256_set1_epi64x(-1),
				_mm256_maskz_lzcnt_epi64(_mm256_test_epi64_mask(rmO, rmO), rmO));
			// F4 = _mm256_or_si256(F4, _mm256_andnot_si256(eraser, rmask));
			F4 = _mm256_ternarylogic_epi64(F4, eraser, rmask, 0xf2);

			F2 = _mm_or_si128(_mm256_castsi256_si128(F4), _mm256_extracti128_si256(F4, 1));
			nflip = -bit_count(_mm_cvtsi128_si64(_mm_or_si128(F2, _mm_unpackhi_epi64(F2, F2))));
				// last square for O if O can move or score <= 0
			score += (nflip - (int)((nflip | (score - 1)) < 0)) * 2;

		} else	score += 2;	// lazy high cut-off, return min flip

	} else {	// if player cannot move, low cut-off will occur whether opponent can move.
			// left: set below LS1B if P is in lmask
		outflank = _mm256_and_si256(P4, lmask);
		F4 = _mm256_maskz_add_epi64(lp, outflank, _mm256_set1_epi64x(-1));
		// F4 = _mm256_and_si256(_mm256_andnot_si256(outflank, F4), lmask);
		F4 = _mm256_ternarylogic_epi64(outflank, F4, lmask, 0x08);

			// right: clear all bits lower than outflank
		outflank = _mm256_and_si256(P4, rmask);
		eraser = _mm256_srlv_epi64(_mm256_set1_epi64x(-1), _mm256_maskz_lzcnt_epi64(rp, outflank));
		// F4 = _mm256_or_si256(F4, _mm256_andnot_si256(eraser, rmask));
		F4 = _mm256_ternarylogic_epi64(F4, eraser, rmask, 0xf2);

		F2 = _mm_or_si128(_mm256_castsi256_si128(F4), _mm256_extracti128_si256(F4, 1));
		nflip = bit_count(_mm_cvtsi128_si64(_mm_or_si128(F2, _mm_unpackhi_epi64(F2, F2))));
		score += nflip * 2;

		// if nflip == 0, score <= alpha so lazy low cut-off
	}

	return score;
}

#else

inline int vectorcall board_score_sse_1(__m128i OP, const int alpha, const int pos)
{
	int score = 2 * bit_count(_mm_cvtsi128_si64(OP)) - SCORE_MAX + 2;	// = (bit_count(P) + 1) - (SCORE_MAX - 1 - bit_count(P))
		// if player can move, final score > this score.
		// if player pass then opponent play, final score < score - 1 (cancel P) - 1 (last O).
		// if both pass, score - 1 (cancel P) - 1 (empty for O) <= final score <= score (empty for P).
	__m256i P4 = _mm256_broadcastq_epi64(OP);
	__m256i lmask = lrmask[pos].v4[0];
	__m256i rmask = lrmask[pos].v4[1];
	__m256i	flip, outflank, eraser;
	__m128i	flip2;
	int score, score2, n_flips;

	SEARCH_STATS(++statistics.n_solve_1);

		// left: look for player LS1B
	outflank = _mm256_and_si256(P4, lmask);
		// set below LS1B if P is in lmask
	flip = _mm256_maskz_add_epi64(_mm256_test_epi64_mask(P4, lmask), outflank, _mm256_set1_epi64x(-1));
	// flip = _mm256_and_si256(_mm256_andnot_si256(outflank, flip), lmask);
	flip = _mm256_ternarylogic_epi64(outflank, flip, lmask, 0x08);

		// right: look for player bit with lzcnt
	eraser = _mm256_srlv_epi64(_mm256_set1_epi64x(-1),
		_mm256_maskz_lzcnt_epi64(_mm256_test_epi64_mask(P4, rmask), _mm256_and_si256(P4, rmask)));
	// flip = _mm256_or_si256(flip, _mm256_andnot_si256(eraser, rmask));
	flip = _mm256_ternarylogic_epi64(flip, eraser, rmask, 0xf2);

	flip2 = _mm_or_si128(_mm256_castsi256_si128(flip), _mm256_extracti128_si256(flip, 1));
	flip2 = _mm_or_si128(flip2, _mm_unpackhi_epi64(flip2, flip2));
	score = 2 * bit_count(_mm_cvtsi128_si64(_mm_or_si128(P, flip2))) - SCORE_MAX + 2;

	if (_mm_testz_si128(flip2, flip2)) {	// (23%)
		score2 = score - 2;	// empty for opponent
		if (score <= 0)
			score = score2;
		if (score > alpha) {	// lazy cut-off (40%)
			outflank = _mm256_andnot_si256(P4, lmask);
				// set below LS1B if P is in lmask
			flip = _mm256_maskz_add_epi64(_mm256_test_epi64_mask(outflank, lmask), outflank, _mm256_set1_epi64x(-1));
			// flip = _mm256_and_si256(_mm256_andnot_si256(outflank, flip), lmask);
			flip = _mm256_ternarylogic_epi64(outflank, flip, lmask, 0x08);

				// right: look for player bit with lzcnt
			outflank = _mm256_andnot_si256(P4, rmask);
			eraser = _mm256_srlv_epi64(_mm256_set1_epi64x(-1),
				_mm256_maskz_lzcnt_epi64(_mm256_test_epi64_mask(outflank, rmask), outflank));
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

#endif

int board_score_1(unsigned long long player, int alpha, int x) {
	return board_score_sse_1(_mm_cvtsi64_si128(player), alpha, x);
}
