/**
 * @file midgame.c
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

/** 512-bit registers available (not disabled by -mno-evex512 / AVX10/256) */
#if defined(__AVX512F__) && !defined(EVAL_GATHER512)
  #if defined(__EVEX512__)
	#define EVAL_GATHER512
  #elif defined(__clang__)
    #if __clang_major__ < 18	// no -mno-evex512 before clang 18
	#define EVAL_GATHER512
    #endif
  #elif defined(__GNUC__)
    #if __GNUC__ < 14	// no -mno-evex512 before gcc 14
	#define EVAL_GATHER512
    #endif
  #elif defined(_MSC_VER)
	#define EVAL_GATHER512
  #endif
#endif

/**
 * @brief evaluate a midgame position with the evaluation function.
 *
 * @param ply	60 - n_empties
 * @param eval	Evaluation function.
 * @return An evaluated score.
 */
static int accumlate_eval(int ply, Eval *eval)
{
	unsigned short *f = eval->feature.us;
#ifdef EVAL_FLAT_TABLE
	// one by one: faster than 16-lane gathers on Zen 5, with the same value
	const short *t = EVAL_FLAT + (size_t) ply * EVAL_FLAT_N;
	const short *c9 = t + offsetof(Eval_weight, C9) / 4;
	const short *c10 = t + offsetof(Eval_weight, C10) / 4;
	const short *s100 = t + offsetof(Eval_weight, S100) / 4;
	const short *s101 = t + offsetof(Eval_weight, S101) / 4;
	const short *s8 = t + offsetof(Eval_weight, S8x4) / 4;
	const short *s7 = t + offsetof(Eval_weight, S7654) / 4;

	return t[0]
	  + c9[f[0]] + c9[f[1]] + c9[f[2]] + c9[f[3]]
	  + c10[f[4]] + c10[f[5]] + c10[f[6]] + c10[f[7]]
	  + s100[f[8]] + s100[f[9]] + s100[f[10]] + s100[f[11]]
	  + s101[f[12]] + s101[f[13]] + s101[f[14]] + s101[f[15]]
	  + s8[f[16]] + s8[f[17]] + s8[f[18]] + s8[f[19]]
	  + s8[f[20]] + s8[f[21]] + s8[f[22]] + s8[f[23]]
	  + s8[f[24]] + s8[f[25]] + s8[f[26]] + s8[f[27]]
	  + s8[f[28]] + s8[f[29]]
	  + s7[f[30]] + s7[f[31]] + s7[f[32]] + s7[f[33]]
	  + s7[f[34]] + s7[f[35]] + s7[f[36]] + s7[f[37]]
	  + s7[f[38]] + s7[f[39]] + s7[f[40]] + s7[f[41]]
	  + s7[f[42]] + s7[f[43]] + s7[f[44]] + s7[f[45]];
#else
	const Eval_weight *w;
	int b0, sum;

	b0 = ply & 1;	// even/odd ply interleaved for aligned VPGATHERDD access (4.5.5)
	ply -= 2;
	if (ply < 0)
		ply = 0;
	ply >>= 1;
	if (ply >= EVAL_N_2PLY - 1)
		ply = EVAL_N_2PLY - 2;

#ifdef EVAL_GATHER512
	// 46 features in 3 x 16-lane gathers (the last 2 lanes unused); integer sums, so the same value as below.
	enum {
		W_C9 = offsetof(Eval_weight, C9) / 4,
		W_C10 = offsetof(Eval_weight, C10) / 4,
		W_S100 = offsetof(Eval_weight, S100) / 4,
		W_S101 = offsetof(Eval_weight, S101) / 4,
		W_S8 = offsetof(Eval_weight, S8x4) / 4,
		W_S7 = offsetof(Eval_weight, S7654) / 4
	};
	w = *EVAL_WEIGHT + ply;
	__m128i SW = _mm_cvtsi32_si128((b0 ^ 1) * 16);	// 16 for even, 0 for odd
	__m512i FF = _mm512_add_epi32(_mm512_cvtepu16_epi32(eval->feature.v16[0]), _mm512_set_epi32(
		W_S101, W_S101, W_S101, W_S101, W_S100, W_S100, W_S100, W_S100, W_C10, W_C10, W_C10, W_C10, W_C9, W_C9, W_C9, W_C9));
	__m512i DD = _mm512_i32gather_epi32(FF, (const int *) w, 4);
	__m512i SS = _mm512_srai_epi32(_mm512_sll_epi32(DD, SW), 16);	// select word and sign extend

	FF = _mm512_add_epi32(_mm512_cvtepu16_epi32(eval->feature.v16[1]), _mm512_set_epi32(
		W_S7, W_S7, W_S8, W_S8, W_S8, W_S8, W_S8, W_S8, W_S8, W_S8, W_S8, W_S8, W_S8, W_S8, W_S8, W_S8));
	DD = _mm512_i32gather_epi32(FF, (const int *) w, 4);
	SS = _mm512_add_epi32(SS, _mm512_srai_epi32(_mm512_sll_epi32(DD, SW), 16));

	FF = _mm512_add_epi32(_mm512_cvtepu16_epi32(eval->feature.v16[2]), _mm512_set1_epi32(W_S7));
	DD = _mm512_mask_i32gather_epi32(_mm512_setzero_si512(), 0x3fff, FF, (const int *) w, 4);
	SS = _mm512_add_epi32(SS, _mm512_srai_epi32(_mm512_sll_epi32(DD, SW), 16));
	sum = _mm512_reduce_add_epi32(SS);
	w = (Eval_weight *)(w->S0 + b0);
	return sum + *(w->S0);

#elif defined(__AVX2__) && !defined(__bdver4__) && !defined(__znver1__) && !defined(__znver2__)
	enum {
		W_C9 = offsetof(Eval_weight, C9) / 4,
		W_C10 = offsetof(Eval_weight, C10) / 4,
		W_S100 = offsetof(Eval_weight, S100) / 4,
		W_S101 = offsetof(Eval_weight, S101) / 4
	};
	w = *EVAL_WEIGHT + ply;
	__m128i SW = _mm_cvtsi32_si128((b0 ^ 1) * 16);	// 16 for even, 0 for odd
	__m256i FF = _mm256_add_epi32(_mm256_cvtepu16_epi32(eval->feature.v8[0]),
		_mm256_set_epi32(W_C10, W_C10, W_C10, W_C10, W_C9, W_C9, W_C9, W_C9));
	__m256i DD = _mm256_i32gather_epi32((int *) w, FF, 4);
	__m256i SS = _mm256_srai_epi32(_mm256_sll_epi32(DD, SW), 16);	// select word and sign extend

	FF = _mm256_add_epi32(_mm256_cvtepu16_epi32(eval->feature.v8[1]),
		_mm256_set_epi32(W_S101, W_S101, W_S101, W_S101, W_S100, W_S100, W_S100, W_S100));
	DD = _mm256_i32gather_epi32((int *) w, FF, 4);
	SS = _mm256_add_epi32(SS, _mm256_srai_epi32(_mm256_sll_epi32(DD, SW), 16));

	DD = _mm256_i32gather_epi32((int *) w->S8x4, _mm256_cvtepu16_epi32(eval->feature.v8[2]), 4);
	SS = _mm256_add_epi32(SS, _mm256_srai_epi32(_mm256_sll_epi32(DD, SW), 16));

	DD = _mm256_i32gather_epi32((int *) w->S7654, _mm256_cvtepu16_epi32(_mm_loadu_si128((__m128i *) &eval->feature.us[30])), 4);
	SS = _mm256_add_epi32(SS, _mm256_srai_epi32(_mm256_sll_epi32(DD, SW), 16));

	DD = _mm256_i32gather_epi32((int *) w->S7654, _mm256_cvtepu16_epi32(_mm_loadu_si128((__m128i *) &eval->feature.us[38])), 4);
	SS = _mm256_add_epi32(SS, _mm256_srai_epi32(_mm256_sll_epi32(DD, SW), 16));
	__m128i S = _mm_add_epi32(_mm256_castsi256_si128(SS), _mm256_extracti128_si256(SS, 1));

	__m128i D = _mm_i32gather_epi32((int *) w->S8x4, _mm_cvtepu16_epi32(eval->feature.v8[3]), 4);
	S = _mm_add_epi32(S, _mm_srai_epi32(_mm_sll_epi32(D, SW), 16));
		// https://stackoverflow.com/questions/6996764/fastest-way-to-do-horizontal-sse-vector-sum-or-other-reduction
	S = _mm_add_epi32(S, _mm_unpackhi_epi64(S, S));
	sum = _mm_cvtsi128_si32(_mm_add_epi32(S, _mm_shufflelo_epi16(S, 0x4e)));
	w = (Eval_weight *)(w->S0 + b0);

#else
	w = (Eval_weight *)((*EVAL_WEIGHT)[ply].S0 + b0);
	sum = *(w->C9[f[0]]) + *(w->C9[f[1]]) + *(w->C9[f[2]]) + *(w->C9[f[3]])
	  + *(w->C10[f[4]]) + *(w->C10[f[5]]) + *(w->C10[f[6]]) + *(w->C10[f[7]])
	  + *(w->S100[f[8]]) + *(w->S100[f[9]]) + *(w->S100[f[10]]) + *(w->S100[f[11]])
	  + *(w->S101[f[12]]) + *(w->S101[f[13]]) + *(w->S101[f[14]]) + *(w->S101[f[15]])
	  + *(w->S8x4[f[16]]) + *(w->S8x4[f[17]]) + *(w->S8x4[f[18]]) + *(w->S8x4[f[19]])
	  + *(w->S8x4[f[20]]) + *(w->S8x4[f[21]]) + *(w->S8x4[f[22]]) + *(w->S8x4[f[23]])
	  + *(w->S8x4[f[24]]) + *(w->S8x4[f[25]]) + *(w->S8x4[f[26]]) + *(w->S8x4[f[27]])
	  + *(w->S7654[f[30]]) + *(w->S7654[f[31]]) + *(w->S7654[f[32]]) + *(w->S7654[f[33]])
	  + *(w->S7654[f[34]]) + *(w->S7654[f[35]]) + *(w->S7654[f[36]]) + *(w->S7654[f[37]])
	  + *(w->S7654[f[38]]) + *(w->S7654[f[39]]) + *(w->S7654[f[40]]) + *(w->S7654[f[41]])
	  + *(w->S7654[f[42]]) + *(w->S7654[f[43]]) + *(w->S7654[f[44]]) + *(w->S7654[f[45]]);
#endif
	return sum + *(w->S8x4[f[28]]) + *(w->S8x4[f[29]]) + *(w->S0);
#endif // EVAL_FLAT_TABLE
}

