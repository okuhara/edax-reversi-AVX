/**
 * @file play.c
 *
 * Edax play control.
 *
 * @date 1998 - 2020
 * @author Richard Delorme
 * @version 4.4
 */

#include "bit.h"
#include "const.h"
#include "game.h"
#include "move.h"
#include "opening.h"
#include "options.h"
#include "play.h"
#include "settings.h"

#include <assert.h>
#include <ctype.h>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

/**
 * @brief Initialization.
 * @param play Play.
 * @param book Opening book.
 */
void play_init(Play *play, Book *book)
{
	search_init(&play->search);
	play->search.stop_pending = &play->stop_pending; // (a stop before the search began: see search_run)
	play->book = book;
	board_init(&play->initial_board);
	play->search.options.header = " depth|score|       time   |  nodes (N)  |   N/s    | principal variation";
	play->search.options.separator = "------+-----+--------------+-------------+----------+----------------------";
	play->player = play->initial_player = BLACK;
	play->time[0].left = options.time;
	play->time[0].extra = 0;
	play->time[1].left = options.time;
	play->time[1].extra = 0;
	play->state = IS_WAITING; // (play_new() stops the pondering: there is none yet)
	play->ponder.launched = false;
	play_new(play);
	lock_init(&play->ponder);
	lock_init(&play->no_stop);
	play->nboard_go = 0;
	play->stop_pending = false;
	play->no_stop.n = 0;
	spin_init(&play->result);
	play->ponder.verbose = false;
	memset(play->error_message, 0, PLAY_MESSAGE_MAX_LENGTH);
	play_force_init(play, "F5");
}

/**
 * @brief Free resources.
 * @param play Play.
 */
void play_free(Play *play)
{
	play_stop_pondering(play);
	search_free(&play->search);
}

/**
 * @brief Start a new game.
 * @param play Play.
 */
void play_new(Play *play)
{
	// a pondering uses the search that is cleaned here, and only the state IS_PONDERING lets it be stopped
	play_stop_pondering(play);
	play->clock = real_clock();
	play->time[0].spent = play->time[1].spent = 0;
	play->board = play->initial_board;
	play->player = play->initial_player;
	play->ponder.board.player = play->ponder.board.opponent = 0;
	search_cleanup(&play->search);
	play->i_game = play->n_game = 0;
	play->state = IS_WAITING;
	play->result.move = NOMOVE; // missing more initialisation ?
	play->time[0].left = options.time;
	play->time[1].left = options.time;
	play->force.i_move = 0;
}

/**
 * @brief Check a game read from a file before it replaces the current game: its initial board, and its moves
 * (same rules as play_move).
 *
 * @param play Play (its error message is set).
 * @param game Game.
 * @param file File name of the game (for the message).
 * @return true if the game can be played.
 */
static bool play_load_check(Play *play, const Game *game, const char *file)
{
	Board board = game->initial_board;
	int i, player = game->player;
	char move[8];
	Move m;

	if (!board_is_usable(&game->initial_board) || (game->player != BLACK && game->player != WHITE)) {
		snprintf(play->error_message, PLAY_MESSAGE_MAX_LENGTH, "Unusable initial board in %s\n", file);
		return false;
	}
	for (i = 0; i < 60 && game->move[i] != NOMOVE; ++i) {
		if (!can_move(board.player, board.opponent) && can_move(board.opponent, board.player)) {
			m = MOVE_INIT;
			board_get_move_flip(&board, PASS, &m);
			board_update(&board, &m);
			player ^= 1;
		}
		m = MOVE_INIT;
		board_get_move_flip(&board, game->move[i], &m);
		if (!board_check_move(&board, &m)) {
			sprintf(play->error_message, "Illegal move #%d: %s\n", i, move_to_string(game->move[i], player, move));
			return false;
		}
		board_update(&board, &m);
		player ^= 1;
	}
	return true;
}

/**
 * @brief Load a saved game.
 * @param play Play.
 * @param file File name of the game.
 * @return true if game is successfuly loaded.
 */
bool play_load(Play *play, const char *file)
{
	Game game;
	FILE *f;
	int i, l;
	char ext[8];
	void (*load)(Game*, FILE*) = NULL;

	l = strlen(file);
	if (l < 4) {
		snprintf(play->error_message, PLAY_MESSAGE_MAX_LENGTH, "Unknown game format extension: %s\n", file);
		return false;
	}
	strcpy(ext, file + l - 4); string_to_lowercase(ext);
	if (strcmp(ext, ".txt") == 0) load = game_import_text;
	else if (strcmp(ext, ".ggf") == 0) load = game_import_ggf;
	else if (strcmp(ext, ".sgf") == 0) load = game_import_sgf;
	else if (strcmp(ext, ".pgn") == 0) load = game_import_pgn;
	else if (strcmp(ext, ".edx") == 0) load = game_read;
	else {
		sprintf(play->error_message, "Unknown game format extension: %s\n", ext);
		return false;
	}
	f = fopen(file, load == game_read ? "rb" : "r");
	if (f == NULL) {
		snprintf(play->error_message, PLAY_MESSAGE_MAX_LENGTH, "Cannot open file %s\n", file); // the name can be longer than the message
		return false;
	}

	if (load == game_read) {
		if (!game_read_checked(&game, f)) {
			snprintf(play->error_message, PLAY_MESSAGE_MAX_LENGTH, "Incomplete or damaged game file %s\n", file);
			fclose(f);
			return false;
		}
	} else {
		load(&game, f);
	}
	fclose(f);

	// check every move before the current game is replaced
	if (!play_load_check(play, &game, file)) return false;

	play->initial_board = game.initial_board;
	play->initial_player = game.player;
	play_new(play);
	for (i = 0; i < 60 && game.move[i] != NOMOVE; ++i) {
		if (play_must_pass(play)) play_move(play, PASS);
		play_move(play, game.move[i]);
	}

	return true;
}

/**
 * @brief Save a played game.
 * @param play Play.
 * @param file File name of the game.
 */
void play_save(Play *play, const char *file)
{
	Game game;
	FILE *f;
	int i, j, l;
	char ext[8];
	void (*save)(const Game*, FILE*) = NULL;


	game_init(&game);
	game.initial_board = play->initial_board;
	game.player = play->initial_player;
	for (i = j = 0; i < play->n_game; ++i) {
		if (play->game[i].x != PASS && j < 60) { // a Game holds 60 moves
			game.move[j++] = play->game[i].x;
		}
	}

	l = strlen(file);
	if (l < 4) { warn("Unknown game format extension: %s\n", file); return; }
	strcpy(ext, file + l - 4); string_to_lowercase(ext);
	if (strcmp(ext, ".txt") == 0) save = game_export_text;
	else if (strcmp(ext, ".ggf") == 0) save = game_export_ggf;
	else if (strcmp(ext, ".sgf") == 0) save = NULL;
	else if (strcmp(ext, ".pgn") == 0) save = game_export_pgn;
	else if (strcmp(ext, ".eps") == 0) save = game_export_eps;
	else if (strcmp(ext, ".svg") == 0) save = game_export_svg;
	else if (strcmp(ext, ".edx") == 0) save = game_write;
	else { warn("Unknown game format extension: %s\n", ext); return; }
	f = fopen(file, save == game_write ? "wb" : "w");
	if (f == NULL) {
		warn("Cannot open file %s\n", file);
		return;
	}

	if (strcmp(ext, ".sgf") == 0) game_save_sgf(&game, f, true);
	else save(&game, f);

	fclose(f);
}


/**
 * @brief Update the game.
 * @param play Play.
 * @param move Move.
 */
void play_update(Play *play, Move *move)
{
	play_force_update(play);
	board_update(&play->board, move);
	play->game[play->i_game] = *move;
	play->n_game = ++play->i_game;
	play->time[play->player].spent += real_clock() - play->clock;
	play->clock = real_clock();
	play->player ^= 1;
}

#if 0
/**
 * @brief Check if game is over.
 * @param play Play.
 * @return true if game is over.
 */
bool play_is_game_over(Play *play)
{
	return !can_move(play->board.player, play->board.opponent) &&
		!can_move(play->board.opponent, play->board.player);
}

/**
 * @brief Check if player must pass.
 * @param play Play.
 * @return true if player must pass.
 */
bool play_must_pass(Play *play)
{
	return !can_move(play->board.player, play->board.opponent) &&
		can_move(play->board.opponent, play->board.player);
}
#endif

/**
 * @brief Search level used to play a move.
 *
 * With a time control, the level is only a cap on the search: unless a level was given
 * explicitly, do not stop at the default level (21), and use the available time.
 * @return level.
 */
static int play_level(void)
{
	return (options.play_type != EDAX_FIXED_LEVEL && !options.level_set) ? 60 : options.level;
}

/**
 * @brief Give the search the time of the move to play.
 *
 * As before: a time per move, or the game time (shared by the search). With a game time and time-ext 1 (search-b,
 * 2026-11-22), the move has the time of Edax (search_edax_clock()), with no iteration that cannot end in it, and a
 * fail low may stretch it.
 * @param play Play.
 * @param search Search.
 */
static void play_set_search_time(Play *play, Search *search)
{
	if (options.play_type == EDAX_TIME_PER_MOVE) {
		search_set_move_time(search, options.time);
	} else if (options.time_ext && options.play_type == EDAX_TIME_PER_GAME) {
		const long long left = play->time[play->player].left;
		long long t, tmax;
		search_edax_clock(left, board_count_empties(&play->board), search_count_tasks(search), &t, &tmax);
		// (tmax = t: no room to stretch, but still no iteration that cannot end in time)
		search_set_move_time_limit(search, t, tmax);
		search->options.time_left = left;
	} else {
		search_set_game_time(search, play->time[play->player].left);
	}
}

/**
 * @brief Start thinking.
 * @param play Play.
 * @param update Flag to tell if edax should update or no its game.
 */
