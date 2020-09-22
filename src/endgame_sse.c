/**
 * @file endgame_avx.c
 *
 *
 * SSE / AVX optimized version of endgame.c for the last four empties.
 *
 * Bitboard and empty list is kept in SSE registers, but performance gain
 * is limited for GCC minGW build since vectorcall is not supported.
 *
 * @date 1998 - 2020
 * @author Richard Delorme
 * @author Toshihiko Okuhara
 * @version 4.4
 * 
 */

#include "bit.h"
#include "settings.h"
#include "search.h"
#include <assert.h>

#define	SWAP64	0x4e	// for _mm_shuffle_epi32
#define	DUPLO	0x44
#define	DUPHI	0xee

#if defined(__AVX__) && (defined(__x86_64__) || defined(_M_X64))
#define	EXTRACT_O(OP)	_mm_extract_epi64(OP, 1)
#else
#define	EXTRACT_O(OP)	_mm_cvtsi128_si64(_mm_shuffle_epi32(OP, DUPHI))
#endif

#ifdef __AVX__
#define	EXTRACT_B3(X)	_mm_extract_epi8(X, 3)
static inline int TESTZ_FLIP(__m128i X) { return _mm_testz_si128(X, X); }
#else
#define	EXTRACT_B3(X)	(_mm_cvtsi128_si32(X) >> 24)
#if defined(__x86_64__) || defined(_M_X64)
#define TESTZ_FLIP(X)	(!_mm_cvtsi128_si64(X))
#else
static inline int TESTZ_FLIP(__m128i X) { return !_mm_cvtsi128_si32(_mm_packs_epi16(X, X)); }
#endif
#define _mm_cvtepu8_epi16(X)	_mm_unpacklo_epi8((X), _mm_setzero_si128())
#endif

// in count_last_flip_sse.c
extern const unsigned char COUNT_FLIP[8][256];
extern const V4DI mask_dvhd[64];

/**
 * @brief Compute a board resulting of a move played on a previous board.
 *
 * @param OP board to play the move on.
 * @param x move to play.
 * @param next resulting board.
 * @return true if no flips.
 */
static inline __m128i board_next_sse(__m128i OP, int x, __m128i flipped)
{
	OP = _mm_xor_si128(OP, _mm_or_si128(flipped, _mm_loadl_epi64((__m128i *) &X_TO_BIT[x])));
	return _mm_shuffle_epi32(OP, SWAP64);
}

/**
 * @brief Get the final score.
 *
 * Get the final score, when no move can be made.
 *
 * @param OP Board.
 * @param n_empties Number of empty squares remaining on the board.
 * @return The final score, as a disc difference.
 */
static int vectorcall board_solve_sse(__m128i OP, int n_empties)
{
	int score = bit_count(_mm_cvtsi128_si64(OP)) * 2 - SCORE_MAX;	// in case of opponents win
	int diff = score + n_empties;		// = n_discs_p - (64 - n_empties - n_discs_p)

	SEARCH_STATS(++statistics.n_search_solve);

	if (diff >= 0)
		score = diff;
	if (diff > 0)
		score += n_empties;
	return score;
}

/**
 * @brief Get the final score.
 *
 * Get the final score, when 1 empty squares remain.
 * The following code has been adapted from Zebra by Gunnar Anderson.
 *
 * @param OP  Board to evaluate.
 * @param beta   Beta bound.
 * @param pos    Last empty square to play.
 * @return       The final opponent score, as a disc difference.
 */
