/**
 * @file count_last_flip_avx_ppfill.c
 *
 * A function is provided to count the number of fipped disc of the last move.
 *
 * Count last flip using the flip_avx_ppfill way.
 * For optimization purpose, the value returned is twice the number of flipped
 * disc, to facilitate the computation of disc difference.
 *
 * @date 2025
 * @author Toshihiko Okuhara
 * @version 4.5
 * 
 */

#include "bit.h"

extern	const V8DI lrmask[66];	// in flip_avx_ppfill.c

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
	__m256i flip, outflank, eraser, rmask, lmask;
	__m128i flip2;

	rmask = lrmask[pos].v4[0];
		// isolate player MS1B by clearing lower shadow bits
	outflank = _mm256_and_si256(PP, rmask);
		// eraser = player's shadow
	eraser = _mm256_or_si256(outflank, _mm256_srlv_epi64(outflank, _mm256_set_epi64x(7, 9, 8, 1)));
	eraser = _mm256_or_si256(eraser, _mm256_srlv_epi64(eraser, _mm256_set_epi64x(14, 18, 16, 2)));
	flip = _mm256_andnot_si256(eraser, rmask);
	flip = _mm256_andnot_si256(_mm256_srlv_epi64(eraser, _mm256_set_epi64x(28, 36, 32, 4)), flip);
		// clear if no player bit, i.e. all opponent
	flip = _mm256_andnot_si256(_mm256_cmpeq_epi64(flip, rmask), flip);

	lmask = lrmask[pos].v4[1];
		// look for player LS1B
	outflank = _mm256_and_si256(PP, lmask);
	outflank = _mm256_and_si256(outflank, _mm256_sub_epi64(_mm256_setzero_si256(), outflank));	// LS1B
		// set all bits if outflank = 0, otherwise higher bits than outflank
	eraser = _mm256_sub_epi64(_mm256_cmpeq_epi64(outflank, _mm256_setzero_si256()), outflank);
	flip = _mm256_or_si256(flip, _mm256_andnot_si256(eraser, lmask));

	flip2 = _mm_or_si128(_mm256_castsi256_si128(flip), _mm256_extracti128_si256(flip, 1));
	flip2 = _mm_or_si128(flip2, _mm_shuffle_epi32(flip2, 0x4e));
	return 2 * bit_count(_mm_cvtsi128_si64(flip2));
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
// experimental AVX2 lastflip with lazy high cut-off version (a little slower)

