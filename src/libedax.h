/**
 * @file libedax.h
 *
 * @brief Edax api (libedax): Edax as a library.
 *
 * The api is the one of libedax by lavox (https://github.com/lavox/edax-reversi), as
 * maintained by sensuikan1973 (https://github.com/sensuikan1973/edax-reversi) and used by
 * libedax4dart: the functions, their arguments and the layout of the structures below are
 * kept, so that a program written for that library works with this one.
 *
 * The structures below are only used to exchange data with the caller: they are not the
 * structures used inside Edax (which differ since Edax 4.5), and are filled from them.
 *
 * This header can be used alone (it does not need the other headers of Edax).
 *
 * @date 2018 - 2026
 * @author lavox (api), sensuikan1973, Nikque (port to Edax 4.5.5)
 */

#ifndef EDAX_LIBEDAX_H
#define EDAX_LIBEDAX_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32)
	#ifdef LIB_BUILD
		#define LIBEDAX_API __declspec(dllexport)
	#else
		#define LIBEDAX_API __declspec(dllimport)
	#endif
#elif defined(__GNUC__)
	#define LIBEDAX_API __attribute__((visibility("default")))
#else
	#define LIBEDAX_API
#endif

/** squares are A1 = 0, B1 = 1, ..., H8 = 63 */
#define LIBEDAX_PASS 64
#define LIBEDAX_NOMOVE 65

/** size of the arrays of moves: index 0 is not a move, and 33 moves at most are returned */
#define LIBEDAX_MOVELIST_SIZE 34

/** size of a sequence of moves */
#define LIBEDAX_LINE_SIZE 80

/** lower limit of edax_book_count_board_bestpath: only the best moves */
#define LIBEDAX_BESTPATH_BEST 128

/** flags of LibedaxPosition */
#define LIBEDAX_FLAG_DONE 1
#define LIBEDAX_FLAG_TODO 2
#define LIBEDAX_FLAG_BESTPATH_BLACK 4

/** board: the discs of the player to move and of its opponent */
typedef struct LibedaxBoard {
	unsigned long long player, opponent;
} LibedaxBoard;

/** move */
typedef struct LibedaxMove {
	unsigned long long flipped;   /**< flipped squares */
	int x;                        /**< square played */
	int score;                    /**< score */
	unsigned int cost;            /**< cost (not used) */
	struct LibedaxMove *next;     /**< next move of the list (from best to worst score) */
} LibedaxMove;

/** list of moves: move[1] to move[n_moves] */
typedef struct LibedaxMoveList {
	LibedaxMove move[LIBEDAX_MOVELIST_SIZE];
	int n_moves;
} LibedaxMoveList;

/** sequence of moves */
typedef struct LibedaxLine {
	char move[LIBEDAX_LINE_SIZE];
	int n_moves;
	int color;
} LibedaxLine;

/** hint */
typedef struct LibedaxHint {
	int depth;                   /**< searched depth (except book moves) */
	int selectivity;             /**< searched selectivity (except book moves) */
	int move;                    /**< best move found */
	int score;                   /**< best score */
	int upper;                   /**< upper score (except book moves) */
	int lower;                   /**< lower score (except book moves) */
	LibedaxLine pv[1];           /**< principal variation */
	long long time;              /**< searched time (except book moves) */
	unsigned long long n_nodes;  /**< searched node count (except book moves) */
	bool book_move;              /**< book move origin */
} LibedaxHint;

/** list of hints: hint[1] to hint[n_hints] */
typedef struct LibedaxHintList {
	LibedaxHint hint[LIBEDAX_MOVELIST_SIZE];
	int n_hints;
} LibedaxHintList;

/** a move of the book with its score */
typedef struct LibedaxLink {
	signed char score;
	unsigned char move;
} LibedaxLink;

/** a position of the book */
typedef struct LibedaxPosition {
	LibedaxBoard board[1];                /**< (unique) board */
	LibedaxLink leaf;                     /**< best remaining move */
	unsigned char flag;                   /**< LIBEDAX_FLAG_... */
	unsigned short n_player_bestpaths;    /**< count of best paths for player */
	unsigned short n_opponent_bestpaths;  /**< count of best paths for opponent */
	LibedaxLink *link;                    /**< linking moves (n_link moves). They stay valid
	                                           until 63 other positions are asked: copy them
	                                           to keep them longer. */
	unsigned int n_wins;                  /**< game win count */
	unsigned int n_draws;                 /**< game draw count */
	unsigned int n_losses;                /**< game loss count */
	unsigned int n_lines;                 /**< unterminated line count */
	struct {
		short value, lower, upper;
	} score;                              /**< position value & bounds */
	unsigned char n_link;                 /**< linking moves number */
	unsigned char level;                  /**< search level */
} LibedaxPosition;