static int vectorcall board_score_sse_1(__m128i OP, const int beta, const int pos)
{
	unsigned char	n_flips;
	unsigned int	t;
	unsigned long long P = _mm_cvtsi128_si64(OP);
	int	score = SCORE_MAX - 2 - 2 * bit_count(P);	// 2 * bit_count(O) - SCORE_MAX
	int	score2;
	const unsigned char *COUNT_FLIP_X = COUNT_FLIP[pos & 7];
	const unsigned char *COUNT_FLIP_Y = COUNT_FLIP[pos >> 3];
	__m128i	II;

	// n_flips = last_flip(pos, P);
#ifdef AVXLASTFLIP
	__m256i	MP = _mm256_and_si256(_mm256_broadcastq_epi64(OP), mask_dvhd[pos].v4);
	n_flips  = COUNT_FLIP_X[(unsigned char) (P >> (pos & 0x38))];
	t = _mm256_movemask_epi8(_mm256_sub_epi8(_mm256_setzero_si256(), MP));
	n_flips += COUNT_FLIP_Y[(unsigned char) t];
	t >>= 16;
#else
	__m128i	PP = _mm_shuffle_epi32(OP, DUPLO);
	II = _mm_sad_epu8(_mm_and_si128(PP, mask_dvhd[pos].v2[0]), _mm_setzero_si128());
	n_flips  = COUNT_FLIP_X[_mm_extract_epi16(II, 4)];
	n_flips += COUNT_FLIP_X[_mm_cvtsi128_si32(II)];
	t = _mm_movemask_epi8(_mm_sub_epi8(_mm_setzero_si128(), _mm_and_si128(PP, mask_dvhd[pos].v2[1])));
#endif
	n_flips += COUNT_FLIP_Y[t >> 8];
	n_flips += COUNT_FLIP_Y[(unsigned char) t];
	score -= n_flips;

	if (n_flips == 0) {
		score2 = score + 2;	// empty for player
		if (score >= 0)
			score = score2;

		if (score < beta) {	// lazy cut-off
			// n_flips = last_flip(pos, EXTRACT_O(OP));
#ifdef AVXLASTFLIP
			MP = _mm256_and_si256(_mm256_permute4x64_epi64(_mm256_castsi128_si256(OP), 0x55), mask_dvhd[pos].v4);
			II = _mm_sad_epu8(_mm256_castsi256_si128(MP), _mm_setzero_si128());
			t = _mm_movemask_epi8(_mm_sub_epi8(_mm_setzero_si128(), _mm256_extracti128_si256(MP, 1)));
#else
			PP = _mm_shuffle_epi32(OP, DUPHI);
			II = _mm_sad_epu8(_mm_and_si128(PP, mask_dvhd[pos].v2[0]), _mm_setzero_si128());
			t = _mm_movemask_epi8(_mm_sub_epi8(_mm_setzero_si128(), _mm_and_si128(PP, mask_dvhd[pos].v2[1])));
#endif
			n_flips  = COUNT_FLIP_X[_mm_extract_epi16(II, 4)];
			n_flips += COUNT_FLIP_X[_mm_cvtsi128_si32(II)];
			n_flips += COUNT_FLIP_Y[t >> 8];
			n_flips += COUNT_FLIP_Y[(unsigned char) t];

			if (n_flips != 0)
				score = score2 + n_flips;
		}
	}

	return score;
}

/**
 * @brief Get the final score.
 *
 * Get the final score, when 2 empty squares remain.
 *
 * @param OP The board to evaluate.
 * @param empties Packed empty square coordinates.
 * @param alpha Alpha bound.
 * @param n_nodes Node counter.
 * @return The final score, as a disc difference.
 */
