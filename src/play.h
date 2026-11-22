/**
 * @file play.h
 *
 * @brief Edax play control - header file.
 *
 * @date 1998 - 2017
 * @author Richard Delorme
 * @version 4.4
 */


#ifndef EDAX_PLAY_H
#define EDAX_PLAY_H

#include "board.h"
#include "book.h"
#include "search.h"
#include "move.h"
#include "util.h"

/** size of a game record: moves and passes. From any board there are at most 62 moves,
 * and a pass is only recorded before a move: 124 at most (the size was 80, which a game
 * with many passes could exceed) */
#define PLAY_GAME_SIZE 128

/** error message max length */
#define PLAY_MESSAGE_MAX_LENGTH 4096

/** play structure */
typedef struct Play {
	Board board;               /**< current board. */
	Board initial_board;       /**< initial board. */
	Search search;             /**< search. */
	Result result;             /**< search result. */
	Book *book;                /**< opening book */
	int type;                  /**< ui type */
	int player;                /**< current player's color. */
	int initial_player;        /**< initial player's color. */
	Move game[PLAY_GAME_SIZE];             /**< game (move sequence). */
	int i_game;                /**< current move index. */
	int n_game;                /**< last move index. */
	volatile PlayState state;  /**< current state */
	int level;                 /**< search level */
	long long clock;           /**< internal clock */
	struct {
		long long spent;       /**< time spent */
		long long left;        /**< time left */
		long long extra;       /**< extra time left */
	} time[2];                 /**< time of each player */
	struct {
		Board real[PLAY_GAME_SIZE];        /**< forced positions */
		Board unique[PLAY_GAME_SIZE];      /**< unique symetry of the forced positions */
		Move move[PLAY_GAME_SIZE];         /**< forced move sequence */
		int n_move;            /**< number of forced move */
		int i_move;            /**< current forced move */
	} force;                   /**< forced line */
	struct {
		Thread thread;         /**< thread. */
		Lock lock;             /**< lock. */
		Board board;           /**< pondered position */
		bool launched;         /**< launched thread */
		bool verbose;          /**< verbose pondering */
	} ponder;                  /**< pondering thread */
	struct {
		Lock lock;             /**< lock. */
		int n;                 /**< number of such commands running */
	} no_stop;                 /**< commands whose searches give their results to the book or to a game file: play_stop() does not cut them short */
	char error_message[PLAY_MESSAGE_MAX_LENGTH]; /**< error message */
	volatile int nboard_go;    /**< NBoard: number of "go" read and not answered yet (under the lock of the ui event) */
	volatile bool stop_pending; /**< NBoard: a ping came for a "go" not answered yet: its search stops at once */
} Play;

/* functions */
void play_init(Play*, Book*);
void play_free(Play*);
void play_new(Play*);
void play_ggs_init(Play*, const char*);
bool play_load(Play*, const char*);
void play_save(Play*, const char*);
void play_auto_save(Play*);
void play_go(Play*, const bool);
void play_hint(Play*, int);
void play_stop(Play*);
void play_no_stop(Play*, const bool);
void* play_ponder_run(void*);
void play_ponder(Play*);
void* play_ponder_loop(void*);
void play_stop_pondering(Play*);
void play_update(Play*, Move*);
void play_pass(Play*);
void play_undo(Play*);
void play_redo(Play*);
bool play_set_initial_board(Play*, const Board*, const int);
void play_set_board(Play*, const char*);
bool play_set_board_from_FEN(Play*, const char*);
void play_game(Play*, const char*);
bool play_move(Play*, int);
bool play_user_move(Play*, const char*);
Move *play_get_last_move(Play*);
void play_analyze(Play*, int);
void play_book_analyze(Play*, int);
const char* play_get_moves(const Play*, char*);
void play_book_load(Play*, const char*);
void play_book_import(Play*, const char*);
void play_book_merge(Play*, const char*);
void play_learn(Play*);
void play_store(Play*);
int play_learn_games(Play*, const char *const*, const int*, const int, int*);
int play_learn_file(Play*, const char*);
char* play_learn_parse(char*, int*);
void play_adjust_time(Play*, const int, const int);
void play_print(Play*, FILE*);
void play_force_init(Play*, const char*);
void play_force_update(Play*);
void play_force_restore(Play*);
bool play_force_go(Play*, Move*);
void play_symetry(Play*, const int);
const char* play_show_opening_name(Play*, const char *(*opening_get_name)(const Board*));
// bool play_is_game_over(Play*);
// bool play_must_pass(Play *play);
#define	play_is_game_over(play)	board_is_game_over(&(play)->board)
#define	play_must_pass(play)	board_is_pass(&(play)->board)

#endif