/** general information about the book (see edax_book_info) */
typedef struct LibedaxBook {
	struct {
		short year;
		char month, day;
		char hour, minute, second;
	} date;
	struct {
		int level;
		int n_empties;
		int midgame_error;
		int endcut_error;
		int verbosity;
	} options;
	struct {
		int n_nodes;
		int n_links;
		int n_todo;
	} stats;
	void *array;                  /**< not used (NULL) */
	void *stack;                  /**< not used (NULL) */
	int n;                        /**< number of buckets */
	int n_nodes;                  /**< number of positions */
	bool need_saving;             /**< not used (false) */
	unsigned long long random[1]; /**< not used (0) */
	void *search;                 /**< not used (NULL) */
	unsigned int count_bestpath_stop; /**< not used (0) */
} LibedaxBook;

/** result of edax_bench. NOTE: the lock which followed these fields is not used anymore. */
typedef struct LibedaxBenchResult {
	unsigned long long T;         /**< time (ms) */
	unsigned long long n_nodes;   /**< nodes */
	int positions;                /**< solved positions */
} LibedaxBenchResult;

/*
 * Settings are read from edax.ini, then config.ini (both in the current directory), then
 * from the arguments (argv[0] is ignored), as with the options of the edax program.
 */
LIBEDAX_API void libedax_initialize(int, char**);
LIBEDAX_API void libedax_terminate(void);

/* game */
LIBEDAX_API void edax_init(void);
LIBEDAX_API void edax_new(void);
LIBEDAX_API void edax_load(const char*);
LIBEDAX_API void edax_save(const char*);
LIBEDAX_API void edax_undo(void);
LIBEDAX_API void edax_redo(void);
LIBEDAX_API void edax_mode(const int);
LIBEDAX_API void edax_setboard(const char*);
LIBEDAX_API void edax_setboard_from_obj(const LibedaxBoard*, const int);
LIBEDAX_API void edax_vmirror(void);
LIBEDAX_API void edax_hmirror(void);
LIBEDAX_API void edax_rotate(const int);
LIBEDAX_API void edax_symetry(const int);
LIBEDAX_API void edax_play(char*);
LIBEDAX_API void edax_force(char*);
LIBEDAX_API void edax_bench(LibedaxBenchResult*, int);
LIBEDAX_API void edax_bench_get_result(LibedaxBenchResult*);
LIBEDAX_API void edax_go(void);
LIBEDAX_API void edax_hint(const int, LibedaxHintList*);
LIBEDAX_API void edax_get_bookmove(LibedaxMoveList*);
LIBEDAX_API int edax_get_bookmove_with_position(LibedaxMoveList*, LibedaxPosition*);
LIBEDAX_API int edax_get_bookmove_with_position_by_moves(const char*, LibedaxMoveList*, LibedaxPosition*);
LIBEDAX_API void edax_hint_prepare(LibedaxMoveList*);
LIBEDAX_API void edax_hint_next(LibedaxHint*);
LIBEDAX_API void edax_hint_next_no_multipv_depth(LibedaxHint*);
LIBEDAX_API void edax_stop(void);
LIBEDAX_API void edax_version(void);
LIBEDAX_API int edax_move(const char*);
LIBEDAX_API void edax_options_dump(void);
LIBEDAX_API const char* edax_opening(void);
LIBEDAX_API const char* edax_ouverture(void);

/* opening book */
LIBEDAX_API void edax_book_store(void);
LIBEDAX_API int edax_book_store_tasks(void);              /* nikque: games that edax_book_store_games learns at the same time */
LIBEDAX_API int edax_book_store_games(const char*, char*); /* nikque: play and store several games */
LIBEDAX_API void edax_book_on(void);
LIBEDAX_API void edax_book_off(void);
LIBEDAX_API void edax_book_randomness(const int);
LIBEDAX_API void edax_book_depth(const int);
LIBEDAX_API void edax_book_new(const int, const int);
LIBEDAX_API void edax_book_load(const char*);
LIBEDAX_API void edax_book_save(const char*);
LIBEDAX_API int edax_book_save_checked(const char*);       /* nikque: book save; 1 if the book was saved, 0 otherwise */
LIBEDAX_API int edax_book_failed(void);                    /* nikque: 1 if the last book function could not add a position */
LIBEDAX_API int edax_book_keep_unread_file(const char*);  /* nikque: before a book saved to another file replaces book_file: keep book_file as .damaged if it could not be loaded; 0 if it must not be replaced */
LIBEDAX_API int edax_book_file_status(const char*);       /* nikque: 0 book_file can be replaced, 1 it could not be read (damaged), 2 it could not be opened (held by another program: never replaced) */
LIBEDAX_API void edax_book_import(const char*);
LIBEDAX_API void edax_book_export(const char*);
LIBEDAX_API void edax_book_merge(const char*);
LIBEDAX_API void edax_book_fix(void);
LIBEDAX_API void edax_book_negamax(void);
LIBEDAX_API void edax_book_correct(void);
LIBEDAX_API void edax_book_prune(void);
LIBEDAX_API void edax_book_subtree(void);
LIBEDAX_API void edax_book_stats_clean(void);
LIBEDAX_API void edax_book_show(LibedaxPosition*);
LIBEDAX_API void edax_book_info(LibedaxBook*);
LIBEDAX_API void edax_book_count_bestpath(LibedaxBoard*, LibedaxPosition*);
LIBEDAX_API void edax_book_count_board_bestpath(LibedaxBoard*, LibedaxPosition*, const int, const int, const int);
LIBEDAX_API void edax_book_stop_count_bestpath(void);
LIBEDAX_API void edax_book_verbose(const int);
LIBEDAX_API void edax_book_add(const char*);
LIBEDAX_API void edax_book_check(const char*);
LIBEDAX_API void edax_book_extract(const char*);
LIBEDAX_API void edax_book_deviate(int, int);
LIBEDAX_API void edax_book_enhance(int, int);
LIBEDAX_API void edax_book_fill(int);
LIBEDAX_API void edax_book_play(void);
LIBEDAX_API void edax_book_deepen(void);
LIBEDAX_API void edax_book_feed_hash(void);
LIBEDAX_API void edax_book_add_board_pre_process(void);
LIBEDAX_API void edax_book_add_board_post_process(void);
LIBEDAX_API void edax_book_add_board(const LibedaxBoard*);

