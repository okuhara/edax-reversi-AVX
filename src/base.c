/**
 * @file base.c
 *
 * Header file for game base management.
 *
 * @date 1998 - 2020
 * @author Richard Delorme
 * @version 4.4
 */

#include "base.h"
#include "sort.h"
#include "options.h"
#include "search.h"
#include "perft.h"
#include "book.h"

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <time.h>

/**
 * @brief Set wthor header.
 *
 * @param wheader Wthor's file header.
 * @param n_games Game number;
 * @param n  Tourney/player number;
 * @param year Tournament year;
 */
static void wthor_header_set(WthorHeader *wheader, unsigned int n_games, unsigned int n, unsigned int year)
{
	time_t t;
	struct tm *date;

	t = time(NULL); date = localtime(&t);
	wheader->century = date->tm_year / 100 + 19;
	wheader->year = date->tm_year % 100;
	wheader->month = date->tm_mon + 1;
	wheader->day = date->tm_mday;
	wheader->n_games = n_games;
	wheader->n = n;
	if (year) wheader->game_year = year;
	else wheader->game_year = date->tm_year + 1900;
	wheader->board_size = 8;
	wheader->game_type = 0;
	wheader->depth = 1;
	wheader->reserved = 0;
}

/**
 * @brief Set a wthor header for a file whose games are kept: the year of its games and the depth of their
 * theoric scores are those of the file.
 *
 * @param wheader Wthor's file header (its game_year and depth are kept).
 * @param n_games Game number.
 */
static void wthor_header_set_kept(WthorHeader *wheader, unsigned int n_games)
{
	const unsigned short game_year = wheader->game_year;
	const char depth = wheader->depth;

	wthor_header_set(wheader, n_games, 0, 0);
	wheader->game_year = game_year;
	wheader->depth = depth;
}

/**
 * @brief Name of a file in the folder of a wthor file (WTHOR.JOU, WTHOR.TRN).
 *
 * @param file Wthor file.
 * @param name Name of the file in its folder.
 * @param path The name (at most size - 1 characters).
 * @param size Size of path.
 * @return false (said) if the name is too long: path is then not set.
 */
static bool wthor_companion_path(const char *file, const char *name, char *path, const size_t size)
{
	const char *c, *start = file;
	size_t n;

	for (c = file; *c; ++c) {
#ifdef _WIN32 // (also a backslash, or the colon of a drive, as path_get_dir)
		if (*c == '/' || *c == '\\' || *c == ':') start = c + 1;
#else
		if (*c == '/') start = c + 1;
#endif
	}
	n = (size_t) (start - file);
	if (n + strlen(name) >= size) {
		warn("The name of the folder of %s is too long for its file %s\n", file, name);
		return false;
	}
	memcpy(path, file, n);
	strcpy(path + n, name);
	return true;
}

/**
 * @brief Read wthor header.
 *
 * @param wheader Wthor's file header.
 * @param f Input stream .
 */
static bool wthor_header_read(WthorHeader *wheader, FILE *f)
{
	int r;

	r = fread(&wheader->century, 1, 1, f);
	r += fread(&wheader->year, 1, 1, f);
	r += fread(&wheader->month, 1, 1, f);
	r += fread(&wheader->day, 1, 1, f);
	r += fread(&wheader->n_games, 4, 1, f); // ok only on intel-x86 endianness.
	r += fread(&wheader->n, 2, 1, f);
	r += fread(&wheader->game_year, 2, 1, f);
	r += fread(&wheader->board_size, 1, 1, f);
	r += fread(&wheader->game_type, 1, 1, f);
	r += fread(&wheader->depth, 1, 1, f);
	r += fread(&wheader->reserved, 1, 1, f);

	if (r != 11) {
		warn("Cannot read wthor header (r = %d)\n", r);
		return false;
	}

	return true;
}

/**
 * @brief Read wthor header.
 *
 * @param wheader Wthor's file header.
 * @param f Input stream .
 */
static bool wthor_header_write(WthorHeader *wheader, FILE *f)
{
	int r;

	r = fwrite(&wheader->century, 1, 1, f);
	r += fwrite(&wheader->year, 1, 1, f);
	r += fwrite(&wheader->month, 1, 1, f);
	r += fwrite(&wheader->day, 1, 1, f);
	r += fwrite(&wheader->n_games, 4, 1, f); // ok only on intel-x86 endianness.
	r += fwrite(&wheader->n, 2, 1, f);
	r += fwrite(&wheader->game_year, 2, 1, f);
	r += fwrite(&wheader->board_size, 1, 1, f);
	r += fwrite(&wheader->game_type, 1, 1, f);
	r += fwrite(&wheader->depth, 1, 1, f);
	r += fwrite(&wheader->reserved, 1, 1, f);

	if (r != 11) {
		warn("Cannot write wthor header (r = %d)\n", r);
		return false;
	}

	return true;
}

/**
 * @brief Init wthor players.
 *
 * @param base Wthor base.
 */
static void wthor_players_init(WthorBase *base)
{
	base->n_players = 1;
	base->player = (char (*)[20]) malloc(sizeof (*base->player));
	if (base->player) {
	    strncpy(base->player[0], "?", 20); // used on purpose, as strncpy fills with '\0' the field name
	} else {
		warn("Cannot allocate Wthor players' array\n");
		base->n_players = 0;
	}
}

/**
 * @brief Load wthor players.
 *
 * @param base Wthor base.
 * @param file File name.
 */
static void wthor_players_load(WthorBase *base, const char *file)
{
	FILE *f;
	WthorHeader header;
	int i, r;

	r = base->n_players = 0;

	if ((f = fopen(file, "rb")) == NULL) {
		warn("Cannot open Wthor players' file %s\n", file);
		fprintf(stderr, "Creating a new %s file\n\n", file);
		wthor_players_init(base);
		return;
	}

	if (wthor_header_read(&header, f)) {;

		base->n_players = header.n;

		base->player = (char (*)[20]) malloc(base->n_players * sizeof (*base->player));
		if (base->player) {
			for (i = 0; i < base->n_players; ++i) {
				r += fread(base->player[i], 20, 1, f);
				base->player[i][19] = '\0'; // (a name of 20 characters has no end)
			}
		} else {
			warn("Cannot allocate Wthor players' array\n");
		}

		if (r != base->n_players) {
	        warn("Error while reading Wthor players' file %s %d/%d\n", file, r, base->n_players);
			base->n_players = 0;
		}
	}

	fclose(f);
}

/**
 * @brief Load wthor players.
 *
 * @param base Wthor base.
 * @param file File name.
 */
static bool wthor_players_save(WthorBase *base, const char *file)
{
	bool ok = false;
	FILE *f;
	WthorHeader header;
	int i, r;

	r = 0;

	wthor_header_set(&header, 0, base->n_players, 0);

	if ((f = fopen(file, "wb")) == NULL) {
		warn("Cannot open Wthor players' file %s\n", file);
		return false;
	}

	if (wthor_header_write(&header, f)) {

		for (i = 0; i < base->n_players; ++i) {
			r += fwrite(base->player[i], 20, 1, f);
		}

		if (r != base->n_players) {
	        warn("Error while reading Wthor players' file %s %d/%d\n", file, r, base->n_players);
			base->n_players = 0;
		} else {
			ok = true;
		}
	}

	if (fclose(f) != 0) ok = false;
	return ok;
}