int vectorcall board_score_sse_1(__m128i OP, const int alpha, const int pos)
{
	int score = 2 * bit_count(_mm_cvtsi128_si64(OP)) - 64 + 2;	// = (bit_count(P) + 1) - (SCORE_MAX - 1 - bit_count(P))
		// if player can move, final score > this score.
		// if player pass then opponent play, final score < score - 1 (cancel P) - 1 (last O).
		// if both pass, score - 1 (cancel P) - 1 (empty for O) <= final score <= score (empty for P).
	__m256i P4 = _mm256_broadcastq_epi64(OP);
	__m256i F4, lmask, rmask, outflank, eraser, lmO, rmO, lp, rp;
	__m128i F2;
	int nflip;

	rmask = lrmask[pos].v4[0];           		lmask = lrmask[pos].v4[1];
	rmO = _mm256_andnot_si256(P4, rmask);		lmO = _mm256_andnot_si256(P4, lmask);
	rp = _mm256_cmpeq_epi64(rmO, rmask); 		lp = _mm256_cmpeq_epi64(lmO, lmask);	// 0 if P exists on mask
	rmO = _mm256_andnot_si256(rp, rmO);  		lmO = _mm256_andnot_si256(lp, lmO);	// all O -> all P

	if (score > alpha) {	// if player can move, high cut-off will occur regardress of n_flips.
		if (_mm256_testz_si256(_mm256_or_si256(lmO, rmO), _mm256_set1_epi64x(NEIGHBOUR[pos]))) {	// pass (16%)
			// n_flips = last_flip(pos, ~P);
				// right: isolate opponent MS1B by clearing lower shadow bits
				// eraser = opponent's shadow
			eraser = _mm256_or_si256(rmO, _mm256_srlv_epi64(rmO, _mm256_set_epi64x(7, 9, 8, 1)));
			eraser = _mm256_or_si256(eraser, _mm256_srlv_epi64(eraser, _mm256_set_epi64x(14, 18, 16, 2)));
			F4 = _mm256_andnot_si256(eraser, rmask);
			F4 = _mm256_andnot_si256(_mm256_srlv_epi64(eraser, _mm256_set_epi64x(28, 36, 32, 4)), F4);
				// clear if no opponent bit, i.e. all player
			F4 = _mm256_andnot_si256(_mm256_cmpeq_epi64(F4, rmask), F4);

				// left: look for opponent LS1B
			outflank = _mm256_and_si256(lmO, _mm256_sub_epi64(_mm256_setzero_si256(), lmO));	// LS1B
				// set all bits if outflank = 0, otherwise higher bits than outflank
			eraser = _mm256_sub_epi64(_mm256_cmpeq_epi64(outflank, _mm256_setzero_si256()), outflank);
			F4 = _mm256_or_si256(F4, _mm256_andnot_si256(eraser, lmask));

			F2 = _mm_or_si128(_mm256_castsi256_si128(F4), _mm256_extracti128_si256(F4, 1));
			nflip = -bit_count(_mm_cvtsi128_si64(_mm_or_si128(F2, _mm_unpackhi_epi64(F2, F2))));
				// last square for O if O can move or score <= 0
			score += (nflip - (int)((nflip | (score - 1)) < 0)) * 2;

		} else	score += 2;	// lazy high cut-off, return min flip

	} else {	// if player cannot move, low cut-off will occur whether opponent can move.
			// right: isolate player MS1B by clearing lower shadow bits
		outflank = _mm256_xor_si256(rmO, rmask);	// masked P (all O -> all P)
			// eraser = player's shadow
		eraser = _mm256_or_si256(outflank, _mm256_srlv_epi64(outflank, _mm256_set_epi64x(7, 9, 8, 1)));
		eraser = _mm256_or_si256(eraser, _mm256_srlv_epi64(eraser, _mm256_set_epi64x(14, 18, 16, 2)));
		F4 = _mm256_andnot_si256(eraser, rmask);
		F4 = _mm256_andnot_si256(_mm256_srlv_epi64(eraser, _mm256_set_epi64x(28, 36, 32, 4)), F4);

			// left: set below LS1B
		outflank = _mm256_xor_si256(lmO, lmask);	// masked P (all O -> all P)
		outflank = _mm256_andnot_si256(outflank, _mm256_add_epi64(outflank, _mm256_set1_epi64x(-1)));
		F4 = _mm256_or_si256(F4, _mm256_and_si256(outflank, lmask));

		F2 = _mm_or_si128(_mm256_castsi256_si128(F4), _mm256_extracti128_si256(F4, 1));
		nflip = bit_count(_mm_cvtsi128_si64(_mm_or_si128(F2, _mm_unpackhi_epi64(F2, F2))));
		score += nflip * 2;

		// if nflip == 0, score <= alpha so lazy low cut-off
	}

	return score;
}

#else // LASTFLIP_LOWCUT

