/**
 * @file perft.c
 *
 * @brief Move generator test.
 *
 * @date 1998 - 2026
 * @author Richard Delorme
 * @version 4.5
 */

#include "perft.h"
#include "bit.h"
#include "board.h"
#include "move.h"
#include "util.h"

#include <math.h>

/**
 * Gathered statistiscs
 */
typedef struct {
	unsigned long long n_moves;  /*< move number */
	unsigned long long n_draws;  /*< number of draws */
	unsigned long long n_losses; /*< number of losses */
	unsigned long long n_wins;   /*< number of wins */
	unsigned long long n_passes; /*< number of passes */
	unsigned int min_mobility;   /*< min mobility */
	unsigned int max_mobility;   /*< max mobility */
} GameStatistics;

/** initial statistics*/
const GameStatistics GAME_STATISTICS_INIT = {0ULL, 0ULL, 0ULL, 0ULL, 0ULL, 64, 0};

/**
 * @brief Accumulate statistics: add local data to global ones.
 * @param global Global statistics.
 * @param local Local statistics.
 */
void game_statistics_cumulate(GameStatistics *global, const GameStatistics *local)
{
	global->n_moves += local->n_moves;
	global->n_draws += local->n_draws;
	global->n_losses += local->n_losses;
	global->n_wins += local->n_wins;
	global->n_passes += local->n_passes;
	if (global->min_mobility > local->min_mobility) global->min_mobility = local->min_mobility;
	if (global->max_mobility < local->max_mobility) global->max_mobility = local->max_mobility;
}

/**
 * Return "discs" or "ply" according to the way counts are handle.
 *
 * @param count_pass  Count passes as a move.
 * @return a string with "discs" or "ply".
 */
static const char* ply_string(const bool count_pass) {
	static const char *(s[]) = {"discs", "ply"};
	return s[count_pass];
}

/**
 * Return ply as a halfmove number or a number of discs.
 *
 * @param ply to modify.
 * @param count_pass  Count passes as a move.
 * @return ply or disc count.
 */
static int ply_value(const int ply, const bool count_pass, const int increment) {
	return ply + (count_pass ? 0 : 3 + increment);
}

/**
 * @brief Move generator performance test function.
 *
 * @param board board to count moves from
 * @param depth depth up to count moves
 * @param count_pass Count passes as a move.
 * @param global_stats statistics
 */
static void count_game(const Board *board, const int depth, const bool count_pass, GameStatistics *global_stats)
{
	GameStatistics stats = GAME_STATISTICS_INIT;
	unsigned long long moves;
	int x;
	Board next;

	moves = board_get_moves(board);
	if (depth == 1) {
		stats.n_moves = stats.max_mobility = stats.min_mobility = bit_count(moves);
		if (moves == 0) {
			if (can_move(board->opponent, board->player)) {
				next = *board; board_pass(&next);
				if (can_move(next.player, next.opponent)) {
					stats.n_passes = 1;
					if (!count_pass) {
						stats.n_moves = stats.max_mobility = stats.min_mobility = bit_count(board_get_moves(&next));
					}
				}
			} else {
				const int n_player = bit_count(board->player);
				const int n_opponent = bit_count(board->opponent);
				if (n_player > n_opponent) stats.n_wins = 1;
				else if (n_player == n_opponent) stats.n_draws = 1;
				else stats.n_losses = 1;
			}
		}
	} else {
		if (moves) {
			foreach_bit (x, moves) {
				board_next(board, x, &next);
				count_game(&next, depth - 1, count_pass, &stats);
			}
		} else {
			board_next(board, PASS, &next);
			if (can_move(next.player, next.opponent)) {
				count_game(&next, depth - count_pass, count_pass, &stats);
			}
		}
	}
	game_statistics_cumulate(global_stats, &stats);
}

/**
 * @brief Move generator performance test
 *
 * @param board
 * @param depth
 * @param count_pass Count passes as a move.
 */
void count_games(const Board *init_board, const int depth, const bool count_pass)
{
	int i;
	unsigned long long t, n;
	GameStatistics stats;
	Board board;
	bool is_initial_board;

	board_init(&board);
	is_initial_board = board_equal(&board, init_board);
	board = *init_board;

	board_print(&board, BLACK, stdout);
	printf("\n %s           moves        passes          wins         draws        losses    mobility        time   speed\n", ply_string(count_pass));
	puts("------------------------------------------------------------------------------------------------------------------");
	n = 1;
	for (i = 1; i <= depth; ++i) {
		stats = GAME_STATISTICS_INIT;
		t = -cpu_clock();
		count_game(&board, i, count_pass, &stats);
		if (is_initial_board) {
			if (i == 1) {
				board_next(init_board, F5, &board);
			} else {
				stats.n_moves *= 4;
		 		stats.n_passes *= 4;
				stats.n_wins *= 4;
				stats.n_draws *= 4;
			 	stats.n_losses *= 4;
			}
		}
		t += cpu_clock();
		printf("   %2d, %15llu, %12llu, %12llu, %12llu, %12llu, ", ply_value(i, count_pass, 0), stats.n_moves + count_pass * stats.n_passes, stats.n_passes, stats.n_wins, stats.n_draws, stats.n_losses);
		printf("  %2d - %2d, ", stats.min_mobility, stats.max_mobility);
		n += stats.n_moves + count_pass * stats.n_passes;
		time_print(t, true, stdout);	printf(", ");
		print_scientific(n / (0.001 * t + 0.001), "N/s\n", stdout);
		if (stats.n_moves + stats.n_passes == 0) break;
	}
	printf("Total %12llu\n", n);
	puts("------------------------------------------------------------------------------------------------------------------");
}