/**
 * @brief Get a Wthor player's index.
 *
 * @param base Wthor base.
 * @param name Player's name.
 */
int wthor_player_get(WthorBase *base, const char *name) 
{
	int i, n;
	char (*player)[20];

	assert(base != NULL && name != NULL);
	assert(base->player != NULL && base->n_players > 0);

	for (i = 0; i < base->n_players; ++i) {
		if (strcmp(name, base->player[i]) == 0) return i;
	}
	
	n = base->n_players + 1;
	player = (char (*)[20]) realloc(base->player, n * sizeof (*base->player));
	if (player) {
		base->player = player;
		base->n_players = n;
		// (the whole field: up to v4.5.5-nikque.32 the bytes after the name went to WTHOR.JOU as they were in memory)
		strncpy(base->player[i], name, 20); // used on purpose, as strncpy fills with '\0' the field name
		base->player[i][19] = '\0'; // force null terminated string
	} else {
		warn("Cannot allocate Wthor players' array\n");
		i = 0;
	}

	return i;
}

/**
 * @brief Load wthor tournaments.
 *
 * @param base Wthor base.
 * @param file File name.
 */
static void wthor_tournaments_load(WthorBase *base, const char *file)
{
	FILE *f;
	WthorHeader header;
	int i, r;

	r = 0;
	base->n_tournaments = 0;

	if ((f = fopen(file, "rb")) == NULL) {
		warn("Cannot open Wthor tournaments' file %s\n", file);
		return;
	}

	if (wthor_header_read(&header, f)) {
		base->n_tournaments = header.n;

		base->tournament = (char (*)[26]) malloc(base->n_tournaments * sizeof (*base->tournament));
		if (base->tournament) {
			for (i = r = 0; i < base->n_tournaments; ++i) {
				r += fread(base->tournament[i], 26, 1, f);
				base->tournament[i][25] = '\0'; // (a name of 26 characters has no end)
			}
		} else {
			warn("Cannot allocate Wthor tournaments' array\n");
		}

		if (r != base->n_tournaments) {
			warn("Error while reading %s %d/%d\n", file, r, base->n_tournaments);
			base->n_tournaments = 0;
		}
	}

	fclose(f);
}

#if 0 // Unused
/**
 * @brief Load wthor tournaments.
 *
 * @param base Wthor base.
 * @param file File name.
 */
static void wthor_tournaments_save(WthorBase *base, const char *file)
{
	FILE *f;
	WthorHeader header;
	int i, r;

	r = 0;

	if ((f = fopen(file, "rw")) == NULL) {
		warn("Cannot open Wthor tournaments' file %s\n", file);
		return;
	}
	wthor_header_set(&header, 0, base->n_tournaments, 0);
	if (wthor_header_write(&header, f)) {
		for (i = r = 0; i < base->n_tournaments; ++i) {
			r += fwrite(base->tournament[i], 26, 1, f);
		}
		if (r != base->n_tournaments) {
			warn("Error while writing %s %d/%d\n", file, r, base->n_tournaments);
		}
	}

	fclose(f);
}
#endif

/**
 * @brief Initialize a Wthor base.
 *
 * @param base Wthor's base.
 */
void wthor_init(WthorBase *base)
{
	base->n_tournaments = 0;
	base->n_players = 0;
	base->n_games = 0;
	base->tournament = NULL;
	base->player = NULL;
	base->game = NULL;
	wthor_header_set(&base->header, 0, 0, 0);
}

/**
 * @brief Load a wthor base.
 *
 * @param base Wthor game base.
 * @param file Game file.
 */
bool wthor_load(WthorBase *base, const char *file)
{
	FILE *f;
	char path[FILENAME_MAX], players_path[FILENAME_MAX];
	int r = 0;

	wthor_init(base);

	if (!wthor_companion_path(file, "WTHOR.TRN", path, sizeof path) || !wthor_companion_path(file, "WTHOR.JOU", players_path, sizeof players_path)) return false;
	wthor_tournaments_load(base, path);
	wthor_players_load(base, players_path);

	if ((f = fopen(file, "rb")) != NULL) {
		if (wthor_header_read(&base->header, f) && base->header.board_size == 8 && (int) base->header.n_games >= 0) {
			base->n_games = base->header.n_games;

			base->game = (WthorGame*) malloc((base->n_games ? base->n_games : 1) * sizeof (WthorGame));
			if (base->game) {
				r = (fread(base->game, sizeof (WthorGame), base->n_games, f) == (unsigned) base->n_games);
				if (!r) warn("Error while reading %s\n", file);
			} else {
				warn("Cannot allocate whor games\n");
			}
			// a file that could not be read completely holds no game (up to v4.5.5-nikque.15 the games that were
			// not read were kept as they were in memory, and saved with the next games)
			if (!r) {
				free(base->game);
				base->game = NULL;
				base->n_games = base->header.n_games = 0; // (foreach_wthorgame goes through header.n_games)
			}
		}
		fclose(f);
	} else {
		base->n_games = 0;
		base->game = NULL;
		warn("Cannot open file %s\n", file);
	}

	return r != 0;
}

/**
 * @brief Free a wthor base.
 *
 * @param base Wthor game base.
 */
void wthor_free(WthorBase *base)
{
	free(base->player);
	free(base->tournament);
	free(base->game);
	wthor_init(base);
}

/**
 * @brief Save a wthor base.
 *
 * @param base Wthor game base.
 * @param file Game file name.
 */
bool wthor_save(WthorBase *base, const char *file)
{
	FILE *f;
	char path[FILENAME_MAX];
	int r = 0;

	if (!wthor_companion_path(file, "WTHOR.JOU", path, sizeof path)) return false; // (nothing is written)
	wthor_players_save(base, path);

	if ((f = fopen(file, "wb")) != NULL) {
		// the header, then every game (up to v4.5.5-nikque.15 the count of the writes, at most 2, was compared
		// with the count of the games, and any write was taken for a success); the year of the games and the
		// depth of their theoric scores are kept
		wthor_header_set_kept(&base->header, base->n_games);
		r = wthor_header_write(&base->header, f);
		if (r && base->n_games > 0) {
			r = (base->game != NULL && fwrite(base->game, sizeof (WthorGame), base->n_games, f) == (unsigned) base->n_games);
		}
		if (fclose(f) != 0) r = 0;
		if (!r) warn("Error while writing %s\n", file);
	} else {
		warn("Cannot open file %s\n", file);
	}

	return r;
}

/**
 * @brief Convert to a wthor base.
 *
 * @param base Generic game base.
 * @param wthor Wthor Game base.
 */
bool base_to_wthor(const Base *base, WthorBase *wthor)
{
	bool ok = true;
	int i, j;
	WthorGame *game;

	// (up to v4.5.5-nikque.15 the number of games was changed and the games lost when there was no memory)
	j = wthor->n_games;
	game = (WthorGame*) realloc(wthor->game, (j + base->n_games ? j + base->n_games : 1) * sizeof (WthorGame));
	if (game) {
		wthor->game = game;
		wthor->n_games = j + base->n_games;
		for (i = 0; i < base->n_games; ++i, ++j) {
			game_to_wthor(base->game + i, wthor->game + j);
			wthor->game[j].black = wthor_player_get(wthor, base->game[i].name[BLACK]); 
			wthor->game[j].white = wthor_player_get(wthor, base->game[i].name[WHITE]); 
		}
	} else {
		warn("Cannot allocate wthor games\n");
		ok = false;
	}

	return ok;
}

