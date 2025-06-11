/**
 * @file endgame_neon.c
 *
 *
 * Arm Neon optimized version of endgame.c for the last four empties.
 *
 * Bitboard and empty list is kept in Neon registers.
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
#include <assert.h>

#define TESTZ_FLIP(X)	(!vgetq_lane_u64((X), 0))

#ifndef HAS_CPU_64
	#define vaddv_u8(x)	vget_lane_u32(vreinterpret_u32_u64(vpaddl_u32(vpaddl_u16(vpaddl_u8(x)))), 0)
#endif

/**
 * @brief Compute a board resulting of a move played on a previous board.
 *
 * @param OP board to play the move on.
 * @param x move to play.
 * @param flipped flipped returned from mm_Flip.
 * @return resulting board.
 */
static inline uint64x2_t board_flip_next(uint64x2_t OP, int x, uint64x2_t flipped)
{
#if !defined(_MSC_VER) && !defined(__clang__)	// MSVC-arm32 does not have vld1q_lane_u64
	// arm64-gcc-13: 8, armv8a-clang-16: 8, msvc-arm64-19: 8, gcc-arm-13: 16, clang-armv7-11: 18
	OP = veorq_u64(OP, vorrq_u64(flipped, vld1q_lane_u64((uint64_t *) &X_TO_BIT[x], flipped, 0)));
	return vextq_u64(OP, OP, 1);
#else	// arm64-gcc-13: 8, armv8a-clang-16: 7, msvc-arm64-19: 7, gcc-arm-13: 21, clang-armv7-11: 15
	OP = veorq_u64(OP, flipped);
	return vcombine_u64(vget_high_u64(OP), vorr_u64(vget_low_u64(OP), vld1_u64((uint64_t *) &X_TO_BIT[x])));
#endif
}

/**
 * @brief Get the final score.
 *
 * Get the final score, when no move can be made.
 *
 * @param P Board.player
 * @param n_empties Number of empty squares remaining on the board.
 * @return The final score, as a disc difference.
 */
