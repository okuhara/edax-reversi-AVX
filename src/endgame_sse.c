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
	int	score, score2;
	unsigned char	n_flips;
	unsigned long long	P;
	unsigned int	t;
	const unsigned char *COUNT_FLIP_X = COUNT_FLIP[pos & 7];
	const unsigned char *COUNT_FLIP_Y = COUNT_FLIP[pos >> 3];
#ifdef AVXLASTFLIP
	__m256i	MP, MO;
#else
	__m128i	PP, OO;
#endif
	__m128i	II;

	P = _mm_cvtsi128_si64(OP);
	score = SCORE_MAX - 2 - 2 * bit_count(P);	// 2 * bit_count(O) - SCORE_MAX

	// n_flips = last_flip(pos, P);
#ifdef AVXLASTFLIP
	n_flips  = COUNT_FLIP_X[(unsigned char) (P >> (pos & 0x38))];
	MP = _mm256_and_si256(_mm256_broadcastq_epi64(OP), mask_dvhd[pos].v4);
	t = _mm256_movemask_epi8(_mm256_sub_epi8(_mm256_setzero_si256(), MP));
	n_flips += COUNT_FLIP_Y[(unsigned char) t];
	t >>= 16;
#else
	PP = _mm_shuffle_epi32(OP, DUPLO);
	II = _mm_sad_epu8(_mm_and_si128(PP, mask_dvhd[pos].v2[0]), _mm_setzero_si128());
	n_flips  = COUNT_FLIP_X[_mm_cvtsi128_si32(II)];
	n_flips += COUNT_FLIP_X[_mm_extract_epi16(II, 4)];
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
			MO = _mm256_and_si256(_mm256_permute4x64_epi64(_mm256_castsi128_si256(OP), 0x55), mask_dvhd[pos].v4);
			II = _mm_sad_epu8(_mm256_castsi256_si128(MO), _mm_setzero_si128());
			t = _mm_movemask_epi8(_mm_sub_epi8(_mm_setzero_si128(), _mm256_extracti128_si256(MO, 1)));
#else
			OO = _mm_shuffle_epi32(OP, DUPHI);
			II = _mm_sad_epu8(_mm_and_si128(OO, mask_dvhd[pos].v2[0]), _mm_setzero_si128());
			t = _mm_movemask_epi8(_mm_sub_epi8(_mm_setzero_si128(), _mm_and_si128(OO, mask_dvhd[pos].v2[1])));
#endif
			n_flips  = COUNT_FLIP_X[_mm_cvtsi128_si32(II)];
			n_flips += COUNT_FLIP_X[_mm_extract_epi16(II, 4)];
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
static int vectorcall search_solve_sse_3(__m128i OP, int alpha, unsigned int sort3, volatile unsigned long long *n_nodes, __m128i empties)
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
#ifndef __AVX__
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

typedef union {
	unsigned int	ui[4];
	__m128i	v4;
} V4SI;