void play_go(Play *play, const bool update)
{
	extern Log xboard_log[1];

	long long t_real = -real_clock();
	long long t_cpu = -cpu_clock();
	Move move;
	Search *const search = &play->search;
	char s_move[4];

	if (play_is_game_over(play)) return;

	if (play_force_go(play, &move)) {
		play_stop_pondering(play);

		play->result.depth = 0;
		play->result.selectivity = 0;
		play->result.move = move.x;
		play->result.score = 0;
		play->result.book_move = false;
		play->result.time = real_clock() + t_real;
		play->result.n_nodes = 0;
		line_init(&play->result.pv, play->player);
		line_push(&play->result.pv, move.x);

		if (options.verbosity) {
			info("\n[Forced move %s]\n\n",  move_to_string(move.x, play->player, s_move));
		}

	} else if (options.book_allowed && book_get_random_move(play->book, &play->board, &move, options.book_randomness)) {
		play_stop_pondering(play);

		play->result.depth = 0;
		play->result.selectivity = 0;
		play->result.move = move.x;
		play->result.score = move.score;
		play->result.book_move = true;
		play->result.time = real_clock() + t_real;
		play->result.n_nodes = 0;
		line_init(&play->result.pv, play->player);
		book_get_line(play->book, &play->board, &move, &play->result.pv);
		
		if (options.verbosity) {
			info("\n[book move]\n");
			if (options.info) book_show(play->book, &play->board);
			info("\n\n");

			if (play->type == UI_XBOARD) {
				search->observer(&play->result);
			} else {
				if (search->options.header) puts(search->options.header);
				if (search->options.separator) puts(search->options.separator);
				ui_output_lock(); // (whole lines, between the lines of the book analyses)
				printf("book    %+02d                                          ", move.score);
				line_print(&play->result.pv, options.width - 54, " ", stdout);
				putchar('\n');
				ui_output_unlock(false);
				if (search->options.separator) puts(search->options.separator);
			} 
		}
	} else if (play->state == IS_PONDERING && board_equal(&play->board, &play->ponder.board)) {
		play->state = IS_THINKING;

		search->options.verbosity = options.verbosity;
		if (options.verbosity) {
			info("\n[switch from pondering to thinking (id.%d)]\n", play->search.id);
			if (search->options.header) puts(search->options.header);
			if (search->options.separator) puts(search->options.separator);
		}

		play_set_search_time(play, search);

		search_time_reset(search, &play->board);
		if (log_is_open(xboard_log)) {
			 fprintf(xboard_log->f, "edax search> cpu: %d\n", options.n_task);
			 fprintf(xboard_log->f, "edax search> time: spent while pondering = %.2f mini = %.2f; maxi = %.2f; extra = %.2f\n",
				0.001 * search_time(search), 0.001 * search->time.mini,  0.001 * search->time.maxi,  0.001 * search->time.extra);
			 fprintf(xboard_log->f, "edax search> level: %d@%d%%\n",
				search->options.depth, selectivity_table[search->options.selectivity].percent);
		}

		search->observer(search->result);
		thread_join(play->ponder.thread);
		play->ponder.launched = false;
		search->observer(search->result);
				
		play->result = *search->result;
		play->state = IS_WAITING;
		if (!board_get_move_flip(&play->board, search->result->move, &move) && move.x != PASS) {
			fatal_error("bad move found: %s\n", move_to_string(move.x, play->player, s_move));
		}
		if (options.verbosity) {
			if (search->options.separator) puts(search->options.separator);
			info("[stop thinking (id.%d)]\n", play->search.id);
		}

	} else {

		play_stop_pondering(play);

		play->state = IS_THINKING;

		search->options.verbosity = options.verbosity;
		if (options.verbosity) {
			info("\n[start thinking (id.%d)]\n", play->search.id);
			if (search->options.header) puts(search->options.header);
			if (search->options.separator) puts(search->options.separator);
		}
		search_set_board(search, &play->board, play->player);
		search_set_level(search, play_level(), search->eval.n_empties);
		play_set_search_time(play, search);

		search_time_init(search); // redondant, 
		if (log_is_open(xboard_log)) {
			 fprintf(xboard_log->f, "edax search> cpu: %d\n", options.n_task);
			 fprintf(xboard_log->f, "edax search> time: left = %.2f mini = %.2f; maxi = %.2f; extra = %.2f\n",
				0.001 * play->time[play->player].left, 0.001 * search->time.mini,  0.001 * search->time.maxi,  0.001 * search->time.extra);
			 fprintf(xboard_log->f, "edax search> level: %d@%d%%\n",
				search->options.depth, selectivity_table[search->options.selectivity].percent);
		}
		
		search_run(search);
		play->result = *search->result;
		play->state = IS_WAITING;
		if (!board_get_move_flip(&play->board, search->result->move, &move) && move.x != PASS) {
			fatal_error("bad move found: %s\n", move_to_string(move.x, play->player, s_move));
		}
		if (options.verbosity) {
			if (search->options.separator) puts(search->options.separator);
			info("[stop thinking] (id.%d)]\n", play->search.id);
		}
	}

	t_real += real_clock() + 1;
	t_cpu += cpu_clock() + 1;
	info("[cpu usage: %.2f%%]\n", 100.0 * t_cpu / t_real);

	if (options.nps > 0) t_real = play->result.time;	// virtual clock (node count / nps), as used by the search
	if (options.play_type != EDAX_TIME_PER_MOVE) play->time[play->player].left -= t_real;

	if (update) play_update(play, &move);

}

/**
 * @brief Start thinking.
 *
 * Evaluate first best moves of the position.
 *
 * @param play Play.
 * @param n Number of (best) moves to evaluate.
 */
void play_hint(Play *play, int n)
{
	Line pv;
	Move *m;
	Search *const search = &play->search;
	MoveList book_moves;
	GameStats stat;
	Board b;

	if (play_is_game_over(play)) return;

	play_stop_pondering(play);

	play->state = IS_THINKING;

	search->options.verbosity = options.verbosity;
	if (options.verbosity) {
		info("\n[start thinking]\n");
		if (search->options.header) puts(search->options.header);
		if (search->options.separator) puts(search->options.separator);
	}
	search_set_board(search, &play->board, play->player);
	search_set_level(search, options.level, search->eval.n_empties);
	if (n > search->movelist.n_moves) n = search->movelist.n_moves;
	info("<hint %d moves>\n", n);

	if (options.book_allowed && book_get_moves(play->book, &play->board, &book_moves)) {
		foreach_move (m, book_moves) if (n) {
			--n;
			line_init(&pv, play->player);
			book_get_line(play->book, &play->board, m, &pv);
			movelist_exclude(&search->movelist, m->x);
			if (play->type == UI_NBOARD) {
				board_next(&play->board, m->x, &b);
				book_get_game_stats(play->book, &b, &stat);
				printf("book "); line_print(&pv, 10, NULL, stdout);
				printf(" %d %llu %d\n", m->score, stat.n_lines, play->book->options.level);
			} else {
				ui_output_lock(); // (whole lines, between the lines of the book analyses)
				printf("book    %+02d                                          ", m->score);
				line_print(&pv, options.width - 54, " ", stdout); putchar('\n');
				ui_output_unlock(false);
			}
		}
	}

	while (n--) {
		if (options.play_type == EDAX_TIME_PER_MOVE) search_set_move_time(search, options.time);
		else search_set_game_time(search, play->time[play->player].left);
		if (n) search->options.multipv_depth = 60;
		search_run(search);
		search->options.multipv_depth = MULTIPV_DEPTH;
		if (play->type == UI_NBOARD) {
			printf("search "); line_print(&search->result->pv, 10, NULL, stdout);
			printf(" %d 0 %d\n", search->result->score, search->result->depth);
		} else {
			if (options.verbosity == 0) search->observer(search->result);
		}
		if (search->stop != STOP_END) break;
		movelist_exclude(&search->movelist, search->result->move);
	}
	if (options.verbosity) {
		info("\n[stop thinking]\n");
		if (search->options.separator) puts(search->options.separator);
	}
}

/**
 * @brief do ponderation.
 *
 * Ponderation (thinking during opponent time) is done within a thread.
 * The thread is launched at startup and immediately suspended, thanks to
 * condition_wait. When edax is required to ponder, the thread is activated
 * by a condition_signal, and the search start. To stop the ponderation,
 * just stop the search and wait for the lock to be release.
 *
 * @param v the play.
 * @return NULL (unused).
 */
void* play_ponder_run(void *v)
{
	extern Log xboard_log[1];
	Play *const play = (Play*) v;
	int player;
	Search *search = &play->search;
	Board board;
	Move move;
	char m[4];

	lock(&play->ponder);
	if (play->state == IS_PONDERING || play->state == IS_ANALYZING) {
		board = play->board;
		player = play->player;
		search_set_game_time(search, TIME_MAX);
		search->options.keep_date = (play->state == IS_PONDERING && search->pv_table.date > 0);
		search->options.ponder_timed = (play->state == IS_PONDERING && options.play_type != EDAX_FIXED_LEVEL);
		if (play->ponder.verbose) search->options.verbosity = options.verbosity;
		else search->options.verbosity = 0;

		move.x = search_guess(search, &board);

		// guess opponent move and start the search
		if (play->state == IS_PONDERING && move.x != NOMOVE) {
			board_get_move_flip(&board, move.x, &move);

			board_update(&board, &move);
				play->ponder.board = board;
				search_set_board(search, &board, player ^ 1);
				search_set_level(search, play_level(), search->eval.n_empties);
				search_run(search);
				if (options.info && play->state == IS_PONDERING) {
					printf("[ponder after %s id.%d: ", move_to_string(move.x, player, m), search->id);
					result_print(search->result, stdout);
					printf("]\n");
				}
			board_restore(&board, &move);
		} else {
			play->ponder.board = board;
			search_set_board(search, &board, player);
			search_set_ponder_level(search, play_level(), search->eval.n_empties);
			log_print(xboard_log, "edax (ponder)> start search\n");
			search_run(search);
			log_print(xboard_log, "edax (ponder)> search ended\n");
			if (options.info && play->state == IS_PONDERING) {
				printf("[ponder (without move) id.%d: ", search->id);
				result_print(search->result, stdout);
				printf("]\n");
			}
		}

		info("[ponderation finished]\n");
		play->state = IS_WAITING;
		search->options.keep_date = false;
		search->options.ponder_timed = false;
	}
	unlock(&play->ponder);

	return NULL;
}

