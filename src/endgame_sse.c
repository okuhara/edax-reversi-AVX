/**
 * @file endgame_sse.c
 *
 *
 * SSE / AVX optimized version of endgame.c for the last four empties.
 *
 * Bitboard and empty list are kept in SSE registers.
 *
 * @date 1998 - 2023
 * @author Richard Delorme
 * @author Toshihiko Okuhara
 * @version 4.4
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

#ifdef __AVX__
	#define	vflip	__m256i
	static inline int vectorcall TESTZ_FLIP(__m256i X) { return _mm256_testz_si256(X, X); }
#else
	#define	vflip	__m128i
	#if defined(__x86_64__) || defined(_M_X64)
		#define TESTZ_FLIP(X)	(!_mm_cvtsi128_si64(X))
	#else
		static inline int vectorcall TESTZ_FLIP(__m128i X) { return !_mm_cvtsi128_si32(_mm_packs_epi16(X, X)); }
	#endif
#endif

// in count_last_flip_sse.c
extern const uint8_t COUNT_FLIP[8][256];
extern const V4DI mask_dvhd[64];

/**
 * @brief Compute a board resulting of a move played on a previous board.
 *
 * @param OP board to play the move on.
 * @param x move to play.
 * @param flipped flipped returned from mm_Flip.
 * @return resulting board.
 */