int vectorcall board_score_sse_1(__m128i OP, const int alpha, const int pos)
{
	int score, score2, n_flips;
	__m256i PP = _mm256_broadcastq_epi64(OP);
	__m256i flip, outflank, eraser, rmask, lmask;
	__m128i flip2;

	SEARCH_STATS(++statistics.n_solve_1);

	rmask = lrmask[pos].v4[0];
		// isolate player MS1B by clearing lower shadow bits
	outflank = _mm256_and_si256(PP, rmask);
		// eraser = player's shadow
	eraser = _mm256_or_si256(outflank, _mm256_srlv_epi64(outflank, _mm256_set_epi64x(7, 9, 8, 1)));
	eraser = _mm256_or_si256(eraser, _mm256_srlv_epi64(eraser, _mm256_set_epi64x(14, 18, 16, 2)));
	flip = _mm256_andnot_si256(eraser, rmask);
	flip = _mm256_andnot_si256(_mm256_srlv_epi64(eraser, _mm256_set_epi64x(28, 36, 32, 4)), flip);
		// clear if no player bit, i.e. all opponent
	flip = _mm256_andnot_si256(_mm256_cmpeq_epi64(flip, rmask), flip);

	lmask = lrmask[pos].v4[1];
		// look for player LS1B
	outflank = _mm256_and_si256(PP, lmask);
	outflank = _mm256_and_si256(outflank, _mm256_sub_epi64(_mm256_setzero_si256(), outflank));	// LS1B
		// set all bits if outflank = 0, otherwise higher bits than outflank
	eraser = _mm256_sub_epi64(_mm256_cmpeq_epi64(outflank, _mm256_setzero_si256()), outflank);
	flip = _mm256_or_si256(flip, _mm256_andnot_si256(eraser, lmask));

	flip2 = _mm_or_si128(_mm256_castsi256_si128(flip), _mm256_extracti128_si256(flip, 1));
	flip2 = _mm_or_si128(flip2, _mm_unpackhi_epi64(flip2, flip2));
	score = 2 * bit_count(_mm_cvtsi128_si64(_mm_or_si128(_mm256_castsi256_si128(PP), flip2))) - 64 + 2;

	if (_mm_testz_si128(flip2, flip2)) {	// (23%)
		score2 = score - 2;	// empty for opponent
		if (score <= 0)
			score = score2;
		if (score > alpha) {	// lazy cut-off (40%)
				// isolate player MS1B by clearing lower shadow bits
			outflank = _mm256_andnot_si256(PP, rmask);
				// eraser = player's shadow
			eraser = _mm256_or_si256(outflank, _mm256_srlv_epi64(outflank, _mm256_set_epi64x(7, 9, 8, 1)));
			eraser = _mm256_or_si256(eraser, _mm256_srlv_epi64(eraser, _mm256_set_epi64x(14, 18, 16, 2)));
			flip = _mm256_andnot_si256(eraser, rmask);
			flip = _mm256_andnot_si256(_mm256_srlv_epi64(eraser, _mm256_set_epi64x(28, 36, 32, 4)), flip);
				// clear if no player bit, i.e. all opponent
			flip = _mm256_andnot_si256(_mm256_cmpeq_epi64(flip, rmask), flip);

				// look for player LS1B
			outflank = _mm256_andnot_si256(PP, lmask);
			outflank = _mm256_and_si256(outflank, _mm256_sub_epi64(_mm256_setzero_si256(), outflank));	// LS1B
				// set all bits if outflank = 0, otherwise higher bits than outflank
			eraser = _mm256_sub_epi64(_mm256_cmpeq_epi64(outflank, _mm256_setzero_si256()), outflank);
			flip = _mm256_or_si256(flip, _mm256_andnot_si256(eraser, lmask));

			flip2 = _mm_or_si128(_mm256_castsi256_si128(flip), _mm256_extracti128_si256(flip, 1));
			flip2 = _mm_or_si128(flip2, _mm_unpackhi_epi64(flip2, flip2));
			n_flips = 2 * bit_count(_mm_cvtsi128_si64(flip2));
			if (n_flips != 0)	// (98%)
				score = score2 - n_flips;
		}
	}

	return score;
}

#endif

// from bench.c
int board_score_1(unsigned long long player, int alpha, int x)
{
	return board_score_sse_1(_mm_cvtsi64_si128(player), alpha, x);
}
