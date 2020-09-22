/**
 * @file endgame_neon.c
 *
 *
 * Arm Neon optimized version of endgame.c for the last four empties.
 *
 * Bitboard and empty list is kept in Neon registers.
 *
 * @date 1998 - 2020
 * @author Richard Delorme
 * @author Toshihiko Okuhara
 * @version 4.4
 * 
 */

#include "bit_intrinsics.h"
#include "settings.h"
#include "search.h"
#include <assert.h>

#define TESTZ_FLIP(X)	(!vgetq_lane_u64((X), 0))

#ifndef HAS_CPU_64
#define vaddv_u8(x)	vget_lane_u64(vpaddl_u32(vpaddl_u16(vpaddl_u8(x))), 0)
#define vaddvq_u16(x)	vget_lane_u64(vpaddl_u32(vpaddl_u16(vadd_u16(vget_high_u16(x), vget_low_u16(x)))), 0)
#endif

// in count_last_flip_neon.c
extern const unsigned char COUNT_FLIP[8][256];
extern const uint64x2_t mask_dvhd[64][2];

/**
 * @brief Compute a board resulting of a move played on a previous board.
 *
 * @param OP board to play the move on.
 * @param x move to play.
 * @param next resulting board.
 * @return true if no flips.
 */
static inline uint64x2_t board_next_neon(uint64x2_t OP, int x, uint64x2_t flipped)
{
	OP = veorq_u64(OP, flipped);
	return vcombine_u64(vget_high_u64(OP), vorr_u64(vget_low_u64(OP), vcreate_u64(X_TO_BIT[x])));
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
static int board_solve_neon(uint64x2_t OP, int n_empties)
{
	int score = vaddv_u8(vcnt_u8(vreinterpret_u8_u64(vget_low_u64(OP)))) * 2 - SCORE_MAX;	// in case of opponents win
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
static int board_score_sse_1(uint64x2_t OP, const int beta, const int pos)
{
	int	score, score2;
	unsigned int	n_flips, t0, t1, m;
	const unsigned char *COUNT_FLIP_X = COUNT_FLIP[pos & 7];
	const unsigned char *COUNT_FLIP_Y = COUNT_FLIP[pos >> 3];
	uint8x16_t	PP = vzipq_u8(vreinterpretq_u8_u64(OP), vreinterpretq_u8_u64(OP)).val[0];
	const uint8x16_t dmask = { 1, 1, 2, 2, 4, 4, 8, 8, 16, 16, 32, 32, 64, 64, 128, 128 };
	static const unsigned short o_mask[64] = {
		0xff01, 0x7f03, 0x3f07, 0x1f0f, 0x0f1f, 0x073f, 0x037f, 0x01ff,
		0xfe03, 0xff07, 0x7f0f, 0x3f1f, 0x1f3f, 0x0f7f, 0x07ff, 0x03fe,
		0xfc07, 0xfe0f, 0xff1f, 0x7f3f, 0x3f7f, 0x1fff, 0x0ffe, 0x07fc,
		0xf80f, 0xfc1f, 0xfe3f, 0xff7f, 0x7fff, 0x3ffe, 0x1ffc, 0x0ff8,
		0xf01f, 0xf83f, 0xfc7f, 0xfeff, 0xfffe, 0x7ffc, 0x3ff8, 0x1ff0,
		0xe03f, 0xf07f, 0xf8ff, 0xfcfe, 0xfefc, 0xfff8, 0x7ff0, 0x3fe0,
		0xc07f, 0xe0ff, 0xf0fe, 0xf8fc, 0xfcf8, 0xfef0, 0xffe0, 0x7fc0,
		0x80ff, 0xc0fe, 0xe0fc, 0xf0f8, 0xf8f0, 0xfce0, 0xfec0, 0xff80
	};

	score = SCORE_MAX - 2 - 2 * vaddv_u8(vcnt_u8(vreinterpret_u8_u64(vget_low_u64(OP))));	// 2 * bit_count(O) - SCORE_MAX

	// n_flips = last_flip(pos, P);
	t0 = vaddvq_u16(vreinterpretq_u16_u64(vandq_u64(vreinterpretq_u64_u8(PP), mask_dvhd[pos][0])));
	n_flips  = COUNT_FLIP_X[t0 >> 8];
	n_flips += COUNT_FLIP_X[(unsigned char) t0];
	t1 = vaddvq_u16(vreinterpretq_u16_u8(vandq_u8(vtstq_u8(PP, vreinterpretq_u8_u64(mask_dvhd[pos][1])), dmask)));
	n_flips += COUNT_FLIP_Y[t1 >> 8];
	n_flips += COUNT_FLIP_Y[(unsigned char) t1];
	score -= n_flips;

	if (n_flips == 0) {
		score2 = score + 2;	// empty for player
		if (score >= 0)
			score = score2;

		if (score < beta) {	// lazy cut-off
			// n_flips = last_flip(pos, O);
			m = o_mask[pos];	// valid diagonal bits
			n_flips  = COUNT_FLIP_X[(t0 >> 8) ^ 0xff];
			n_flips += COUNT_FLIP_X[(unsigned char) (t0 ^ m)];
			n_flips += COUNT_FLIP_Y[(t1 ^ m) >> 8];
			n_flips += COUNT_FLIP_Y[(unsigned char) ~t1];

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
static int board_solve_neon_2(uint64x2_t OP, int alpha, volatile unsigned long long *n_nodes, uint8x8_t empties)
{
	uint64x2_t flipped, PO;
	int score, bestscore, nodes;
	int x1 = vget_lane_u8(empties, 1);
	int x2 = vget_lane_u8(empties, 0);
	unsigned long long bb;
	// const int beta = alpha + 1;

	SEARCH_STATS(++statistics.n_board_solve_2);

	bb = vgetq_lane_u64(OP, 1);	// opponent
	if ((NEIGHBOUR[x1] & bb) && !TESTZ_FLIP(flipped = mm_Flip(OP, x1))) {
		bestscore = board_score_sse_1(board_next_neon(OP, x1, flipped), alpha + 1, x2);
		nodes = 2;

		if ((bestscore <= alpha) && (NEIGHBOUR[x2] & bb) && !TESTZ_FLIP(flipped = mm_Flip(OP, x2))) {
			score = board_score_sse_1(board_next_neon(OP, x2, flipped), alpha + 1, x1);
			if (score > bestscore) bestscore = score;
			nodes = 3;
		}

	} else if ((NEIGHBOUR[x2] & bb) && !TESTZ_FLIP(flipped = mm_Flip(OP, x2))) {
		bestscore = board_score_sse_1(board_next_neon(OP, x2, flipped), alpha + 1, x1);
		nodes = 2;

	} else {	// pass
		bb = vgetq_lane_u64(OP, 0);	// player
		PO = vextq_u64(OP, OP, 1);
		if ((NEIGHBOUR[x1] & bb) && !TESTZ_FLIP(flipped = mm_Flip(PO, x1))) {
			bestscore = -board_score_sse_1(board_next_neon(PO, x1, flipped), -alpha, x2);
			nodes = 2;

			if ((bestscore > alpha) && (NEIGHBOUR[x2] & bb) && !TESTZ_FLIP(flipped = mm_Flip(PO, x2))) {
				score = -board_score_sse_1(board_next_neon(PO, x2, flipped), -alpha, x1);
				if (score < bestscore) bestscore = score;
				nodes = 3;
			}

		} else if ((NEIGHBOUR[x2] & bb) && !TESTZ_FLIP(flipped = mm_Flip(PO, x2))) {
			bestscore = -board_score_sse_1(board_next_neon(PO, x2, flipped), -alpha, x1);
			nodes = 2;

		} else {	// gameover
			bestscore = board_solve_neon(OP, 2);
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
static int search_solve_sse_3(uint64x2_t OP, int alpha, volatile unsigned long long *n_nodes, uint8x8_t empties)
{
	uint64x2_t flipped, PO;
	int score, bestscore, x;
	unsigned long long bb;
	// const int beta = alpha + 1;

	SEARCH_STATS(++statistics.n_search_solve_3);
	SEARCH_UPDATE_INTERNAL_NODES(*n_nodes);

	// best move alphabeta search
	bestscore = -SCORE_INF;
	bb = vgetq_lane_u64(OP, 1);	// opponent
	x = vget_lane_u8(empties, 2);
	if ((NEIGHBOUR[x] & bb) && !TESTZ_FLIP(flipped = mm_Flip(OP, x))) {
		bestscore = -board_solve_neon_2(board_next_neon(OP, x, flipped), -(alpha + 1), n_nodes, empties);
		if (bestscore > alpha) return bestscore;
	}

	x = vget_lane_u8(empties, 1);
	if ((NEIGHBOUR[x] & bb) && !TESTZ_FLIP(flipped = mm_Flip(OP, x))) {
		score = -board_solve_neon_2(board_next_neon(OP, x, flipped), -(alpha + 1), n_nodes, vuzp_u8(empties, empties).val[0]);
		if (score > alpha) return score;
		else if (score > bestscore) bestscore = score;
	}

	x = vget_lane_u8(empties, 0);
	if ((NEIGHBOUR[x] & bb) && !TESTZ_FLIP(flipped = mm_Flip(OP, x))) {
		score = -board_solve_neon_2(board_next_neon(OP, x, flipped), -(alpha + 1), n_nodes, vext_u8(empties, empties, 1));
		if (score > bestscore) bestscore = score;
	}

	else if (bestscore == -SCORE_INF) {	// pass ?
		// best move alphabeta search
		bestscore = SCORE_INF;
		bb = vgetq_lane_u64(OP, 0);	// player
		PO = vextq_u64(OP, OP, 1);
		x = vget_lane_u8(empties, 2);
		if ((NEIGHBOUR[x] & bb) && !TESTZ_FLIP(flipped = mm_Flip(PO, x))) {
			bestscore = board_solve_neon_2(board_next_neon(PO, x, flipped), alpha, n_nodes, empties);
			if (bestscore <= alpha) return bestscore;
		}

		x = vget_lane_u8(empties, 1);
		if ((NEIGHBOUR[x] & bb) && !TESTZ_FLIP(flipped = mm_Flip(PO, x))) {
			score = board_solve_neon_2(board_next_neon(PO, x, flipped), alpha, n_nodes, vuzp_u8(empties, empties).val[0]);
			if (score <= alpha) return score;
			else if (score < bestscore) bestscore = score;
		}

		x = vget_lane_u8(empties, 0);
		if ((NEIGHBOUR[x] & bb) && !TESTZ_FLIP(flipped = mm_Flip(PO, x))) {
			score = board_solve_neon_2(board_next_neon(PO, x, flipped), alpha, n_nodes, vext_u8(empties, empties, 1));
			if (score < bestscore) bestscore = score;
		}

		else if (bestscore == SCORE_INF)	// gameover
			bestscore = board_solve_neon(OP, 3);
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
	uint64x2_t	OP, flipped;
	uint8x16_t	empties_series;	// B15:4th, B11:3rd, B7:2nd, B3:1st, lower 3 bytes for 3 empties
	uint32x4_t	shuf;
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
	static const uint32x4_t shuf_mask[] = {
		{ 0x03020100, 0x02030100, 0x01030200, 0x00030201 },	//  0: 1(x1) 3(x2 x3 x4), 1(x1) 1(x2) 2(x3 x4), 1 1 1 1, 4
		{ 0x03020100, 0x02030100, 0x01020300, 0x00020301 },	//  1: 1(x2) 3(x1 x3 x4)
		{ 0x03010200, 0x02010300, 0x01030200, 0x00010302 },	//  2: 1(x3) 3(x1 x2 x4)
		{ 0x03000201, 0x02000301, 0x01000302, 0x00030201 },	//  3: 1(x4) 3(x1 x2 x3)
		{ 0x03010200, 0x01030200, 0x02030100, 0x00030201 },	//  4: 1(x1) 1(x3) 2(x2 x4)	x4x1x2x3-x2x1x3x4-x3x1x2x4-x1x3x2x4
		{ 0x03000201, 0x00030201, 0x02030100, 0x01030200 },	//  5: 1(x1) 1(x4) 2(x2 x3)	x3x1x2x4-x2x1x3x4-x4x1x2x3-x1x4x2x3
		{ 0x02010300, 0x01020300, 0x03020100, 0x00030201 },	//  6: 1(x2) 1(x3) 2(x1 x4)	x4x1x2x3-x1x2x3x4-x3x2x1x4-x2x3x1x4
		{ 0x02000301, 0x00020301, 0x03020100, 0x01030200 },	//  7: 1(x2) 1(x4) 2(x1 x3)	x3x1x2x4-x1x2x3x4-x4x2x1x3-x2x4x1x3
		{ 0x01000302, 0x00010302, 0x03020100, 0x02030100 },	//  8: 1(x3) 1(x4) 2(x1 x2)	x2x1x3x4-x1x2x3x4-x4x3x1x2-x3x4x1x2
		{ 0x03020100, 0x02030100, 0x01000302, 0x00010302 },	//  9: 2(x1 x2) 2(x3 x4)		x4x3x1x2-x3x4x1x2-x2x1x3x4-x1x2x3x4
		{ 0x03010200, 0x02000301, 0x01030200, 0x00020301 },	// 10: 2(x1 x3) 2(x2 x4)		x4x2x1x3-x3x1x2x4-x2x4x1x3-x1x3x2x4
		{ 0x03000201, 0x02010300, 0x01020300, 0x00030201 }	// 11: 2(x1 x4) 2(x2 x3)		x4x1x2x3-x3x2x1x4-x2x3x1x4-x1x4x2x3
	};

	SEARCH_STATS(++statistics.n_search_solve_4);
	SEARCH_UPDATE_INTERNAL_NODES(search->n_nodes);

	// stability cutoff
	if (search_SC_NWS(search, alpha, &score)) return score;

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
	shuf = shuf_mask[paritysort];
#ifdef HAS_CPU_64
	empties_series = vqtbl1q_u8(empties_series, vreinterpretq_u8_u32(shuf));
#else
	empties_series = vcombine_u8(vtbl1_u8(vget_low_u8(empties_series), vget_low_u8(vreinterpretq_u8_u32(shuf))),
		vtbl1_u8(vget_low_u8(empties_series), vget_high_u8(vreinterpretq_u8_u32(shuf))));
#endif

	// best move alphabeta search
	bestscore = -SCORE_INF;
	opp = vgetq_lane_u64(OP, 1);
	x1 = vgetq_lane_u8(empties_series, 3);
	if ((NEIGHBOUR[x1] & opp) && !TESTZ_FLIP(flipped = mm_Flip(OP, x1))) {
		bestscore = -search_solve_sse_3(board_next_neon(OP, x1, flipped), -(alpha + 1), &search->n_nodes, vget_low_u8(empties_series));
		if (bestscore > alpha) return bestscore;
	}

	empties_series = vextq_u8(empties_series, empties_series, 4);
	x2 = vgetq_lane_u8(empties_series, 3);
	if ((NEIGHBOUR[x2] & opp) && !TESTZ_FLIP(flipped = mm_Flip(OP, x2))) {
		score = -search_solve_sse_3(board_next_neon(OP, x2, flipped), -(alpha + 1), &search->n_nodes, vget_low_u8(empties_series));
		if (score > alpha) return score;
		else if (score > bestscore) bestscore = score;
	}

	empties_series = vextq_u8(empties_series, empties_series, 4);
	x3 = vgetq_lane_u8(empties_series, 3);
	if ((NEIGHBOUR[x3] & opp) && !TESTZ_FLIP(flipped = mm_Flip(OP, x3))) {
		score = -search_solve_sse_3(board_next_neon(OP, x3, flipped), -(alpha + 1), &search->n_nodes, vget_low_u8(empties_series));
		if (score > alpha) return score;
		else if (score > bestscore) bestscore = score;
	}

	empties_series = vextq_u8(empties_series, empties_series, 4);
	x4 = vgetq_lane_u8(empties_series, 3);
	if ((NEIGHBOUR[x4] & opp) && !TESTZ_FLIP(flipped = mm_Flip(OP, x4))) {
		score = -search_solve_sse_3(board_next_neon(OP, x4, flipped), -(alpha + 1), &search->n_nodes, vget_low_u8(empties_series));
		if (score > bestscore) bestscore = score;
	}

	else if (bestscore == -SCORE_INF) {	// no move
		if (can_move(opp, vgetq_lane_u64(OP, 0))) { // pass
			search_pass_endgame(search);
			bestscore = -search_solve_4(search, -(alpha + 1));
			search_pass_endgame(search);
		} else { // gameover
			bestscore = board_solve_neon(OP, 4);
		}
	}

	assert(SCORE_MIN <= bestscore && bestscore <= SCORE_MAX);
	return bestscore;
}
