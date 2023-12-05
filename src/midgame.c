/**
 * @file midgame.c
 *
 * Search near the end of the game.
 *
 * @date 1998 - 2023
 * @author Richard Delorme
 * @author Toshihiko Okuhara
 * @version 4.4
 */

#include "search.h"

#include "bit.h"
#include "options.h"
#include "stats.h"
#include "ybwc.h"
#include "settings.h"

#include <assert.h>
#include <stddef.h>
#include <math.h>

#ifndef RCD
/** macro RCD : set to 0.0 if -rcd is added to the icc compiler */
#define RCD 0.5
#endif

/**
 * @brief evaluate a midgame position with the evaluation function.
 *
 * @param ply	60 - n_empties
 * @param eval	Evauation function.
 */
static int accumlate_eval(int ply, Eval *eval)
{
	unsigned short *f = eval->feature.us;
	const Eval_weight *w;
	int sum;

	assert(ply < EVAL_N_PLY);
	ply -= 2;
	if (ply < 0)
		ply &= 1;
	w = &(*EVAL_WEIGHT)[ply];

#if defined(__AVX2__) && !defined(__bdver4__) && !defined(__znver1__) && !defined(__znver2__)
	enum {
		W_C9 = offsetof(Eval_weight, C9) / sizeof(short) - 1,	// -1 to load the data into hi-word
		W_C10 = offsetof(Eval_weight, C10) / sizeof(short) - 1,
		W_S100 = offsetof(Eval_weight, S100) / sizeof(short) - 1,
		W_S101 = offsetof(Eval_weight, S101) / sizeof(short) - 1
	};

	__m256i FF = _mm256_add_epi32(_mm256_cvtepu16_epi32(eval->feature.v8[0]),
		_mm256_set_epi32(W_C10, W_C10, W_C10, W_C10, W_C9, W_C9, W_C9, W_C9));
	__m256i DD = _mm256_i32gather_epi32((int *) w, FF, 2);
	__m256i SS = _mm256_srai_epi32(DD, 16);	// sign extend

	FF = _mm256_add_epi32(_mm256_cvtepu16_epi32(eval->feature.v8[1]),
		_mm256_set_epi32(W_S101, W_S101, W_S101, W_S101, W_S100, W_S100, W_S100, W_S100));
	DD = _mm256_i32gather_epi32((int *) w, FF, 2);
	SS = _mm256_add_epi32(SS, _mm256_srai_epi32(DD, 16));

	DD = _mm256_i32gather_epi32((int *)((short *) w->S8x4 - 1), _mm256_cvtepu16_epi32(eval->feature.v8[2]), 2);
	SS = _mm256_add_epi32(SS, _mm256_srai_epi32(DD, 16));

	DD = _mm256_i32gather_epi32((int *)((short *) w->S7654 - 1), _mm256_cvtepu16_epi32(*(__m128i *) &f[30]), 2);
	SS = _mm256_add_epi32(SS, _mm256_srai_epi32(DD, 16));

	DD = _mm256_i32gather_epi32((int *)((short *) w->S7654 - 1), _mm256_cvtepu16_epi32(*(__m128i *) &f[38]), 2);
	SS = _mm256_add_epi32(SS, _mm256_srai_epi32(DD, 16));
	__m128i S = _mm_add_epi32(_mm256_castsi256_si128(SS), _mm256_extracti128_si256(SS, 1));

	__m128i D = _mm_i32gather_epi32((int *)((short *) w->S8x4 - 1), _mm_cvtepu16_epi32(eval->feature.v8[3]), 2);
	S = _mm_add_epi32(S, _mm_srai_epi32(D, 16));

	S = _mm_hadd_epi32(S, S);
	sum = _mm_cvtsi128_si32(S) + _mm_extract_epi32(S, 1);

#else
	sum = w->C9[f[ 0]] + w->C9[f[ 1]] + w->C9[f[ 2]] + w->C9[f[ 3]]
	  + w->C10[f[ 4]] + w->C10[f[ 5]] + w->C10[f[ 6]] + w->C10[f[ 7]]
	  + w->S100[f[ 8]] + w->S100[f[ 9]] + w->S100[f[10]] + w->S100[f[11]]
	  + w->S101[f[12]] + w->S101[f[13]] + w->S101[f[14]] + w->S101[f[15]]
	  + w->S8x4[f[16]] + w->S8x4[f[17]] + w->S8x4[f[18]] + w->S8x4[f[19]]
	  + w->S8x4[f[20]] + w->S8x4[f[21]] + w->S8x4[f[22]] + w->S8x4[f[23]]
	  + w->S8x4[f[24]] + w->S8x4[f[25]] + w->S8x4[f[26]] + w->S8x4[f[27]]
	  + w->S7654[f[30]] + w->S7654[f[31]] + w->S7654[f[32]] + w->S7654[f[33]]
	  + w->S7654[f[34]] + w->S7654[f[35]] + w->S7654[f[36]] + w->S7654[f[37]]
	  + w->S7654[f[38]] + w->S7654[f[39]] + w->S7654[f[40]] + w->S7654[f[41]]
	  + w->S7654[f[42]] + w->S7654[f[43]] + w->S7654[f[44]] + w->S7654[f[45]];
#endif
	return sum + w->S8x4[f[28]] + w->S8x4[f[29]] + w->S0;
}