/**
 * @brief print a wthor game.
 *
 * @param base Wthor game base.
 * @param i game intex
 * @param f output stream.
 */
void wthor_print_game(WthorBase *base, int i, FILE *f)
{
	Game game;

	if (0 <= i && i < base->n_games) {
		// the numbers of a damaged file may be beyond the names (or the names were not read)
		const WthorGame *g = base->game + i;
		const char *tournament = (base->tournament && 0 <= g->tournament && g->tournament < base->n_tournaments) ? base->tournament[g->tournament] : "?";
		const char *black = (base->player && 0 <= g->black && g->black < base->n_players) ? base->player[g->black] : "?";
		const char *white = (base->player && 0 <= g->white && g->white < base->n_players) ? base->player[g->white] : "?";

		fprintf(f, "Game #%d: %s: %4d - %s vs. %s: ",
			i, tournament, base->header.game_year, black, white);

		wthor_to_game(base->game + i, &game);
		game_export_text(&game, f);

		fprintf(f, "Theoric score %d empties : %+02d, ", base->header.depth, base->game[i].theoric_score);
		fprintf(f, "Score final : %+02d (as black disc count.)\n", base->game[i].score);
	}
}

/**
 * @brief Get a position from a Wthor game.
 *
 * @param game Wthor game.
 * @param n_empties number of empty square of the position to find.
 * @param board Board found.
 * @param player
 */
static void wthorgame_get_board(WthorGame *game, const int n_empties, Board *board, int *player)
{
	int i;
	Move move;
	char s_move[4];

	*player = BLACK; board_init(board);
	// (the depth of the header may be negative, and a move of a damaged file may be no square)
	for (i = 0; i < 60 && i < 60 - n_empties && game->x[i]; ++i) {
		const int x = move_from_wthor(game->x[i]);
		if (board_is_pass(board)) {
			board_pass(board); *player ^= 1;
		}
		if (x < A1 || x > H8) {
			warn("Illegal move %d\n", game->x[i]);
			break;
		}
		board_get_move_flip(board, x, &move);
		if (board_check_move(board, &move)) {
			board_update(board, &move); *player ^= 1;
		} else {
			warn("Illegal move %s\n", move_to_string(move.x, *player, s_move));
			break;
		}
	}
}

/**
 * @brief Verify that a PV does not contain errors.
 *
 * @param init_board Initial board.
 * @param pv PV to check
 * @param search Search engine.
 */
int pv_check(const Board *init_board, Line *pv, Search *search)
{
	Game game;

	line_to_game(init_board, pv, &game);
	return game_analyze(&game, search, board_count_empties(init_board), false);
}

/**
 * @brief Test Search with a wthor base.
 *
 * @param file Game File.
 * @param search Search.
 */
void wthor_test(const char *file, Search *search)
{
	WthorBase base;
	WthorGame *wthor;
	Board board;
	int player;
	int score;
	int n_empties;
	int n_failure;
	long long n_nodes;
	long long t;
	int n_err;

	if (wthor_load(&base, file)) {

		if (search->options.verbosity == 1) {
			if (search->options.header) puts(search->options.header);
			if (search->options.separator) puts(search->options.separator);
		}

		n_failure = 0;
		n_nodes = 0;
		t = 0;

		foreach_wthorgame(wthor, base) {
			wthorgame_get_board(wthor, base.header.depth, &board, &player);
			n_empties = board_count_empties(&board);
			if (n_empties != base.header.depth && !board_is_game_over(&board)) {
				warn("Incomplete or Illegal game: %d empties\n", n_empties);
				wthor_print_game(&base, wthor - base.game, stderr);
				continue;
			}

			if (player == WHITE) score = 64 - 2 * wthor->theoric_score;
			else score = 2 * wthor->theoric_score - 64;
			if (abs(score) > 64) {
				warn("Impossible theoric score:\n");
				wthor_print_game(&base, wthor - base.game, stderr);
				continue;
			}

			search_cleanup(search);
			search_set_board(search, &board, player);
			search_set_level(search, 60, base.header.depth);
			search_run(search);
			if (search->options.verbosity) putchar('\n');
			n_nodes += search->result->n_nodes;
			t += search->result->time;
			if (score != search->result->score) {
				warn("Wrong theoric score: %+d (Wthor) instead of %+d (Edax)\n", score, search->result->score);
				wthor_print_game(&base, wthor - base.game, stderr);
				++n_failure;
				assert(false); // stop here when debug is on
			}

			if (options.pv_check) {
				Line pv;
				line_copy(&pv, &search->result->pv, 0);
				n_err = pv_check(&board, &pv, search);
				if (n_err) {
					char s[80];
					warn("Wrong pv:\n");
					board_print(&board, player, stderr);
					fprintf(stderr, "setboard %s\nplay ", board_to_string(&board, player, s));
					line_print(&pv, 200, " ", stderr);
					putc('\n', stderr); putc('\n', stderr);
					assert(false); // stop here when debug is on
				}
			}

			if (search->options.verbosity == 0) {
				printf("%s  game: %4d, error: %2d ; ", file, (int)(wthor - base.game) + 1, n_failure);
				printf("%lld n, ", n_nodes); time_print(t, false, stdout); putchar('\r');
				fflush(stdout);
			}
		}
		if (search->options.verbosity == 1) {
			if (search->options.separator) puts(search->options.separator);
		}
		putchar('\n');

		wthor_free(&base);
	}
	return;
}

/**
 * @brief Test Eval with a wthor base.
 *
 * Given a wthor file, compare the result of a search to the theoretical scores.
 *
 * @param file Game File.
 * @param search Search.
 * @param histogram output array.
 */
void wthor_eval(const char *file, Search *search, unsigned long long histogram[129][65])
{
	WthorBase base;
	WthorGame *wthor;
	Board board;
	int player;
	int score;
	int n_empties;

	if (wthor_load(&base, file)) {
		foreach_wthorgame(wthor, base) {
			wthorgame_get_board(wthor, base.header.depth, &board, &player);
			n_empties = board_count_empties(&board);
			if (n_empties != base.header.depth && !board_is_game_over(&board)) {
				continue;
			}

			if (player == WHITE) score = 64 - 2 * wthor->theoric_score;
			else score = 2 * wthor->theoric_score - 64;
			if (abs(score) > 64) {
				continue;
			}

			search_cleanup(search);
			search_set_board(search, &board, player);
			search_set_level(search, options.level, base.header.depth);
			search_run(search);
			++histogram[search->result->score + 64][(score + 64) / 2];
		}
		wthor_free(&base);
	}
	return;
}

/**
 * @brief Change players to "Edax (delorme)" and tourney to "Etudes"
 * in a wthor base.
 *
 * @param file Wthor game file.
 */
void wthor_edaxify(const char *file)
{
	WthorBase base;
	WthorGame *wthor;

	if (wthor_load(&base, file)) {
		foreach_wthorgame(wthor, base) {
			wthor->black = 1368; // "Edax (delorme)"
			wthor->white = 1368; // "Edax (delorme)"
			wthor->tournament = 157; // "Etudes"
		}
		wthor_save(&base, file);
		wthor_free(&base);
	}
}

/**
 * @brief Initialize a game database.
 *
 * @param base Game base.
 */
