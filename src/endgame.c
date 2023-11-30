/**
 * @file endgame.c
 *
 * Search near the end of the game.
 *
 * @date 1998 - 2023
 * @author Richard Delorme
 * @version 4.4
 */


#include "search.h"

#include "bit.h"
#include "settings.h"
#include "stats.h"
#include "ybwc.h"

#include <assert.h>

#if LAST_FLIP_COUNTER == COUNT_LAST_FLIP_CARRY
	#include "count_last_flip_carry_64.c"
#elif LAST_FLIP_COUNTER == COUNT_LAST_FLIP_SSE
	#ifdef hasSSE2
		#include "count_last_flip_sse.c"
	#else
		#include "count_last_flip_neon.c"
	#endif
#elif LAST_FLIP_COUNTER == COUNT_LAST_FLIP_BITSCAN
	#include "count_last_flip_bitscan.c"
#elif LAST_FLIP_COUNTER == COUNT_LAST_FLIP_PLAIN
	#include "count_last_flip_plain.c"
#elif LAST_FLIP_COUNTER == COUNT_LAST_FLIP_32
	#include "count_last_flip_32.c"
#elif LAST_FLIP_COUNTER == COUNT_LAST_FLIP_BMI2
	#include "count_last_flip_bmi2.c"
#else // LAST_FLIP_COUNTER == COUNT_LAST_FLIP_KINDERGARTEN
	#include "count_last_flip_kindergarten.c"
#endif

#if ((MOVE_GENERATOR == MOVE_GENERATOR_AVX) || (MOVE_GENERATOR == MOVE_GENERATOR_SSE)) && (LAST_FLIP_COUNTER == COUNT_LAST_FLIP_SSE)
	#include "endgame_sse.c"	// vectorcall version
#elif (MOVE_GENERATOR == MOVE_GENERATOR_NEON) && (LAST_FLIP_COUNTER == COUNT_LAST_FLIP_SSE)
	#include "endgame_neon.c"
#endif

/**
 * @brief Get the final score.
 *
 * Get the final score, when no move can be made.
 *
 * @param board Board.
 * @param n_empties Number of empty squares remaining on the board.
 * @return The final score, as a disc difference.
 */