/**
 * @brief Move generator test for a single move
 *
 * @param x A move
 * @param board A board
 * @param depth A depth
 * @param count_pass Count passes as a move
 */
static void count_divide(int x, const Board *board, const int depth, const bool count_pass) {
	Board next;
	GameStatistics stats =  GAME_STATISTICS_INIT;
	unsigned long long t, n;
	char s[4];

	t = -cpu_clock();
		board_next(board, x, &next);
		if (depth >= 1) count_game(&next, depth, count_pass, &stats);
	t += cpu_clock();
	printf("%s   ", move_to_string(x, BLACK, s));
	printf("  %2d, %15llu, %12llu, %12llu, %12llu, %12llu, ", ply_value(depth, count_pass, 0), stats.n_moves + stats.n_passes, stats.n_passes, stats.n_wins, stats.n_draws, stats.n_losses);
	printf("  %2d - %2d, ", stats.min_mobility, stats.max_mobility);
	n = stats.n_moves + (count_pass ? stats.n_passes : 0);
	time_print(t, true, stdout);	printf(", ");
	print_scientific(n / (0.001 * t + 0.001), "N/s\n", stdout);
}

/**
 * @brief Move generator debug test
 *
 * @param board
 * @param depth
 * @param count_pass Count passes as a move.
 */
void divide(const Board *board, const int depth, const bool count_pass)
{
	unsigned long long moves;
	int x;

	board_print(board, BLACK, stdout);
	printf("divide %d %d\n", depth, count_pass);
	printf("\nmove %4s           moves        passes          wins         draws        losses    mobility        time   speed\n", ply_string(count_pass));
	puts("------------------------------------------------------------------------------------------------------------------");
	moves = board_get_moves(board);
	if (moves) {
		foreach_bit (x, moves) count_divide(x, board, depth - 1, count_pass);
	} else {
		count_divide(PASS, board, depth - count_pass, count_pass);
	}
	puts("------------------------------------------------------------------------------------------------------------------");
}

/**
 * @brief Estimate move counts from a single game.
 *
 * @param board Board
 * @param depth Depth.
 * @param count_pass Count passes as a move.
 * @param r random generator
 * @param n node counter array.
 */
static void estimate_game(const Board *board, const int depth, const bool count_pass, Random *r, double *n)
{
	unsigned long long moves;
	int x, i, j, k;
	Board next;

	moves = board_get_moves(board);
	i = bit_count(moves);
	if (i == 0 && !can_move(board->opponent, board->player)) {
		n[depth] = 0;
	} else {
		n[depth] = 1;
		if (i == 0) {
			board_next(board, PASS, &next);
			estimate_game(&next, depth + count_pass, count_pass, r, n);
		} else {
			j = random_get(r) % i;
			foreach_bit (x, moves) {
				if (j == 0) {
					board_next(board, x, &next);
					estimate_game(&next, depth + 1, count_pass, r, n);
					for (k = depth; n[k]; ++k) n[k] *= i;
					break;
				} else --j;
			}
		}
	}
}

/**
 * @brief Estimate move counts from a single game.
 *
 * @param board Board
 * @param depth Depth.
 * @param count_pass Count passes as a move.
 * @param r random generator
 * @param n node counter array.
 */
static void estimate_game_6x6(const Board *board, const int depth, const bool count_pass, Random *r, double *n)
{
	unsigned long long moves;
	int x, i, j, k;
	Board next;

	moves = get_moves_6x6(board->player, board->opponent);
	i = bit_count(moves);
	if (i == 0 && !can_move_6x6(board->opponent, board->player)) {
		n[depth] = 0;
	} else {
		n[depth] = 1;
		if (i == 0) {
			board_next(board, PASS, &next);
			estimate_game_6x6(&next, depth + count_pass, count_pass, r, n);
		} else {
			j = random_get(r) % i;
			foreach_bit (x, moves) {
				if (j == 0) {
					board_next(board, x, &next);
					estimate_game_6x6(&next, depth + 1, count_pass, r, n);
					for (k = depth; n[k]; ++k) n[k] *= i;
					break;
				} else --j;
			}
		}
	}
}

/**
 * @brief Move estimate games
 *
 * @param board
 * @param n Number of trials
 * @param count_pass Count passes as a move.
 */