/**
 * @brief evaluate a midgame position with the evaluation function.
 *
 * @param search Position to evaluate.
 */
int search_eval_0(Search *search)
{
	int score;

	SEARCH_STATS(++statistics.n_search_eval_0);
	SEARCH_UPDATE_EVAL_NODES(search->n_nodes);

	score = accumlate_eval(60 - search->eval.n_empties,  &search->eval);

	if (score > 0) score += 64;	else score -= 64;
	score /= 128;

	if (score < SCORE_MIN + 1) score = SCORE_MIN + 1;
	if (score > SCORE_MAX - 1) score = SCORE_MAX - 1;

	return score;
}

/**
 * @brief Evaluate a position at depth 1.
 *
 * As an optimization, the last move is used to only updates the evaluation
 * features.
 *
 * @param search Position to evaluate.
 * @param alpha Alpha bound.
 * @param beta Beta bound.
 */
int search_eval_1(Search *search, const int alpha, int beta, bool pass1)
{
	Eval Ev;
	int x, score, bestscore, betathres;
	unsigned long long flipped;
	unsigned long long moves = get_moves(search->board.player, search->board.opponent);

	SEARCH_STATS(++statistics.n_search_eval_1);
	SEARCH_UPDATE_INTERNAL_NODES(search->n_nodes);

	if (moves) {
		bestscore = -SCORE_INF * 128;
		if (beta > SCORE_MAX - 1) betathres = ((SCORE_MAX - 1) * 128) - 64;
		else betathres = (beta * 128) - ((beta > 0) ? 64 : 63);	// lowest score rounded to beta

		x = NOMOVE;
		do {
			do {
				x = search->empties[x].next;
			} while (!(moves & x_to_bit(x)));

			moves &= ~x_to_bit(x);
			flipped = board_flip(&search->board, x);
			if (flipped == search->board.opponent)
				return SCORE_MAX;	// wipeout

			eval_update_leaf(x, flipped, &Ev, &search->eval);
			SEARCH_UPDATE_EVAL_NODES(search->n_nodes);

			score = -accumlate_eval(60 - search->eval.n_empties + 1, &Ev);

			if (score > bestscore)
				bestscore = score;
			if (bestscore >= betathres) break;
		} while (moves);

		if (bestscore >= 0) {
			bestscore = (bestscore + 64) >> 7;
			if (bestscore > SCORE_MAX - 1) bestscore = SCORE_MAX - 1;
		} else {
			bestscore = -((-bestscore + 64) >> 7);
			if (bestscore < SCORE_MIN + 1) bestscore = SCORE_MIN + 1;
		}

	} else {
		if (pass1) { // game over
			bestscore = search_solve(search);
		} else {
			search_update_pass_midgame(search, &Ev);
			bestscore = -search_eval_1(search, -beta, -alpha, true);
			search_restore_pass_midgame(search, &Ev);
		}
	}

	assert(SCORE_MIN <= bestscore && bestscore <= SCORE_MAX);
	return bestscore;
}

/**
 * @brief Evaluate a position at depth 2.
 *
 * Simple alpha-beta with no move sorting.
 *
 * @param search Position to evaluate.
 * @param alpha Lower bound
 * @param beta  Upper bound
 */