static int board_solve(const Board *board, const int n_empties)
{
	int score = bit_count(board->player) * 2 - SCORE_MAX;	// in case of opponents win
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
 * Get the final score, when no move can be made.
 *
 * @param search Search.
 * @return The final score, as a disc difference.
 */
int search_solve(const Search *search)
{
	return board_solve(&search->board, search->eval.n_empties);
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

#if ((MOVE_GENERATOR != MOVE_GENERATOR_AVX) && (MOVE_GENERATOR != MOVE_GENERATOR_SSE) && (MOVE_GENERATOR != MOVE_GENERATOR_NEON)) || (LAST_FLIP_COUNTER != COUNT_LAST_FLIP_SSE)
/**
 * @brief Get the final score.
 *
 * Get the final score, when 1 empty squares remain.
 * The following code has been adapted from Zebra by Gunnar Anderson.
 *
 * @param board  Board to evaluate.
 * @param beta   Beta bound.
 * @param x      Last empty square to play.
 * @return       The final opponent score, as a disc difference.
 */
int board_score_1(const Board *board, const int beta, const int x)
{
	int score, score2, n_flips;

	score = 2 * bit_count(board->opponent) - SCORE_MAX;

	n_flips = last_flip(x, board->player);
	score -= n_flips;

	if (n_flips == 0) {
		score2 = score + 2;	// empty for player
		if (score >= 0)
			score = score2;
		if (score < beta) {	// lazy cut-off
			if ((n_flips = last_flip(x, board->opponent)) != 0)
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
 * @param board The board to evaluate.
 * @param alpha Alpha bound.
 * @param x1 First empty square coordinate.
 * @param x2 Second empty square coordinate.
 * @param search Search.
 * @return The final score, as a disc difference.
 */
static int board_solve_2(Board *board, int alpha, const int x1, const int x2, volatile unsigned long long *n_nodes)
{
	Board next;
	int score, bestscore, nodes;
	// const int beta = alpha + 1;

	SEARCH_STATS(++statistics.n_board_solve_2);

	if ((NEIGHBOUR[x1] & board->opponent) && board_next(board, x1, &next)) {
		bestscore = board_score_1(&next, alpha + 1, x2);

		if ((bestscore <= alpha) && (NEIGHBOUR[x2] & board->opponent) && board_next(board, x2, &next)) {
			score = board_score_1(&next, alpha + 1, x1);
			if (score > bestscore) bestscore = score;
			nodes = 3;
		} else	nodes = 2;

	} else if ((NEIGHBOUR[x2] & board->opponent) && board_next(board, x2, &next)) {
		bestscore = board_score_1(&next, alpha + 1, x1);
		nodes = 2;

	} else {	// pass
		if ((NEIGHBOUR[x1] & board->player) && board_pass_next(board, x1, &next)) {
			bestscore = -board_score_1(&next, -alpha, x2);

			if ((bestscore > alpha) && (NEIGHBOUR[x2] & board->player) && board_pass_next(board, x2, &next)) {
				score = -board_score_1(&next, -alpha, x1);
				if (score < bestscore) bestscore = score;
				nodes = 3;
			} else	nodes = 2;

		} else if ((NEIGHBOUR[x2] & board->player) && board_pass_next(board, x2, &next)) {
			bestscore = -board_score_1(&next, -alpha, x1);
			nodes = 2;

		} else {	// gameover
			bestscore = board_solve(board, 2);
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
 * @param search Search.
 * @param alpha Alpha bound.
 * @return The final score, as a disc difference.
 */
static int search_solve_3(Search *search, const int alpha, Board *board, unsigned int parity)
{
	Board next;
	int x1 = search->empties[NOMOVE].next;
	int x2 = search->empties[x1].next;
	int x3 = search->empties[x2].next;
	int score, bestscore;
	// const int beta = alpha + 1;

	SEARCH_STATS(++statistics.n_search_solve_3);
	SEARCH_UPDATE_INTERNAL_NODES(search->n_nodes);

	// parity based move sorting
	if (!(parity & QUADRANT_ID[x1])) {
		if (parity & QUADRANT_ID[x2]) { // case 1(x2) 2(x1 x3)
			int tmp = x1; x1 = x2; x2 = tmp;
		} else { // case 1(x3) 2(x1 x2)
			int tmp = x1; x1 = x3; x3 = x2; x2 = tmp;
		}
	}

	// best move alphabeta search
	bestscore = -SCORE_INF;
	if ((NEIGHBOUR[x1] & board->opponent) && board_next(board, x1, &next)) {
		bestscore = -board_solve_2(&next, -(alpha + 1), x2, x3, &search->n_nodes);
		if (bestscore > alpha) return bestscore;
	}

	if ((NEIGHBOUR[x2] & board->opponent) && board_next(board, x2, &next)) {
		score = -board_solve_2(&next, -(alpha + 1), x1, x3, &search->n_nodes);
		if (score > alpha) return score;
		else if (score > bestscore) bestscore = score;
	}

	if ((NEIGHBOUR[x3] & board->opponent) && board_next(board, x3, &next)) {
		score = -board_solve_2(&next, -(alpha + 1), x1, x2, &search->n_nodes);
		if (score > bestscore) bestscore = score;
	}

	// pass ?
	else if (bestscore == -SCORE_INF) {
		// best move alphabeta search
		bestscore = SCORE_INF;
		if ((NEIGHBOUR[x1] & board->player) && board_pass_next(board, x1, &next)) {
			bestscore = board_solve_2(&next, alpha, x2, x3, &search->n_nodes);
			if (bestscore <= alpha) return bestscore;
		}

		if ((NEIGHBOUR[x2] & board->player) && board_pass_next(board, x2, &next)) {
			score = board_solve_2(&next, alpha, x1, x3, &search->n_nodes);
			if (score <= alpha) return score;
			else if (score < bestscore) bestscore = score;
		}

		if ((NEIGHBOUR[x3] & board->player) && board_pass_next(board, x3, &next)) {
			score = board_solve_2(&next, alpha, x1, x2, &search->n_nodes);
			if (score < bestscore) bestscore = score;
		}

		else if (bestscore == SCORE_INF)	// gameover
			bestscore = board_solve(board, 3);
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
static int search_solve_4(Search *search, const int alpha)
{
	Board next;
	int x1, x2, x3, x4;
	int score, bestscore;
	unsigned int parity;
	// const int beta = alpha + 1;

	SEARCH_STATS(++statistics.n_search_solve_4);
	SEARCH_UPDATE_INTERNAL_NODES(search->n_nodes);

	// stability cutoff
	if (search_SC_NWS(search, alpha, &score)) return score;

	x1 = search->empties[NOMOVE].next;
	x2 = search->empties[x1].next;
	x3 = search->empties[x2].next;
	x4 = search->empties[x3].next;

	// parity based move sorting.
	// The following hole sizes are possible:
	//    4 - 1 3 - 2 2 - 1 1 2 - 1 1 1 1
	// Only the 1 1 2 case needs move sorting.
	parity = search->eval.parity;
	if (!(parity & QUADRANT_ID[x1])) {
		if (parity & QUADRANT_ID[x2]) {
			if (parity & QUADRANT_ID[x3]) { // case 1(x2) 1(x3) 2(x1 x4)
				int tmp = x1; x1 = x2; x2 = x3; x3 = tmp;
			} else { // case 1(x2) 1(x4) 2(x1 x3)
				int tmp = x1; x1 = x2; x2 = x4; x4 = x3; x3 = tmp;
			}
		} else if (parity & QUADRANT_ID[x3]) { // case 1(x3) 1(x4) 2(x1 x2)
			int tmp = x1; x1 = x3; x3 = tmp; tmp = x2; x2 = x4; x4 = tmp;
		}
	} else {
		if (!(parity & QUADRANT_ID[x2])) {
			if (parity & QUADRANT_ID[x3]) { // case 1(x1) 1(x3) 2(x2 x4)
				int tmp = x2; x2 = x3; x3 = tmp;
			} else { // case 1(x1) 1(x4) 2(x2 x3)
				int tmp = x2; x2 = x4; x4 = x3; x3 = tmp;
			}
		}
	}

	// best move alphabeta search
	bestscore = -SCORE_INF;
	if ((NEIGHBOUR[x1] & search->board.opponent) && board_next(&search->board, x1, &next)) {
		empty_remove(search->empties, x1);
		bestscore = -search_solve_3(search, -(alpha + 1), &next, parity ^ QUADRANT_ID[x1]);
		empty_restore(search->empties, x1);
		if (bestscore > alpha) return bestscore;
	}

	if ((NEIGHBOUR[x2] & search->board.opponent) && board_next(&search->board, x2, &next)) {
		empty_remove(search->empties, x2);
		score = -search_solve_3(search, -(alpha + 1), &next, parity ^ QUADRANT_ID[x2]);
		empty_restore(search->empties, x2);
		if (score > alpha) return score;
		else if (score > bestscore) bestscore = score;
	}

	if ((NEIGHBOUR[x3] & search->board.opponent) && board_next(&search->board, x3, &next)) {
		empty_remove(search->empties, x3);
		score = -search_solve_3(search, -(alpha + 1), &next, parity ^ QUADRANT_ID[x3]);
		empty_restore(search->empties, x3);
		if (score > alpha) return score;
		else if (score > bestscore) bestscore = score;
	}

	if ((NEIGHBOUR[x4] & search->board.opponent) && board_next(&search->board, x4, &next)) {
		empty_remove(search->empties, x4);
		score = -search_solve_3(search, -(alpha + 1), &next, parity ^ QUADRANT_ID[x4]);
		empty_restore(search->empties, x4);
		if (score > bestscore) bestscore = score;
	}

	else if (bestscore == -SCORE_INF) {	// no move
		if (can_move(search->board.opponent, search->board.player)) { // pass
			search_pass_endgame(search);
			bestscore = -search_solve_4(search, -(alpha + 1));
			search_pass_endgame(search);
		} else { // gameover
			bestscore = board_solve(&search->board, 4);
		}
	}

 	assert(SCORE_MIN <= bestscore && bestscore <= SCORE_MAX);
	return bestscore;
}
#endif

/**
 * @brief  Evaluate a position using a shallow NWS.
 *
 * This function is used when there are few empty squares on the board. Here,
 * optimizations are in favour of speed instead of efficiency.
 * Move ordering is constricted to the hole parity and the type of squares.
 * No hashtable are used and anticipated cut-off is limited to stability cut-off.
 *
 * @param search Search.
 * @param alpha Alpha bound.
 * @return The final score, as a disc difference.
 */
static int search_shallow(Search *search, const int alpha)
{
	unsigned long long flipped;
	int x, score, bestscore = -SCORE_INF;
	// const int beta = alpha + 1;
	Board board0;
	unsigned int parity0, paritymask;

	assert(SCORE_MIN <= alpha && alpha <= SCORE_MAX);
	assert(0 <= search->eval.n_empties && search->eval.n_empties <= DEPTH_TO_SHALLOW_SEARCH);

	SEARCH_STATS(++statistics.n_NWS_shallow);
	SEARCH_UPDATE_INTERNAL_NODES(search->n_nodes);

	// stability cutoff
	if (search_SC_NWS(search, alpha, &score)) return score;

	board0 = search->board;
	paritymask = parity0 = search->eval.parity;
	--search->eval.n_empties;	// for next depth
	do {	// odd first, even second
		if (paritymask) {	// skip all even or all add
			foreach_empty (x, search->empties) {
				if (paritymask & QUADRANT_ID[x]) {
					if ((NEIGHBOUR[x] & board0.opponent) && (flipped = board_flip(&board0, x))) {
						search->eval.parity = parity0 ^ QUADRANT_ID[x];
						empty_remove(search->empties, x);
						search->board.player = board0.opponent ^ flipped;
						search->board.opponent = board0.player ^ (flipped | x_to_bit(x));
						board_check(&search->board);

						if (search->eval.n_empties == 4)
							score = -search_solve_4(search, -(alpha + 1));
						else	score = -search_shallow(search, -(alpha + 1));

						empty_restore(search->empties, x);

						if (score > alpha) {
							search->board = board0;
							search->eval.parity = parity0;
							++search->eval.n_empties;
							return score;
						} else if (score > bestscore)
							bestscore = score;
					}
				}
			}
		}
	} while ((paritymask ^= 15) != parity0);
	search->board = board0;
	search->eval.parity = parity0;
	++search->eval.n_empties;

	// no move
	if (bestscore == -SCORE_INF) {
		if (can_move(search->board.opponent, search->board.player)) { // pass
			search_pass_endgame(search);
			bestscore = -search_shallow(search, -(alpha + 1));
			search_pass_endgame(search);
		} else { // gameover
			bestscore = search_solve(search);
		}
	}

 	assert(SCORE_MIN <= bestscore && bestscore <= SCORE_MAX);
	return bestscore;
}

/**
 * @brief Evaluate an endgame position with a Null Window Search algorithm.
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
	HashTable *const hash_table = &search->hash_table;
	unsigned long long hash_code;
	// const int beta = alpha + 1;
	HashData hash_data;
	HashStoreData hash_store_data;
	MoveList movelist;
	Move *move;
	long long nodes_org;
	Board board0;
	unsigned int parity0;

	if (search->stop) return alpha;

	assert(search->eval.n_empties == bit_count(~(search->board.player|search->board.opponent)));
	assert(SCORE_MIN <= alpha && alpha <= SCORE_MAX);

	SEARCH_STATS(++statistics.n_NWS_endgame);

	if (search->eval.n_empties <= DEPTH_TO_SHALLOW_SEARCH) return search_shallow(search, alpha);

	SEARCH_UPDATE_INTERNAL_NODES(search->n_nodes);

	// stability cutoff
	if (search_SC_NWS(search, alpha, &score)) return score;

	// transposition cutoff
	hash_code = board_get_hash_code(&search->board);
	if (hash_get(hash_table, &search->board, hash_code, &hash_data) && search_TC_NWS(&hash_data, search->eval.n_empties, NO_SELECTIVITY, alpha, &score)) return score;

	search_get_movelist(search, &movelist);

	nodes_org = search->n_nodes;

	// special cases
	if (movelist_is_empty(&movelist)) {
		if (can_move(search->board.opponent, search->board.player)) { // pass
			search_pass_endgame(search);
			bestscore = -NWS_endgame(search, -(alpha + 1));
			search_pass_endgame(search);
			hash_store_data.data.move[0] = PASS;
		} else  { // game over
			bestscore = search_solve(search);
			hash_store_data.data.move[0] = NOMOVE;
		}
	} else {
		movelist_evaluate(&movelist, search, &hash_data, alpha, 0);

		board0 = search->board;
		parity0 = search->eval.parity;
		bestscore = -SCORE_INF;
		// loop over all moves
		foreach_best_move(move, movelist) {
			search_swap_parity(search, move->x);
			empty_remove(search->empties, move->x);
			board_update(&search->board, move);
			--search->eval.n_empties;

			score = -NWS_endgame(search, -(alpha + 1));

			search->eval.parity = parity0;
			empty_restore(search->empties, move->x);
			search->board = board0;
			++search->eval.n_empties;

			if (score > bestscore) {
				bestscore = score;
				hash_store_data.data.move[0] = move->x;
				if (bestscore > alpha) break;
			}
		}
	}

	if (!search->stop) {
		hash_store_data.data.wl.c.depth = search->eval.n_empties;
		hash_store_data.data.wl.c.selectivity = NO_SELECTIVITY;
		hash_store_data.data.wl.c.cost = last_bit(search->n_nodes - nodes_org);
		// hash_store_data.data.move[0] = bestmove;
		hash_store_data.alpha = alpha;
		hash_store_data.beta = alpha + 1;
		hash_store_data.score = bestscore;
		hash_store(hash_table, &search->board, hash_code, &hash_store_data);

		if (SQUARE_STATS(1) + 0) {
			foreach_move(move, movelist)
				++statistics.n_played_square[search->eval.n_empties][SQUARE_TYPE[move->x]];
			if (bestscore > alpha) ++statistics.n_good_square[search->eval.n_empties][SQUARE_TYPE[bestscore]];
		}
	 	assert(SCORE_MIN <= bestscore && bestscore <= SCORE_MAX);
	 	assert((bestscore & 1) == 0);
		return bestscore;
	}

	return alpha;
}