void estimate_games(const Board *board, const long long n, const bool count_pass, const int size)
{
	int i, j;
	unsigned long long t;
	double x[128], m[128], s[128], em[128], es[128], en[128];
	double total_mean = 0.0, total_squared_deviations = 0.0;
	double EM, ES;
	Random r;
	const char *(bool_string[2]) = {"false", "true"};

	random_seed(&r, real_clock());
	EM = ES = 0.0;
	for (i = 0; i < 128; ++i) m[i] = s[i] = 0.0;
	for (i = 0; i < 128; ++i) em[i] = es[i] = en[i] = 0.0;

	board_print(board, BLACK, stdout);
	printf("estimate games: trials %lld, pass %s, size %dx%d\n", n, bool_string[count_pass], size, size);

	t = -cpu_clock();
	for (j = 1; j <= n; ++j) {
		double trial_total = 0.0;
		for (i = 0; i < 128; ++i) x[i] = 0.0;
		if (size == 6)  estimate_game_6x6(board, 1, count_pass, &r, x);
		else estimate_game(board, 1, count_pass, &r, x);
		for (i = 1; x[i]; ++i) {
			m[i] += x[i]; s[i] += x[i] * x[i];
			trial_total += x[i];
		}

		// Each trial contributes one sample to the total and its variance.
		const double delta = trial_total - total_mean;
		total_mean += delta / j;
		total_squared_deviations += delta * (trial_total - total_mean);

		{
			em[i] += x[i - 1]; es[i] += x[i - 1] * x[i - 1];
			EM += x[i - 1]; ES += x[i - 1] * x[i - 1];
			en[i]++;
		}
	}
	t += cpu_clock();

	printf("%5s           moves         accuracy         games         accuracy\n", ply_string(count_pass));
	puts("------------------------------------------------------------------");
	for (i = 1; m[i] || en[i]; ++i) {
		m[i] /= n;
		s[i] = sqrt((s[i] / n - m[i] * m[i])/ (n - 1));
		printf("%5d: %e +/- %e; ", i, m[i], s[i]);

		if (en[i]) {
			em[i] /= n;
			es[i] = sqrt((es[i] / n - em[i] * em[i]) / (n - 1));
			printf("%e +/- %e;", em[i], es[i]);
		}
		putchar('\n');
	}
	EM /= n;
	ES = sqrt((ES / n - EM * EM)/n);
	printf("Total %e +/- %e: %e +/- %e in", total_mean, sqrt(total_squared_deviations) / n, EM, ES);
	time_print(t, false, stdout); printf("\n");
}

/**
 * @brief Estimate move counts from a single game.
 *
 * @param board Board
 * @param ply Ply.
 * @param r random generator.
 * @param move move array.
 * @param max_mobility Highest mobility count.
 * @param max_empties Stage of highest mobility.
 * @param n test counter.
 */

static void test_mobility(const Board *board, const int ply, Random *r, int *move, int *max_mobility, int *max_empties, const unsigned long long n)
{
	unsigned long long moves;
	int x, i, k, e;
	Board next;

	e = board_count_empties(board);

	if (e > *max_mobility) {
		moves = board_get_moves(board);
		if (moves) {
			i = bit_count(moves);
			if (i > *max_mobility || (i == *max_mobility && e > *max_empties)) {
				*max_mobility = i;
				*max_empties = e;
				printf("\n after %llu trials:\n\n", n);
				board_print(board, ply % 2, stdout);
				for (k = 1; k < ply; ++k) {
					move_print(move[k], k % 2, stdout);
					putchar(' ');
				}
				putchar('\n');
			}
			moves &= 0x007e7e7e7e7e7e00ULL;
			if (moves) {
				i = random_get(r) % bit_count(moves);
				foreach_bit (x, moves) {
					if (i-- == 0) {
						move[ply] = x;
						board_next(board, x, &next);
						test_mobility(&next, ply + 1, r, move, max_mobility, max_empties, n);
						break;
					}
				}
			}
		} else {
			if (can_move(board->opponent, board->player)) {
				move[ply] = PASS;
				board_next(board, PASS, &next);
				test_mobility(&next, ply + 1, r, move, max_mobility, max_empties, n);
			}
		}
	}
}

/**
 * @brief Move estimate games
 *
 * @param board
 * @param t time to test
 */
void seek_highest_mobility(const Board *board, const unsigned long long t)
{
	int max_mobility = get_mobility(board->player, board->opponent);
	int max_empties = board_count_empties(board);
	const long long t_max = t * 1000 + cpu_clock();
	int i;
	unsigned long long n = 0;
	const int bucket = 10000;
	int x[128];
	Random r;

	random_seed(&r, real_clock());

	while (cpu_clock() < t_max) {
		i = bucket;
		while (i-- > 0) {
			test_mobility(board, 1, &r, x, &max_mobility, &max_empties, ++n);
		}
	}
}