void base_init(Base *base)
{
	base->size = 0;
	base->n_games = 0;
	base->game = NULL;
}

/**
 * @brief Free resources of a game database.
 *
 * @param base Game base.
 */
void base_free(Base *base)
{
	free(base->game);
	base_init(base);
}

/**
 * @brief Add a game to a game database.
 *
 * @param base Game base.
 * @param game Game to add.
 * @return false if the memory is exhausted (the game is not added).
 */
bool base_append(Base *base, const Game *game)
{
	if (base->n_games == base->size) {
		Game *ptr;
		int size = base->size;

		if (size == 0) size = 16384;
		else size *= 2;
		ptr = (Game*) realloc(base->game, size * sizeof (Game));
		if (ptr == NULL) {
			error("cannot reallocate base game");
			return false;
		}
		base->game = ptr;
		base->size = size;
	}
	base->game[base->n_games++] = *game;
	return true;
}

/** A game and its place in the base, to sort the games (base_unique). */
typedef struct {
	const Game *game;
	int i;
} BaseGameRef;

/** @brief Order of the games, on what game_equals() compares; the place in the base last. */
static int base_game_compare(const void *a, const void *b)
{
	const BaseGameRef *ra = (const BaseGameRef*) a, *rb = (const BaseGameRef*) b;
	const Game *g1 = ra->game, *g2 = rb->game;
	int r;

	if (g1->hash != g2->hash) return g1->hash < g2->hash ? -1 : 1;
	if (g1->player != g2->player) return g1->player < g2->player ? -1 : 1;
	if (g1->initial_board.player != g2->initial_board.player) return g1->initial_board.player < g2->initial_board.player ? -1 : 1;
	if (g1->initial_board.opponent != g2->initial_board.opponent) return g1->initial_board.opponent < g2->initial_board.opponent ? -1 : 1;
	if (g1->date.year != g2->date.year) return g1->date.year < g2->date.year ? -1 : 1;
	if (g1->date.month != g2->date.month) return g1->date.month < g2->date.month ? -1 : 1;
	if (g1->date.day != g2->date.day) return g1->date.day < g2->date.day ? -1 : 1;
	if (g1->date.hour != g2->date.hour) return g1->date.hour < g2->date.hour ? -1 : 1;
	if (g1->date.minute != g2->date.minute) return g1->date.minute < g2->date.minute ? -1 : 1;
	if (g1->date.second != g2->date.second) return g1->date.second < g2->date.second ? -1 : 1;
	if ((r = strcmp(g1->name[0], g2->name[0])) != 0) return r;
	if ((r = strcmp(g1->name[1], g2->name[1])) != 0) return r;
	if ((r = memcmp(g1->move, g2->move, sizeof g1->move)) != 0) return r;
	return ra->i - rb->i;
}

#define GAME_LESS(a, b) (base_game_compare(&(a), &(b)) < 0)
// (the same games keep the order of the base: the order is total)
DEFINE_SORT(games_sort, BaseGameRef, GAME_LESS, base_game_compare)

/**
 * @brief Make games unique in the game database.
 *
 * The first of the same games is kept, in the order of the base. The games are sorted to find the same ones
 * (up to v4.5.5-nikque.15 each game was compared with all the games kept before it: hours for a million games).
 *
 * @param base Game base.
 */
void base_unique(Base *base)
{
	int i, j, k;
	bool found;
	BaseGameRef *ref;
	bool *keep;

	ref = (BaseGameRef*) malloc((base->n_games ? base->n_games : 1) * sizeof (BaseGameRef));
	keep = (bool*) malloc((base->n_games ? base->n_games : 1) * sizeof (bool));
	if (ref && keep) {
		for (i = 0; i < base->n_games; ++i) {
			ref[i].game = base->game + i;
			ref[i].i = i;
		}
		games_sort(ref, base->n_games);
		for (i = 0; i < base->n_games; ++i) {
			keep[ref[i].i] = (i == 0 || !game_equals(ref[i - 1].game, ref[i].game));
		}
		for (i = k = 0; i < base->n_games; ++i) {
			if (keep[i]) base->game[k++] = base->game[i];
		}
	} else { // no memory: as before
		for (i = k = 0; i < base->n_games; ++i) {
			for (j = 0, found = false; j < k && !found; ++j)
				found = game_equals(base->game + j, base->game + i);
			if (!found) base->game[k++] = base->game[i];
		}
	}
	free(ref);
	free(keep);

	base->n_games = k;
}

/**
 * @brief Read a game from a file: GAME_IMPORT_READ, GAME_IMPORT_NONE (no game left), or GAME_IMPORT_INCOMPLETE
 * (a game cut by the end of the file: the file is not read completely).
 */
static int base_import_text(Game *game, FILE *f) { return game_import_text_checked(game, f) ? GAME_IMPORT_READ : GAME_IMPORT_NONE; }
static int base_import_pgn(Game *game, FILE *f) { return game_import_pgn_checked(game, f) ? GAME_IMPORT_READ : GAME_IMPORT_NONE; }
static int base_import_edx(Game *game, FILE *f) { return game_read_checked(game, f) ? GAME_IMPORT_READ : GAME_IMPORT_NONE; }
static int base_import_wthor(Game *game, FILE *f) { game_import_wthor(game, f); return feof(f) ? GAME_IMPORT_NONE : GAME_IMPORT_READ; }

/**
 * @brief Read the games of a file, one after the other.
 *
 * @param file Game filename.
 * @param take Given each game; false stops the reading.
 * @param to Given to take.
 * @param complete Set to true if the file was opened and read to its end, without any error.
 * @return the number of games taken.
 */
static int base_read_games(const char *file, bool (*take)(void*, const Game*), void *to, bool *complete)
{
	int (*load)(Game*, FILE*) = base_import_text;
	Game game;
	FILE *f;
	char ext[8];
	int l, n_games = 0, r;
	bool ok = true;
	WthorHeader header;

	*complete = false;
	l = strlen(file);
	if (l < 4) { warn("Unknown game format extension: %s\n", file); return 0; }
	strcpy(ext, file + l - 4); string_to_lowercase(ext);
	if (strcmp(ext, ".txt") == 0) load = base_import_text;
	else if (strcmp(ext, ".ggf") == 0) load = game_import_ggf_checked;
	else if (strcmp(ext, ".sgf") == 0) load = game_import_sgf_checked;
	else if (strcmp(ext, ".pgn") == 0) load = base_import_pgn;
	else if (strcmp(ext, ".wtb") == 0) load = base_import_wthor;
	else if (strcmp(ext, ".edx") == 0) load = base_import_edx;
	else {
		warn("Unknown game format extension: %s\n", ext);
		return 0;
	}

	if (load == base_import_wthor || load == base_import_edx) f = fopen(file, "rb");
	else f = fopen(file, "r");
	if (f == NULL) {
		warn("Cannot open file %s\n", file);
		return 0;
	}

	info("loading games...");
	if (load == base_import_wthor && !wthor_header_read(&header, f)) ok = false;
	// the last game of a file is read when the end of the file is reached (a text file without an end of line
	// after its last game, a pgn file): up to v4.5.5-nikque.15 it was not added
	while ((r = load(&game, f)) == GAME_IMPORT_READ) {
		if (ferror(f) || n_games == INT_MAX || !take(to, &game)) { ok = false; break; }
		++n_games;
	}
	if (ferror(f)) ok = false;
	// a ggf or sgf game cut by the end of the file: the file is not complete (base correct does not replace it)
	if (r == GAME_IMPORT_INCOMPLETE) ok = false;
	// a binary file ends after its last game: a truncated or damaged game stops before (up to v4.5.5-nikque.15
	// the games read until then were taken for the whole file)
	if (load == base_import_edx && (!feof(f) || ftell(f) % (long) sizeof (Game) != 0)) ok = false;
	// a wthor file holds the count of its games in its header: fewer games is a truncated file
	if (ok && load == base_import_wthor && n_games < header.n_games) ok = false;
	info("done (%d games loaded)\n", n_games);
	if (!ok) warn("%s could not be read completely (%d games loaded)\n", file, n_games);

	fclose(f);
	*complete = ok;

	return n_games;
}