int search_eval_2(Search *search, int alpha, const int beta, bool pass1)
{
	int x, prev, bestscore, score;
	unsigned long long flipped;
	Eval eval0;
	vBoard board0 = load_vboard(search->board);
	unsigned long long moves = vboard_get_moves(board0, search->board);

	SEARCH_STATS(++statistics.n_search_eval_2);
	SEARCH_UPDATE_INTERNAL_NODES(search->n_nodes);

	assert(-SCORE_MAX <= alpha);
	assert(beta <= SCORE_MAX);
	assert(alpha <= beta);

	if (moves) {
		bestscore = -SCORE_INF;
		eval0.feature = search->eval.feature;
		eval0.n_empties = search->eval.n_empties--;

		x = NOMOVE;
		do {
			do {
				x = search->empties[prev = x].next;
			} while (!(moves & x_to_bit(x)));

			moves &= ~x_to_bit(x);
			search->empties[prev].next = search->empties[x].next;	// remove - maintain single link only
			flipped = vboard_next(board0, x, &search->board);
			eval_update_leaf(x, flipped, &search->eval, &eval0);
			score = -search_eval_1(search, -beta, -alpha, false);
			search->empties[prev].next = x;	// restore

			if (score > bestscore) {
				bestscore = score;
				if (bestscore >= beta) break;
				else if (bestscore > alpha) alpha = bestscore;
			}
		} while (moves);
		search->eval.feature = eval0.feature;
		search->eval.n_empties = eval0.n_empties;
		store_vboard(search->board, board0);

	} else {
		if (pass1) { // game over
			bestscore = search_solve(search);
		} else {
			search_update_pass_midgame(search, &eval0);
			bestscore = -search_eval_2(search, -beta, -alpha, true);
			search_restore_pass_midgame(search, &eval0);
		}
	}

	assert(SCORE_MIN <= bestscore && bestscore <= SCORE_MAX);

	return bestscore;
}

/**
 * @brief Probcut
 *
 * Check if a position is worth to analyze further.
 *
 * @param search Position to test.
 * @param alpha Alpha lower bound.
 * @param depth Search depth.
 * @param parent Parent node.
 * @param value Returned value.
 * (called from PVS_shallow and NWS_midgame @ depth <= 3)
 *
 * @return true if probable cutoff has been found, false otherwise.
 */
static inline void search_update_probcut(Search *search, const NodeType node_type) 
{
	search->node_type[search->height] = node_type;
	if (!USE_RECURSIVE_PROBCUT) search->selectivity = NO_SELECTIVITY;
	LIMIT_RECURSIVE_PROBCUT(++search->probcut_level;)
}


static inline void search_restore_probcut(Search *search, const NodeType node_type, const int selectivity) 
{
	search->node_type[search->height] = node_type;
	if (!USE_RECURSIVE_PROBCUT) search->selectivity = selectivity;
	LIMIT_RECURSIVE_PROBCUT(--search->probcut_level;)
}