/**
 * @brief evaluate a midgame position with the evaluation function.
 *
 * @param search Position to evaluate.
 * @return An evaluated score.
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
 * @brief Evaluate a position at depth 1. (min stage)
 *
 * As an optimization, the last move is used to only updates the evaluation
 * features.
 *
 * @param search Position to evaluate.
 * @param alpha Alpha bound.
 * @param beta Beta bound.
 * @param moves Next turn legal moves.
 * @return An evaluated min score.
 */
int search_eval_1(Search *search, int alpha, int beta, unsigned long long moves)
{
	int x, score, bestscore, alphathres;
	unsigned long long flipped;
	Eval Ev;
	V2DI board0;

	SEARCH_STATS(++statistics.n_search_eval_1);
	SEARCH_UPDATE_INTERNAL_NODES(search->n_nodes);

	if (moves) {
		bestscore = SCORE_INF * 128;	// min stage
		if (alpha < SCORE_MIN + 1) alphathres = ((SCORE_MIN + 1) * 128) + 64;
		else alphathres = (alpha * 128) + 63 + (int) (alpha < 0);	// highest score rounded to alpha

		board0.board = search->board;
		x = NOMOVE;
		do {
			do {
				x = search->empties[x].next;
			} while (!(moves & x_to_bit(x)));

			moves &= ~x_to_bit(x);
			flipped = vboard_flip(board0, x);
			if (flipped == search->board.opponent)
				return SCORE_MIN;	// wipeout

			eval_update_leaf(x, flipped, &Ev, &search->eval);
			SEARCH_UPDATE_EVAL_NODES(search->n_nodes);

			score = accumlate_eval(60 - search->eval.n_empties + 1, &Ev);

			if (score < bestscore)
				bestscore = score;
		} while (moves && (bestscore > alphathres));

		if (bestscore >= 0) bestscore += 64; else bestscore -= 64;
		bestscore /= 128;

		if (bestscore < SCORE_MIN + 1) bestscore = SCORE_MIN + 1;
		if (bestscore > SCORE_MAX - 1) bestscore = SCORE_MAX - 1;

	} else {
		moves = get_moves(search->board.opponent, search->board.player);
		if (moves) {
			search_update_pass_midgame(search, &Ev);
			bestscore = -search_eval_1(search, -beta, -alpha, moves);
			search_restore_pass_midgame(search, &Ev);
		} else { // game over
			bestscore = -search_solve(search);
		}
	}

	assert(SCORE_MIN <= bestscore && bestscore <= SCORE_MAX);
	return bestscore;
}