/**
 * @brief Ponder.
 *
 * Think during opponent time. Activate the thread suspended in
 * play_ponder_loop.
 *
 * @param play Play.
 */
void play_ponder(Play *play)
{
	if (play_is_game_over(play)) return;
	if (options.can_ponder && play->state == IS_WAITING) {
		play->ponder.board.player = play->ponder.board.opponent = 0;
		play->stop_pending = false; // (a stop was for the command that ends)
		play->state = IS_PONDERING;
		info("\n[start ponderation]\n");
		if (thread_create(&play->ponder.thread, play_ponder_run, play)) play->ponder.launched = true;
		else play->state = IS_WAITING; // no thread (memory exhausted): no pondering (its end was waited for, for ever)
	}
}

/**
 * @brief Stop pondering.
 *
 * If edax is pondering, stop the search, and wait that the pondering thread
 * is suspended.
 *
 * @param play Play.
 */
void play_stop_pondering(Play *play)
{
	while (play->state == IS_PONDERING) {
		info("[stop pondering]\n");
		search_stop_all(&play->search, STOP_PONDERING);
		relax(10);
	}

	if (play->ponder.launched) {
		info("[joining thread]\n");
		thread_join(play->ponder.thread);
		play->ponder.launched = false;
		info("[thread joined]\n");
	}
}

/**
 * @brief Stop thinking.
 * @param play Play.
 */
void play_stop(Play *play)
{
	bool stopped = false;

	// A book or base command, or the store of a game, does not look at the stop: only the search that is
	// running would be cut short, and its unfinished result would go to the book or to the game file
	// (up to v4.5.5-nikque.12 it did). The stop is checked and done under the lock that sets the state.
	lock(&play->no_stop);
	if (play->no_stop.n == 0) {
		search_stop_all(&play->search, STOP_ON_DEMAND);
		stopped = true;
	}
	unlock(&play->no_stop);
	if (stopped) info("[stop on user demand]\n");
	else fprintf(stderr, "[stop: nothing is stopped while a book or base command is running]\n"); // (stderr: the standard output is the channel of the protocols)
}

/**
 * @brief Tell that a command whose searches give their results to the book or to a game file starts or ends.
 *
 * While such a command runs, play_stop() does not stop the search.
 *
 * @param play Play.
 * @param on true when the command starts, false when it ends.
 */
void play_no_stop(Play *play, const bool on)
{
	lock(&play->no_stop);
	play->no_stop.n += (on ? 1 : -1);
	if (on) play->stop_pending = false; // (its searches give their results to the book: they are not cut short)
	unlock(&play->no_stop);
}

/**
 * @brief Undo a move.
 * @param play Play.
 */
void play_undo(Play *play)
{
	if (play->i_game > 0) {
		play_stop_pondering(play);
		lock(&play->ponder);
		play_force_restore(play);
		board_restore(&play->board, &play->game[--play->i_game]);
		play->player ^= 1;
		unlock(&play->ponder);
	}
}

/**
 * @brief Redo a move.
 * @param play Play.
 */
void play_redo(Play *play)
{
	if (play->i_game < play->n_game) {
		play_stop_pondering(play);
		play_force_update(play);
		board_update(&play->board, &play->game[play->i_game++]);
		play->player ^= 1;
	}
}

/**
 * @brief Start a new game from a board given from outside (setboard, FEN, libedax).
 *
 * A board of fewer than 4 discs, or with discs on the same square, is refused: the current game is kept
 * (up to v4.5.5-nikque.15 it was searched with more than 60 empties, out of the table of the levels).
 *
 * @param play Play.
 * @param board The initial board.
 * @param player The player to move.
 * @return false if the board is refused.
 */
bool play_set_initial_board(Play *play, const Board *board, const int player)
{
	if (!board_is_usable(board)) {
		warn("setboard: a board needs at least 4 discs\n");
		return false;
	}
	play->initial_board = *board;
	play->initial_player = player;
	play_force_init(play, "");
	play_new(play);
	return true;
}

/**
 * @brief Set a new board.
 * @param play Play.
 * @param board A new board.
 */
void play_set_board(Play *play, const char *board)
{
	Board b;
	int player;

	play_stop_pondering(play);
	player = board_set(&b, board);
	if (player == EMPTY) { /* bad board ? */
		b.opponent &= (~b.player);
		player = (board_count_empties(&b) & 1);
		if (player == WHITE) board_swap_players(&b);
	}
	play_set_initial_board(play, &b, player);
}

/**
 * @brief Set a new board.
 * @param play Play.
 * @param board A new board.
 * @return false if the FEN is refused (the current game is kept).
 */
bool play_set_board_from_FEN(Play *play, const char *board)
{
	Board b;
	int player;

	play_stop_pondering(play);
	// a FEN that is refused leaves the current game as it is (up to v4.5.5-nikque.15 the initial board was
	// changed, and the initial player became EMPTY)
	player = board_from_FEN(&b, board);
	return player != EMPTY && play_set_initial_board(play, &b, player);
}

/**
 * @brief Play a move sequence.
 * @param play Play.
 * @param string move sequence.
 */
void play_game(Play *play, const char *string)
{
	Move move;
	const char *next;

	play_stop_pondering(play);

	// convert an opening name to a move sequence...
	next = opening_get_line(string);
	if (next) string = next;	

	while ((next = parse_move(string, &play->board, &move)) != string || move.x == PASS) {
		string = next;
		play_update(play, &move);
	}
}

/**
 * @brief Play a move.
 * @param play Play.
 * @param x Coordinate played.
 * @return true if the move has been legally played.
 */
bool play_move(Play *play, int x)
{
	Move move;

	move = MOVE_INIT;
	board_get_move_flip(&play->board, x, &move);
	if (board_check_move(&play->board, &move)) {
		play_update(play, &move);
		return true;
	} else {
		return false;
	}
}

/**
 * @brief Play a user move.
 * @param play Play.
 * @param string Move as a string.
 * @return true if the move has been legally played.
 */
bool play_user_move(Play *play, const char *string)
{
	Move move;

	if (parse_move(string, &play->board, &move) != string) {
		play_update(play, &move);
		return true;
	} else {
		return false;
	}
}


/**
 * @brief Get the last played move.
 * @param play Play.
 * @return last played move.
 */
Move* play_get_last_move(Play *play)
{
	return play->i_game ? play->game + play->i_game - 1 : NULL;
}


/**
 * @brief Seek for the best alternative move.
 *
 * @param play Play.
 * @param played Last played move.
 * @param alternative Second best move.
 * @param depth Depth searched.
 * @param percent Probcut selectivity searched.
 * @return The number of alternatives.
 */
static int play_alternative(Play *play, Move *played, Move *alternative, int *depth, int *percent)
{
	Search *const search = &play->search;
	Result *const result = search->result;
	Board excluded, board, unique;
	Move *move;
	unsigned long long hash_code;

	search_set_board(search, &play->board, play->player);
	if (A1 <= played->x && played->x <= H8) {
		movelist_exclude(&search->movelist, played->x);
		hash_code = board_get_hash_code(&search->board);
		hash_exclude_move(&search->pv_table, &search->board, hash_code, played->x);
		hash_exclude_move(&search->hash_table, &search->board, hash_code, played->x);
		// also remove moves leading to symetrical positions
		board_next(&play->board, played->x, &board);
		board_unique(&board, &excluded);
		foreach_move (move, search->movelist) {
			board_next(&play->board, move->x, &board);
			board_unique(&board, &unique);
			if (board_equal(&excluded, &unique)) {
				hash_code = board_get_hash_code(&search->board);
				hash_exclude_move(&search->pv_table, &search->board, hash_code, move->x);
				hash_exclude_move(&search->hash_table, &search->board, hash_code, move->x);
				move = movelist_exclude(&search->movelist, move->x);
			}
		}
	}
	if (search->movelist.n_moves >= 1 || played->x == NOMOVE) {
		search_set_level(search, options.level, search->eval.n_empties);
		search->options.verbosity = 0;
		search_run(search);
		search->options.verbosity = options.verbosity;
		alternative->x = result->move;
		alternative->score = result->score;
		*depth = result->depth;
		*percent = selectivity_table[result->selectivity].percent;
	}
	return search->movelist.n_moves;
}

/**
 * @brief Write a line if an analysis.
 *
 * Write a line of a post-mortem game analysis.
 *
 * @param play Play.
 * @param m Move actually played.
 * @param a Best alternative move.
 * @param n_moves Number of alternatives.
 * @param depth Analysis depth.
 * @param percent Analysis probcut selectivity (as a percent).
 * @param f Output stream.
 */
static void play_write_analysis(Play *play, const Move *m, const Move *a, const int n_moves, const int depth, const int percent, FILE *f)
{
	char s[4];

	if (n_moves >= 0) {
		int n_empties = board_count_empties(&play->board);
		fprintf(f, "%3d ", 61 - n_empties);
		if (depth == -1) fputs("  book  ", f);
		else {
			fprintf(f, " %3d", depth);
			if (percent < 100) fprintf(f, "@%2d%%", percent);
			else fputs("    ", f);
		};
		fprintf(f, "%3d   ", n_moves);
		fprintf(f, " %s    %+3d  ", move_to_string(m->x, play->player, s), m->score);
		if (n_moves > 0) {
			if (m->score > a->score) fputs(" > ", f);
			else if (m->score == a->score) fputs(" = ", f);
			else fputs( " < ", f);
			fprintf(f, "  %+3d     %s ", a->score, move_to_string(a->x, play->player, s));
			if (m->score < a->score) {
				if (depth == n_empties) {
					if (percent == 100) fputs("<- Mistake", f);
					else fputs("<- Possible mistake", f);
				} else {
					fputs("<- Edax disagrees", f);
					if (a->score - m->score > 4) fputs(" strongly", f);
				}
			}
		}
		putc('\n', f);
	}
}

/**
 * @brief Analyze a played game.
 * @param play Play.
 * @param n number of moves to analyze.
 */