static int vectorcall board_solve_sse_2(__m128i OP, int alpha, volatile unsigned long long *n_nodes, __m128i empties)
{
	__m128i flipped, PO;
	int score, bestscore, nodes;
	int x1 = _mm_extract_epi16(empties, 1);
	int x2 = _mm_extract_epi16(empties, 0);
	unsigned long long bb;
	// const int beta = alpha + 1;

	SEARCH_STATS(++statistics.n_board_solve_2);

	bb = EXTRACT_O(OP);	// opponent
	if ((NEIGHBOUR[x1] & bb) && !TESTZ_FLIP(flipped = mm_Flip(OP, x1))) {
		bestscore = board_score_sse_1(board_next_sse(OP, x1, flipped), alpha + 1, x2);
		nodes = 2;

		if ((bestscore <= alpha) && (NEIGHBOUR[x2] & bb) && !TESTZ_FLIP(flipped = mm_Flip(OP, x2))) {
			score = board_score_sse_1(board_next_sse(OP, x2, flipped), alpha + 1, x1);
			if (score > bestscore) bestscore = score;
			nodes = 3;
		}

	} else if ((NEIGHBOUR[x2] & bb) && !TESTZ_FLIP(flipped = mm_Flip(OP, x2))) {
		bestscore = board_score_sse_1(board_next_sse(OP, x2, flipped), alpha + 1, x1);
		nodes = 2;

	} else {	// pass
		bb = _mm_cvtsi128_si64(OP);	// player
		PO = _mm_shuffle_epi32(OP, SWAP64);
		if ((NEIGHBOUR[x1] & bb) && !TESTZ_FLIP(flipped = mm_Flip(PO, x1))) {
			bestscore = -board_score_sse_1(board_next_sse(PO, x1, flipped), -alpha, x2);
			nodes = 2;

			if ((bestscore > alpha) && (NEIGHBOUR[x2] & bb) && !TESTZ_FLIP(flipped = mm_Flip(PO, x2))) {
				score = -board_score_sse_1(board_next_sse(PO, x2, flipped), -alpha, x1);
				if (score < bestscore) bestscore = score;
				nodes = 3;
			}

		} else if ((NEIGHBOUR[x2] & bb) && !TESTZ_FLIP(flipped = mm_Flip(PO, x2))) {
			bestscore = -board_score_sse_1(board_next_sse(PO, x2, flipped), -alpha, x1);
			nodes = 2;

		} else {	// gameover
			bestscore = board_solve_sse(OP, 2);
			nodes = 1;
		}
	}

	SEARCH_UPDATE_2EMPTIES_NODES(*n_nodes += nodes;)
	assert(SCORE_MIN <= bestscore && bestscore <= SCORE_MAX);
	assert((bestscore & 1) == 0);
	return bestscore;
}

/**
 * @brief Get the final score.
 *
 * Get the final score, when 3 empty squares remain.
 *
 * @param OP The board to evaluate.
 * @param empties Packed empty square coordinates.
 * @param alpha Alpha bound.
 * @param sort3 Parity flags.
 * @param n_nodes Node counter.
 * @return The final score, as a disc difference.
 */