#ifdef EVAL_UPDATE_PARITY
/**
 * @brief The moves of search_eval_2(), with the parity of the empty squares as a constant.
 *
 * The loop then neither keeps the parity across the calls of search_eval_1(), nor tests it at each move.
 */
static EVAL_INLINE int search_eval_2_moves(Search *search, int alpha, const int beta, unsigned long long moves, const Eval *eval0, const V2DI board0, const int parity)
{
	int x = NOMOVE, bestscore = -SCORE_INF, score;
	unsigned long long flipped;

	do {
		do {
			x = search->empties[x].next;
		} while (!(moves & x_to_bit(x)));

		moves &= ~x_to_bit(x);
		flipped = vboard_next(board0, x, &search->board);
		eval_update_sse_parity(x, flipped, &search->eval, eval0, parity);
		score = search_eval_1(search, alpha, beta, board_get_moves(&search->board));

		if (score > bestscore) {
			bestscore = score;
			if (bestscore >= beta) break;
			else if (bestscore > alpha) alpha = bestscore;
		}
	} while (moves);

	return bestscore;
}
#endif

/**
 * @brief Evaluate a position at depth 2.
 *
 * Simple alpha-beta with no move sorting.
 *
 * @param search Position to evaluate.
 * @param alpha Lower bound
 * @param beta  Upper bound
 * @param moves Next turn legal moves.
 * @return An evaluated best score.
 */
