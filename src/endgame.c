/**
 * @file endgame.c
 *
 * Search near the end of the game.
 *
 * @date 1998 - 2025
 * @author Richard Delorme
 * @author Toshihiko Okuhara
 * @version 4.5
 */

#include "search.h"

#include "bit.h"
#include "settings.h"
#include "stats.h"
#include "ybwc.h"

#include <assert.h>

#if COUNT_LAST_FLIP == COUNT_LAST_FLIP_32
	#include "count_last_flip_32.c"
#elif COUNT_LAST_FLIP == COUNT_LAST_FLIP_CARRY
	#include "count_last_flip_carry_64.c"
#elif COUNT_LAST_FLIP == COUNT_LAST_FLIP_BITSCAN
	#include "count_last_flip_bitscan.c"
#elif COUNT_LAST_FLIP == COUNT_LAST_FLIP_BMI
	#include "count_last_flip_bmi.c"
#elif COUNT_LAST_FLIP == COUNT_LAST_FLIP_PLAIN
	#include "count_last_flip_plain.c"
#elif COUNT_LAST_FLIP == COUNT_LAST_FLIP_BMI2
	#include "count_last_flip_bmi2.c"
#elif COUNT_LAST_FLIP == COUNT_LAST_FLIP_SSE
	#include "count_last_flip_sse.c"
#elif COUNT_LAST_FLIP == COUNT_LAST_FLIP_AVX_PPFILL
	#include "count_last_flip_avx_ppfill.c"
#elif COUNT_LAST_FLIP == COUNT_LAST_FLIP_AVX512
	#include "count_last_flip_avx512cd.c"
#elif COUNT_LAST_FLIP == COUNT_LAST_FLIP_NEON
	#include "count_last_flip_neon.c"
#elif COUNT_LAST_FLIP == COUNT_LAST_FLIP_SVE
	#include "count_last_flip_sve_lzcnt.c"
#else // COUNT_LAST_FLIP == COUNT_LAST_FLIP_KINDERGARTEN
	#include "count_last_flip_kindergarten.c"
#endif

/**
 * @brief Get the final score.
 *
 * Get the final score, when no move can be made.
 *
 * @param player Board.player
 * @param n_empties Number of empty squares remaining on the board.
 * @return The final score, as a disc difference.
 */