/**
 * Hash entry;
 */
typedef struct {
	Board board;             /**< board */
	GameStatistics stats;    /**< statistics */
	int depth;               /**< depth */
} GameHash;

/** Hash entry initial value */
const GameHash GAME_HASH_INIT = {{0ULL, 0ULL}, {0ULL, 0ULL, 0ULL, 0ULL, 0ULL, 64, 0}, 0};

/** HashTable */
typedef struct {
	GameHash *array; /**< array of hash entries */
	int size;          /**< size */
	int mask;          /**< mask */
	unsigned long long n_tries; /**< n_tries */
	unsigned long long n_hits;  /**< n_tries */
} GameHashTable;

/**
 * @brief Hash table initialisation
 *
 * @param hash Hash table.
 * @param bitsize Hash table size (as log2(size)).
 */
static void gamehash_init(GameHashTable *hash, int bitsize)
{
	int i;

	hash->size = (1 << bitsize) + 3;
	hash->mask = (1 << bitsize) - 1;
	hash->array = (GameHash*) malloc((hash->size) * sizeof (GameHash));
	if (hash->array == NULL) fatal_error("Cannot allocate perft hashtable.\n");
	for (i = 0; i < hash->size; ++i) hash->array[i] = GAME_HASH_INIT;
	hash->n_tries = hash->n_hits = 0;
}

/**
 * @brief Hash table resource freeing.
 *
 * @param hash Hash table.
 */
static void gamehash_delete(GameHashTable *hash)
{
	free(hash->array);
}

/**
 * @brief Store a game position.
 *
 * @param  hash Hash table.
 * @param b position.
 * @param depth Depth.
 * @param stats position's statistics.
 */
static void gamehash_store(GameHashTable *hash, const Board *b, const int depth, const GameStatistics *stats)
{
	Board u;
	GameHash *i, *j;

	if (depth > 2) {
		board_unique(b, &u);
		i = j = hash->array + (board_get_hash_code(&u) & hash->mask);
		++j; if (i->stats.n_moves > j->stats.n_moves) i = j;
		++j; if (i->stats.n_moves > j->stats.n_moves) i = j;
		++j; if (i->stats.n_moves > j->stats.n_moves) i = j;

		i->board = u;
		i->stats = *stats;
		i->depth = depth;
	}
}

/**
 * @brief Seek for a position in the hash table.
 *
 * @param hash Hash table.
 * @param b position.
 * @param depth Depth.
 * @param stats position's statistics.
 * @return true if no position is found, false otherwise.
 */
static bool gamehash_fail(GameHashTable *hash, const Board *b, const int depth, GameStatistics *stats)
{
	Board u;
	GameHash *i, *j;

	if (depth > 2) {
		board_unique(b, &u);
		j = hash->array + (board_get_hash_code(&u) & hash->mask);
		++hash->n_tries;

		for (i = j; i < j + 4; ++i) {
			if (depth == i->depth && i->board.player == u.player && i->board.opponent == u.opponent) {
				*stats = i->stats;
				++hash->n_hits;
				return false;
			}
		}

	}

	return true;
}


/**
 * @brief Count games recursively.
 *
 * @param hash Hash table.
 * @param board position.
 * @param depth Depth.
 * @param count_pass Count passes as a move.
 * @param global_stats Game's statistics.
 */
static void quick_count_game_6x6(GameHashTable *hash, const Board *board, const int depth, const bool count_pass, GameStatistics *global_stats)
{
	GameStatistics stats = GAME_STATISTICS_INIT;
	unsigned long long moves;
	int x;
	Board next;

	if (depth == 1) {
		moves = get_moves_6x6(board->player, board->opponent);
		stats.n_moves = stats.max_mobility = stats.min_mobility = bit_count(moves);
		if (moves == 0) {
			if (can_move_6x6(board->opponent, board->player)) {
				stats.n_passes = 1;
				if (!count_pass) {
					next = *board; board_pass(&next);
					stats.n_moves = stats.max_mobility = stats.min_mobility = bit_count(board_get_moves(&next));
				}
			} else {
				const int n_player = bit_count(board->player);
				const int n_opponent = bit_count(board->opponent);
				if (n_player > n_opponent) stats.n_wins = 1;
				else if (n_player == n_opponent) stats.n_draws = 1;
				else stats.n_losses = 1;
			}
		}
	} else if (gamehash_fail(hash, board, depth, &stats)) {
		moves = get_moves_6x6(board->player, board->opponent);
		if (moves) {
			foreach_bit (x, moves) {
				board_next(board, x, &next);
				quick_count_game_6x6(hash, &next, depth - 1, count_pass, &stats);
			}
		} else {
			board_next(board, PASS, &next);
			if (can_move_6x6(next.player, next.opponent)) {
				quick_count_game_6x6(hash, &next, depth - count_pass, count_pass, &stats);
			}
		}
		gamehash_store(hash, board, depth, &stats);
	}
	game_statistics_cumulate(global_stats, &stats);
}