int search_eval_2(Search *search, int alpha, int beta, unsigned long long moves)
{
	int x, bestscore, score;
	unsigned long long flipped;
	Eval eval0;
	V2DI board0;

	SEARCH_STATS(++statistics.n_search_eval_2);
	SEARCH_UPDATE_INTERNAL_NODES(search->n_nodes);

	assert(-SCORE_MAX <= alpha);
	assert(beta <= SCORE_MAX);
	assert(alpha <= beta);

	if (moves) {
		bestscore = -SCORE_INF;
		eval0.feature = search->eval.feature;
		eval0.n_empties = search->eval.n_empties--;
		board0.board = search->board;
#ifdef EVAL_UPDATE_PARITY
		(void) x; (void) flipped; (void) score;
		if (eval0.n_empties & 1) bestscore = search_eval_2_moves(search, alpha, beta, moves, &eval0, board0, 1);
		else bestscore = search_eval_2_moves(search, alpha, beta, moves, &eval0, board0, 0);
#else
		x = NOMOVE;
		do {
			do {
				x = search->empties[x].next;
			} while (!(moves & x_to_bit(x)));

			moves &= ~x_to_bit(x);
			// search->empties[prev].next = search->empties[x].next;	// let search_eval_1 skip the last occupied
			flipped = vboard_next(board0, x, &search->board);
			eval_update_leaf(x, flipped, &search->eval, &eval0);
			score = search_eval_1(search, alpha, beta, board_get_moves(&search->board));
			// search->empties[prev].next = x;	// restore

			if (score > bestscore) {
				bestscore = score;
				if (bestscore >= beta) break;
				else if (bestscore > alpha) alpha = bestscore;
			}
		} while (moves);
#endif
		search->eval.feature = eval0.feature;
		search->eval.n_empties = eval0.n_empties;
		search->board = board0.board;

	} else {
		moves = get_moves(search->board.opponent, search->board.player);
		if (moves) {
			search_update_pass_midgame(search, &eval0);
			bestscore = -search_eval_2(search, -beta, -alpha, moves);
			search_restore_pass_midgame(search, &eval0);
		} else { // game over
			bestscore = search_solve(search);
		}
	}

	assert(SCORE_MIN <= bestscore && bestscore <= SCORE_MAX);

	return bestscore;
}

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

/**
 * @brief Late move reduction of a move (search-b, 2026-11-22; lmr 0 = none).
 *
 * Called after the move is made. A move late in the list, far from the principal variation, is searched 2 (or 4)
 * plies shallower first; a score above alpha is searched again at the full depth. Never in a search to the end
 * (depth == empties), nor at 100% selectivity. The conditions follow the idea of Melody (GPL-3.0), written anew.
 *
 * @param search Search (at the position after the move).
 * @param depth depth of the parent node.
 * @param order rank of the move in the parent node (1 = first).
 * @param hash_move the move is the best move of the hash table.
 * @return the reduction (0, 2 or 4).
 */
int lmr_reduction(const Search *search, const int depth, const int order, const bool hash_move)
{
	const int parent_empties = search->eval.n_empties + 1;
	const int dev = search->pv_dev[search->height];
	int r = 0;
	if (options.lmr == 0 || search->selectivity >= NO_SELECTIVITY || depth >= parent_empties || hash_move) return 0;
	if (options.lmr == 1 && search->probcut_level > 0) return 0;
	if (depth >= 12 && order >= 4 && dev >= 3) r = 2;
	if (r && depth >= 22 && order >= 6 && dev >= 4) r = 4;
	return r;
}

/*
 * pc-slope: a search to the end scores further from 0 than its probe. A probe s stands for
 * s + (slope - 1) * max(-limit, min(limit, s)); this is the s that stands for a score (the idea of the linear
 * regression of Logistello, written anew).
 */
static double probcut_probe_of(const double score)
{
	const double a = options.pc_slope, l = options.pc_slope_limit;
	if (score > a * l) return score - (a - 1.0) * l;
	if (score < -a * l) return score + (a - 1.0) * l;
	return score / a;
}

/* search-b: factor of the probcut margins (1.0 with the default options: the margins of v4.5.5-nikque.32) */
static double probcut_factor(const Search *search, const int depth, const int n_moves, const bool with_dev)
{
	const int empties = search->eval.n_empties;
	double f = options.pc_r;
	if (depth >= empties && options.pc_end[empties] > 0.0) f *= options.pc_end[empties];
	if (with_dev && options.pc_dev_k > 0.0) {
		const int d = search->pv_dev[search->height];
		if (d > options.pc_dev_d0) f *= MAX(options.pc_dev_fmin, 1.0 - options.pc_dev_k * (d - options.pc_dev_d0));
	}
	const double *const mob = options.pc_mob[empties <= 34 ? 0 : empties <= 46 ? 1 : 2];
	if (mob[0] != 0.0 || mob[1] != 0.0 || mob[2] != 0.0) {
		const int n_opp = bit_count(get_moves(search->board.opponent, search->board.player));
		const double m = mob[0] + mob[1] * log((double) MAX(1, n_moves)) + mob[2] * log((double) MAX(1, n_opp));
		f *= MIN(1.25, MAX(0.8, m));
	}
	return f;
}