/** @brief Take a game into a base (see base_read_games). */
static bool base_take_game(void *to, const Game *game)
{
	return base_append((Base*) to, game);
}

/**
 * @brief Load a game database, telling whether the whole file was read.
 *
 * @param base Game base (the games are added to it).
 * @param file Game filename.
 * @param complete Set to true if the file was opened and read to its end, without any error.
 * @return true if the base holds games.
 */
static bool base_load_checked(Base *base, const char *file, bool *complete)
{
	base_read_games(file, base_take_game, base, complete);
	return base->n_games > 0;
}

/**
 * @brief Load a game database.
 *
 * @param base Game base.
 * @param file Game filename.
 * @return true if the base holds games, and the file was read to its end without any error.
 */
bool base_load(Base *base, const char *file)
{
	bool complete;
	return base_load_checked(base, file, &complete) && complete;
}

/**
 * @brief Save a game database.
 *
 * The games are added to the games the file already holds.
 *
 * @param base Game base.
 * @param file Game filename.
 * @return true if the games were written (false: the file could not be read completely, or written).
 */
bool base_save(const Base *base, const char *file)
{
	// the games of the file, then the new ones, to another file that replaces it once they are all written
	// (up to v4.5.5-nikque.32 the file was written in place: a write that failed lost the games that it held)
	BaseWriter *w = base_writer_open(file);
	int i;

	if (w == NULL) return false;
	for (i = 0; i < base->n_games; ++i) {
		if (!base_writer_add(w, base->game + i)) {
			warn("Cannot save the games to %s: it was not replaced\n", file);
			base_writer_abort(w);
			return false;
		}
	}
	return base_writer_close(w);
}

/**
 * @brief Add the games of a file to another file (base convert, base append, base unique).
 *
 * Nothing is written if the input cannot be read to its end, or if the two names are of the same file (its
 * games would be there twice).
 *
 * @param from Input game file.
 * @param to Output game file (its games are kept).
 * @param unique Only once each game of the input (base unique).
 * @return true if the games were written.
 */
bool base_add_file(const char *from, const char *to, const bool unique)
{
	Base base;
	bool complete, ok = false;

	if (*to == '\0') {
		warn("The name of the output file is missing\n");
		return false;
	}
	if (file_same(from, to)) {
		warn("%s and %s are the same file: its games would be there twice; nothing is written\n", from, to);
		return false;
	}
	base_init(&base);
	base_load_checked(&base, from, &complete);
	if (!complete) {
		warn("Cannot read all the games of %s: nothing is written to %s\n", from, to);
	} else {
		if (unique) base_unique(&base);
		ok = base_save(&base, to);
	}
	base_free(&base);
	return ok;
}

/*
 * A game file written game by game.
 *
 * The games of the file are kept: they are copied to another file, read to their end, and the new games are
 * written after them; the file is only replaced when everything is written (a file that cannot be read to its
 * end, or a write that fails, leaves it as it was). The games are not held in memory.
 */
struct BaseWriter {
	FILE *f;
	void (*save)(const Game*, FILE*); /**< NULL: wthor */
	WthorBase wthor;                  /**< players, and the header */
	long long n_games;                /**< games of the file */
	bool ok;
	char file[FILENAME_MAX + 1];
	char tmp_file[FILENAME_MAX + 1];
};

/**
 * @brief Write a game to a file being written.
 *
 * @param w File being written.
 * @param game Game.
 * @return false if it cannot be written (the file will be left as it was).
 */
bool base_writer_add(BaseWriter *w, const Game *game)
{
	if (!w->ok) return false;
	if (w->n_games >= INT_MAX) {
		warn("%s cannot hold more than %d games\n", w->file, INT_MAX);
		return w->ok = false;
	}
	if (w->save) {
		w->save(game, w->f);
	} else {
		WthorGame thor;
		const int black = wthor_player_get(&w->wthor, game->name[BLACK]);
		const int white = wthor_player_get(&w->wthor, game->name[WHITE]);

		if (w->wthor.n_players > USHRT_MAX) { // (a game holds the index of a player in 16 bits)
			warn("%s cannot hold more than %d players\n", w->file, USHRT_MAX);
			return w->ok = false;
		}
		game_to_wthor(game, &thor);
		thor.black = black;
		thor.white = white;
		if (fwrite(&thor, sizeof thor, 1, w->f) != 1) w->ok = false;
	}
	if (ferror(w->f)) w->ok = false;
	if (w->ok) ++w->n_games;
	else warn("Error while writing %s\n", w->tmp_file);
	return w->ok;
}

/** @brief Take a game of the file into the file being written (see base_read_games). */
static bool base_writer_take(void *to, const Game *game)
{
	return base_writer_add((BaseWriter*) to, game);
}

/**
 * @brief Copy the games of a wthor file as they are (with their players, tournament and scores).
 *
 * @param w File being written.
 * @param f The file.
 * @return false if it cannot be read to the end of its games.
 */
static bool base_writer_copy_wthor(BaseWriter *w, FILE *f)
{
	WthorHeader header;
	WthorGame thor;
	int i;

	if (!wthor_header_read(&header, f) || header.board_size != 8 || header.n_games < 0) return false;
	for (i = 0; i < header.n_games; ++i) {
		if (fread(&thor, sizeof thor, 1, f) != 1) return false;
		if (fwrite(&thor, sizeof thor, 1, w->f) != 1) return false;
	}
	w->n_games = header.n_games;
	// the year of the games and the depth of their theoric scores of the file are kept (see base_writer_close)
	w->wthor.header.game_year = header.game_year;
	w->wthor.header.depth = header.depth;
	return true;
}

/**
 * @brief Release a file being written, without replacing the file.
 *
 * @param w File being written (released).
 */
void base_writer_abort(BaseWriter *w)
{
	if (w == NULL) return;
	if (w->f) {
		fclose(w->f);
		remove(w->tmp_file);
	}
	wthor_free(&w->wthor);
	free(w);
}

/**
 * @brief Start to write games to a file, after the games it holds.
 *
 * @param file Game filename (txt, ggf, sgf, pgn, wtb, edx).
 * @return the file being written, or NULL (said; the file is left as it is).
 */