/**
 * @brief Count games recursively.
 *
 * @param  hash Hash table.
 * @param board position.
 * @param depth Depth.
 * @param count_pass Count passes as a move.
 * @param global_stats Game's statistics.
 */
static void quick_count_game(GameHashTable *hash, const Board *board, const int depth, const bool count_pass, GameStatistics *global_stats)
{
	GameStatistics stats = GAME_STATISTICS_INIT;
	unsigned long long moves;
	int x;
	Board next;

	if (depth == 1) {
		moves = board_get_moves(board);
		stats.n_moves = stats.max_mobility = stats.min_mobility = bit_count(moves);
		if (moves == 0) {
			if (can_move(board->opponent, board->player)) {
				stats.n_passes = 1;
				if (!count_pass) {
					next = *board; board_pass(&next);
					stats.n_moves = stats.max_mobility = stats.min_mobility = bit_count(board_get_moves(&next));
				}
			} else {
				const int n_player = bit_count(board->player);
				const int n_opponent = bit_count(board->opponent);
				if (n_player > n_opponent) stats.n_wins = 1;
				else if (n_player == n_opponent) stats.n_draws = 1;
				else stats.n_losses = 1;
			}
		}
	} else if (gamehash_fail(hash, board, depth, &stats)) {
		moves = board_get_moves(board);
		if (moves) {
			foreach_bit (x, moves) {
				board_next(board, x, &next);
				quick_count_game(hash, &next, depth - 1, count_pass, &stats);
			}
		} else {
			board_next(board, PASS, &next);
			if (can_move(next.player, next.opponent)) {
				quick_count_game(hash, &next, depth - count_pass, count_pass, &stats);
			}
		}
		gamehash_store(hash, board, depth, &stats);
	}
	game_statistics_cumulate(global_stats, &stats);
}

/**
 * @brief Count games.
 *
 * @param board position.
 * @param depth Depth.
 * @param count_pass Count passes as a move.
 * @param size Size of the board (6 or 8).
 */
void quick_count_games(const Board *board, const int depth, const bool count_pass, const int size)
{
	int i;
	long long t;
	GameHashTable hash;
	GameStatistics stats;
	unsigned long long n;

	board_print(board, BLACK, stdout);
	printf("\n%5s           moves        passes          wins         draws        losses    mobility        time   speed\n", ply_string(count_pass));
	puts("------------------------------------------------------------------------------------------------------------------");
	n = 1;
	for (i = 1; i <= depth; ++i) {
		gamehash_init(&hash, options.hash_table_size);
		stats = GAME_STATISTICS_INIT;
		t = -cpu_clock();
		if (size == 6) quick_count_game_6x6(&hash, board, i, count_pass, &stats);
		else quick_count_game(&hash, board, i, count_pass, &stats);
		t += cpu_clock();
		printf("  %2d, %15llu, %12llu, %12llu, %12llu, %12llu, ", ply_value(i, count_pass, 0), stats.n_moves + stats.n_passes, stats.n_passes, stats.n_wins, stats.n_draws, stats.n_losses);
		printf("  %2d - %2d, ", stats.min_mobility, stats.max_mobility);
		time_print(t, true, stdout);	printf(", ");
		n += stats.n_moves + stats.n_passes;
		print_scientific(n / (0.001 * t + 0.001), "moves/s,", stdout);
		printf("  (h_tries = %lld, h_hits = %lld)\n", hash.n_tries, hash.n_hits);
		gamehash_delete(&hash);
		if (stats.n_moves + stats.n_passes == 0) break;
	}
	printf("Total %12llu\n", n);
	puts("------------------------------------------------------------------------------------------------------------------");
}


/**
 * Compact board 13 bytes
 */
#pragma pack(1)
typedef struct CBoard {
	unsigned char x[13];
} CBoard;
#pragma pack()

static void compact_board(const Board *b, CBoard *c) {
	int i, n, m, x;

	i = n = m = 0;
	for (x = A1; x <= H8; ++x) {
		n = 3 * n + board_get_square_color(b, x);
		if (++m == 5) {
			c->x[i++] = n;
			n = m = 0;
		}
	}
	c->x[i] = n;
}


/**
 * Array of position.
 */
typedef struct PosArray {
	CBoard *item; /**< dynamic array */
	int n;       /**< number of items in the array */
	int size;    /**< capacity of the array */
} PosArray;

/**
 * @brief array initialisation.
 * @param array Array of positions.
 */
static void positionarray_init(PosArray *array)
{
	array->item = NULL;
	array->n = array->size = 0;
}

/**
 * @brief array suppression.
 * @param array Array of positions.
 */
static void positionarray_delete(PosArray *array)
{
	free(array->item);
}

/**
 * @brief Append a position.
 * @param array Array of positions.
 * @param b Position.
 * @return true if a position is added to the array, false if it is already present.
 */