static inline __m128i vectorcall board_flip_next(__m128i OP, int x, vflip flipped)
{
	OP = _mm_xor_si128(OP, _mm_or_si128(reduce_vflip(flipped), _mm_loadl_epi64((__m128i *) &X_TO_BIT[x])));
	return _mm_shuffle_epi32(OP, SWAP64);
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
 * @return       The final opponent score, as a disc difference.
 */
static inline int vectorcall board_score_sse_1(__m128i PO, const int alpha, const int pos)
{
	uint_fast8_t	n_flips;
	unsigned int	t;
	unsigned long long P;
	int	score, score2;
	const uint8_t *COUNT_FLIP_X = COUNT_FLIP[pos & 7];
	const uint8_t *COUNT_FLIP_Y = COUNT_FLIP[pos >> 3];
	__m128i	II;

	// n_flips = last_flip(pos, P);
  #ifdef AVXLASTFLIP
	__m256i M = mask_dvhd[pos].v4;
	__m256i PP = _mm256_permute4x64_epi64(_mm256_castsi128_si256(PO), 0x55);

	P = _mm_cvtsi128_si64(_mm256_castsi256_si128(PP));
	n_flips  = COUNT_FLIP_X[(P >> (pos & 0x38)) & 0xFF];
    #ifdef __AVX512VL__
    	t = _cvtmask32_u32(_mm256_test_epi8_mask(PP, M));
    #else
	t = _mm256_movemask_epi8(_mm256_sub_epi8(_mm256_setzero_si256(), _mm256_and_si256(PP, M)));
    #endif
	n_flips += COUNT_FLIP_Y[t & 0xFF];
	t >>= 16;
  #else
	__m128i M0 = mask_dvhd[pos].v2[0];
	__m128i M1 = mask_dvhd[pos].v2[1];
	__m128i	PP = _mm_shuffle_epi32(PO, DUPHI);

	P = _mm_cvtsi128_si64(PP);
	II = _mm_sad_epu8(_mm_and_si128(PP, M0), _mm_setzero_si128());
	n_flips  = COUNT_FLIP_X[_mm_extract_epi16(II, 4)];
	n_flips += COUNT_FLIP_X[_mm_cvtsi128_si32(II)];
    #ifdef __AVX512VL__
    	t = _cvtmask16_u32(_mm_test_epi8_mask(PP, M1));
    #else
	t = _mm_movemask_epi8(_mm_sub_epi8(_mm_setzero_si128(), _mm_and_si128(PP, M1)));
    #endif
  #endif
	n_flips += COUNT_FLIP_Y[t >> 8];
	n_flips += COUNT_FLIP_Y[t & 0xFF];

	score = 2 * bit_count(P) - SCORE_MAX + 2;	// = (bit_count(P) + 1) - (SCORE_MAX - 1 - bit_count(P))
	score += n_flips;

	if (n_flips == 0) {
		score2 = score - 2;	// empty for opponent
		if (score <= 0)
			score = score2;

		if (score > alpha) {	// lazy cut-off
			// n_flips = last_flip(pos, ~P);
  #ifdef AVXLASTFLIP
			PP = _mm256_andnot_si256(PP, M);
			II = _mm_sad_epu8(_mm256_castsi256_si128(PP), _mm_setzero_si128());
			t = _mm_movemask_epi8(_mm_sub_epi8(_mm_setzero_si128(), _mm256_extracti128_si256(PP, 1)));
  #else
			II = _mm_sad_epu8(_mm_andnot_si128(PP, M0), _mm_setzero_si128());
			t = _mm_movemask_epi8(_mm_sub_epi8(_mm_setzero_si128(), _mm_andnot_si128(PP, M1)));
  #endif
			n_flips  = COUNT_FLIP_X[_mm_extract_epi16(II, 4)];
			n_flips += COUNT_FLIP_X[_mm_cvtsi128_si32(II)];
			n_flips += COUNT_FLIP_Y[t >> 8];
			n_flips += COUNT_FLIP_Y[t & 0xFF];

			if (n_flips != 0)
				score = score2 - n_flips;
		}
	}

	return score;
}

// from bench.c
int board_score_1(const unsigned long long player, const int beta, const int x)
{
	return board_score_sse_1(_mm_shuffle_epi32(_mm_cvtsi64_si128(player), SWAP64), beta, x);
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
static int vectorcall board_solve_2(__m128i OP, int alpha, volatile unsigned long long *n_nodes, __m128i empties)
{
	__m128i PO;
	vflip	flipped;
	int score, bestscore, nodes;
	int x1 = _mm_extract_epi16(empties, 1);
	int x2 = _mm_extract_epi16(empties, 0);
	unsigned long long opponent;
	// const int beta = alpha + 1;

	SEARCH_STATS(++statistics.n_board_solve_2);

	opponent = EXTRACT_O(OP);
	if ((NEIGHBOUR[x1] & opponent) && !TESTZ_FLIP(flipped = mm_Flip(OP, x1))) {
		bestscore = board_score_sse_1(_mm_xor_si128(OP, reduce_vflip(flipped)), alpha, x2);

		if ((bestscore > alpha) && (NEIGHBOUR[x2] & opponent) && !TESTZ_FLIP(flipped = mm_Flip(OP, x2))) {
			score = board_score_sse_1(_mm_xor_si128(OP, reduce_vflip(flipped)), alpha, x1);
			if (score < bestscore)
				bestscore = score;
			nodes = 3;
		} else	nodes = 2;

	} else if ((NEIGHBOUR[x2] & opponent) && !TESTZ_FLIP(flipped = mm_Flip(OP, x2))) {
		bestscore = board_score_sse_1(_mm_xor_si128(OP, reduce_vflip(flipped)), alpha, x1);
		nodes = 2;

	} else {	// pass - NEIGHBOUR test is almost 100% true
		alpha = ~alpha;	// = -alpha - 1
		PO = _mm_shuffle_epi32(OP, SWAP64);
		if (!TESTZ_FLIP(flipped = mm_Flip(PO, x1))) {
			bestscore = board_score_sse_1(_mm_xor_si128(PO, reduce_vflip(flipped)), alpha, x2);

			if ((bestscore > alpha) && !TESTZ_FLIP(flipped = mm_Flip(PO, x2))) {
				score = board_score_sse_1(_mm_xor_si128(PO, reduce_vflip(flipped)), alpha, x1);
				if (score < bestscore)
					bestscore = score;
				nodes = 3;
			} else	nodes = 2;

		} else if (!TESTZ_FLIP(flipped = mm_Flip(PO, x2))) {
			bestscore = board_score_sse_1(_mm_xor_si128(PO, reduce_vflip(flipped)), alpha, x1);
			nodes = 2;

		} else {	// gameover
			bestscore = board_solve(EXTRACT_O(PO), 2);
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
static int vectorcall search_solve_3(__m128i OP, int alpha, volatile unsigned long long *n_nodes, __m128i empties)
{
	vflip flipped;
	int score, bestscore, x, pol;
	unsigned long long bb;
	// const int beta = alpha + 1;

	SEARCH_STATS(++statistics.n_search_solve_3);
	SEARCH_UPDATE_INTERNAL_NODES(*n_nodes);

#ifdef __AVX__
	empties = _mm_cvtepu8_epi16(empties);
#elif defined(__SSSE3__)
	empties = _mm_unpacklo_epi8((empties), _mm_setzero_si128());
#endif
	bestscore = -SCORE_INF;
	pol = 1;
	do {
		// best move alphabeta search
		bb = EXTRACT_O(OP);	// opponent
		x = _mm_extract_epi16(empties, 2);
		if ((NEIGHBOUR[x] & bb) && !TESTZ_FLIP(flipped = mm_Flip(OP, x))) {
			bestscore = board_solve_2(board_flip_next(OP, x, flipped), alpha, n_nodes, empties);
			if (bestscore > alpha) return bestscore * pol;
		}

		x = _mm_extract_epi16(empties, 1);
		if (/* (NEIGHBOUR[x] & bb) && */ !TESTZ_FLIP(flipped = mm_Flip(OP, x))) {
			score = board_solve_2(board_flip_next(OP, x, flipped), alpha, n_nodes, _mm_shufflelo_epi16(empties, 0xd8));	// (d3d1)d2d0
			if (score > alpha) return score * pol;
			else if (score > bestscore) bestscore = score;
		}

		x = _mm_extract_epi16(empties, 0);
		if (/* (NEIGHBOUR[x] & bb) && */ !TESTZ_FLIP(flipped = mm_Flip(OP, x))) {
			score = board_solve_2(board_flip_next(OP, x, flipped), alpha, n_nodes, _mm_shufflelo_epi16(empties, 0xc9));	// (d3d0)d2d1
			if (score > bestscore) bestscore = score;
		}

		if (bestscore > -SCORE_INF)
			return bestscore * pol;

		OP = _mm_shuffle_epi32(OP, SWAP64);
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

// pick the move for this ply and pass the rest as packed 3 x 8 bit (AVX/SSSE3) or 3 x 16 bit (SSE), in search order.
#if defined(__SSSE3__) || defined(__AVX__)
  #ifdef __AVX__
	#define	EXTRACT_MOVE(X,i)	_mm_extract_epi8((X), (i) * 4 + 3)
  #else
	#define	EXTRACT_MOVE(X,i)	(_mm_extract_epi16((X), (i) * 2 + 1) >> 8)
  #endif
	#define	v3_empties_0(empties,sort3)	(empties)
	#define	v3_empties(empties,i,shuf,sort3)	_mm_srli_si128((empties), (i) * 4)
#else
	#define	EXTRACT_MOVE(X,i)	_mm_extract_epi16((X), 3 - (i))
	static inline __m128i vectorcall v3_empties_0(__m128i empties, int sort3) {
		// parity based move sorting
		// if (sort3 == 3) empties = _mm_shufflelo_epi16(empties, 0xe1); // swap x2, x3
		if (sort3 & 2)	empties = _mm_shufflelo_epi16(empties, 0xc9); // case 1(x3) 2(x1 x2): x3->x1->x2->x3
		if (sort3 & 1)	empties = _mm_shufflelo_epi16(empties, 0xd8); // case 1(x2) 2(x1 x3): swap x1, x2
		return empties;
	}
	#define	v3_empties(empties,i,shuf,sort3)	v3_empties_0(_mm_shufflelo_epi16((empties), (shuf)), (sort3))
#endif

static int search_solve_4(Search *search, int alpha)
{
	__m128i	OP;
	vflip	flipped;
	__m128i	empties_series;	// (AVX) B15:4th, B11:3rd, B7:2nd, B3:1st, lower 3 bytes for 3 empties
				// (SSE) W3:1st, W2:2nd, W1:3rd, W0:4th
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
		0x0000,	//  0: 1(x1) 3(x2 x3 x4), 1(x1) 1(x2) 2(x3 x4), 1 1 1 1, 4		x4x1x2x3-x3x1x2x4-x2x1x3x4-x1x2x3x4
		0x1100,	//  1: 1(x2) 3(x1 x3 x4)	x4x2x1x3-x3x2x1x4-x2x1x3x4-x1x2x3x4
		0x2011,	//  2: 1(x3) 3(x1 x2 x4)	x4x3x1x2-x3x1x2x4-x2x3x1x4-x1x3x2x4
		0x0222,	//  3: 1(x4) 3(x1 x2 x3)	x4x1x2x3-x3x4x1x2-x2x4x1x3-x1x4x2x3
		0x3000,	//  4: 1(x1) 1(x3) 2(x2 x4)	x4x1x2x3-x2x1x3x4-x3x1x2x4-x1x3x2x4 <- x4x1x3x2-x2x1x3x4-x3x1x2x4-x1x3x2x4
		0x3300,	//  5: 1(x1) 1(x4) 2(x2 x3)	x3x1x2x4-x2x1x3x4-x4x1x2x3-x1x4x2x3 <- x3x1x4x2-x2x1x4x3-x4x1x2x3-x1x4x2x3
		0x2000,	//  6: 1(x2) 1(x3) 2(x1 x4)	x4x1x2x3-x1x2x3x4-x3x2x1x4-x2x3x1x4 <- x4x2x3x1-x1x2x3x4-x3x2x1x4-x2x3x1x4
		0x2300,	//  7: 1(x2) 1(x4) 2(x1 x3)	x3x1x2x4-x1x2x3x4-x4x2x1x3-x2x4x1x3 <- x3x2x4x1-x1x2x4x3-x4x2x1x3-x2x4x1x3
		0x2200,	//  8: 1(x3) 1(x4) 2(x1 x2)	x2x1x3x4-x1x2x3x4-x4x3x1x2-x3x4x1x2 <- x2x3x4x1-x1x3x4x2-x4x3x1x2-x3x4x1x2
		0x2200,	//  9: 2(x1 x2) 2(x3 x4)	x4x3x1x2-x3x4x1x2-x2x1x3x4-x1x2x3x4
		0x1021,	// 10: 2(x1 x3) 2(x2 x4)	x4x2x1x3-x3x1x2x4-x2x4x1x3-x1x3x2x4
		0x0112	// 11: 2(x1 x4) 2(x2 x3)	x4x1x2x3-x3x2x1x4-x2x3x1x4-x1x4x2x3
	};
#endif

	SEARCH_STATS(++statistics.n_search_solve_4);
	SEARCH_UPDATE_INTERNAL_NODES(search->n_nodes);

	// stability cutoff (try 12%, cut 7%)
	if (search_SC_NWS_4(search, alpha, &score)) return score;

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
	switch (paritysort) {
		case 4: // case 1(x1) 1(x3) 2(x2 x4)
			empties_series = _mm_shufflelo_epi16(empties_series, 0xd8);	// x1x3x2x4
			break;
		case 5: // case 1(x1) 1(x4) 2(x2 x3)
			empties_series = _mm_shufflelo_epi16(empties_series, 0xc9);	// x1x4x2x3
			break;
		case 6:	// case 1(x2) 1(x3) 2(x1 x4)
			empties_series = _mm_shufflelo_epi16(empties_series, 0x9c);	// x2x3x1x4
			break;
		case 7: // case 1(x2) 1(x4) 2(x1 x3)
			empties_series = _mm_shufflelo_epi16(empties_series, 0x8d);	// x2x4x1x3
			break;
		case 8:	// case 1(x3) 1(x4) 2(x1 x2)
			empties_series = _mm_shufflelo_epi16(empties_series, 0x4e);	// x3x4x1x2
			break;
	}
	sort3 = sort3_shuf[paritysort];
#endif

	// best move alphabeta search
	bestscore = SCORE_INF;	// min stage
	opp = EXTRACT_O(OP);
	x1 = EXTRACT_MOVE(empties_series, 0);
	if ((NEIGHBOUR[x1] & opp) && !TESTZ_FLIP(flipped = mm_Flip(OP, x1))) {
		bestscore = search_solve_3(board_flip_next(OP, x1, flipped), alpha, &search->n_nodes,
			v3_empties_0(empties_series, sort3));
		if (bestscore <= alpha) return bestscore;
	}

	x2 = EXTRACT_MOVE(empties_series, 1);
	if ((NEIGHBOUR[x2] & opp) && !TESTZ_FLIP(flipped = mm_Flip(OP, x2))) {
		score = search_solve_3(board_flip_next(OP, x2, flipped), alpha, &search->n_nodes,
			v3_empties(empties_series, 1, 0xb4, sort3 >> 4));	// (SSE) x2x1x3x4
		if (score <= alpha) return score;
		else if (score < bestscore) bestscore = score;
	}

	x3 = EXTRACT_MOVE(empties_series, 2);
	if ((NEIGHBOUR[x3] & opp) && !TESTZ_FLIP(flipped = mm_Flip(OP, x3))) {
		score = search_solve_3(board_flip_next(OP, x3, flipped), alpha, &search->n_nodes,
			v3_empties(empties_series, 2, 0x78, sort3 >> 8));	// (SSE) x3x1x2x4
		if (score <= alpha) return score;
		else if (score < bestscore) bestscore = score;
	}

	x4 = EXTRACT_MOVE(empties_series, 3);
	if ((NEIGHBOUR[x4] & opp) && !TESTZ_FLIP(flipped = mm_Flip(OP, x4))) {
		score = search_solve_3(board_flip_next(OP, x4, flipped), alpha, &search->n_nodes,
			v3_empties(empties_series, 3, 0x39, sort3 >> 12));	// (SSE) x4x1x2x3
		if (score < bestscore) bestscore = score;
	}
	else if (bestscore == SCORE_INF) {	// no move
		if (can_move(opp, _mm_cvtsi128_si64(OP))) { // pass
			board_pass(&search->board);
			bestscore = -search_solve_4(search, -(alpha + 1));
			board_pass(&search->board);

		} else { // gameover
			bestscore = board_solve(opp, 4);
		}
	}

	assert(SCORE_MIN <= bestscore && bestscore <= SCORE_MAX);
	return bestscore;
}