BaseWriter* base_writer_open(const char *file)
{
	const size_t l = strlen(file);
	BaseWriter *w;
	FILE *old;
	char ext[8];
	bool is_wthor = false, complete = true;

	if (l < 4) { warn("Unknown game format extension: %s\n", file); return NULL; }
	if (l + 16 > FILENAME_MAX) { warn("The file name is too long: %s\n", file); return NULL; }
	w = (BaseWriter*) calloc(1, sizeof *w);
	if (w == NULL) { warn("Cannot allocate the memory to write %s\n", file); return NULL; }
	wthor_init(&w->wthor);
	strcpy(w->file, file);
	sprintf(w->tmp_file, "%s.writing%s", file, file + l - 4); // (the same extension: the same format)
	strcpy(ext, file + l - 4); string_to_lowercase(ext);
	if (strcmp(ext, ".txt") == 0) w->save = game_export_text;
	else if (strcmp(ext, ".ggf") == 0) w->save = game_export_ggf;
	else if (strcmp(ext, ".sgf") == 0) w->save = game_export_sgf;
	else if (strcmp(ext, ".pgn") == 0) w->save = game_export_pgn;
	else if (strcmp(ext, ".edx") == 0) w->save = game_write;
	else if (strcmp(ext, ".wtb") == 0) is_wthor = true;
	else {
		warn("Unknown game format extension: %s\n", ext);
		base_writer_abort(w);
		return NULL;
	}
	if (is_wthor) {
		char path[FILENAME_MAX + 16];

		if (!wthor_companion_path(file, "WTHOR.JOU", path, sizeof path)) {
			base_writer_abort(w);
			return NULL;
		}
		wthor_players_load(&w->wthor, path);
		if (w->wthor.n_players <= 0 || w->wthor.player == NULL) {
			warn("Cannot read the players of %s: it was not replaced\n", file);
			base_writer_abort(w);
			return NULL;
		}
	}
	w->f = fopen(w->tmp_file, (is_wthor || w->save == game_write) ? "wb" : "w");
	if (w->f == NULL) {
		warn("Cannot open file %s\n", w->tmp_file);
		base_writer_abort(w);
		return NULL;
	}
	w->ok = true;
	if (is_wthor) { // (written again at the end, with the count of the games)
		wthor_header_set(&w->wthor.header, 0, 0, 0);
		w->ok = wthor_header_write(&w->wthor.header, w->f);
	}

	// the games of the file are kept: it must be read completely
	if ((old = fopen(file, "rb")) != NULL) {
		if (is_wthor) complete = base_writer_copy_wthor(w, old);
		fclose(old);
		if (!is_wthor) base_read_games(file, base_writer_take, w, &complete);
		if (!complete) {
			warn("Cannot read all the games of %s: it was not replaced\n", file);
			base_writer_abort(w);
			return NULL;
		}
	} else if (errno != ENOENT) { // (a file that is there, and cannot be read: it is not replaced)
		warn("Cannot open file %s: it was not replaced\n", file);
		base_writer_abort(w);
		return NULL;
	}
	return w;
}

/**
 * @brief End a file being written: it replaces the file.
 *
 * @param w File being written (released).
 * @return true if the file was replaced; else it is left as it was.
 */
bool base_writer_close(BaseWriter *w)
{
	char path[FILENAME_MAX + 16];
	bool ok = w->ok;

	if (ok && w->save == NULL) {
		wthor_header_set_kept(&w->wthor.header, (unsigned int) w->n_games); // (the year and depth of the file, if any)
		if (fseek(w->f, 0, SEEK_SET) != 0 || !wthor_header_write(&w->wthor.header, w->f)) ok = false;
	}
	if (ferror(w->f)) ok = false;
	if (fclose(w->f) != 0) ok = false;
	w->f = NULL;
	if (ok && w->save == NULL) { // the players first: their list only grows, so it also suits the games that are there
		char tmp_path[FILENAME_MAX + 32];

		ok = wthor_companion_path(w->file, "WTHOR.JOU", path, sizeof path);
		if (ok) {
			strcpy(tmp_path, path); strcat(tmp_path, ".writing");
			ok = wthor_players_save(&w->wthor, tmp_path) && file_replace(tmp_path, path);
			if (!ok) remove(tmp_path);
		}
	}
	if (ok && !file_replace(w->tmp_file, w->file)) ok = false;
	if (!ok) {
		warn("Cannot save the games to %s: it was not replaced\n", w->file);
		remove(w->tmp_file);
	}
	wthor_free(&w->wthor);
	free(w);
	return ok;
}

/**
 * @brief Replace a game database by the games of a base (base correct, base complete).
 *
 * The games are saved to another file first: the file is only replaced once they are written.
 * Up to v4.5.5-nikque.15 the file was removed before the games were saved.
 *
 * @param base Game base.
 * @param file Game filename.
 * @return true if the file was replaced.
 */
bool base_replace(const Base *base, const char *file)
{
	const int l = strlen(file);
	char *tmp_file;
	bool ok;

	if (l < 4) { warn("Unknown game format extension: %s\n", file); return false; }
	tmp_file = (char*) malloc(l + 16);
	if (tmp_file == NULL) return false;
	sprintf(tmp_file, "%s.saving%s", file, file + l - 4); // (the same extension: the same format)
	remove(tmp_file);
	ok = base_save(base, tmp_file);
	if (ok && !file_replace(tmp_file, file)) ok = false;
	if (!ok) {
		warn("Cannot save the games to %s: it was not replaced\n", file);
		remove(tmp_file);
	}
	free(tmp_file);
	return ok;
}

/**
 * @brief Open the file of the problems of a game database (base_to_problem, base_to_FEN).
 *
 * @param n_empties Number of empties of the problems.
 * @param problem Problems filename.
 * @return the file; NULL (with a warning) if the number of empties is out of range or if the file cannot be opened.
 */
static FILE* base_problem_open(const int n_empties, const char *problem)
{
	FILE *f;

	if (n_empties < 0 || n_empties > 60) { // (a game holds 60 moves)
		warn("%s: %d empties is out of range\n", problem, n_empties);
		return NULL;
	}
	f = fopen(problem, "w");
	if (f == NULL) warn("Cannot open file %s\n", problem); // (up to v4.5.5-nikque.15 a file that could not be opened stopped Edax)
	return f;
}

/**
 * @brief Convert a game database to a set of problems.
 *
 * @param base Game base.
 * @param n_empties Number of empties.
 * @param problem Problems filename.
 */
void base_to_problem(Base *base, const int n_empties, const char *problem)
{
	int i;
	Board board;
	char s[80];
	FILE *f;

	f = base_problem_open(n_empties, problem);
	if (f == NULL) return;

	for (i = 0; i < base->n_games; ++i) {
		if (game_get_board(base->game + i, 60 - n_empties, &board)) {
			board_to_string(&board, n_empties & 1, s);
			fprintf(f, "%s\n", s);
		}
	}

	fclose(f);
}

/**
 * @brief Convert a game database to a set of problems.
 *
 * @param base Game base.
 * @param n_empties Number of empties.
 * @param problem Problems filename.
 */
void base_to_FEN(Base *base, const int n_empties, const char *problem)
{
	int i;
	Board board;
	FILE *f;

	f = base_problem_open(n_empties, problem);
	if (f == NULL) return;

	for (i = 0; i < base->n_games; ++i) {
		if (game_get_board(base->game + i, 60 - n_empties, &board)) {
			board_print_FEN(&board, n_empties & 1, f);
			putc('\n', f);
		}
	}

	fclose(f);
}

/**
 * @brief Base analysis.
 *
 * @param base Game base.
 * @param search Search engine.
 * @param n_empties Number of empties.
 * @param apply_correction Correct bad moves from the games.
 */