static bool search_probcut(Search *search, const int alpha, const int depth, Node *parent, int *value)
{
	// assertion 
	assert(search != NULL);
	assert(parent != NULL);
	assert(search->node_type[search->height] != PV_NODE);
	assert(0 <= search->selectivity && search->selectivity <= NO_SELECTIVITY);

	// do probcut ?
	if (USE_PROBCUT && depth >= options.probcut_d && search->selectivity < NO_SELECTIVITY	LIMIT_RECURSIVE_PROBCUT(&& search->probcut_level < 2)) {
		int probcut_error, eval_error;
		int probcut_depth, probcut_beta, probcut_alpha;
		int eval_score, eval_beta, eval_alpha;
		int score;
		const int beta = alpha + 1;
		double t = selectivity_table[search->selectivity].t;
		const int saved_selectivity = search->selectivity;
		const NodeType node_type = search->node_type[search->height];

		PROBCUT_STATS(++statistics.n_probcut_try);

		// compute reduced depth & associated error
		probcut_depth = 2 * floor(options.probcut_d * depth) + (depth & 1);
		if (probcut_depth == 0) probcut_depth = depth - 2; 
		assert(probcut_depth > 1 && probcut_depth <= depth - 2 && (probcut_depth & 1) == (depth & 1));
		probcut_error = t * eval_sigma(search->eval.n_empties, depth, probcut_depth) + RCD;

		// compute evaluation error (i.e. error at depth 0) averaged for both depths
		eval_score = search_eval_0(search);
		eval_error = t * 0.5 * (eval_sigma(search->eval.n_empties, depth, 0) + eval_sigma(search->eval.n_empties, depth, probcut_depth)) + RCD;
	
		// try a probable upper cut first
		eval_beta = beta - eval_error;
		probcut_beta = beta + probcut_error;
		probcut_alpha = probcut_beta - 1;
		if (eval_score >= eval_beta && probcut_beta < SCORE_MAX) { 	// check if trying a beta probcut is worth
			PROBCUT_STATS(++statistics.n_probcut_high_try);
			search_update_probcut(search, CUT_NODE);
			score = NWS_midgame(search, probcut_alpha, probcut_depth, parent);
			search_restore_probcut(search, node_type, saved_selectivity);
			if (score >= probcut_beta) {
				*value = beta;
				PROBCUT_STATS(++statistics.n_probcut_high_cutoff);
				return true;
			}
		}
		
		// try a probable lower cut if upper cut failed
		eval_alpha = alpha + eval_error;
		probcut_alpha = alpha - probcut_error;
		if (eval_score < eval_alpha && probcut_alpha > SCORE_MIN) { // check if trying an alpha probcut is worth
			PROBCUT_STATS(++statistics.n_probcut_low_try);
			search_update_probcut(search, ALL_NODE);
			score = NWS_midgame(search, probcut_alpha, probcut_depth, parent);
			search_restore_probcut(search, node_type, saved_selectivity);
			if (score <= probcut_alpha) {
				*value = alpha;
				PROBCUT_STATS(++statistics.n_probcut_low_cutoff);
				return true;
			}
		}
	}

	return false;
}

/**
 * @brief Evaluate a midgame position with a Null Window Search algorithm.
 *
 * This function is used when there are still many empty squares on the board. Move
 * ordering, hash table cutoff, enhanced transposition cutoff, etc. are used in
 * order to diminish the size of the tree to analyse, but at the expense of a
 * slower speed.
 * (called from guess_move @ depth <= 6 and movelist_evaluate @ depth > 2)
 *
 * @param search     search.
 * @param alpha      lower bound.
 * @param depth Search remaining depth.
 * @param hash_table Hash Table to use.
 * @return An evaluated score, as a disc difference.
 */
static int NWS_shallow(Search *search, const int alpha, int depth, HashTable *hash_table)
{
	int score, bestscore;
	unsigned long long hash_code;
	// const int beta = alpha + 1;
	HashStoreData hash_data;
	MoveList movelist;
	Move *move;
	Search_Backup backup;
	long long nodes_org = search->n_nodes;

	if (depth == 2) return search_eval_2(search, alpha, alpha + 1, false);

	SEARCH_STATS(++statistics.n_NWS_midgame);
	SEARCH_UPDATE_INTERNAL_NODES(search->n_nodes);

	assert(search->eval.n_empties == bit_count(~(search->board.player | search->board.opponent)));
	assert(SCORE_MIN <= alpha && alpha <= SCORE_MAX);
	assert(depth > 2);
	assert(hash_table != NULL);

	hash_code = board_get_hash_code(&search->board);
	hash_prefetch(hash_table, hash_code);

	// stability cutoff
	if (search_SC_NWS(search, alpha, &score)) return score;

	search_get_movelist(search, &movelist);
	// transposition cutoff
	if (hash_get(hash_table, &search->board, hash_code, &hash_data.data) && search_TC_NWS(&hash_data.data, depth, search->selectivity, alpha, &score))
		return score;

	if (movelist_is_empty(&movelist)) { // no moves ?
		if (can_move(search->board.opponent, search->board.player)) { // pass ?
			search_update_pass_midgame(search, &backup.eval);
			bestscore = -NWS_shallow(search, ~alpha, depth, hash_table);
			hash_data.data.move[0] = PASS;
			search_restore_pass_midgame(search, &backup.eval);
		} else { // game-over !
			bestscore = search_solve(search);
			hash_data.data.move[0] = NOMOVE;
		}
	} else {
		// sort the list of moves
		movelist_evaluate(&movelist, search, &hash_data.data, alpha, depth);
		movelist_sort(&movelist);

		// loop over all moves
		bestscore = -SCORE_INF;
		backup.board = search->board;
		backup.eval = search->eval;
		move = movelist.move[0].next;
		do {
			search_update_midgame(search, move);
			score = -NWS_shallow(search, ~alpha, depth - 1, hash_table);
			search_restore_midgame(search, move->x, &backup);
			if (score > bestscore) {
				bestscore = score;
				hash_data.data.move[0] = move->x;
				if (score > alpha) break;
			}
		} while ((move = move->next));
	}

	// save the best result in hash tables
	hash_data.data.wl.c.depth = depth;
	hash_data.data.wl.c.selectivity = search->selectivity;
	hash_data.data.wl.c.cost = last_bit(search->n_nodes - nodes_org);
	// hash_data.data.move[0] = bestmove;
	hash_data.alpha = alpha;
	hash_data.beta = alpha + 1;
	hash_data.score = bestscore;
	hash_store(hash_table, &search->board, hash_code, &hash_data);
 	assert(SCORE_MIN <= bestscore && bestscore <= SCORE_MAX);

	return bestscore;
}