void play_analyze(Play *play, int n)
{
	int i, score, n_alternatives;
	int depth = 0, percent = 0, n_empties;
	Move *move, alternative;
	int n_exact[2] = {0, 0}, n_eval[2] = {0, 0};
	int n_error[2] = {0, 0}, n_rejection[2] = {0, 0};
	int disc_error[2] = {0, 0}, disc_rejection[2] = {0, 0};
	const char *clr = "                                                                              \r";
	

	play_stop_pondering(play);

	puts("\n              N     played        alternative");
	puts("ply  level   alt. move  score     score   move");
	puts("---+-------+-----+-----------+--+---------------");

	search_cleanup(&play->search);
	search_set_board(&play->search, &play->board, play->player);
	alternative.x = NOMOVE;
	play_alternative(play, &alternative, &alternative, &depth, &percent);
	score = alternative.score;

	for (i = play->i_game - 1; i >= 0 && i >= play->i_game - n; --i) {
		move = play->game + i;
		move->score = -score;
		play->player ^= 1;
		board_restore(&play->board, move);
		if (move->x == PASS) ++n;

		n_empties = board_count_empties(&play->board);
		n_alternatives = play_alternative(play, move, &alternative, &depth, &percent);
		if (options.verbosity == 1) fputs(clr, stdout);
		play_write_analysis(play, move, &alternative, n_alternatives, depth, percent, stdout);

		score = move->score; 
		if (n_alternatives > 0) {
			if (depth == n_empties && percent == 100) ++n_exact[play->player]; else ++n_eval[play->player];
			if (alternative.score > score) {
				if (depth == n_empties && percent == 100) {
					++n_error[play->player];
					disc_error[play->player] += alternative.score - score;
				} else {
					++n_rejection[play->player];
					disc_rejection[play->player] += alternative.score - score;
				}
				score = alternative.score;
			}
		}
		if (play->search.stop == STOP_ON_DEMAND) break;
	}
	puts("\n      | rejections : discs | errors    : discs | error rate |");
	{
		static const char *const name[2] = {"Black", "White"};
		int c;
		for (c = BLACK; c <= WHITE; ++c) {
			printf("%s | %3d / %3d  :  %+4d | %3d / %3d :  %+4d |      ",
				name[c], n_rejection[c], n_eval[c], disc_rejection[c], n_error[c], n_exact[c], disc_error[c]);
			if (n_exact[c] > 0) printf("%5.3f |\n", 1.0 * disc_error[c] / n_exact[c]);
			else puts("    - |"); // no move was checked by an exact search (up to v4.5.5-nikque.12: a division by zero, printed as "nan")
		}
	}

	if (i < 0 || i < play->i_game - n) ++i;
	for (; i < play->i_game; ++i) {
		board_update(&play->board, play->game + i);
		play->player ^= 1;
	}
}

/**
 * @brief Seek for the best alternative move from the opening book.
 *
 * @param play Play.
 * @param played Last played move.
 * @param alternative Second best move.
 * @return The number of alternatives.
 */
static int play_book_alternative(Play *play, Move *played, Move* alternative)
{
	MoveList movelist;
	Move *exclude, *best;
	Board excluded, board, unique;
	Move *move;

	if (book_get_moves(play->book, &play->board, &movelist)) {
		exclude = movelist_exclude(&movelist, played->x);

		if (exclude && exclude->x == played->x) {
			played->score = exclude->score;
			// also remove moves leading to symetrical positions
			board_next(&play->board, played->x, &board);
			board_unique(&board, &excluded);
			foreach_move (move, movelist) {
				board_next(&play->board, move->x, &board);
				board_unique(&board, &unique);
				if (board_equal(&excluded, &unique)) move = movelist_exclude(&movelist, move->x);
			}
			if (movelist.n_moves) {
				best = movelist_best(&movelist);
				*alternative = *best;
			}
			return movelist.n_moves;
		}
	}
	return -1;
}

/**
 * @brief Analyze a played game.
 * @param play Play.
 * @param n number of moves to analyze.
 */
void play_book_analyze(Play *play, int n)
{
	int i, n_alternatives;
	Move *move, alternative;

	play_stop_pondering(play);

	puts("\n              N     played        alternative");
	puts("ply  level   alt. move  score     score   move");
	puts("---+-------+-----+-----------+--+---------------");

	for (i = play->i_game - 1; i >= 0 && i >= play->i_game - n; --i) {
		move = play->game + i;
		play->player ^= 1;
		board_restore(&play->board, move);
		if (move->x == PASS) ++n;

		n_alternatives = play_book_alternative(play, move, &alternative);
		play_write_analysis(play, move, &alternative, n_alternatives, -1, 100, stdout);

		if (play->search.stop == STOP_ON_DEMAND) break;
	}

	if (i < 0 || i < play->i_game - n) ++i;
	for (; i < play->i_game; ++i) {
		board_update(&play->board, play->game + i);
		play->player ^= 1;
	}
}

/**
 * @brief Moves of the game up to the current position, passes left out ("f5d6c3...").
 * @param play Play.
 * @param moves Moves (out, 2 * PLAY_GAME_SIZE + 1 characters).
 * @return moves.
 */
const char* play_get_moves(const Play *play, char *moves)
{
	int i, n = 0;
	for (i = 0; i < play->i_game && i < PLAY_GAME_SIZE; ++i) {
		const int x = play->game[i].x;
		if (A1 <= x && x <= H8) {
			moves[n++] = (char) ('a' + x % 8);
			moves[n++] = (char) ('1' + x / 8);
		}
	}
	moves[n] = '\0';
	return moves;
}

/**
 * @brief Load an opening book (book load command): the current book is kept when the file cannot be loaded.
 *
 * @param play Play.
 * @param file Book file name (NULL: no file).
 * @return true if the book of the file was loaded (up to v4.5.5-nikque.33 nothing told it).
 */
bool play_book_load(Play *play, const char *file)
{
	Book *book = play->book;
	Book next = {0};
	FILE *exists;

	next.search = book->search;
	// (book_load() makes a new book when the file cannot be opened, and says "New book ...": the file is
	// opened first, for no such line about a book that would be dropped at once)
	if (file == NULL || (exists = fopen(file, "rb")) == NULL) {
		warn("Book %s was not loaded; current book retained\n", file ? file : "(no name)");
	} else if (fclose(exists), book_load(&next, file)) {
		book_free(book);
		*book = next;
		book_set_loaded_file(file, false); // the book file may still hold the previous book: see book_save_to_book_file()
		return true;
	} else {
		book_free(&next);
		warn("Book %s was not loaded; current book retained\n", file);
	}
	return false;
}

/**
 * @brief Import an opening book (text format; book import command): as book load, the current book is kept
 * when nothing is imported.
 *
 * @param play Play.
 * @param file File name.
 */
void play_book_import(Play *play, const char *file)
{
	Book *book = play->book;
	Book next = {0};

	next.search = book->search;
	if (book_import(&next, file)) {
		book_free(book);
		*book = next;
		book_set_loaded_file(file, true); // the book file still holds the previous book: see book_save_to_book_file()
		book_link(book);
		book_fix(book);
		book_negamax(book);
		book_sort(book);
	} else {
		warn("Book %s was not imported; current book retained\n", file);
	}
}

/**
 * @brief Merge an opening book to the current one (book merge command).
 *
 * @param play Play.
 * @param file File name of the book to merge.
 */
void play_book_merge(Play *play, const char *file)
{
	Book *book = play->book;
	char merged_file[FILENAME_MAX];

	if (book_merge_file(book, file)) { // the source book is streamed, not loaded
		book_link_parallel(book); // rebuild links before validating imported positions
		book_fix(book);
		book_negamax(book);
		book_sort_parallel(book);
		if (options.book_merge_auto_save) {
			// keep need_saving: the exit save to the book file behaves as before
			const bool need_saving = book->need_saving;
			if (strlen(options.book_file) + sizeof ".mrg" > sizeof merged_file) {
				warn("Book file name too long; merged book was not saved\n");
			} else {
				file_add_ext(options.book_file, ".mrg", merged_file);
				if (book_save(book, merged_file)) printf("Merged book saved to %s\n", merged_file);
				book->need_saving = need_saving;
			}
		}
	} else warn("Book %s was not merged\n", file);
}

/**
 * @brief Add the positions of a game to the book, from the last one to the first one,
 * or plan their searches (see book_plan_begin).
 *
 * @param book Opening book.
 * @param initial_board Initial board of the game.
 * @param game Moves of the game.
 * @param n_game Number of moves.
 * @param plan Only plan the searches.
 */
static void play_store_boards(Book *book, const Board *initial_board, Move *game, const int n_game, const bool plan)
{
	void (*store)(Book*, const Board*) = plan ? book_plan_board : book_add_board;
	Board board;
	int i;

	board = *initial_board;
	for (i = 0; i < n_game && board_check_move(&board, game + i); ++i) {
		board_update(&board, game + i);
	}

	for (--i; i >= 0; --i) {
		store(book, &board);
		board_restore(&board, game + i);
	}
	store(book, &board);
}

/** what a book learned of a file of games (book learn: see LearnRecord) */
typedef struct LearnProgress {
	long long lines;               /**< lines of the file of games learned in the book */
	unsigned long long hash;       /**< hash of these lines (learn_hash_line) */
	long long games, learned;      /**< games read and learned in these lines */
	unsigned long long positions;  /**< number of positions of the book */
	int level, empties;            /**< level and depth (empty squares) of the book */
	unsigned long long book;       /**< fingerprint of the book (book_fingerprint) */
} LearnProgress;

/** the saves to <book-file>.store of a book learn <file> */
typedef struct LearnSaves {
	const char *games_file;         /**< file of games */
	const char *path;               /**< progress file */
	LearnProgress saved;            /**< the book of the last save that succeeded (at first: the book in use) */
	bool own;                       /**< the progress file is the one of this file of games */
	const int *line;                /**< line of each game of the group */
	const unsigned long long *hash; /**< hash of the lines up to each of them */
	long long games, learned;       /**< games read and learned before the group */
	int n_stored, n_learned;        /**< games of the group read and learned in the book to save */
} LearnSaves;

/** the book learn <file> that runs, NULL if none: play_store_end saves its progress with the book */
static LearnSaves *learn_saves = NULL;

