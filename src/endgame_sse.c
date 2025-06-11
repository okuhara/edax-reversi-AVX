/**
 * @file endgame_sse.c
 *
 *
 * SSE / AVX optimized version of endgame.c for the last four empties.
 *
 * Bitboard and empty list are kept in SSE registers.
 *
 * @date 1998 - 2025
 * @author Richard Delorme
 * @author Toshihiko Okuhara
 * @version 4.5
 * 
 */

#include "bit.h"
#include "settings.h"
#include "search.h"
#include <stdint.h>
#include <assert.h>

#define	SWAP64	0x4e	// for _mm_shuffle_epi32
#define	DUPLO	0x44
#define	DUPHI	0xee

#if defined(__AVX__) && (defined(__x86_64__) || defined(_M_X64))
	#define	EXTRACT_O(OP)	_mm_extract_epi64(OP, 1)
#else
	#define	EXTRACT_O(OP)	_mm_cvtsi128_si64(_mm_shuffle_epi32(OP, DUPHI))
#endif

#if defined(__AVX__) || defined(__SSE4_1__)
	#define	EXTRACT_MOVE(X,i)	_mm_extract_epi8((X), (i) * 4 + 3)
#else
	#define	EXTRACT_MOVE(X,i)	(_mm_extract_epi16((X), (i) * 2 + 1) >> 8)
#endif

#if defined(__AVX__) || defined(__SSE4_1__)
	static inline int vectorcall TESTZ_FLIP(__m128i X) { return _mm_testz_si128(X, X); }
#elif defined(__x86_64__) || defined(_M_X64)
	#define TESTZ_FLIP(X)	(!_mm_cvtsi128_si64(X))
#else
	static inline int vectorcall TESTZ_FLIP(__m128i X) { return !_mm_cvtsi128_si32(_mm_packs_epi16(X, X)); }
#endif

/**
 * @brief Compute a board resulting of a move played on a previous board.
 *
 * @param OP board to play the move on.
 * @param x move to play.
 * @param flipped flipped returned from mm_Flip.
 * @return resulting board.
 */
static inline __m128i vectorcall board_flip_next(__m128i OP, int x, __m128i flipped)
{
	OP = _mm_xor_si128(OP, _mm_or_si128(reduce_vflip(flipped), _mm_loadl_epi64((__m128i *) &X_TO_BIT[x])));
	return _mm_shuffle_epi32(OP, SWAP64);
}

/**
 * @brief Get the final score.
 *
 * Get the final min score, when 2 empty squares remain.
 *
 * @param OP The board to evaluate.
 * @param alpha Alpha bound.
 * @param n_nodes Node counter.
 * @param empties Packed empty square coordinates.
 * @return The final min score, as a disc difference.
 */
static int vectorcall solve_2(__m128i OP, int alpha, volatile unsigned long long *n_nodes, __m128i empties)
{
	__m128i flipped;
	int score, bestscore, nodes, x1, x2;
	unsigned long long opponent;
	// const int beta = alpha + 1;

	SEARCH_STATS(++statistics.n_solve_2);

	x1 = _mm_cvtsi128_si32(empties);
	x2 = x1 & 0xFF;	// _mm_extract_epi16(empties, 0)
	x1 >>= 16;	// _mm_extract_epi16(empties, 1)
	opponent = EXTRACT_O(OP);
	if ((NEIGHBOUR[x1] & opponent) && !TESTZ_FLIP(flipped = mm_Flip(OP, x1))) {
		bestscore = mm_solve_1(_mm_xor_si128(_mm_shuffle_epi32(OP, SWAP64), reduce_vflip(flipped)), alpha, x2);

		if ((bestscore > alpha) && (NEIGHBOUR[x2] & opponent) && !TESTZ_FLIP(flipped = mm_Flip(OP, x2))) {
			score = mm_solve_1(_mm_xor_si128(_mm_shuffle_epi32(OP, SWAP64), reduce_vflip(flipped)), alpha, x1);
			if (score < bestscore)
				bestscore = score;
			nodes = 3;
		} else	nodes = 2;

	} else if ((NEIGHBOUR[x2] & opponent) && !TESTZ_FLIP(flipped = mm_Flip(OP, x2))) {
		bestscore = mm_solve_1(_mm_xor_si128(_mm_shuffle_epi32(OP, SWAP64), reduce_vflip(flipped)), alpha, x1);
		nodes = 2;

	} else {	// pass - NEIGHBOUR test is almost 100% true
		alpha = ~alpha;	// = -alpha - 1
		if (!TESTZ_FLIP(flipped = mm_Flip(_mm_shuffle_epi32(OP, SWAP64), x1))) {
			bestscore = mm_solve_1(_mm_xor_si128(OP, reduce_vflip(flipped)), alpha, x2);

			if ((bestscore > alpha) && !TESTZ_FLIP(flipped = mm_Flip(_mm_shuffle_epi32(OP, SWAP64), x2))) {
				score = mm_solve_1(_mm_xor_si128(OP, reduce_vflip(flipped)), alpha, x1);
				if (score < bestscore)
					bestscore = score;
				nodes = 3;
			} else	nodes = 2;

		} else if (!TESTZ_FLIP(flipped = mm_Flip(_mm_shuffle_epi32(OP, SWAP64), x2))) {
			bestscore = mm_solve_1(_mm_xor_si128(OP, reduce_vflip(flipped)), alpha, x1);
			nodes = 2;

		} else {	// gameover
			bestscore = board_solve(_mm_cvtsi128_si64(OP), 2);
			nodes = 1;
		}
		bestscore = -bestscore;
	}

	SEARCH_UPDATE_2EMPTIES_NODES(*n_nodes += nodes;)
	assert(SCORE_MIN <= bestscore && bestscore <= SCORE_MAX);
	assert((bestscore & 1) == 0);
	return bestscore;
}