static int vectorcall search_solve_sse_3(__m128i OP, int alpha, int sort3, volatile unsigned long long *n_nodes, __m128i empties)
{
	__m128i flipped, PO;
	int score, bestscore, x;
	unsigned long long bb;
	// const int beta = alpha + 1;

	SEARCH_STATS(++statistics.n_search_solve_3);
	SEARCH_UPDATE_INTERNAL_NODES(*n_nodes);

	empties = _mm_cvtepu8_epi16(empties);	// to ease shuffle
	// parity based move sorting
	if (sort3 & 0x03) {
#if !(defined(__SSSE3__) || defined(__AVX__))
		if (sort3 & 0x01)
			empties = _mm_shufflelo_epi16(empties, 0xd8); // case 1(x2) 2(x1 x3)
		else
			empties = _mm_shufflelo_epi16(empties, 0xc9); // case 1(x3) 2(x1 x2)
#endif
	}

	// best move alphabeta search
	bestscore = -SCORE_INF;
	bb = EXTRACT_O(OP);	// opponent
	x = _mm_extract_epi16(empties, 2);
	if ((NEIGHBOUR[x] & bb) && !TESTZ_FLIP(flipped = mm_Flip(OP, x))) {
		bestscore = -board_solve_sse_2(board_next_sse(OP, x, flipped), -(alpha + 1), n_nodes, empties);
		if (bestscore > alpha) return bestscore;
	}

	x = _mm_extract_epi16(empties, 1);
	if ((NEIGHBOUR[x] & bb) && !TESTZ_FLIP(flipped = mm_Flip(OP, x))) {
		score = -board_solve_sse_2(board_next_sse(OP, x, flipped), -(alpha + 1), n_nodes, _mm_shufflelo_epi16(empties, 0xd8));
		if (score > alpha) return score;
		else if (score > bestscore) bestscore = score;
	}

	x = _mm_extract_epi16(empties, 0);
	if ((NEIGHBOUR[x] & bb) && !TESTZ_FLIP(flipped = mm_Flip(OP, x))) {
		score = -board_solve_sse_2(board_next_sse(OP, x, flipped), -(alpha + 1), n_nodes, _mm_shufflelo_epi16(empties, 0xc9));
		if (score > bestscore) bestscore = score;
	}

	else if (bestscore == -SCORE_INF) {	// pass ?
		// best move alphabeta search
		bestscore = SCORE_INF;
		bb = _mm_cvtsi128_si64(OP);	// player
		PO = _mm_shuffle_epi32(OP, SWAP64);
		x = _mm_extract_epi16(empties, 2);
		if ((NEIGHBOUR[x] & bb) && !TESTZ_FLIP(flipped = mm_Flip(PO, x))) {
			bestscore = board_solve_sse_2(board_next_sse(PO, x, flipped), alpha, n_nodes, empties);
			if (bestscore <= alpha) return bestscore;
		}

		x = _mm_extract_epi16(empties, 1);
		if ((NEIGHBOUR[x] & bb) && !TESTZ_FLIP(flipped = mm_Flip(PO, x))) {
			score = board_solve_sse_2(board_next_sse(PO, x, flipped), alpha, n_nodes, _mm_shufflelo_epi16(empties, 0xd8));
			if (score <= alpha) return score;
			else if (score < bestscore) bestscore = score;
		}

		x = _mm_extract_epi16(empties, 0);
		if ((NEIGHBOUR[x] & bb) && !TESTZ_FLIP(flipped = mm_Flip(PO, x))) {
			score = board_solve_sse_2(board_next_sse(PO, x, flipped), alpha, n_nodes, _mm_shufflelo_epi16(empties, 0xc9));
			if (score < bestscore) bestscore = score;
		}

		else if (bestscore == SCORE_INF)	// gameover
			bestscore = board_solve_sse(OP, 3);
	}

	assert(SCORE_MIN <= bestscore && bestscore <= SCORE_MAX);
	return bestscore;
}

/**
 * @brief Get the final score.
 *
 * Get the final score, when 4 empty squares remain.
 *
 * @param search Search position.
 * @param alpha Upper score value.
 * @return The final score, as a disc difference.
 */