static void learn_saves_store(LearnSaves*, Book*, const char*);

/**
 * @brief Link, negamax and save the book after games were stored, if positions or links were added
 * (book store, book learn).
 *
 * @param book Opening book.
 */
static void play_store_end(Book *book)
{
	char file[FILENAME_MAX + 1];

	if (book->stats.n_nodes + book->stats.n_links) {
		book->need_saving = true; // also when links were added without any search
		book_link(book);
		book_negamax(book);
		if (options.book_store_auto_save) {
			file_add_ext(options.book_file, ".store", file);
			if (learn_saves) learn_saves_store(learn_saves, book, file);
			else book_save_progress(book, file);
		}
	}
}

/**
 * @brief store the game into the opening book
 *
 * @param play Play.
 */
void play_store(Play *play)
{
	play_no_stop(play, true); // (the searches of the positions of the game are not cut short by a stop)
	play->book->stats.n_nodes = play->book->stats.n_links = 0;

	if (book_store_task_count() > 1 && book_plan_begin(play->book)) { // book-store-tasks: search the positions at the same time
		play_store_boards(play->book, &play->initial_board, play->game, play->n_game, true);
		book_plan_search(play->book);
	}
	play_store_boards(play->book, &play->initial_board, play->game, play->n_game, false);
	book_plan_end(play->book);

	play_store_end(play->book);
	play_no_stop(play, false);
}

/** a game to play and to learn (see play_learn_games) */
typedef struct LearnGame {
	const char *moves;         /**< first moves of the game */
	int randomness;            /**< randomness of the book moves */
	Move game[PLAY_GAME_SIZE]; /**< moves of the game (with the passes: as Play.game; 80 up to v4.5.5-nikque.12) */
	int n_game;                /**< number of moves */
	bool legal;                /**< the first moves are legal */
	unsigned long long seed;   /**< seed of the random choices among the book moves of this game */
} LearnGame;

/** games played at the same time */
typedef struct LearnShared {
	Book *book;
	LearnGame *game;
	int n, next, n_done;
	int n_threads, n_busy;     /**< threads shared by the lanes; lanes that still have a game to play */
	Lock lock;                 /**< guards next, n_done and n_busy */
} LearnShared;

/** a thread playing games, with its own search */
typedef struct LearnLane {
	LearnShared *shared;
	Search *search;
	Random random;             /**< to choose among the book moves (seeded for each game) */
	bool progress;             /**< this one shows the progress */
	int n_tasks;               /**< threads of its search */
} LearnLane;

/** size of the copy of the first moves of a game: a game has 60 moves at most (120 characters) */
#define LEARN_MOVES_SIZE 256

/**
 * @brief Copy the first moves of a game, in lower case and without the spaces.
 *
 * A line can hold any number of spaces between its moves: copied with them, a long line was cut,
 * and when the cut fell between two moves the shorter game was learned instead.
 * Without the spaces, what does not fit is beyond the 127th move: it cannot be played anyway.
 *
 * @param copy Copy (LEARN_MOVES_SIZE characters).
 * @param moves First moves of the game.
 */
static void learn_moves_copy(char *copy, const char *moves)
{
	int k = 0;

	for (; *moves && k < LEARN_MOVES_SIZE - 1; ++moves) if (*moves != ' ') copy[k++] = *moves;
	copy[k] = '\0';
	string_to_lowercase(copy);
}

/**
 * @brief Play the first moves of a game.
 *
 * @param g Game.
 * @param board Board (the initial position; updated).
 * @return player to move.
 */
static int learn_game_start(LearnGame *g, Board *board)
{
	const char *string = g->moves, *next;
	Move move;
	int player = BLACK;

	board_init(board);
	g->n_game = 0;
	next = opening_get_line(string);
	if (next) string = next;
	while (g->n_game < PLAY_GAME_SIZE && ((next = parse_move(string, board, &move)) != string || move.x == PASS)) { // as play_game()
		string = next;
		board_update(board, &move);
		g->game[g->n_game++] = move;
		player ^= 1;
	}
	string = parse_skip_spaces(string);
	g->legal = (*string == '\0'); // Edax ignores an illegal move and the following ones: such a game is not learned
	return player;
}

/** threads of a lane above which it gets no more (see learn_lane_share_threads) */
#define LEARN_LANE_SHARE_MAX 8

/**
 * @brief Give to the search of a lane the threads of the lanes that have no game left to play.
 *
 * When the last games are being played, the lanes that are over leave their threads idle. The search of
 * a lane that still plays gets them for its next moves (between two moves: nothing is stopped, the threads
 * are only created). Its hash tables keep their size.
 * A lane that already has LEARN_LANE_SHARE_MAX threads or more keeps them: more threads did not make its
 * moves faster (2 games at level 24, 16 threads each: 1.008 +- 0.005 times the time, 72 rounds), while
 * lanes of 1 thread gain much (128 games at level 18: 0.90).
 *
 * @param lane Lane.
 */
static void learn_lane_share_threads(LearnLane *lane)
{
#ifndef BOOK_TEST_ONE_THREAD
	LearnShared *s = lane->shared;
	int n;

	if (lane->n_tasks >= LEARN_LANE_SHARE_MAX || options.book_reproducible) return; // (book-reproducible: one-thread searches)
	lock(s);
	n = MIN(s->n_threads / s->n_busy, MAX_THREADS - 1);
	unlock(s);
	if (n > lane->n_tasks) {
		search_set_task_number(lane->search, n);
		lane->n_tasks = n;
	}
#else
	(void) lane; // test builds: every search keeps its threads, to compare the books
#endif
}

/**
 * @brief Play games to their end, as play_go() does: a move of the book, or the move of a search.
 *
 * The book is only read.
 *
 * @param v Lane.
 * @return NULL.
 */
static void* learn_lane_run(void *v)
{
	LearnLane *lane = (LearnLane*) v;
	LearnShared *s = lane->shared;
	Search *const search = lane->search;
	long long t = real_clock() + 1000;
	char s_move[4];

	for (;;) {
		LearnGame *g;
		Board board;
		Move move;
		long long left[2]; // time left to each player, as play->time[].left
		int i, player, n_done;

		lock(s);
		i = s->next < s->n ? s->next++ : -1;
		if (i < 0) --s->n_busy; // (see learn_lane_share_threads)
		unlock(s);
		if (i < 0) break;

		g = s->game + i;
		player = learn_game_start(g, &board);
		random_seed(&lane->random, g->seed); // (the choices of a game do not depend on the lane that plays it)
		search_cleanup(search); // as play_new()
		left[0] = left[1] = options.time;
		while (g->legal && g->n_game < PLAY_GAME_SIZE && !board_is_game_over(&board)) {
			long long t_real = -real_clock();

			move = MOVE_INIT;
			if (g->n_game == 0) { // as play_force_go(): the first move is F5
				board_get_move_flip(&board, F5, &move);
				t_real += real_clock() + 1;
			} else if (options.book_allowed && book_get_random_move_with(s->book, &board, &move, g->randomness, &lane->random) && move.x != NOMOVE) {
				t_real += real_clock() + 1;
			} else {
				learn_lane_share_threads(lane);
				search->options.verbosity = 0;
				search_set_board(search, &board, player);
				search_set_level(search, play_level(), search->eval.n_empties);
				if (options.play_type == EDAX_TIME_PER_MOVE) search_set_move_time(search, options.time);
				else search_set_game_time(search, left[player]);
				search_time_init(search);
				search_run(search);
#ifdef BOOK_TEST_NODES
				{ extern void book_test_count_play(const unsigned long long); book_test_count_play(search_count_nodes(search)); }
#endif
				if (!board_get_move_flip(&board, search->result->move, &move) && move.x != PASS) {
					fatal_error("bad move found: %s\n", move_to_string(move.x, player, s_move));
				}
				t_real += real_clock() + 1;
				if (options.nps > 0) t_real = search->result->time; // virtual clock (node count / nps), as used by the search
			}
			// as play_go(): with a time per game, the time of the move is taken from the time left to the player
			// (up to v4.5.5-nikque.8 every move got the time of the whole game)
			if (options.play_type != EDAX_TIME_PER_MOVE) left[player] -= t_real;
			board_update(&board, &move);
			g->game[g->n_game++] = move;
			player ^= 1;
		}

		lock(s);
		n_done = ++s->n_done;
		unlock(s);
		if (lane->progress && real_clock() >= t) {
			book_print("Playing games...%d/%d\r", n_done, s->n);
			t = real_clock() + 1000;
		}
	}
	return NULL;
}

/**
 * @brief Play games and store them into the opening book, one after the other, with the game of the user
 * interface (see play_learn_games): as the commands init, play, go (until the game is over) and book store.
 *
 * @param play Play.
 * @param moves First moves of each game.
 * @param randomness Randomness of the book moves of each game (NULL: the current setting).
 * @param n Number of games.
 * @param status Set for each game (see play_learn_games). Can be NULL.
 * @param verbosity Verbosity of the search for the book store, as set by the book command.
 * @return number of learned games.
 */
static int learn_games_in_turn(Play *play, const char *const *moves, const int *randomness, const int n, int *status, const int verbosity)
{
	Book *book = play->book;
	const int book_randomness = options.book_randomness;
	int i, n_learned = 0;

	for (i = 0; i < n; ++i) {
		char copy[LEARN_MOVES_SIZE], played[256];
		int j, k;

		if (randomness) options.book_randomness = randomness[i];
		// init
		board_init(&play->initial_board);
		play->initial_player = BLACK;
		play_force_init(play, "F5");
		play_new(play);
		// play
		learn_moves_copy(copy, moves[i]);
		play_game(play, copy);
		for (j = k = 0; j < play->n_game; ++j) {
			if (play->game[j].x != PASS) { move_to_string(play->game[j].x, WHITE, played + k); k += 2; }
		}
		played[k] = '\0';
		string_to_lowercase(played);
		if (strcmp(played, copy) != 0) { // Edax ignores an illegal move and the following ones: such a game is not learned
			if (status) status[i] = 1;
			continue;
		}
		// go
		while (!play_is_game_over(play)) play_go(play, true);
		// book store
		play->search.options.verbosity = verbosity; // as set by the book command (play_go() changed it)
		if (learn_saves) { learn_saves->n_stored = i + 1; learn_saves->n_learned = n_learned + 1; } // (saved after each game)
		play_store(play);
		if (book->failed) { // a position could not be added: this game and the following ones are not learned
			if (status) for (; i < n; ++i) status[i] = 2;
			break;
		}
		if (status) status[i] = 0;
		++n_learned;
	}
	options.book_randomness = book_randomness;
	play->search.options.verbosity = verbosity;
	return n_learned;
}