#ifdef PROBCUT_COUNT
/* search-b: how often the moves off the principal variation change the (whole discs) margin (build with /D PROBCUT_COUNT) */
static void probcut_count_dev(const Search *search, const int depth, const int probcut_depth, const int n_moves, const double t, const int probcut_error)
{
	const int d = MIN(15, search->pv_dev[search->height]);
	const double ft = probcut_factor(search, depth, n_moves, false) * t;
	const double sigma = options.probcut_model ? probcut_sigma(search->eval.n_empties, depth, probcut_depth) : eval_sigma(search->eval.n_empties, depth, probcut_depth);
	const int without = ft * sigma + RCD;
	++statistics.n_probcut_dev_try[d];
	if (without != probcut_error) ++statistics.n_probcut_dev_changed[d];
}
#endif

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
 * @return true if probable cutoff has been found, false otherwise.
 */
static bool search_probcut(Search *search, const int alpha, const int depth, const int n_moves, Node *parent, int *value)
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
		const double t = options.probcut_model ? probcut_refit_t[search->selectivity] : selectivity_table[search->selectivity].t;
		const int saved_selectivity = search->selectivity;
		const NodeType node_type = search->node_type[search->height];

		PROBCUT_STATS(++statistics.n_probcut_try);

		// compute reduced depth & associated error
		probcut_depth = 2 * floor(options.probcut_d * depth) + (depth & 1);
		if (probcut_depth == 0) probcut_depth = depth - 2; 
		assert(probcut_depth > 1 && probcut_depth <= depth - 2 && (probcut_depth & 1) == (depth & 1));
		const bool sloped = options.pc_slope > 1.0 && depth >= search->eval.n_empties && search->eval.n_empties >= options.pc_slope_min;
		const double ft = probcut_factor(search, depth, n_moves, true) * t; // (search-b: t with the default options)
		if (options.probcut_model) probcut_error = ft * probcut_sigma(search->eval.n_empties, depth, probcut_depth) + RCD;	// refit (experimental)
		else probcut_error = ft * eval_sigma(search->eval.n_empties, depth, probcut_depth) + RCD;
		PROBCUT_STATS(probcut_count_dev(search, depth, probcut_depth, n_moves, t, probcut_error);)

		// compute evaluation error (i.e. error at depth 0) averaged for both depths
		eval_score = search_eval_0(search);
		eval_error = ft * 0.5 * (eval_sigma(search->eval.n_empties, depth, 0) + eval_sigma(search->eval.n_empties, depth, probcut_depth)) + RCD;
	
		// try a probable upper cut first
		eval_beta = beta - eval_error;
		probcut_beta = beta + probcut_error;
		if (sloped) {	// (rounded to the side that cuts less; a cut beyond the board stays none)
			eval_beta = (int) ceil(probcut_probe_of(beta)) - eval_error;
			if (probcut_beta < SCORE_MAX) probcut_beta = (int) ceil(probcut_probe_of(probcut_beta) - 1e-6);
		}
		probcut_alpha = probcut_beta - 1;
		if ((eval_score >= eval_beta || !options.pc_gate) && probcut_beta < SCORE_MAX) { 	// check if trying a beta probcut is worth
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
		if (sloped) {
			eval_alpha = (int) floor(probcut_probe_of(alpha)) + eval_error;
			if (probcut_alpha > SCORE_MIN) probcut_alpha = (int) floor(probcut_probe_of(probcut_alpha) + 1e-6);
		}
		if ((eval_score < eval_alpha || !options.pc_gate) && probcut_alpha > SCORE_MIN) { // check if trying an alpha probcut is worth
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
 * @brief Evaluate a midgame position with a Null Window Search algorithm. (No probcut)
 *
 * This function is used when there are still many empty squares on the board. Move
 * ordering, hash table cutoff, enhanced transposition cutoff, etc. are used in
 * order to diminish the size of the tree to analyse, but at the expense of a
 * slower speed.
 * (called from PVS_shallow and NWS_midgame @ depth <= 3)
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
	Eval eval0;
	V2DI board0;
	long long nodes_org;

	if (depth == 2) return search_eval_2(search, alpha, alpha + 1, board_get_moves(&search->board));

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
	board0.board = search->board;
	eval0 = search->eval;

	if (movelist.n_moves > 1) {
		// transposition cutoff
		if (hash_get(hash_table, &search->board, hash_code, &hash_data.data) && search_TC_NWS(&hash_data.data, depth, NO_SELECTIVITY, alpha, &score))
			return score;

		// sort the list of moves
		nodes_org = search->n_nodes;
		movelist_evaluate(&movelist, search, &hash_data.data, alpha, depth);

		// loop over all moves
		bestscore = -SCORE_INF;
		foreach_best_move(move, movelist) {
			search_update_midgame(search, move);
			score = -NWS_shallow(search, ~alpha, depth - 1, hash_table);
			search_restore_midgame(search, move->x, &eval0);
			search->board = board0.board;
			if (score > bestscore) {
				bestscore = score;
				hash_data.data.move[0] = move->x;
				if (score > alpha) break;
			}
		}

		// save the best result in hash tables
		hash_data.data.wl.c.depth = depth;
		hash_data.data.wl.c.selectivity = NO_SELECTIVITY;	// (4.5.1)
		hash_data.data.wl.c.cost = last_bit(search->n_nodes - nodes_org);
		// hash_data.data.move[0] = bestmove;
		hash_data.alpha = alpha;
		hash_data.beta = alpha + 1;
		hash_data.score = bestscore;
		hash_store(hash_table, &search->board, hash_code, &hash_data);

	} else if (movelist.n_moves == 1) {
		move = movelist_first(&movelist);
		search_update_midgame(search, move);
		bestscore = -NWS_shallow(search, ~alpha, depth - 1, hash_table);
		search_restore_midgame(search, move->x, &eval0);
		search->board = board0.board;

	} else { // no moves
		if (can_move(search->board.opponent, search->board.player)) { // pass ?
			search_update_pass_midgame(search, &eval0);
			bestscore = -NWS_shallow(search, ~alpha, depth, hash_table);
			search_restore_pass_midgame(search, &eval0);
		} else { // game-over !
			bestscore = search_solve(search);
		}
	}

	assert(SCORE_MIN <= bestscore && bestscore <= SCORE_MAX);
	return bestscore;
}

/**
 * @brief Evaluate a midgame position at shallow depth. (No probcut)
 *
 * This function is used when there are still many empty squares on the board. Move
 * ordering, hash table cutoff, enhanced transposition cutoff, etc. are used in
 * order to diminish the size of the tree to analyse, but at the expense of a
 * slower speed.
 * (called from guess_move @ depth <= 6 and movelist_evaluate @ depth > 2)
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
	// unsigned long long hash_code;
	HashStoreData hash_data;
	MoveList movelist;
	Move *move;
	Eval eval0;
	Board board0;
	long long nodes_org;

	if (depth == 2) return search_eval_2(search, alpha, beta, board_get_moves(&search->board));

	SEARCH_STATS(++statistics.n_PVS_shallow);
	SEARCH_UPDATE_INTERNAL_NODES(search->n_nodes);

	assert(search->eval.n_empties == bit_count(~(search->board.player | search->board.opponent)));
	assert(SCORE_MIN <= alpha && alpha <= SCORE_MAX);

	// stability cutoff
	if (USE_SC && beta >= PVS_STABILITY_THRESHOLD[search->eval.n_empties]) {
		CUTOFF_STATS(++statistics.n_stability_try;)
		score = SCORE_MAX - 2 * get_stability(search->board.opponent, search->board.player);
		if (score <= alpha) {
			CUTOFF_STATS(++statistics.n_stability_low_cutoff;)
			return score;
		}
		else if (score < beta) beta = score;
	}

	search_get_movelist(search, &movelist);
	board0 = search->board;
	eval0 = search->eval;

	if (movelist.n_moves > 1) {
		// transposition cutoff (unused, normally first searched position)
		// hash_code = board_get_hash_code(&search->board);
		// if (hash_get(&search->shallow_table, &search->board, hash_code, &hash_data.data) && search_TC_PVS(&hash_data.data, depth, NO_SELECTIVITY, &alpha, &beta, &score)) return score;

		// sort the list of moves
		nodes_org = search->n_nodes;
		movelist_evaluate(&movelist, search, &HASH_DATA_INIT, alpha, depth);

		// loop over all moves
		move = movelist_best(&movelist);
		search_update_midgame(search, move);
		bestscore = -PVS_shallow(search, -beta, -alpha, depth - 1);
		hash_data.data.move[0] = move->x;
		search_restore_midgame(search, move->x, &eval0);
		search->board = board0;
		lower = (bestscore > alpha) ? bestscore : alpha;

		while ((bestscore < beta) && (move = move_next_best(move))) {
			search_update_midgame(search, move);
			score = -NWS_shallow(search, ~lower, depth - 1, &search->shallow_table);
			if (lower < score && score < beta)
				lower = score = -PVS_shallow(search, -beta, -lower, depth - 1);
			search_restore_midgame(search, move->x, &eval0);
			search->board = board0;
			if (score > bestscore) {
				bestscore = score;
				hash_data.data.move[0] = move->x;
			}
		}

		// save the best result in shallow hash
		hash_data.data.wl.c.depth = depth;
		hash_data.data.wl.c.selectivity = NO_SELECTIVITY;	// (4.5.1)
		hash_data.data.wl.c.cost = last_bit(search->n_nodes - nodes_org);
		// hash_data.data.move[0] = bestmove;
		hash_data.alpha = alpha;
		hash_data.beta = beta;
		hash_data.score = bestscore;
		hash_store(&search->shallow_table, &search->board, board_get_hash_code(&search->board), &hash_data);

	} else if (movelist.n_moves == 1) {
		move = movelist.move[0].next;
		search_update_midgame(search, move);
		bestscore = -PVS_shallow(search, -beta, -alpha, depth - 1);
		search_restore_midgame(search, move->x, &eval0);
		search->board = board0;

	} else { // no moves
		if (can_move(search->board.opponent, search->board.player)) { // pass ?
			search_update_pass_midgame(search, &eval0);
			bestscore = -PVS_shallow(search, -beta, -alpha, depth);
			search_restore_pass_midgame(search, &eval0);
		} else { // game-over !
			bestscore = search_solve(search);
		}
	}

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
	// const int beta = alpha + 1;
	HashStoreData hash_data;
	MoveList movelist;
	Move *move;
	Node node;
	Eval eval0;
	V2DI board0;
	long long nodes_org;

	assert(search->eval.n_empties == bit_count(~(search->board.player | search->board.opponent)));
	assert((2 <= depth && depth < search->eval.n_empties) || depth == search->eval.n_empties);
	assert(SCORE_MIN <= alpha && alpha <= SCORE_MAX);
	assert(parent != NULL);

	search_check_timeout(search);
	if (search->stop)
		return alpha;

	if (search->eval.n_empties == 0)
		return search_solve_0(search);
	else if (depth < search->eval.n_empties) {
		if (depth <= 3)
			return NWS_shallow(search, alpha, depth, &search->hash_table);
	} else {
		if (depth < DEPTH_MIDGAME_TO_ENDGAME)
			return NWS_endgame(search, alpha);
	}

	SEARCH_STATS(++statistics.n_NWS_midgame);
	SEARCH_UPDATE_INTERNAL_NODES(search->n_nodes);

	// stability cutoff
	if (search_SC_NWS(search, alpha, &score)) return score;

	hash_code = board_get_hash_code(&search->board);
	hash_prefetch(&search->hash_table, hash_code);
	hash_prefetch(&search->pv_table, hash_code);

	nodes_org = search->n_nodes + search->child_nodes;
	search_get_movelist(search, &movelist);

	// transposition cutoff
	if (hash_get(&search->hash_table, &search->board, hash_code, &hash_data.data) || hash_get(&search->pv_table, &search->board, hash_code, &hash_data.data))
		if (search_TC_NWS(&hash_data.data, depth, search->selectivity, alpha, &score)) return score;

	if (movelist_is_empty(&movelist)) { // no moves ?
		node_init(&node, search, alpha, alpha + 1, depth, movelist.n_moves, parent);
		if (can_move(search->board.opponent, search->board.player)) { // pass ?
			search_update_pass_midgame(search, &eval0);
			node.bestscore = -NWS_midgame(search, -node.beta, depth, &node);
			search_restore_pass_midgame(search, &eval0);
		} else { // game-over !
			node.bestscore = search_solve(search);
		}

	} else {
		// probcut
		if (search_probcut(search, alpha, depth, movelist.n_moves, parent, &score)) return score;

		// sort the list of moves
		if (movelist.n_moves > 1) {
			if (hash_data.data.move[0] == NOMOVE) hash_get(&search->hash_table, &search->board, hash_code, &hash_data.data);
			movelist_evaluate(&movelist, search, &hash_data.data, alpha, depth + options.inc_sort_depth[search->node_type[search->height]]);
			movelist_sort(&movelist);
		}

		// ETC
		if (search_ETC_NWS(search, &movelist, hash_code, depth, search->selectivity, alpha, &score)) return score;

		node_init(&node, search, alpha, alpha + 1, depth, movelist.n_moves, parent);

		// loop over all moves
		board0.board = search->board;
		eval0 = search->eval;
		Move *const first_move = node_first_move(&node, &movelist);
		for (move = first_move; move; move = node_next_move(&node)) {
			if (!node_split(&node, move)) {
				search_update_midgame(search, move);
				if (move != first_move) ++search->pv_dev[search->height];
				const int r = options.lmr ? lmr_reduction(search, depth, node_move_rank(&node, move), move->x == hash_data.data.move[0]) : 0;
				if (r) {
					move->score = -NWS_midgame(search, ~alpha, depth - 1 - r, &node);
					if (move->score > alpha && !search->stop) move->score = -NWS_midgame(search, ~alpha, depth - 1, &node);
				} else move->score = -NWS_midgame(search, ~alpha, depth - 1, &node);
				search_restore_midgame(search, move->x, &eval0);
				search->board = board0.board;
				node_update(&node, move);
			}
		}
		node_wait_slaves(&node);
	}

	// save the best result in hash tables
	if (!search->stop) {
		// hack: an endgame node of DEPTH_MIDGAME_TO_ENDGAME is exact, its moves being searched by NWS_endgame; not after
		// a pass, whose search stays here at the same depth and may be cut by probcut: stored as exact, that score was
		// taken by the exact search after a selective one (a wrong score at level 60 from 17 empties on)
		if ((search->eval.n_empties <= depth) ? (depth <= DEPTH_MIDGAME_TO_ENDGAME && !movelist_is_empty(&movelist)) : depth <= 4)
			hash_data.data.wl.c.selectivity = NO_SELECTIVITY;
		else hash_data.data.wl.c.selectivity = search->selectivity;
		hash_data.data.wl.c.depth = depth;
		hash_data.data.wl.c.cost = last_bit(search->n_nodes + search->child_nodes - nodes_org);
		hash_data.data.move[0] = node.bestmove;
		hash_data.alpha = alpha;
		hash_data.beta = alpha + 1;
		hash_data.score = node.bestscore;

		if (search->height <= PV_HASH_HEIGHT) hash_store(&search->pv_table, &search->board, hash_code, &hash_data);
		hash_store(&search->hash_table, &search->board, hash_code, &hash_data);

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
	unsigned long long hash_code;
	HashStoreData hash_data;
	MoveList movelist;
	Move *move;
	Node node;
	Eval eval0;
	Board board0;
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
	else if (USE_PV_EXTENSION && search->eval.n_empties <= search->depth_pv_extension)
		depth = search->eval.n_empties;
	else if (depth == 2 && search->eval.n_empties > 2)
		return search_eval_2(search, alpha, beta, board_get_moves(&search->board));

	nodes_org = search_count_nodes(search);
	SEARCH_UPDATE_INTERNAL_NODES(search->n_nodes);

	search_get_movelist(search, &movelist);
	node_init(&node, search, alpha, beta, depth, movelist.n_moves, parent);
	node.pv_node = true;
	hash_code = board_get_hash_code(&search->board);

	// special cases
	if (movelist_is_empty(&movelist)) {
		if (can_move(search->board.opponent, search->board.player)) {
			search_update_pass_midgame(search, &eval0);
			search->node_type[search->height] = PV_NODE;
			node.bestscore = -PVS_midgame(search, -beta, -alpha, depth, &node);
			search_restore_pass_midgame(search, &eval0);
			node.bestmove = PASS;
		} else {
			node.alpha = -(node.beta = +SCORE_INF);
			node.bestscore = search_solve(search);
			node.bestmove = NOMOVE;
		}

	} else { // normal PVS
		if (movelist.n_moves > 1) {
			//IID
			if (!hash_get(&search->pv_table, &search->board, hash_code, &hash_data.data))
				hash_get(&search->hash_table, &search->board, hash_code, &hash_data.data);

			if (USE_IID && hash_data.data.move[0] == NOMOVE) {	// (unused)
				if (depth == search->eval.n_empties) reduced_depth = depth - ITERATIVE_MIN_EMPTIES;
				else reduced_depth = depth - 2;
				if (reduced_depth >= 3) {
					saved_selectivity = search->selectivity; search->selectivity = 0;
					depth_pv_extension = search->depth_pv_extension;
					search->depth_pv_extension = 0;
					PVS_midgame(search, SCORE_MIN, SCORE_MAX, reduced_depth, parent);
					hash_get(&search->pv_table, &search->board, hash_code, &hash_data.data);
					search->depth_pv_extension = depth_pv_extension;
					search->selectivity = saved_selectivity;
				}
			}

			// Evaluate moves for sorting. For a better ordering, the depth is artificially increased
			movelist_evaluate(&movelist, search, &hash_data.data, node.alpha, depth + options.inc_sort_depth[PV_NODE]);
			movelist_sort(&movelist);
		}

		// first move
		board0 = search->board;
		eval0 = search->eval;
		if ((move = node_first_move(&node, &movelist))) { // why if there ?
			search_update_midgame(search, move); search->node_type[search->height] = PV_NODE;
			move->score = -PVS_midgame(search, -beta, -alpha, depth - 1, &node);
			search_restore_midgame(search, move->x, &eval0);
			search->board = board0;
			node_update(&node, move);

			// other moves : try to refute the first/best one
			while ((move = node_next_move(&node))) {
				if (!node_split(&node, move)) {
					const int alpha = node.alpha;
					search_update_midgame(search, move);
					const int off = (search->height > 1 || options.pc_dev_root); // (height 1: the root of a lazy helper)
					search->pv_dev[search->height] += off;
					move->score = -NWS_midgame(search, -alpha - 1, depth - 1, &node);
					if (!search->stop && alpha < move->score && move->score < beta) {
						search->pv_dev[search->height] -= off; // (searched again as the principal variation)
						search->node_type[search->height] = PV_NODE;
						move->score = -PVS_midgame(search, -beta, -alpha, depth - 1, &node);
					}
					search_restore_midgame(search, move->x, &eval0);
					search->board = board0;
					node_update(&node, move);
				}
			}
			node_wait_slaves(&node);
		}
	}

	// save the best result in hash tables
	if (!search->stop) {
		if (depth <= ((search->eval.n_empties <= depth) ? DEPTH_MIDGAME_TO_ENDGAME : 4))
			hash_data.data.wl.c.selectivity = NO_SELECTIVITY;
		else	hash_data.data.wl.c.selectivity = search->selectivity;
		hash_data.data.wl.c.depth = depth;
		hash_data.data.wl.c.cost = last_bit(search_count_nodes(search) - nodes_org);
		hash_data.data.move[0] = node.bestmove;
		hash_data.alpha = alpha;
		hash_data.beta = beta;
		hash_data.score = node.bestscore;

		hash_store(&search->hash_table, &search->board, hash_code, &hash_data);
		hash_store(&search->pv_table, &search->board, hash_code, &hash_data);

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