/**
 * @brief Get the final score.
 *
 * Get the final max score, when 3 empty squares remain.
 *
 * @param OP The board to evaluate.
 * @param alpha Alpha bound.
 * @param n_nodes Node counter.
 * @param empties Packed empty square coordinates.
 * @return The final max score, as a disc difference.
 */
static int vectorcall solve_3(__m128i OP, int alpha, volatile unsigned long long *n_nodes, __m128i empties)
{
	__m128i flipped;
	int score, bestscore, x, pol;
	unsigned long long opponent;

	SEARCH_STATS(++statistics.n_solve_3);
	SEARCH_UPDATE_INTERNAL_NODES(*n_nodes);

#if defined(__AVX__) || defined(__SSE4_1__)
	empties = _mm_cvtepu8_epi16(empties);
#else
	empties = _mm_unpacklo_epi8((empties), _mm_setzero_si128());
#endif
	bestscore = -SCORE_INF;
	pol = 1;
	do {
		// best move alphabeta search
		opponent = EXTRACT_O(OP);
		x = _mm_extract_epi16(empties, 2);
		if ((NEIGHBOUR[x] & opponent) && !TESTZ_FLIP(flipped = mm_Flip(OP, x))) {
			bestscore = solve_2(board_flip_next(OP, x, flipped), alpha, n_nodes, empties);
			if (bestscore > alpha) return bestscore * pol;
		}

		x = _mm_extract_epi16(empties, 1);
		if (/* (NEIGHBOUR[x] & opponent) && */ !TESTZ_FLIP(flipped = mm_Flip(OP, x))) {
			score = solve_2(board_flip_next(OP, x, flipped), alpha, n_nodes, _mm_shufflelo_epi16(empties, 0xd8));	// (d3d1)d2d0
			if (score > alpha) return score * pol;
			else if (score > bestscore) bestscore = score;
		}

		x = _mm_extract_epi16(empties, 0);
		if (/* (NEIGHBOUR[x] & opponent) && */ !TESTZ_FLIP(flipped = mm_Flip(OP, x))) {
			score = solve_2(board_flip_next(OP, x, flipped), alpha, n_nodes, _mm_shufflelo_epi16(empties, 0xc9));	// (d3d0)d2d1
			if (score > bestscore) bestscore = score;
			return bestscore * pol;
		}

		if (bestscore > -SCORE_INF)
			return bestscore * pol;

		OP = _mm_shuffle_epi32(OP, SWAP64);	// pass
		alpha = ~alpha;	// = -(alpha + 1)
	} while ((pol = -pol) < 0);

	return board_solve(_mm_cvtsi128_si64(OP), 3);	// gameover
}

/**
 * @brief Get the final score.
 *
 * Get the final min score, when 4 empty squares remain.
 *
 * @param search Search position.
 * @param alpha Upper score value.
 * @return The final min score, as a disc difference.
 */