void base_analyze(Base *base, Search *search, const int n_empties, const bool apply_correction)
{
	int i;
	int n_error;

	for (i = 0; i < base->n_games; ++i) {
		if (game_score(base->game + i) == 0) continue;
		game_export_text(base->game + i, stdout);
		n_error = game_analyze(base->game + i, search, n_empties, apply_correction);
		if (n_error) {
			printf("Game #%d contains %d errors", i, n_error);
			if (apply_correction) {
				if (game_analyze(base->game + i, search, n_empties, false)) printf("... correction failed! ***BUG DETECTED!***\n");
				else printf("... corrected!\n");
			} else putchar('\n');
		}
		printf("%d/%d %.1f %% done.\r", i + 1, base->n_games, 100.0 * (i + 1) / base->n_games); fflush(stdout);
	}
}

/*
 * base complete: several games completed at the same time.
 *
 * A thread completes a copy of a game. The games are put back, and their searches reported to the observer of
 * the main search, in the order of the file, whatever the order they end in. A timed save (book-save-interval,
 * to <file>.completing.<ext>) is done between two games put back: it holds no game being completed. When no
 * game is left to start, the threads that end are given to the searches that still run, as for the planned
 * searches of the book (not with book-reproducible).
 */

#define COMPLETE_SLOTS 4   /**< games completed ahead of the next one to put back, for each thread */

/** a game completed ahead of its turn */
typedef struct CompleteSlot {
	Game game;
	Result *report;            /**< results to report */
	int n_report, report_size;
	int n;                     /**< searches done */
	int index;                 /**< game; -1: free */
	bool ready;
} CompleteSlot;

struct CompleteWorker;

/** state shared by the threads of base complete */
typedef struct CompleteShared {
	Base *base;
	Search *main_search;       /**< its observer gets the results */
	const char *save_file;     /**< file of the timed saves (NULL: none) */
	unsigned long long t;      /**< time of the last save */
	Lock lock;
	Condition cond;            /**< a slot is free */
	CompleteSlot *slot;
	int n_slot;
	int next, applied, n_completed;
	struct CompleteWorker *worker;
	int n_worker, n_threads;
} CompleteShared;

/** a thread of base complete */
typedef struct CompleteWorker {
	CompleteShared *shared;
	Search *search;
	CompleteSlot *current;
	int n_tasks;               /**< threads of its search */
	int want;                  /**< threads that its search should have */
	int run_tasks;             /**< threads of the search that runs (0: none) */
	bool busy;
} CompleteWorker;

/** @brief Give the threads of the workers without a game to the searches that still run (lock held). */
static void complete_share_threads(CompleteShared *s)
{
	int i, n_busy = 0, n;

	if (options.book_reproducible) return; // (one-thread searches)
	for (i = 0; i < s->n_worker; ++i) if (s->worker[i].busy) ++n_busy;
	if (n_busy == 0) return;
	n = MIN(s->n_threads / n_busy, MAX_THREADS - 1);
	for (i = 0; i < s->n_worker; ++i) {
		CompleteWorker *w = s->worker + i;

		if (w->busy && n >= 2 * w->want) w->want = n;
		if (w->busy && w->run_tasks > 0 && w->run_tasks < w->want) search_stop_all(w->search, STOP_ON_DEMAND);
	}
}

/** @brief A search of a game being completed (see game_complete_with). */
static void complete_run(void *v, Search *search, const Board *board, const int player)
{
	CompleteWorker *w = (CompleteWorker*) v;
	CompleteShared *s = w->shared;
	int n;

	for (;;) {
		for (;;) {
			lock(s);
			n = w->want;
			if (n == w->n_tasks) w->run_tasks = n; // (a stop is only asked while run_tasks is set)
			unlock(s);
			if (n == w->n_tasks) break;
			search_set_task_number(search, n);
			w->n_tasks = n;
		}
		search_set_board(search, board, player);
		search_set_level(search, options.level, search->eval.n_empties);
		search_run(search);
		lock(s);
		w->run_tasks = 0;
		n = w->want;
		unlock(s);
		if (search->stop != STOP_ON_DEMAND || n <= w->n_tasks) break; // the search ended by itself
		search->options.keep_date = true; // searched again with more threads, and what the hash tables kept
	}
	search->options.keep_date = false;
}

/** @brief Keep a result to report (see game_complete_with). */
static void complete_report(void *v, const Result *result)
{
	CompleteSlot *e = ((CompleteWorker*) v)->current;

	if (e->n_report == e->report_size) {
		const int size = e->report_size * 2 + 8;
		Result *report = (Result*) realloc((void*) e->report, size * sizeof *report);

		if (report == NULL) return; // (no memory: this result is not shown)
		e->report = report;
		e->report_size = size;
	}
	memcpy((void*) (e->report + e->n_report++), (const void*) result, sizeof *result);
}

/** @brief Save the games completed so far, when the interval of the timed saves is over. */
static void complete_save_timed(const Base *base, const char *save_file, unsigned long long *t)
{
	bool due;

	if (save_file == NULL) return;
	due = options.book_save_interval > 0 && real_clock() - (long long) *t >= (long long) options.book_save_interval * 60000LL;
#ifdef BOOK_TEST_COUNTS
	if (book_test_save_ms >= 0) due = real_clock() - (long long) *t >= book_test_save_ms;
#endif
	if (!due) return;
	base_replace(base, save_file);
#ifdef BOOK_TEST_COUNTS
	if (book_test_saved) book_test_saved(save_file);
#endif
	*t = real_clock();
}

/** @brief Put back the games that are ready, in their order (lock held). */
static void complete_apply_ready(CompleteShared *s)
{
	bool moved = false;
	int i;

	for (;;) {
		CompleteSlot *e = s->slot + s->applied % s->n_slot;

		if (e->index != s->applied || !e->ready) break;
		s->base->game[s->applied] = e->game;
		for (i = 0; i < e->n_report; ++i) {
			putchar('\n');
			s->main_search->observer(e->report + i);
		}
		if (e->n > 0) ++s->n_completed;
		if (e->n > 0 || (s->applied % 1000) == 0) {
			printf("%d/%d games completed (%.1f %% done).\r", s->n_completed, s->applied + 1, 100.0 * (s->applied + 1) / s->base->n_games);
			fflush(stdout);
		}
		e->index = -1;
		e->n_report = 0;
		++s->applied;
		moved = true;
		complete_save_timed(s->base, s->save_file, &s->t);
	}
	if (moved) condition_broadcast(s);
}

/** @brief A thread of base complete. */
static void* complete_worker_run(void *v)
{
	CompleteWorker *w = (CompleteWorker*) v;
	CompleteShared *s = w->shared;

	lock(s);
	for (;;) {
		CompleteSlot *e;

		while (s->next < s->base->n_games && s->next - s->applied >= s->n_slot) condition_wait(s);
		if (s->next >= s->base->n_games) {
			w->busy = false;
			complete_share_threads(s);
			break;
		}
		e = s->slot + s->next % s->n_slot;
		e->index = s->next++;
		e->ready = false;
		e->game = s->base->game[e->index];
		w->current = e;
		w->busy = true;
		if (s->next >= s->base->n_games) {
			complete_share_threads(s);
			condition_broadcast(s); // (the threads that wait for a slot have nothing left to start)
		}
		unlock(s);

		e->n = game_complete_with(&e->game, w->search, complete_run, complete_report, w);

		lock(s);
		w->busy = false;
		e->ready = true;
		complete_apply_ready(s);
	}
	unlock(s);
	return NULL;
}