/**
 * @brief Play games and store them into the opening book.
 *
 * For each game: play its first moves from the initial position, let Edax play both sides to
 * the end (as the go command does), then store the game (as book store does).
 *
 * With book-store-tasks = 1, the games are played and stored one after the other, with the
 * game of the user interface: it is the same as the commands init, play, go (until the game is
 * over) and book store, for each game.
 *
 * With book-store-tasks > 1, as many games are played at the same time, each one with its own
 * search (n-tasks / book-store-tasks threads), reading the book as it was before the call;
 * then all the games are stored: their positions are searched at the same time, and the book
 * is linked, negamaxed and saved (to <book-file>.store) once. The game of the user interface
 * is not changed. (If the memory for the searches of these games is not available, the games
 * are learned one after the other, as with book-store-tasks = 1.)
 *
 * @param play Play.
 * @param moves First moves of each game.
 * @param randomness Randomness of the book moves of each game (NULL: the current setting).
 * @param n Number of games.
 * @param status Set for each game: 0 = learned, 1 = not learned (illegal move), 2 = not learned
 * (failure: a position could not be added to the book, see Book.failed). Can be NULL.
 * @return number of learned games.
 */
int play_learn_games(Play *play, const char *const *moves, const int *randomness, const int n, int *status)
{
	Book *book = play->book;
	const int book_randomness = options.book_randomness;
	const int n_lanes = MIN(book_store_task_count(), n);
	const int verbosity = play->search.options.verbosity;
	LearnGame *game = NULL;
	char *buffer = NULL;
	LearnLane *lane = NULL;
	Search **search = NULL;
	LearnShared shared;
	Board initial_board;
	int i, n_learned = 0, lane_tasks = 1;

	if (n <= 0) return 0;
	play_stop_pondering(play);

	if (book_store_task_count() > 1) {
		const int n_tasks = lane_tasks = options.book_reproducible ? 1 : MAX(1, book_store_thread_count() / n_lanes); // (a search with several threads can give another game)

		game = (LearnGame*) calloc(n, sizeof *game);
		buffer = (char*) malloc((size_t) n * LEARN_MOVES_SIZE);
		lane = (LearnLane*) calloc(n_lanes, sizeof *lane);
		if (game && buffer && lane) search = book_store_searches(book, n_lanes, n_tasks);
		if (search == NULL) { // not enough memory to play the games at the same time: one game after the other
			warn("not enough memory to play %d games at the same time: they are learned one after the other\n", n_lanes);
			free(game); free(buffer); free(lane);
			game = NULL;
		}
	}

	if (game == NULL) return learn_games_in_turn(play, moves, randomness, n, status, verbosity);

	// play the games
	for (i = 0; i < n; ++i) {
		learn_moves_copy(buffer + i * LEARN_MOVES_SIZE, moves[i]);
		game[i].moves = buffer + i * LEARN_MOVES_SIZE;
		game[i].randomness = randomness ? randomness[i] : book_randomness;
		game[i].seed = random_get(&book->random);
	}
	shared.book = book; shared.game = game; shared.n = n; shared.next = shared.n_done = 0;
	shared.n_threads = book_store_thread_count(); shared.n_busy = n_lanes;
	lock_init(&shared);
	for (i = 0; i < n_lanes; ++i) {
		lane[i].shared = &shared;
		lane[i].search = search[i];
		lane[i].progress = (i == 0);
		lane[i].n_tasks = lane_tasks;
	}
	book_print("Playing games...\r");
	thread_run_workers(learn_lane_run, lane, sizeof *lane, n_lanes, true, false); // (this thread runs the first lane)
	lock_free(&shared);
	for (i = 0; i < n_lanes; ++i) { // back to the state of the pool
		if (lane[i].n_tasks != lane_tasks) search_set_task_number(search[i], lane_tasks);
	}
	book_print("Playing games...%d done\n", n);

	// store the games
	book->stats.n_nodes = book->stats.n_links = 0;
	board_init(&initial_board);
	if (book_plan_begin(book)) {
		for (i = 0; i < n; ++i) if (game[i].legal) play_store_boards(book, &initial_board, game[i].game, game[i].n_game, true);
		book_plan_search(book);
	}
	for (i = 0; i < n; ++i) {
		if (game[i].legal) {
			play_store_boards(book, &initial_board, game[i].game, game[i].n_game, false);
			++n_learned;
		}
		if (status) status[i] = game[i].legal ? 0 : 1;
	}
	book_plan_end(book);
	if (book->failed) { // a position could not be added: which games are complete in the book is not known
		if (status) for (i = 0; i < n; ++i) if (game[i].legal) status[i] = 2;
		n_learned = 0;
	}
	if (learn_saves) { learn_saves->n_stored = n; learn_saves->n_learned = n_learned; } // (saved after the group)
	play_store_end(book);
	free(game); free(buffer); free(lane);

	return n_learned;
}

/**
 * @brief Parse a line that describes a game to learn.
 *
 * A line holds the first moves of a game from the initial position ("f5d6c3"), or
 * "<book randomness>,<moves>" ("2,f5d6c3"). Empty lines, lines that start with '#' and
 * lines with "//" are comments.
 *
 * @param line Line (modified).
 * @param randomness Set to the book randomness, if the line gives it.
 * @return the moves (inside line); an empty string for a comment; NULL if the line is not a game.
 */
char* play_learn_parse(char *line, int *randomness)
{
	char *s = parse_skip_spaces(line), *e;

	for (e = s + strlen(s); e > s && isspace((unsigned char) e[-1]); --e) ;
	*e = '\0';
	if (*s == '\0' || *s == '#' || strstr(s, "//")) return e;
	if (isdigit((unsigned char) *s)) { // <book randomness>,<moves>
		const long long r = strtoll(s, &e, 10); // as the book-randomness setting: any number, INT_MAX if larger
		e = parse_skip_spaces(e);
		if (*e != ',' || r < 0) return NULL;
		*randomness = (r > INT_MAX) ? INT_MAX : (int) r;
		s = parse_skip_spaces(e + 1);
	}
	if (*s == '\0') return NULL;
	for (e = s; *e; ) {
		if (*e == ' ') ++e;
		else if (strchr("abcdefghABCDEFGH", e[0]) && e[1] >= '1' && e[1] <= '8') e += 2;
		else return NULL;
	}
	return s;
}

/*
 * Progress of book learn <file>.
 *
 * While the games of a file are learned, the file <book-file>.learn tells how many lines of the file of games
 * are learned in the book of <book-file>.store, with the name of the file of games, a hash of these lines, and
 * the size, level, depth and fingerprint (book_fingerprint: its positions, links and scores) of this book.
 * book learn run again with the same file, on this book (<book-file>.store loaded, or the book saved at the
 * exit), skips these lines: the games are not played again. A progress of another file, or of another book
 * (even of as many positions), is not used: the games are learned from the first line. When the whole file is
 * learned, the progress file is removed; it is kept when the file of games cannot be read to its end. With
 * book-store-auto-save off (no save to <book-file>.store), no progress is kept.
 *
 * A progress file of v4.5.5-nikque.29 to .33 (without file= and book=) is not used either: it tells the book
 * by its numbers of positions and links only.
 *
 * The book and the progress cannot be replaced together, and a run can stop at any moment:
 * - before a save, the progress is written with two states: the book of the last save (lines=, hash=, ...) and
 *   the book to save (next_lines=, next_hash=, ...). The book on the disk is one of the two, and a new run takes
 *   the one of the book in use, by its fingerprint (next_links= is still written for the earlier versions). A save
 *   that fails leaves the first one true;
 * - after the save, the progress is written with this book alone, as up to v4.5.5-nikque.32 (that wrote it only
 *   then: a run stopped in between could not be continued). Such a file is read as a book without next state.
 * Each write replaces the whole file (see learn_progress_write).
 */

/** what a progress file of book learn tells */
typedef struct LearnRecord {
	LearnProgress saved;  /**< the book of the last save (lines = 0: none, the book in use when book learn started) */
	LearnProgress next;   /**< the book that was being saved */
	bool has_next;        /**< next is given */
	long long next_links; /**< links of next, when it has as many positions as saved (else -1; written for the
	                           versions up to v4.5.5-nikque.33, that tell the two books apart by it) */
	bool old;             /**< written up to v4.5.5-nikque.33: no fingerprint of the book, nor name of the file of games */
	char *file;           /**< file of games (NULL if none is given) */
} LearnRecord;

/** first value of the hash of the lines of a file of games (FNV-1a, 64 bits) */
#define LEARN_HASH_START 0xcbf29ce484222325ULL

/**
 * @brief Add a line of a file of games to a hash of its lines.
 *
 * @param h Hash of the lines before.
 * @param line Line, as read (without its end).
 * @return the hash with the line.
 */
static unsigned long long learn_hash_line(unsigned long long h, const char *line)
{
	const unsigned char *s;

	for (s = (const unsigned char*) line; *s; ++s) {
		h ^= *s;
		h *= 0x100000001b3ULL;
	}
	h ^= '\n';
	h *= 0x100000001b3ULL;
	return h;
}

/**
 * @brief Read the progress file of book learn.
 *
 * @param path Progress file.
 * @param r What it tells (out).
 * @return false if there is no progress file, or if it does not hold every value.
 */