static int board_solve_neon(uint64x1_t P, int n_empties)
{
	int score = vaddv_u8(vcnt_u8(vreinterpret_u8_u64(P))) * 2 - SCORE_MAX;	// in case of opponents win
	int diff = score + n_empties;		// = n_discs_p - (64 - n_empties - n_discs_p)

	SEARCH_STATS(++statistics.n_search_solve);

	if (diff == 0)
		score = diff;
	else if (diff > 0)
		score = diff + n_empties;
	return score;
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
static int solve_2(uint64x2_t OP, int alpha, volatile unsigned long long *n_nodes, uint8x8_t empties)
{
	uint64x2_t flipped;
	int score, bestscore, nodes;
	int x1 = vget_lane_u8(empties, 1);
	int x2 = vget_lane_u8(empties, 0);
	unsigned long long opponent;
	// const int beta = alpha + 1;

	SEARCH_STATS(++statistics.n_solve_2);

	opponent = vgetq_lane_u64(OP, 1);
	if ((NEIGHBOUR[x1] & opponent) && !TESTZ_FLIP(flipped = mm_Flip(OP, x1))) {
		bestscore = mm_solve_1(vget_high_u64(veorq_u64(OP, flipped)), alpha, x2);

		if ((bestscore > alpha) && (NEIGHBOUR[x2] & opponent) && !TESTZ_FLIP(flipped = mm_Flip(OP, x2))) {
			score = mm_solve_1(vget_high_u64(veorq_u64(OP, flipped)), alpha, x1);
			if (score < bestscore)
				bestscore = score;
			nodes = 3;
		} else	nodes = 2;

	} else if ((NEIGHBOUR[x2] & opponent) && !TESTZ_FLIP(flipped = mm_Flip(OP, x2))) {
		bestscore = mm_solve_1(vget_high_u64(veorq_u64(OP, flipped)), alpha, x1);
		nodes = 2;

	} else {	// pass - NEIGHBOUR test is almost 100% true
		alpha = ~alpha;	// = -alpha - 1
		OP = vextq_u64(OP, OP, 1);
		if (!TESTZ_FLIP(flipped = mm_Flip(OP, x1))) {
			bestscore = mm_solve_1(vget_high_u64(veorq_u64(OP, flipped)), alpha, x2);

			if ((bestscore > alpha) && !TESTZ_FLIP(flipped = mm_Flip(OP, x2))) {
				score = mm_solve_1(vget_high_u64(veorq_u64(OP, flipped)), alpha, x1);
				if (score < bestscore)
					bestscore = score;
				nodes = 3;
			} else	nodes = 2;

		} else if (!TESTZ_FLIP(flipped = mm_Flip(OP, x2))) {
			bestscore = mm_solve_1(vget_high_u64(veorq_u64(OP, flipped)), alpha, x1);
			nodes = 2;

		} else {	// gameover
			bestscore = board_solve_neon(vget_high_u64(OP), 2);
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
static int solve_3(uint64x2_t OP, int alpha, volatile unsigned long long *n_nodes, uint8x8_t empties)
{
	uint64x2_t flipped;
	int score, bestscore, x, pol;
	unsigned long long opponent;

	SEARCH_STATS(++statistics.n_solve_3);
	SEARCH_UPDATE_INTERNAL_NODES(*n_nodes);

	bestscore = -SCORE_INF;
	pol = 1;
	do {
		// best move alphabeta search
		opponent = vgetq_lane_u64(OP, 1);
		x = vget_lane_u8(empties, 2);
		if ((NEIGHBOUR[x] & opponent) && !TESTZ_FLIP(flipped = mm_Flip(OP, x))) {
			bestscore = solve_2(board_flip_next(OP, x, flipped), alpha, n_nodes, empties);
			if (bestscore > alpha) return bestscore * pol;
		}

		x = vget_lane_u8(empties, 1);
		if (/* (NEIGHBOUR[x] & opponent) && */ !TESTZ_FLIP(flipped = mm_Flip(OP, x))) {
			score = solve_2(board_flip_next(OP, x, flipped), alpha, n_nodes, vuzp_u8(empties, empties).val[0]);	// d2d0
			if (score > alpha) return score * pol;
			else if (score > bestscore) bestscore = score;
		}

		x = vget_lane_u8(empties, 0);
		if (/* (NEIGHBOUR[x] & opponent) && */ !TESTZ_FLIP(flipped = mm_Flip(OP, x))) {
			score = solve_2(board_flip_next(OP, x, flipped), alpha, n_nodes, vext_u8(empties, empties, 1));	// d2d1
			if (score > bestscore) bestscore = score;
			return bestscore * pol;
		}

		if (bestscore > -SCORE_INF)
			return bestscore * pol;

		OP = vextq_u64(OP, OP, 1);	// pass
		alpha = ~alpha;	// = -(alpha + 1)
	} while ((pol = -pol) < 0);

	return board_solve_neon(vget_low_u64(OP), 3);	// gameover
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
	uint64x2_t	OP, flipped;
	uint8x16_t	empties_series;	// B15:4th, B11:3rd, B7:2nd, B3:1st, lower 3 bytes for 3 empties
	uint8x16_t	shuf;
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
	static const uint64x2_t shuf_mask[] = {	// 4.5.5: prefer 1 empty over 3 empties
		{ 0x0203010003020100, 0x0003020101030200 },	//  0: 1(x1) 3(x2 x3 x4), 1(x1) 1(x2) 2(x3 x4), 1 1 1 1, 4
		{ 0x0302010002030100, 0x0003020101030200 },	//  1: 1(x2) 3(x1 x3 x4)
		{ 0x0302010001030200, 0x0003020102030100 },	//  2: 1(x3) 3(x1 x2 x4)
		{ 0x0302010000030201, 0x0103020002030100 },	//  3: 1(x4) 3(x1 x2 x3)
		{ 0x0103020003010200, 0x0003020102030100 },	//  4: 1(x1) 1(x3) 2(x2 x4)	x4x1x2x3-x2x1x3x4-x3x1x2x4-x1x3x2x4
		{ 0x0003020103000201, 0x0103020002030100 },	//  5: 1(x1) 1(x4) 2(x2 x3)	x3x1x2x4-x2x1x3x4-x4x1x2x3-x1x4x2x3
		{ 0x0102030002010300, 0x0003020103020100 },	//  6: 1(x2) 1(x3) 2(x1 x4)	x4x1x2x3-x1x2x3x4-x3x2x1x4-x2x3x1x4
		{ 0x0002030102000301, 0x0103020003020100 },	//  7: 1(x2) 1(x4) 2(x1 x3)	x3x1x2x4-x1x2x3x4-x4x2x1x3-x2x4x1x3
		{ 0x0001030201000302, 0x0203010003020100 },	//  8: 1(x3) 1(x4) 2(x1 x2)	x2x1x3x4-x1x2x3x4-x4x3x1x2-x3x4x1x2
		{ 0x0203010003020100, 0x0001030201000302 },	//  9: 2(x1 x2) 2(x3 x4)	x4x3x1x2-x3x4x1x2-x2x1x3x4-x1x2x3x4
		{ 0x0200030103010200, 0x0002030101030200 },	// 10: 2(x1 x3) 2(x2 x4)	x4x2x1x3-x3x1x2x4-x2x4x1x3-x1x3x2x4
		{ 0x0201030003000201, 0x0003020101020300 }	// 11: 2(x1 x4) 2(x2 x3)	x4x1x2x3-x3x2x1x4-x2x3x1x4-x1x4x2x3
	};

	SEARCH_STATS(++statistics.n_search_solve_4);
	SEARCH_UPDATE_INTERNAL_NODES(search->n_nodes);

	// stability cutoff (try 12%, cut 7%)
	if (search_SC_NWS_4(search->board.player, search->board.opponent, alpha, &score)) return score;

	OP = vld1q_u64((uint64_t *) &search->board);
	x1 = search->empties[NOMOVE].next;
	x2 = search->empties[x1].next;
	x3 = search->empties[x2].next;
	x4 = search->empties[x3].next;

	// parity based move sorting.
	// The following hole sizes are possible:
	//    4 - 1 3 - 2 2 - 1 1 2 - 1 1 1 1
	// Only the 1 1 2 case needs move sorting on this ply.
	empties_series = vreinterpretq_u8_u32(vdupq_n_u32((x1 << 24) | (x2 << 16) | (x3 << 8) | x4));
	paritysort = parity_case[((x3 ^ x4) & 0x24) + (((x2 ^ x4) & 0x24) >> 1) + (((x1 ^ x4) & 0x24) >> 2)];
	shuf = vreinterpretq_u8_u64(shuf_mask[paritysort]);
#ifdef HAS_CPU_64
	empties_series = vqtbl1q_u8(empties_series, shuf);
#else
	empties_series = vcombine_u8(vtbl1_u8(vget_low_u8(empties_series), vget_low_u8(shuf)),
		vtbl1_u8(vget_low_u8(empties_series), vget_high_u8(shuf)));
#endif

	bestscore = SCORE_INF;	// min stage
	pol = 1;
	do {
		// best move alphabeta search
		opponent = vgetq_lane_u64(OP, 1);
		x1 = vgetq_lane_u8(empties_series, 3);
		if ((NEIGHBOUR[x1] & opponent) && !TESTZ_FLIP(flipped = mm_Flip(OP, x1))) {
			bestscore = solve_3(board_flip_next(OP, x1, flipped), alpha, &search->n_nodes,
				vget_low_u8(empties_series));
			if (bestscore <= alpha) return bestscore * pol;
		}

		x2 = vgetq_lane_u8(empties_series, 7);
		if ((NEIGHBOUR[x2] & opponent) && !TESTZ_FLIP(flipped = mm_Flip(OP, x2))) {
			score = solve_3(board_flip_next(OP, x2, flipped), alpha, &search->n_nodes,
				vget_low_u8(vextq_u8(empties_series, empties_series, 4)));
			if (score <= alpha) return score * pol;
			else if (score < bestscore) bestscore = score;
		}

		x3 = vgetq_lane_u8(empties_series, 11);
		if ((NEIGHBOUR[x3] & opponent) && !TESTZ_FLIP(flipped = mm_Flip(OP, x3))) {
			score = solve_3(board_flip_next(OP, x3, flipped), alpha, &search->n_nodes,
				vget_high_u8(empties_series));
			if (score <= alpha) return score * pol;
			else if (score < bestscore) bestscore = score;
		}

		x4 = vgetq_lane_u8(empties_series, 15);
		if ((NEIGHBOUR[x4] & opponent) && !TESTZ_FLIP(flipped = mm_Flip(OP, x4))) {
			score = solve_3(board_flip_next(OP, x4, flipped), alpha, &search->n_nodes,
				vget_low_u8(vextq_u8(empties_series, empties_series, 12)));
			if (score < bestscore) bestscore = score;
			return bestscore * pol;
		}

		if (bestscore < SCORE_INF)
			return bestscore * pol;

		OP = vextq_u64(OP, OP, 1);	// pass
		alpha = ~alpha;	// = -(alpha + 1)
	} while ((pol = -pol) < 0);

	return board_solve_neon(vget_high_u64(OP), 4);	// gameover
}