/**
 * @brief Complete the games of a base at the same time.
 *
 * @return false if it was not done (one game at a time, or no memory): nothing was changed.
 */
static bool base_complete_parallel(Base *base, Search *search, const char *save_file)
{
	CompleteShared shared;
	CompleteWorker w[MAX_THREADS];
	Search **pool;
	const int n = MIN(MIN(book_store_task_count(), base->n_games), MAX_THREADS - 1);
	int i, n_tasks;

	if (n < 1 || (n == 1 && !options.book_reproducible)) return false; // (one game after the other, with the main search)
	n_tasks = options.book_reproducible ? 1 : MAX(1, MIN(book_store_thread_count() / n, MAX_THREADS - 1));
	pool = book_game_searches(n, n_tasks);
	if (pool == NULL) return false;
	shared.n_slot = COMPLETE_SLOTS * n;
	shared.slot = (CompleteSlot*) calloc(shared.n_slot, sizeof *shared.slot);
	if (shared.slot == NULL) return false;
	for (i = 0; i < shared.n_slot; ++i) shared.slot[i].index = -1;
	shared.base = base;
	shared.main_search = search;
	shared.save_file = save_file;
	shared.t = real_clock();
	shared.next = shared.applied = shared.n_completed = 0;
	shared.worker = w;
	shared.n_worker = n;
	shared.n_threads = book_store_thread_count();
	lock_init(&shared);
	condition_init(&shared);
	for (i = 0; i < n; ++i) {
		w[i].shared = &shared;
		w[i].search = pool[i];
		w[i].current = NULL;
		w[i].n_tasks = search_count_tasks(pool[i]);
		w[i].want = n_tasks;
		w[i].run_tasks = 0;
		w[i].busy = false;
		pool[i]->options.verbosity = 0;
		pool[i]->options.time = search->options.time;
		pool[i]->options.time_per_move = search->options.time_per_move;
	}
	thread_run_workers(complete_worker_run, w, sizeof *w, n, false, false);
	for (i = 0; i < n; ++i) { // back to the state of the pool
		if (w[i].n_tasks != 1) search_set_task_number(pool[i], 1);
		pool[i]->options.keep_date = false;
		pool[i]->options.time = TIME_MAX;
		pool[i]->options.time_per_move = true;
	}
	for (i = 0; i < shared.n_slot; ++i) free((void*) shared.slot[i].report);
	free(shared.slot);
	condition_free(&shared);
	lock_free(&shared);
	printf("%d/%d games completed (all done).          \n", shared.n_completed, base->n_games);
	return true;
}

/**
 * @brief Complete the games of a base.
 *
 * @param base Game base.
 * @param search Search engine (its observer gets the results).
 * @param save_file File of the timed saves (NULL: none).
 */
static void base_complete_saving(Base *base, Search *search, const char *save_file)
{
	unsigned long long t = real_clock();
	int i, n;
	int completed;

	if (!base_complete_parallel(base, search, save_file)) { // one game after the other
		for (i = n = 0; i < base->n_games; ++i) {
			completed = 0;
			if (game_complete(base->game + i, search) > 0) completed = 1;
			n += completed;
			if (completed || (i % 1000) == 0) {
				printf("%d/%d games completed (%.1f %% done).\r", n, i + 1, 100.0 * (i + 1) / base->n_games); fflush(stdout);
			}
			complete_save_timed(base, save_file, &t);
		}
		printf("%d/%d games completed (all done).          \n", n, base->n_games);
	}
	book_store_release(); // (the searches of the games: up to 57 MB each)
}

/**
 * @brief Base completion.
 *
 * @param base Game base.
 * @param search Search engine.
 */
void base_complete(Base *base, Search *search)
{
	base_complete_saving(base, search, NULL);
}

/**
 * @brief Complete the games of a file (base complete).
 *
 * The file is replaced once all its games are completed. While they are, they are saved by book-save-interval
 * to <file>.completing.<ext> (removed at the end): base complete on a copy of that file goes on from there.
 *
 * @param file Game filename.
 * @param search Search engine.
 * @return true if the file was replaced.
 */
bool base_complete_file(const char *file, Search *search)
{
	const size_t l = strlen(file);
	char save_file[FILENAME_MAX + 1];
	bool ok = false, can_save;
	Base base;
	FILE *f;

	base_init(&base);
	if (base_load(&base, file)) { // a file which was not loaded (or not completely) is kept as it is
		can_save = (l >= 4 && l + 32 <= FILENAME_MAX); // (room for the names of the files being written)
		if (can_save) sprintf(save_file, "%s.completing%s", file, file + l - 4);
		base_complete_saving(&base, search, can_save ? save_file : NULL);
		ok = base_replace(&base, file); // (the file is replaced once the games are saved to another file)
		if (ok && can_save && (f = fopen(save_file, "rb")) != NULL) {
			fclose(f);
			remove(save_file);
		}
	}
	base_free(&base);
	return ok;
}

/**
 * @brief Base Compare.
 *
 * Display the number of positions two base files have in common. 
 *
 * @param file_1 Game base file.
 * @param file_2 Game base file.
 */
void base_compare(const char *file_1, const char *file_2)
{
	Base base_1[1], base_2[2];
	PositionHash hash;
	Board board;
	int i, j;
	long long n_1, n_2, n_2_only;

	base_init(base_1);
	base_init(base_2);
	n_1 = 0;
	n_2 = 0;
	n_2_only = 0;

	base_load(base_1, file_1);
	positionhash_init(&hash, options.hash_table_size);
	for (i = 0; i < base_1->n_games; ++i) {
		Game *game = base_1->game + i;
		board = game->initial_board;
		for (j = 0; j < 60 && game->move[j] != NOMOVE; ++j) {
			if (!game_update_board(&board, game->move[j])) break; // BAD MOVE -> end of game
			if (positionhash_append(&hash, &board)) {
				++n_1;
			}
		}
	}
	base_free(base_1);

	base_load(base_2, file_2);
	for (i = 0; i < base_2->n_games; ++i) {
		Game *game = base_2->game + i;
		board = game->initial_board;
		for (j = 0; j < 60 && game->move[j] != NOMOVE; ++j) {
			if (!game_update_board(&board, game->move[j])) break; // BAD MOVE -> end of game
			if (positionhash_append(&hash, &board)) {
				++n_2_only;
			}
		}
	}

	positionhash_delete(&hash);
	positionhash_init(&hash, options.hash_table_size);
	for (i = 0; i < base_2->n_games; ++i) {
		Game *game = base_2->game + i;
		board = game->initial_board;
		for (j = 0; j < 60 && game->move[j] != NOMOVE; ++j) {
			if (!game_update_board(&board, game->move[j])) break; // BAD MOVE -> end of game
			if (positionhash_append(&hash, &board)) {
				++n_2;
			}
		}
	}
	base_free(base_2);

	positionhash_delete(&hash);

	printf("%s : %lld positions - %lld original positions\n", file_1, n_1, n_1 - (n_2- n_2_only));
	printf("%s : %lld positions - %lld original positions\n", file_2, n_2, n_2_only);
	printf("%lld common positions\n", n_2-n_2_only);
}