static int search_solve_4(Search *search, int alpha)
{
	__m128i	OP, flipped;
	__m128i	empties_series;	// B15:4th, B11:3rd, B7:2nd, B3:1st, lower 3 bytes for 3 empties
	int x1, x2, x3, x4, paritysort, score, bestscore, pol;
	unsigned long long opponent;
	// const int beta = alpha + 1;
	static const unsigned char parity_case[64] = {	/* x4x3x2x1 = */
		/*0000*/  0, /*0001*/  0, /*0010*/  1, /*0011*/  9, /*0100*/  2, /*0101*/ 10, /*0110*/ 11, /*0111*/  3,
		/*0002*/  0, /*0003*/  0, /*0012*/  0, /*0013*/  0, /*0102*/  4, /*0103*/  4, /*0112*/  5, /*0113*/  5,
		/*0020*/  1, /*0021*/  0, /*0030*/  1, /*0031*/  0, /*0120*/  6, /*0121*/  7, /*0130*/  6, /*0131*/  7,
		/*0022*/  9, /*0023*/  0, /*0032*/  0, /*0033*/  9, /*0122*/  8, /*0123*/  0, /*0132*/  0, /*0133*/  8,
		/*0200*/  2, /*0201*/  4, /*0210*/  6, /*0211*/  8, /*0300*/  2, /*0301*/  4, /*0310*/  6, /*0311*/  8,
		/*0202*/ 10, /*0203*/  4, /*0212*/  7, /*0213*/  0, /*0302*/  4, /*0303*/ 10, /*0312*/  0, /*0313*/  7,
		/*0220*/ 11, /*0221*/  5, /*0230*/  6, /*0231*/  0, /*0320*/  6, /*0321*/  0, /*0330*/ 11, /*0331*/  5,
		/*0222*/  3, /*0223*/  5, /*0232*/  7, /*0233*/  8, /*0322*/  8, /*0323*/  7, /*0332*/  5, /*0333*/  3
	};
#if defined(__SSSE3__) || defined(__AVX__)
	static const V4SI shuf_mask[] = {	// 4.5.5: prefer 1 empty over 3 empties
		{{ 0x03020100, 0x02030100, 0x01030200, 0x00030201 }},	//  0: 1(x1) 3(x2 x3 x4), 1(x1) 1(x2) 2(x3 x4), 1 1 1 1, 4
		{{ 0x02030100, 0x03020100, 0x01030200, 0x00030201 }},	//  1: 1(x2) 3(x1 x3 x4)
		{{ 0x01030200, 0x03020100, 0x02030100, 0x00030201 }},	//  2: 1(x3) 3(x1 x2 x4)
		{{ 0x00030201, 0x03020100, 0x02030100, 0x01030200 }},	//  3: 1(x4) 3(x1 x2 x3)
		{{ 0x03010200, 0x01030200, 0x02030100, 0x00030201 }},	//  4: 1(x1) 1(x3) 2(x2 x4)
		{{ 0x03000201, 0x00030201, 0x02030100, 0x01030200 }},	//  5: 1(x1) 1(x4) 2(x2 x3)
		{{ 0x02010300, 0x01020300, 0x03020100, 0x00030201 }},	//  6: 1(x2) 1(x3) 2(x1 x4)
		{{ 0x02000301, 0x00020301, 0x03020100, 0x01030200 }},	//  7: 1(x2) 1(x4) 2(x1 x3)
		{{ 0x01000302, 0x00010302, 0x03020100, 0x02030100 }},	//  8: 1(x3) 1(x4) 2(x1 x2)
		{{ 0x03020100, 0x02030100, 0x01000302, 0x00010302 }},	//  9: 2(x1 x2) 2(x3 x4)
		{{ 0x03010200, 0x02000301, 0x01030200, 0x00020301 }},	// 10: 2(x1 x3) 2(x2 x4)
		{{ 0x03000201, 0x02010300, 0x01020300, 0x00030201 }}	// 11: 2(x1 x4) 2(x2 x3)
	};
#else
	__m128i empties0, empties1;
#endif

	SEARCH_STATS(++statistics.n_search_solve_4);
	SEARCH_UPDATE_INTERNAL_NODES(search->n_nodes);

	// stability cutoff (try 12%, cut 7%)
	if (search_SC_NWS_4(search->board.player, search->board.opponent, alpha, &score)) return score;

	OP = _mm_loadu_si128((__m128i *) &search->board);
	x1 = search->empties[NOMOVE].next;
	x2 = search->empties[x1].next;
	x3 = search->empties[x2].next;
	x4 = search->empties[x3].next;

	// parity based move sorting.
	// The following hole sizes are possible:
	//    4 - 1 3 - 2 2 - 1 1 2 - 1 1 1 1
	// Only the 1 1 2 case needs move sorting on this ply.
	// 4.5.5: prefer 1 empty over 3 empties, 1 3 case also needs sorting.
	paritysort = parity_case[((x3 ^ x4) & 0x24) + ((((x2 ^ x4) & 0x24) * 2 + ((x1 ^ x4) & 0x24)) >> 2)];
	empties_series = _mm_cvtsi32_si128((x1 << 24) | (x2 << 16) | (x3 << 8) | x4);
#if defined(__SSSE3__) || defined(__AVX__)
	empties_series = _mm_shuffle_epi8(empties_series, shuf_mask[paritysort].v4);

#else // SSE
	empties_series = _mm_unpacklo_epi8((empties_series), _mm_setzero_si128());
	empties0 = empties1 = _mm_unpacklo_epi64(empties_series, empties_series);
	switch (paritysort) {
		default: // 0: 1(x1) 3(x2 x3 x4), 1(x1) 1(x2) 2(x3 x4), 1 1 1 1, 4
									// x1x2x3x4
			empties0 = _mm_shufflehi_epi16(empties0, 0xb4);	// x2x1x3x4
			empties1 = _mm_shufflelo_epi16(empties1, 0x78);	// x3x1x2x4
			empties1 = _mm_shufflehi_epi16(empties1, 0x39);	// x4x1x2x3
			break;
		case 1: // 1(x2) 3(x1 x3 x4)
			empties0 = _mm_shufflelo_epi16(empties0, 0xb4);	// x2x1x3x4
									// x1x2x3x4
			empties1 = _mm_shufflelo_epi16(empties1, 0x78);	// x3x1x2x4
			empties1 = _mm_shufflehi_epi16(empties1, 0x39);	// x4x1x2x3
			break;
		case 2: // 1(x3) 3(x1 x2 x4)
			empties0 = _mm_shufflelo_epi16(empties0, 0x78);	// x3x1x2x4
									// x1x2x3x4
			empties1 = _mm_shufflelo_epi16(empties1, 0xb4);	// x2x1x3x4
			empties1 = _mm_shufflehi_epi16(empties1, 0x39);	// x4x1x2x3
			break;
		case 3: // 1(x4) 3(x1 x2 x3)
			empties0 = _mm_shufflelo_epi16(empties0, 0x39);	// x4x1x2x3
									// x1x2x3x4
			empties1 = _mm_shufflelo_epi16(empties1, 0xb4);	// x2x1x3x4
			empties1 = _mm_shufflehi_epi16(empties1, 0x78);	// x3x1x2x4
			break;
		case 4: // 1(x1) 1(x3) 2(x2 x4)
			empties0 = _mm_shufflelo_epi16(empties0, 0xd8);	// x1x3x2x4
			empties0 = _mm_shufflehi_epi16(empties0, 0x78);	// x3x1x2x4
			empties1 = _mm_shufflelo_epi16(empties1, 0xb4);	// x2x1x3x4
			empties1 = _mm_shufflehi_epi16(empties1, 0x39);	// x4x1x2x3
			break;
		case 5: // 1(x1) 1(x4) 2(x2 x3)
			empties0 = _mm_shufflelo_epi16(empties0, 0xc9);	// x1x4x2x3
			empties0 = _mm_shufflehi_epi16(empties0, 0x39);	// x4x1x2x3
			empties1 = _mm_shufflelo_epi16(empties1, 0xb4);	// x2x1x3x4
			empties1 = _mm_shufflehi_epi16(empties1, 0x78);	// x3x1x2x4
			break;
		case 6: // 1(x2) 1(x3) 2(x1 x4)
			empties0 = _mm_shufflelo_epi16(empties0, 0x9c);	// x2x3x1x4
			empties0 = _mm_shufflehi_epi16(empties0, 0x6c);	// x3x2x1x4
									// x1x2x3x4
			empties1 = _mm_shufflehi_epi16(empties1, 0x39);	// x4x1x2x3
			break;
		case 7: // 1(x2) 1(x4) 2(x1 x3)
			empties0 = _mm_shufflelo_epi16(empties0, 0x8d);	// x2x4x1x3
			empties0 = _mm_shufflehi_epi16(empties0, 0x2d);	// x4x2x1x3
									// x1x2x3x4
			empties1 = _mm_shufflehi_epi16(empties1, 0x78);	// x3x1x2x4
			break;
		case 8: // 1(x3) 1(x4) 2(x1 x2)
			empties0 = _mm_shufflelo_epi16(empties0, 0x4e);	// x3x4x1x2
			empties0 = _mm_shufflehi_epi16(empties0, 0x1e);	// x4x3x1x2
									// x1x2x3x4
			empties1 = _mm_shufflehi_epi16(empties1, 0xb4);	// x2x1x3x4
			break;
		case 9:	// 2(x1 x2) 2(x3 x4)
									// x1x2x3x4
			empties0 = _mm_shufflehi_epi16(empties0, 0xb4);	// x2x1x3x4
			empties1 = _mm_shufflelo_epi16(empties1, 0x4e);	// x3x4x1x2
			empties1 = _mm_shufflehi_epi16(empties1, 0x1e);	// x4x3x1x2
			break;
		case 10: // 2(x1 x3) 2(x2 x4)
			empties0 = _mm_shufflelo_epi16(empties0, 0xd8);	// x1x3x2x4
			empties0 = _mm_shufflehi_epi16(empties0, 0x8d);	// x2x4x1x3
			empties1 = _mm_shufflelo_epi16(empties1, 0x78);	// x3x1x2x4
			empties1 = _mm_shufflehi_epi16(empties1, 0x2d);	// x4x2x1x3
			break;
		case 11: // 2(x1 x4) 2(x2 x3)
			empties0 = _mm_shufflelo_epi16(empties0, 0xc9);	// x1x4x2x3
			empties0 = _mm_shufflehi_epi16(empties0, 0x9c);	// x2x3x1x4
			empties1 = _mm_shufflelo_epi16(empties1, 0x6c);	// x3x2x1x4
			empties1 = _mm_shufflehi_epi16(empties1, 0x39);	// x4x1x2x3
			break;
	}
	empties_series = _mm_packus_epi16(empties0, empties1);
#endif

	bestscore = SCORE_INF;	// min stage
	pol = 1;
	do {
		// best move alphabeta search
		opponent = EXTRACT_O(OP);
		x1 = EXTRACT_MOVE(empties_series, 0);
		if ((NEIGHBOUR[x1] & opponent) && !TESTZ_FLIP(flipped = mm_Flip(OP, x1))) {
			bestscore = solve_3(board_flip_next(OP, x1, flipped), alpha, &search->n_nodes, empties_series);
			if (bestscore <= alpha) return bestscore * pol;
		}

		x2 = EXTRACT_MOVE(empties_series, 1);
		if ((NEIGHBOUR[x2] & opponent) && !TESTZ_FLIP(flipped = mm_Flip(OP, x2))) {
			score = solve_3(board_flip_next(OP, x2, flipped), alpha, &search->n_nodes, _mm_srli_si128(empties_series, 4));
			if (score <= alpha) return score * pol;
			else if (score < bestscore) bestscore = score;
		}

		x3 = EXTRACT_MOVE(empties_series, 2);
		if ((NEIGHBOUR[x3] & opponent) && !TESTZ_FLIP(flipped = mm_Flip(OP, x3))) {
			score = solve_3(board_flip_next(OP, x3, flipped), alpha, &search->n_nodes, _mm_srli_si128(empties_series, 8));
			if (score <= alpha) return score * pol;
			else if (score < bestscore) bestscore = score;
		}

		x4 = EXTRACT_MOVE(empties_series, 3);
		if ((NEIGHBOUR[x4] & opponent) && !TESTZ_FLIP(flipped = mm_Flip(OP, x4))) {
			score = solve_3(board_flip_next(OP, x4, flipped), alpha, &search->n_nodes, _mm_srli_si128(empties_series, 12));
			if (score <= bestscore) bestscore = score;
			return bestscore * pol;
		}

		if (bestscore < SCORE_INF)
			return bestscore * pol;

		OP = _mm_shuffle_epi32(OP, SWAP64);	// pass
		alpha = ~alpha;	// = -(alpha + 1)
	} while ((pol = -pol) < 0);

	return board_solve(EXTRACT_O(OP), 4);	// gameover
}