static bool positionarray_append(PosArray *array, const CBoard *b)
{
	int i;
	for (i = 0; i < array->n; ++i) {
		if (memcmp(array->item + i, b, 13) == 0) return false;
	}

	if (array->size < array->n + 1) {
		array->item = (CBoard*) realloc(array->item, (array->size = (array->size + 1)) * sizeof (CBoard));
		if (array->item == NULL) fatal_error("Cannot re-allocate board array.\n");
	}
	array->item[array->n++] = *b;
	return true;
}

/**
 * HashTable of shapes
 */
typedef struct {
	Board *array;
	int size;
	int mask;
} BoardCache;

/**
 * @brief Initialisation of the hash table.
 * @param hash Hash table.
 * @param bitsize Hash table size (as log2(size)).
 */
static void boardcache_init(BoardCache *hash, int bitsize)
{
	int i;

	hash->size = (1 << bitsize) + 3;
	hash->mask = (1 << bitsize) - 1;
	hash->array = (Board *) malloc(hash->size * sizeof (Board));
	if (hash->array == NULL) fatal_error("Cannot re-allocate board array.\n");
	for (i = 0; i < hash->size; ++i) hash->array[i].player = hash->array[i].opponent = -1ULL;
}

/**
 * @brief Free the hash table.
 * @param hash Hash table.
 */
static void boardcache_delete(BoardCache *hash)
{
	free(hash->array);
}

/**
 * @brief Append a unique board to the hash table.
 * @param hash Hash table.
 * @param b Position.
 */
static bool boardcache_undone(BoardCache *hash, const Board *b)
{
	Board u;
	unsigned long long h;
	int i;

	board_unique(b, &u);
	h = board_get_hash_code(&u);
	i = (h & hash->mask);
	if (board_equal(&u, hash->array + i)
	 || board_equal(&u, hash->array + i + 1)
	 || board_equal(&u, hash->array + i + 2)
	 || board_equal(&u, hash->array + i + 3)) return false;

	return true;
}

static void boardcache_append(BoardCache *hash, const Board *b)
{
	Board u;
	unsigned long long h;
	int i, j, k, l;

	board_unique(b, &u);
	h = board_get_hash_code(&u);
	i = (h & hash->mask);
	if (board_equal(&u, hash->array + i)
	 || board_equal(&u, hash->array + i + 1)
	 || board_equal(&u, hash->array + i + 2)
	 || board_equal(&u, hash->array + i + 3)) return;

	l = board_count_empties(hash->array + i); j = i;
	k = board_count_empties(hash->array + ++i); if (k > l) {l = k; j = i;}
	k = board_count_empties(hash->array + ++i); if (k > l) {l = k; j = i;}
	k = board_count_empties(hash->array + ++i); if (k > l) {l = k; j = i;}

	hash->array[j] = u;
}

/**
 * @brief Initialisation of the hash table.
 * @param hash Hash table.
 * @param bitsize Hash table size (as log2(size)).
 */
void positionhash_init(PositionHash *hash, int bitsize)
{
	int i;

	hash->size = (1 << bitsize);
	hash->mask = hash->size - 1;
	hash->array = (PosArray*) malloc(hash->size * sizeof (PosArray));
	if (hash->array == NULL) fatal_error("Cannot re-allocate board array.\n");
	for (i = 0; i < hash->size; ++i) positionarray_init(hash->array + i);
}

/**
 * @brief Free the hash table.
 * @param hash Hash table.
 */
void positionhash_delete(PositionHash *hash)
{
	int i;

	for (i = 0; i < hash->size; ++i) positionarray_delete(hash->array + i);
	free(hash->array);
}

/**
 * @brief Append a position to the hash table.
 * @param hash Hash table.
 * @param b Position.
 * @return true if a position is added to the hash table, false otherwsise.
 */
bool positionhash_append(PositionHash *hash, const Board *b)
{
	Board u;
	CBoard c;
	unsigned long long h;

	board_unique(b, &u);
	compact_board(&u, &c);
	h = board_get_hash_code(&u);
	return positionarray_append(hash->array + (h & hash->mask), &c);
}

/**
 * @brief Recursively count positions.
 * @param hash Hash table with unique positions.
 * @param cache Hash table caching count search.
 * @param board position.
 * @param depth depth.
 */
static unsigned long long count_position(PositionHash *hash, BoardCache *cache, const Board *board, const int depth, const bool count_pass)
{
	unsigned long long nodes = 0;
	unsigned long long moves;
	int x;
	Board next;

	if (boardcache_undone(cache, board)) {
		if (depth == 0) return positionhash_append(hash, board);
		moves = board_get_moves(board);

		if (moves) {
			foreach_bit (x, moves) {
				board_next(board, x, &next);
				nodes += count_position(hash, cache, &next, depth - 1, count_pass);
			}
		} else if (can_move(board->opponent, board->player)) {
			board_next(board, PASS, &next);
			nodes += count_position(hash, cache, &next, depth - count_pass, count_pass);
		}
		boardcache_append(cache, board);
	}

	return nodes;
}