int board_solve(const unsigned long long player, const int n_empties)
{
	int score = bit_count(player) * 2 - SCORE_MAX;	// in case of opponents win
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
 * Get the final score, when no move can be made.
 *
 * @param search Search.
 * @return The final score, as a disc difference.
 */
int search_solve(const Search *search)
{
	return board_solve(search->board.player, search->eval.n_empties);
}

/**
 * @brief Get the final score.
 *
 * Get the final score, when the board is full.
 *
 * @param search Search.
 * @return The final score, as a disc difference.
 */
int search_solve_0(const Search *search)
{
	SEARCH_STATS(++statistics.n_search_solve_0);

	return 2 * bit_count(search->board.player) - SCORE_MAX;
}

#if (MOVE_GENERATOR >= MOVE_GENERATOR_SSE) && (MOVE_GENERATOR <= MOVE_GENERATOR_AVX512)
	#include "endgame_sse.c"	// vectorcall version
#elif (MOVE_GENERATOR >= MOVE_GENERATOR_NEON) && (MOVE_GENERATOR <= MOVE_GENERATOR_SVE)
	#include "endgame_neon.c"
#else
/**
 * @brief Get the final score.
 *
 * Get the final min score, when 2 empty squares remain.
 *
 * @param player Board.player to evaluate.
 * @param opponent Board.opponent to evaluate.
 * @param alpha Alpha bound.
 * @param x1 First empty square coordinate.
 * @param x2 Second empty square coordinate.
 * @param n_nodes Node counter.
 * @return The final min score, as a disc difference.
 */
static int solve_2(unsigned long long player, unsigned long long opponent, int alpha, int x1, int x2, volatile unsigned long long *n_nodes)
{
	unsigned long long flipped;
	int score, bestscore, nodes;
	// const int beta = alpha + 1;

	SEARCH_STATS(++statistics.n_solve_2);

	if ((NEIGHBOUR[x1] & opponent) && (flipped = Flip(x1, player, opponent))) {	// (84%/84%)
		bestscore = solve_1(opponent ^ flipped, alpha, x2);

		if ((bestscore > alpha) && (NEIGHBOUR[x2] & opponent) && (flipped = Flip(x2, player, opponent))) {	// (50%/93%/92%)
			score = solve_1(opponent ^ flipped, alpha, x1);
			if (score < bestscore)
				bestscore = score;
			nodes = 3;
		} else	nodes = 2;

	} else if ((NEIGHBOUR[x2] & opponent) && (flipped = Flip(x2, player, opponent))) {	// (96%/75%)
		bestscore = solve_1(opponent ^ flipped, alpha, x1);
		nodes = 2;

	} else {	// pass (17%) - NEIGHBOUR test is almost 100% true
		alpha = ~alpha;	// = -alpha - 1
		if ((flipped = Flip(x1, opponent, player))) {	// (95%)
			bestscore = solve_1(player ^ flipped, alpha, x2);

			if ((bestscore > alpha) && (flipped = Flip(x2, opponent, player))) {	// (20%/100%)
				score = solve_1(player ^ flipped, alpha, x1);
				if (score < bestscore)
					bestscore = score;
				nodes = 3;
			} else	nodes = 2;

		} else if ((flipped = Flip(x2, opponent, player))) {	// (97%)
			bestscore = solve_1(player ^ flipped, alpha, x1);
			nodes = 2;

		} else {	// gameover
			bestscore = board_solve(player, 2);
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
 * @param player Board.player to evaluate.
 * @param opponent Board.opponent to evaluate.
 * @param alpha Alpha bound.
 * @param shuf3 Parity flags.
 * @param empties Packed empty square coordinates.
 * @param n_nodes Node counter.
 * @return The final max score, as a disc difference.
 */
static int solve_3(unsigned long long player, unsigned long long opponent, int alpha, unsigned int shuf3, unsigned int empties3, volatile unsigned long long *n_nodes)
{
	unsigned long long flipped, next_player, next_opponent;
	int score, bestscore, pol;
	// const int beta = alpha + 1;
	int x1 = (empties3 >> ((shuf3 & 0x30) >> 1)) & 0xFF;
	int x2 = (empties3 >> ((shuf3 & 0x0C) << 1)) & 0xFF;
	int x3 = (empties3 >> ((shuf3 & 0x03) * 8)) & 0xFF;

	SEARCH_STATS(++statistics.n_solve_3);
	SEARCH_UPDATE_INTERNAL_NODES(*n_nodes);

	bestscore = -SCORE_INF;
	pol = 1;
	do {
		// best move alphabeta search
		if ((NEIGHBOUR[x1] & opponent) && (flipped = Flip(x1, player, opponent))) {	// (89%/91%)
			next_player = opponent ^ flipped;
			next_opponent = player ^ (flipped | x_to_bit(x1));
			bestscore = solve_2(next_player, next_opponent, alpha, x2, x3, n_nodes);
			if (bestscore > alpha) return bestscore * pol;	// (78%/63%)
		}

		if (/* (NEIGHBOUR[x2] & opponent) && */ (flipped = Flip(x2, player, opponent))) {	// (97%/78%)
			next_player = opponent ^ flipped;
			next_opponent = player ^ (flipped | x_to_bit(x2));
			score = solve_2(next_player, next_opponent, alpha, x1, x3, n_nodes);
			if (score > alpha) return score * pol;	// (32%/9%)
			else if (score > bestscore) bestscore = score;
		}

		if (/* (NEIGHBOUR[x3] & opponent) && */ (flipped = Flip(x3, player, opponent))) {	// (100%/89%)
			next_player = opponent ^ flipped;
			next_opponent = player ^ (flipped | x_to_bit(x3));
			score = solve_2(next_player, next_opponent, alpha, x1, x2, n_nodes);
			if (score > bestscore) bestscore = score;
			return bestscore * pol;	// (26%)
		}

		if (bestscore > -SCORE_INF)	// (76%)
			return bestscore * pol;	// (9%)

		next_opponent = player; player = opponent; opponent = next_opponent;	// pass
		alpha = ~alpha;	// = -(alpha + 1)
	} while ((pol = -pol) < 0);

	return board_solve(player, 3);	// gameover
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
	unsigned long long player, opponent, flipped, next_player, next_opponent;
	int x1, x2, x3, x4, paritysort, score, bestscore, pol;
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
	unsigned int empties_series, shuf4;
	static const unsigned int sort4_shuf[] = {	// 4.5.5: use shuffle mask in non-simd version too.
		0x3978b4e4,	//  0: 1(x1) 3(x2 x3 x4), 1(x1) 1(x2) 2(x3 x4), 1 1 1 1, 4		x4x1x2x3-x3x1x2x4-x2x1x3x4-x1x2x3x4
		0x3978e4b4,	//  1: 1(x2) 3(x1 x3 x4)	x4x1x2x3-x3x1x2x4-x1x2x3x4-x2x1x3x4
		0x39b4e478,	//  2: 1(x3) 3(x1 x2 x4)	x4x1x2x3-x2x1x3x4-x1x2x3x4-x3x1x2x4
		0x78b4e439,	//  3: 1(x4) 3(x1 x2 x3)	x3x1x2x4-x2x1x3x4-x1x2x3x4-x4x1x2x3
		0x39b478d8,	//  4: 1(x1) 1(x3) 2(x2 x4)	x4x1x2x3-x2x1x3x4-x3x1x2x4-x1x3x2x4
		0x78b439c9,	//  5: 1(x1) 1(x4) 2(x2 x3)	x3x1x2x4-x2x1x3x4-x4x1x2x3-x1x4x2x3
		0x39e46c9c,	//  6: 1(x2) 1(x3) 2(x1 x4)	x4x1x2x3-x1x2x3x4-x3x2x1x4-x2x3x1x4
		0x78e42d8d,	//  7: 1(x2) 1(x4) 2(x1 x3)	x3x1x2x4-x1x2x3x4-x4x2x1x3-x2x4x1x3
		0xb4e41e4e,	//  8: 1(x3) 1(x4) 2(x1 x2)	x2x1x3x4-x1x2x3x4-x4x3x1x2-x3x4x1x2
		0x1e4eb4e4,	//  9: 2(x1 x2) 2(x3 x4)	x4x3x1x2-x3x4x1x2-x2x1x3x4-x1x2x3x4
		0x2d788dd8,	// 10: 2(x1 x3) 2(x2 x4)	x4x2x1x3-x3x1x2x4-x2x4x1x3-x1x3x2x4
		0x396c9cc9	// 11: 2(x1 x4) 2(x2 x3)	x4x1x2x3-x3x2x1x4-x2x3x1x4-x1x4x2x3
	};

	SEARCH_STATS(++statistics.n_search_solve_4);
	SEARCH_UPDATE_INTERNAL_NODES(search->n_nodes);

	// stability cutoff (try 12%, cut 7%)
	player = search->board.player;
	opponent = search->board.opponent;
	if (search_SC_NWS_4(player, opponent, alpha, &score)) return score;

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
	shuf4 = sort4_shuf[paritysort];
	empties_series = (x1 << 24) | (x2 << 16) | (x3 << 8) | x4;

	bestscore = SCORE_INF;	// min stage
	pol = 1;
	do {
		// best move alphabeta search
		x1 = (empties_series >> ((shuf4 >> (6 - 3)) & 0x18)) & 0xFF;
		if ((NEIGHBOUR[x1] & opponent) && (flipped = Flip(x1, player, opponent))) {	// (76%/77%)
			next_player = opponent ^ flipped;
			next_opponent = player ^ (flipped | x_to_bit(x1));
			bestscore = solve_3(next_player, next_opponent, alpha, shuf4, empties_series, &search->n_nodes);
			if (bestscore <= alpha) return bestscore * pol;	// (68%)
		}

		x2 = (empties_series >> ((shuf4 >> (14 - 3)) & 0x18)) & 0xFF;
		if ((NEIGHBOUR[x2] & opponent) && (flipped = Flip(x2, player, opponent))) {	// (87%/84%)
			next_player = opponent ^ flipped;
			next_opponent = player ^ (flipped | x_to_bit(x2));
			score = solve_3(next_player, next_opponent, alpha, shuf4 >> 8, empties_series, &search->n_nodes);
			if (score <= alpha) return score * pol;	// (37%)
			else if (score < bestscore) bestscore = score;
		}

		x3 = (empties_series >> ((shuf4 >> (22 - 3)) & 0x18)) & 0xFF;
		if ((NEIGHBOUR[x3] & opponent) && (flipped = Flip(x3, player, opponent))) {	// (77%/80%)
			next_player = opponent ^ flipped;
			next_opponent = player ^ (flipped | x_to_bit(x3));
			score = solve_3(next_player, next_opponent, alpha, shuf4 >> 16, empties_series, &search->n_nodes);
			if (score <= alpha) return score * pol;	// (14%)
			else if (score < bestscore) bestscore = score;
		}

		x4 = (empties_series >> ((shuf4 >> 30) * 8)) & 0xFF;
		if ((NEIGHBOUR[x4] & opponent) && (flipped = Flip(x4, player, opponent))) {	// (79%/88%)
			next_player = opponent ^ flipped;
			next_opponent = player ^ (flipped | x_to_bit(x4));
			score = solve_3(next_player, next_opponent, alpha, shuf4 >> 24, empties_series, &search->n_nodes);
			if (score < bestscore) bestscore = score;
			return bestscore * pol;	// (37%)
		}

		if (bestscore < SCORE_INF)	// (72%)
			return bestscore * pol;	// (13%)

		next_opponent = player; player = opponent; opponent = next_opponent;	// pass
		alpha = ~alpha;	// = -(alpha + 1)
	} while ((pol = -pol) < 0);

	return board_solve(opponent, 4);	// gameover
}
#endif

/**
 * @brief  Evaluate a position using a shallow NWS. (5..6 empties)
 *
 * This function is used when there are few empty squares on the board. Here,
 * optimizations are in favour of speed instead of efficiency.
 * Move ordering is constricted to the hole parity and the type of squares.
 * No hashtable are used and anticipated cut-off is limited to stability cut-off.
 *
 * @param search Search. (breaks board and parity; caller has a copy)
 * @param alpha Alpha bound.
 * @return The final score, as a disc difference.
 */
static int search_shallow(Search *search, const int alpha, bool pass1)
{
	unsigned long long moves, prioritymoves;
	int x, prev, score, bestscore;
	// const int beta = alpha + 1;
	V2DI board0;
	unsigned int parity0;

	assert(SCORE_MIN <= alpha && alpha <= SCORE_MAX);
	assert(0 <= search->eval.n_empties && search->eval.n_empties <= DEPTH_TO_SHALLOW_SEARCH);

	SEARCH_STATS(++statistics.n_NWS_shallow);
	SEARCH_UPDATE_INTERNAL_NODES(search->n_nodes);

	// stability cutoff (try 8%, cut 7%)
	if (search_SC_NWS(search, alpha, &score)) return score;

	board0.board = search->board;
	moves = vboard_get_moves(board0);
	if (moves == 0) {	// pass (2%)
		if (pass1)	// gameover (1%)
			return search_solve(search);

		search_pass(search);
		bestscore = -search_shallow(search, ~alpha, true);
		// search_pass(search);
		return bestscore;
	}

	bestscore = -SCORE_INF;
	parity0 = search->eval.parity;
	prioritymoves = moves & quadrant_mask[parity0];
	if (prioritymoves == 0)	// all even
		prioritymoves = moves;

	if (search->eval.n_empties == 5)	// transfer to search_solve_n, no longer uses n_empties, parity (53%)
		do {
			moves ^= prioritymoves;
			x = NOMOVE;
			do {
				do {
					x = search->empties[prev = x].next;
				} while (!(prioritymoves & x_to_bit(x)));	// (58%)

				prioritymoves &= ~x_to_bit(x);
				search->empties[prev].next = search->empties[x].next;	// remove - maintain single link only
				vboard_next(board0, x, &search->board);
				score = search_solve_4(search, alpha);
				search->empties[prev].next = x;	// restore

				if (score > alpha)	// (49%)
					return score;
				else if (score > bestscore)
					bestscore = score;
			} while (prioritymoves);	// (34%)
		} while ((prioritymoves = moves));	// (38%)

	else {
		--search->eval.n_empties;	// for next depth
		do {
			moves ^= prioritymoves;
			x = NOMOVE;
			do {
				do {
					x = search->empties[prev = x].next;
				} while (!(prioritymoves & x_to_bit(x)));	// (57%)

				prioritymoves &= ~x_to_bit(x);
				search->eval.parity = parity0 ^ QUADRANT_ID[x];
				search->empties[prev].next = search->empties[x].next;	// remove - maintain single link only
				vboard_next(board0, x, &search->board);
				score = -search_shallow(search, ~alpha, false);
				search->empties[prev].next = x;	// restore

				if (score > alpha) {	// (40%)
					// search->board = board0.board;
					// search->eval.parity = parity0;
					++search->eval.n_empties;
					return score;

				} else if (score > bestscore)
					bestscore = score;
			} while (prioritymoves);	// (54%)
		} while ((prioritymoves = moves));	// (23%)
		++search->eval.n_empties;
	}
	// search->board = board0.board;	// restore in caller
	// search->eval.parity = parity0;

 	assert(SCORE_MIN <= bestscore && bestscore <= SCORE_MAX);
	return bestscore;	// (33%)
}

/**
 * @brief Evaluate an endgame position with a Null Window Search algorithm,
 * with thread-local lockfree hash. (7..10 empties)

 * Lightweight transposition table with thread local (thus lockfree) 1-way hash
 * is used for 7..10 empties (and occasionally less from PVS).
 * http://www.amy.hi-ho.ne.jp/okuhara/edaxopt.htm#localhash
 *
 * @param search Search.
 * @param alpha Alpha bound.
 * @return The final score, as a disc difference.
 */

static int NWS_endgame_local(Search *search, const int alpha)
{
	int score, ofssolid, bestmove, bestscore, lower, upper;
	unsigned long long solid_opp;
	// const int beta = alpha + 1;
	Hash *hash_entry;
	Move *move;
	// long long nodes_org;
	V2DI board0, hashboard;
	unsigned int parity0;
	unsigned long long full[5];
	MoveList movelist;

	assert(bit_count(~(search->board.player|search->board.opponent)) < DEPTH_TO_USE_LOCAL_HASH);
	assert(SCORE_MIN <= alpha && alpha <= SCORE_MAX);

	SEARCH_STATS(++statistics.n_NWS_endgame);
	SEARCH_UPDATE_INTERNAL_NODES(search->n_nodes);

	// stability cutoff
	board0.board = hashboard.board = search->board;
	ofssolid = 0;
	// search_SC_NWS(search, alpha, &score)
	if (USE_SC && alpha >= NWS_STABILITY_THRESHOLD[search->eval.n_empties]) {	// (7%)
		CUTOFF_STATS(++statistics.n_stability_try;)
		score = SCORE_MAX - 2 * get_stability_fulls(search->board.opponent, search->board.player, full);
		if (score <= alpha) {	// (3%)
			CUTOFF_STATS(++statistics.n_stability_low_cutoff;)
			return score;
		}

		// Improvement of Serch by Reducing Redundant Information in a Position of Othello
		// Hidekazu Matsuo, Shuji Narazaki
		// http://id.nii.ac.jp/1001/00156359/
		solid_opp = full[4] & hashboard.board.opponent;	// full[4] = all full
  #ifndef POPCOUNT
		if (solid_opp)	// (72%)
  #endif
		{
  #ifdef hasSSE2
			hashboard.v2 = _mm_xor_si128(hashboard.v2, _mm_set1_epi64x(solid_opp));
  #else
			hashboard.board.player ^= solid_opp;	// normalize solid to player
			hashboard.board.opponent ^= solid_opp;
  #endif
			ofssolid = bit_count(solid_opp) * 2;	// hash score is ofssolid grater than real
		}
	}

	hash_entry = search->thread_hash.hash + (board_get_hash_code(&hashboard.board) & search->thread_hash.hash_mask);
	// PREFETCH(hash_entry);

	search_get_movelist(search, &movelist);

	if (movelist.n_moves > 0) {
		// transposition cutoff
		// hash_get(&search->thread_hash, &hashboard.board, hash_code, &hash_data.data)
		unsigned char hashmove[2] = { NOMOVE, NOMOVE };
		if (vboard_equal(hashboard, &hash_entry->board)) {	// (6%)
			hashmove[0] = hash_entry->data.move[0];
			lower = hash_entry->data.lower - ofssolid;
			upper = hash_entry->data.upper - ofssolid;
			// search_TC_NWS(&hash_data.data, search->eval.n_empties, NO_SELECTIVITY, alpha, &score)
			if (USE_TC /* && (data->wl.c.selectivity >= NO_SELECTIVITY && data->wl.c.depth >= search->eval.n_empties) */) {
				CUTOFF_STATS(++statistics.n_hash_try;)
				if (alpha < lower) {
					CUTOFF_STATS(++statistics.n_hash_high_cutoff;)
					return lower;
				}
				if (alpha >= upper) {
					CUTOFF_STATS(++statistics.n_hash_low_cutoff;)
					return upper;
				}
			}
		}
		if (movelist.n_moves > 1)
			movelist_evaluate_fast(&movelist, search, hashmove);

		// nodes_org = search->n_nodes;
		parity0 = search->eval.parity;
		bestscore = -SCORE_INF;
		--search->eval.n_empties;	// for next move
		// loop over all moves
		move = &movelist.move[0];
		while ((move = move_next_best(move))) {	// (76%)
			search->eval.parity = parity0 ^ QUADRANT_ID[move->x];
			vboard_update(&search->board, board0, move);
			if (search->eval.n_empties <= DEPTH_TO_SHALLOW_SEARCH) {
				search->empties[search->empties[move->x].previous].next = search->empties[move->x].next;	// remove - maintain single link only
				score = -search_shallow(search, ~alpha, false);
				search->empties[search->empties[move->x].previous].next = move->x;	// restore
			} else {
				empty_remove(search->empties, move->x);
				score = -NWS_endgame_local(search, ~alpha);
				empty_restore(search->empties, move->x);
			}
			search->board = board0.board;

			if (score > bestscore) {	// (63%)
				bestscore = score;
				bestmove = move->x;
				if (bestscore > alpha) break;	// (39%)
			}
		}
		++search->eval.n_empties;
		search->eval.parity = parity0;

		if (search->stop)	// (1%)
			return alpha;

		hash_store_local(hash_entry, hashboard, alpha + ofssolid, alpha + ofssolid + 1, bestscore + ofssolid, bestmove);

	} else {	// (1%)
		if (can_move(search->board.opponent, search->board.player)) { // pass
			search_pass(search);
			bestscore = -NWS_endgame_local(search, ~alpha);
			search_pass(search);
		} else  { // game over
			bestscore = search_solve(search);
		}
	}

	if (SQUARE_STATS(1) + 0) {
		foreach_move(move, movelist)
			++statistics.n_played_square[search->eval.n_empties][SQUARE_TYPE[move->x]];
		if (bestscore > alpha)
			++statistics.n_good_square[search->eval.n_empties][SQUARE_TYPE[bestscore]];
	}
 	assert(SCORE_MIN <= bestscore && bestscore <= SCORE_MAX);
 	assert((bestscore & 1) == 0);
	return bestscore;
}

/**
 * @brief Evaluate an endgame position with a Null Window Search algorithm. (11..15 empties)
 *
 * This function is used when there are still many empty squares on the board. Move
 * ordering, hash table cutoff, enhanced transposition cutoff, etc. are used in
 * order to diminish the size of the tree to analyse, but at the expense of a
 * slower speed.
 *
 * @param search Search.
 * @param alpha Alpha bound.
 * @return The final score, as a disc difference.
 */
int NWS_endgame(Search *search, const int alpha)
{
	int score, bestscore;
	unsigned long long hash_code;
	// const int beta = alpha + 1;
	HashStoreData hash_data;
	Move *move;
	long long nodes_org;
	V2DI board0;
	unsigned int parity0;
	MoveList movelist;

	assert(bit_count(~(search->board.player|search->board.opponent)) < DEPTH_MIDGAME_TO_ENDGAME);
	assert(SCORE_MIN <= alpha && alpha <= SCORE_MAX);

	if (search->stop) return alpha;

	if (search->eval.n_empties <= DEPTH_TO_USE_LOCAL_HASH)
		return NWS_endgame_local(search, alpha);

	SEARCH_STATS(++statistics.n_NWS_endgame);
	SEARCH_UPDATE_INTERNAL_NODES(search->n_nodes);

	// stability cutoff
	if (search_SC_NWS(search, alpha, &score)) return score;

	hash_code = board_get_hash_code(&search->board);
	hash_prefetch(&search->hash_table, hash_code);

	search_get_movelist(search, &movelist);
	board0.board = search->board;

	if (movelist.n_moves > 0) {	// (96%)
		// transposition cutoff
		if (hash_get(&search->hash_table, &search->board, hash_code, &hash_data.data)) {	// (6%)
			if (search_TC_NWS(&hash_data.data, search->eval.n_empties, NO_SELECTIVITY, alpha, &score))	// (6%)
				return score;
		}
		if (movelist.n_moves > 1)
			movelist_evaluate_fast(&movelist, search, hash_data.data.move);

		nodes_org = search->n_nodes;
		parity0 = search->eval.parity;
		bestscore = -SCORE_INF;
		// loop over all moves
		move = &movelist.move[0];
		--search->eval.n_empties;	// for next move
		while ((move = move_next_best(move))) {	// (76%)
			search->eval.parity = parity0 ^ QUADRANT_ID[move->x];
			empty_remove(search->empties, move->x);
			vboard_update(&search->board, board0, move);
			score = -NWS_endgame(search, ~alpha);
			empty_restore(search->empties, move->x);
			search->board = board0.board;

			if (score > bestscore) {	// (63%)
				bestscore = score;
				hash_data.data.move[0] = move->x;
				if (bestscore > alpha) break;	// (39%)
			}
		}
		++search->eval.n_empties;
		search->eval.parity = parity0;

		if (search->stop)	// (1%)
			return alpha;

		hash_data.data.wl.c.depth = search->eval.n_empties;
		hash_data.data.wl.c.selectivity = NO_SELECTIVITY;
		hash_data.data.wl.c.cost = last_bit(search->n_nodes - nodes_org);
		// hash_data.data.move[0] = bestmove;
		hash_data.alpha = alpha;
		hash_data.beta = alpha + 1;
		hash_data.score = bestscore;
		hash_store(&search->hash_table, &search->board, hash_code, &hash_data);

	} else {	// (1%)
		if (can_move(search->board.opponent, search->board.player)) { // pass
			search_pass(search);
			bestscore = -NWS_endgame(search, ~alpha);
			search_pass(search);
		} else  { // game over
			bestscore = search_solve(search);
		}
	}

	if (SQUARE_STATS(1) + 0) {
		foreach_move(move, movelist)
			++statistics.n_played_square[search->eval.n_empties][SQUARE_TYPE[move->x]];
		if (bestscore > alpha)
			++statistics.n_good_square[search->eval.n_empties][SQUARE_TYPE[bestscore]];
	}
 	assert(SCORE_MIN <= bestscore && bestscore <= SCORE_MAX);
 	assert((bestscore & 1) == 0);
	return bestscore;
}