int search_solve_4(Search *search, const int alpha)
{
	__m128i	OP, flipped;
	__m128i	empties_series;	// B15:4th, B11:3rd, B7:2nd, B3:1st, lower 3 bytes for 3 empties
	int x1, x2, x3, x4, q1, q2, q3;
	int score, bestscore;
	unsigned int parity;
	unsigned long long opp;
	// const int beta = alpha + 1;
#ifdef __AVX__
	static const V4SI shuf_x1_1_2[2] = {	// case 1(x1) 1(x4) 2(x2 x3), case 1(x1) 1(x3) 2(x2 x4)
		 {{ 0x03000201, 0x00030201, 0x02030100, 0x01030200 }},		// x3x1x2x4-x2x1x3x4-x4x1x2x3-x1x4x2x3
		 {{ 0x03010200, 0x01030200, 0x02030100, 0x00030201 }}};		// x4x1x2x3-x2x1x3x4-x3x1x2x4-x1x3x2x4
	static const V4SI shuf_x2_1_2[2] = {	// case 1(x2) 1(x4) 2(x1 x3), case 1(x2) 1(x3) 2(x1 x4)
		 {{ 0x02000301, 0x00020301, 0x03020100, 0x01030200 }},		// x3x1x2x4-x1x2x3x4-x4x2x1x3-x2x4x1x3
		 {{ 0x02010300, 0x01020300, 0x03020100, 0x00030201 }}};		// x4x1x2x3-x1x2x3x4-x3x2x1x4-x2x3x1x4
	static const V4SI shuf_1_3[2][2] = {
		{{{ 0x03020100, 0x02030100, 0x01030200, 0x00030201 }},  	// case 1(x1) 3(x2 x3 x4), case 1 1 1 1
		 {{ 0x03020100, 0x02030100, 0x01020300, 0x00020301 }}}, 	// case 1(x2) 3(x1 x3 x4)
		{{{ 0x03010200, 0x02010300, 0x01030200, 0x00010302 }},  	// case 1(x3) 3(x1 x2 x4)
		 {{ 0x03000201, 0x02000301, 0x01000302, 0x00030201 }}}};	// case 1(x4) 3(x1 x2 x3)
	static const V4SI shuf_2_2[2][2] = {
		{{{ 0x03000201, 0x02010300, 0x01020300, 0x00030201 }},  	// case 2(x1 x4) 2(x2 x3)
		 {{ 0x03010200, 0x02000301, 0x01030200, 0x00020301 }}}, 	// case 2(x1 x3) 2(x2 x4)
		{{{ 0x03020100, 0x02030100, 0x01000302, 0x00010302 }},  	// case 2(x1 x2) 2(x3 x4)
		 {{ 0x03020100, 0x02030100, 0x01030200, 0x00030201 }}}};	// case 4
	enum { sort3 = 0 };	// sort is done on 4 empties
#else
	unsigned int sort3;	// for move sorting on 3 empties
	static const short sort3_1_3[2][2] =
		{{ 0x0000,	// case 1(x1) 3(x2 x3 x4)	// x4x1x2x3-x3x1x2x4-x2x1x3x4-x1x2x3x4
		   0x1100 },	// case 1(x2) 3(x1 x3 x4)	// x4x2x1x3-x3x2x1x4-x2x1x3x4-x1x2x3x4
		 { 0x2011,	// case 1(x3) 3(x1 x2 x4)	// x4x3x1x2-x3x1x2x4-x2x3x1x4-x1x3x2x4
		   0x0222 }};	// case 1(x4) 3(x1 x2 x3)	// x4x1x2x3-x3x4x1x2-x2x4x1x3-x1x4x2x3
	static const short sort3_2_2[2][2] =
		{{ 0x0112,	// case 2(x1 x4) 2(x2 x3)	// x4x1x2x3-x3x2x1x4-x2x3x1x4-x1x4x2x3
		   0x1021 },	// case 2(x1 x3) 2(x2 x4)	// x4x2x1x3-x3x1x2x4-x2x4x1x3-x1x3x2x4
		 { 0x2200,	// case 2(x1 x2) 2(x3 x4)	// x4x3x1x2-x3x4x1x2-x2x1x3x4-x1x2x3x4
		   0x0000 }};	// case 4			// x4x1x2x3-x3x1x2x4-x2x1x3x4-x1x2x3x4
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
	parity = search->eval.parity;
	q1 = QUADRANT_ID[x1];
	q2 = QUADRANT_ID[x2];
	q3 = QUADRANT_ID[x3];
#ifdef __AVX__
	empties_series = _mm_cvtsi32_si128((x1 << 24) | (x2 << 16) | (x3 << 8) | x4);
	if (parity & q1) {
		if (parity & q2) {
			if (parity & q3) { // case 1 3, case 1 1 1 1
				empties_series = _mm_shuffle_epi8(empties_series, shuf_1_3[q1 == q2][q1 == q3].v4);
			} else {	// case 1(x1) 1(x2) 2(x3 x4)	// x4x1x2x3-x3x1x2x4-x2x1x3x4-x1x2x3x4
				empties_series = _mm_shuffle_epi8(empties_series,
					_mm_set_epi32(0x00030201, 0x01030200, 0x02030100, 0x03020100));
			}
		} else { // case 1(x1) 1(x3) 2(x2 x4), case 1(x1) 1(x4) 2(x2 x3)
			empties_series = _mm_shuffle_epi8(empties_series, shuf_x1_1_2[(parity & q3) != 0].v4);
		}
	} else {
		if (parity & q2) { // case 1(x2) 1(x3) 2(x1 x4), case 1(x2) 1(x4) 2(x1 x3)
			empties_series = _mm_shuffle_epi8(empties_series, shuf_x2_1_2[(parity & q3) != 0].v4);
		} else {
			if (parity & q3) { // case 1(x3) 1(x4) 2(x1 x2)	// x2x1x3x4-x1x2x3x4-x4x3x1x2-x3x4x1x2
				empties_series = _mm_shuffle_epi8(empties_series,
					_mm_set_epi32(0x02030100, 0x03020100, 0x00010302, 0x01000302));
			} else {	// case 2 2, case 4
				empties_series = _mm_shuffle_epi8(empties_series, shuf_2_2[q1 == q2][q1 == q3].v4);
			}
		}
	}

#else // SSE
	empties_series = _mm_cvtsi32_si128((x3 << 16) | x4);
	empties_series = _mm_insert_epi16(empties_series, x2, 2);
	empties_series = _mm_insert_epi16(empties_series, x1, 3);
	empties_series = _mm_packus_epi16(_mm_unpacklo_epi64(empties_series, _mm_shufflelo_epi16(empties_series, 0xb4)),
		_mm_unpacklo_epi64(_mm_shufflelo_epi16(empties_series, 0x78), _mm_shufflelo_epi16(empties_series, 0x39)));
							// x4x1x2x3-x3x1x2x4-x2x1x3x4-x1x2x3x4
	if (parity & q1) {
		if (parity & q2) {
			sort3 = 0;	// case 1(x1) 1(x2) 2(x3 x4)
			if (parity & q3) { // case 1 3, case 1 1 1 1
				sort3 = sort3_1_3[q1 == q2][q1 == q3];
			}
		} else {
			if (parity & q3) { // case 1(x1) 1(x3) 2(x2 x4)
				empties_series = _mm_shuffle_epi32(empties_series, 0xd8);	// x4...x2...x3...x1...
				sort3 = 0x0001;		// ..-x1x3x2x4
			} else { // case 1(x1) 1(x4) 2(x2 x3)
				empties_series = _mm_shuffle_epi32(empties_series, 0x9c);	// x3...x2...x4...x1...
				sort3 = 0x0002;		// ..-x1x4x2x3
			}
		}
	} else {
		if (parity & q2) {
			if (parity & q3) { // case 1(x2) 1(x3) 2(x1 x4)
				empties_series = _mm_shuffle_epi32(empties_series, 0xc9);	// x4...x1...x3...x2...
				sort3 = 0x0011;		// ..-x3x2x1x4-x2x3x1x4
			} else { // case 1(x2) 1(x4) 2(x1 x3)
				empties_series = _mm_shuffle_epi32(empties_series, 0x8d);	// x3...x1...x4...x2...
				sort3 = 0x0012;		// ..-x4x2x1x3-x2x4x1x3
			}
		} else {
			if (parity & q3) { // case 1(x3) 1(x4) 2(x1 x2)
				empties_series = _mm_shuffle_epi32(empties_series, 0x4e);	// x2...x1...x4...x3...
				sort3 = 0x0022;		// ..-x4x3x1x2-x3x4x1x2
			} else {	// case 2 2, case 4
				sort3 = sort3_2_2[q1 == q2][q1 == q3];
			}
		}
	}
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