/**
 * @brief Evaluate a midgame position at shallow depth.
 *
 * This function is used when there are still many empty squares on the board. Move
 * ordering, hash table cutoff, enhanced transposition cutoff, etc. are used in
 * order to diminish the size of the tree to analyse, but at the expense of a
 * slower speed.
 *
 * @param search Search.
 * @param alpha Alpha bound.
 * @param beta Beta bound.
 * @param depth Search depth.
 * @return An evaluated score, as a disc difference.
 */
int PVS_shallow(Search *search, int alpha, int beta, int depth)
{
	int score, bestscore, lower;
	unsigned long long hash_code;
	HashStoreData hash_data;
	MoveList movelist;
	Move *move;
	Search_Backup backup;
	long long nodes_org = search->n_nodes;

	if (depth == 2) return search_eval_2(search, alpha, beta, false);

	SEARCH_STATS(++statistics.n_PVS_shallow);
	SEARCH_UPDATE_INTERNAL_NODES(search->n_nodes);

	assert(search->eval.n_empties == bit_count(~(search->board.player | search->board.opponent)));
	assert(SCORE_MIN <= alpha && alpha <= SCORE_MAX);

	// stability cutoff
	if (search_SC_PVS(search, &alpha, &beta, &score)) return score;

	// transposition cutoff
	hash_code = board_get_hash_code(&search->board);
//	if (hash_get(&search->shallow_table, &search->board, hash_code, &hash_data.data) && search_TC_PVS(&hash_data.data, depth, search->selectivity, &alpha, &beta, &score)) return score;
	hash_get(&search->shallow_table, &search->board, hash_code, &hash_data.data);

	search_get_movelist(search, &movelist);

	if (movelist_is_empty(&movelist)) { // no moves ?
		if (can_move(search->board.opponent, search->board.player)) { // pass ?
			search_update_pass_midgame(search, &backup.eval);
			bestscore = -PVS_shallow(search, -beta, -alpha, depth);
			hash_data.data.move[0] = PASS;
			search_restore_pass_midgame(search, &backup.eval);
		} else { // game-over !
			bestscore = search_solve(search);
			hash_data.data.move[0] = NOMOVE;
		}
	} else {
		// sort the list of moves
		movelist_evaluate(&movelist, search, &hash_data.data, alpha, depth);
		movelist_sort(&movelist) ;

		// loop over all moves
		backup.board = search->board;
		backup.eval = search->eval;
		move = movelist.move[0].next;

		search_update_midgame(search, move);
		bestscore = score = -PVS_shallow(search, -beta, -alpha, depth - 1);
		hash_data.data.move[0] = move->x;
		search_restore_midgame(search, move->x, &backup);
		lower = (bestscore > alpha) ? bestscore : alpha;

		while ((move = move->next) && (bestscore < beta)) {
			search_update_midgame(search, move);
			score = -NWS_shallow(search, -lower - 1, depth - 1, &search->hash_table);
			if (alpha < score && score < beta)
				score = -PVS_shallow(search, -beta, -lower, depth - 1);
			search_restore_midgame(search, move->x, &backup);
			if (score > bestscore) {
				bestscore = score;
				hash_data.data.move[0] = move->x;
				if (score > lower) lower = score;
			}
		}
	}

	// save the best result in hash tables
	hash_data.data.wl.c.depth = depth;
	hash_data.data.wl.c.selectivity = search->selectivity;
	hash_data.data.wl.c.cost = last_bit(search->n_nodes - nodes_org);
	// hash_data.data.move[0] = bestmove;
	hash_data.alpha = alpha;
	hash_data.beta = beta;
	hash_data.score = bestscore;
	hash_store(&search->shallow_table, &search->board, hash_code, &hash_data);
 	assert(SCORE_MIN <= bestscore && bestscore <= SCORE_MAX);

	return bestscore;
}


