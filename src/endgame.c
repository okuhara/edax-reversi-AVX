/**
 * @file endgame.c
 *
 * Search near the end of the game.
 *
 * @date 1998 - 2022
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

/**
 * @brief Get the final score.
 *
 * Get the final score, when no move can be made.
 *
 * @param player Board.player
 * @param n_empties Number of empty squares remaining on the board.
 * @return The final score, as a disc difference.
 */
static int board_solve(const unsigned long long player, const int n_empties)
{
	int score = bit_count(player) * 2 - SCORE_MAX;	// in case of opponents win
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

#if ((MOVE_GENERATOR == MOVE_GENERATOR_AVX) || (MOVE_GENERATOR == MOVE_GENERATOR_SSE)) && (LAST_FLIP_COUNTER == COUNT_LAST_FLIP_SSE)
	#include "endgame_sse.c"	// vectorcall version
#elif (MOVE_GENERATOR == MOVE_GENERATOR_NEON) && (LAST_FLIP_COUNTER == COUNT_LAST_FLIP_SSE)
	#include "endgame_neon.c"
#else
/**
 * @brief Compute a board resulting of a move played on a previous board.
 *
 * @param board board to play the move on.
 * @param x move to play.
 * @param flipped flipped returned from Flip.
 * @param next resulting board.
 * @return pointer to next.
 */
static inline void board_flip_next(Board *board, int x, unsigned long long flipped, Board *next)
{
	next->player = board->opponent ^ flipped;
	next->opponent = board->player ^ (flipped | x_to_bit(x));
}

/**
 * @brief Get the final score.
 *
 * Get the final score, when 1 empty square remain.
 * The following code has been adapted from Zebra by Gunnar Anderson.
 *
 * @param player Board.player to evaluate.
 * @param beta   Beta bound.
 * @param x      Last empty square to play.
 * @return       The final opponent score, as a disc difference.
 */
int board_score_1(const unsigned long long player, const int beta, const int x)
{
	int score, score2, n_flips;

	score = SCORE_MAX - 2 - 2 * bit_count(player);	// = 2 * bit_count(opponent) - SCORE_MAX

	n_flips = last_flip(x, player);
	score -= n_flips;

	if (n_flips == 0) {
		score2 = score + 2;	// empty for player
		if (score >= 0)
			score = score2;
		if (score < beta) {	// lazy cut-off
			if ((n_flips = last_flip(x, ~player)) != 0)
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
 * @param player Board.player to evaluate.
 * @param opponent Board.opponent to evaluate.
 * @param alpha Alpha bound.
 * @param x1 First empty square coordinate.
 * @param x2 Second empty square coordinate.
 * @param n_nodes Node counter.
 * @return The final score, as a disc difference.
 */
static int board_solve_2(unsigned long long player, unsigned long long opponent, int alpha, int x1, int x2, volatile unsigned long long *n_nodes)
{
	unsigned long long flipped;
	int score, bestscore, nodes;
	// const int beta = alpha + 1;

	SEARCH_STATS(++statistics.n_board_solve_2);

	if ((NEIGHBOUR[x1] & opponent) && (flipped = Flip(x1, player, opponent))) {
		bestscore = board_score_1(opponent ^ flipped, alpha + 1, x2);

		if ((bestscore <= alpha) && (NEIGHBOUR[x2] & opponent) && (flipped = Flip(x2, player, opponent))) {
			score = board_score_1(opponent ^ flipped, alpha + 1, x1);
			if (score > bestscore) bestscore = score;
			nodes = 3;
		} else	nodes = 2;

	} else if ((NEIGHBOUR[x2] & opponent) && (flipped = Flip(x2, player, opponent))) {
		bestscore = board_score_1(opponent ^ flipped, alpha + 1, x1);
		nodes = 2;

	} else {	// pass
		if ((NEIGHBOUR[x1] & player) && (flipped = Flip(x1, opponent, player))) {
			bestscore = -board_score_1(player ^ flipped, -alpha, x2);

			if ((bestscore > alpha) && (NEIGHBOUR[x2] & player) && (flipped = Flip(x2, opponent, player))) {
				score = -board_score_1(player ^ flipped, -alpha, x1);
				if (score < bestscore) bestscore = score;
				nodes = 3;
			} else	nodes = 2;

		} else if ((NEIGHBOUR[x2] & player) && (flipped = Flip(x2, opponent, player))) {
			bestscore = -board_score_1(player ^ flipped, -alpha, x1);
			nodes = 2;

		} else {	// gameover
			bestscore = board_solve(player, 2);
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
 * @param board The board to evaluate.
 * @param alpha Alpha bound.
 * @param sort3 Parity flags.
 * @param x1 First empty square coordinate.
 * @param x2 Second empty square coordinate.
 * @param x3 Third empty square coordinate.
 * @param n_nodes Node counter.
 * @return The final score, as a disc difference.
 */
static int search_solve_3(Board *board, int alpha, int sort3, int x1, int x2, int x3, volatile unsigned long long *n_nodes)
{
	unsigned long long flipped, next_player, next_opponent;
	int score, bestscore, tmp;
	// const int beta = alpha + 1;

	SEARCH_STATS(++statistics.n_search_solve_3);
	SEARCH_UPDATE_INTERNAL_NODES(*n_nodes);

	// parity based move sorting
	switch (sort3 & 0x03) {
		case 1:
			tmp = x1; x1 = x2; x2 = tmp;	// case 1(x2) 2(x1 x3)
			break;
		case 2:
			tmp = x1; x1 = x3; x3 = x2; x2 = tmp;	// case 1(x3) 2(x1 x2)
			break;
		case 3:
			tmp = x2; x2 = x3; x3 = tmp;
			break;
	}

	// best move alphabeta search
	bestscore = -SCORE_INF;
	if ((NEIGHBOUR[x1] & board->opponent) && (flipped = board_flip(board, x1))) {
		next_player = board->opponent ^ flipped;
		next_opponent = board->player ^ (flipped | x_to_bit(x1));
		bestscore = -board_solve_2(next_player, next_opponent, -(alpha + 1), x2, x3, n_nodes);
		if (bestscore > alpha) return bestscore;
	}

	if ((NEIGHBOUR[x2] & board->opponent) && (flipped = board_flip(board, x2))) {
		next_player = board->opponent ^ flipped;
		next_opponent = board->player ^ (flipped | x_to_bit(x2));
		score = -board_solve_2(next_player, next_opponent, -(alpha + 1), x1, x3, n_nodes);
		if (score > alpha) return score;
		else if (score > bestscore) bestscore = score;
	}

	if ((NEIGHBOUR[x3] & board->opponent) && (flipped = board_flip(board, x3))) {
		next_player = board->opponent ^ flipped;
		next_opponent = board->player ^ (flipped | x_to_bit(x3));
		score = -board_solve_2(next_player, next_opponent, -(alpha + 1), x1, x2, n_nodes);
		if (score > bestscore) bestscore = score;
	}

	// pass ?
	else if (bestscore == -SCORE_INF) {
		// best move alphabeta search
		bestscore = SCORE_INF;
		if ((NEIGHBOUR[x1] & board->player) && (flipped = Flip(x1, board->opponent, board->player))) {
			next_player = board->player ^ flipped;
			next_opponent = board->opponent ^ (flipped | x_to_bit(x1));
			bestscore = board_solve_2(next_player, next_opponent, alpha, x2, x3, n_nodes);
			if (bestscore <= alpha) return bestscore;
		}

		if ((NEIGHBOUR[x2] & board->player) && (flipped = Flip(x2, board->opponent, board->player))) {
			next_player = board->player ^ flipped;
			next_opponent = board->opponent ^ (flipped | x_to_bit(x2));
			score = board_solve_2(next_player, next_opponent, alpha, x1, x3,  n_nodes);
			if (score <= alpha) return score;
			else if (score < bestscore) bestscore = score;
		}

		if ((NEIGHBOUR[x3] & board->player) && (flipped = Flip(x3, board->opponent, board->player))) {
			next_player = board->player ^ flipped;
			next_opponent = board->opponent ^ (flipped | x_to_bit(x3));
			score = board_solve_2(next_player, next_opponent, alpha, x1, x2, n_nodes);
			if (score < bestscore) bestscore = score;
		}

		else if (bestscore == SCORE_INF)	// gameover
			bestscore = board_solve(board->player, 3);
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
	unsigned long long flipped;
	int x1, x2, x3, x4, tmp, paritysort, score, bestscore;
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
	// Only the 1 1 2 case needs move sorting on this ply.
	paritysort = parity_case[((x3 ^ x4) & 0x24) + ((((x2 ^ x4) & 0x24) * 2 + ((x1 ^ x4) & 0x24)) >> 2)];
	switch (paritysort) {
		case 4: // case 1(x1) 1(x3) 2(x2 x4)
			tmp = x2; x2 = x3; x3 = tmp;
			break;
		case 5: // case 1(x1) 1(x4) 2(x2 x3)
			tmp = x2; x2 = x4; x4 = x3; x3 = tmp;
			break;
		case 6:	// case 1(x2) 1(x3) 2(x1 x4)
			tmp = x1; x1 = x2; x2 = x3; x3 = tmp;
			break;
		case 7: // case 1(x2) 1(x4) 2(x1 x3)
			tmp = x1; x1 = x2; x2 = x4; x4 = x3; x3 = tmp;
			break;
		case 8:	// case 1(x3) 1(x4) 2(x1 x2)
			tmp = x1; x1 = x3; x3 = tmp; tmp = x2; x2 = x4; x4 = tmp;
			break;
	}
	sort3 = sort3_shuf[paritysort];

	// best move alphabeta search
	bestscore = -SCORE_INF;
	if ((NEIGHBOUR[x1] & search->board.opponent) && (flipped = board_flip(&search->board, x1))) {
		board_flip_next(&search->board, x1, flipped, &next);
		bestscore = -search_solve_3(&next, -(alpha + 1), sort3, x2, x3, x4, &search->n_nodes);
		if (bestscore > alpha) return bestscore;
	}

	if ((NEIGHBOUR[x2] & search->board.opponent) && (flipped = board_flip(&search->board, x2))) {
		board_flip_next(&search->board, x2, flipped, &next);
		score = -search_solve_3(&next, -(alpha + 1), sort3 >> 4, x1, x3, x4, &search->n_nodes);
		if (score > alpha) return score;
		else if (score > bestscore) bestscore = score;
	}

	if ((NEIGHBOUR[x3] & search->board.opponent) && (flipped = board_flip(&search->board, x3))) {
		board_flip_next(&search->board, x3, flipped, &next);
		score = -search_solve_3(&next, -(alpha + 1), sort3 >> 8, x1, x2, x4, &search->n_nodes);
		if (score > alpha) return score;
		else if (score > bestscore) bestscore = score;
	}

	if ((NEIGHBOUR[x4] & search->board.opponent) && (flipped = board_flip(&search->board, x4))) {
		board_flip_next(&search->board, x4, flipped, &next);
		score = -search_solve_3(&next, -(alpha + 1), sort3 >> 12, x1, x2, x3, &search->n_nodes);
		if (score > bestscore) bestscore = score;
	}

	else if (bestscore == -SCORE_INF) {	// no move
		if (can_move(search->board.opponent, search->board.player)) { // pass
			search_pass_endgame(search);
			bestscore = -search_solve_4(search, -(alpha + 1));
			search_pass_endgame(search);
		} else { // gameover
			bestscore = board_solve(search->board.player, 4);
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
	int x, prev, score, bestscore = -SCORE_INF;
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
			for (x = search->empties[prev = NOMOVE].next; x != NOMOVE; x = search->empties[prev = x].next) {	// maintain single link only
				if (paritymask & QUADRANT_ID[x]) {
					if ((NEIGHBOUR[x] & board0.opponent) && (flipped = board_flip(&board0, x))) {
						search->eval.parity = parity0 ^ QUADRANT_ID[x];
						search->empties[prev].next = search->empties[x].next;	// remove
						search->board.player = board0.opponent ^ flipped;
						search->board.opponent = board0.player ^ (flipped | x_to_bit(x));
						board_check(&search->board);

						if (search->eval.n_empties == 4)
							score = -search_solve_4(search, -(alpha + 1));
						else	score = -search_shallow(search, -(alpha + 1));

						search->empties[prev].next = x;	// restore

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
	int score, ofssolid, bestscore;
	HashTable *const hash_table = &search->hash_table;
	unsigned long long hash_code, allfull, solid_opp;
	// const int beta = alpha + 1;
	HashData hash_data;
	HashStoreData hash_store_data;
	MoveList movelist;
	Move *move;
	long long nodes_org;
	Board board0, hashboard;
	unsigned int parity0;
	V4DI full;

	if (search->stop) return alpha;

	assert(search->eval.n_empties == bit_count(~(search->board.player|search->board.opponent)));
	assert(SCORE_MIN <= alpha && alpha <= SCORE_MAX);

	SEARCH_STATS(++statistics.n_NWS_endgame);

	if (search->eval.n_empties <= DEPTH_TO_SHALLOW_SEARCH) return search_shallow(search, alpha);

	SEARCH_UPDATE_INTERNAL_NODES(search->n_nodes);

	// Improvement of Serch by Reducing Redundant Information in a Position of Othello
	// Hidekazu Matsuo, Shuji Narazaki
	// http://id.nii.ac.jp/1001/00156359/
	// (1-2% improvement)
	if (search->eval.n_empties <= MASK_SOLID_DEPTH) {
		allfull = get_all_full_lines(search->board.player | search->board.opponent, &full);

		// stability cutoff
		if (search_SC_NWS_fulls_given(search, alpha, &score, allfull, &full))
			return score;

		solid_opp = allfull & search->board.opponent;
		hashboard.player = search->board.player ^ solid_opp;	// normalize solid to player
		hashboard.opponent = search->board.opponent ^ solid_opp;
		ofssolid = bit_count(solid_opp) * 2;	// hash score is ofssolid grater than real

	} else {
		// stability cutoff
		if (search_SC_NWS(search, alpha, &score))
			return score;

		hashboard = search->board;
		ofssolid = 0;
	}

	// transposition cutoff

	hash_code = board_get_hash_code(&hashboard);
	if (hash_get(hash_table, &hashboard, hash_code, &hash_data)) {
		hash_data.lower -= ofssolid;
		hash_data.upper -= ofssolid;
		if (search_TC_NWS(&hash_data, search->eval.n_empties, NO_SELECTIVITY, alpha, &score))
			return score;
	}
	// else if (ofssolid)	// slows down
	//	hash_get(hash_table, &search->board, board_get_hash_code(&search->board), &hash_data);

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
		if (movelist.n_moves > 1)
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
		hash_store_data.alpha = alpha + ofssolid;
		hash_store_data.beta = alpha + ofssolid + 1;
		hash_store_data.score = bestscore + ofssolid;
		hash_store(hash_table, &hashboard, hash_code, &hash_store_data);

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

	return alpha;
}