static bool learn_progress_read(const char *path, LearnRecord *r)
{
	FILE *f = fopen(path, "r");
	LearnProgress *p = &r->saved, *q = &r->next;
	char *line;
	int seen = 0, seen_next = 0;

	memset(r, 0, sizeof *r);
	r->next_links = -1;
	if (f == NULL) return false;
	while ((line = string_read_line(f)) != NULL) {
		if (strncmp(line, "file=", 5) == 0) {
			free(r->file);
			r->file = string_duplicate(line + 5);
			if (r->file) seen |= 256;
		} else if (sscanf(line, "lines=%lld", &p->lines) == 1) seen |= 1;
		else if (sscanf(line, "hash=%llx", &p->hash) == 1) seen |= 2;
		else if (sscanf(line, "games=%lld", &p->games) == 1) seen |= 4;
		else if (sscanf(line, "learned=%lld", &p->learned) == 1) seen |= 8;
		else if (sscanf(line, "positions=%llu", &p->positions) == 1) seen |= 16;
		else if (sscanf(line, "level=%d", &p->level) == 1) seen |= 32;
		else if (sscanf(line, "empties=%d", &p->empties) == 1) seen |= 64;
		else if (sscanf(line, "book=%llx", &p->book) == 1) seen |= 128;
		else if (sscanf(line, "next_book=%llx", &q->book) == 1) seen_next |= 32;
		else if (sscanf(line, "next_lines=%lld", &q->lines) == 1) seen_next |= 1;
		else if (sscanf(line, "next_hash=%llx", &q->hash) == 1) seen_next |= 2;
		else if (sscanf(line, "next_games=%lld", &q->games) == 1) seen_next |= 4;
		else if (sscanf(line, "next_learned=%lld", &q->learned) == 1) seen_next |= 8;
		else if (sscanf(line, "next_positions=%llu", &q->positions) == 1) seen_next |= 16;
		else sscanf(line, "next_links=%lld", &r->next_links);
		free(line);
	}
	if (ferror(f)) seen = 0; // (a file read in part tells nothing)
	fclose(f);
	q->level = p->level;
	q->empties = p->empties;
	// a file of the versions up to v4.5.5-nikque.33 tells the book by its numbers of positions and links only (another
	// book may have them), and not the file of games: it is not used
	r->old = ((seen & 127) == 127 && (seen & 384) != 384);
	r->has_next = (seen_next == 63 && q->lines > 0);
	return seen == 511 && (p->lines > 0 || (p->lines == 0 && r->has_next));
}

/**
 * @brief Find the state of the book in use in a progress.
 *
 * @param r Progress.
 * @param book Book in use.
 * @param fingerprint Its fingerprint (book_fingerprint).
 * @return the state, NULL if the progress is of another book.
 */
static const LearnProgress* learn_progress_find(const LearnRecord *r, const Book *book, const unsigned long long fingerprint)
{
	if (r->saved.level != book->options.level || r->saved.empties != book->options.n_empties) return NULL;
	if (r->has_next && r->next.positions == book->n_nodes && r->next.book == fingerprint) return &r->next;
	// (the state of no line learned skips nothing: its fingerprint is not computed, see play_learn_file)
	return (r->saved.positions == book->n_nodes && (r->saved.lines == 0 || r->saved.book == fingerprint)) ? &r->saved : NULL;
}

/**
 * @brief Write the progress file of book learn: a new file, that then replaces it.
 *
 * A file that cannot be replaced at once (another program reads it) is tried again for a short time.
 *
 * @param path Progress file.
 * @param r Progress.
 * @param file File of games.
 * @return true if the file was written.
 */
static bool learn_progress_write(const char *path, const LearnRecord *r, const char *file)
{
	const LearnProgress *p = &r->saved, *q = &r->next;
	char tmp_file[FILENAME_MAX + 1];
	FILE *f;
	bool ok;
	int i;

	file_add_ext(path, ".tmp", tmp_file);
	if ((f = fopen(tmp_file, "w")) == NULL) {
		warn("cannot write %s: the progress of book learn is not kept\n", path);
		return false;
	}
	fprintf(f, "# book learn %s: the first lines of this file of games are learned in the book that has these positions.\n", file);
	fprintf(f, "# book learn with the same file, on this book, skips them. This file is removed when the whole file is learned.\n");
	fprintf(f, "file=%s\n", file);
	fprintf(f, "lines=%lld\nhash=%016llx\ngames=%lld\nlearned=%lld\npositions=%llu\nlevel=%d\nempties=%d\nbook=%016llx\n",
		p->lines, p->hash, p->games, p->learned, p->positions, p->level, p->empties, p->book);
	if (r->has_next) {
		fprintf(f, "# a save of the book began: if it ended, the book has these values.\n");
		fprintf(f, "next_lines=%lld\nnext_hash=%016llx\nnext_games=%lld\nnext_learned=%lld\nnext_positions=%llu\nnext_book=%016llx\n",
			q->lines, q->hash, q->games, q->learned, q->positions, q->book);
		if (r->next_links >= 0) fprintf(f, "next_links=%lld\n", r->next_links);
	}
	// on the disk before the book that it tells
	ok = (fflush(f) == 0 && ferror(f) == 0);
#ifdef _WIN32
	if (ok && _commit(_fileno(f)) != 0) ok = false;
#else
	if (ok && fsync(fileno(f)) != 0) ok = false;
#endif
	if (fclose(f) != 0) ok = false;
	if (ok) {
		for (i = 1; !(ok = file_replace(tmp_file, path)) && i < 10; ++i) relax(20);
	}
	if (!ok) {
		remove(tmp_file);
		warn("cannot write %s: the progress of book learn is not kept\n", path);
	}
	return ok;
}

/**
 * @brief Save the book of book learn <file> to <book-file>.store, with its progress before and after.
 *
 * @param s Saves of book learn.
 * @param book Opening book.
 * @param store File to save the book to.
 */
static void learn_saves_store(LearnSaves *s, Book *book, const char *store)
{
	const int i = s->n_stored - 1;
	LearnProgress next = {s->line[i], s->hash[i], s->games + s->n_stored, s->learned + s->n_learned,
		book->n_nodes, book->options.level, book->options.n_empties, 0};
	LearnRecord r = {s->saved, next, true, -1, false, NULL};

	if (book->failed) { // which games are complete in the book is not known
		book_save_progress(book, store);
		return;
	}
	r.next.book = next.book = book_fingerprint(book); // (the book to save)
	if (next.positions == s->saved.positions) r.next_links = book_count_links(book);
	if (learn_progress_write(s->path, &r, s->games_file)) s->own = true;
	if (book_save_progress(book, store)) {
		r.saved = s->saved = next;
		r.has_next = false;
		if (learn_progress_write(s->path, &r, s->games_file)) s->own = true;
	}
}

/**
 * @brief Play and learn the games of a file (book learn command).
 *
 * Each line of the file holds the first moves of a game from the initial position ("f5d6c3"),
 * or "<book randomness>,<moves>" ("2,f5d6c3"). Empty lines, lines that start with '#' and lines
 * with "//" are ignored. The games are learned by groups of book-store-tasks games (see
 * play_learn_games).
 *
 * The lines learned by a run that did not end are skipped (see LearnRecord). (Up to
 * v4.5.5-nikque.28, a new run played and learned all the games of the file again.)
 *
 * @param play Play.
 * @param file File name.
 * @return number of learned games, -1 if the file cannot be read.
 */
int play_learn_file(Play *play, const char *file)
{
	enum { LEARN_GROUP_MAX = MAX_THREADS };
	FILE *f = fopen(file, "r");
	const int n_group = MIN(book_store_task_count(), LEARN_GROUP_MAX);
	char *line, *moves[LEARN_GROUP_MAX];
	int randomness[LEARN_GROUP_MAX], status[LEARN_GROUP_MAX], number[LEARN_GROUP_MAX];
	unsigned long long group_hash[LEARN_GROUP_MAX];
	int i, n = 0, n_line = 0, n_games = 0, n_learned = 0;
	bool more = true;
	Book *book = play->book;
	char progress_file[FILENAME_MAX + 1];
	LearnRecord record;
	LearnSaves saves = {0};
	unsigned long long hash = LEARN_HASH_START, fingerprint = 0; // (0: not computed, for a progress of no line learned)
	bool read_failed = false;

	if (f == NULL) {
		warn("cannot open game file %s\n", file);
		return -1;
	}

	// the lines learned by a run that did not end
	file_add_ext(options.book_file, ".learn", progress_file);
	if (!learn_progress_read(progress_file, &record)) {
		if (record.old) warn("%s was written by an earlier version, that does not tell the book and the file of games it is of: the games of %s are learned from the first line\n", progress_file, file);
	} else if (strcmp(record.file, file) != 0) {
		warn("%s is the progress of book learn of another file (%s): the games of %s are learned from the first line\n", progress_file, record.file, file);
	} else {
		const LearnProgress *done = learn_progress_find(&record, book, fingerprint = book_fingerprint(book));

		if (done == NULL) {
			const LearnProgress *of = record.saved.lines ? &record.saved : &record.next;

			warn("%s is the progress of book learn of another book (%llu positions, level %d, depth %d; the book in use: %u positions, level %d, depth %d): the games of %s are learned from the first line\n",
				progress_file, of->positions, of->level, 61 - of->empties, book->n_nodes, book->options.level, 61 - book->options.n_empties, file);
		} else if (done->lines > 0) { // (0: stopped before the first save ended, nothing is learned in this book)
			while (n_line < done->lines && (line = string_read_line(f)) != NULL) {
				if (ferror(f)) { free(line); break; }
				hash = learn_hash_line(hash, line);
				++n_line;
				free(line);
			}
			if (ferror(f)) { // (the lines to skip are not known: nothing is learned)
				warn("cannot read game file %s\n", file);
				free(record.file);
				fclose(f);
				return -1;
			}
			if (n_line == done->lines && hash == done->hash) {
				saves.saved = *done;
				saves.own = true;
				book_print("Book learn: the first %d lines of %s (%lld games) were learned by a run that did not end: they are skipped\n", n_line, file, done->games);
			} else {
				warn("%s is the progress of book learn of another file: the games of %s are learned from the first line\n", progress_file, file);
				rewind(f);
				n_line = 0;
				hash = LEARN_HASH_START;
			}
		}
	}
	free(record.file);
	if (!saves.own) {
		const LearnProgress none = {0, LEARN_HASH_START, 0, 0, book->n_nodes, book->options.level, book->options.n_empties, fingerprint};

		saves.saved = none;
	}
	saves.games_file = file;
	saves.path = progress_file;
	saves.line = number;
	saves.hash = group_hash;
	saves.games = saves.saved.games;
	saves.learned = saves.saved.learned;

	while (more) {
		line = string_read_line(f);
		if (ferror(f)) { // (not the end of the file: a line read in part is not a game, and the progress is kept)
			free(line);
			line = NULL;
			read_failed = true;
		}
		more = (line != NULL);
		if (line) {
			int r = options.book_randomness;
			char *s;

			hash = learn_hash_line(hash, line); // (as read: play_learn_parse changes the line)
			s = play_learn_parse(line, &r);
			++n_line;
			if (s == NULL || *s == '\0') {
				if (s == NULL) warn("%s:%d: not the moves of a game: ignored\n", file, n_line);
				free(line);
				continue;
			}
			memmove(line, s, strlen(s) + 1);
			moves[n] = line; randomness[n] = r; number[n] = n_line; group_hash[n] = hash;
			++n;
		}
		if (n == n_group || (!more && n > 0)) {
			int n_group_learned;

			learn_saves = &saves;
			n_group_learned = play_learn_games(play, (const char *const*) moves, randomness, n, status);
			learn_saves = NULL;
			saves.games += n;
			saves.learned += n_group_learned;
			n_learned += n_group_learned;
			n_games += n;
			for (i = 0; i < n; ++i) {
				if (status[i] == 1) warn("%s:%d: illegal move: the game was not learned\n", file, number[i]);
				else if (status[i]) warn("%s:%d: a position could not be added to the book: the game was not learned\n", file, number[i]);
				free(moves[i]);
			}
			n = 0;
			if (play->book->failed) more = false; // a position could not be added: stop learning
		}
	}
	fclose(f);
	if (read_failed) warn("cannot read game file %s after line %d: the following games are not learned\n", file, n_line);
	// the whole file is learned: the progress is removed (it is kept when learning stopped)
	if (saves.own && !play->book->failed && !read_failed && remove(progress_file) == 0) book_print("Book learn: %s is learned to its end: %s removed\n", file, progress_file);
	book_print("%d/%d games learned\n", n_learned, n_games);
	return n_learned;
}