/**
 * @brief Evaluate a midgame position with a Null Window Search algorithm.
 *
 * This function is used when there are still many empty squares on the board. Move
 * ordering, hash table cutoff, enhanced transposition cutoff, etc. are used in
 * order to diminish the size of the tree to analyse, but at the expense of a
 * slower speed.
 *
 * @param search Search.
 * @param alpha Alpha bound.
 * @param depth Depth.
 * @param parent Parent node.
 * @return A score, as a disc difference.
 */
int NWS_midgame(Search *search, const int alpha, int depth, Node *parent)
{
	int score;
	unsigned long long hash_code;
	const int beta = alpha + 1;
	HashStoreData hash_data;
	MoveList movelist;
	Move *move;
	Node node;
	Search_Backup backup;
	long long nodes_org = search->n_nodes + search->child_nodes;

	assert(search->eval.n_empties == bit_count(~(search->board.player | search->board.opponent)));
	assert((2 <= depth && depth < search->eval.n_empties) || depth == search->eval.n_empties);
	assert(SCORE_MIN <= alpha && alpha <= SCORE_MAX);
	assert(parent != NULL);

	search_check_timeout(search);
	if (search->stop) return alpha;
	else if (search->eval.n_empties == 0)
		return search_solve_0(search);
	else if (depth <= 3 && depth < search->eval.n_empties)
		return NWS_shallow(search, alpha, depth, &search->hash_table);
	else if (search->eval.n_empties <= depth && depth < DEPTH_MIDGAME_TO_ENDGAME)
		return NWS_endgame(search, alpha);

	SEARCH_STATS(++statistics.n_NWS_midgame);
	SEARCH_UPDATE_INTERNAL_NODES(search->n_nodes);

	// stability cutoff
	if (search_SC_NWS(search, alpha, &score)) return score;

	hash_code = board_get_hash_code(&search->board);
	hash_prefetch(&search->hash_table, hash_code);
	hash_prefetch(&search->pv_table, hash_code);

	search_get_movelist(search, &movelist);

	// transposition cutoff
	if (hash_get(&search->hash_table, &search->board, hash_code, &hash_data.data) || hash_get(&search->pv_table, &search->board, hash_code, &hash_data.data))
		if (search_TC_NWS(&hash_data.data, depth, search->selectivity, alpha, &score)) return score;

	if (movelist_is_empty(&movelist)) { // no moves ?
		node_init(&node, search, alpha, beta, depth, movelist.n_moves, parent);
		if (can_move(search->board.opponent, search->board.player)) { // pass ?
			search_update_pass_midgame(search, &backup.eval);
			node.bestscore = -NWS_midgame(search, -node.beta, depth, &node);
			search_restore_pass_midgame(search, &backup.eval);
		} else { // game-over !
			node.bestscore = search_solve(search);
		}

	} else {
		// probcut
		if (search_probcut(search, alpha, depth, parent, &score)) return score;

		// sort the list of moves
		if (movelist.n_moves > 1) {
			if (hash_data.data.move[0] == NOMOVE) hash_get(&search->hash_table, &search->board, hash_code, &hash_data.data);
			movelist_evaluate(&movelist, search, &hash_data.data, alpha, depth + options.inc_sort_depth[search->node_type[search->height]]);
			movelist_sort(&movelist);
		}

		// ETC
		if (search_ETC_NWS(search, &movelist, hash_code, depth, search->selectivity, alpha, &score)) return score;

		node_init(&node, search, alpha, beta, depth, movelist.n_moves, parent);

		// loop over all moves
		backup.board = search->board;
		backup.eval = search->eval;
		for (move = node_first_move(&node, &movelist); move; move = node_next_move(&node)) {
			if (!node_split(&node, move)) {
				search_update_midgame(search, move);
				move->score = -NWS_midgame(search, -beta, depth - 1, &node);
				search_restore_midgame(search, move->x, &backup);
				node_update(&node, move);
			}
		}
		node_wait_slaves(&node);
	}

	// save the best result in hash tables
	if (!search->stop) {
		if (search->eval.n_empties < depth && depth <= DEPTH_MIDGAME_TO_ENDGAME) hash_data.data.wl.c.selectivity = NO_SELECTIVITY; // hack
		else hash_data.data.wl.c.selectivity = search->selectivity;
		hash_data.data.wl.c.depth = depth;
		hash_data.data.wl.c.cost = last_bit(search->n_nodes + search->child_nodes - nodes_org);
		hash_data.data.move[0] = node.bestmove;
		hash_data.alpha = alpha;
		hash_data.beta = beta;
		hash_data.score = node.bestscore;

		if (search->height <= PV_HASH_HEIGHT) hash_store(&search->pv_table, &search->board, hash_code, &hash_data);
		hash_store(&search->hash_table, &search->board, hash_code, &hash_data);

		SQUARE_STATS(foreach_move(move, &movelist))
		SQUARE_STATS(++statistics.n_played_square[search->eval.n_empties][SQUARE_TYPE[move->x]];)
		SQUARE_STATS(if (node.bestscore > alpha) ++statistics.n_good_square[search->eval.n_empties][SQUARE_TYPE[node->bestmove]];)

	 	assert(SCORE_MIN <= node.bestscore && node.bestscore <= SCORE_MAX);
	} else {
		node.bestscore = alpha;
	}

	node_free(&node);

	return node.bestscore;
}