/* game database */
LIBEDAX_API void edax_base_problem(const char*, const int, const char*);
LIBEDAX_API void edax_base_tofen(const char*, const int, const char*);
LIBEDAX_API void edax_base_correct(const char*, const int);
LIBEDAX_API void edax_base_complete(const char*);
LIBEDAX_API void edax_base_convert(const char*, const char*);
LIBEDAX_API void edax_base_unique(const char*, const char*);

/* options & state */
LIBEDAX_API void edax_set_option(const char*, const char*);
LIBEDAX_API char* edax_get_moves(char*);
LIBEDAX_API int edax_is_game_over(void);
LIBEDAX_API int edax_can_move(void);
LIBEDAX_API void edax_get_last_move(LibedaxMove*);
LIBEDAX_API void edax_get_board(LibedaxBoard*);
LIBEDAX_API int edax_get_current_player(void);
LIBEDAX_API int edax_get_disc(const int);
LIBEDAX_API int edax_get_mobility_count(const int);
LIBEDAX_API void edax_play_print(void);
LIBEDAX_API void edax_enable_book_verbose(void);
LIBEDAX_API void edax_disable_book_verbose(void);
LIBEDAX_API int edax_board_is_pass(const LibedaxBoard*);
LIBEDAX_API int edax_board_get_square_color(const LibedaxBoard*, const int);

/*
 * functions added by this version (not in the original libedax)
 */
LIBEDAX_API void edax_book_deviate2(int, int);
LIBEDAX_API void edax_book_deviate3(int, int);
LIBEDAX_API long long edax_book_deviate_export(int, int, int, const char*, int); /* book deviate|deviate2|deviate3 <x> <y> export <file> [leaf|ends|all] */
LIBEDAX_API void edax_book_leaf_recalculate(int, int);
LIBEDAX_API void edax_book_leaf_recalculate2(int, int);
LIBEDAX_API void edax_book_leaf_recalculate3(int, int);
LIBEDAX_API void edax_book_leaf_recalculate4(int, int);
LIBEDAX_API void edax_book_fill2(int);                          /* book fill2 <depth> */
LIBEDAX_API void edax_book_fill_limited(int, int, int);         /* book fill <depth> <move loss> <total loss> */
LIBEDAX_API void edax_book_fill2_limited(int, int, int);        /* book fill2 <depth> <move loss> <total loss> */
LIBEDAX_API void edax_book_leaf_expand(void);                   /* book leaf expand */
LIBEDAX_API void edax_book_leaf_expand_limited(int, int);       /* book leaf expand <move loss> <total loss> */
LIBEDAX_API void edax_book_play_to(int);                        /* book play <n> */
LIBEDAX_API void edax_book_extend(void);                        /* book extend */
LIBEDAX_API void edax_book_exhaust(void);                       /* book exhaust */
LIBEDAX_API void edax_book_extract_all(const char*);            /* book extract all <file> */
LIBEDAX_API void edax_base_append(const char*, const char*);    /* base append <input> <output> */
LIBEDAX_API int libedax_cpu_level(void);

/*
 * bit & board utilities (the original libedax exported them from Edax)
 */
#ifndef LIB_BUILD
LIBEDAX_API int bit_count(unsigned long long);
LIBEDAX_API int first_bit(unsigned long long);
LIBEDAX_API int last_bit(unsigned long long);
LIBEDAX_API unsigned long long get_moves(const unsigned long long, const unsigned long long);
LIBEDAX_API bool can_move(const unsigned long long, const unsigned long long);
#endif

#ifdef __cplusplus
}
#endif

#endif /* EDAX_LIBEDAX_H */