/**
 * @brief Recursively count positions.
 * @param hash Hash table with unique positions.
 * @param cache Hash table caching count search.
 * @param board position.
 * @param depth depth.
 */
static unsigned long long count_position_6x6(PositionHash *hash, BoardCache *cache, const Board *board, const int depth, const bool count_pass)
{
	unsigned long long nodes = 0;
	unsigned long long moves;
	int x;
	Board next;

	if (boardcache_undone(cache, board)) {
		if (depth == 0) return positionhash_append(hash, board);
		moves = get_moves_6x6(board->player, board->opponent);

		if (moves) {
			foreach_bit (x, moves) {
				board_next(board, x, &next);
				nodes += count_position_6x6(hash, cache, &next, depth - 1, count_pass);
			}
		} else if (can_move_6x6(board->opponent, board->player)) {
			board_next(board, PASS, &next);
			nodes += count_position_6x6(hash, cache, &next, depth - count_pass, count_pass);
		}
		boardcache_append(cache, board);
	}

	return nodes;
}

/**
 * @brief Count positions.
 * @param board position.
 * @param depth depth.
 * @param size board_size (8 or 6).
 */
void count_positions(const Board *board, const int depth, const bool count_pass, const int size)
{
	int i;
	unsigned long long n, c;
	long long t;
	PositionHash hash;
	BoardCache cache;

	board_print(board, BLACK, stdout);
	printf("\n %5s       nodes         total            time   speed\n", ply_string(count_pass));
	puts("----------------------------------------------------------");
	c = 0;
	for (i = 1; i <= depth; ++i) {
		positionhash_init(&hash, options.hash_table_size);
		boardcache_init(&cache, options.hash_table_size);
		t = -cpu_clock();
		if (size == 6) c += (n = count_position_6x6(&hash, &cache, board, i, count_pass));
		else c += (n = count_position(&hash, &cache, board, i, count_pass));
		t += cpu_clock();
		printf("  %2d, %12llu, %12llu, ", ply_value(i, count_pass, 1), n, c);
		time_print(t, true, stdout);	printf(", ");
		print_scientific(c / (0.001 * t + 0.001), "N/s\n", stdout);
		positionhash_delete(&hash);
		boardcache_delete(&cache);
	}
	puts("----------------------------------------------------------");
}

/**
 * @brief unique shape.
 *
 * @param shape input Shape.
 * @return
 */
unsigned long long shape_unique(unsigned long long shape)
{
	unsigned long long sym, unique;
	int i;

	unique = shape;
	for (i = 1; i < 8; ++i) {
		sym = shape;
		if (i & 1) sym = horizontal_mirror(sym);
		if (i & 2) sym = vertical_mirror(sym);
		if (i & 4) sym = transpose(sym);
		if (unique > sym) unique = sym;
	}

	return unique;
}

/**
 * Array of shape.
 */
typedef struct {
	unsigned long long *item; /**< dynamic array */
	int n;       /**< number of items in the array */
	int size;    /**< capacity of the array */
} ShapeArray;

/**
 * @brief array initialisation.
 * @param array Array of shapes.
 */
static void shapearray_init(ShapeArray *array)
{
	array->item = NULL;
	array->n = array->size = 0;
}

/**
 * @brief array suppression.
 * @param array Array of shapes.
 */
static void shapearray_delete(ShapeArray *array)
{
	free(array->item);
}

/**
 * @brief Append a shape into the array.
 * @param array Array of shapes.
 * @param shape Shape.
 */
static bool shapearray_append(ShapeArray *array, const unsigned long long shape)
{
	int i;

	for (i = 0; i < array->n; ++i) {
		if (array->item[i] == shape) return false;
	}

	if (array->size == array->n) {
		array->item = (unsigned long long*) realloc(array->item, (array->size += 1) * sizeof (unsigned long long));
		if (array->item == NULL) fatal_error("Cannot re-allocate board array.\n");
	}
	array->item[array->n++] = shape;
	return true;
}


/**
 * HashTable of shapes
 */
typedef struct {
	ShapeArray *array;
	int size;
	int mask;
} ShapeHash;


/**
 * @brief Initialisation of the hash table.
 * @param hash Hash table.
 * @param bitsize Hash table size (as log2(size)).
 */
static void shapehash_init(ShapeHash *hash, int bitsize)
{
	int i;

	hash->size = (1 << bitsize);
	hash->mask = hash->size - 1;
	hash->array = (ShapeArray *) malloc(hash->size * sizeof (ShapeArray));
	if (hash->array == NULL) fatal_error("Cannot re-allocate board array.\n");
	for (i = 0; i < hash->size; ++i) shapearray_init(hash->array + i);
}

/**
 * @brief Free the hash table.
 * @param hash Hash table.
 */