/**
 * @brief Evaluate a position with a deep Principal Variation Search algorithm.
 *
 * This function is used when there are still many empty squares on the board. Move
 * ordering, hash table cutoff, enhanced transposition cutoff, etc. are used in
 * order to diminish the size of the tree to analyse, but at the expense of a
 * slower speed.
 *
 * @param search Search.
 * @param alpha Lower bound.
 * @param beta Upper bound.
 * @param depth Depth.
 * @param parent Parent node.
 * @return A score, as a disc difference.
 */
int PVS_midgame(Search *search, const int alpha, const int beta, int depth, Node *parent)
{
	// declaration
	HashTable *const hash_table = &search->hash_table;
	HashTable *const pv_table = &search->pv_table;
	unsigned long long hash_code;
	HashStoreData hash_data;
	MoveList movelist;
	Move *move;
	Node node;
	Search_Backup backup;
	long long nodes_org;
	int reduced_depth, depth_pv_extension, saved_selectivity;

	SEARCH_STATS(++statistics.n_PVS_midgame);

	assert(search->eval.n_empties == bit_count(~(search->board.player | search->board.opponent)));
	assert(depth <= search->eval.n_empties);
	assert((-SCORE_MAX <= alpha && alpha <= SCORE_MAX) || printf("alpha = %d\n", alpha));
	assert((-SCORE_MAX <= beta && beta <= SCORE_MAX) || printf("beta = %d\n", beta));
	assert(alpha <= beta);

	// end of search ?
	search_check_timeout(search);
	if (search->stop) return alpha;
	else if (search->eval.n_empties == 0)
		return search_solve_0(search);
	else if (USE_PV_EXTENSION && depth < search->eval.n_empties && search->eval.n_empties <= search->depth_pv_extension)
		return PVS_midgame(search, alpha, beta, search->eval.n_empties, parent);
	else if (depth == 2 && search->eval.n_empties > 2)
		return search_eval_2(search, alpha, beta, false);

	nodes_org = search_count_nodes(search);
	SEARCH_UPDATE_INTERNAL_NODES(search->n_nodes);

	search_get_movelist(search, &movelist);
	node_init(&node, search, alpha, beta, depth, movelist.n_moves, parent);
	node.pv_node = true;
	hash_code = board_get_hash_code(&search->board);

	// special cases
	if (movelist_is_empty(&movelist)) {
		if (can_move(search->board.opponent, search->board.player)) {
			search_update_pass_midgame(search, &backup.eval);
			search->node_type[search->height] = PV_NODE;
			node.bestscore = -PVS_midgame(search, -beta, -alpha, depth, &node);
			search_restore_pass_midgame(search, &backup.eval);
			node.bestmove = PASS;
		} else {
			node.alpha = -(node.beta = +SCORE_INF);
			node.bestscore = search_solve(search);
			node.bestmove = NOMOVE;
		}
	} else { // normal PVS
		if (movelist.n_moves > 1) {
			//IID
			if (!hash_get(pv_table, &search->board, hash_code, &hash_data.data))
				hash_get(hash_table, &search->board, hash_code, &hash_data.data);
			if (USE_IID && hash_data.data.move[0] == NOMOVE) {
				if (depth == search->eval.n_empties) reduced_depth = depth - ITERATIVE_MIN_EMPTIES;
				else reduced_depth = depth - 2;
				if (reduced_depth >= 3) {
					saved_selectivity = search->selectivity; search->selectivity = 0;
					depth_pv_extension = search->depth_pv_extension;
					search->depth_pv_extension = 0;
					PVS_midgame(search, SCORE_MIN, SCORE_MAX, reduced_depth, parent);
					hash_get(pv_table, &search->board, hash_code, &hash_data.data);
					search->depth_pv_extension = depth_pv_extension;
					search->selectivity = saved_selectivity;
				}
			}

			// Evaluate moves for sorting. For a better ordering, the depth is artificially increased
			movelist_evaluate(&movelist, search, &hash_data.data, node.alpha, depth + options.inc_sort_depth[PV_NODE]);
			movelist_sort(&movelist);
		}

		// first move
		backup.board = search->board;
		backup.eval = search->eval;
		if ((move = node_first_move(&node, &movelist))) { // why if there ?
			search_update_midgame(search, move); search->node_type[search->height] = PV_NODE;
			move->score = -PVS_midgame(search, -beta, -alpha, depth - 1, &node);
			search_restore_midgame(search, move->x, &backup);
			node_update(&node, move);

			// other moves : try to refute the first/best one
			while ((move = node_next_move(&node))) {
				if (!node_split(&node, move)) {
					const int alpha = node.alpha;
					search_update_midgame(search, move);
					move->score = -NWS_midgame(search, -alpha - 1, depth - 1, &node);
					if (!search->stop && alpha < move->score && move->score < beta) {
						search->node_type[search->height] = PV_NODE;
						move->score = -PVS_midgame(search, -beta, -alpha, depth - 1, &node);
					}
					search_restore_midgame(search, move->x, &backup);
					node_update(&node, move);
				}
			}
			node_wait_slaves(&node);
		}
	}

	// save the best result in hash tables
	if (!search->stop) {
		if (search->eval.n_empties < depth && depth <= DEPTH_MIDGAME_TO_ENDGAME)
			hash_data.data.wl.c.selectivity = NO_SELECTIVITY;
		else	hash_data.data.wl.c.selectivity = search->selectivity;
		hash_data.data.wl.c.depth = depth;
		hash_data.data.wl.c.cost = last_bit(search_count_nodes(search) - nodes_org);
		hash_data.data.move[0] = node.bestmove;
		hash_data.alpha = alpha;
		hash_data.beta = beta;
		hash_data.score = node.bestscore;

		hash_store(hash_table, &search->board, hash_code, &hash_data);
		hash_store(pv_table, &search->board, hash_code, &hash_data);

		SQUARE_STATS(foreach_move(move, movelist))
		SQUARE_STATS(++statistics.n_played_square[search->eval.n_empties][SQUARE_TYPE[move->x]];)
		SQUARE_STATS(if (node.bestscore > alpha) ++statistics.n_good_square[search->eval.n_empties][SQUARE_TYPE[node.bestmove]];)

	 	assert(SCORE_MIN <= node.bestscore && node.bestscore <= SCORE_MAX);

	} else {
		node.bestscore = alpha;
	}

	node_free(&node);

	return node.bestscore;
}