int search_solve_4(Search *search, const int alpha)
{
	__m128i	OP, flipped;
	__m128i	empties_series;	// B15:4th, B11:3rd, B7:2nd, B3:1st, lower 3 bytes for 3 empties
	int x1, x2, x3, x4, paritysort, score, bestscore;
	unsigned long long opp;
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
	union V4SI {
		unsigned int	ui[4];
		__m128i	v4;
	};
	static const union V4SI shuf_mask[] = {	// make search order identical to 4.4.0
		{{ 0x03020100, 0x02030100, 0x01030200, 0x00030201 }},	//  0: 1(x1) 3(x2 x3 x4), 1(x1) 1(x2) 2(x3 x4), 1 1 1 1, 4
		{{ 0x03020100, 0x02030100, 0x01020300, 0x00020301 }},	//  1: 1(x2) 3(x1 x3 x4)
		{{ 0x03010200, 0x02010300, 0x01030200, 0x00010302 }},	//  2: 1(x3) 3(x1 x2 x4)
		{{ 0x03000201, 0x02000301, 0x01000302, 0x00030201 }},	//  3: 1(x4) 3(x1 x2 x3)
		{{ 0x03010200, 0x01030200, 0x02030100, 0x00030201 }},	//  4: 1(x1) 1(x3) 2(x2 x4)
		{{ 0x03000201, 0x00030201, 0x02030100, 0x01030200 }},	//  5: 1(x1) 1(x4) 2(x2 x3)
		{{ 0x02010300, 0x01020300, 0x03020100, 0x00030201 }},	//  6: 1(x2) 1(x3) 2(x1 x4)
		{{ 0x02000301, 0x00020301, 0x03020100, 0x01030200 }},	//  7: 1(x2) 1(x4) 2(x1 x3)
		{{ 0x01000302, 0x00010302, 0x03020100, 0x02030100 }},	//  8: 1(x3) 1(x4) 2(x1 x2)
		{{ 0x03020100, 0x02030100, 0x01000302, 0x00010302 }},	//  9: 2(x1 x2) 2(x3 x4)
		{{ 0x03010200, 0x02000301, 0x01030200, 0x00020301 }},	// 10: 2(x1 x3) 2(x2 x4)
		{{ 0x03000201, 0x02010300, 0x01020300, 0x00030201 }}	// 11: 2(x1 x4) 2(x2 x3)
	};
	enum { sort3 = 0 };	// sort is done on 4 empties
#else
	int sort3;	// for move sorting on 3 empties
	static const short sort3_shuf[] = {
		0x0000,	//  0: 1(x1) 3(x2 x3 x4), 1(x1) 1(x2) 2(x3 x4), 1 1 1 1, 4
		0x1100,	//  1: 1(x2) 3(x1 x3 x4)	x4x2x1x3-x3x2x1x4-x2x1x3x4-x1x2x3x4
		0x2011,	//  2: 1(x3) 3(x1 x2 x4)	x4x3x1x2-x3x1x2x4-x2x3x1x4-x1x3x2x4
		0x0222,	//  3: 1(x4) 3(x1 x2 x3)	x4x1x2x3-x3x4x1x2-x2x4x1x3-x1x4x2x3
		0x0001,	//  4: 1(x1) 1(x3) 2(x2 x4)	x4x1x2x3-x2x1x3x4-x3x1x2x4-x1x3x2x4
		0x0002,	//  5: 1(x1) 1(x4) 2(x2 x3)	x3x1x2x4-x2x1x3x4-x4x1x2x3-x1x4x2x3
		0x0011,	//  6: 1(x2) 1(x3) 2(x1 x4)	x4x1x2x3-x1x2x3x4-x3x2x1x4-x2x3x1x4
		0x0012,	//  7: 1(x2) 1(x4) 2(x1 x3)	x3x1x2x4-x1x2x3x4-x4x2x1x3-x2x4x1x3
		0x0022,	//  8: 1(x3) 1(x4) 2(x1 x2)	x2x1x3x4-x1x2x3x4-x4x3x1x2-x3x4x1x2
		0x2200,	//  9: 2(x1 x2) 2(x3 x4)	x4x3x1x2-x3x4x1x2-x2x1x3x4-x1x2x3x4
		0x1021,	// 10: 2(x1 x3) 2(x2 x4)	x4x2x1x3-x3x1x2x4-x2x4x1x3-x1x3x2x4
		0x0112	// 11: 2(x1 x4) 2(x2 x3)	x4x1x2x3-x3x2x1x4-x2x3x1x4-x1x4x2x3
	};
#endif

	SEARCH_STATS(++statistics.n_search_solve_4);
	SEARCH_UPDATE_INTERNAL_NODES(search->n_nodes);

	// stability cutoff
	if (search_SC_NWS(search, alpha, &score)) return score;

	OP = _mm_loadu_si128((__m128i *) &search->board);
	x1 = search->empties[NOMOVE].next;
	x2 = search->empties[x1].next;
	x3 = search->empties[x2].next;
	x4 = search->empties[x3].next;

	// parity based move sorting.
	// The following hole sizes are possible:
	//    4 - 1 3 - 2 2 - 1 1 2 - 1 1 1 1
	// Only the 1 1 2 case needs move sorting on this ply.
	paritysort = parity_case[((x3 ^ x4) & 0x24) + ((((x2 ^ x4) & 0x24) * 2 + ((x1 ^ x4) & 0x24)) >> 2)];
#if defined(__SSSE3__) || defined(__AVX__)
	empties_series = _mm_cvtsi32_si128((x1 << 24) | (x2 << 16) | (x3 << 8) | x4);
	empties_series = _mm_shuffle_epi8(empties_series, shuf_mask[paritysort].v4);

#else // SSE
	empties_series = _mm_cvtsi32_si128((x3 << 16) | x4);
	empties_series = _mm_insert_epi16(empties_series, x2, 2);
	empties_series = _mm_insert_epi16(empties_series, x1, 3);
	empties_series = _mm_packus_epi16(_mm_unpacklo_epi64(empties_series, _mm_shufflelo_epi16(empties_series, 0xb4)),
		_mm_unpacklo_epi64(_mm_shufflelo_epi16(empties_series, 0x78), _mm_shufflelo_epi16(empties_series, 0x39)));
			// x4x1x2x3-x3x1x2x4-x2x1x3x4-x1x2x3x4
	switch (paritysort) {
		case 4: // case 1(x1) 1(x3) 2(x2 x4)
			empties_series = _mm_shuffle_epi32(empties_series, 0xd8);	// x4...x2...x3...x1...
			break;
		case 5: // case 1(x1) 1(x4) 2(x2 x3)
			empties_series = _mm_shuffle_epi32(empties_series, 0x9c);	// x3...x2...x4...x1...
			break;
		case 6:	// case 1(x2) 1(x3) 2(x1 x4)
			empties_series = _mm_shuffle_epi32(empties_series, 0xc9);	// x4...x1...x3...x2...
			break;
		case 7: // case 1(x2) 1(x4) 2(x1 x3)
			empties_series = _mm_shuffle_epi32(empties_series, 0x8d);	// x3...x1...x4...x2...
			break;
		case 8:	// case 1(x3) 1(x4) 2(x1 x2)
			empties_series = _mm_shuffle_epi32(empties_series, 0x4e);	// x2...x1...x4...x3...
			break;
	}
	sort3 = sort3_shuf[paritysort];
#endif

	// best move alphabeta search
	bestscore = -SCORE_INF;
	opp = EXTRACT_O(OP);
	x1 = EXTRACT_B3(empties_series);
	if ((NEIGHBOUR[x1] & opp) && !TESTZ_FLIP(flipped = mm_Flip(OP, x1))) {
		bestscore = -search_solve_sse_3(board_next_sse(OP, x1, flipped), -(alpha + 1), sort3, &search->n_nodes, empties_series);
		if (bestscore > alpha) return bestscore;
	}

	empties_series = _mm_shuffle_epi32(empties_series, 0x39);
	x2 = EXTRACT_B3(empties_series);
	if ((NEIGHBOUR[x2] & opp) && !TESTZ_FLIP(flipped = mm_Flip(OP, x2))) {
		score = -search_solve_sse_3(board_next_sse(OP, x2, flipped), -(alpha + 1), sort3 >> 4, &search->n_nodes, empties_series);
		if (score > alpha) return score;
		else if (score > bestscore) bestscore = score;
	}

	empties_series = _mm_shuffle_epi32(empties_series, 0x39);
	x3 = EXTRACT_B3(empties_series);
	if ((NEIGHBOUR[x3] & opp) && !TESTZ_FLIP(flipped = mm_Flip(OP, x3))) {
		score = -search_solve_sse_3(board_next_sse(OP, x3, flipped), -(alpha + 1), sort3 >> 8, &search->n_nodes, empties_series);
		if (score > alpha) return score;
		else if (score > bestscore) bestscore = score;
	}

	empties_series = _mm_shuffle_epi32(empties_series, 0x39);
	x4 = EXTRACT_B3(empties_series);
	if ((NEIGHBOUR[x4] & opp) && !TESTZ_FLIP(flipped = mm_Flip(OP, x4))) {
		score = -search_solve_sse_3(board_next_sse(OP, x4, flipped), -(alpha + 1), sort3 >> 12, &search->n_nodes, empties_series);
		if (score > bestscore) bestscore = score;
	}

	else if (bestscore == -SCORE_INF) {	// no move
		if (can_move(opp, _mm_cvtsi128_si64(OP))) { // pass
			search_pass_endgame(search);
			bestscore = -search_solve_4(search, -(alpha + 1));
			search_pass_endgame(search);
		} else { // gameover
			bestscore = board_solve_sse(OP, 4);
		}
	}

	assert(SCORE_MIN <= bestscore && bestscore <= SCORE_MAX);
	return bestscore;
}
