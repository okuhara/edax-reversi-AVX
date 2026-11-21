/**
 * @file options.h
 *
 * Options header.
 *
 * @date 1998 - 2026
 * @author Richard Delorme
 * @version 4.5
 */

#ifndef EDAX_OPTIONS_H
#define EDAX_OPTIONS_H

#include <stdio.h>
#include <stdbool.h>

typedef enum {
	EDAX_FIXED_LEVEL,
	EDAX_TIME_PER_GAME,
	EDAX_TIME_PER_MOVE
} PlayType;

/** options to control various heuristics */
typedef struct {
	int hash_table_size;                  /**< size (in number of bits) of the hash table */

	int inc_sort_depth[3];                /**< increment sorting depth */

	int n_task;                           /**< search in parallel, using n_tasks */
	bool cpu_affinity;                    /**< set one thread per core (logical or physical) to diminish context change */
	bool can_ponder;                      /**< pondering on/off */

	bool all_best;                        /**< search for all best moves when solving problem */

	int verbosity;                        /**< search display */
	int noise;                            /**< search display min depth */
	int width;                            /**< line width */
	bool echo;                            /**< repeat user input */
	bool info;                            /**< info display */
	bool debug_cassio;                    /**< display debug info in cassio's "fenetre de rapport"*/
	bool transgress_cassio;               /**< adapt Cassio requests to search & solve faster */

	long long time;                       /**< time in sec. */
	int level;                            /**< level */
	PlayType play_type;                   /**< game|move-time switch */
	int depth;                            /**< depth (only for testing) */
	int selectivity;                      /**< selectivity (only for testing) */

	int mode;                             /**< mode play (human/edax, etc.) */

	double speed;                         /**< edax speed in N/S (for a more accurate time management) */
	double nps;                           /**< edax assumed speed (for nps based timing */

	int alpha;                            /**< alpha bound */
	int beta;                             /**< beta bound */

	char *eval_file;                      /**< evaluation file */

	char *book_file;                      /**< opening book filename */
	bool book_allowed;                    /**< switch to use or not the opening book*/
	int book_randomness;                  /**< book randomness */

	char *ggs_host;                       /**< ggs host (ip or host name) */
	char *ggs_login;                      /**< ggs login */
	char *ggs_password;                   /**< ggs password */
	char *ggs_port;                       /**< ggs port */
	bool ggs_open;                        /**< ggs open number (set it false for tournaments) */

	bool pv_debug;                        /**< debug PV */
	bool pv_check;                        /**< check PV correctness */
	bool pv_guess;                        /**< guess PV missing moves */

	double probcut_d;

	char *game_file;                      /**< game file */

	char *search_log_file;                /**< log file (for search) */
	char *ui_log_file;                    /**< log file (for user interface) */
	char *ggs_log_file;                   /**< log file (for ggs) */

	char *name;                           /**< program name */

	bool auto_start;                      /**< start a new game after a game is over */
	bool auto_store;                      /**< store a game in a book after each game */
	bool auto_swap;                       /**< change computer's side after each game */
	bool auto_quit;                       /**< quit when game is over */
	int repeat;                           /**< repeat 'n' games (before quitting)*/
	int book_save_interval;               /**< minutes between timed book saves; 0 disables them */
	int book_deviate_save_rounds;          /**< productive deviate rounds between saves; 0 means completion only */
	bool book_merge_auto_save;            /**< save the book to "<book-file>.mrg" after each successful book merge */
	bool hash_table_auto;                 /**< choose hash_table_size from the thread count and the memory size */
	int book_expand_tasks;                /**< book positions expanded at the same time (each search uses n_task / book_expand_tasks threads); 0 = auto */
	bool speed_set;                       /**< speed given by the user; otherwise the time management uses the measured search speed */
	bool level_set;                       /**< level given by the user (with a time control, it caps the search level) */
	int probcut_model;                    /**< probcut error model: 0 = standard, 1 = refit (experimental, see eval.c) */
	int book_depth;                       /**< book depth set at startup (book-depth); 0 = auto: keep the depth of the loaded book */
	int book_store_tasks;                 /**< games learned at the same time by book store/add/learn (each game uses n_task / book_store_tasks threads), and leaf searches of book link done at the same time; 0 = auto (n_task, the default), 1 = one position after the other */
	bool book_store_auto_save;            /**< save the book to "<book-file>.store" after book store and after each group of games of book learn (the default); off: a program that saves the book itself after each call (edax_runner) does not write it twice */
	int book_leaf_recalculate_rounds;     /**< passes of book leaf recalculate (2, 3, 4) at most: a pass that changes no leaf is the last one; 1 = a single pass (the default) */
	bool book_seed_set;                   /**< book-seed given: the random numbers of the book (moves chosen among equal ones) start from book_seed; else from the clock (the default) */
	unsigned long long book_seed;         /**< seed of the random numbers of the book (see book_seed_set) */
	bool book_reproducible;               /**< the book functions give the same book from a run to the next (one-thread searches, expansions applied in the order of the list); off by default */
	bool book_reduce_shrink;              /**< book reduce: fewer buckets when most positions are removed (on by default; off: the book saved is in the order of v4.5.5-nikque.21) */
	bool book_reduce_reuse;               /**< book reduce: the positions that book reducecount found just before are used, the book being the same (on by default) */
	int book_reduce_index;                /**< book reducecount: a position from its number, 0 = binary search of the buckets, 1 = with a table of every 64 numbers (the default) */
	int book_reduce_walk;                 /**< book reducecount with threads: 0 = walk by accumulated loss (the default), 1 = in the order of the groups (the children first) */

	// TODO: add more options?
} Options;

extern Options options;

void options_usage(void);
int options_read(const char*, const char*);
void options_parse(const char*);
void options_parse_defaults(const char*);
void options_bound(void);
int hash_table_size_auto(const int);
void options_free(void);
void options_dump(FILE *f);

void version(void);
void usage(void);

#endif