static void shapehash_delete(ShapeHash *hash)
{
	int i;

	for (i = 0; i < hash->size; ++i) shapearray_delete(hash->array + i);
	free(hash->array);
}

/**
 * @brief Append a shape to the hash table.
 * @param hash Hash table.
 * @param b Position.
 */
static bool shapehash_append(ShapeHash *hash, const Board *b)
{
	unsigned long long u;

	u = shape_unique(b->player | b->opponent);
	return shapearray_append(hash->array + (crc32c_u64(0, u) & hash->mask), u);
}

/**
 * @brief Recursively count shapes.
 * @param hash Hash table with unique shapes.
 * @param cache Hash table caching count search.
 * @param board shape.
 * @param depth depth.
 */
static unsigned long long count_shape(ShapeHash *hash, BoardCache *cache, const Board *board, const int depth, const bool count_pass)
{
	unsigned long long nodes = 0;
	unsigned long long moves;
	int x;
	Board next;

	if (boardcache_undone(cache, board)) {
		if (depth == 0) return shapehash_append(hash, board);
		moves = board_get_moves(board);

		if (moves) {
			foreach_bit (x, moves) {
				board_next(board, x, &next);
				nodes += count_shape(hash, cache, &next, depth - 1, count_pass);
			}
		} else {
			board_next(board, PASS, &next);
			if (can_move(next.player, next.opponent)) { // (the board after the pass: up to v4.5.5-nikque.15 the board that has no move)
				nodes += count_shape(hash, cache, &next, depth - count_pass, count_pass);
			}
		}
		boardcache_append(cache, board);
	}

	return nodes;
}

/**
 * @brief Recursively count shapes.
 * @param hash Hash table with unique shapes.
 * @param cache Hash table caching count search.
 * @param board Board.
 * @param depth depth.
 */
static unsigned long long count_shape_6x6(ShapeHash *hash, BoardCache *cache, const Board *board, const int depth, const bool count_pass)
{
	unsigned long long nodes = 0;
	unsigned long long moves;
	int x;
	Board next;

	if (boardcache_undone(cache, board)) {
		if (depth == 0) return shapehash_append(hash, board);
		moves = get_moves_6x6(board->player, board->opponent);

		if (moves) {
			foreach_bit (x, moves) {
				board_next(board, x, &next);
				nodes += count_shape_6x6(hash, cache, &next, depth - 1, count_pass);
			}
		} else {
			board_next(board, PASS, &next);
			if (can_move_6x6(next.player, next.opponent)) { // (the board after the pass: see count_shape)
				nodes += count_shape_6x6(hash, cache, &next, depth - count_pass, count_pass);
			}
		}
		boardcache_append(cache, board);
	}

	return nodes;
}


/**
 * @brief Count shapes.
 * @param board Board.
 * @param depth depth.
 * @param size size (8 or 6).
 */
void count_shapes(const Board *board, const int depth, const bool count_pass, const int size)
{
	int i;
	unsigned long long n, c;
	long long t;
	ShapeHash hash;
	BoardCache cache;

	board_print(board, BLACK, stdout);
	printf("\n %5s       nodes         total            time   speed\n", ply_string(count_pass));
	puts("----------------------------------------------------------");
	c = 0;
	for (i = 0; i <= depth; ++i) {
		shapehash_init(&hash, options.hash_table_size);
		boardcache_init(&cache, options.hash_table_size);
		t = -cpu_clock();
		if (size == 6) c += (n = count_shape_6x6(&hash, &cache, board, i, count_pass));
		else c += (n = count_shape(&hash, &cache, board, i, count_pass));
		t += cpu_clock();
		printf("  %2d, %12llu, %12llu, ", ply_value(i, count_pass, 1), n, c);
		time_print(t, true, stdout);	printf(", ");
		print_scientific(c / (0.001 * t + 0.001), "N/s\n", stdout);
		shapehash_delete(&hash);
		boardcache_delete(&cache);
	}
	puts("----------------------------------------------------------");
}

/**
 * @brief seek a game that reach to a position
 *
 * @param target position seeked
 * @param board starting position.
 * @param line line to reach the target position
 */
bool seek_position(const Board *target, const Board *board, Line *line) {
	const unsigned long long mask = target->opponent | target->player;
	unsigned long long moves;
	int x;
	Board next;

	if (board_equal(board, target)) return true;

	moves = board_get_moves(board);
	if (moves) {
		moves &= mask;
		foreach_bit (x, moves) {
			if (!line_push(line, x)) return false; // (a full line)
			board_next(board, x, &next);
			if (seek_position(target, &next, line)) return true;
			line_pop(line);
		}
	} else {
		board_next(board, PASS, &next);
		if (can_move(next.player, next.opponent)) {
			// the pass is in the line, as in the other lines (up to v4.5.5-nikque.15 it was left out: the moves after it
			// were printed with the colors of the other player)
			if (!line_push(line, PASS)) return false;
			if (seek_position(target, &next, line)) return true;
			line_pop(line);
		}
	}

	return false;
}