/**
 * @brief adjust time.
 *
 * Set remaining time to play from a server (GGS) or a GUI (Quarry, ...).
 *
 * @param play Play.
 * @param left Time left.
 * @param extra Extra time.
 */
void play_adjust_time(Play *play, const int left, const int extra)
{
	play->time[play->player].left = left;
	play->time[play->player].extra = extra;
}

/**
 * @brief Print the game state.
 *
 * Print the game state: board, time, played move, etc.
 *
 * @param play Play.
 * @param f Output stream.
 */
void play_print(Play *play, FILE *f)
{
	int i, j, x, discs[2], mobility[2], square;
	char history[64];
	const char color[5] = "?*O-.";
	const char big_color[3][4] = {"|##", "|()", "|  "};
	const char player[2][6] = {"Black", "White"};
	const int p = play->player;
	unsigned long long bk, wh, bk0, wh0, moves;
	bool gameover;

	if (p == BLACK) {
		bk = play->board.player;
		wh = play->board.opponent;
	} else {
		bk = play->board.opponent;
		wh = play->board.player;
	}
	if ((p ^ (play->i_game & 1)) == BLACK) {
		bk0 = play->initial_board.player;
		wh0 = play->initial_board.opponent;
	} else {
		bk0 = play->initial_board.opponent;
		wh0 = play->initial_board.player;
	}

	moves = board_get_moves(&play->board);
	discs[BLACK] = bit_count(bk);
	discs[WHITE] = bit_count(wh);
	mobility[BLACK] = get_mobility(bk, wh);
	mobility[WHITE] = get_mobility(wh, bk);
	gameover = (mobility[BLACK] + mobility[WHITE] == 0);

	memset(history, 0, 64);
	for (i = j = 0; i < play->i_game; i++) {
		x = play->game[i].x;
		if (A1 <= x && x <= H8) history[x] = ++j;
	}

	fputs("  A B C D E F G H            BLACK            A  B  C  D  E  F  G  H\n", f);
	for (i = 0; i < 8; i++) {
		fputc(i + '1', f);
		fputc(' ', f);
		for (j = 0; j < 8; j++) {
			square = 2 - (wh & 1) - 2 * (bk & 1);
			if ((square == EMPTY) && (moves & 1))
				square = EMPTY + 1;
			fputc(color[square + 1], f);
			fputc(' ', f);
			bk >>= 1;
			wh >>= 1;
			moves >>= 1;
		}
		fputc(i + '1', f);

		switch(i) {
		case 0:
			fputs("  ", f); time_print(play->time[BLACK].spent, true, f); fputs("       ", f);
			break;
		case 1:
			fprintf(f, "   %2d discs  %2d moves   ", discs[BLACK], mobility[BLACK]);
			break;
		case 3:
			if (gameover) fprintf(f, "       Game over        ");
			else fprintf(f, "  ply %2d (%2d empties)   ", play->i_game + 1, board_count_empties(&play->board));
			break;
		case 4:
			if (gameover) {
				if (discs[BLACK] > discs[WHITE]) fprintf(f, "       %s won        ", player[BLACK]);
				else if (discs[BLACK] < discs[WHITE]) fprintf(f, "       %s won        ", player[WHITE]);
				else fprintf(f, "          draw          ");
			} else fprintf(f, "    %s's turn (%c)    ", player[p], color[p + 1]);
			break;
		case 6:
			fprintf(f, "   %2d discs  %2d moves   ", discs[WHITE], mobility[WHITE]);
			break;
		case 7:
			fputs("  ", f); time_print(play->time[WHITE].spent, true, f); fputs("       ", f);
			break;
		default:
			fputs("                        ", f);
			break;
		}
		fputc(i + '1', f); fputc(' ', f);
		for (j = 0; j < 8; j++){
			x = i * 8 + j;
			if (history[x])
				fprintf(f, "|%2d", history[x]);
			else
				fputs(big_color[2 - (wh0 & 1) - 2 * (bk0 & 1)], f);
			wh0 >>= 1;
			bk0 >>= 1;
		}
		fprintf(f, "| %1d\n", i + 1);
	}
	fputs("  A B C D E F G H            WHITE            A  B  C  D  E  F  G  H\n", f);
	fflush(f);
}

/**
 * @brief Initialize a forced line.
 *
 * @param play Play.
 * @param string A string with a sequence of moves.
 */
void play_force_init(Play *play, const char *string)
{
	Move move;
	const char *next;
	Board board;

	play->force.n_move = play->force.i_move = 0;
	board = play->initial_board;
	play->force.real[play->force.n_move] = board;
	board_unique(&board, play->force.unique + play->force.n_move);

	next = opening_get_line(string);
	if (next) string = next;

	while ((next = parse_move(string, &board, &move)) != string || move.x == PASS) {
		string = next;
		play->force.move[play->force.n_move] = move;
		board_update(&board, &move);
		++play->force.n_move;
		play->force.real[play->force.n_move] = board;
		board_unique(&board, play->force.unique + play->force.n_move);
	}
}

/**
 * @brief Update a forced line.
 *
 * Check if the actual board is in the forced line, and update the
 * forced line accordingly.
 *
 * @param play Play.
 */
void play_force_update(Play *play)
{
	Board unique;
	if (play->force.i_move < play->force.n_move) {
		board_unique(&play->board, &unique);
		if ((board_equal(&unique, play->force.unique + play->force.i_move))) {
			++play->force.i_move;
		}
	}
}

/**
 * @brief Restore a forced line.
 *
 * Check if the actual board is in the forced line, and restore the
 * forced line accordingly.
 *
 * @param play Play.
 */
void play_force_restore(Play *play)
{
	Board unique;
	if (play->force.i_move > 0) {
		board_unique(&play->board, &unique);
		if (board_equal(&unique, play->force.unique + play->force.i_move)) {
			--play->force.i_move;
		}
	}
}

/**
 * @brief Play a forced move.
 *
 * Check if the actual board is in the forced line, and play the next forced move.
 *
 * @param play Play.
 * @param move Output move.
 * @return 'true' if a forced move have been found.
 */
bool play_force_go(Play *play, Move *move)
{
	Board unique;
	Board sym;
	int s, x;

	if (play->force.i_move < play->force.n_move) {
		if (board_equal(&play->board, play->force.real + play->force.i_move)) {
			*move = play->force.move[play->force.i_move];
			return true;
		}

		board_unique(&play->board, &unique);
		if (board_equal(&unique, play->force.unique + play->force.i_move)) {
			for (s = 1; s < 8; ++s) {
				board_symetry(play->force.real + play->force.i_move, s, &sym);
				if (board_equal(&play->board, &sym)) {
					x = symetry(play->force.move[play->force.i_move].x, s);
					board_get_move_flip(&play->board, x, move);
					return true;
				}
			}
		}
	}

	return false;
}

/**
 * @brief Get the symetry of the actual position.
 *
 * @param play Play.
 * @param sym Symetry.
 */
void play_symetry(Play *play, const int sym)
{
	int i, x;
	Move move = MOVE_INIT;
	Board board;

	board_symetry(&play->initial_board, sym, &play->initial_board);
	board_symetry(&play->board, sym, &play->board);
	board = play->initial_board;
	for (i = 0; i  < play->n_game; ++i) {
		x = symetry(play->game[i].x, sym);
		board_get_move_flip(&board, x, &move);
		board_update(&board, &move);
		play->game[i] = move;
	}
}

/**
 * @brief Print the opening name 
 */
const char* play_show_opening_name(Play *play, const char *(*opening_get_name)(const Board*))
{
	int i;
	Board board;
	const char *name;
	const char *last = NULL;

	board = play->initial_board;
	for (i = 0; i  < play->i_game; ++i) {
		board_update(&board, play->game + i);
		name = opening_get_name(&board);
		if (name != NULL) last = name;
	}

	return last;
}
