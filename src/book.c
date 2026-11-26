/**
 * @file book.c
 *
 * File for opening book management.
 *
 * Edax book is a set of positions. Each position is unique concerning all possible symetries.
 * A position is made of an othello board, a set of moves leading to other positions in the book,
 * called here "link", and the best remaining move, as evaluated by a search at fixed depth, called
 * here leaf. It also contains win/draw/loss statistics, which is actually useless, and a score
 * with two bounds from retropropagated error.
 * Several algorithms are present to add positions in the book, in the most usefull way.
 *
 * @date 1998 - 2020
 * @author Richard Delorme
 * @version 4.5
 */

#include "book.h"
#include "sort.h"
#include "base.h"
#include "search.h"
#include "const.h"
#include "bit.h"
#include "options.h"
#include "util.h"

#include <assert.h>
#include <time.h>
#include <stdarg.h>
#include <limits.h>
#include <stdint.h>
#include <errno.h>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#include <sys/stat.h>
#include <sched.h>
#ifdef __linux__
#include <fcntl.h>
#include <sys/syscall.h>
#endif
#endif

#define BOOK_DEBUG 0
static const int BOOK_INFO_RESOLUTION = 100000;

#define clear_line() bprint("                                                                                \r")

bool book_verbose = false;

/**
 * @brief print a message about the book on stdout, if the book is verbose.
 * @param format Format string.
 * @param ... variable arguments.
 */
#if defined(__GNUC__)
void book_print(const char *format, ...) __attribute__((format(printf, 1, 2)));
#endif
#define bprint book_print /* (the name used in this file) */

void book_print(const char *format, ...)
{
	if (book_verbose) {
		va_list args;

		va_start(args, format);
		vprintf(format, args);
		va_end(args);
		fflush (stdout);
	}
}

/**
 * @brief Run the threads of a book function (negamax, fix, link, sort, deviate walks).
 *
 * The calling thread runs the first worker, the others get their own threads (see thread_run_workers).
 * With the cpu option, each of them gets its own cpu, as the threads of a search do: where the option
 * binds the threads (linux), a new thread would else stay on the cpu of the main thread, and all the
 * threads of the book functions would share cpu 0.
 *
 * @param function Function to run on each worker.
 * @param worker Array of workers.
 * @param size Size of a worker.
 * @param n Number of workers.
 */
static void book_run_workers(void* (*function)(void*), void *worker, const size_t size, const int n)
{
	thread_run_workers(function, worker, size, n, true, options.cpu_affinity);
}

/** @return the number of threads of a walk of the book (negamax, prune, deviate): n-tasks, from 1 to MAX_THREADS. */
static int book_walk_threads(void)
{
	return MAX(1, MIN(options.n_task, MAX_THREADS));
}

/**
 * @brief Tell if the searches of the book functions must be done one at a time.
 *
 * Where the cpu option binds the threads (linux), the threads of every search are bound to the cpus
 * 0 to n-1: searches done at the same time (book-store-tasks, book-expand-tasks) would all run on
 * the same cpus. They are done one after the other, with the main search, as Edax always did.
 */
static bool book_one_search_at_a_time(void)
{
	return options.cpu_affinity && thread_cpu_bound();
}

/** struct Link
 * @brief a move (with its score) linking to another Position.
 */
typedef struct Link {
	signed char score; /**< move score */
	unsigned char move; /**< move coordinate */
} Link;

const Link BAD_LINK = {-SCORE_INF, NOMOVE};

/**
 * @brief check if a link is unvalid.
 *
 * @param link checked link.
 * @return true if link is unvalid, false otherwise.
 */
static inline bool link_is_bad(const Link *link)
{
	return link->score == -SCORE_INF;
}

/** Number of links stored inside the Position itself (99% of positions of a large book). */
#define POSITION_INLINE_LINKS 4

/** Type of the bit-fields of a Position (one byte: GCC and Clang take an unsigned char as an extension of C99). */
#if defined(__GNUC__)
	#define POSITION_BITS __extension__ unsigned char
#else
	#define POSITION_BITS unsigned char
#endif

/**
 * struct Position
 * @brief A position stored in the book.
 */
typedef struct Position {
	Board board;               /**< (unique) board */
	union {
		Link *array;           /**< heap array, used when n_link > POSITION_INLINE_LINKS */
		Link in[POSITION_INLINE_LINKS]; /**< in-place storage for up to POSITION_INLINE_LINKS links */
	} links;                   /**< linking moves (use position_links()) */
	unsigned int n_wins;       /**< game win count */
	unsigned int n_draws;      /**< game draw count */
	unsigned int n_losses;     /**< game loss count */
	unsigned int n_lines;      /**< unterminated line count */
	struct {
		signed char value, lower, upper;
	} score;                   /**< Position value & bounds (saved as 16-bit values; see score_char()) */
	Link leaf;                 /**< best remaining move */
	unsigned char n_link;      /**< linking moves number */
	POSITION_BITS level : 6;   /**< search level (0 to 60) */
	POSITION_BITS marks : 2;   /**< progress marks of book correct and book leaf recalculate (see below) */
	unsigned char state;       /**< book epoch and done/todo flags (see below) */
} Position;                    /* 48 bytes on 64-bit targets (56 in v4.5.5-nikque.4, 64 before) */

typedef char position_size_check[(sizeof (void*) != 8 || sizeof (Position) == 48) ? 1 : -1];

/*
 * Progress marks. book correct marks each solved position that it has searched again, and book leaf
 * recalculate each position whose leaf it has searched again. A marked position is not searched again by
 * the same command: run on a book saved before the command ended (a timed save, or the save at the exit),
 * the command goes on with the positions left. When the command ends, it removes all its marks, and a
 * finished book has none. The marks are saved in the two upper bits of the byte of the level (0 to 60):
 * the versions up to v4.5.5-nikque.28 read a book with marks as a damaged book.
 */
#define POSITION_MARK_CORRECTED 1    /**< book correct: this solved position was searched again */
#define POSITION_MARK_RECALCULATED 2 /**< book leaf recalculate: the leaf of this position was searched again */
#define POSITION_LEVEL_BITS 0x3f     /**< bits of the level in the saved byte (the marks are above) */

/*
 * The state of a position holds the book epoch at which its done/todo flags were set, so that
 * book_clean() only has to change the epoch instead of rewriting every position (a full scan is
 * still done once every POSITION_EPOCH_MAX epochs). The flags of an older epoch are all clear.
 * POSITION_BUSY marks a position being computed by the parallel negamax (always run just after
 * book_clean(), when no position has a todo flag of the current epoch).
 */
#define POSITION_EPOCH_MASK 0x1f
#define POSITION_EPOCH_MAX 31
#define POSITION_BUSY 0x20
#define POSITION_TODO 0x40
#define POSITION_DONE 0x80

/** @return state with a flag set at epoch (the flags of an older epoch are cleared). */
static inline unsigned char position_state_set(const unsigned char state, const unsigned char epoch, const unsigned char flag)
{
	return (unsigned char) (((state & POSITION_EPOCH_MASK) == epoch ? state : epoch) | flag);
}

#ifdef BOOK_TEST_COUNTS /* test builds: what the commands did on the whole book, and how they expanded */
static long long book_test_scans = 0, book_test_negamaxes = 0, book_test_saves = 0;
static long long book_test_serial = 0;    /* rounds of 2 positions or more expanded one after the other */
static long long book_test_parallel = 0;  /* rounds expanded by several searches at the same time */
static long long book_test_save_ms = -1;  /* time between two timed saves, in milliseconds (-1: book-save-interval) */
static long long book_test_wide_walks = 0, book_test_wide_threaded = 0; /* levels of 4096 boards or more walked, and by several threads */
static void (*book_test_saved)(const char *file) = NULL; /* called after the timed saves of the commands that learn by rounds */
#define book_test_add(counter) (++(counter))
#else
#define book_test_add(counter) ((void) 0)
#endif

#define position_state_is(state, book, flag) (((state) & (POSITION_EPOCH_MASK | (flag))) == ((book)->epoch | (flag)))
#define position_is_done(p, book) position_state_is((p)->state, book, POSITION_DONE)
#define position_set_done(p, book) ((p)->state = position_state_set((p)->state, (book)->epoch, POSITION_DONE))
#define position_is_todo(p, book) position_state_is((p)->state, book, POSITION_TODO)
#define position_set_todo(p, book) ((p)->state = position_state_set((p)->state, (book)->epoch, POSITION_TODO))
#define position_clear_todo(p) ((p)->state &= (unsigned char) ~POSITION_TODO)

/**
 * @brief Score stored in a position (one byte): scores are within [-SCORE_INF, SCORE_INF], except
 * bounds widened by book enhance errors above 63, which are saturated.
 */
static inline signed char score_char(const int score)
{
	return (signed char) (score < -SCORE_INF ? -SCORE_INF : (score > SCORE_INF ? SCORE_INF : score));
}

/** @brief linking moves of a position, wherever they are stored. */
#define position_links(p) ((p)->n_link > POSITION_INLINE_LINKS ? (p)->links.array : (p)->links.in)

static Position* book_probe(const Book*, const Board*);
static int book_add(Book*, const Position*);
static void book_mark_todo(Book*, Position*);
static void position_print(const Position*, const Board*, FILE*);
static bool book_game_boards(Book*, const Game*, const bool);

#define foreach_link(l, p)  \
	for ((l) = position_links(p); (l) < position_links(p) + (p)->n_link; ++(l))

/**
 * @brief Replace the links of a position.
 *
 * Fewer links than the position has are written in place (no memory is allocated: removing links never fails).
 *
 * @param position Position (its previous links are released).
 * @param links New links (may not alias the position's storage).
 * @param n Number of links.
 * @return false if memory is exhausted (the position is left as it was).
 */
static bool position_set_links(Position *position, const Link *links, const int n)
{
	Link *array = NULL;

	if (n > POSITION_INLINE_LINKS) {
		if (position->n_link >= n) { // the array of the position is large enough
			memcpy(position->links.array, links, sizeof (Link) * n);
			position->n_link = (unsigned char) n;
			return true;
		}
		array = (Link*) malloc(sizeof (Link) * n);
		if (array == NULL) return false;
		memcpy(array, links, sizeof (Link) * n);
	}
	if (position->n_link > POSITION_INLINE_LINKS) free(position->links.array);
	position->links.array = array;
	if (array == NULL && n > 0) memcpy(position->links.in, links, sizeof (Link) * n);
	position->n_link = (unsigned char) n;
	return true;
}

/**
 * @brief return the number of plies from where the search is solving.
 *
 * @param depth search depth.
 * @return book depth.
 */
static int get_book_depth(const int depth)
{
	if (depth <= 10) return 60 - 2 * depth;
	else if (depth <= 18) return 39;
	else if (depth <= 24) return 36;
	else if (depth < 30) return 33;
	else if (depth < 36) return 30;
	else if (depth < 42) return 66 - depth;
	else return 24;
}


/**
 * @brief Check if position is ok or need fixing.
 *
 * Note: All positions should always be OK! A wrong position means a BUG!
 *
 * @param position Position.
 * @param verbose Explain what is wrong (false: only check; used by the threads of book_fix).
 * @return true if ok, false if it needs fixing.
 */
static bool position_check(const Position *position, const bool verbose)
{
	Board board;
	Move move;
	const Link *l;
	int i, j;
	char s[4];

	// board is legal ?
	if (position->board.player & position->board.opponent) {
		if (verbose) warn("Board is illegal: Two discs on the same square?\n");
		if (verbose) board_print(&position->board, BLACK, stderr);
		return false;
	}
	if (((position->board.player | position->board.opponent) & 0x0000001818000000ULL) != 0x0000001818000000ULL) {
		if (verbose) warn("Board is illegal: Empty center?\n");
		if (verbose) board_print(&position->board, BLACK, stderr);
		return false;
	}

	// a level above 60 does not exist (damaged file): it would index LEVEL[][] out of bounds
	if (position->level > 60) {
		if (verbose) warn("level %d is wrong\n", position->level);
		if (verbose) position_print(position, &position->board, stdout);
		return false;
	}

	// is board unique
	board_unique(&position->board, &board);
	if (!board_equal(&position->board, &board)) {
		if (verbose) warn("board is not unique\n");
		if (verbose) position_print(position, &position->board, stdout);
		return false;
	}

	// are moves legal ?
	foreach_link(l, position) {
		if (l->move == PASS) {
			if (position->n_link > 1
			 || can_move(board.player, board.opponent)
			 || !can_move(board.opponent, board.player)) {
				if (verbose) warn("passing move is wrong\n");
				if (verbose) position_print(position, &position->board, stdout);
				return false;
			}
		} else {
			if (/*l->move < A1 ||*/ l->move > H8
			 || board_is_occupied(&board, l->move)
			 || board_get_move_flip(&board, l->move, &move) == 0) {
				if (verbose) warn("link %s is wrong\n", move_to_string(l->move, WHITE, s));
				if (verbose) position_print(position, &position->board, stdout);
				return false;
			}
		}
	}

	l = &position->leaf;
	if (l->move == PASS) {
		if (position->n_link > 0
		 || can_move(board.player, board.opponent)
		 || !can_move(board.opponent, board.player)) {
			if (verbose) warn("passing move is wrong\n");
			if (verbose) position_print(position, &position->board, stdout);
			return false;
		}
	} else if (l->move == NOMOVE) {
		if (get_mobility(position->board.player, position->board.opponent) != position->n_link && !(position->n_link == 1 && position_links(position)[0].move == PASS)) {
			if (verbose) warn("nomove is wrong\n");
			if (verbose) position_print(position, &position->board, stdout);
			return false;
		}
	} else if (/*l->move < A1 ||*/ l->move > H8
		 || board_is_occupied(&board, l->move)
		 || board_get_move_flip(&board, l->move, &move) == 0) {
			if (verbose) warn("leaf %s is wrong\n", move_to_string(l->move, WHITE, s));
			if (verbose) position_print(position, &position->board, stdout);
			return false;
	}

	// doublons ?
	l = position_links(position);
	for (i = 0; i < position->n_link; ++i) {
		for (j = i + 1; j < position->n_link; ++j) {
			if (l[j].move == l[i].move) {
				if (verbose) warn("doublon found in links\n");
				if (verbose) position_print(position, &position->board, stdout);
				return false;
			}
		}
		if (position->leaf.move == l[i].move) {
			if (verbose) warn("doublon found in links/leaf\n");
			if (verbose) position_print(position, &position->board, stdout);
			return false;
		}
	}
	return true;
}

/** @brief Check if position is ok or need fixing, and explain what is wrong. */
static bool position_is_ok(const Position *position)
{
	return position_check(position, true);
}

/**
 * @brief Initialize a position.
 *
 * @param position Position.
 */
static void position_init(Position *position)
{
	position->board.player = position->board.opponent = 0;

	position->leaf = BAD_LINK;
	position->links.array = NULL;

	position->n_wins = position->n_draws = position->n_losses = position->n_lines = 0;
	position->score.value = position->score.lower = -SCORE_INF;
	position->score.upper = +SCORE_INF;

	position->n_link = 0;
	position->level = 0;
	position->marks = 0;
	position->state = 1 | POSITION_DONE; // done at epoch 1 (as before)
}

/**
 * @brief Merge a position with another one.
 *
 * A position is merged if its level is > to the destination position; or == and
 * its leaf move is not contains into the destination link moves.
 * 
 * Note: link moves are not copied. This can be done later with position_link().
 *
 * @param dest Destination position.
 * @param src Source position.
 */
static void position_merge(Position *dest, const Position *src)
{
	Link *l;

	position_init(dest);		//??? dest->n_link = 0,
	dest->board = src->board;
	if (dest->level == src->level) { 
		foreach_link(l, dest) {	// so this does nothing
			if (l->move == src->leaf.move) return;
		}
		dest->leaf = src->leaf;
	} else if (dest->level > src->level) {
		return;
	} else {
		dest->leaf = src->leaf;
		dest->level = src->level;
	}
	// start from the leaf score (as position_search() does), not from -SCORE_INF:
	// positions that negamax cannot reach from the root would keep +/-127 values
	if (dest->leaf.move != NOMOVE && dest->leaf.score > dest->score.value) dest->score.value = dest->leaf.score;
}

/**
 * @brief Free resources used by a position.
 *
 * @param position Position.
 */
static void position_free(Position *position)
{
	if (position->n_link > POSITION_INLINE_LINKS) free(position->links.array);
}

/**
 * @brief Buffered sequential access to a binary book file.
 *
 * The binary format stores each position as 40 bytes of fixed fields,
 * n_link links of 2 bytes and a 2-byte leaf. Reading or writing it field by
 * field costs ~15 stdio calls per position (about 10 billion calls for a
 * 650 million position book), so records are packed into a large buffer.
 */
typedef struct BookStream {
	FILE *f;
	unsigned char *buffer;
	size_t size, n, pos;
} BookStream;

#define BOOK_STREAM_SIZE (16u << 20)
#define POSITION_FIXED_SIZE 40

static bool book_stream_open(BookStream *s, FILE *f)
{
	s->f = f;
	s->size = BOOK_STREAM_SIZE;
	s->n = s->pos = 0;
	s->buffer = (unsigned char*) malloc(s->size);
	return s->buffer != NULL;
}

static void book_stream_close(BookStream *s)
{
	free(s->buffer);
	s->buffer = NULL;
}

static bool book_stream_read(BookStream *s, void *data, size_t len)
{
	unsigned char *d = (unsigned char*) data;
	while (len) {
		size_t k;
		if (s->pos == s->n) {
			s->n = fread(s->buffer, 1, s->size, s->f);
			s->pos = 0;
			if (s->n == 0) return false;
		}
		k = s->n - s->pos; if (k > len) k = len;
		memcpy(d, s->buffer + s->pos, k);
		s->pos += k; d += k; len -= k;
	}
	return true;
}

/** @return true if no unread byte remains in the stream. */
static bool book_stream_at_end(BookStream *s)
{
	return s->pos == s->n && fgetc(s->f) == EOF && !ferror(s->f);
}

static bool book_stream_write(BookStream *s, const void *data, size_t len)
{
	const unsigned char *d = (const unsigned char*) data;
	while (len) {
		size_t k;
		if (s->n == s->size) {
			if (fwrite(s->buffer, 1, s->n, s->f) != s->n) return false;
			s->n = 0;
		}
		k = s->size - s->n; if (k > len) k = len;
		memcpy(s->buffer + s->n, d, k);
		s->n += k; d += k; len -= k;
	}
	return true;
}

static bool book_stream_flush(BookStream *s)
{
	bool ok = (s->n == 0 || fwrite(s->buffer, 1, s->n, s->f) == s->n);
	s->n = 0;
	return ok;
}

/** @return true if the moves and the level read from a book file can be those of a position. */
static bool position_read_is_sound(const int n_link, const Link *links, const Link *leaf, const int level)
{
	int i;

	if (level > 60 || n_link + (leaf->move != NOMOVE) > MAX_MOVE) return false;
	for (i = 0; i < n_link; ++i) if (links[i].move > PASS) return false;
	return leaf->move <= NOMOVE;
}

/** @return true if a board read from a book can be the one of a position: no square with two discs, four discs at least. */
static bool position_board_is_sound(const Board *board)
{
	return (board->player & board->opponent) == 0 && bit_count(board->player | board->opponent) >= 4;
}

/**
 * @brief Read a position.
 *
 * @param position Position to read in.
 * @param s Input stream.
 */
static bool position_read(Position *position, BookStream *s)
{
	unsigned char record[POSITION_FIXED_SIZE + sizeof (Link) * 256]; // the largest record: 255 links and the leaf
	const unsigned char *h;
	const Link *links;
	int n_link;

	if (s->n - s->pos >= sizeof record) { // the whole record is in the buffer: it is read there
		h = s->buffer + s->pos;
		n_link = h[38];
		s->pos += POSITION_FIXED_SIZE + sizeof (Link) * (n_link + 1);
	} else {
		if (!book_stream_read(s, record, POSITION_FIXED_SIZE)) return false;
		n_link = record[38];
		if (!book_stream_read(s, record + POSITION_FIXED_SIZE, sizeof (Link) * (n_link + 1))) return false;
		h = record;
	}
	memcpy(&position->board.player, h, 8);
	memcpy(&position->board.opponent, h + 8, 8);
	memcpy(&position->n_wins, h + 16, 4);
	memcpy(&position->n_draws, h + 20, 4);
	memcpy(&position->n_losses, h + 24, 4);
	memcpy(&position->n_lines, h + 28, 4);
	{
		short value, lower, upper;
		memcpy(&value, h + 32, 2);
		memcpy(&lower, h + 34, 2);
		memcpy(&upper, h + 36, 2);
		position->score.value = score_char(value);
		position->score.lower = score_char(lower);
		position->score.upper = score_char(upper);
	}
	position->level = h[39] & POSITION_LEVEL_BITS;
	position->marks = h[39] >> 6; // (progress marks: see POSITION_MARK_CORRECTED)

	position->state = 0;

	links = (const Link*) (h + POSITION_FIXED_SIZE); // (two bytes, of one each)
	position->leaf = links[n_link];
	// a damaged file (or one of another tool): more moves than a position has, a level above 60 (an index of
	// LEVEL[][]), a move that is no square: the book is not loaded (the lists of moves and the tables would be
	// read or written out of bounds)
	if (!position_board_is_sound(&position->board) || !position_read_is_sound(n_link, links, &position->leaf, position->level)) return false;

	position->n_link = 0;
	if (!position_set_links(position, links, n_link)) {
		error("cannot allocate opening book position's moves\n");
		return false;
	}

	return true;
}

/**
 * @brief Read a position from a line of a text book: "<board>,<level>[,<leaf move>,<leaf score>]".
 *
 * Empty lines are skipped, and so is a line "% depth <n>" (the depth of the book that was exported,
 * written by book_export() since v4.5.5-nikque.13), whose value is given back.
 *
 * @param position Position to read in.
 * @param f Input stream.
 * @param verbose Explain what is wrong with a line that holds no position.
 * @param depth Depth told by the file (unchanged if the lines read do not tell it).
 * @return 1 if a position was read, 0 if the line holds no position, -1 at the end of the file.
 */
static int position_import(Position *position, FILE *f, const bool verbose, int *depth)
{
	char *line, *s, *old;
	const char *wrong = NULL;
	int value;
	Move move;

	for (;;) {
		if ((line = string_read_line(f)) == NULL) return -1;
		s = parse_skip_spaces(line);
		if (strncmp(s, "% depth ", 8) == 0) {
			value = 0; parse_int(s + 8, &value);
			if (1 <= value && value <= 60) *depth = value;
			else if (verbose) warn("wrong depth: %s\n", line);
		} else if (*s != '\0') break;
		free(line);
	}

	position_init(position);
	s = parse_board(line, &position->board, &value);
	if (s == line || !position_board_is_sound(&position->board)) {
		wrong = "wrong board";
	} else if (*(s = parse_find(s, ',')) != ',') {
		wrong = "missing ',' after board setting";
	} else {
		value = -1; s = parse_int(old = s + 1, &value); BOUND(value, -1, 60, "level");
		if (s == old || value == -1) wrong = "wrong level";
	}

	if (wrong == NULL) {
		position->level = value;
		// the leaf is optional: a move, then its score
		s = parse_find(s, ',');
		if (*s == ',') {
			s = parse_move(old = s + 1, &position->board, &move);
			if (s != old && *(s = parse_find(s, ',')) == ',') {
				s = parse_int(old = s + 1, &value);
				if (s == old) {
				} else if (value < SCORE_MIN || value > SCORE_MAX) {
					wrong = "wrong leaf score"; // (up to v4.5.5-nikque.26 kept as a signed char: 300 was read as 44)
				} else {
					position->leaf.move = move.x;
					position->leaf.score = value;
				}
			}
		}
	}
	if (wrong != NULL && verbose) warn("%s: %s\n=> wrong position\n", wrong, line);

	free(line);
	return wrong == NULL;
}

/**
 * @brief Write a position.
 *
 * @param position position to write out.
 * @param f output stream.
 */
static bool position_write(const Position *position, BookStream *s)
{
	unsigned char h[POSITION_FIXED_SIZE + 2 * 257];
	const int n = position->n_link;

	memcpy(h, &position->board.player, 8);
	memcpy(h + 8, &position->board.opponent, 8);
	memcpy(h + 16, &position->n_wins, 4);
	memcpy(h + 20, &position->n_draws, 4);
	memcpy(h + 24, &position->n_losses, 4);
	memcpy(h + 28, &position->n_lines, 4);
	{
		const short value = position->score.value, lower = position->score.lower, upper = position->score.upper;
		memcpy(h + 32, &value, 2);
		memcpy(h + 34, &lower, 2);
		memcpy(h + 36, &upper, 2);
	}
	h[38] = position->n_link;
	h[39] = (unsigned char) (position->level | position->marks << 6);
	if (n) memcpy(h + POSITION_FIXED_SIZE, position_links(position), sizeof (Link) * n);
	memcpy(h + POSITION_FIXED_SIZE + sizeof (Link) * n, &position->leaf, sizeof (Link));

	return book_stream_write(s, h, POSITION_FIXED_SIZE + sizeof (Link) * (n + 1));
}

/**
 * @brief write a position.
 *
 * @param p position to write out.
 * @param f output stream.
 */
static bool position_export(const Position *p, FILE* f)
{
	char b[80], m[4];

	board_to_string(&p->board, BLACK, b);
	move_to_string(p->leaf.move, BLACK, m);
	return (fprintf(f, "%s,%d,%s,%d\n", b, p->level, m, p->leaf.score) > 0);
}

/**
 * @brief Make position unique, regarding symetries.
 *
 * @param position position.
 */
static void position_unique(Position *position)
{
	Board board;
	int i, s;

	board = position->board;
	if ((s = board_unique(&board, &position->board)) != 0) {
		for (i = 0; i < position->n_link; ++i) {
			position_links(position)[i].move = symetry(position_links(position)[i].move, s);
		}
		position->leaf.move = symetry(position->leaf.move, s);
	}
}

/**
 * @brief Get moves from a position.
 *
 * @param position position to get moves from.
 * @param board board.
 * @param movelist movelist.
 */
static int position_get_moves(const Position *position, const Board *board, MoveList *movelist)
{
	Move *previous = movelist->move;
	Move *move = movelist->move + 1;
	Board sym;
	int i, x, s;

	for (s = 0; s < 8; ++s) {
		board_symetry(&position->board, s, &sym);

		if (board_equal(&sym, board)) {
			// a damaged book file may hold more moves than a position has, or moves that are no square:
			// they are left out (the list holds MAX_MOVE moves; a wrong square would index the flip tables)
			const Move *const end = movelist->move + MAX_MOVE + 1;
			for (i = 0; i < position->n_link && move < end; ++i) {
				x = symetry(position_links(position)[i].move, s);
				if (x < A1 || x > PASS) continue;
				board_get_move_flip(board, x, move);
				move->score = position_links(position)[i].score;
				previous = previous->next = move;
				++move;
			}
			x = symetry(position->leaf.move, s);
			if (A1 <= x && x <= PASS && move < end) {
				board_get_move_flip(board, x, move);
				move->score = position->leaf.score;
				previous = previous->next = move;
				++move;
			}
			previous->next = NULL;
			movelist->n_moves = move - movelist->move - 1;
			movelist_sort(movelist);
			return s;
		}
	}

	fatal_error("unreachable code\n");
	return -1;
}

/**
 * @brief print a position in a readable format.
 *
 * @param position position to print out.
 * @param board Symetrical board to use.
 * @param f output stream.
 */
static void position_show(const Position *position, const Board *board, FILE *f)
{
	MoveList movelist;
	Move *move;
	const int n_empties = board_count_empties(board);
	const int color = n_empties & 1;
	int sym;
	char s[4];

	board_print(board, color, f);

	fprintf(f, "\nLevel: %d\n", position->level);
	fprintf(f, "Best score: %+02d [%+02d, %+02d]\n", position->score.value, position->score.lower, position->score.upper);
	fprintf(f, "Moves:");
	sym = position_get_moves(position, board, &movelist);
	foreach_move(move, movelist) {
		move_to_string(move->x, color, s);
		if (symetry(position->leaf.move, sym) == move->x) {
			fprintf(f, " <%s:%+02d>", s, move->score);
		} else {
			fprintf(f, " [%s:%+02d]", s, move->score);
		}
	}
}

/**
 * @brief print a position in a compact but readable format.
 *
 * @param position position to print out.
 * @param board Symetrical board to use.
 * @param f output stream.
 */
static void position_print(const Position *position, const Board *board, FILE *f)
{
	MoveList movelist;
	Move *move;
	int color = board_count_empties(board) & 1, sym;
	char b[80], m[4];

	board_to_string(board, color, b);
	fprintf(f, "{board:%s; ", b);
	fprintf(f, "level:%d; ", position->level);
	fprintf(f, "best: %+02d [%+02d, %+02d];", position->score.value, position->score.lower, position->score.upper);
	fprintf(f, "moves:");
	sym = position_get_moves(position, board, &movelist);
	foreach_move(move, movelist) {
		move_to_string(move->x, color, m);
		if (symetry(position->leaf.move, sym) == move->x) {
			fprintf(f, " <%s:%+02d>", m, move->score);
		} else {
			fprintf(f, " [%s:%+02d]", m, move->score);
		}
	}
	fprintf(f, "}\n");
}

/**
 * @brief Chose a move at random from the position.
 *
 * @param position Position to chose a move from.
 * @param board Correctly rotated/mirrored board.
 * @param move Chosen move.
 * @param r Random data.
 * @param randomness Randomness intensity (randomness = 0 means no randomness).
 */
static void position_get_random_move(const Position *position, const Board *board, Move *move, Random *r, const int randomness)
{
	const int margin = MIN(randomness, 2 * SCORE_MAX); // the scores differ by 2 * SCORE_MAX at most: no overflow with a huge randomness
	MoveList movelist;
	Move *m;
	int i, n;

	position_get_moves(position, board, &movelist);

	// only the legal moves are chosen: a damaged book may hold a move that cannot be played on this board
	n = 0;
	foreach_best_move(m, movelist) {
		if (position->score.value > m->score + margin) break;
		if (board_check_move(board, m)) ++n;
	}

	if (n == 0) { // no legal move within the margin
		move->x = NOMOVE;
		move->flipped = 0;
		return;
	} else {
		i = (random_get(r) % n); // is good enough here
	}

	foreach_best_move(m, movelist) {
		if (board_check_move(board, m) && i-- == 0) break;
	}

	*move = *m;
}

/**
 * @brief Add a link to this position.
 *
 * The result tells apart a link that was already present (0) from a link that could not be added
 * (-1: no memory). Up to v4.5.5-nikque.15 both were false: a leaf that could not become a link was
 * then replaced by the result of a new search, and the failure was not told.
 *
 * @param position Position to chose a move from.
 * @param link Link to add.
 * @return LINK_ADDED if the link has been added, LINK_PRESENT if it was already present,
 *         LINK_FAILED if it could not be added (no memory).
 */
enum { LINK_FAILED = -1, LINK_PRESENT = 0, LINK_ADDED = 1 };
static int position_add_link(Position *position, const Link *link)
{
	Link *l;
	int last = position->n_link;

	foreach_link (l, position) {
		if (l->move == link->move) {
			l->score = link->score; // update the link ?
			return LINK_PRESENT;
		}
	}

	if (last < POSITION_INLINE_LINKS) {
		position->links.in[last] = *link;
	} else {
		if (last == POSITION_INLINE_LINKS) { // move the in-place links to the heap
			l = (Link*) malloc(sizeof (Link) * (last + 1));
			if (l) memcpy(l, position->links.in, sizeof (Link) * last);
		} else {
			l = (Link*) realloc(position->links.array, sizeof (Link) * (last + 1));
		}
		if (l == NULL) {
			error("cannot allocate opening book position's moves\n");
			return LINK_FAILED;
		}
		l[last] = *link;
		position->links.array = l;
	}
	++position->n_link;

	if (link->score > position->score.value) position->score.value = link->score;

	if (link->move == position->leaf.move) position->leaf = BAD_LINK;

	return LINK_ADDED;
}

/**
 * @brief Add a link to a position of the book (position_add_link) and count it.
 *
 * A link that cannot be added (no memory) sets book->failed, which stops the learning loops.
 *
 * @param book Opening book.
 * @param position Position.
 * @param link Link to add.
 */
static inline void book_add_link(Book *book, Position *position, const Link *link)
{
	const int added = position_add_link(position, link);

	if (added == LINK_ADDED) ++book->stats.n_links;
	else if (added == LINK_FAILED) book->failed = true;
}

/**
 * @brief Sort the link moves.
 *
 * @param position Position to sort.
 */
static void position_sort(Position *position)
{
	Link *i, *j, *best;

	if (position->n_link > 1) {
		Link *links = position_links(position);
		for (i = links; i < links + position->n_link - 1; ++i) {
			best = i;
			for (j = i + 1; j < links + position->n_link; ++j) {
				if (j->score > best->score) best = j;
			}
			if (best > i) {
				Link tmp = *best;
				*best = *i;
				*i = tmp;
			}
		}
	}
}

#ifdef BOOK_TEST_NODES
/* test builds: count the searches of the book functions and their nodes (printed after each negamax) */
static long long book_test_nodes = 0, book_test_searches = 0, book_test_play_nodes = 0;
void book_test_count_play(const unsigned long long n)
{
#if defined(_MSC_VER)
	_InterlockedExchangeAdd64(&book_test_play_nodes, (long long) n);
#else
	__atomic_add_fetch(&book_test_play_nodes, (long long) n, __ATOMIC_RELAXED);
#endif
}
static void book_test_count(const unsigned long long n)
{
#if defined(_MSC_VER)
	_InterlockedExchangeAdd64(&book_test_nodes, (long long) n);
	_InterlockedExchangeAdd64(&book_test_searches, 1);
#else
	__atomic_add_fetch(&book_test_nodes, (long long) n, __ATOMIC_RELAXED);
	__atomic_add_fetch(&book_test_searches, 1, __ATOMIC_RELAXED);
#endif
}
#endif

/** what position_search_with() and position_search_planned() did (their result is a set of these) */
enum {
	LEAF_LINKED = 1,     /**< the leaf became a link */
	LEAF_SEARCHED = 2,   /**< a search was done */
	LEAF_NOT_LINKED = 4  /**< the leaf could not become a link (no memory): nothing was done */
};

static int position_search_with(Position *position, Search *search);
static int position_search_planned(Position *position, Book *book);
static Search* book_search(Book *book);
static struct BookPlan *book_plan = NULL; /**< searches done ahead of book_add_board() (book-store-tasks); NULL: none */

/**
 * @brief Tell if the leaf of a position has to be searched.
 *
 * It has when a move of the position is not a link. A position without any move (a pass, or the end
 * of the game) is searched once, to get its score. The plan (book_plan_board, book_link_plan) applies
 * the same rule as the functions that search.
 *
 * @param n_link Number of links of the position.
 * @param n_moves Number of moves of its board.
 * @param no_score The position has no score yet (-SCORE_INF).
 */
static inline bool leaf_search_needed(const int n_link, const int n_moves, const bool no_score)
{
	return n_link < n_moves || (n_link == 0 && n_moves == 0 && no_score);
}

/**
 * @brief Forget the leaf of a position that is going to be searched again.
 *
 * A position without any move and without link (the end of the game, or a pass that leads to a
 * position that the book does not hold) has the score of its leaf, and is only searched when it has
 * no score (leaf_search_needed): its score is forgotten too. (Up to v4.5.5-nikque.12, book correct
 * and book deepen only forgot the leaf: such a position was not searched again, lost its leaf for
 * good, was reported as an error, and the next negamax gave it no score at all.)
 *
 * @param position Position.
 */
static void position_forget_leaf(Position *position)
{
	position->leaf = BAD_LINK;
	if (position->n_link == 0 && get_mobility(position->board.player, position->board.opponent) == 0) {
		position->score.value = -SCORE_INF;
	}
}

/**
 * @brief Evaluate a position.
 *
 * If needed, find the best remaining move, after link moves are excluded.
 *
 * @param position Position to search.
 * @param book Opening book.
 */
static void position_search(Position *position, Book *book)
{
	const int r = book_plan ? position_search_planned(position, book) : position_search_with(position, book_search(book));

	if (r & LEAF_LINKED) ++book->stats.n_links;
	if (r & LEAF_NOT_LINKED) book->failed = true; // the leaf was kept, not searched again
	if (r & (LEAF_LINKED | LEAF_SEARCHED)) book->need_saving = true;
}

/**
 * @brief Evaluate a position with a given search (no book access).
 *
 * @param position Position to search.
 * @param search Search to use.
 * @return LEAF_LINKED if the leaf became a link, | LEAF_SEARCHED if a search was done; LEAF_NOT_LINKED
 *         if the leaf could not become a link (no memory: nothing was done).
 */
static int position_search_with(Position *position, Search *search)
{
	Link *l;
	const int n_moves = get_mobility(position->board.player, position->board.opponent);
	long long time;
	bool time_per_move;
	int r = 0;

#ifdef BOOK_TEST_ISOLATE
	search_cleanup(search); // test builds: every search starts with empty hash tables, as the planned searches do
#endif
	if (position->leaf.move != NOMOVE) {
		const int added = position_add_link(position, &position->leaf);
		if (added == LINK_FAILED) return LEAF_NOT_LINKED; // the leaf is kept: a new search would replace it
		if (added == LINK_ADDED) r = LEAF_LINKED;
	}

	if (leaf_search_needed(position->n_link, n_moves, position->score.value == -SCORE_INF)) {
		search_set_board(search, &position->board, BLACK);
		search_set_level(search, position->level, search->eval.n_empties);

		foreach_link (l, position) {
			movelist_exclude(&search->movelist, l->move);
		}

		if (search->options.verbosity >= 2) {
			board_print(&search->board, search->player, stdout);
			// (libedax and nboard have no header: up to v4.5.5-nikque.15 "book verbose 2" then gave NULL to puts())
			if (search->options.header) puts(search->options.header);
			if (search->options.separator) puts(search->options.separator);
		}

		time = search->options.time;
		time_per_move = search->options.time_per_move;
		search->options.time = TIME_MAX;
		search->options.time_per_move = true;

		search_run(search);
#ifdef BOOK_TEST_NODES
		book_test_count(search_count_nodes(search)); // test builds: nodes of the book searches
#endif

		search->options.time = time;
		search->options.time_per_move = time_per_move;

		position->leaf.score = search->result->score;
		position->leaf.move = search->result->move;
		if (position->leaf.score > position->score.value) {
			position->score.value = position->leaf.score;
		}
		r |= LEAF_SEARCHED;
	}
	return r;
}

/**
 * @brief Link a position.
 *
 * Find moves that lead to other positions in the book.
 *
 * @param position Position to link.
 * @param book Opening book.
 */
static void position_link(Position *position, Book *book)
{
	int x;
	unsigned long long moves = board_get_moves(&position->board);
	Board next;
	Link link;
	Position *child;

	if (moves) {
		foreach_bit(x, moves) {
			board_next(&position->board, x, &next);
			child = book_probe(book, &next);
			if (child) {
				link.score = -child->score.value;
				link.move = x;
				book_add_link(book, position, &link);
			}
		}
	} else if (can_move(position->board.opponent, position->board.player)) {// pass ?
		next.player = position->board.opponent;
		next.opponent = position->board.player;
		child = book_probe(book, &next);
		if (child) {
			link.score = -child->score.value;
			link.move = PASS;
			book_add_link(book, position, &link);
		}
	}
}

static bool position_remove_links(Position*, Book*);

/**
 * @brief Take back the links of a position to the positions that are not in the book, after a child
 * could not be added (no memory): no link of the book, in memory or saved, leads to a missing position.
 * The move of such a link becomes the leaf again if its score is better (see position_remove_links).
 *
 * @param book Opening book.
 * @param position Position that was linked to the child.
 */
static void book_unlink_missing_child(Book *book, Position *position)
{
	const int n_link = position->n_link;

	if (position_remove_links(position, book)) book->stats.n_links -= n_link - position->n_link;
}

/**
 * @brief Expand a position.
 *
 * Expand the best yet unlink move. This will add a new position to the book.
 * Two new moves will also be analyzed, one for the new position, the other for
 * the actual position as a new best unlink move.
 *
 * @param position Position to expand.
 * @param book Opening book.
 */
static void position_expand(Position *position, Book *book)
{
	Position child;
	int r;

	if (position->leaf.move != NOMOVE) {
		position_init(&child);

		board_next(&position->board, position->leaf.move, &child.board);

		child.level = position->level;
		position_link(&child, book);
		search_cleanup(book_search(book));
		position_search(&child, book);
		position->leaf.score = -child.score.value;
		position_search(position, book);
		position_unique(&child);
		r = book_add(book, &child);
		if (r <= 0) position_free(&child); // already in the book, or not added
		if (r < 0) book_unlink_missing_child(book, position); // (the position did not move: nothing was added)
	}
}

/**
 * @brief Negamax a position.
 *
 * Go through the book sub-tree following the current position & negamax the best scores back to this position.
 *
 * @param position Position to expand.
 * @param book Opening book.
 */
static int position_negamax(Position *position, Book *book)
{
	Link *l;
	Board target;
	Position *child;

	if (!position_is_done(position, book)) {
		GameStats stat = {0,0,0,0};
		const int n_empties = board_count_empties(&position->board);
		const int search_depth = LEVEL[position->level][n_empties].depth;
		const int bias = (search_depth & 1) - (n_empties & 1);

		position_set_done(position, book);

		const int old_value = position->score.value, old_lower = position->score.lower, old_upper = position->score.upper;
		const unsigned int old_counts[4] = {position->n_wins, position->n_draws, position->n_losses, position->n_lines};
		position->score.value = position->score.lower = position->score.upper = -SCORE_INF;

		if (position->leaf.score > -SCORE_INF) {
			position->score.value = position->leaf.score;
			// is solving
			if (search_depth == n_empties && LEVEL[position->level][n_empties].selectivity == NO_SELECTIVITY) {
				position->score.lower = position->score.upper = position->score.value;
				if (position->leaf.score > 0) ++stat.n_wins;
				else if (position->leaf.score < 0) ++stat.n_losses;
				else ++stat.n_draws;
			// is pre-solving
			} else if (search_depth == n_empties) {
				position->score.lower = score_char(position->score.value - book->options.endcut_error);
				position->score.upper = score_char(position->score.value + book->options.endcut_error);
			} else { // midgame
				position->score.lower = score_char(position->score.value - book->options.midgame_error - bias);
				position->score.upper = score_char(position->score.value + book->options.midgame_error - bias);
			}
			++stat.n_lines;
		}

		foreach_link(l, position) {
			board_next(&position->board, l->move, &target);
			child = book_probe(book, &target);
			if (child == NULL) continue; // link to a missing position (corrupted book, see book_fix)
			position_negamax(child, book);
			if (l->score != -child->score.value) {
				l->score = -child->score.value;
				book->need_saving = true;
			}
			if (l->score > position->score.value) position->score.value = l->score;
			if (-child->score.upper > position->score.lower) position->score.lower = -child->score.upper;
			if (-child->score.lower > position->score.upper) position->score.upper = -child->score.lower;

			stat.n_wins += child->n_losses;
			stat.n_draws += child->n_draws;
			stat.n_losses += child->n_wins;
			stat.n_lines += child->n_lines;
		}

		position->n_wins = (unsigned int) MIN(UINT_MAX, stat.n_wins);
		position->n_draws = (unsigned int) MIN(UINT_MAX, stat.n_draws);
		position->n_losses = (unsigned int) MIN(UINT_MAX, stat.n_losses);
		position->n_lines = (unsigned int) MIN(UINT_MAX, stat.n_lines);
		if (old_value != position->score.value || old_lower != position->score.lower || old_upper != position->score.upper
			|| old_counts[0] != MIN(UINT_MAX, stat.n_wins) || old_counts[1] != MIN(UINT_MAX, stat.n_draws)
			|| old_counts[2] != MIN(UINT_MAX, stat.n_losses) || old_counts[3] != MIN(UINT_MAX, stat.n_lines)) book->need_saving = true;
	}

	return position->score.value;
}

/*
 * Parallel negamax.
 *
 * The negamaxed values of a position only depend on its leaf and on the final
 * values of its children (max and sums), so the result does not depend on the
 * order in which positions are computed. Several threads walk the book from the
 * same root; a thread computes a position after claiming it (done = epoch|0x80),
 * the others help by walking its children and then wait for it. Links always go
 * to a position with fewer empties (or to the passed position, that cannot pass
 * back), so waiting cannot deadlock (the links of a damaged book that do not are
 * not followed: see negamax_link_target).
 */
#define NEGAMAX_MAX_LINKS 64

#if defined(_MSC_VER)
#include <intrin.h>
static inline bool atomic_cas_uchar(unsigned char *p, const unsigned char from, const unsigned char to)
{
	return (unsigned char) _InterlockedCompareExchange8((volatile char*) p, (char) to, (char) from) == from;
}
#if defined(_M_X64) || defined(_M_IX86)
// x86 loads/stores already have acquire/release ordering: only stop the compiler
static inline unsigned char atomic_load_uchar(const unsigned char *p)
{
	const unsigned char v = *(volatile const unsigned char*) p;
	_ReadWriteBarrier();
	return v;
}
static inline void atomic_store_uchar(unsigned char *p, const unsigned char v)
{
	_ReadWriteBarrier();
	*(volatile unsigned char*) p = v;
}
#else // ARM64: use interlocked (full barrier) operations
static inline unsigned char atomic_load_uchar(const unsigned char *p)
{
	return (unsigned char) _InterlockedOr8((volatile char*) p, 0);
}
static inline void atomic_store_uchar(unsigned char *p, const unsigned char v)
{
	_InterlockedExchange8((volatile char*) p, (char) v);
}
#endif
#else
static inline bool atomic_cas_uchar(unsigned char *p, unsigned char from, const unsigned char to)
{
	return __atomic_compare_exchange_n(p, &from, to, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
}
static inline unsigned char atomic_load_uchar(const unsigned char *p) { return __atomic_load_n(p, __ATOMIC_ACQUIRE); }
static inline void atomic_store_uchar(unsigned char *p, const unsigned char v) { __atomic_store_n(p, v, __ATOMIC_RELEASE); }
#endif

/**
 * @brief Wait a moment for another thread: a pause of the processor, and at every 64th call
 * the turn is given to another thread.
 *
 * @param spin Count of the calls (kept by the caller: 0 before the first call).
 */
static inline void book_spin_wait(int *spin)
{
	if (++*spin < 64) {
#if defined(_M_X64) || defined(_M_IX86)
		_mm_pause();
#elif defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
		__builtin_ia32_pause(); // same as _mm_pause(), also without SSE headers (-march=i386)
#endif
	} else {
		*spin = 0;
#ifdef _WIN32
		Sleep(0);
#else
		sched_yield();
#endif
	}
}

/**
 * @brief Compute the negamaxed values of a position from its (already negamaxed) children.
 * Same computation as position_negamax().
 *
 * @return true if a saved score or count changed (the book needs saving: the caller tells it, as the threads
 * do not write book->need_saving at the same time).
 */
static bool position_negamax_compute(Position *position, Book *book, Position **children)
{
	bool changed = false;
	GameStats stat = {0,0,0,0};
	const int n_empties = board_count_empties(&position->board);
	const int search_depth = LEVEL[position->level][n_empties].depth;
	const int bias = (search_depth & 1) - (n_empties & 1);
	Link *l = position_links(position);
	int i;

	const int old_value = position->score.value, old_lower = position->score.lower, old_upper = position->score.upper;
	const unsigned int old_counts[4] = {position->n_wins, position->n_draws, position->n_losses, position->n_lines};
	position->score.value = position->score.lower = position->score.upper = -SCORE_INF;

	if (position->leaf.score > -SCORE_INF) {
		position->score.value = position->leaf.score;
		if (search_depth == n_empties && LEVEL[position->level][n_empties].selectivity == NO_SELECTIVITY) {
			position->score.lower = position->score.upper = position->score.value;
			if (position->leaf.score > 0) ++stat.n_wins;
			else if (position->leaf.score < 0) ++stat.n_losses;
			else ++stat.n_draws;
		} else if (search_depth == n_empties) {
			position->score.lower = score_char(position->score.value - book->options.endcut_error);
			position->score.upper = score_char(position->score.value + book->options.endcut_error);
		} else {
			position->score.lower = score_char(position->score.value - book->options.midgame_error - bias);
			position->score.upper = score_char(position->score.value + book->options.midgame_error - bias);
		}
		++stat.n_lines;
	}

	for (i = 0; i < position->n_link; ++i) {
		const Position *child = children[i];
		if (child == NULL) continue; // dangling link (corrupted book)
		if (l[i].score != -child->score.value) {
			l[i].score = -child->score.value;
			changed = true;
		}
		if (l[i].score > position->score.value) position->score.value = l[i].score;
		if (-child->score.upper > position->score.lower) position->score.lower = -child->score.upper;
		if (-child->score.lower > position->score.upper) position->score.upper = -child->score.lower;

		stat.n_wins += child->n_losses;
		stat.n_draws += child->n_draws;
		stat.n_losses += child->n_wins;
		stat.n_lines += child->n_lines;
	}

	position->n_wins = (unsigned int) MIN(UINT_MAX, stat.n_wins);
	position->n_draws = (unsigned int) MIN(UINT_MAX, stat.n_draws);
	position->n_losses = (unsigned int) MIN(UINT_MAX, stat.n_losses);
	position->n_lines = (unsigned int) MIN(UINT_MAX, stat.n_lines);
	if (old_value != position->score.value || old_lower != position->score.lower || old_upper != position->score.upper
		|| old_counts[0] != MIN(UINT_MAX, stat.n_wins) || old_counts[1] != MIN(UINT_MAX, stat.n_draws)
		|| old_counts[2] != MIN(UINT_MAX, stat.n_losses) || old_counts[3] != MIN(UINT_MAX, stat.n_lines)) changed = true;
	return changed;
}

struct PositionArray;
static struct PositionArray* book_array(const Book*, const Board*);
static void position_array_prefetch(const struct PositionArray*);
static Position* position_array_probe(struct PositionArray*, const Board*);

#ifdef BOOK_TEST_NEGAMAX_NO_PREFETCH // (test builds: the lookup of v4.5.5-nikque.9, one child after the other)
/**
 * @brief Position that a link leads to, for the parallel negamax.
 *
 * The threads walk the links without marking where they are, and wait for each other: a link of a
 * damaged book that leads back to its own position, or to a position above it (a pass that is not
 * one, a move on an occupied square), would be walked without end (stack overflow), where
 * position_negamax() stops at the positions that it has already seen. So only the links that make the
 * game progress are followed: a move that adds a disc, or the pass of a player who cannot move to a
 * player who can. Every link of a valid book is one of them.
 *
 * @param position Position.
 * @param link Link of the position.
 * @param book Opening book.
 * @return the position, or NULL if it is not in the book or if the link is not followed.
 */
static Position* negamax_link_target(const Position *position, const Link *link, const Book *book)
{
	const Board *board = &position->board;
	Board target;

	if (link->move <= H8) {
		board_next(board, link->move, &target);
		if (bit_count(target.player | target.opponent) <= bit_count(board->player | board->opponent)) return NULL;
	} else if (link->move == PASS) {
		if (can_move(board->player, board->opponent) || !can_move(board->opponent, board->player)) return NULL;
		target.player = board->opponent;
		target.opponent = board->player;
	} else {
		return NULL;
	}
	return book_probe(book, &target);
}
#else
/**
 * @brief Where the position that a link leads to is looked for, for the parallel negamax.
 *
 * The threads walk the links without marking where they are, and wait for each other: a link of a
 * damaged book that leads back to its own position, or to a position above it (a pass that is not
 * one, a move on an occupied square), would be walked without end (stack overflow), where
 * position_negamax() stops at the positions that it has already seen. So only the links that make the
 * game progress are followed: a move that adds a disc, or the pass of a player who cannot move to a
 * player who can. Every link of a valid book is one of them.
 *
 * @param position Position.
 * @param link Link of the position.
 * @param book Opening book.
 * @param unique Board to look for (output).
 * @return the array of positions to search for this board, or NULL if the link is not followed.
 */
static struct PositionArray* negamax_link_array(const Position *position, const Link *link, const Book *book, Board *unique)
{
	const Board *board = &position->board;
	Board target;

	if (link->move <= H8) {
		board_next(board, link->move, &target);
		if (bit_count(target.player | target.opponent) <= bit_count(board->player | board->opponent)) return NULL;
	} else if (link->move == PASS) {
		if (can_move(board->player, board->opponent) || !can_move(board->opponent, board->player)) return NULL;
		target.player = board->opponent;
		target.opponent = board->player;
	} else {
		return NULL;
	}
	board_unique(&target, unique);
	return book_array(book, unique);
}
#endif

static void position_negamax_parallel(Position *position, Book *book, const int id, bool *changed)
{
	const unsigned char done = book->epoch | POSITION_DONE, busy = book->epoch | POSITION_BUSY;
	const unsigned char d = atomic_load_uchar(&position->state);
	Position *children[NEGAMAX_MAX_LINKS];
	const Link *l;
	int i, n, first;
	bool own;

	if (d == done) return;
	own = (d != busy && atomic_cas_uchar(&position->state, d, busy));

	n = position->n_link;
	if (n > NEGAMAX_MAX_LINKS) fatal_error("too many links\n");
	l = position_links(position);
#ifndef BOOK_TEST_NEGAMAX_NO_PREFETCH
	{
		// the children are anywhere in memory: ask for all of them (the arrays, then their positions) before
		// reading the first one, instead of waiting for each one in turn.
		struct PositionArray *array[NEGAMAX_MAX_LINKS];
		Board unique[NEGAMAX_MAX_LINKS];

		for (i = 0; i < n; ++i) {
			array[i] = negamax_link_array(position, l + i, book, unique + i);
			if (array[i]) PREFETCH(array[i]);
		}
		for (i = 0; i < n; ++i) if (array[i]) position_array_prefetch(array[i]);
		for (i = 0; i < n; ++i) children[i] = array[i] ? position_array_probe(array[i], unique + i) : NULL;
	}
#else
	for (i = 0; i < n; ++i) children[i] = negamax_link_target(position, l + i, book);
#endif
	// threads start with different children to spread the work
	first = n ? (id * 7 + board_count_empties(&position->board)) % n : 0;
	for (i = 0; i < n; ++i) {
		Position *child = children[(first + i) % n];
		if (child && atomic_load_uchar(&child->state) != done) position_negamax_parallel(child, book, id, changed);
	}

	if (own) {
		if (position_negamax_compute(position, book, children)) *changed = true;
		atomic_store_uchar(&position->state, done);
	} else {
		int spin = 0;
		while (atomic_load_uchar(&position->state) != done) book_spin_wait(&spin);
	}
}

typedef struct NegamaxWorker {
	Book *book;
	Position *root;
	int id;
	bool changed;   /**< a score of a link changed (book->need_saving is set after the threads end) */
} NegamaxWorker;

static void* negamax_worker(void *v)
{
	NegamaxWorker *w = (NegamaxWorker*) v;
	position_negamax_parallel(w->root, w->book, w->id, &w->changed);
	return NULL;
}

/**
 * @brief Negamax the book sub-tree of a position with options.n_task threads.
 *
 * @param root Position to negamax (book_clean() must have been called).
 * @param book Opening book.
 */
static void book_negamax_position(Position *root, Book *book)
{
	book_test_add(book_test_negamaxes);
	NegamaxWorker w[MAX_THREADS];
	const int n = book_walk_threads();
	int i;

	if (n <= 1) {
		position_negamax(root, book);
		return;
	}
	for (i = 0; i < n; ++i) {
		w[i].book = book; w[i].root = root; w[i].id = i; w[i].changed = false;
	}
	book_run_workers(negamax_worker, w, sizeof *w, n);
	for (i = 0; i < n; ++i) if (w[i].changed) book->need_saving = true;
}

#ifdef BOOK_TEST_SUBTREE_STAT
static long long subtree_stat_new, subtree_stat_t0;
#define SUBTREE_STAT(s) (fprintf(stderr, "<subtree-stat %s: %lld ms, nodes %u>\n", s, real_clock() - subtree_stat_t0, book->n_nodes), subtree_stat_t0 = real_clock())
#else
#define SUBTREE_STAT(s)
#endif

/**
 * @brief Prune a position.
 *
 * @param position Position to prune.
 * @param book Opening book.
 * @param player_deviation Player's error.
 * @param opponent_deviation Opponent's error.
 * @param lower Error lower bound.
 * @param upper Error upper bound.
 * @param seen State flag of the walks with these deviations and bounds (0: none): a position that
 *        already has it is not walked again (the same positions would be kept again). The callers
 *        only use infinite bounds, so the deviations alone tell the walks apart.
 * @param seen_next State flag of the walks with the deviations swapped.
 */
static void position_prune(Position *position, Book *book, const int player_deviation, const int opponent_deviation, const int lower, const int upper, const unsigned char seen, const unsigned char seen_next)
{
	Link *l;
	Board target;
	Position *child;

	// if position is not done yet & good enough & inside the book height limit
	if (lower <= position->score.value && position->score.value <= upper && board_count_empties(&position->board) >= book->options.n_empties - 1) {
		if (seen && position_state_is(position->state, book, seen)) return;
#ifdef BOOK_TEST_SUBTREE_STAT
		if (!position_is_done(position, book)) ++subtree_stat_new;
#endif
		position_set_done(position, book); position->state |= seen; book->stats.n_todo++;
#ifndef BOOK_TEST_PRUNE_PROGRESS_OLD
		if (book->stats.n_todo % BOOK_INFO_RESOLUTION == 0) bprint("Book prune %lld to keep\r", book->stats.n_todo); // (once: not at each return to this count)
#endif

		// prune all children close to the best move
		foreach_link(l, position) {
			if (position->score.value - l->score <= player_deviation && lower <= l->score && l->score <= upper) {
				board_next(&position->board, l->move, &target);
				child = book_probe(book, &target);
				if (child) position_prune(child, book, opponent_deviation, player_deviation, -upper, -lower, seen_next, seen);
			}
		}
#ifdef BOOK_TEST_PRUNE_PROGRESS_OLD
		if (book->stats.n_todo % BOOK_INFO_RESOLUTION == 0) {
			bprint("Book prune %lld to keep\r", book->stats.n_todo);

		}
#endif
	}
}

#ifndef BOOK_TEST_PRUNE_MARK_OLD
/*
 * Marking of the positions to keep (book subtree, book prune) with several threads.
 *
 * position_prune() marks what can be reached from the root: a position (with the deviation of the
 * side to move, see book_prune) is walked once, and what is walked from it only depends on the
 * book, that does not change meanwhile. So the marks do not depend on the order of the walk, and
 * the number of marks is the same. Here each thread walks with its own stack of positions to walk;
 * a position is pushed by the thread that marks it (compare and swap of its state), so it is
 * walked once. A thread with nothing left takes positions from a shared pool, that the others fill
 * with the oldest half of their stacks (the positions nearest to the root) when a thread is waiting.
 * The positions that the links lead to are asked from the memory together, before the first one is read.
 */
#define PRUNE_MARK_LINKS 64
#ifndef PRUNE_MARK_SEQUENTIAL
#define PRUNE_MARK_SEQUENTIAL 4096 /**< marks done by the calling thread before the others are started (small walks) */
#endif
#define PRUNE_MARK_FLUSH 1024      /**< marks between two updates of the shared count */

static void book_clean(Book*);

/** @return true if book enhance goes on from a position to this child. */
static bool enhance_follows(const Position *position, const Position *child)
{
	return -child->score.upper >= position->score.lower || -child->score.lower >= position->score.upper;
}

/** @return true if book enhance expands the leaf of this position. */
static bool enhance_selects(const Position *position, const Book *book)
{
	if (position->leaf.score > -SCORE_INF) {
		const int n_empties = board_count_empties(&position->board);
		const int search_depth = LEVEL[position->level][n_empties].depth;
		const int bias = (search_depth & 1) - (n_empties & 1);
		int lower, upper;
		// is solving
		if (search_depth == n_empties && LEVEL[position->level][n_empties].selectivity == NO_SELECTIVITY) {
			lower = upper = position->leaf.score;
		// is pre-solving
		} else if (search_depth == n_empties) {
			lower = position->leaf.score - book->options.endcut_error;
			upper = position->leaf.score + book->options.endcut_error;
		} else { // midgame
			lower = position->leaf.score - book->options.midgame_error - bias;
			upper = position->leaf.score + book->options.midgame_error - bias;
		}

		return lower >= position->score.lower || upper >= position->score.upper;
	}
	return false;
}
/** list of positions to walk: pointer | kind of walk (0 or 1) */
typedef struct PruneMarkList {
	uintptr_t *item;
	long long n, size;
} PruneMarkList;

typedef struct PruneMarkShared {
	Book *book;
	Lock lock;                 /**< guards the pool and the counts */
	PruneMarkList pool;        /**< positions given by the threads to the threads with nothing left */
	int n_busy;                /**< threads with positions to walk */
	long long n_marked;        /**< marks so far (progress), with those of the walks before */
	int deviation[2];          /**< deviation of the side to move, for each kind of walk */
	unsigned char seen[2];     /**< state flag of each kind of walk */
	bool enhance;              /**< the walk of book enhance (see book_enhance_mark) */
	unsigned char hungry;      /**< a thread waits for positions */
	unsigned char oom;
} PruneMarkShared;

typedef struct PruneMarkWorker {
	PruneMarkShared *shared;
	PruneMarkList stack;
	PruneMarkList found;       /**< book enhance: the positions to expand */
	long long n_marked, n_new, n_flushed; /**< marks; marks of positions that were not kept yet (only counted in the test builds); marks given to the shared count */
	bool busy;
} PruneMarkWorker;

static bool prune_mark_reserve(PruneMarkList *l, const long long n)
{
	if (l->n + n > l->size) {
		const long long size = l->size + l->size / 2 + n + 1024;
		uintptr_t *item = (uintptr_t*) realloc(l->item, size * sizeof *item);
		if (item == NULL) return false;
		l->item = item; l->size = size;
	}
	return true;
}

/** Mark a position for a kind of walk, if it is not, and push it. */
static void prune_mark_push(PruneMarkWorker *w, Position *p, const int kind)
{
	PruneMarkShared *s = w->shared;
	const Book *book = s->book;
	const unsigned char seen = s->seen[kind];
	unsigned char state;

	if (s->enhance) {
		if (board_count_empties(&p->board) < book->options.n_empties || board_is_game_over(&p->board)) return;
	} else if (board_count_empties(&p->board) < book->options.n_empties - 1) return; // (the bounds of position_prune are infinite)
	do {
		state = atomic_load_uchar(&p->state);
		if (position_state_is(state, book, seen)) return; // already walked
	} while (!atomic_cas_uchar(&p->state, state, (unsigned char) (position_state_set(state, book->epoch, POSITION_DONE) | seen)));
	++w->n_marked;
#ifdef BOOK_TEST_SUBTREE_STAT
	if (!position_state_is(state, book, POSITION_DONE)) ++w->n_new;
#endif
	if (!prune_mark_reserve(&w->stack, 1)) { atomic_store_uchar(&s->oom, 1); return; }
	w->stack.item[w->stack.n++] = (uintptr_t) p | (uintptr_t) kind;
}

/** Walk the links of a marked position (as position_prune). */
static void prune_mark_walk(PruneMarkWorker *w, const uintptr_t item)
{
	PruneMarkShared *s = w->shared;
	const Book *book = s->book;
	const Position *position = (const Position*) (item & ~(uintptr_t) 1);
	const int kind = (int) (item & 1);
	const int deviation = s->deviation[kind];
	const Link *l = position_links(position);
	struct PositionArray *array[PRUNE_MARK_LINKS];
	Board unique[PRUNE_MARK_LINKS], target;
	int first, i, n;

	for (first = 0; first < position->n_link; first += PRUNE_MARK_LINKS, l += PRUNE_MARK_LINKS) {
		n = MIN(position->n_link - first, PRUNE_MARK_LINKS);
		for (i = 0; i < n; ++i) {
			array[i] = NULL;
			if (position->score.value - l[i].score <= deviation && -SCORE_INF <= l[i].score) { // (the bounds of position_prune are infinite: a score is never above SCORE_INF)
				board_next(&position->board, l[i].move, &target);
				board_unique(&target, unique + i);
				array[i] = book_array(book, unique + i);
				PREFETCH(array[i]);
			}
		}
		for (i = 0; i < n; ++i) if (array[i]) position_array_prefetch(array[i]);
		for (i = 0; i < n; ++i) if (array[i]) {
			Position *child = position_array_probe(array[i], unique + i);
			if (child && (!s->enhance || enhance_follows(position, child))) prune_mark_push(w, child, kind ^ 1);
		}
	}
	if (s->enhance && enhance_selects(position, book)) {
		if (prune_mark_reserve(&w->found, 1)) w->found.item[w->found.n++] = (uintptr_t) position;
		else atomic_store_uchar(&s->oom, 1);
	}
}

/** Add the marks of a thread to the shared count (and show it, as position_prune). */
static void prune_mark_flush(PruneMarkWorker *w)
{
	PruneMarkShared *s = w->shared;
	const long long n = w->n_marked - w->n_flushed;

	if (n == 0 || s->enhance) return;
	lock(s);
	s->n_marked += n;
	if (s->n_marked / BOOK_INFO_RESOLUTION != (s->n_marked - n) / BOOK_INFO_RESOLUTION) {
		bprint("Book prune %lld to keep\r", s->n_marked / BOOK_INFO_RESOLUTION * BOOK_INFO_RESOLUTION);
	}
	unlock(s);
	w->n_flushed = w->n_marked;
}

/** Take positions from the pool. @return false when the walk is over. */
static bool prune_mark_take(PruneMarkWorker *w)
{
	PruneMarkShared *s = w->shared;
	int spin = 0;

	for (;;) {
		lock(s);
		if (w->busy) { --s->n_busy; w->busy = false; }
		if (s->pool.n > 0) {
			const long long n = (s->pool.n + 1) / 2;
			if (prune_mark_reserve(&w->stack, n)) {
				memcpy(w->stack.item + w->stack.n, s->pool.item + s->pool.n - n, n * sizeof *w->stack.item);
				w->stack.n += n; s->pool.n -= n;
				++s->n_busy; w->busy = true;
				unlock(s);
				return true;
			}
			atomic_store_uchar(&s->oom, 1); // (the walk is done again by the caller)
			s->pool.n = 0;
		}
		if (s->n_busy == 0) { unlock(s); return false; }
		atomic_store_uchar(&s->hungry, 1);
		unlock(s);
		book_spin_wait(&spin);
	}
}

/** Give the oldest half of the stack to the pool. */
static void prune_mark_give(PruneMarkWorker *w)
{
	PruneMarkShared *s = w->shared;
	const long long n = w->stack.n / 2;

	lock(s);
	if (prune_mark_reserve(&s->pool, n)) {
		memcpy(s->pool.item + s->pool.n, w->stack.item, n * sizeof *w->stack.item);
		s->pool.n += n;
		w->stack.n -= n;
		memmove(w->stack.item, w->stack.item + n, w->stack.n * sizeof *w->stack.item);
	} // (else: this thread walks them)
	atomic_store_uchar(&s->hungry, 0);
	unlock(s);
}

static void* prune_mark_worker(void *v)
{
	PruneMarkWorker *w = (PruneMarkWorker*) v;
	PruneMarkShared *s = w->shared;

	for (;;) {
		if (w->stack.n == 0 && !prune_mark_take(w)) break;
		prune_mark_walk(w, w->stack.item[--w->stack.n]);
		if (w->n_marked - w->n_flushed >= PRUNE_MARK_FLUSH) prune_mark_flush(w);
		if (w->stack.n >= 2 && atomic_load_uchar(&s->hungry)) prune_mark_give(w);
	}
	prune_mark_flush(w);
	return NULL;
}

/**
 * @brief Mark the positions to keep with several threads (same marks and same count as position_prune).
 *
 * @param book Opening book.
 * @param root Position to walk from.
 * @param deviation_0 Deviation of the side to move in the walks of kind 0.
 * @param deviation_1 Deviation of the side to move in the walks of kind 1 (the positions after those of kind 0, and back).
 * @param seen_0 State flag of the walks of kind 0.
 * @param seen_1 State flag of the walks of kind 1.
 * @param kind Kind of the walk of the root.
 * @return false if nothing was marked (one thread; or memory exhausted: then book_clean() was called):
 *         position_prune() is to be used.
 */
static bool book_prune_mark(Book *book, Position *root, const int deviation_0, const int deviation_1, const unsigned char seen_0, const unsigned char seen_1, const int kind)
{
	PruneMarkShared shared;
	PruneMarkWorker *w;
	const int n = book_walk_threads();
	int i;
	bool ok;

	if (n <= 1 || seen_0 == 0 || seen_1 == 0) return false;
	w = (PruneMarkWorker*) calloc(n, sizeof *w);
	if (w == NULL) return false;
	memset(&shared, 0, sizeof shared);
	shared.book = book; shared.n_marked = book->stats.n_todo;
	shared.deviation[0] = deviation_0; shared.deviation[1] = deviation_1;
	shared.seen[0] = seen_0; shared.seen[1] = seen_1;
	lock_init(&shared);
	for (i = 0; i < n; ++i) w[i].shared = &shared;

	// the calling thread starts alone: a small walk ends here
	prune_mark_push(w, root, kind);
	while (w->stack.n > 0 && w->n_marked < PRUNE_MARK_SEQUENTIAL) prune_mark_walk(w, w->stack.item[--w->stack.n]);
	if (w->stack.n > 0) {
		w->busy = true; shared.n_busy = 1;
		book_run_workers(prune_mark_worker, w, sizeof *w, n);
	}

	ok = !shared.oom;
	if (ok) {
		for (i = 0; i < n; ++i) {
			book->stats.n_todo += w[i].n_marked;
#ifdef BOOK_TEST_SUBTREE_STAT
			subtree_stat_new += w[i].n_new;
#endif
		}
	} else {
		book_clean(book);
	}
	for (i = 0; i < n; ++i) free(w[i].stack.item);
	free(shared.pool.item);
	lock_free(&shared);
	free(w);
	return ok;
}
/**
 * @brief The walk of book enhance with several threads (the marks of position_enhance).
 *
 * The positions to expand are what can be reached by the links that enhance_follows() takes: the book does not
 * change meanwhile, so the order of the walk does not matter, and the list is sorted before it is expanded.
 *
 * @param book Opening book (book_clean() must have been called).
 * @param root Position to walk from.
 * @return false if nothing was marked (one thread; or memory exhausted): position_enhance() is to be used.
 */
static bool book_enhance_mark(Book *book, Position *root)
{
	PruneMarkShared shared;
	PruneMarkWorker *w;
	const int n = book_walk_threads();
	long long j;
	int i;
	bool ok;

	if (n <= 1) return false;
	w = (PruneMarkWorker*) calloc(n, sizeof *w);
	if (w == NULL) return false;
	memset(&shared, 0, sizeof shared);
	shared.book = book;
	shared.enhance = true;
	shared.deviation[0] = shared.deviation[1] = 4 * SCORE_INF; // (every link)
	shared.seen[0] = shared.seen[1] = POSITION_DONE;
	lock_init(&shared);
	for (i = 0; i < n; ++i) w[i].shared = &shared;

	prune_mark_push(w, root, 0);
	while (w->stack.n > 0 && w->n_marked < PRUNE_MARK_SEQUENTIAL) prune_mark_walk(w, w->stack.item[--w->stack.n]);
	if (w->stack.n > 0) {
		w->busy = true; shared.n_busy = 1;
		book_run_workers(prune_mark_worker, w, sizeof *w, n);
	}

	ok = !shared.oom;
	if (ok) {
		for (i = 0; i < n; ++i) {
			for (j = 0; j < w[i].found.n; ++j) book_mark_todo(book, (Position*) w[i].found.item[j]);
		}
	} else {
		book_clean(book);
	}
	for (i = 0; i < n; ++i) { free(w[i].stack.item); free(w[i].found.item); }
	free(shared.pool.item);
	lock_free(&shared);
	free(w);
	return ok;
}
#endif

/**
 * @brief Remove bad links after book pruning.
 *
 * @param position Position to fix.
 * @param book Opening book.
 * @return true if a link was removed.
 */
static bool book_has_symmetric_position(const Book *book, const Board *board)
{
	for (int s = 0; s < 8; ++s) {
		Board equivalent;
		board_symetry(board, s, &equivalent);
		if (position_array_probe(book_array(book, &equivalent), &equivalent)) return true;
	}
	return false;
}

static bool position_remove_missing_links(Position *position, Book *book, bool allow_symmetry)
{
	int i, n = 0;
	const Link *l = position_links(position);
	Link kept[256];
	Board target;

	for (i = 0; i < position->n_link; ++i) {
		const int x = l[i].move;
		if (allow_symmetry && (x > PASS || (x == PASS ? !board_is_pass(&position->board)
			: board_is_occupied(&position->board, x) || !board_flip(&position->board, x)))) {
			kept[n++] = l[i];
			continue;
		}
		board_next(&position->board, x, &target);
		if (!book_probe(book, &target) && (!allow_symmetry || !book_has_symmetric_position(book, &target))) {
			if (l[i].score > position->leaf.score) position->leaf = l[i];
		} else {
			kept[n++] = l[i];
		}
	}
	if (n == position->n_link) return false;
	if (!position_set_links(position, kept, n)) {
		error("cannot allocate opening book position's moves\n");
	}
	return true;
}

static bool position_remove_links(Position *position, Book *book)
{
	return position_remove_missing_links(position, book, false);
}

/**
 * @brief Check if a position links to a position missing from the book.
 *
 * @param position Position.
 * @param book Opening book.
 * @return true if a link leads to a missing position.
 */
static bool position_has_missing_link(const Position *position, const Book *book)
{
	const Link *l;
	Board target;

	foreach_link(l, position) {
		board_next(&position->board, l->move, &target);
		if (!book_probe(book, &target)) return true;
	}
	return false;
}

/*
 * book leaf recalculate: the walks of book deviate / deviate2 are used to select the leaves to
 * search again, instead of the positions to expand.
 * - LEAF_RECALC_SELECTED: the positions that the walk would expand (leaf inside the limits),
 * - LEAF_RECALC_WALKED: every position that the walk goes through, and the positions just under
 *   the depth of the walk that its links lead to (they only have a leaf, that gives its score to the link).
 * In both cases, only a leaf that a search can give again and that is not solved (exact, whatever eval.dat).
 */
enum { LEAF_RECALC_OFF = 0, LEAF_RECALC_SELECTED, LEAF_RECALC_WALKED };
static int leaf_recalc_mode = LEAF_RECALC_OFF;

/** book fill with loss limits: the deviate2/3 walk selects every position it walks, not only those whose leaf it would expand */
static bool deviate_walk_all = false;

/** book reduce: the positions of the walk are only marked "todo", not listed (they are not expanded) */
static bool deviate_mark_only = false;

/** @return true if the level of the position solves it exactly. */
static bool position_is_solved(const Position *p)
{
	const int n_empties = board_count_empties(&p->board);
	return LEVEL[p->level][n_empties].depth == n_empties && LEVEL[p->level][n_empties].selectivity == NO_SELECTIVITY;
}

/**
 * @return true if the leaf of the position can be searched again by book leaf recalculate:
 * a move that is not a link yet, or the pass of a position without move whose next position is not in
 * the book (its score comes from a search too; up to v4.5.5-nikque.12 such a leaf was never searched again).
 * The end of the game has no leaf move, and its score does not depend on eval.dat. A leaf already searched
 * again by this command, or by a run of it that did not end (POSITION_MARK_RECALCULATED), is not searched again.
 */
static bool leaf_recalc_wanted(const Position *p)
{
	const int n_moves = get_mobility(p->board.player, p->board.opponent);

	if (p->leaf.move == NOMOVE || position_is_solved(p) || (p->marks & POSITION_MARK_RECALCULATED)) return false;
	if (n_moves == 0) return p->n_link == 0 && p->leaf.move == PASS;
	return p->n_link < n_moves;
}

/** @return true if the position is just under the depth of the walks and gets its leaf searched again (LEAF_RECALC_WALKED). */
static bool leaf_recalc_bottom(const Book *book, const Position *p)
{
	return leaf_recalc_mode == LEAF_RECALC_WALKED && board_count_empties(&p->board) == book->options.n_empties - 1 && leaf_recalc_wanted(p);
}

/**
 * @brief Tell if a walk of book deviate selects the leaf of a position (position_deviate, depth_worker_run).
 *
 * The leaf is expanded when its score is close enough to the best score and inside the bounds. With book leaf
 * recalculate, the leaf is searched again: a leaf that a search can give again, selected so (LEAF_RECALC_SELECTED)
 * or whatever its score (LEAF_RECALC_WALKED).
 *
 * @param p Position walked.
 * @param player_deviation Player's error.
 * @param lower Error lower bound.
 * @param upper Error upper bound.
 * @return true if the leaf is selected.
 */
static inline bool deviate_leaf_selected(const Position *p, const int player_deviation, const int lower, const int upper)
{
	const bool close = p->score.value - p->leaf.score <= player_deviation && lower <= p->leaf.score && p->leaf.score <= upper;

	if (leaf_recalc_mode != LEAF_RECALC_OFF) return leaf_recalc_wanted(p) && (leaf_recalc_mode == LEAF_RECALC_WALKED || close);
	return close;
}

/**
 * @brief Tell if a walk of book deviate2 / deviate3 selects the leaf of a position (position_deviate_total, loss_worker_run).
 *
 * The leaf is expanded when its loss is within the limits, as a link that the walk follows. With book leaf
 * recalculate, as deviate_leaf_selected().
 *
 * @param p Position walked.
 * @param move_loss Maximum loss for one move.
 * @param total_loss Maximum cumulative loss for both players.
 * @param loss Accumulated loss from the root to this position.
 * @return true if the leaf is selected.
 */
static inline bool deviate_total_leaf_selected(const Position *p, const int move_loss, const int total_loss, const int loss)
{
	const int move_error = p->score.value - p->leaf.score;
	const bool close = 0 <= move_error && move_error <= move_loss && loss + move_error <= total_loss;

	if (leaf_recalc_mode != LEAF_RECALC_OFF) return leaf_recalc_wanted(p) && (leaf_recalc_mode == LEAF_RECALC_WALKED || close);
	return p->leaf.move != NOMOVE && close;
}

/**
 * @brief Deviate a position.
 *
 * This is the important part of the opening book code, where it finds the best moves to add to the book.
 * Considering the current position, it will deviate a child position or expand a move if:
 * - move_score < best_score - player_deviation
 * - lower <= move_score <= upper.
 * So a good candidate move for expansion need to have a score close to the best move, and not too far
 * from the root score from where we deviates, in order to not derive to very bad positions.
 *
 * @param position Position to expand.
 * @param book Opening book.
 * @param player_deviation Player's error.
 * @param opponent_deviation Opponent's error.
 * @param lower Error lower bound.
 * @param upper Error upper bound.
 */
static void position_deviate(Position *position, Book *book, const int player_deviation, const int opponent_deviation, const int lower, const int upper)
{
	Link *l;
	Board target;
	Position *child;

	// if position is not done yet & good enough & inside the book height limit
	if (!position_is_done(position, book) && lower <= position->score.value && position->score.value <= upper && board_count_empties(&position->board) >= book->options.n_empties && !board_is_game_over(&position->board)) {
		position_set_done(position, book);

		// deviate all children close to the best move
		foreach_link(l, position) {
			if (position->score.value - l->score <= player_deviation && lower <= l->score && l->score <= upper) {
				board_next(&position->board, l->move, &target);
				child = book_probe(book, &target);
				if (child) position_deviate(child, book, opponent_deviation, player_deviation, -upper, -lower);
			}
		}

		// expand the best remaining move (book leaf recalculate: search it again)
		if (deviate_leaf_selected(position, player_deviation, lower, upper)) {
			book_mark_todo(book, position); book->stats.n_todo++;
			if (leaf_recalc_mode == LEAF_RECALC_OFF && book->stats.n_todo % 10 == 0) bprint("Book deviate %lld todo\r", book->stats.n_todo);
		}
	} else if (leaf_recalc_bottom(book, position) && !position_is_todo(position, book) && lower <= position->score.value && position->score.value <= upper) {
		book_mark_todo(book, position); book->stats.n_todo++;
	}
}

/*
 * Visit marks of the deviate/deviate2/deviate3 selection walks: one byte per position, allocated
 * for a walk only (up to v4.5.5-nikque.4, a 4-byte field of every position kept them):
 * - deviate2/3: smallest accumulated loss + 1 (the loss is at most VISIT_LOSS_MAX),
 * - deviate: ply parity + 1,
 * - 0: not visited.
 * No position is added or moved during a walk.
 */
#define VISIT_LOSS_MAX 254

static inline unsigned char* book_visit(const Book*, const Position*);

/**
 * @brief Deviate a position with a cumulative move-loss limit.
 *
 * Each move loss is the difference between the best score at the position
 * and the score of the selected move. The path is followed only while every
 * move loss is within move_loss and their sum is within total_loss.
 *
 * @param position Position to expand.
 * @param book Opening book.
 * @param move_loss Maximum loss for one move.
 * @param total_loss Maximum cumulative loss for both players.
 * @param loss Accumulated loss from the root to this position.
 * @param skip_solved deviate2: the leaf of a solved position is not expanded. deviate3: it is.
 */
static void position_deviate_total(Position *position, Book *book, const int move_loss, const int total_loss, const int loss, const bool skip_solved)
{
	Link *l;
	Board target;
	Position *child;
	int move_error;

	if (loss <= total_loss && leaf_recalc_bottom(book, position) && !position_is_todo(position, book)) {
		book_mark_todo(book, position);
		book->stats.n_todo++;
	}
	if (loss > total_loss || board_count_empties(&position->board) < book->options.n_empties || board_is_game_over(&position->board)) return;

	// A transposed position can be reached by different lines. Revisit it only
	// when this path has a smaller accumulated loss, which leaves more budget
	// available to every continuation from the same board.
	{
		unsigned char *v = book_visit(book, position);
		if (*v && loss >= *v - 1) return;
		position_set_done(position, book);
		*v = (unsigned char) (loss + 1);
	}

	foreach_link(l, position) {
		move_error = position->score.value - l->score;
		if (0 <= move_error && move_error <= move_loss && loss + move_error <= total_loss) {
			board_next(&position->board, l->move, &target);
			child = book_probe(book, &target);
			if (child) position_deviate_total(child, book, move_loss, total_loss, loss + move_error, skip_solved);
		}
	}

	if (deviate_walk_all) { // (book fill with loss limits: every position of the walk)
		if (!position_is_todo(position, book)) {
			if (deviate_mark_only) position_set_todo(position, book);
			else book_mark_todo(book, position);
			book->stats.n_todo++;
		}
		return;
	}

	// A solved leaf already has an exact score. Keep following existing links,
	// since descendants may have been searched at a lower level.
	if (skip_solved && position_is_solved(position)) return;

	// expand the best remaining move (book leaf recalculate: search it again)
	if (deviate_total_leaf_selected(position, move_loss, total_loss, loss) && !position_is_todo(position, book)) {
		book_mark_todo(book, position);
		book->stats.n_todo++;
		if (leaf_recalc_mode == LEAF_RECALC_OFF && book->stats.n_todo % 10 == 0) bprint("Book deviate%d %lld todo\r", skip_solved ? 2 : 3, book->stats.n_todo);
	}
}

/**
 * @brief Enhance a position.
 *
 * This is the other important part of the opening book code, where it finds the best moves to add to the book by
 * another methods.
 * Here we use score negamaxed bounds to decide if a move is a good candidate for further enhancement or for expansion.
 * The purpose of this algorithm is to find more moves.
 *
 *
 * @param position Position to expand.
 * @param book Opening book.
 */
static void position_enhance(Position *position, Book *book)
{
	Link *l;
	Board target;
	Position *child;

	if (!position_is_done(position, book) && board_count_empties(&position->board) >= book->options.n_empties && !board_is_game_over(&position->board)) {
		position_set_done(position, book);

		foreach_link(l, position) {
			board_next(&position->board, l->move, &target);
			child = book_probe(book, &target);
			if (child && enhance_follows(position, child)) {
				position_enhance(child, book);
			}
		}

		if (enhance_selects(position, book)) book_mark_todo(book, position);
	}
}

/**
 * @brief Feed hash from a position.
 *
 * Go through the book sub-tree following the current position & feed the hash table from this position.
 *
 * @param board Position to expand.
 * @param book Opening book.
 * @param search Hashtables container.
 * @param is_pv Flag to tell if the position is from the principal variation.
 */
static void board_feed_hash(Board *board, const Book *book, Search *search, const bool is_pv)
{
	Position *position;
	const unsigned long long hash_code = board_get_hash_code(board);
	MoveList movelist;
	Move *m;
	HashStoreData hash_data;

	position = book_probe(book, board);
	if (position) {
		const int n_empties = board_count_empties(&position->board);
		const int score = position->score.value;
		int move = NOMOVE;

		hash_data.data.wl.c.depth = LEVEL[position->level][n_empties].depth;
		hash_data.data.wl.c.selectivity = LEVEL[position->level][n_empties].selectivity;

		position_get_moves(position, board, &movelist);
		foreach_move(m, movelist) {
			if (move == NOMOVE) move = m->x;
			board_update(board, m);
				board_feed_hash(board, book, search, is_pv && m->score == score);
			board_restore(board, m);
		}

		hash_data.data.lower = hash_data.data.upper = score;
		hash_data.data.move[0] = move;
		hash_feed(&search->hash_table, board, hash_code, &hash_data);
		if (is_pv) hash_feed(&search->pv_table, board, hash_code, &hash_data);
	}
}

/**
 * @brief Fill the opening book.
 *
 * Add positions to link existing positions.
 *
 * @param board Candidate position.
 * @param book Opening book.
 * @param depth Depth at which to search a link.
 * @return true if the board is in the book, possibly just after having been added to it.
 */
static bool board_fill(Board *board, Book *book, int depth)
{
	if (depth > 0) {
		MoveList movelist;
		Move *m;
		bool filled = false;

		movelist_get_moves(&movelist, board);
		if (movelist.n_moves == 0 && can_move(board->opponent, board->player)) {
			board_pass(board);
			if (board_fill(board, book, depth - 1)) {
				book_add_board(book, board);
				filled = true;
			}
			board_pass(board);							
		} else {
			foreach_move(m, movelist) {
				board_update(board, m);
				if (board_fill(board, book, depth - 1)) {
					book_add_board(book, board);
					filled = true;
				}
				board_restore(board, m);					
			}
		}
		return filled;
	}
	return book_probe(book, board) != NULL;
}

/**
 * @brief Fix a position.
 *
 * Recompute all elements of a buggy position
 *
 * @param position Position to fix.
 * @param book Opening book.
 */
static void position_fix(Position *position, Book *book)
{
	Board board;

	if ((position->board.player & position->board.opponent) || 
	    ((position->board.player | position->board.opponent) & 0x0000001818000000ULL) != 0x0000001818000000ULL) {
		position_free(position);
		position_init(position);
		return;
	}
	board_unique(&position->board, &board);
	position_free(position);
	position_init(position);
	position->board = board;
	position->level = book->options.level;
	position_link(position, book);
	position_search(position, book);
}

/**
 * @brief An array with positions.
 *
 * size < 0: the positions are stored in the book pool filled at load time
 * (capacity == n); the array moves to its own heap block when it grows.
 */
typedef struct PositionArray {
	Position *positions;
	int n;
	int size;
} PositionArray;

/**
 * @brief Allocate the visit marks (all 0) of a walk.
 * @param book Opening book.
 * @return true on success.
 */
static bool book_visit_init(Book *book)
{
	unsigned int i, n = 0;

	book->visit_first = (unsigned int*) malloc(book->n * sizeof *book->visit_first);
	if (book->visit_first) {
		for (i = 0; i < (unsigned int) book->n; ++i) {
			book->visit_first[i] = n;
			n += (unsigned int) book->array[i].n;
		}
		book->visit = (unsigned char*) calloc(n ? n : 1, 1);
	}
	if (book->visit == NULL) {
		free(book->visit_first); book->visit_first = NULL;
		error("cannot allocate the marks of the book walk");
		book->failed = true;
		return false;
	}
	return true;
}

/** @brief Clear the visit marks (for a new walk). */
static void book_visit_clear(Book *book)
{
	const PositionArray *a = book->array + book->n - 1;
	memset(book->visit, 0, book->visit_first[book->n - 1] + (size_t) a->n);
}

/** @brief Free the visit marks. */
static void book_visit_free(Book *book)
{
	free(book->visit); book->visit = NULL;
	free(book->visit_first); book->visit_first = NULL;
}

/** @return the visit mark of a position. */
static inline unsigned char* book_visit(const Book *book, const Position *p)
{
	const unsigned long long i = board_get_hash_code(&p->board) & (book->n - 1);
	return book->visit + book->visit_first[i] + (p - book->array[i].positions);
}



/**
 * @brief Initialize the array.
 *
 * @param a Positions' array.
 */
static void position_array_init(PositionArray *a)
{
	a->size = a->n = 0;
	a->positions = NULL;
}

static bool position_array_reserve(PositionArray *a, Book *book, int size)
{
	if (a->size >= size) return true;
	if ((size_t) size > SIZE_MAX / sizeof (Position)) return false;
	Position *positions;
	if (a->size < 0) {
		positions = (Position*) malloc((size_t) size * sizeof (Position));
		if (positions) {
			memcpy(positions, a->positions, (size_t) a->n * sizeof (Position));
			book->pool_unused += a->n;
		}
	} else {
		positions = (Position*) realloc(a->positions, (size_t) size * sizeof (Position));
	}
	if (!positions) return false;
	a->positions = positions;
	a->size = size;
	return true;
}

/**
 * @brief Add a position to the array.
 *
 * @param a Positions' array.
 * @param p Position to add.
 * @param book Book of the array (its epoch; the places of the pool no longer used are counted).
 * @return 1 if the position was added, 0 if it is already there, -1 if memory is exhausted.
 */
static int position_array_add(PositionArray *a, const Position *p, Book *book)
{
	int i;

	board_check(&p->board);
	// (a position without any link nor leaf is what book_merge_file() adds: position_merge() does not copy the
	// links, that book_link_parallel() builds just after. position_is_ok() refuses it: "nomove is wrong")
	assert((p->n_link == 0 && p->leaf.move == NOMOVE) || position_is_ok(p));

	for (i = 0; i < a->n; ++i) if (board_equal(&a->positions[i].board, &p->board)) return 0;
	if (a->size < 0 || a->n == a->size) {
		const int size = a->n < INT_MAX - a->n / 2 - 1 ? a->n + a->n / 2 + 1 : INT_MAX;
		if (a->n == INT_MAX || !position_array_reserve(a, book, size)) {
			error("cannot add a position to the book\n");
			return -1;
		}
	}
	a->positions[a->n] = *p;
	a->positions[a->n].state = book->epoch | POSITION_DONE; // a new position is 'done' until the next book_clean()
	++a->n;
	return 1;
}

/**
 * @brief Free resources used by a position array.
 *
 * @param a Positions' array.
 */
static void position_array_free(PositionArray *a)
{
	int i;
	for (i = 0; i < a->n; ++i) position_free(a->positions + i);
	if (a->size >= 0) free(a->positions); // else: part of the book pool
}

/**
 * @brief Find a position in the array.
 *
 * @param a Positions' array.
 * @param board Board to find in the array.
 * @return a position containg the board (or a symetry) or NULL is no position is found.
 */
static Position* position_array_probe(PositionArray *a, const Board *board)
{
	int i;
	for (i = 0; i < a->n; ++i) if (board_equal(&a->positions[i].board, board)) return a->positions + i;
	return NULL;
}

/** @brief Array of the positions with the hash code of a board (unique board). */
static PositionArray* book_array(const Book *book, const Board *unique)
{
	return book->array + (board_get_hash_code(unique) & (book->n - 1));
}

/** @brief Ask for the memory of the positions of an array (the array itself must be readable). */
static void position_array_prefetch(const PositionArray *a)
{
	PREFETCH(a->positions);
}

#define foreach_position(p, a, b) \
	for (a = b->array; a < b->array + b->n; ++a) \
	for (p = a->positions; p < a->positions + a->n; ++p)

/**
 * @brief Set book date.
 *
 * @param book Opening book.
 */
static void book_set_date(Book *book)
{
	time_t t = time(NULL);
	struct tm *tm = localtime(&t);

	memset(&book->date, 0, sizeof book->date); // the padding byte is saved too
	book->date.year = tm->tm_year + 1900;
	book->date.month = tm->tm_mon + 1;
	book->date.day = tm->tm_mday;
	book->date.hour = tm->tm_hour;
	book->date.minute = tm->tm_min;
	book->date.second = tm->tm_sec;
}

/**
 * @brief Get book age, in seconds.
 *
 * @param book Opening book.
 * @return book age.
 */
static double book_get_age(Book *book)
{
	struct tm tm;
	double t;

	tm.tm_year = book->date.year - 1900;
	tm.tm_mon = book->date.month - 1;
	tm.tm_mday = book->date.day;
	tm.tm_hour = book->date.hour;
	tm.tm_min = book->date.minute;
	tm.tm_sec = book->date.second;
	tm.tm_isdst = -1;

	t = difftime(time(NULL), mktime(&tm));

	return t;
}



/**
 * @brief Find a position in the book.
 *
 * @param book Opening book.
 * @param board Board to find in the array.
 * @return a position containg the board (or a symetry) or NULL is no position is found.
 */
static Position* book_probe(const Book *book, const Board *board)
{
	Board unique;
	board_unique(board, &unique);
	return position_array_probe(book->array + (board_get_hash_code(&unique) & (book->n - 1)), &unique);
}

/**
 * @brief Add a position to the book.
 *
 * A failure (memory exhausted, or more positions than the file format allows)
 * sets book->failed, which stops the learning loops.
 *
 * @param book Opening book.
 * @param p Position to add.
 * @return 1 if the position was added, 0 if it is already in the book, -1 on failure.
 *         On 0 or -1 the caller keeps the ownership of the position's links.
 */
static int book_add(Book *book, const Position *p)
{
	const unsigned long long i = board_get_hash_code(&p->board) & (book->n - 1);
	int r;

	if (book->n_nodes == UINT_MAX) {
		error("the book cannot hold more than %u positions\n", UINT_MAX);
		book->failed = true;
		return -1;
	}
	r = position_array_add(book->array + i, p, book);
	if (r > 0) {
		++book->n_nodes;
		++book->stats.n_nodes;
	} else if (r < 0) {
		book->failed = true;
	}
	return r;
}

/**
 * @brief Take a position out of the book, by its place (its links are not released: see book_remove_at).
 *
 * The position is removed from the array that holds it, whatever its hash code tells: a position whose
 * board was changed by book fix (made unique) may be in another bucket than its board's, and searching
 * the board's bucket would not find it (the callers, which loop on the array, would never end).
 *
 * @param book Opening book.
 * @param a Array that holds the position.
 * @param i Index of the position in the array.
 */
static void book_detach_at(Book *book, PositionArray *a, const int i)
{
	if (a->size < 0) ++book->pool_unused; // (see book_pool_reclaim)
	memmove(a->positions + i, a->positions + i + 1, (size_t) (a->n - i - 1) * sizeof (Position));
	--a->n;
	--book->n_nodes;
	--book->stats.n_nodes;
}

/** @brief Remove a position from the book, by its place (see book_detach_at). */
static void book_remove_at(Book *book, PositionArray *a, const int i)
{
	position_free(a->positions + i);
	book_detach_at(book, a, i);
}

/**
 * @brief Remove the positions that are not marked "todo" (book reduce), each bucket in one pass.
 *
 * The positions kept keep their order, as with book_remove_at called for each position removed, but each one moves
 * once at most (book_remove_at moves the rest of the bucket for each position removed).
 *
 * @param book Opening book.
 */
static void book_remove_unmarked(Book *book)
{
	PositionArray *a;
	for (a = book->array; a < book->array + book->n; ++a) {
		int i, j = 0;
		for (i = 0; i < a->n; ++i) {
			if (position_is_todo(a->positions + i, book)) {
				if (j != i) a->positions[j] = a->positions[i];
				++j;
			} else position_free(a->positions + i);
		}
		if (j < a->n) {
			const int removed = a->n - j;
			if (a->size < 0) book->pool_unused += removed; // (see book_pool_reclaim)
			a->n = j;
			book->n_nodes -= (unsigned int) removed;
			book->stats.n_nodes -= removed;
		}
	}
}

/** @return the place of a position in the book: bucket << 32 | index in the bucket. */
static inline unsigned long long position_key(const Book *book, const Position *p)
{
	const unsigned long long i = board_get_hash_code(&p->board) & (book->n - 1);
	return (i << 32) | (unsigned long long) (p - book->array[i].positions);
}

/** @return the position at a place of the book (see position_key). */
static inline Position* book_position(const Book *book, const unsigned long long key)
{
	return book->array[key >> 32].positions + (int) (key & 0xffffffffu);
}

/**
 * @brief Add the place of a position to the list of the positions to expand.
 *
 * @param book Opening book.
 * @param key Place of the position (see position_key).
 * @return false if the memory is exhausted: the list is no longer valid (the whole book is scanned instead).
 */
static bool book_todo_push(Book *book, const unsigned long long key)
{
	if (book->todo_list.n == book->todo_list.size) {
		const long long size = book->todo_list.size + book->todo_list.size / 2 + 1024;
		unsigned long long *item = (unsigned long long*) realloc(book->todo_list.item, size * sizeof *item);
		if (item == NULL) { book->todo_list.valid = false; return false; }
		book->todo_list.item = item;
		book->todo_list.size = size;
	}
	book->todo_list.item[book->todo_list.n++] = key;
	return true;
}

/**
 * @brief Mark a position to be expanded and remember where it is.
 *
 * Positions never move inside their bucket while a book is being deviated
 * (new positions are appended), so book_expand() can visit the marked
 * positions in the original bucket order without scanning the whole book.
 *
 * @param book Opening book.
 * @param p Position (must belong to the book).
 */
static void book_mark_todo(Book *book, Position *p)
{
	position_set_todo(p, book);
	if (book->todo_list.valid) book_todo_push(book, position_key(book, p));
}

static int todo_item_cmp(const void *a, const void *b)
{
	const unsigned long long x = *(const unsigned long long*) a, y = *(const unsigned long long*) b;
	return (x > y) - (x < y);
}

#define KEY_LESS(a, b) ((a) < (b))
// (places of positions are numbers: any sort gives the same order)
DEFINE_SORT(keys_sort, unsigned long long, KEY_LESS, todo_item_cmp)

/** @brief Sort places (see position_key) in the order of the book, unless they already are (book_pick_todo sorts them). */
static void todo_sort(unsigned long long *item, const long long n)
{
	long long i;
	for (i = 1; i < n; ++i) if (item[i - 1] > item[i]) {
		keys_sort(item, n);
		return;
	}
}
/**
 * @brief Set all positions as undone.
 *
 * @param book Opening book.
 */
static void book_clean(Book *book)
{
	PositionArray *a;
	Position *p;
	book->stats.n_nodes = book->stats.n_links = book->stats.n_todo = 0;
	book->todo_list.n = 0;
	book->todo_list.valid = true;
	if (++book->epoch > POSITION_EPOCH_MAX) {
		foreach_position(p, a, book) p->state = 0;
		book->epoch = 1;
	}
}

/**
 * @brief Find the initial position in the book.
 *
 * Attention: when a position is added to the book, the pointer
 * returned by this position may be wrong. If the root position is updated
 * the contents of the pointed structure may be wrong. So it is needed to
 * recall this function each time as necessary.
 *
 * @param book Opening book.
 * @return the inital position.
 */
static Position *book_root(Book *book)
{
	Board board;

	board_init(&board);
	return book_probe(book, &board);
}

/**
 * @brief Seed of the random numbers of a book: book-seed if it is set, else the clock.
 *
 * @return the seed.
 */
static unsigned long long book_random_seed(void)
{
	return options.book_seed_set ? options.book_seed : (unsigned long long) real_clock();
}

/**
 * @brief Initialize the opening book.
 *
 * Create an empty opening book.
 *
 * @param book Opening book.
 */
void book_init(Book *book)
{
	int i;

	book_set_date(book);

	book->options.level = 21;
	book->options.n_empties = 24;
	book->options.midgame_error = 2;
	book->options.endcut_error = 1;
	book->options.verbosity = 0; // (saved with the book; the search set before is kept)

	book->stats.n_nodes = book->stats.n_links = book->stats.n_todo = 0;
	book->stack = NULL;

	book->n = 65536;
	book->array = (PositionArray*) malloc(book->n * sizeof *book->array);
	if (book->array == NULL) fatal_error("cannot allocate space to store the positions");
	for (i = 0; i < book->n; ++i) position_array_init(book->array + i);

	book->n_nodes = 0;
	book->failed = false;
	book->epoch = 1;
	book->todo_list.item = NULL;
	book->todo_list.n = book->todo_list.size = 0;
	book->todo_list.valid = false;
	book->pool = NULL;
	book->pool_size = 0;
	book->pool_unused = 0;
	book->visit = NULL;
	book->visit_first = NULL;
	random_seed(&book->random, book_random_seed());
	book->need_saving = false;
}

#ifndef BOOK_BUCKETS_MIN
#define BOOK_BUCKETS_MIN 65536  /**< the fewest buckets (the tests of book-reduce-shrink build with fewer, for small books) */
#endif

/**
 * @brief Number of buckets for a book of n positions.
 *
 * About 16 positions per bucket or less (65536 buckets at least). The upper
 * limit used to be 2^26 buckets (1.07 billion positions); books of any size
 * that the file format can hold now keep short buckets.
 *
 * @param n Number of positions.
 * @return Number of buckets (a power of 2).
 */
static int book_bucket_count(const long long n)
{
	int b = BOOK_BUCKETS_MIN;
	while (b < (1 << 28) && (long long) b * 16 < n) b <<= 1;
	return b;
}

/**
 * @brief Release the pool of a book (the positions stored at load time: see book_load).
 *
 * @param book Opening book.
 */
static void book_pool_free(Book *book)
{
	free(book->pool);
	book->pool = NULL;
	book->pool_size = 0;
	book->pool_unused = 0;
}

/**
 * @brief Give a book new buckets, that hold all its positions (book_grow_buckets, book_shrink_buckets).
 *
 * The arrays of the former buckets and the pool are released (the links are not: they moved with the positions).
 *
 * @param book Opening book.
 * @param array New buckets.
 * @param n Number of new buckets.
 */
static void book_set_buckets(Book *book, PositionArray *array, const int n)
{
	PositionArray *a;

	for (a = book->array; a < book->array + book->n; ++a) if (a->size >= 0) free(a->positions);
	free(book->array);
	book_pool_free(book);
	book->array = array;
	book->n = n;
	book->todo_list.valid = false; // positions moved
	book->todo_list.n = 0;
}

/**
 * @brief Increase the number of buckets of a book.
 *
 * Used before adding many positions to a book with too few buckets (e.g. a
 * new book merged with a large one): every book search scans a bucket, so the
 * time grows with the number of positions per bucket. Positions keep their
 * relative order (bucket order, then position order).
 *
 * @param book Opening book.
 * @param n_positions Expected number of positions.
 */
static void book_grow_buckets(Book *book, const long long n_positions)
{
	const int n = book_bucket_count(n_positions);
	PositionArray *array, *a;
	Position *p;
	int i;

	if (n_positions <= (long long) book->n * 32 || n <= book->n) return; // not worth moving the book
	array = (PositionArray*) malloc(n * sizeof *array);
	if (array == NULL) return; // keep the current buckets
	for (i = 0; i < n; ++i) position_array_init(array + i);
	foreach_position(p, a, book) {
		PositionArray *b = array + (board_get_hash_code(&p->board) & (n - 1));
		if (b->n == b->size) {
			const int size = b->size + b->size / 2 + 1;
			Position *q = (Position*) realloc(b->positions, size * sizeof (Position));
			if (q == NULL) { // keep the current buckets
				for (i = 0; i < n; ++i) free(array[i].positions);
				free(array);
				return;
			}
			b->positions = q; b->size = size;
		}
		b->positions[b->n++] = *p; // the links move with the position
	}
	book_set_buckets(book, array, n);
}

/**
 * @brief Decrease the number of buckets of a book from which most positions were removed (book reduce).
 *
 * The book gets the buckets of a book of its size when it is loaded (book_bucket_count): each bucket its own array
 * of the exact size, and the pool and the arrays of the removed positions are given back. The positions and their
 * links are the same; the book is saved in the order of the new buckets (as after loading the saved book again).
 *
 * @param book Opening book.
 * @return true if the buckets changed.
 */
static bool book_shrink_buckets(Book *book)
{
	const int n = book_bucket_count(book->n_nodes);
	PositionArray *array, *a;
	Position *p;
	int i;

	if ((long long) n * 4 > book->n) return false; // (not worth moving the book)
	array = (PositionArray*) malloc(n * sizeof *array);
	if (array == NULL) return false; // keep the current buckets
	for (i = 0; i < n; ++i) position_array_init(array + i);
	foreach_position(p, a, book) ++array[board_get_hash_code(&p->board) & (n - 1)].size; // (the size of each array first)
	for (i = 0; i < n; ++i) {
		if (array[i].size && (array[i].positions = (Position*) malloc(array[i].size * sizeof (Position))) == NULL) {
			while (i > 0) free(array[--i].positions);
			free(array);
			return false;
		}
	}
	foreach_position(p, a, book) {
		PositionArray *b = array + (board_get_hash_code(&p->board) & (n - 1));
		b->positions[b->n++] = *p; // the links move with the position
	}
	book_set_buckets(book, array, n);
	return true;
}

/**
 * @brief Free resources used by the opening book.
 *
 * @param book Opening book.
 */
void book_free(Book *book)
{
	int i;
	for (i = 0; i < book->n; ++i) {
		position_array_free(book->array + i);
	}
	free(book->array);
	book_pool_free(book);
	free(book->todo_list.item);
	book->todo_list.item = NULL;
	book->todo_list.n = book->todo_list.size = 0;
	book->todo_list.valid = false;
	book_store_release();
}

/**
 * @brief Create a new opening book.
 *
 * Create an opening book with the initial position & a single non link move.
 *
 * @param book Opening book.
 * @param level search level to evaluate positions.
 * @param n_empties number of empty positions up to which to evaluate positions.
 */
void book_new(Book *book, int level, int n_empties)
{
	Board board;

	bprint("New book %d %d...", level, n_empties);
	book_init(book);
	book->options.level = level;
	book->options.n_empties = n_empties;

	board_init(&board);
	book_add_board(book, &board);
	bprint("...done>\n");
	book->need_saving = true;
}

/**
 * @brief Number of bytes left in a file.
 *
 * @param f File (seekable).
 * @return bytes from the current position to the end, -1 on error.
 */
static long long book_file_remaining(FILE *f)
{
	long long pos, end;
#ifdef _WIN32
	pos = _ftelli64(f);
	if (pos < 0 || _fseeki64(f, 0, SEEK_END) != 0) return -1;
	end = _ftelli64(f);
	if (_fseeki64(f, pos, SEEK_SET) != 0) return -1;
#else
	pos = (long long) ftello(f);
	if (pos < 0 || fseeko(f, 0, SEEK_END) != 0) return -1;
	end = (long long) ftello(f);
	if (fseeko(f, (off_t) pos, SEEK_SET) != 0) return -1;
#endif
	return end - pos;
}

/**
 * Book files that exist but could not be loaded. Edax then works with a new book, which does not hold
 * their positions:
 * - a file that could not be read as a book (damaged, truncated, another version): when the book is saved
 *   to it, the file is kept under another name (<file>.damaged) instead of being replaced;
 * - a file that could not be opened (held by another program without sharing, no permission...), even after
 *   waiting a little (see book_open): it is not damaged, and no save replaces it (up to v4.5.5-nikque.26 it
 *   was taken for a damaged file: a good book held by an antivirus at the start went to <file>.damaged, and
 *   the new book replaced it).
 * A file keeps its mark until it is loaded or kept aside: up to v4.5.5-nikque.26 only the last file was
 * marked, and the book file that could not be loaded at the start was replaced (without being kept) once
 * another file could not be loaded either ("book load").
 */
typedef struct BookUnreadMark {
	char *file;     /**< name of the file */
	bool unopened;  /**< the file could not be opened (else: it could not be read as a book) */
} BookUnreadMark;

static struct {
	BookUnreadMark *mark;  /**< the files */
	int n;                 /**< number of files */
} book_unread = {NULL, 0};
static bool book_unread_untracked = false;

/**
 * @return the index of the mark of a file, or -1 if it has none. A name written another way is of the
 * same file (see file_same: up to v4.5.5-nikque.26 the names were compared, and the file was replaced
 * without being kept when it was named another way).
 */
static int book_unread_find(const char *file)
{
	int i;

	if (file) for (i = 0; i < book_unread.n; ++i) if (file_same(file, book_unread.mark[i].file)) return i;
	return -1;
}

/**
 * @brief Mark a file that could not be loaded.
 * @param file File name.
 * @param unopened The file could not be opened (else: it could not be read as a book).
 */
static void book_unread_add(const char *file, const bool unopened)
{
	BookUnreadMark *marks;
	char *name;
	int i;

	if (file == NULL) return;
	if ((i = book_unread_find(file)) >= 0) { book_unread.mark[i].unopened = unopened; return; }
	marks = (BookUnreadMark*) realloc(book_unread.mark, (book_unread.n + 1) * sizeof *marks);
	if (marks) book_unread.mark = marks;
	name = marks ? string_duplicate(file) : NULL;
	if (name == NULL) {
		book_unread_untracked = true;
		error("cannot allocate the mark of %s, which could not be loaded\n", file);
		return;
	}
	book_unread.mark[book_unread.n].file = name;
	book_unread.mark[book_unread.n++].unopened = unopened;
}

/**
 * @brief Remove a mark: the file was loaded, or it is kept under another name.
 *
 * @param i Index of the mark (see book_unread_find), found before the file is renamed: a file named
 * another way than its mark is found by the file itself, which no longer has its name once kept aside.
 */
static void book_unread_remove(const int i)
{
	if (0 <= i && i < book_unread.n) {
		free(book_unread.mark[i].file);
		book_unread.mark[i] = book_unread.mark[--book_unread.n];
	}
}

/**
 * @brief Forget the marks of the files that could not be loaded (the book is freed: libedax_terminate).
 */
void book_unread_forget(void)
{
	while (book_unread.n > 0) free(book_unread.mark[--book_unread.n].file);
	book_unread_untracked = false;
}

/**
 * @brief What became of a book file that was to be loaded.
 *
 * @param file File name.
 * @return BOOK_FILE_REPLACEABLE (it was loaded, or no load of it failed), BOOK_FILE_DAMAGED (it could not
 *         be read as a book: a save to it keeps it as <file>.damaged first) or BOOK_FILE_UNOPENED (it could
 *         not be opened: no save replaces it).
 */
int book_file_status(const char *file)
{
	const int i = book_unread_find(file);

	if (book_unread_untracked) return BOOK_FILE_UNOPENED;
	if (i < 0) return BOOK_FILE_REPLACEABLE;
	return book_unread.mark[i].unopened ? BOOK_FILE_UNOPENED : BOOK_FILE_DAMAGED;
}

/** @brief Leave a book without any position after a load that failed (book_free() can be called on it). */
static void book_set_unloaded(Book *book)
{
	book->array = NULL;
	book->n = book->n_nodes = 0;
	book->pool = NULL;
	book->pool_size = 0;
	book->pool_unused = 0;
	book->todo_list.item = NULL;
	book->todo_list.n = book->todo_list.size = 0;
	book->todo_list.valid = false;
	book->need_saving = false; // never overwrite a damaged source automatically
}

/**
 * Book file that still holds the book that was in use before "book new" (or edax_book_new) made a new one
 * in memory. When the new book is saved to that file without the file being named (when Edax ends, or by
 * "book save" without a file name), the file is kept under another name ("<file>.old") instead of being
 * replaced. Up to v4.5.5-nikque.12 "book new" then "quit" left a book of one position in its place.
 * "book save <file>", with the name, replaces the file as asked.
 */
static char *book_replaced_file = NULL;
static bool book_replaced_untracked = false;

/** The command after which the book in memory no longer is the book of that file (for the message). */
static const char *book_replaced_by = "book new";

/**
 * @brief Tell that the book in memory was made new, and no longer is the book of this file.
 *
 * @param file Book file name (NULL: no such file).
 */
void book_set_replaced_file(const char *file)
{
	char *name = file ? string_duplicate(file) : NULL;
	book_replaced_untracked = file && !name;
	if (book_replaced_untracked) {
		error("cannot remember the previous book file %s; automatic saves are blocked\n", file);
		return;
	}
	free(book_replaced_file);
	book_replaced_file = name;
	book_replaced_by = "book new";
}

/**
 * @brief Tell that the book in memory now comes from another file than the book file of the settings
 * ("book load <file>", "book import <file>").
 *
 * As after "book new", the book file still holds the previous book: a save that does not name it keeps
 * it under another name (up to v4.5.5-nikque.12 it was replaced when Edax ended after a change).
 * A file that a book command made from the book file itself (<book file>.dev2, .store, .mrg, ...) is
 * the same book going on: loading it leaves the book file to be replaced as before. This is not so
 * for the files kept aside (<book file>.old, .damaged).
 *
 * @param file File that was loaded or imported.
 * @param imported true for "book import", false for "book load".
 */
void book_set_loaded_file(const char *file, const bool imported)
{
	const char *book_file = options.book_file;
	size_t n;

	if (file == NULL || book_file == NULL || strcmp(file, book_file) == 0) return; // (the book file itself: book_load() has cleared the mark)
	n = strlen(book_file);
	if (!imported && strncmp(file, book_file, n) == 0 && file[n] == '.') {
		static const char *const extensions[] = {".store", ".dev", ".dev2", ".dev3", ".enh", ".mrg", ".play",
			".gam", ".ext", ".exh", ".fill", ".fill2", ".lexp", ".err", ".dep", ".leaf", ".leaf2", ".leaf3", ".leaf4"};
		for (size_t i = 0; i < sizeof extensions / sizeof *extensions; ++i) {
			if (strcmp(file + n, extensions[i]) == 0) {
				book_set_replaced_file(NULL);
				return;
			}
		}
	}
	book_set_replaced_file(book_file);
	book_replaced_by = imported ? "book import" : "book load";
}

/**
 * @brief Number of positions that the header of a book file tells.
 *
 * @param file File name.
 * @return the number of positions, or -1 if the file cannot be read as a book of this version.
 */
static long long book_file_positions(const char *file)
{
	unsigned int header_edax, header_book, n;
	unsigned char header_version, header_release;
	long long count = -1;
	FILE *f = fopen(file, "rb");

	if (f) {
		if (fread(&header_edax, sizeof header_edax, 1, f) == 1 && fread(&header_book, sizeof header_book, 1, f) == 1
		 && fread(&header_version, 1, 1, f) == 1 && fread(&header_release, 1, 1, f) == 1
		 && header_edax == EDAX && header_book == BOOK && header_version == VERSION
		 && fseek(f, (long) (sizeof ((Book*) NULL)->date + sizeof ((Book*) NULL)->options), SEEK_CUR) == 0
		 && fread(&n, sizeof n, 1, f) == 1) count = n;
		fclose(f);
	}
	return count;
}

/**
 * @brief Keep a file under another name ("<file><ext>", then "<file><ext>.1", ...) instead of replacing it.
 *
 * @param file File name.
 * @param ext Extension added to the name (".damaged": a file that could not be loaded; ".old": the book in use before "book new").
 * @param reason Why the file is kept (for the message).
 * @return true if the file was renamed or does not exist any more.
 */
static bool book_rename_back(const char *from, const char *to)
{
#ifdef _WIN32
	if (MoveFileExA(from, to, 0)) return true;
	const DWORD e = GetLastError();
	errno = e == ERROR_ALREADY_EXISTS || e == ERROR_FILE_EXISTS ? EEXIST : EIO;
	return false;
#else
#if defined(__linux__) && !defined(__ANDROID__) && defined(SYS_renameat2)
	// RENAME_NOREPLACE is the Linux kernel flag with value 1.
	if (syscall(SYS_renameat2, AT_FDCWD, from, AT_FDCWD, to, 1U) == 0) return true;
	if (errno != ENOSYS && errno != EINVAL && errno != ENOTSUP && errno != EOPNOTSUPP) return false;
#elif defined(__APPLE__)
	if (renamex_np(from, to, RENAME_EXCL) == 0) return true;
	if (errno != ENOSYS && errno != EINVAL && errno != ENOTSUP && errno != EOPNOTSUPP) return false;
#endif
	// Prefer an exclusive hard link where supported. If unlink fails, leave both names.
	if (link(from, to) == 0) return unlink(from) == 0;
	if (errno != ENOSYS && errno != EINVAL && errno != ENOTSUP && errno != EOPNOTSUPP
		&& errno != EPERM && errno != EXDEV && errno != EMLINK) return false;

	// Filesystems without either exclusive operation use a checked rename.
	struct stat destination;
	if (lstat(to, &destination) == 0) { errno = EEXIST; return false; }
	if (errno != ENOENT) return false;
	return rename(from, to) == 0;
#endif
}

static bool book_set_aside(const char *file, const char *ext, const char *reason, char **kept)
{
	char *name = (char*) malloc(strlen(file) + 32);
	FILE *f;
	bool ok = false;
	int i;

	if (name == NULL) return false;
	errno = 0;
	if ((f = fopen(file, "rb")) == NULL) { // nothing to keep, or not a file that can be read (a directory, a locked file): left as it is
		free(name);
		return errno == ENOENT;
	}
	fclose(f);
	for (i = 0; i < 100 && !ok; ++i) {
		if (i) sprintf(name, "%s%s.%d", file, ext, i); else sprintf(name, "%s%s", file, ext);
		if (book_rename_back(file, name)) {
			warn("%s %s: it is kept as %s\n", file, reason, name);
			ok = true;
		} else if (errno != EEXIST) break;
	}
	if (ok && kept) *kept = name; // (the caller frees it)
	else free(name);
	return ok;
}

/**
 * @brief Keep the book file that could not be loaded under another name now, before a book
 * saved to another file replaces it.
 *
 * book_save() does it when the book is saved to that file. A program that saves the book to
 * another file and then renames it to the book file (edax_runner: book.dat.saving, then book.dat)
 * calls this before the rename: else the file that could not be loaded is replaced and lost.
 *
 * @param file Book file to be replaced.
 * @return true if the file can be replaced (it was loaded, or it is now kept as <file>.damaged);
 *         false if it could not be opened, or if it could not be loaded and could not be kept: it must
 *         not be replaced.
 */
bool book_keep_unread_file(const char *file)
{
	const int i = book_unread_find(file);

	if (book_unread_untracked) {
		error("cannot replace %s: protection for an unreadable book could not be recorded\n", file);
		return false;
	}
	if (i < 0) return true;
	if (book_unread.mark[i].unopened) {
		errno = 0;
		error("\n%s could not be opened when the book was loaded (it may be used by another program); it must not be replaced\n", file);
		return false;
	}
	if (!book_set_aside(file, ".damaged", "could not be loaded", NULL)) {
		error("\nCannot keep the book that could not be loaded; %s must not be replaced\n", file);
		return false;
	}
	book_unread_remove(i);
	return true;
}

/**
 * @brief Release the end of the pool: it holds n positions from its start (the rest is no longer used).
 *
 * The block is made smaller in place with MSVC; elsewhere realloc() may move it, and the buckets
 * of the pool then point to the same places of the new block.
 *
 * @param book opening book.
 * @param n Number of positions kept at the start of the pool.
 */
static void book_pool_shrink(Book *book, const long long n)
{
	if (n == 0) {
		book_pool_free(book);
	} else if (n < book->pool_size) {
#if defined(_MSC_VER)
		if (_expand(book->pool, (size_t) n * sizeof (Position)) != NULL) book->pool_size = n;
#else
		const uintptr_t old = (uintptr_t) book->pool;
		Position *pool = (Position*) realloc(book->pool, (size_t) n * sizeof (Position));
		if (pool != NULL) {
			if ((uintptr_t) pool != old) {
				PositionArray *a;
				for (a = book->array; a < book->array + book->n; ++a) {
					if (a->size < 0 && a->n > 0) a->positions = pool + ((uintptr_t) a->positions - old) / sizeof (Position);
				}
			}
			book->pool = pool;
			book->pool_size = n;
		}
#endif
	}
}

/** @brief Go back to a place of a book file, to read it again from there. @return false on failure. */
static bool book_stream_seek(BookStream *s, const long long place)
{
	s->n = s->pos = 0;
#ifdef _WIN32
	return _fseeki64(s->f, place, SEEK_SET) == 0;
#else
	return fseeko(s->f, (off_t) place, SEEK_SET) == 0;
#endif
}

/**
 * @brief Load the positions of a book file saved in another bucket order (book_load): a book whose buckets
 * were more (after book subtree or book prune) or fewer (a book that grew) when it was saved.
 *
 * What was read is released, the positions of each bucket are counted, then the positions are read again
 * into the places of their buckets in the pool, in the order of the file: the book is the same as with
 * the positions added one after the other, in one block (the memory of a book saved in this bucket order).
 *
 * @param loaded Book being loaded (its buckets and its pool of n positions).
 * @param s Input stream.
 * @param start Place of the first position in the file.
 * @param file File name (for the messages).
 * @param n Number of positions of the file.
 * @return false if the file cannot be read (a message was printed).
 */
static bool book_load_by_bucket(Book *loaded, BookStream *s, const long long start, const char *file, const unsigned int n)
{
	Position *pool = loaded->pool, p;
	PositionArray *a;
	unsigned int i, used = 0;

	for (a = loaded->array; a < loaded->array + loaded->n; ++a) {
		position_array_free(a);
		position_array_init(a);
	}
	loaded->n_nodes = 0;
	loaded->stats.n_nodes = 0;

	// the number of positions of each bucket (in size)
	if (!book_stream_seek(s, start)) { error("Cannot read %s again\n", file); return false; }
	for (i = 0; i < n; ++i) {
		if (!position_read(&p, s)) {
			error("Truncated or damaged opening book %s at position %u/%u\n", file, i, n);
			return false;
		}
		++book_array(loaded, &p.board)->size;
		position_free(&p);
	}
	for (a = loaded->array; a < loaded->array + loaded->n; ++a) {
		if (a->size > 0) {
			a->positions = pool + used;
			used += (unsigned int) a->size;
			a->size = -a->size; // (in the pool: -1 once the positions are read; until then, minus the places of the bucket)
		}
	}

	// the positions, in their places
	if (!book_stream_seek(s, start)) { error("Cannot read %s again\n", file); return false; }
	for (i = 0; i < n; ++i) {
		if (!position_read(&p, s)) {
			error("Truncated or damaged opening book %s at position %u/%u\n", file, i, n);
			return false;
		}
		a = book_array(loaded, &p.board);
		if (a->n == -a->size) { // not what was counted: the file changed
			position_free(&p);
			error("%s changed while it was read\n", file);
			return false;
		}
		a->positions[a->n] = p;
		a->positions[a->n].state = loaded->epoch | POSITION_DONE;
		++a->n;
	}

	// duplicated positions (looked for once all are in place, bucket after bucket): the later ones are left
	// out, and the count check of book_load fails
	for (a = loaded->array; a < loaded->array + loaded->n; ++a) {
		if (a->size < 0) a->size = -1;
		for (int k = 1; k < a->n; ++k) {
			int j;
			for (j = 0; j < k && !board_equal(&a->positions[j].board, &a->positions[k].board); ++j) ;
			if (j < k) {
				position_free(a->positions + k);
				memmove(a->positions + k, a->positions + k + 1, (size_t) (a->n - k - 1) * sizeof (Position));
				--a->n; --k;
			}
		}
		loaded->n_nodes += (unsigned int) a->n;
	}
	loaded->stats.n_nodes = loaded->n_nodes;
	return true;
}

/** Number of times a book file that exists but cannot be opened is tried again, every 100 ms (for 2 seconds). */
#define BOOK_OPEN_RETRIES 20

/**
 * @brief Open a book file to load it, waiting a little when it exists but cannot be opened.
 *
 * Another program (an antivirus, a backup, a copy...) may hold the file for a while without sharing it.
 *
 * @param file File name.
 * @return the file, or NULL (errno: the error of the last try; ENOENT: no such file).
 */
static FILE* book_open(const char *file)
{
	FILE *f;
	int i;

	for (i = 0; ; ++i) {
		errno = 0;
		f = fopen(file, "rb");
		if (f != NULL || errno == ENOENT || i == BOOK_OPEN_RETRIES) return f;
		relax(100);
	}
}

/**
 * @brief Load the opening book.
 *
 * @param book Opening book.
 * @param file File name.
 * @return true if the book of the file was loaded; false if the file does not exist (a new book is made)
 *         or cannot be loaded (the book is left without any position).
 */
bool book_load(Book *book, const char *file)
{
	FILE *f;

	f = book_open(file);
	if (f == NULL && errno != ENOENT) { // the file exists (or may exist) but cannot be opened: not a new book, nor a damaged one
		error("Cannot open opening book %s; it is not loaded, and no save replaces it (it may be used by another program)\n", file);
		book_unread_add(file, true);
		book_set_unloaded(book);
		return false;
	}
	if (f) {
		Book loaded = {0};
		BookStream stream = {0};
		Position p, *pool = NULL;
		unsigned int used = 0;
		int last_bucket = -1;
		bool pooling;
		unsigned int header_edax, header_book;
		unsigned char header_version, header_release;
		unsigned int i, expected;	// the position count is saved as a 32-bit unsigned int
		long long start; // the place of the first position
		int r, j;
		loaded.search = book->search;
		loaded.epoch = 1;

		info("Loading book from %s...", file);
		r = fread(&header_edax, sizeof (unsigned int), 1, f);
		r += fread(&header_book, sizeof (unsigned int), 1, f);
		if (r != 2 || header_edax != EDAX || header_book != BOOK) {
			error("%s is not an edax opening book\n", file);
			goto book_load_failed;
		}

		r = fread(&header_version, 1, 1, f);
		r += fread(&header_release, 1, 1, f);
		if (r != 2 || header_version != VERSION) {
			error("%s is not a compatible version\n", file);
			goto book_load_failed;
		}

		r = fread(&loaded.date, sizeof loaded.date, 1, f);
		r += fread(&loaded.options, sizeof loaded.options, 1, f);
		r += fread(&expected, sizeof expected, 1, f);
		if (r != 3) {
			error("Cannot read book settings from %s\n", file);
			goto book_load_failed;
		}
		BOUND(loaded.options.level, 0, 60, "book level"); // damaged settings: the level is an index of LEVEL[][]
		BOUND(loaded.options.n_empties, 0, 60, "book depth (empties)"); // (a game holds 60 moves: see book_check_game)
		{	// every position takes at least POSITION_FIXED_SIZE bytes and its leaf: reject a count the file cannot hold
			const long long remaining = book_file_remaining(f);
			if (remaining < 0 || (long long) expected > remaining / (long long) (POSITION_FIXED_SIZE + sizeof (Link))) {
				error("Invalid position count in %s\n", file);
				goto book_load_failed;
			}
		}

#ifdef _WIN32
		start = _ftelli64(f);
#else
		start = (long long) ftello(f);
#endif
		loaded.n = book_bucket_count(expected);

		loaded.array = (PositionArray*) malloc(loaded.n * sizeof (PositionArray));
		if (loaded.array == NULL) {
			error("cannot allocate space to store the positions\n");
			goto book_load_failed;
		}
		for (j = 0; j < loaded.n; ++j) position_array_init(loaded.array + j);

		if (!book_stream_open(&stream, f)) {
			error("cannot allocate the book read buffer\n");
			goto book_load_failed;
		}
		// A saved book lists its positions bucket by bucket: store them contiguously
		// in one pool, exactly sized (no per-bucket block, no spare capacity).
		// Positions out of bucket order (a book saved with other buckets) are read again: see book_load_by_bucket.
		// Without a pool, the positions go to the usual growing arrays.
		// (32-bit builds: the size of the pool must fit a size_t, or a smaller block would be allocated)
		pool = (expected > 0 && (unsigned long long) expected * sizeof (Position) <= SIZE_MAX) ? (Position*) malloc((size_t) expected * sizeof (Position)) : NULL;
		loaded.pool = pool;
		loaded.pool_size = pool ? expected : 0;
		pooling = (pool != NULL);
		for (i = 0; i < expected; ++i) {
			if (!position_read(&p, &stream)) {
				error("Truncated or damaged opening book %s at position %u/%u\n", file, i, expected);
				goto book_load_failed;
			}
			if (pooling) {
				const int b = (int) (board_get_hash_code(&p.board) & (loaded.n - 1));
				PositionArray *a = loaded.array + b;
				if (b < last_bucket) { // not saved in this bucket order: read again, bucket by bucket
					position_free(&p);
					if (start < 0 || !book_load_by_bucket(&loaded, &stream, start, file, expected)) goto book_load_failed;
					used = expected;
					break;
				}
				if (a->n == 0 || (a->size < 0 && a->positions + a->n == pool + used)) {
					int k;
					for (k = 0; k < a->n; ++k) if (board_equal(&a->positions[k].board, &p.board)) break;
					if (k == a->n) {
						if (a->n == 0) { a->positions = pool + used; a->size = -1; }
						pool[used] = p;
						pool[used].state = loaded.epoch | POSITION_DONE;
						++used; ++a->n; ++loaded.n_nodes; ++loaded.stats.n_nodes;
					} else {
						position_free(&p); // duplicated position: the count check below fails
					}
					last_bucket = b;
					continue;
				}
			}
			if (book_add(&loaded, &p) <= 0) position_free(&p); // duplicated position: the count check below fails
			if (loaded.failed) { // out of memory: stop here (every other position would fail too)
				error("Not enough memory to load %s (position %u/%u)\n", file, i, expected);
				goto book_load_failed;
			}
		}

		if (pool && used < expected) { // release the unused part of the pool
			book_pool_shrink(&loaded, used);
			pool = loaded.pool;
		}
		if (ferror(f) || !book_stream_at_end(&stream) || loaded.n_nodes != expected) {
			error("Invalid opening book size or position count in %s\n", file);
			goto book_load_failed;
		}
		book_stream_close(&stream);

		random_seed(&loaded.random, book_random_seed());
		loaded.need_saving = false;

		info("done\n");
		fclose(f);
		book_unread_remove(book_unread_find(file));
		if (book_replaced_file && strcmp(file, book_replaced_file) == 0) book_set_replaced_file(NULL); // (the book in memory is the one of the file again)
		*book = loaded;
		return true;

book_load_failed:
		book_stream_close(&stream);
		fclose(f);
		book_unread_add(file, false); // a save to this file keeps it under another name (see book_save)
		if (loaded.array) book_free(&loaded);
		book_set_unloaded(book);
		return false;
	} else {
		book_new(book, options.level, 60 - get_book_depth(options.level));
		return false;
	}
}

/**
 * @brief Load the book file of the settings when a user interface starts.
 *
 * - no such file: a new book, saved when Edax ends;
 * - a file that cannot be loaded: a new book too, but it is only saved if it changes (and the
 *   file is then kept under another name: see book_save);
 * - a file that cannot be opened, even after waiting a little (held by another program...): a new
 *   book too, which is never saved to that file (see book_unread, book_file_status);
 * - book-depth = n: the book depth is set as with the "book depth" command (and saved with the
 *   book); auto (0) keeps the depth saved in the file.
 *
 * @param book Opening book.
 */
void book_load_at_startup(Book *book)
{
	if (!book_load(book, options.book_file) && book->array == NULL) {
		book_new(book, options.level, 60 - get_book_depth(options.level));
		book->need_saving = false;
	}
	if (options.book_depth > 0) book->options.n_empties = 61 - options.book_depth;
}

/**
 * @brief Import an opening book.
 *
 * Read the opening book from a portable text format.
 * After the book is imported, it is needed to
 * relink & negamax it.
 *
 * @param book Opening book (not initialized; left as it is when nothing is imported).
 * @param file File name.
 * @return true if a book was imported; false if the file cannot be opened or holds no position.
 */
bool book_import(Book *book, const char *file)
{
	FILE *f = fopen(file, "r");
	if (f) {
		PositionArray *a;
		Position *p, position;
		int n_empties, r, depth = 0;
		long long n_wrong = 0;
		Search *const search = book->search;

		book_init(book);
		// a line that holds no position is skipped (the first ones are explained), not the rest of the file
		while ((r = position_import(&position, f, n_wrong < 10, &depth)) >= 0) {
			if (r == 0) { ++n_wrong; continue; }
			position_unique(&position); // (a board written in another orientation: in the bucket of its unique board, where it is looked for)
			if (book_add(book, &position) < 0) break; // (memory exhausted: book->failed)
			// more buckets as the book grows (each search scans a bucket): as many as book_load() makes
			// (once at each step: if the memory is exhausted, the buckets are kept)
			if ((long long) book->n_nodes == (long long) book->n * 16 + 1) book_grow_buckets(book, 2LL * book->n_nodes);
			if (book->n_nodes % BOOK_INFO_RESOLUTION == 0) bprint("importing book from %s... %u positions\r", file, book->n_nodes);
		}
		bprint("importing book from %s... %u positions", file, book->n_nodes);
		if (n_wrong) warn("%lld lines of %s hold no position: skipped\n", n_wrong, file);
		if (book->n_nodes == 0 || book->failed || ferror(f)) { // a part of the file only is not imported: the current book is kept
			bprint("\n");
			if (book->failed) error("\"%s\" was not imported (see above)\n", file); // (memory exhausted, or too many positions)
			else if (ferror(f)) error("cannot read \"%s\"\n", file);
			else error("no position found in \"%s\"\n", file);
			fclose(f);
			book_free(book);
			memset(book, 0, sizeof *book);
			book->search = search;
			return false;
		}

		book->options.n_empties = 60;
		book->options.level = 0;
		foreach_position(p, a, book) {
			n_empties = board_count_empties(&p->board);
			if (p->level > book->options.level) book->options.level = p->level;
			if (n_empties < book->options.n_empties) book->options.n_empties = n_empties;
		}
		// The depth of the book: the one that the file tells ("% depth <n>"). Without it, from the positions: the
		// deepest ones are those that a book holds one move beyond its depth (with a leaf only), so the book ends
		// one move before them. (Up to v4.5.5-nikque.12 the depth was the one of the deepest positions themselves:
		// a book exported then imported was one move deeper than before.)
		if (depth > 0) book->options.n_empties = 61 - depth;
		else if (book->options.n_empties < 60) ++book->options.n_empties;

		random_seed(&book->random, book_random_seed());
		book->need_saving = true;

		bprint("...done\n");
		fclose(f);
		return true;
	} else {
		error("cannot open \"%s\" to import the opening book\n", file);
		return false;
	}
}

/**
 * @brief Open a text file to write (book export, the files of the analyses) under another name in the same folder:
 * book_text_commit then puts it in the place of the file. A file that cannot be written to its end leaves the former
 * file as it was (up to v4.5.5-nikque.33 the file was written in its place, and left half written).
 *
 * @param file File to write.
 * @param tmp Name of the file written (out; given to book_text_commit).
 * @return the file written, NULL if it cannot be opened.
 */
FILE* book_text_create(const char *file, char **tmp)
{
	FILE *f;

	*tmp = (char*) malloc(strlen(file) + 32);
	if (*tmp == NULL) return NULL;
#ifdef _WIN32
	sprintf(*tmp, "%s.saving.%lu", file, (unsigned long) GetCurrentProcessId());
#else
	sprintf(*tmp, "%s.saving.%lu", file, (unsigned long) getpid());
#endif
	f = fopen(*tmp, "w");
	if (f == NULL) {
		free(*tmp);
		*tmp = NULL;
	}
	return f;
}

/**
 * @brief Close a file opened by book_text_create and, if it was written to its end, put it in the place of the file.
 *
 * @param f File written.
 * @param tmp Its name (freed).
 * @param file File to replace.
 * @param ok false if the file is not complete (stopped, no memory): it is removed, the former file is kept.
 * @return true if the file was replaced.
 */
bool book_text_commit(FILE *f, char *tmp, const char *file, bool ok)
{
	if (ferror(f) != 0) ok = false; // (before fclose: the stream is no longer valid after it)
	if (fclose(f) != 0) ok = false;
	if (ok && !file_replace(tmp, file)) ok = false;
	if (!ok) remove(tmp);
	free(tmp);
	return ok;
}

/**
 * @brief Export an opening book.
 *
 * Save the book in a portable text format.
 *
 * @param book Opening book.
 * @param file File name.
 */
void book_export(Book *book, const char *file)
{
	FILE *f;
	PositionArray *a;
	Position *p;
	char *tmp;
	bool ok = false;

	f = book_text_create(file, &tmp);
	if (f == NULL) {
		error("cannot open file %s", file);
		return;
	}
	
	info("Exporting book to %s...", file);
	foreach_position(p, a, book) {
		if (!position_export(p, f)) {
			error("cannot export book to %s", file);
			goto book_export_end;
		}
	}
	// the depth of the book, that the positions do not tell (read by book_import(); at the end of the file: a
	// program that stops reading at the first line that is not a position has read them all)
	if (fprintf(f, "%% depth %d\n", 61 - book->options.n_empties) < 0) {
		error("cannot export book to %s", file);
		goto book_export_end;
	}
	ok = true;

book_export_end:
	if (book_text_commit(f, tmp, file, ok)) info("done\n");
	else if (ok) error("cannot export book to %s", file); // (written, but not closed or not renamed)
}

/**
 * @brief Remove the temporary files left by an Edax that was killed while it saved this book.
 *
 * book_save() writes "<file>.tmp.<process id>" then renames it. A file with the id of a process
 * that does not run any more is removed (Windows program only; a library is used by programs that
 * clean up themselves, e.g. edax_runner).
 *
 * @param file Book file name.
 */
static void book_remove_stale_tmp(const char *file)
{
#if defined(_WIN32) && !defined(LIB_BUILD)
	const size_t n_file = strlen(file);
	const char *name = file + n_file;
	char *path = (char*) malloc(n_file + MAX_PATH + 8);
	WIN32_FIND_DATAA found;
	HANDLE search;

	if (path == NULL) return;
	while (name > file && name[-1] != '/' && name[-1] != '\\' && name[-1] != ':') --name; // the file name without its folder
	sprintf(path, "%s.tmp.*", file);
	if ((search = FindFirstFileA(path, &found)) != INVALID_HANDLE_VALUE) {
		do {
			const char *id = found.cFileName + strlen(name) + 5; // after "<name>.tmp."
			char *end;
			unsigned long pid;
			HANDLE process;
			DWORD code = 0;
			bool running;

			if (strlen(found.cFileName) <= strlen(name) + 5) continue;
			pid = strtoul(id, &end, 10);
			if (end == id || *end != '\0' || pid == 0 || pid == GetCurrentProcessId()) continue;
			process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
			if (process) {
				running = (GetExitCodeProcess(process, &code) && code == STILL_ACTIVE);
				CloseHandle(process);
			} else {
				running = (GetLastError() != ERROR_INVALID_PARAMETER); // no such process, or one that cannot be asked (kept)
			}
			if (running) continue;
			sprintf(path, "%.*s%s", (int) (name - file), file, found.cFileName);
			if (remove(path) == 0) warn("%s was left by an Edax that was stopped while saving: removed\n", path);
		} while (FindNextFileA(search, &found));
		FindClose(search);
	}
	free(path);
#else
	(void) file;
#endif
}

/**
 * @brief Save an opening book.
 *
 * Save the book in a fast binary format.
 *
 * @param book Opening book.
 * @param file File name.
 * @param keep_replaced Keep, under another name, a file that still holds the book in use before "book new" (see book_replaced_file).
 * @return true if the book was saved.
 */
static bool book_save_to(Book *book, const char *file, const bool keep_replaced)
{
	const bool replaced = (book_replaced_file != NULL && strcmp(file, book_replaced_file) == 0);
	unsigned int header_edax = EDAX, header_book = BOOK;
	unsigned char header_version = VERSION, header_release = RELEASE;
	char *tmp_file;
	char *aside = NULL; // name the book file was kept as, given back if the new file does not replace it
	int unread = -1;
	FILE *f;
	int r;
	PositionArray *a;
	Position *p;

	if (keep_replaced && book_replaced_untracked) {
		error("cannot save %s automatically: the previous book file could not be recorded\n", file);
		return false;
	}
	// the file could not be opened when the book was to be loaded from it: this book does not hold its positions
	if (book_file_status(file) == BOOK_FILE_UNOPENED) {
		errno = 0;
		error("\nCannot save book to %s: the file could not be opened when the book was loaded (it may be used by another program), and this book does not hold its positions; existing book was not replaced\n", file);
		return false;
	}
	book_remove_stale_tmp(file);
	tmp_file = (char*) malloc(strlen(file) + 32);
	if (tmp_file == NULL) { error("Cannot allocate save path for %s\n", file); return false; }
#ifdef _WIN32
	sprintf(tmp_file, "%s.tmp.%lu", file, (unsigned long) GetCurrentProcessId());
#else
	sprintf(tmp_file, "%s.tmp.%lu", file, (unsigned long) getpid());
#endif
	f = fopen(tmp_file, "wb");
	if (f == NULL) { error("Cannot open temporary book %s\n", tmp_file); free(tmp_file); return false; }
	info("Saving book to %s...", file);
	book_set_date(book);

	r = fwrite(&header_edax, sizeof (unsigned int), 1, f);
	r += fwrite(&header_book, sizeof (unsigned int), 1, f);
	r += fwrite(&header_version, 1, 1, f);
	r += fwrite(&header_release, 1, 1, f);
	r += fwrite(&book->date, sizeof book->date, 1, f);
	r += fwrite(&book->options, sizeof book->options, 1, f);
	r += fwrite(&book->n_nodes, sizeof book->n_nodes, 1, f);

	if (r == 7) {
		BookStream stream;
		if (!book_stream_open(&stream, f)) r = 0;
		else {
			foreach_position(p, a, book) {
				if (!position_write(p, &stream)) {
					r = 0;
					break;
				}
			}
			if (!book_stream_flush(&stream)) r = 0;
			book_stream_close(&stream);
		}
	}
	// the data must be on disk before the new file replaces the book
	if (r == 7 && fflush(f) != 0) r = 0;
#ifdef _WIN32
	if (r == 7 && _commit(_fileno(f)) != 0) r = 0;
#else
	if (r == 7 && fsync(fileno(f)) != 0) r = 0;
#endif
	if (fclose(f) != 0) r = 0;
	if (r != 7) {
		error("\nCannot write complete book to %s; existing book was not replaced\n", file);
		goto book_save_failed;
	}
	// the file could not be loaded: its content is not in this book, so it is kept under another name
	if (book_file_status(file) == BOOK_FILE_DAMAGED) {
		unread = book_unread_find(file); // (forgotten once the new file replaces it)
		if (!book_set_aside(file, ".damaged", "could not be loaded", &aside)) {
			error("\nCannot keep the book that could not be loaded; %s was not replaced\n", file);
			goto book_save_failed;
		}
	}
	// the file holds the book in use before "book new", and the save was not asked for this file by its name:
	// it is kept under another name (not a file that only holds the initial position)
	if (keep_replaced && replaced) {
		const long long n = book_file_positions(file);
		char reason[64];
		snprintf(reason, sizeof reason, "holds the book in use before \"%s\"", book_replaced_by); // (book new, book load or book import)
		if (n != 0 && n != 1 && aside == NULL && !book_set_aside(file, ".old", reason, &aside)) {
			error("\nCannot keep the book in use before \"%s\"; %s was not replaced\n", book_replaced_by, file);
			goto book_save_failed;
		}
	}
	if (!file_replace(tmp_file, file)) {
		error("\nCannot save book to %s; existing book was not replaced\n", file);
		goto book_save_failed;
	}
	if (unread >= 0) book_unread_remove(unread);
	free(aside);
	free(tmp_file);
	if (replaced) book_set_replaced_file(NULL); // (the file now holds the book in memory)
	book->need_saving = false;
	info("done\n");
	return true;

book_save_failed: // the book file is left as it was
	if (aside) { // (kept under another name, then not replaced: it gets its name back)
		if (book_rename_back(aside, file)) warn("%s is back under its name\n", file);
		else error("cannot give %s its name back: the book is kept as %s\n", file, aside);
		free(aside);
	}
	remove(tmp_file);
	free(tmp_file);
	return false;
}

/**
 * @brief Save an opening book to a file given by its name.
 *
 * @param book Opening book.
 * @param file File name.
 * @return true if the book was saved.
 */
bool book_save(Book *book, const char *file)
{
	return book_save_to(book, file, false);
}

/**
 * @brief Save an opening book to the book file of the settings, when no file was named: when Edax
 * ends, and for "book save" without a file name.
 *
 * A file that still holds the book in use before "book new" is kept under another name.
 *
 * @param book Opening book.
 * @return true if the book was saved.
 */
bool book_save_to_book_file(Book *book)
{
	return book_save_to(book, options.book_file, true);
}

/**
 * @brief Save the book being learned to a side file (.store, .dev, .enh, ...).
 *
 * Unlike a save asked by the user (book_save), the book file itself is not up to date:
 * if the book needed saving, it still does (it is saved to the book file on exit).
 *
 * @param book Opening book.
 * @param file File name.
 * @return true if the book was saved.
 */
bool book_save_progress(Book *book, const char *file)
{
	const bool need_saving = book->need_saving;
	const bool ok = book_save(book, file);

	book_test_add(book_test_saves);

	book->need_saving = need_saving;
	return ok;
}

/*
 * Bucket-range parallelism.
 * Each task gets a contiguous range of buckets; tasks only read the book
 * structure (no position is added or removed while they run).
 */
#ifndef BOOK_TEST_TASKS
#define book_n_task() options.n_task
#else
#define book_n_task() BOOK_TEST_TASKS /* test builds: threads of the book functions, whatever the search uses */
#endif

struct ChangedSet;

typedef struct BookTask {
	Book *book;
	int first, last;           /**< bucket range [first, last) */
	const struct ChangedSet *changed; /**< book_link: positions whose score changed while linking */
	unsigned long long *item;  /**< collected (bucket << 32 | index << 8 | move) items, in bucket order */
	long long n, size;
	long long n_changed;       /**< positions changed by the task (book_remove_links_range) */
	bool oom;
	void (*run)(struct BookTask*);
	long long done;     /**< positions scanned so far (progress display only; see book_task_add_done) */
	long long finished; /**< 1 when the task is over (progress display only; see book_task_finish) */
} BookTask;

// the progress of a task is written by its thread and read by the thread that prints it: atomic accesses
#if defined(_MSC_VER)
// (_InterlockedCompareExchange64 only: the other 64-bit intrinsics do not exist in 32-bit x86 builds)
static inline void book_task_add_done(BookTask *task, const long long n)
{
	long long old;
	do old = task->done; while (_InterlockedCompareExchange64(&task->done, old + n, old) != old);
}
#define book_task_done(task) _InterlockedCompareExchange64(&(task)->done, 0, 0)
#define book_task_finish(task) _InterlockedCompareExchange64(&(task)->finished, 1, 0)
#define book_task_is_finished(task) (_InterlockedCompareExchange64(&(task)->finished, 0, 0) != 0)
#else
#define book_task_add_done(task, n) __atomic_add_fetch(&(task)->done, (long long) (n), __ATOMIC_RELAXED)
#define book_task_done(task) __atomic_load_n(&(task)->done, __ATOMIC_RELAXED)
#define book_task_finish(task) __atomic_store_n(&(task)->finished, 1, __ATOMIC_RELEASE)
#define book_task_is_finished(task) (__atomic_load_n(&(task)->finished, __ATOMIC_ACQUIRE) != 0)
#endif

static void book_task_push(BookTask *task, const unsigned long long item)
{
	if (task->n == task->size) {
		const long long size = task->size + task->size / 2 + 4096;
		unsigned long long *p = (unsigned long long*) realloc(task->item, size * sizeof *p);
		if (p == NULL) { task->oom = true; return; }
		task->item = p; task->size = size;
	}
	task->item[task->n++] = item;
}

static void* book_task_main(void *v)
{
	BookTask *task = (BookTask*) v;
	task->run(task);
	book_task_finish(task);
	return NULL;
}

/** @return true (at most once per second) when a progress line is due. */
static bool book_progress_due(long long *next)
{
	const long long t = real_clock();
	if (t < *next) return false;
	*next = t + 1000;
	return true;
}

/**
 * @brief Run a function on every bucket range with options.n_task threads.
 *
 * With a progress label, all the ranges run in worker threads and this thread prints
 * "<label>...<positions scanned>/<positions> positions checked" once per second.
 *
 * @param changed Given to every task (see book_link_tasks; NULL otherwise).
 * @return number of tasks (task[0..n-1] hold the results, in bucket order).
 */
static int book_parallel_with(Book *book, void (*run)(BookTask*), BookTask *task, const char *progress, const struct ChangedSet *changed)
{
	int i, n = book_n_task();
	if (n > MAX_THREADS) n = MAX_THREADS;
	if (n < 1) n = 1;
	for (i = 0; i < n; ++i) {
		task[i].book = book;
		task[i].changed = changed;
		task[i].first = (int) ((long long) book->n * i / n);
		task[i].last = (int) ((long long) book->n * (i + 1) / n);
		task[i].item = NULL; task[i].n = task[i].size = 0; task[i].oom = false;
		task[i].n_changed = 0;
		task[i].run = run;
		task[i].done = 0; task[i].finished = 0;
	}
	if (progress && book_verbose) {
		Thread thread[MAX_THREADS];
		bool created[MAX_THREADS];
		long long next = real_clock() + 1000;
		bool finished = false;
		int n_wait = 0;
		for (i = 0; i < n; ++i) {
			created[i] = thread_create(thread + i, book_task_main, task + i);
			if (!created[i]) book_task_main(task + i); // no thread (memory exhausted): this one does the task
			else if (options.cpu_affinity) thread_set_cpu(thread[i], i); // (see book_run_workers)
		}
		while (!finished) {
			long long done = 0;
			relax(++n_wait <= 20 ? 1 : 50); // the scan of a small book is over at once
			finished = true;
			for (i = 0; i < n; ++i) { done += book_task_done(task + i); if (!book_task_is_finished(task + i)) finished = false; }
			if (!finished && book_progress_due(&next)) bprint("%s...%lld/%u positions checked\r", progress, done, book->n_nodes);
		}
		for (i = 0; i < n; ++i) if (created[i]) thread_join(thread[i]);
	} else {
		book_run_workers(book_task_main, task, sizeof *task, n);
	}
	return n;
}

static int book_parallel(Book *book, void (*run)(BookTask*), BookTask *task, const char *progress)
{
	return book_parallel_with(book, run, task, progress, NULL);
}

static void book_tasks_free(BookTask *task, const int n)
{
	int i;
	for (i = 0; i < n; ++i) free(task[i].item);
}

#define TASK_ITEM(b, k, x) (((unsigned long long) (b) << 32) | ((unsigned long long) (k) << 8) | (unsigned long long) (x))
#define TASK_BUCKET(item) ((int) ((item) >> 32))
#define TASK_INDEX(item) ((int) (((item) >> 8) & 0xffffff))
#define TASK_MOVE(item) ((int) ((item) & 0xff))
#define TASK_NO_LINK 0xff /* the position has no new link but its leaf must be searched */
#define TASK_KEY(item) (((unsigned long long) TASK_BUCKET(item) << 32) | (unsigned long long) TASK_INDEX(item)) /* place of the position (see position_key) */
#define TASK_POSITION(book, item) ((book)->array[TASK_BUCKET(item)].positions + TASK_INDEX(item))

static bool position_has_link(const Position *position, const int x)
{
	const Link *l;
	foreach_link(l, position) if (l->move == x) return true;
	return false;
}

/**
 * @brief Links of a position as a set of moves (the form that the plan of the searches uses).
 *
 * @param position Position.
 * @param pass Set to true if the pass is a link.
 * @return the moves that are links, as a bitboard.
 */
static unsigned long long position_link_set(const Position *position, bool *pass)
{
	unsigned long long links = 0;
	const Link *l;

	*pass = false;
	foreach_link(l, position) {
		if (l->move == PASS) *pass = true;
		else if (l->move <= H8) links |= x_to_bit(l->move);
	}
	return links;
}

/*
 * Positions whose score changed while book_link added links (book_link with threads).
 *
 * The one by one book_link() gives to a link the score that its position has at that time:
 * a position linked after one of these gets its new score, a position linked before it keeps
 * the old one. The threads of the first phase read the old scores, so the links to these
 * positions from the positions that come after them are set again in a last phase.
 */
typedef struct ChangedSet {
	unsigned long long *list;  /**< bucket << 32 | index of the positions, in book order */
	long long n;
	unsigned long long *code;  /**< hash table: hash code of the board | 1 (0: free slot); NULL: not available */
	unsigned long long *key;   /**< hash table: bucket << 32 | index */
	unsigned long long mask;
} ChangedSet;

#define changed_set_slot(set, c) (((c) >> 24) & (set)->mask) /* the low bits of the code choose the bucket of the book */

/** Build the hash table of the set (if it cannot be allocated, the list is searched instead). */
static void changed_set_index(ChangedSet *set, const Book *book)
{
	unsigned long long size = 1024, j;
	long long i;

	while (size < (unsigned long long) set->n * 2) size <<= 1;
	set->code = (unsigned long long*) calloc(size, sizeof *set->code);
	set->key = (unsigned long long*) malloc(size * sizeof *set->key);
	if (set->code == NULL || set->key == NULL) {
		free(set->code); free(set->key);
		set->code = set->key = NULL;
		return;
	}
	set->mask = size - 1;
	for (i = 0; i < set->n; ++i) {
		const Position *p = book_position(book, set->list[i]);
		const unsigned long long code = board_get_hash_code(&p->board) | 1;
		for (j = changed_set_slot(set, code); set->code[j]; j = (j + 1) & set->mask) ;
		set->code[j] = code; set->key[j] = set->list[i];
	}
}

static void changed_set_free(ChangedSet *set)
{
	free(set->code); free(set->key); free(set->list);
}

/** Last phase of book_link: set again the links to the positions whose score changed before their parent was linked. */
static void book_link_refresh(BookTask *task)
{
	Book *book = task->book;
	const ChangedSet *set = task->changed;
	int b, k;
	Board next, unique;
	Link *l;

	for (b = task->first; b < task->last; ++b) {
		const PositionArray *a = book->array + b;
		for (k = 0; k < a->n; ++k) {
			Position *p = a->positions + k;
			const unsigned long long key = ((unsigned long long) b << 32) | (unsigned long long) k;
			foreach_link(l, p) {
				board_next(&p->board, l->move, &next);
				board_unique(&next, &unique);
				if (set->code) {
					const unsigned long long code = board_get_hash_code(&unique) | 1;
					unsigned long long j;
					for (j = changed_set_slot(set, code); set->code[j]; j = (j + 1) & set->mask) {
						if (set->code[j] == code && set->key[j] < key) {
							const Position *child = book_position(book, set->key[j]);
							if (board_equal(&child->board, &unique)) { l->score = -child->score.value; break; }
						}
					}
				} else { // no hash table: look for the position in the book, then in the list
					const Position *child = position_array_probe(book_array(book, &unique), &unique);
					if (child) {
						const unsigned long long child_key = position_key(book, child);
						long long lo = 0, hi = set->n - 1;
						while (child_key < key && lo <= hi) {
							const long long mid = (lo + hi) / 2;
							if (set->list[mid] == child_key) { l->score = -child->score.value; break; }
							if (set->list[mid] < child_key) lo = mid + 1; else hi = mid - 1;
						}
					}
				}
			}
		}
	}
}

#ifndef BOOK_TEST_LINK_NO_PREFETCH
/**
 * The positions that the moves of a position lead to, looked for together: their places in the book
 * are computed and asked for (prefetch) before the first one is read, as position_negamax_parallel()
 * does. Same positions, in the same order, as one book_probe() for each move.
 */
typedef struct ChildProbe {
	PositionArray *array[MAX_MOVE];
	Board unique[MAX_MOVE];
	int x[MAX_MOVE];
	int n;
} ChildProbe;

static inline void child_probe_add(ChildProbe *c, const Book *book, const Board *next, const int x)
{
	board_unique(next, c->unique + c->n);
	c->array[c->n] = book_array(book, c->unique + c->n);
	PREFETCH(c->array[c->n]);
	c->x[c->n++] = x;
}

static inline void child_probe_fetch(const ChildProbe *c)
{
	int i;
	for (i = 0; i < c->n; ++i) position_array_prefetch(c->array[i]);
}

/**
 * @brief Look for the positions that the moves of a position lead to (the pass, if it has no move), together.
 *
 * @param c Positions looked for (out; read them with position_array_probe()).
 * @param book Opening book.
 * @param p Position.
 * @param unlinked Only the moves that are not links yet.
 */
static inline void child_probe_moves(ChildProbe *c, const Book *book, const Position *p, const bool unlinked)
{
	unsigned long long moves = board_get_moves(&p->board);
	Board next;
	int x;

	c->n = 0;
	if (moves) {
		foreach_bit(x, moves) {
			if (unlinked && position_has_link(p, x)) continue;
			board_next(&p->board, x, &next);
			child_probe_add(c, book, &next, x);
		}
	} else if (can_move(p->board.opponent, p->board.player) && !(unlinked && position_has_link(p, PASS))) {
		next.player = p->board.opponent;
		next.opponent = p->board.player;
		child_probe_add(c, book, &next, PASS);
	}
	child_probe_fetch(c);
}

/**
 * Phase 1 of book_link: find the missing links.
 * As position_link() does, it also gives to the existing links the score of their position
 * (only this thread writes to the links of a position, and no thread reads the links of
 * another position).
 */
static void book_link_find(BookTask *task)
{
	Book *book = task->book;
	int b, k, x, i;
	ChildProbe c;

	for (b = task->first; b < task->last; ++b) {
		const PositionArray *a = book->array + b;
		for (k = 0; k < a->n; ++k) {
			Position *p = a->positions + k;
			bool found = false;
			const Position *child;
			Link *l;

			child_probe_moves(&c, book, p, false);
			for (i = 0; i < c.n; ++i) {
				child = position_array_probe(c.array[i], c.unique + i);
				if (child) {
					x = c.x[i];
					foreach_link(l, p) if (l->move == x) break;
					if (l < position_links(p) + p->n_link) l->score = -child->score.value;
					else { book_task_push(task, TASK_ITEM(b, k, x)); found = true; }
				}
			}
			if (!found && p->leaf.move == NOMOVE) book_task_push(task, TASK_ITEM(b, k, TASK_NO_LINK));
		}
		book_task_add_done(task, a->n);
	}
}

/**
 * Phase 1 of the link of book merge (as from v4.5.5-nikque.3): find the missing links (read only).
 * The moves that are already links are not probed and keep their score: book merge negamaxes the
 * book afterwards, which sets again the scores of the links of every position reachable from the root.
 */
static void book_link_find_missing(BookTask *task)
{
	Book *book = task->book;
	int b, k, i;
	ChildProbe c;

	for (b = task->first; b < task->last; ++b) {
		const PositionArray *a = book->array + b;
		for (k = 0; k < a->n; ++k) {
			const Position *p = a->positions + k;
			bool found = false;

			child_probe_moves(&c, book, p, true);
			for (i = 0; i < c.n; ++i) {
				if (position_array_probe(c.array[i], c.unique + i)) { book_task_push(task, TASK_ITEM(b, k, c.x[i])); found = true; }
			}
			if (!found && p->leaf.move == NOMOVE) book_task_push(task, TASK_ITEM(b, k, TASK_NO_LINK));
		}
		book_task_add_done(task, a->n);
	}
}
#else // test builds: the lookup of v4.5.5-nikque.9, one child after the other
static void book_link_find(BookTask *task)
{
	Book *book = task->book;
	int b, k, x;
	Board next;

	for (b = task->first; b < task->last; ++b) {
		const PositionArray *a = book->array + b;
		for (k = 0; k < a->n; ++k) {
			Position *p = a->positions + k;
			unsigned long long moves = board_get_moves(&p->board);
			bool found = false;
			const Position *child;
			Link *l;
			if (moves) {
				foreach_bit(x, moves) {
					board_next(&p->board, x, &next);
					child = book_probe(book, &next);
					if (child) {
						foreach_link(l, p) if (l->move == x) break;
						if (l < position_links(p) + p->n_link) l->score = -child->score.value;
						else { book_task_push(task, TASK_ITEM(b, k, x)); found = true; }
					}
				}
			} else if (can_move(p->board.opponent, p->board.player)) {
				next.player = p->board.opponent;
				next.opponent = p->board.player;
				child = book_probe(book, &next);
				if (child) {
					foreach_link(l, p) if (l->move == PASS) break;
					if (l < position_links(p) + p->n_link) l->score = -child->score.value;
					else { book_task_push(task, TASK_ITEM(b, k, PASS)); found = true; }
				}
			}
			if (!found && p->leaf.move == NOMOVE) book_task_push(task, TASK_ITEM(b, k, TASK_NO_LINK));
		}
		book_task_add_done(task, a->n);
	}
}

static void book_link_find_missing(BookTask *task)
{
	Book *book = task->book;
	int b, k, x;
	Board next;

	for (b = task->first; b < task->last; ++b) {
		const PositionArray *a = book->array + b;
		for (k = 0; k < a->n; ++k) {
			const Position *p = a->positions + k;
			unsigned long long moves = board_get_moves(&p->board);
			bool found = false;
			if (moves) {
				foreach_bit(x, moves) {
					if (!position_has_link(p, x)) {
						board_next(&p->board, x, &next);
						if (book_probe(book, &next)) { book_task_push(task, TASK_ITEM(b, k, x)); found = true; }
					}
				}
			} else if (can_move(p->board.opponent, p->board.player) && !position_has_link(p, PASS)) {
				next.player = p->board.opponent;
				next.opponent = p->board.player;
				if (book_probe(book, &next)) { book_task_push(task, TASK_ITEM(b, k, PASS)); found = true; }
			}
			if (!found && p->leaf.move == NOMOVE) book_task_push(task, TASK_ITEM(b, k, TASK_NO_LINK));
		}
		book_task_add_done(task, a->n);
	}
}
#endif

/*
 * Leaves of the merge source for positions that are in both books.
 * When relinking clears the leaf of such a position, the source leaf can be
 * used instead of a new search if it is still not a link: after relinking, the
 * destination links include every link of the source position, so the source
 * best non-link move is also the best non-link move of the destination.
 */
typedef struct MergeHint {
	unsigned long long key;    /**< bucket << 32 | index in the destination book */
	Link leaf;
	unsigned char level;
} MergeHint;

static MergeHint *merge_hint = NULL;
static long long merge_hint_n = 0, merge_hint_size = 0;

static void merge_hint_free(void)
{
	free(merge_hint); merge_hint = NULL;
	merge_hint_n = merge_hint_size = 0;
}

static bool merge_hint_add(const unsigned long long key, const Link *leaf, const int level)
{
	if (merge_hint_n == merge_hint_size) {
		const long long size = merge_hint_size + merge_hint_size / 2 + 4096;
		MergeHint *h = (MergeHint*) realloc(merge_hint, size * sizeof *h);
		if (h == NULL) return false;
		merge_hint = h; merge_hint_size = size;
	}
	merge_hint[merge_hint_n].key = key;
	merge_hint[merge_hint_n].leaf = *leaf;
	merge_hint[merge_hint_n].level = (unsigned char) level;
	++merge_hint_n;
	return true;
}

static int merge_hint_cmp(const void *a, const void *b)
{
	const unsigned long long x = ((const MergeHint*) a)->key, y = ((const MergeHint*) b)->key;
	return (x > y) - (x < y);
}

static const MergeHint* merge_hint_find(const unsigned long long key)
{
	long long lo = 0, hi = merge_hint_n - 1;
	while (lo <= hi) {
		const long long mid = (lo + hi) / 2;
		if (merge_hint[mid].key == key) return merge_hint + mid;
		if (merge_hint[mid].key < key) lo = mid + 1; else hi = mid - 1;
	}
	return NULL;
}

static void book_link_one_by_one(Book*);

static void plan_job_add(struct BookPlan*, const Board*, const unsigned long long, const bool, const int);

/**
 * @brief Plan the leaf searches that linking is going to do (book-store-tasks > 1).
 *
 * The positions that get new links, and their links, are known from the items: a position whose
 * leaf becomes a link (or that has no leaf) is searched again without its links, unless a leaf of
 * the merged book can be used. These searches are done at the same time (see book_plan_begin).
 *
 * @param book Opening book (not changed).
 * @param task Items found by the threads.
 * @param n Number of tasks.
 */
static void book_link_plan(Book *book, const BookTask *task, const int n)
{
	int i;
	long long j;

	for (i = 0; i < n; ++i) {
		for (j = 0; j < task[i].n; ) {
			const unsigned long long key = task[i].item[j] >> 8;
			const Position *p = TASK_POSITION(book, task[i].item[j]);
			const MergeHint *hint = merge_hint_find(TASK_KEY(task[i].item[j]));
			const int n_moves = get_mobility(p->board.player, p->board.opponent);
			bool pass, no_leaf = (p->leaf.move == NOMOVE);
			unsigned long long links = position_link_set(p, &pass);
			int value = p->score.value, n_link;

			for (; j < task[i].n && (task[i].item[j] >> 8) == key; ++j) { // the new links of this position
				const int x = TASK_MOVE(task[i].item[j]);
				if (x == TASK_NO_LINK) continue;
				if (x == PASS) pass = true; else links |= x_to_bit(x);
				if (x == p->leaf.move) no_leaf = true;
				value = SCORE_INF; // a link was added: the score is not -SCORE_INF any more
			}
			if (!no_leaf) continue;
			if (hint && hint->level == p->level && hint->leaf.move != NOMOVE
			 && !(hint->leaf.move == PASS ? pass : (hint->leaf.move <= H8 && (links & x_to_bit(hint->leaf.move)) != 0))) continue;
			n_link = bit_count(links) + pass;
			if (leaf_search_needed(n_link, n_moves, value == -SCORE_INF)) plan_job_add(book_plan, &p->board, links, pass, p->level);
		}
	}
}

/**
 * @brief Link a book using several threads.
 *
 * Same links and leaf searches as the one by one book_link(), in the same order.
 * - exact (book_link): the book is the same as with the one by one book_link().
 * - not exact (book merge, as from v4.5.5-nikque.3 to 6): the scores of the links that already
 *   existed are not refreshed here (book_negamax() recomputes them for every position reachable
 *   from the root), and the leaves of the merged book are used (merge hints).
 * With book-store-tasks > 1, the leaf searches are done at the same time before (each one with
 * empty hash tables: see book_plan_begin); with book-store-tasks = 1 they are done one by one.
 *
 * @param book Opening book.
 * @param exact Same result as the one by one book_link().
 */
static void book_link_tasks(Book *book, const bool exact)
{
	BookTask task[MAX_THREADS];
	ChangedSet changed = {0};
	int i, n, value = 0;
	long long j, n_items = 0, i_item = 0, next = real_clock() + 1000;
	bool first = true, oom = false, plan = false;

	bprint("Linking book...\r");
	n = book_parallel(book, exact ? book_link_find : book_link_find_missing, task, "Linking book");
	for (i = 0; i < n; ++i) { n_items += task[i].n; oom = oom || task[i].oom; }
	if (exact && !oom) {
		changed.list = (unsigned long long*) malloc((n_items + 1) * sizeof *changed.list); // enough for every position of the items
		oom = (changed.list == NULL);
	}
	if (oom) {
		// nothing was done yet, but the scores of some existing links (exact), that the one by one link sets again
		book_tasks_free(task, n);
		merge_hint_free();
		error("cannot allocate link list; using sequential link\n");
		book_link_one_by_one(book);
		return;
	}

	if (n_items > 0 && book_plan == NULL && book_store_task_count() > 1 && book_plan_begin(book)) {
		plan = true;
		book_link_plan(book, task, n);
		book_plan_search(book);
	}

	for (i = 0; i < n; ++i) {
		for (j = 0; j < task[i].n; ++j) {
			if ((++i_item & 15) == 0 && book_progress_due(&next)) bprint("Linking book...%lld/%lld positions linked\r", i_item, n_items);
			const unsigned long long item = task[i].item[j];
			Position *p = TASK_POSITION(book, item);
			const int x = TASK_MOVE(item);
			const bool last = (j + 1 == task[i].n || (task[i].item[j + 1] >> 8) != (item >> 8));
			if (first) value = p->score.value; // score before the first item of this position
			first = last;
			if (x != TASK_NO_LINK) {
				Board target;
				Position *child;
				Link link;
				if (x == PASS) { target.player = p->board.opponent; target.opponent = p->board.player; }
				else board_next(&p->board, x, &target);
				child = book_probe(book, &target);
				link.score = -child->score.value;
				link.move = x;
				book_add_link(book, p, &link);
			}
			if (last && p->leaf.move == NOMOVE) {
				const MergeHint *hint = merge_hint_find(TASK_KEY(item));
				const int n_moves = get_mobility(p->board.player, p->board.opponent);
				if (hint && hint->level == p->level && hint->leaf.move != NOMOVE && !position_has_link(p, hint->leaf.move)) {
					// the source book already searched this position with a subset of these links
					p->leaf = hint->leaf;
					if (p->leaf.score > p->score.value) p->score.value = p->leaf.score;
					book->need_saving = true;
				} else if (leaf_search_needed(p->n_link, n_moves, p->score.value == -SCORE_INF)) {
					position_search(p, book);
				}
			}
			if (last && exact && p->score.value != value) changed.list[changed.n++] = TASK_KEY(item);
		}
	}
	if (plan) book_plan_end(book);
	book_tasks_free(task, n);
	merge_hint_free();
	if (changed.n) {
		changed_set_index(&changed, book);
		book_tasks_free(task, book_parallel_with(book, book_link_refresh, task, NULL, &changed));
	}
	changed_set_free(&changed);
	bprint("Linking book...%u done\n", book->n_nodes);
}

/**
 * @brief Link a book after book merge.
 *
 * With the leaves of the merged book (merge hints), even with one thread.
 *
 * @param book opening book.
 */
void book_link_parallel(Book *book)
{
	book_link_tasks(book, false);
}

/**
 * Phase 1 of book_fix: find wrong positions (read only; what is wrong is explained when they are fixed).
 */
static void book_fix_find(BookTask *task)
{
	int b, k;
	for (b = task->first; b < task->last; ++b) {
		const PositionArray *a = task->book->array + b;
		for (k = 0; k < a->n; ++k) {
			if (!position_check(a->positions + k, false)) book_task_push(task, TASK_ITEM(b, k, 0));
			else if (position_has_missing_link(a->positions + k, task->book)) book_task_push(task, TASK_ITEM(b, k, 1));
		}
		book_task_add_done(task, a->n);
	}
}

static void book_fix_one_by_one(Book*, int, int, int*, int*);

static void book_sort_range(BookTask *task)
{
	int b, k;
	for (b = task->first; b < task->last; ++b) {
		PositionArray *a = task->book->array + b;
		for (k = 0; k < a->n; ++k) position_sort(a->positions + k);
	}
}

void book_sort_parallel(Book *book)
{
	BookTask task[MAX_THREADS];
	bprint("Sorting book...");
	book_tasks_free(task, book_parallel(book, book_sort_range, task, NULL));
	bprint("done>\n");
}

/** @brief Mix the bits of a 64-bit value (the last step of splitmix64). */
static inline unsigned long long hash_mix(unsigned long long x)
{
	x ^= x >> 30; x *= 0xbf58476d1ce4e5b9ULL;
	x ^= x >> 27; x *= 0x94d049bb133111ebULL;
	return x ^ (x >> 31);
}

/**
 * @brief Mix the values of a position read from a book file into a hash (book_merge_file: the positions of the
 * second reading must be the ones checked by the first).
 *
 * @param p Position read.
 * @param h Hash so far.
 * @return the new hash.
 */
static unsigned long long position_read_hash(const Position *p, unsigned long long h)
{
	const Link *l = position_links(p);
	int i;

	h = hash_mix(h ^ p->board.player);
	h = hash_mix(h ^ p->board.opponent);
	h = hash_mix(h ^ ((unsigned long long) p->n_wins << 32 | p->n_draws));
	h = hash_mix(h ^ ((unsigned long long) p->n_losses << 32 | p->n_lines));
	h = hash_mix(h ^ ((unsigned long long) (unsigned char) p->score.value << 40 | (unsigned long long) (unsigned char) p->score.lower << 32
		| (unsigned long long) (unsigned char) p->score.upper << 24 | (unsigned long long) p->level << 16 | (unsigned long long) p->marks << 8 | p->n_link));
	h = hash_mix(h ^ ((unsigned long long) (unsigned char) p->leaf.score << 8 | p->leaf.move));
	for (i = 0; i < p->n_link; ++i) h = hash_mix(h ^ ((unsigned long long) (unsigned char) l[i].score << 8 | l[i].move));
	return h;
}

/**
 * @brief Merge a book file into the current book without loading it.
 *
 * The file is read twice: first to check that it is a complete book, then to
 * add the positions missing from the destination (see position_merge()).
 * On any error the destination book is left unchanged.
 *
 * The positions that the merge adds are the ones 'done' at the current epoch (as every new
 * position: see position_array_add). The merge starts a new epoch, so that no other position is.
 * (Up to v4.5.5-nikque.8 the added positions kept a mark of their own until negamax reached them:
 * after a merge that added positions out of reach from the root, the next merge of a book holding
 * one of them failed with "duplicated position", and removed all of them if it had added something.)
 *
 * @param dest Destination opening book.
 * @param file Source book file.
 * @return true on success.
 */
bool book_merge_file(Book *dest, const char *file)
{
	FILE *f = fopen(file, "rb");
	BookStream stream = {0};
	Position p, merged;
	unsigned int header_edax = 0, header_book = 0;
	unsigned char header[2];	// version, release
	unsigned int i, expected = 0;	// the position count is saved as a 32-bit unsigned int
	long long n_added = 0, next;
	unsigned long long hash = 0, checked_hash = 0; // (of the positions read: the file must not change between the two readings)
	int pass, k;
	bool ok = false, use_hints = true;

	if (f == NULL) { error("cannot open %s\n", file); return false; }
	if (fread(&header_edax, 4, 1, f) != 1 || fread(&header_book, 4, 1, f) != 1 || header_edax != EDAX || header_book != BOOK
	 || fread(header, 2, 1, f) != 1 || header[0] != VERSION) {
		error("%s is not a compatible edax opening book\n", file);
		fclose(f); return false;
	}
	{ Book h; if (fread(&h.date, sizeof h.date, 1, f) != 1 || fread(&h.options, sizeof h.options, 1, f) != 1 || fread(&expected, sizeof expected, 1, f) != 1) {
		error("Cannot read book settings from %s\n", file);
		fclose(f); return false;
	} }
	if (!book_stream_open(&stream, f)) { fclose(f); return false; }

	book_clean(dest); // no position of the destination is 'done' at the new epoch
	merge_hint_free(); // (hints of a merge that was not followed by book_link_parallel)

	for (pass = 0; pass < 2; ++pass) {
		const long long start = 8 + 2 + sizeof dest->date + sizeof dest->options + sizeof expected;
		if (pass) book_grow_buckets(dest, (long long) dest->n_nodes + expected); // the source is valid: prepare the destination
#ifdef _WIN32
		if (pass && _fseeki64(f, start, SEEK_SET) != 0) goto merge_end;
#else
		if (pass && fseeko(f, start, SEEK_SET) != 0) goto merge_end;
#endif
		stream.n = stream.pos = 0;
		hash = 0;
		bprint("%s book %s...\r", pass ? "Merging" : "Checking", file);
		next = real_clock() + 1000;
		for (i = 0; i < expected; ++i) {
			if ((i & 0xffff) == 0 && i && book_progress_due(&next)) bprint("%s book %s...%u/%u positions\r", pass ? "Merging" : "Checking", file, i, expected);
			if (!position_read(&p, &stream)) {
				error("Truncated or damaged opening book %s at position %u/%u\n", file, i, expected);
				goto merge_end;
			}
			hash = position_read_hash(&p, hash);
			if (pass) {
				Position *q = book_probe(dest, &p.board);
				if (q == NULL) {
					position_merge(&merged, &p);
					// a board written in another orientation (a book file of another tool): in the bucket of its unique board,
					// where it is looked for (up to v4.5.5-nikque.26 the merge linked no move to it, then book_fix moved it there)
					position_unique(&merged);
					if (dest->n_nodes == UINT_MAX || position_array_add(book_array(dest, &merged.board), &merged, dest) <= 0) { position_free(&p); error("cannot add a position to the book\n"); goto merge_end; }
					++dest->n_nodes; ++dest->stats.n_nodes; ++n_added;
				} else if (!position_is_done(q, dest)) { // a position of the destination: remember the source leaf (see book_link_parallel)
					// (a board of the file that is not the unique one has its leaf in another orientation: no hint)
					if (use_hints && p.leaf.move != NOMOVE && board_equal(&p.board, &q->board) && (p.leaf.move != q->leaf.move || p.n_link != q->n_link)) {
						if (!merge_hint_add(position_key(dest, q), &p.leaf, p.level)) { merge_hint_free(); use_hints = false; }
					}
				} else {
					position_free(&p);
					error("duplicated position in %s\n", file);
					goto merge_end;
				}
			}
			position_free(&p);
		}
		if (ferror(f) || !book_stream_at_end(&stream)) {
			error(pass ? "%s changed while it was merged: nothing is merged\n" : "Invalid opening book size or position count in %s\n", file);
			goto merge_end;
		}
		if (!pass) checked_hash = hash;
		else if (hash != checked_hash) {
			error("%s changed while it was merged: nothing is merged\n", file);
			goto merge_end;
		}
	}
	ok = true;
	if (merge_hint_n) qsort(merge_hint, merge_hint_n, sizeof *merge_hint, merge_hint_cmp);

merge_end:
	book_stream_close(&stream);
	fclose(f);
	if (!ok) merge_hint_free();
	if (!ok && n_added) { // remove what was added: the destination is left unchanged
		PositionArray *a;
		for (a = dest->array; a < dest->array + dest->n; ++a)
		for (k = 0; k < a->n; ++k) if (position_is_done(a->positions + k, dest)) { book_remove_at(dest, a, k); --k; }
	}
	if (ok) dest->need_saving = dest->need_saving || n_added > 0;
	bprint("Merging book %s...%lld positions added\n", file, n_added);
	return ok;
}

/**
 * @brief Negamax a book.
 *
 * @param book opening book.
 */
void book_negamax(Book *book)
{
	Position *root = book_root(book);

	if (root) {
		bprint("Negamaxing book...");
		book_clean(book);
		book_negamax_position(root, book);
		bprint("done\n");
	}
#ifdef BOOK_TEST_NODES
	fprintf(stderr, "<play nodes: %lld>", book_test_play_nodes);
	fprintf(stderr, "<book searches: %lld, nodes: %lld>\n", book_test_searches, book_test_nodes);
#endif
}

/**
 * @brief Negamax a book cut from a position (after book subtree).
 *
 * book_negamax() starts from the initial position: it does nothing when the book was cut from
 * another position. The counts of lines and the score bounds of the positions whose links were
 * cut (book depth reduced) then kept the values of the former book.
 *
 * @param book opening book.
 * @param board Position the book was cut from.
 */
void book_negamax_subtree(Book *book, const Board *board)
{
#ifndef BOOK_TEST_SUBTREE_NO_NEGAMAX
	Position *root = book_probe(book, board);

	if (root && !book_root(book)) {
		bprint("Negamaxing book...");
		book_clean(book);
		book_negamax_position(root, book);
		bprint("done\n");
	}
#else
	(void) book; (void) board;
#endif
}

/**
 * @brief Negamax a book from the position a learning command started from, found again (adding positions may
 * move it to another place).
 *
 * @param book opening book.
 * @param board Position the command started from (in the book).
 * @return the position.
 */
static Position* book_negamax_from(Book *book, const Board *board)
{
	Position *root = book_probe(book, board);

	book_clean(book);
	book_negamax_position(root, book);
	return root;
}

/**
 * @brief Link a book.
 *
 * @param book opening book.
 */
void book_link(Book *book)
{
	// with several threads: the same book, found faster (the threads look for the missing links)
	if (book_n_task() > 1) book_link_tasks(book, true);
	else book_link_one_by_one(book);
}

/**
 * @brief Link a book, a position after the other.
 *
 * @param book opening book.
 */
static void book_link_one_by_one(Book *book)
{
	PositionArray *a;
	Position *p;
	unsigned int i = 0; // (a book holds more positions than an int counts)

	bprint("Linking book...\r");
	foreach_position(p, a, book) {
		position_link(p, book);
		if (p->leaf.move == NOMOVE) {
			position_search(p, book);
		}
		if (++i % BOOK_INFO_RESOLUTION == 0) bprint("Linking book...%u\r", i);
	}
	bprint("Linking book...%u done\n", i);
}

/**
 * @brief Fix the positions of a book, a position after the other, from a position to the end.
 *
 * @param book opening book.
 * @param b Bucket of the first position to check.
 * @param k Index of the first position to check in its bucket.
 * @param n_fix Number of fixed positions (updated).
 * @param n_missing Number of positions with links to missing positions (updated).
 */
static void book_fix_one_by_one(Book *book, int b, int k, int *n_fix, int *n_missing)
{
	unsigned int n_checked = (unsigned int) k;
	long long next = real_clock() + 1000;
	bool progress = false;
	int i;

	for (i = 0; i < b; ++i) n_checked += (unsigned int) book->array[i].n;
	for (; b < book->n; ++b, k = 0) {
		PositionArray *a = book->array + b;
		for (; k < a->n; ++k) {
			Position *p = a->positions + k;
			// most positions need no fix: show the checked positions (once per second)
			if ((++n_checked & 0xfff) == 0 && book_progress_due(&next)) {
				bprint("Fixing book...%u/%u positions checked\r", n_checked, book->n_nodes);
				progress = true;
			}
			if (!position_is_ok(p)) {
				position_fix(p, book);
				++*n_fix;
			} else if (position_has_missing_link(p, book)) {
				position_remove_links(p, book);
				++*n_missing;
				++*n_fix;
			}
		}
	}
	if (progress) clear_line();
}

/** Find the positions whose board is not the unique one (see book_fix_unique_boards). */
static void book_fix_unique_find(BookTask *task)
{
	Book *book = task->book;
	Board unique;
	int b, k;

	for (b = task->first; b < task->last; ++b) {
		const PositionArray *a = book->array + b;
		for (k = 0; k < a->n; ++k) {
			board_unique(&a->positions[k].board, &unique);
			if (!board_equal(&unique, &a->positions[k].board)) book_task_push(task, TASK_ITEM(b, k, 0));
		}
		book_task_add_done(task, a->n);
	}
}

/**
 * @brief Turn the positions whose board is not the unique one (a book file of another tool) to their unique
 * board, and move them to the bucket of that board, where they are looked for.
 *
 * Their links and leaf turn with the board (they are not searched again). When the book already holds the
 * unique board, the position that was there is kept and the one moved is left out; of two positions moved to
 * the same board, the first in the order of the book is kept.
 *
 * @param book opening book.
 * @param n_fix Number of fixed positions (updated).
 */
static int book_fix_bucket_compare(const void *a, const void *b)
{
	const unsigned int x = *(const unsigned int*) a, y = *(const unsigned int*) b;
	return (x > y) - (x < y);
}

static bool book_fix_unique_boards(Book *book, int *n_fix)
{
	BookTask task[MAX_THREADS];
	const int n_tasks = book_parallel(book, book_fix_unique_find, task, NULL);
	Position *moved = NULL;
	long long n = 0, j;
	int i;
	bool oom = false;

	for (i = 0; i < n_tasks; ++i) { n += task[i].n; oom = oom || task[i].oom; }
	if (n > 0 && !oom && (unsigned long long) n <= SIZE_MAX / sizeof *moved) {
		moved = (Position*) malloc((size_t) n * sizeof *moved);
		unsigned int *buckets = (unsigned int*) malloc((size_t) n * sizeof *buckets);
		if (!moved || !buckets) oom = true;
		if (!oom) {
			long long k = 0;
			for (i = 0; i < n_tasks; ++i) for (j = 0; j < task[i].n; ++j) {
				Board unique;
				board_unique(&TASK_POSITION(book, task[i].item[j])->board, &unique);
				buckets[k++] = (unsigned int) (board_get_hash_code(&unique) & (book->n - 1));
			}
			qsort(buckets, (size_t) n, sizeof *buckets, book_fix_bucket_compare);
			for (k = 0; k < n && !oom;) {
				const long long first = k;
				PositionArray *dest = book->array + buckets[k++];
				while (k < n && buckets[k] == buckets[first]) ++k;
				const long long size = dest->n + k - first;
				oom = size > INT_MAX || !position_array_reserve(dest, book, (int) size);
			}
		}
		free(buckets);
	} else if (n > 0) oom = true;
	if (moved && !oom) {
		long long m = n;
		// taken out from the end, so that the places of the positions still to take do not change
		for (i = n_tasks - 1; i >= 0; --i) for (j = task[i].n - 1; j >= 0; --j) {
			const unsigned long long item = task[i].item[j];
			PositionArray *a = book->array + TASK_BUCKET(item);
			moved[--m] = a->positions[TASK_INDEX(item)];
			book_detach_at(book, a, TASK_INDEX(item));
		}
		for (j = 0; j < n; ++j) {
			position_unique(moved + j);
			if (book_probe(book, &moved[j].board) != NULL || book_add(book, moved + j) <= 0) position_free(moved + j);
			++*n_fix;
		}
	}
	free(moved);
	book_tasks_free(task, n_tasks);
	if (oom) {
		book->failed = true;
		error("not enough memory to move the positions whose board is not unique\n");
	}
	return !oom;
}

/**
 * @brief Fix a book.
 *
 * Wrong positions are recomputed; links to positions missing from the book
 * are removed (their best score may become the leaf, as book prune does).
 *
 * With several threads, the threads check the positions, then the positions found are fixed in
 * the order of the book: the result is the same as with one thread. Removing links does not
 * change what the next positions link to, but recomputing a wrong position can (its board
 * changes): from the first wrong position, the positions are checked again, one after the other.
 *
 * @param book opening book.
 */
bool book_fix(Book *book)
{
	int n_fix = 0, n_missing = 0;

	bprint("Fixing book...\r");
	if (!book_fix_unique_boards(book, &n_fix)) {
		// Repair dangling links even when non-canonical positions cannot be moved.
		PositionArray *a;
		Position *p;
		unsigned int cleaned = 0;
		foreach_position(p, a, book) {
			if (board_is_usable(&p->board) && position_remove_missing_links(p, book, true)) ++cleaned;
		}
		if (cleaned) {
			book->need_saving = true;
			warn("links to missing positions removed from %u positions\n", cleaned);
		}
		return false;
	}
	if (book_n_task() > 1) {
		BookTask task[MAX_THREADS];
		const int n = book_parallel(book, book_fix_find, task, "Fixing book");
		long long j;
		int i;
		bool oom = false;

		for (i = 0; i < n; ++i) oom = oom || task[i].oom;
		if (oom) {
			book_fix_one_by_one(book, 0, 0, &n_fix, &n_missing);
		} else {
			if (book_verbose) clear_line();
			for (i = 0; i < n; ++i) for (j = 0; j < task[i].n; ++j) {
				const unsigned long long item = task[i].item[j];
				if (TASK_MOVE(item) == 0) {
					book_fix_one_by_one(book, TASK_BUCKET(item), TASK_INDEX(item), &n_fix, &n_missing);
					i = n; break;
				}
				position_remove_links(TASK_POSITION(book, item), book);
				++n_missing;
				++n_fix;
			}
		}
		book_tasks_free(task, n);
	} else {
		book_fix_one_by_one(book, 0, 0, &n_fix, &n_missing);
	}
	if (n_missing) warn("links to missing positions removed from %d positions\n", n_missing);
	if (n_fix) book->need_saving = true;
	bprint("Fixing book...%d done\n", n_fix);
	return true;
}

/**
 * @brief Check whether the configured interval for a timed book save elapsed.
 *
 * @param start Time of the last timed save in milliseconds.
 * @return true when a timed save is due.
 */
static bool book_save_interval_elapsed(const long long start)
{
#ifdef BOOK_TEST_COUNTS
	if (book_test_save_ms >= 0) return real_clock() - start >= book_test_save_ms;
#endif
	return options.book_save_interval > 0
		&& real_clock() - start >= (long long)options.book_save_interval * 60000LL;
}

/**
 * @brief Save the book being learned to its side file when the interval of the timed saves is over.
 *
 * @param book Opening book.
 * @param file Side file (.dev, .fill, ...).
 * @param t Time of the last timed save, in milliseconds (updated).
 */
static void book_save_timed(Book *book, const char *file, unsigned long long *t)
{
	if (book_save_interval_elapsed((long long) *t)) {
		book_save_progress(book, file); // timed progress save
		*t = real_clock();
	}
}

/** Save completed deviate rounds at the configured cadence and at convergence.
 * Timed saves inside book_expand are independent: they can capture a partial
 * round, so they do not reset this completed-round counter.
 */
static void book_deviate_save_progress(Book *book, const char *file, const long long n_diffs, int *rounds_since_save)
{
	if (n_diffs > 0 && *rounds_since_save < INT_MAX) ++*rounds_since_save;
	if (*rounds_since_save > 0
	 && (n_diffs == 0 || (options.book_deviate_save_rounds > 0
	     && *rounds_since_save >= options.book_deviate_save_rounds))) {
		if (book_save_progress(book, file)) *rounds_since_save = 0;
	}
}

/**
 * @brief Remove a progress mark from every position of a book (the command that set it has ended).
 *
 * @param book opening book.
 * @param mark POSITION_MARK_CORRECTED or POSITION_MARK_RECALCULATED.
 * @return the number of positions that had the mark.
 */
static long long book_unmark(Book *book, const int mark)
{
	PositionArray *a;
	Position *p;
	long long n = 0;

	foreach_position(p, a, book) {
		if (p->marks & mark) {
			p->marks = (unsigned char) (p->marks & ~mark);
			++n;
		}
	}
	if (n) book->need_saving = true;
	return n;
}

/**
 * @brief Tell if a level searches a position deeper than another level.
 *
 * @param level Level.
 * @param than The other level.
 * @param n_empties Empty squares of the position.
 * @return true if the depth of level is greater, or if it is the same with a higher selectivity (fewer cuts).
 */
static bool level_is_deeper(const int level, const int than, const int n_empties)
{
	const int depth = LEVEL[level][n_empties].depth, than_depth = LEVEL[than][n_empties].depth;

	return depth > than_depth || (depth == than_depth && LEVEL[level][n_empties].selectivity > LEVEL[than][n_empties].selectivity);
}

/**
 * @brief Deepen a book.
 *
 * The positions whose level searches less deep than the level of the book get the level of the book, and
 * their leaves are searched again with it. A position searched again keeps this level: run on a book
 * saved before it ended (a timed save, or the save at the exit), the command goes on with the positions
 * left. (Up to v4.5.5-nikque.28, the positions of another level kept their level, and were searched
 * again with it: nothing became deeper, and a new run searched every position again. A position with a
 * deeper level than the book was searched again with its own level too: it is now left as it is.)
 *
 * @param book opening book.
 */
void book_deepen(Book *book)
{
	PositionArray *a;
	Position *p;
	int i = 0;
	unsigned long long t = real_clock();
	char file[FILENAME_MAX + 1];

	file_add_ext(options.book_file, ".dep", file);

	bprint("Deepening book...\r");
	foreach_position(p, a, book) {
		int n_empties = board_count_empties(&p->board);
		if (level_is_deeper(book->options.level, p->level, n_empties)) {
			p->level = book->options.level; // (the level of the search, and the mark of a position done)
			p->marks = 0; // (a new search: the marks of book correct and book leaf recalculate are for the former leaf)
			position_forget_leaf(p); // (also the score of a position without move and without link: it is searched again)
			position_search(p, book);
			book->need_saving = true; // (the level changed, also when no search was needed)
			if (++i % 10 == 0) {
				bprint("Deepening book...%d\r", i);
			}
			if (book_save_interval_elapsed((long long)t)) {
				book_save_progress(book, file); // timed progress save
				t = real_clock();
			}
		}
	}
	bprint("Deepening book...%d done\n", i);
}

static bool book_correct_concurrent(Book*, const char*, long long, int*, int*);

/**
 * @brief Correct wrong solved score in the book.
 *
 * Correct erroneous solved positions. Edax may be unstable and introduce bugs from time to time...
 *
 * Each solved position searched again gets the mark POSITION_MARK_CORRECTED, and a marked position is not
 * searched again: run on a book saved before it ended (the timed saves to <book-file>.err, or the save at
 * the exit), book correct goes on with the positions left. When every solved position is done, the marks
 * are removed. (Up to v4.5.5-nikque.28, every run searched all the solved positions again.)
 *
 * @param book opening book.
 */
void book_correct_solved(Book *book)
{
	PositionArray *a;
	Position *p;
	int i = 0;
	unsigned long long t = real_clock();
	char file[FILENAME_MAX + 1];
	Link old_leaf;
	int n_error = 0;
	char s[4];
	long long n_left = 0, n_marked = 0;

	file_add_ext(options.book_file, ".err", file);

	foreach_position(p, a, book) {
		if (position_is_solved(p)) {
			if (p->marks & POSITION_MARK_CORRECTED) ++n_marked; else ++n_left;
		}
	}
	if (n_marked) bprint("Correcting solved positions: %lld already done by a run that did not end, %lld left\n", n_marked, n_left);

#ifndef BOOK_TEST_CORRECT_OLD
	// several positions at the same time (book-expand-tasks), when the memory for the searches is available
	if (book_correct_concurrent(book, file, n_left, &i, &n_error)) {
		book_unmark(book, POSITION_MARK_CORRECTED);
		bprint("Correcting solved positions...%d done (%d error found)\n", i, n_error);
		return;
	}
#endif

	bprint("Correcting solved positions...\r");
	foreach_position(p, a, book) {
		if (position_is_solved(p) && !(p->marks & POSITION_MARK_CORRECTED)) {
			const int n_empties = board_count_empties(&p->board);
			old_leaf = p->leaf;
			position_forget_leaf(p); // (also the score of a position without move and without link: it is searched again)
			position_search(p, book);
			p->marks = (unsigned char) (p->marks | POSITION_MARK_CORRECTED); // (saved with the position: a new run does not search it again)
			book->need_saving = true;
			if (p->leaf.score != old_leaf.score) {
				++n_error;
				bprint("\nError found:\n");
				position_print(p, &p->board, stdout);
				move_to_string(old_leaf.move, n_empties & 1, s);
				bprint("instead of <%s:%d>\n\n", s, old_leaf.score);
			}
			if (++i % 10 == 0 || p->leaf.score != old_leaf.score) {
				bprint("Correcting solved positions...%d (%d error found)\r", i, n_error); 
			}
			if (book_save_interval_elapsed((long long)t)) {
				book_save_progress(book, file); // timed progress save
				t = real_clock();
			}
		}
	}
	book_unmark(book, POSITION_MARK_CORRECTED); // (every solved position is done)
	bprint("Correcting solved positions...%d done (%d error found)\n", i, n_error);
}

/** book-reproducible: expansions that each thread can search ahead of the next one to apply (a long search
 * holds up the others when they are this many items ahead of it) */
#ifndef BOOK_EXPAND_SLOTS
#define BOOK_EXPAND_SLOTS 64
#endif

static long long book_test_expand_redo = 0; /**< book-reproducible: expansions searched again because their links changed (read by tests/regression.c) */

/** book-reproducible: an expansion searched ahead of its turn, applied to the book in the order of the todo list */
typedef struct ExpandSlot {
	long long index;            /**< item of the todo list; -1: free */
	bool ready;                 /**< searched (or nothing to do): waits for its turn */
	bool skip;                  /**< nothing to expand: the position is not to do, or has no leaf */
	Position parent;            /**< copy of the parent (searched) */
	Position child;             /**< new position, with the links its search excluded */
	Link leaf;                  /**< move and score of the child seen from the parent */
	int r;                      /**< result of the search of the parent */
} ExpandSlot;

struct LearnRun;
static void learn_save_timed(struct LearnRun *run);

/** state shared by the threads of a concurrent book_expand */
typedef struct ExpandShared {
	Book *book;
	Lock lock;                  /**< guards the book, the counters and the output */
	Condition cond;             /**< book-reproducible: a slot is free, or an expansion is to search again */
	long long next;             /**< next todo_list item to take */
	int n_done;                 /**< expanded positions (progress) */
	const char *action, *tmp_file;
	unsigned long long t;       /**< time of the last timed save */
	struct LearnRun *learn;     /**< command that learns by rounds, with its own time of the last save (NULL: none) */
	bool stop;
	ExpandSlot *slot;           /**< book-reproducible: the expansions taken and not applied yet (NULL: applied when searched) */
	int n_slot;                 /**< number of slots: at most n_slot items are taken ahead of the next one to apply */
	long long applied;          /**< book-reproducible: next item to apply (the ones before are applied) */
	ExpandSlot *redo;           /**< book-reproducible: the next item to apply, to search again (its links changed) */
#ifdef BOOK_TEST_COUNTS
	int n_running, max_running; /**< test builds: searches running, and the most at the same time */
#endif
} ExpandShared;

#ifdef BOOK_TEST_COUNTS /* test builds: the searches at the same time */
#define expand_test_begin(s) (++(s)->n_running, (s)->max_running = MAX((s)->max_running, (s)->n_running))
#define expand_test_end(s) (--(s)->n_running)
#else
#define expand_test_begin(s) ((void) 0)
#define expand_test_end(s) ((void) 0)
#endif

/** one thread of a concurrent book_expand, with its own search (and hash tables) */
typedef struct ExpandWorker {
	ExpandShared *shared;
	Search *search;
} ExpandWorker;

/**
 * @brief Copy a position with its own copy of the links.
 *
 * @param dest Copy (to release with position_free()).
 * @param src Position.
 * @return false if the links cannot be allocated.
 */
static bool position_copy(Position *dest, const Position *src)
{
	*dest = *src;
	if (src->n_link > POSITION_INLINE_LINKS) {
		dest->links.array = (Link*) malloc(src->n_link * sizeof (Link));
		if (dest->links.array == NULL) { dest->n_link = 0; return false; }
		memcpy(dest->links.array, src->links.array, src->n_link * sizeof (Link));
	}
	return true;
}

/**
 * @brief Count a position of the todo list as done (expanded, or with nothing to expand), and show the progress (lock held).
 *
 * @param s Shared state.
 */
static void book_expand_count(ExpandShared *s)
{
	const Book *book = s->book;

	++s->n_done;
	bprint("%s...%d/%lld done: %lld positions, %lld links\r", s->action, s->n_done, book->stats.n_todo, book->stats.n_nodes, book->stats.n_links);
}

/**
 * @brief Take a parent to expand: copy it, and link its new child as the book is now (lock held).
 *
 * @param s Shared state.
 * @param q Parent (in the book).
 * @param parent Copy of the parent (out).
 * @param child New position (out).
 * @return false if the parent cannot be copied (the expansions stop).
 */
static bool book_expand_take(ExpandShared *s, const Position *q, Position *parent, Position *child)
{
	Book *book = s->book;

	if (!position_copy(parent, q)) { error("cannot copy a position"); book->failed = s->stop = true; return false; }
	position_init(child);
	board_next(&parent->board, parent->leaf.move, &child->board);
	child->level = parent->level;
	position_link(child, book);
	return true;
}

/**
 * @brief Search an expansion (lock not held): the new child, then the parent again, as position_expand().
 *
 * @param search Search of the thread.
 * @param parent Copy of the parent.
 * @param child New position.
 * @param leaf Move and score of the child seen from the parent (out).
 * @return the result of the search of the parent (see position_search_with).
 */
static int book_expand_search(Search *search, Position *parent, Position *child, Link *leaf)
{
	search_cleanup(search);
	position_search_with(child, search);
	parent->leaf.score = -child->score.value;
	*leaf = parent->leaf;
	return position_search_with(parent, search);
}

/**
 * @brief Apply a searched expansion to the book (lock held): update the parent, add the child.
 *
 * @param s Shared state.
 * @param parent Copy of the parent (released).
 * @param child New position (added to the book, or released).
 * @param leaf Move and score of the child seen from the parent.
 * @param r Result of the search of the parent.
 */
static void book_expand_apply(ExpandShared *s, Position *parent, Position *child, const Link *leaf, const int r)
{
	Book *book = s->book;
	Position *q = book_probe(book, &parent->board); // the parent may have moved: probe it again
	int added;

	if (q) {
		q->leaf = *leaf;
		added = position_add_link(q, &q->leaf);
		if (added == LINK_ADDED) ++book->stats.n_links;
		if (added == LINK_FAILED || (r & LEAF_NOT_LINKED)) book->failed = true; // no memory: the leaf is kept (its position is added below)
		else if (r & LEAF_SEARCHED) {
			q->leaf = parent->leaf;
			if (q->leaf.score > q->score.value) q->score.value = q->leaf.score;
		}
	}
	book->need_saving = true;
	position_free(parent);
	position_unique(child);
	added = book_add(book, child);
	if (added <= 0) position_free(child); // already in the book, or not added
	if (added < 0 && q) book_unlink_missing_child(book, q); // (q did not move: nothing was added)
	if (book->failed) s->stop = true; // a position could not be added: stop learning
	book_expand_count(s);
	// (the other threads wait for the lock)
	if (s->learn) learn_save_timed(s->learn);
	else book_save_timed(book, s->tmp_file, &s->t);
}

/**
 * @brief Expand todo positions in a thread.
 *
 * The book is only read (links of the new position) and written (parent update,
 * new position) under the shared lock; the two searches of an expansion run
 * outside of it with the thread's own search. The parent is updated with the same
 * steps as position_expand() (it is not changed by the other expansions).
 * The links of the new position are the ones of the book when it is taken: they
 * depend on the expansions that the other threads applied before (see book_expand_worker_ordered).
 *
 * @param v Worker.
 * @return NULL.
 */
static void* book_expand_worker(void *v)
{
	ExpandWorker *w = (ExpandWorker*) v;
	ExpandShared *s = w->shared;
	Book *book = s->book;

	for (;;) {
		Position parent, child, *q = NULL;
		Link leaf;
		int r;

		lock(s);
		while (!s->stop && s->next < book->todo_list.n) {
			Position *p = book_position(book, book->todo_list.item[s->next++]);
			if (!position_is_todo(p, book)) continue;
			if (p->leaf.move != NOMOVE) { q = p; break; }
			book_expand_count(s);
		}
		if (q == NULL) { unlock(s); break; }
		if (!book_expand_take(s, q, &parent, &child)) { unlock(s); break; }
		expand_test_begin(s);
		unlock(s);

		r = book_expand_search(w->search, &parent, &child, &leaf);

		lock(s);
		expand_test_end(s);
		book_expand_apply(s, &parent, &child, &leaf, r);
		unlock(s);
	}
	return NULL;
}

/**
 * @brief Tell if a new position has the links that position_link() would give it now.
 *
 * @param position New position (not in the book).
 * @param book Opening book.
 * @return true if its links (moves and scores) are the ones of the book.
 */
static bool position_links_current(const Position *position, Book *book)
{
	unsigned long long moves = board_get_moves(&position->board);
	const Link *links = position_links(position);
	Board next[64];
	int move[64];
	Position *child;
	int x, i, j, n = 0, n_linked = 0;

	if (moves) {
		foreach_bit(x, moves) {
			board_next(&position->board, x, next + n);
			move[n++] = x;
		}
	} else if (can_move(position->board.opponent, position->board.player)) { // pass
		next[0].player = position->board.opponent;
		next[0].opponent = position->board.player;
		move[n++] = PASS;
	}
	for (j = 0; j < n; ++j) {
		child = book_probe(book, next + j);
		if (child == NULL) continue;
		for (i = 0; i < position->n_link && links[i].move != move[j]; ++i) ;
		if (i == position->n_link || links[i].score != -child->score.value) return false;
		++n_linked;
	}
	return n_linked == position->n_link;
}

/**
 * @brief book-reproducible: apply the expansions that are ready, in the order of the todo list (lock held).
 *
 * An expansion is applied when the ones before it are. The links of its child were read when it was
 * taken: the expansions applied since then can have added a position that its child links to, or changed
 * the score of such a position. Its child then gets the links of the book now, and the expansion is
 * searched again (s->redo) before it is applied: each expansion is searched with the book that the
 * expansions before it in the list give, as one after the other (position_expand).
 *
 * @param s Shared state.
 */
static void book_expand_apply_ready(ExpandShared *s)
{
	Book *book = s->book;
	bool moved = false;

	while (!s->stop && s->redo == NULL) {
		ExpandSlot *e = s->slot + s->applied % s->n_slot;
		if (e->index != s->applied || !e->ready) break;
		if (!e->skip) {
			if (!position_links_current(&e->child, book)) {
				const Position *q = book_position(book, book->todo_list.item[e->index]);
				book->stats.n_links -= e->child.n_link; // (counted again by position_link)
				position_free(&e->parent);
				position_free(&e->child);
				e->ready = false;
				if (book_expand_take(s, q, &e->parent, &e->child)) { s->redo = e; ++book_test_expand_redo; }
				else e->index = -1;
				break;
			}
			book_expand_apply(s, &e->parent, &e->child, &e->leaf, e->r);
		}
		e->index = -1;
		++s->applied;
		moved = true;
	}
	if (moved || s->redo || s->stop) condition_broadcast(s);
}

/**
 * @brief Expand todo positions in a thread, the result not depending on the order the threads end in (book-reproducible).
 *
 * As book_expand_worker, the searches run outside the lock. An expansion is applied to the book in the order
 * of the todo list, after the ones before it (book_expand_apply_ready). A thread takes the next item while at
 * most n_slot items wait for their turn. With one-thread searches that start with empty hash tables, the book
 * is the same as when the positions are expanded one after the other with such a search.
 *
 * @param v Worker.
 * @return NULL.
 */
static void* book_expand_worker_ordered(void *v)
{
	ExpandWorker *w = (ExpandWorker*) v;
	ExpandShared *s = w->shared;
	Book *book = s->book;

	lock(s);
	for (;;) {
		ExpandSlot *e = NULL;

		while (!s->stop) {
			if (s->redo) { e = s->redo; s->redo = NULL; break; }
			if (s->next >= book->todo_list.n) break;
			if (s->next - s->applied >= s->n_slot) { condition_wait(s); continue; }
			e = s->slot + s->next % s->n_slot;
			e->index = s->next++;
			const Position *p = book_position(book, book->todo_list.item[e->index]);
			e->skip = !position_is_todo(p, book) || p->leaf.move == NOMOVE;
			if (e->skip) {
				if (position_is_todo(p, book)) book_expand_count(s);
				e->ready = true;
				book_expand_apply_ready(s);
				e = NULL;
				continue;
			}
			e->ready = false;
			if (!book_expand_take(s, p, &e->parent, &e->child)) { e->index = -1; e = NULL; condition_broadcast(s); }
			break;
		}
		if (e == NULL) break;
		expand_test_begin(s);
		unlock(s);

#ifdef BOOK_TEST_EXPAND_DELAY
		if (e->index % 3 == BOOK_TEST_EXPAND_DELAY) relax(3); // test builds: the expansions end in another order
#endif
		e->r = book_expand_search(w->search, &e->parent, &e->child, &e->leaf);

		lock(s);
		expand_test_end(s);
		e->ready = true;
		book_expand_apply_ready(s);
	}
	unlock(s);
	return NULL;
}

#ifdef BOOK_TEST_POOL_MAX
static int book_test_pool_searches = 0; /* test builds: searches created by the book functions and not released yet */
#endif

/** memory that a 32-bit program keeps free when it creates a search for the book functions (see below) */
#define BOOK_SEARCH_MEMORY_MARGIN ((size_t) 128 << 20)

/**
 * @brief Check that a search with hash tables of this size can be created.
 *
 * search_init_with() ends the program when the memory is exhausted, and the book in memory is lost.
 * The searches that the book functions create to search several positions at the same time are not
 * needed to go on: the memory of their hash tables is asked first, and without it the caller works
 * with fewer searches, or with the main search only (a 32-bit program cannot hold as many searches
 * as threads at a high level).
 *
 * A 32-bit program must also keep room for what is allocated later without such a check: the stacks of
 * the threads given to the searches and to the workers (1 MB each), their small tables, the positions
 * added to the book. (Without it, the searches of the positions filled the address space, the threads of
 * a search could not be created, and book learn waited for them for ever.)
 *
 * @param hash_bits Size of the main hash table (in number of bits).
 * @return true if the memory is available.
 */
static bool book_search_memory_available(const int hash_bits)
{
	const size_t n_main = (size_t) 1 << hash_bits, n_other = (n_main > 16 ? n_main >> 4 : 1);
	void *main_table, *other_tables, *margin = NULL;
	bool ok;

#ifdef BOOK_TEST_POOL_MAX
	if (book_test_pool_searches >= BOOK_TEST_POOL_MAX) return false; // test builds: as if the memory was exhausted
#endif
	main_table = malloc((n_main + 8) * sizeof (Hash));
	other_tables = malloc(2 * (n_other + 8) * sizeof (Hash)); // pv and shallow tables
	ok = (main_table != NULL && other_tables != NULL);
	if (ok && sizeof (void*) < 8) { // 32-bit program: the address space is what runs out
		margin = malloc(BOOK_SEARCH_MEMORY_MARGIN);
		ok = (margin != NULL);
	}
	free(main_table); free(other_tables); free(margin);
	return ok;
}

/**
 * @brief Create a search for the book functions.
 *
 * @param n_tasks Threads of the search.
 * @param hash_bits Size of its main hash table (in number of bits).
 * @return the search, NULL if the memory is not available.
 */
static Search* book_search_create(const int n_tasks, const int hash_bits)
{
	Search *search;

	if (!book_search_memory_available(hash_bits)) return NULL;
	search = (Search*) mm_malloc(sizeof (Search));
	if (search == NULL) return NULL;
	search_init_with(search, n_tasks, hash_bits);
#ifdef BOOK_TEST_POOL_MAX
	++book_test_pool_searches;
#endif
	return search;
}

/** @brief Release a search created by book_search_create(). */
static void book_search_release(Search *search)
{
	search_free(search);
	mm_free(search);
#ifdef BOOK_TEST_POOL_MAX
	--book_test_pool_searches;
#endif
}

/** searches that the book functions use to search several positions at the same time, kept from a call to the next */
typedef struct SearchPool {
	Search **search;
	int n, n_tasks, hash_bits; /**< number of searches; threads and hash table size of each one */
} SearchPool;

static SearchPool expand_pool;   /**< concurrent expansion: kept from one book_expand to the next of a learning command */
static SearchPool single_pool;   /**< book-reproducible: the one-thread search of the positions searched one after the other (see book_search) */
static SearchPool store_pool[2]; /**< to learn games (see book_store_release). [0]: one thread each, for the positions; [1]: for the games played at the same time */

/** @brief Release the searches of a pool. */
static void search_pool_release(SearchPool *pool)
{
	int i;
	for (i = 0; i < pool->n; ++i) book_search_release(pool->search[i]);
	free(pool->search);
	pool->search = NULL;
	pool->n = 0;
}

/**
 * @brief Get searches of a pool.
 *
 * @param pool Pool.
 * @param n Number of searches.
 * @param n_tasks Threads of each search.
 * @param hash_bits Hash table size of each search.
 * @return the number of searches available in pool->search: n, or fewer if the memory is exhausted.
 */
static int search_pool_get(SearchPool *pool, const int n, const int n_tasks, const int hash_bits)
{
	if (pool->n && (pool->n_tasks != n_tasks || pool->hash_bits != hash_bits)) search_pool_release(pool);
	if (pool->n < n) {
		Search **s = (Search**) realloc(pool->search, n * sizeof *s);
		if (s == NULL) return pool->n;
		pool->search = s;
		pool->n_tasks = n_tasks;
		pool->hash_bits = hash_bits;
		for (; pool->n < n; ++pool->n) {
			Search *search = book_search_create(n_tasks, hash_bits);
			if (search == NULL) break;
			search->options.verbosity = 0; // (the concurrent expansion sets its own)
			pool->search[pool->n] = search;
		}
	}
	return MIN(n, pool->n);
}

static int book_plan_hash_bits(const Book*, const int);

/** book-expand-tasks = auto: a round gets one thread for each search if it has this many positions for each thread.
 * A round with fewer positions than that is short: its last searches would run alone, each with one thread only,
 * and the searches are created again when the kind of round changes (level 18, 32 threads, rounds of 0 to a few
 * hundred positions: 19% more time for each position with 1, 9% more with 4, than with 2 threads each). */
#ifndef BOOK_EXPAND_ONE_THREAD_ROUND
#define BOOK_EXPAND_ONE_THREAD_ROUND 32
#endif

/**
 * @brief Threads of each search when book-expand-tasks = auto.
 *
 * Up to level 18 a search does not use a second thread well. A round with many positions to expand
 * (BOOK_EXPAND_ONE_THREAD_ROUND for each thread: 1024 with 32 threads) gets one thread for each search,
 * as many searches as threads (32 threads, 6.49 million positions, level 18: about 20% more positions
 * expanded in a minute than with 2 threads each). A round with fewer positions keeps 2 threads for each
 * search, the rule up to v4.5.5-nikque.9 for every round.
 * Above level 18: 4 threads up to level 24, then 8, as before.
 *
 * @param book Opening book.
 * @param n_todo Positions to expand in this round.
 * @return the number of threads.
 */
static int book_expand_auto_threads(const Book *book, const long long n_todo)
{
	const int level = book->options.level;

	if (level > 18) return level <= 24 ? 4 : 8;
#ifdef BOOK_TEST_EXPAND_OLD
	(void) n_todo;
	return 2; // test builds: the rule of v4.5.5-nikque.9, to compare
#else
	return n_todo >= (long long) BOOK_EXPAND_ONE_THREAD_ROUND * options.n_task ? 1 : 2;
#endif
}

/**
 * @brief Number of book positions expanded at the same time.
 *
 * book-expand-tasks = n, or auto (0): see book_expand_auto_threads.
 * Always 1 with the cpu option where it binds the threads (see book_one_search_at_a_time).
 *
 * @param book Opening book.
 * @param n_todo Positions to expand in this round.
 * @return the number of concurrent expansions (1 = one by one).
 */
static int book_expand_task_count(const Book *book, const long long n_todo)
{
	int n = options.book_expand_tasks;

	if (book_one_search_at_a_time()) return 1;
	if (n <= 0) n = options.book_reproducible ? options.n_task : options.n_task / book_expand_auto_threads(book, n_todo); // (book-reproducible: one-thread searches)
	return MAX(1, MIN(n, options.n_task));
}

/**
 * @brief Size of the hash tables of the searches that the book functions run at the same time
 * (book deviate, book enhance, book correct, book leaf recalculate: book-expand-tasks).
 *
 * The size that hash-table-size gives to a search with these threads. A round of book-expand-tasks =
 * auto that gives one thread to each search has twice as many searches as a round that gives them two
 * threads: its searches get the size of the one-thread searches of book store (19 bits at most up to
 * level 18, 20 up to level 21, 21 above: the level is the higher one of the book and of the level
 * setting), and at most half the size of the searches of the other rounds, so that such a round does
 * not take more memory than the others (except with hash-table-size = 10, the smallest size, that
 * cannot be halved: the searches of both kinds of round then have 10 bits, and a one-thread round
 * twice the hash tables of the others). Up to v4.5.5-nikque.12 only the first limit applied,
 * and such a round took twice the memory of the others when hash-table-size was not above that limit:
 * set to 19 or less (level 18 or less), to 20 or less (level setting 19 to 21), to 21 or less or to
 * auto (level setting above 21). With hash-table-size = auto and a level setting up to 21, or with a
 * larger hash-table-size, the sizes are the ones of v4.5.5-nikque.12.
 *
 * @param book Opening book.
 * @param n_tasks Threads of each search.
 * @return size (in number of bits).
 */
static int book_expand_hash_bits(const Book *book, const int n_tasks)
{
	int bits = options.hash_table_auto ? hash_table_size_auto(n_tasks) : options.hash_table_size;

	if (options.book_expand_tasks <= 0 && n_tasks == 1) {
		const int two = options.hash_table_auto ? hash_table_size_auto(2) : options.hash_table_size; // a search with two threads

		bits = book_plan_hash_bits(book, 1);
		if (bits > two - 1) bits = MAX(two - 1, 10); // (10 bits: the smallest hash-table-size)
	}
	return bits;
}

/** book-reproducible: largest hash table of a one-thread search of the book functions (21 bits: 57 MB) */
#define BOOK_SINGLE_HASH_BITS_MAX 21

/**
 * @brief book-reproducible: size of the hash tables of the one-thread searches of the book functions.
 *
 * The size of the main search of a run with one thread (n-tasks 1): hash-table-size, auto (21 bits) by default,
 * at most BOOK_SINGLE_HASH_BITS_MAX. A book function then gives the book that it gives with n-tasks 1, whatever
 * the number of threads (up to this size: there are as many searches as threads).
 *
 * @return size (in number of bits).
 */
static int book_single_hash_bits(void)
{
	const int bits = options.hash_table_auto ? hash_table_size_auto(1) : options.hash_table_size;

	return MIN(bits, BOOK_SINGLE_HASH_BITS_MAX);
}

/**
 * @brief Get the searches that search n positions at the same time (book-expand-tasks), in expand_pool:
 * as many searches as book_expand_task_count() tells (n at most), each with its share of the threads
 * and the hash tables of book_expand_hash_bits().
 *
 * @param book Opening book.
 * @param n Positions to search.
 * @return the number of searches available in expand_pool.search (fewer than asked if the memory is exhausted).
 */
static int book_expand_pool_get(const Book *book, const long long n)
{
	const int n_searches = book_expand_task_count(book, n);
	const int n_tasks = options.book_reproducible ? 1 : MAX(1, options.n_task / n_searches); // (a search with several threads can give another result from a run to the next)
	// (auto, one thread each: as many searches as threads, with smaller hash tables: see book_expand_hash_bits)
	const int bits = options.book_reproducible ? book_single_hash_bits() : book_expand_hash_bits(book, n_tasks);

	return search_pool_get(&expand_pool, (int) MIN(n_searches, n), n_tasks, bits);
}

/**
 * @brief Give a search of a pool the output settings of the search of the book.
 *
 * @param search Search.
 * @param book Opening book.
 * @return the search.
 */
static Search* book_search_output(Search *search, const Book *book)
{
	search->options.verbosity = book->search->options.verbosity;
	search->options.header = book->search->options.header;
	search->options.separator = book->search->options.separator;
	return search;
}

/**
 * @brief A search of expand_pool, with the output settings of the search of the book.
 *
 * @param book Opening book.
 * @param i Index of the search.
 * @return the search.
 */
static Search* book_expand_pool_search(const Book *book, const int i)
{
	return book_search_output(expand_pool.search[i], book);
}

/**
 * @brief The search of the positions that the book functions search one after the other.
 *
 * The main search, with all the threads. With book-reproducible, a one-thread search with the hash tables
 * of the searches of book_expand_concurrent (book_single_hash_bits): a position searched one after the other
 * then gets the result that it gets there, whatever the number of threads (the main search if the memory is
 * exhausted).
 *
 * @param book Opening book.
 * @return the search.
 */
static Search* book_search(Book *book)
{
	if (options.book_reproducible) {
		if (search_pool_get(&single_pool, 1, 1, book_single_hash_bits()) == 1) return book_search_output(single_pool.search[0], book);
		warn("book-reproducible: not enough memory for a one-thread search; the main search is used\n");
	}
	return book->search;
}

/**
 * @brief Expand the todo positions on several threads (book-expand-tasks > 1).
 *
 * @param book opening book.
 * @param action String with a description of current action.
 * @param tmp_file Temporary file name.
 * @return false if the memory for at least two searches is not available (nothing was done:
 * the positions are then expanded one after the other, with the main search).
 */
static bool book_expand_concurrent(Book *book, const char *action, const char *tmp_file, struct LearnRun *learn)
{
	ExpandShared shared;
	ExpandWorker *w;
	const int n_workers = book_expand_pool_get(book, book->todo_list.n);
	int i;

	if (n_workers < 2) return false;
	w = (ExpandWorker*) calloc(n_workers, sizeof *w);
	if (w == NULL) return false;
	shared.book = book; shared.next = 0; shared.n_done = 0; shared.action = action; shared.tmp_file = tmp_file;
	shared.t = real_clock(); shared.stop = false;
	shared.learn = learn;
	shared.slot = NULL; shared.n_slot = 0; shared.applied = 0; shared.redo = NULL;
	if (options.book_reproducible) { // the expansions are applied in the order of the list (book_expand_worker_ordered)
		shared.n_slot = BOOK_EXPAND_SLOTS * n_workers;
		shared.slot = (ExpandSlot*) malloc(shared.n_slot * sizeof *shared.slot);
		if (shared.slot == NULL) { free(w); return false; }
		for (i = 0; i < shared.n_slot; ++i) shared.slot[i].index = -1;
	}
	lock_init(&shared);
	condition_init(&shared);
#ifdef BOOK_TEST_COUNTS
	shared.n_running = shared.max_running = 0;
#endif

	for (i = 0; i < n_workers; ++i) {
		w[i].shared = &shared;
		w[i].search = book_expand_pool_search(book, i);
	}
	thread_run_workers(shared.slot ? book_expand_worker_ordered : book_expand_worker, w, sizeof *w, n_workers, false, false); // (each worker in its own thread, as before)
	if (shared.slot) { // expansions searched and not applied (the expansions stopped: a position could not be added)
		for (i = 0; i < shared.n_slot; ++i) if (shared.slot[i].index >= 0 && !shared.slot[i].skip) {
			position_free(&shared.slot[i].parent);
			position_free(&shared.slot[i].child);
		}
		free(shared.slot);
	}
	condition_free(&shared);
	lock_free(&shared);
	free(w);
#ifdef BOOK_TEST_COUNTS
	++book_test_parallel;
	fprintf(stderr, "<book expand: %lld todo, %d workers, %d at the same time>\n", book->todo_list.n, n_workers, shared.max_running);
#endif
	bprint("%s...%d/%lld done: %lld positions, %lld links\n", action, shared.n_done, book->stats.n_todo, book->stats.n_nodes, book->stats.n_links);
	return true;
}

/** state shared by the threads of a concurrent book_correct_solved */
typedef struct CorrectShared {
	Book *book;
	Lock lock;                  /**< guards the book, the counters and the output */
	PositionArray *a;           /**< next position to look at: bucket, */
	int k;                      /**< and index in the bucket */
	int n_done, n_error;
	const char *tmp_file;
	unsigned long long t;       /**< time of the last timed save */
} CorrectShared;

/** one thread of a concurrent book_correct_solved, with its own search (and hash tables) */
typedef struct CorrectWorker {
	CorrectShared *shared;
	Search *search;
} CorrectWorker;

/**
 * @brief Search the solved positions again in a thread.
 *
 * The positions are taken in the order of the book. No position is added, removed or moved by
 * book correct, so a position stays where it is while it is searched (on a copy, without the
 * lock); the book is only read and written with the lock held (a timed save sees whole positions).
 */
static void* book_correct_worker(void *v)
{
	CorrectWorker *w = (CorrectWorker*) v;
	CorrectShared *s = w->shared;
	Book *book = s->book;
	char str[4];

	for (;;) {
		Position copy, *p = NULL;
		Link old_leaf;
		bool copied;
		int r;

		lock(s);
		while (s->a < book->array + book->n) {
			if (s->k >= s->a->n) { ++s->a; s->k = 0; continue; }
			p = s->a->positions + s->k++;
			if (position_is_solved(p) && !(p->marks & POSITION_MARK_CORRECTED)) break;
			p = NULL;
		}
		if (p == NULL) { unlock(s); break; }
		old_leaf = p->leaf;
		copied = position_copy(&copy, p);
		if (copied) {
			unlock(s);
			position_forget_leaf(&copy);
			if (options.book_reproducible) search_cleanup(w->search); // (the move of an exact score can depend on the searches before)
			r = position_search_with(&copy, w->search);
			lock(s);
			p->leaf = copy.leaf;
			p->score.value = copy.score.value;
			position_free(&copy);
		} else { // no memory for the copy: search the position itself, with the lock held
			position_forget_leaf(p);
			r = position_search_with(p, w->search);
		}
		(void) r;
		p->marks = (unsigned char) (p->marks | POSITION_MARK_CORRECTED); // (saved with the position: a new run does not search it again)
		book->need_saving = true;
		++s->n_done;
		if (p->leaf.score != old_leaf.score) {
			++s->n_error;
			bprint("\nError found:\n");
			position_print(p, &p->board, stdout);
			move_to_string(old_leaf.move, board_count_empties(&p->board) & 1, str);
			bprint("instead of <%s:%d>\n\n", str, old_leaf.score);
		}
		if (s->n_done % 10 == 0 || p->leaf.score != old_leaf.score) {
			bprint("Correcting solved positions...%d (%d error found)\r", s->n_done, s->n_error);
		}
		book_save_timed(book, s->tmp_file, &s->t); // (the other threads wait for the lock)
		unlock(s);
	}
	return NULL;
}

/**
 * @brief Search the solved positions again, several at the same time (see book_correct_solved).
 *
 * As many searches as book deviate uses to expand as many positions (book-expand-tasks), with the
 * same threads and hash tables. The scores are exact: they do not depend on the search that finds them.
 *
 * @param book opening book.
 * @param tmp_file Temporary file name (timed saves).
 * @param n_solved Number of solved positions to search (without the mark POSITION_MARK_CORRECTED).
 * @param n_done Number of positions searched (out).
 * @param n_error Number of positions with another score (out).
 * @return false if the positions are to be searched one after the other (nothing was done).
 */
static bool book_correct_concurrent(Book *book, const char *tmp_file, const long long n_solved, int *n_done, int *n_error)
{
	CorrectShared shared;
	CorrectWorker *w;
	int n_workers, i;

	if (options.n_task < 2 || book_one_search_at_a_time()) return false;
	if (MIN(book_expand_task_count(book, n_solved), n_solved) < 2) return false;
	n_workers = book_expand_pool_get(book, n_solved); // (the searches of book_expand_concurrent)
	if (n_workers < 2) { search_pool_release(&expand_pool); return false; }
	w = (CorrectWorker*) calloc(n_workers, sizeof *w);
	if (w == NULL) { search_pool_release(&expand_pool); return false; }
	shared.book = book; shared.a = book->array; shared.k = 0; shared.n_done = shared.n_error = 0;
	shared.tmp_file = tmp_file; shared.t = real_clock();
	lock_init(&shared);

	bprint("Correcting solved positions...\r");
	for (i = 0; i < n_workers; ++i) {
		w[i].shared = &shared;
		w[i].search = book_expand_pool_search(book, i);
	}
	thread_run_workers(book_correct_worker, w, sizeof *w, n_workers, false, false);
	lock_free(&shared);
	free(w);
	search_pool_release(&expand_pool);
	*n_done = shared.n_done; *n_error = shared.n_error;
	return true;
}

/**
 * @brief Expand a book.
 *
 * Research all non link best move.
 *
 * @param book opening book.
 * @param action String with a description of current action.
 * @param tmp_file Temporary file name.
 */
static void book_expand_learn(Book *book, const char *action, const char *tmp_file, struct LearnRun *learn)
{
	PositionArray *a;
	Position *p;
	int i = 0, j, k;
	unsigned long long t = real_clock();

	bprint("%s...\r", action);

	// The todo positions are visited in bucket order: either from the list recorded
	// while marking them, or by scanning the whole book.
	if (book->todo_list.valid) todo_sort(book->todo_list.item, book->todo_list.n);

	if (book->todo_list.valid && book->todo_list.n > 1 && book_expand_task_count(book, book->todo_list.n) > 1) {
		if (book_expand_concurrent(book, action, tmp_file, learn)) return;
		// (not enough memory for the searches: one position after the other, below)
	}
#ifdef BOOK_TEST_COUNTS
	if (book->stats.n_todo > 1) ++book_test_serial;
	fprintf(stderr, "<book expand: %lld todo, one after the other>\n", book->stats.n_todo);
#endif

	a = book->array; k = 0;
	for (j = 0; ; ++j) {
		if (book->todo_list.valid) {
			if (j == book->todo_list.n) break;
			p = book_position(book, book->todo_list.item[j]); // do not keep p across expansions: a->positions may change!
		} else {
			while (a < book->array + book->n && k == a->n) { ++a; k = 0; }
			if (a == book->array + book->n) break;
			p = a->positions + k++;
		}
		if (position_is_todo(p, book)) {
			position_expand(p, book);
			if (book->failed) break; // a position could not be added: stop learning
			bprint("%s...%d/%lld done: %lld positions, %lld links\r", action, ++i, book->stats.n_todo, book->stats.n_nodes, book->stats.n_links);
			if (book->search->options.verbosity >= 2) putchar('\n'); else putchar('\r');
			
			if (learn) {
				learn_save_timed(learn);
			} else if (book_save_interval_elapsed((long long)t)) {
				book_save_progress(book, tmp_file); // timed progress save
				t = real_clock();
			}
		}
	}
	bprint("%s...%d/%lld done: %lld positions, %lld links\n", action, i, book->stats.n_todo, book->stats.n_nodes, book->stats.n_links);
}

// (book deviate, deviate2, deviate3, enhance, leaf expand: defined with the parts of book play)
static void book_expand(Book*, const char*, const char*);

/**
 * @brief Sort a book.
 *
 * @param book opening book.
 */
void book_sort(Book *book)
{
	PositionArray *a;
	Position *p;

	if (book_n_task() > 1) { // each position is sorted on its own: the result is the same
		book_sort_parallel(book);
		return;
	}
	bprint("Sorting book...");
	foreach_position(p, a, book) {
		position_sort(p);
	}
	bprint("done>\n");
}

/**
 * @brief Fill a book as up to v4.5.5-nikque.13: book fill 1 (no position is added: the positions one move
 * after the positions of the book get their missing links), and every book fill <n> in the test builds
 * BOOK_TEST_FILL_OLD, to compare.
 *
 * One thread; the positions added in a round are filled from in the same round; the position
 * filled from gets no link to the positions added from it.
 *
 * @param book opening book.
 * @param depth Distance to fill between two positions.
 */
static void book_fill_old(Book *book, const int depth)
{
	PositionArray *a;
	Position *p;
	long long n_diffs;
	int n_empties, k;
	char file[FILENAME_MAX + 1];

	file_add_ext(options.book_file, ".fill", file);

	do {
		n_diffs = 0;
		book->stats.n_nodes = book->stats.n_links = 0;
		for (a = book->array; a < book->array + book->n; ++a)
		for (k = 0; k < a->n; ++k) { // do not use foreach_positions here! a->positions may change!
			p = a->positions + k;
			n_empties = board_count_empties(&p->board);
			if (n_empties >= book->options.n_empties) {
				Board board = p->board; // board_fill() changes its board and adding positions may move p
				board_fill(&board, book, depth);
				if (n_diffs < book->stats.n_nodes + book->stats.n_links) {
					n_diffs = book->stats.n_nodes + book->stats.n_links;
					bprint("Book fill...%lld %lld done\r", book->stats.n_nodes, book->stats.n_links); 
				}
			}
		}
		bprint("Book fill...%lld %lld done\n", book->stats.n_nodes, book->stats.n_links);
		if (n_diffs) {
			book_negamax(book);
			book_save_progress(book, file);
		}
	} while (n_diffs && !book->failed); // stop if a position cannot be added
	bprint("Book fill... finished\n");
}

/*
 * Parallel selection of the positions to expand (book deviate, deviate2, deviate3).
 *
 * The per-position visit state is kept in the visit marks of the walk (see book_visit_init):
 * - deviate2/3: smallest accumulated loss + 1 (see book_deviate_total_by_loss).
 * - deviate: ply parity + 1 (see book_deviate_by_depth).
 * Positions to expand are collected per thread, then appended to the todo list.
 */

typedef struct DeviateWorker {
	Book *book;
	int mode;                  /**< 0: deviate, 2: deviate2, 3: deviate3 */
	int a, b, lower, upper;    /**< deviate: player & opponent deviations, window. deviate2/3: move & total loss */
	unsigned long long *item;  /**< todo positions (bucket << 32 | index) */
	long long n, size, n_todo;
	bool oom;
	volatile bool *conflict;
} DeviateWorker;

static void deviate_worker_todo(DeviateWorker *w, Position *p)
{
	Book *book = w->book;
	const unsigned char s = atomic_load_uchar(&p->state);
	if (position_state_is(s, book, POSITION_TODO) || !atomic_cas_uchar(&p->state, s, position_state_set(s, book->epoch, POSITION_TODO))) return; // already marked
	++w->n_todo;
	if (deviate_mark_only) return;
	if (w->n == w->size) {
		const long long size = w->size + w->size / 2 + 1024;
		unsigned long long *item = (unsigned long long*) realloc(w->item, size * sizeof *item);
		if (item == NULL) { w->oom = true; return; }
		w->item = item; w->size = size;
	}
	w->item[w->n++] = position_key(book, p);
}

/*
 * deviate2/3 selection by increasing accumulated loss.
 *
 * Positions are processed level by level (level = accumulated loss), so each
 * position is walked once, with its smallest loss, instead of being re-walked
 * every time a path with a smaller loss is found. The todo set is the same as
 * the one of the recursive walk (the todo test only depends on the smallest
 * loss). Each level is processed by options.n_task threads.
 */
typedef struct PositionList {
	Position **item;
	long long n, size;
} PositionList;

static bool position_list_push(PositionList *l, Position *p)
{
	if (l->n == l->size) {
		const long long size = l->size + l->size / 2 + 1024;
		Position **item = (Position**) realloc(l->item, size * sizeof *item);
		if (item == NULL) return false;
		l->item = item; l->size = size;
	}
	l->item[l->n++] = p;
	return true;
}

typedef struct LossWorker {
	DeviateWorker w;           /**< todo list, counters, parameters */
	PositionList *next;        /**< positions reached with a larger loss, per level (total_loss + 1 lists) */
	PositionList same;         /**< positions reached with the same loss */
	Position **cur;            /**< positions to process */
	long long first, last;
	int level;
} LossWorker;

/** Append the positions that the threads of a walk marked to the todo list of the book. */
static void deviate_workers_todo(Book *book, const LossWorker *lw, const int n)
{
	int i;
	long long j;

	for (i = 0; i < n; ++i) {
		book->stats.n_todo += lw[i].w.n_todo;
		for (j = 0; j < lw[i].w.n; ++j) if (!book_todo_push(book, lw[i].w.item[j])) break;
	}
}

/** @return true if the position can be walked (same tests as position_deviate_total). */
static inline bool deviate_total_walkable(const Book *book, const Position *p)
{
	return board_count_empties(&p->board) >= book->options.n_empties && !board_is_game_over(&p->board);
}

/** @return true if loss is the new smallest loss of the position. */
static inline bool deviate_total_relax(Book *book, Position *p, const int loss)
{
	unsigned char *v = book_visit(book, p);
	const unsigned char mark = (unsigned char) (loss + 1);
	unsigned char old;
	do {
		old = atomic_load_uchar(v);
		if (old && old <= mark) return false;
	} while (!atomic_cas_uchar(v, old, mark));
	return true;
}

static void* loss_worker_run(void *v)
{
	LossWorker *lw = (LossWorker*) v;
	DeviateWorker *w = &lw->w;
	Book *book = w->book;
	const int move_loss = w->a, total_loss = w->b, loss = lw->level;
	long long k;
	Board target;

	for (k = lw->first; k < lw->last; ++k) {
		Position *position = lw->cur[k];
		const Link *l;
		int move_error;

		if (atomic_load_uchar(book_visit(book, position)) != loss + 1) continue; // reached later with a smaller loss

		foreach_link(l, position) {
			move_error = position->score.value - l->score;
			if (0 <= move_error && move_error <= move_loss && loss + move_error <= total_loss) {
				Position *child;
				board_next(&position->board, l->move, &target);
				child = book_probe(book, &target);
				if (child && deviate_total_walkable(book, child) && deviate_total_relax(book, child, loss + move_error)) {
					if (!position_list_push(move_error ? lw->next + loss + move_error : &lw->same, child)) w->oom = true;
				} else if (child && leaf_recalc_bottom(book, child)) {
					deviate_worker_todo(w, child);
				}
			}
		}

		if (deviate_walk_all) { // (book fill with loss limits: every position of the walk)
			deviate_worker_todo(w, position);
			continue;
		}

		if (w->mode == 2 && position_is_solved(position)) continue;

		if (deviate_total_leaf_selected(position, move_loss, total_loss, loss)) deviate_worker_todo(w, position);
	}
	return NULL;
}

/**
 * @brief Walk a list of positions with the threads of a deviate walk (loss_worker_run, depth_worker_run), then
 * gather the positions that they reached at the same level.
 *
 * @param lw Workers (the first ones walk).
 * @param n Number of threads.
 * @param run Walk of a thread.
 * @param cur Positions to walk.
 * @param level Level of these positions (deviate2/3: accumulated loss; deviate: ply).
 * @param reached Positions reached (emptied after the walk: it may be cur).
 * @return false if the memory is exhausted.
 */
static bool deviate_walk_level(LossWorker *lw, const int n, void* (*run)(void*), PositionList *cur, const int level, PositionList *reached)
{
	const int m = (cur->n < 4096) ? 1 : n; // small levels are not worth threads
	bool ok = true;
	long long j;
	int i;

	for (i = 0; i < m; ++i) {
		lw[i].cur = cur->item; lw[i].level = level;
		lw[i].first = cur->n * i / m; lw[i].last = cur->n * (i + 1) / m;
		lw[i].same.n = 0;
	}
	book_run_workers(run, lw, sizeof *lw, m);
	reached->n = 0;
	for (i = 0; i < m && ok; ++i) {
		if (lw[i].w.oom) ok = false;
		for (j = 0; j < lw[i].same.n && ok; ++j) ok = position_list_push(reached, lw[i].same.item[j]);
	}
	return ok;
}

static bool book_deviate_total_by_loss(Book *book, Position *root, const int mode, const int move_loss, const int total_loss)
{
	LossWorker lw[MAX_THREADS];
	PositionList *level, cur = {0};
	const int n = book_walk_threads();
	int i, j, L;
	bool ok = true;

	if (!book->todo_list.valid) return false;
	level = (PositionList*) calloc(total_loss + 1, sizeof *level);
	if (level == NULL) return false;
	for (i = 0; i < n; ++i) {
		memset(lw + i, 0, sizeof lw[i]);
		lw[i].w.book = book; lw[i].w.mode = mode; lw[i].w.a = move_loss; lw[i].w.b = total_loss;
		lw[i].next = (PositionList*) calloc(total_loss + 1, sizeof (PositionList));
		if (lw[i].next == NULL) ok = false;
	}

	if (ok && deviate_total_walkable(book, root) && deviate_total_relax(book, root, 0)) ok = position_list_push(level + 0, root);
	else if (ok && leaf_recalc_bottom(book, root)) { // (book leaf recalculate4 from a position just under the depth of the book: its leaf, as position_deviate_total)
		deviate_worker_todo(&lw[0].w, root);
		if (lw[0].w.oom) ok = false;
	}

	for (L = 0; ok && L <= total_loss; ++L) {
		// take the level list; positions reached with the same loss are processed in extra rounds
		cur = level[L]; memset(level + L, 0, sizeof level[L]);
		while (ok && cur.n > 0) ok = deviate_walk_level(lw, n, loss_worker_run, &cur, L, &cur);
		free(cur.item); cur.item = NULL; cur.size = 0;
		// move the positions reached with larger losses to the global levels
		for (i = 0; i < n && ok; ++i) {
			for (j = L + 1; j <= total_loss && j <= L + move_loss; ++j) {
				long long k;
				for (k = 0; k < lw[i].next[j].n && ok; ++k) ok = position_list_push(level + j, lw[i].next[j].item[k]);
				lw[i].next[j].n = 0;
			}
		}
	}

	if (ok) deviate_workers_todo(book, lw, n);

	for (i = 0; i < n; ++i) {
		if (lw[i].next) { for (j = 0; j <= total_loss; ++j) free(lw[i].next[j].item); free(lw[i].next); }
		free(lw[i].same.item); free(lw[i].w.item);
	}
	for (L = 0; L <= total_loss; ++L) free(level[L].item);
	free(level);
	return ok;
}

/*
 * deviate selection ply by ply (breadth first), each ply with options.n_task threads.
 * Deviations and window only depend on the ply parity, so the visited set is the
 * one of the recursive walk unless a position is reached with both parities
 * (then the caller redoes the recursive walk).
 */
static void* depth_worker_run(void *v)
{
	LossWorker *lw = (LossWorker*) v;
	DeviateWorker *w = &lw->w;
	Book *book = w->book;
	const int parity = lw->level & 1;
	const int player_deviation = parity ? w->b : w->a; // the child ply uses the other deviation
	const int lower = parity ? -w->upper : w->lower, upper = parity ? -w->lower : w->upper;
	const unsigned char mark = (unsigned char) (parity + 1), child_mark = (unsigned char) ((parity ^ 1) + 1);
	long long k;
	Board target;

	for (k = lw->first; k < lw->last && !*w->conflict; ++k) {
		Position *position = lw->cur[k];
		const Link *l;
		unsigned char *visit;

		if (!(lower <= position->score.value && position->score.value <= upper && board_count_empties(&position->board) >= book->options.n_empties && !board_is_game_over(&position->board))) {
			if (leaf_recalc_bottom(book, position) && lower <= position->score.value && position->score.value <= upper) deviate_worker_todo(w, position);
			continue;
		}
		visit = book_visit(book, position);
		if (atomic_load_uchar(visit) || !atomic_cas_uchar(visit, 0, mark)) {
			if (atomic_load_uchar(visit) != mark) *w->conflict = true;
			continue;
		}
		foreach_link(l, position) {
			if (position->score.value - l->score <= player_deviation && lower <= l->score && l->score <= upper) {
				Position *child;
				board_next(&position->board, l->move, &target);
				child = book_probe(book, &target);
				if (child && atomic_load_uchar(book_visit(book, child)) != child_mark && !position_list_push(&lw->same, child)) w->oom = true;
			}
		}
		if (deviate_leaf_selected(position, player_deviation, lower, upper)) deviate_worker_todo(w, position);
	}
	return NULL;
}

static bool book_deviate_by_depth(Book *book, Position *root, const int player_deviation, const int opponent_deviation, const int lower, const int upper)
{
	LossWorker lw[MAX_THREADS];
	PositionList cur = {0}, next = {0};
	volatile bool conflict = false;
	const int n = book_walk_threads();
	int i, depth;
	bool ok = true;

	if (!book->todo_list.valid) return false;
	for (i = 0; i < n; ++i) {
		memset(lw + i, 0, sizeof lw[i]);
		lw[i].w.book = book; lw[i].w.conflict = &conflict;
		lw[i].w.a = player_deviation; lw[i].w.b = opponent_deviation; lw[i].w.lower = lower; lw[i].w.upper = upper;
	}
	ok = position_list_push(&cur, root);
	for (depth = 0; ok && !conflict && cur.n > 0; ++depth) {
		ok = deviate_walk_level(lw, n, depth_worker_run, &cur, depth, &next);
		{ PositionList t = cur; cur = next; next = t; }
	}
	if (conflict || !ok) {
		ok = false;
	} else {
		deviate_workers_todo(book, lw, n);
	}
	for (i = 0; i < n; ++i) { free(lw[i].same.item); free(lw[i].w.item); }
	free(cur.item); free(next.item);
	return ok;
}

/** walk wrappers: parallel when possible, else the original sequential walk (after a new book_clean) */
static void book_select_deviate(Book *book, Position *root, const int player_deviation, const int opponent_deviation, const int lower, const int upper)
{
	if (!book_visit_init(book)) return;
	if (!book_deviate_by_depth(book, root, player_deviation, opponent_deviation, lower, upper)) {
		book_clean(book);
		position_deviate(root, book, player_deviation, opponent_deviation, lower, upper);
	}
	book_visit_free(book);
}

static void book_select_deviate_total(Book *book, Position *root, const int move_loss, int total_loss, const bool skip_solved)
{
	if (total_loss > VISIT_LOSS_MAX) {
		warn("total loss %d reduced to %d\n", total_loss, VISIT_LOSS_MAX);
		total_loss = VISIT_LOSS_MAX;
	}
	if (!book_visit_init(book)) return;
	if (!book_deviate_total_by_loss(book, root, skip_solved ? 2 : 3, move_loss, total_loss)) {
		book_clean(book);
		book_visit_clear(book);
		position_deviate_total(root, book, move_loss, total_loss, 0, skip_solved);
	}
	book_visit_free(book);
}

/**
 * @brief Deviate a book.
 *
 * @param book opening book.
 * @param board Position to start from.
 * @param relative_error Error relative to the current position's score.
 * @param absolute_error Error relative to the root position's score.
 */
void book_deviate(Book *book, Board *board, const int relative_error, const int absolute_error)
{
	Position *root = book_probe(book, board);
	if (root) {
		int score;
		long long n_diffs;
		int rounds_since_save = 0;
		char file[FILENAME_MAX + 1];

		file_add_ext(options.book_file, ".dev", file);
		book_clean(book);
		book_negamax_position(root, book);

		do {
			score = root->score.value;

			bprint("Book deviate %d %d:\n", relative_error, absolute_error);
			book_clean(book);
			book_select_deviate(book, root, relative_error, 0, score - absolute_error, score + absolute_error);
			bprint("Book deviate %lld todo\n", book->stats.n_todo);

			book_expand(book, "Book deviate", file);
			n_diffs = book->stats.n_nodes + book->stats.n_links;

			// Adding positions may relocate the array containing root.
			root = book_probe(book, board);
			bprint("Book deviate %d %d:\n", relative_error, absolute_error);
			book_clean(book);
			book_select_deviate(book, root, 0, relative_error, score - absolute_error, score + absolute_error);
			bprint("Book deviate %lld todo\n", book->stats.n_todo);

			book_expand(book, "Book deviate", file);
			n_diffs += book->stats.n_nodes + book->stats.n_links;

			root = book_probe(book, board);
			book_clean(book);
			book_negamax_position(root, book);
			book_deviate_save_progress(book, file, n_diffs, &rounds_since_save);
		} while (n_diffs && !book->failed); // stop if a position cannot be added
		bprint("Book deviate %d %d...finished\n", relative_error, absolute_error);
	}
	search_pool_release(&expand_pool);
	book_store_release(); // (the searches of the positions linked)
}

/**
 * @brief Deviate a book while limiting cumulative move losses (book deviate2 and book deviate3).
 *
 * @param book opening book.
 * @param board Position to start from.
 * @param move_loss Maximum loss for one move.
 * @param total_loss Maximum cumulative loss for both players.
 * @param skip_solved deviate2: the leaf of a solved position is not expanded. deviate3: it is.
 */
static void book_deviate_total(Book *book, Board *board, const int move_loss, const int total_loss, const bool skip_solved)
{
	const char *name = skip_solved ? "Book deviate2" : "Book deviate3";
	Position *root = book_probe(book, board);
	if (root) {
		long long n_diffs;
		int rounds_since_save = 0;
		char file[FILENAME_MAX + 1];

		file_add_ext(options.book_file, skip_solved ? ".dev2" : ".dev3", file);
		book_clean(book);
		book_negamax_position(root, book);

		do {
			bprint("%s %d %d:\n", name, move_loss, total_loss);
			book_clean(book);
			book_select_deviate_total(book, root, move_loss, total_loss, skip_solved);
			bprint("%s %lld todo\n", name, book->stats.n_todo);

			book_expand(book, name, file);
			n_diffs = book->stats.n_nodes + book->stats.n_links;

			root = book_negamax_from(book, board);
			book_deviate_save_progress(book, file, n_diffs, &rounds_since_save);
		} while (n_diffs && !book->failed); // stop if a position cannot be added
		bprint("%s %d %d...finished\n", name, move_loss, total_loss);
	}
	search_pool_release(&expand_pool);
	book_store_release(); // (the searches of the positions linked)
}

/**
 * @brief Deviate a book while limiting cumulative move losses, without expanding the solved positions.
 *
 * @param book opening book.
 * @param board Position to start from.
 * @param move_loss Maximum loss for one move.
 * @param total_loss Maximum cumulative loss for both players.
 */
void book_deviate2(Book *book, Board *board, const int move_loss, const int total_loss)
{
	book_deviate_total(book, board, move_loss, total_loss, true);
}

/**
 * @brief Deviate a book while limiting cumulative move losses (the solved positions are expanded too).
 *
 * @param book opening book.
 * @param board Position to start from.
 * @param move_loss Maximum loss for one move.
 * @param total_loss Maximum cumulative loss for both players.
 */
void book_deviate3(Book *book, Board *board, const int move_loss, const int total_loss)
{
	book_deviate_total(book, board, move_loss, total_loss, false);
}

/*
 * book leaf recalculate: search again the leaves that a walk of book deviate / deviate2 selects
 * (after eval.dat was changed, for instance). No position is added, removed or moved.
 */

/** what the searches changed */
typedef struct LeafRecalcStats {
	long long n_done, n_score, n_up, n_down, n_move;
	int max_up, max_down;
} LeafRecalcStats;

/** state shared by the threads of book leaf recalculate */
typedef struct LeafRecalcShared {
	Book *book;
	Lock lock;                  /**< guards the book, the counters and the output */
	long long next;             /**< next todo_list item to take */
	long long n_round;          /**< leaves searched in this walk */
	LeafRecalcStats *stats;
	const char *name, *tmp_file;
	unsigned long long t;       /**< time of the last timed save */
} LeafRecalcShared;

typedef struct LeafRecalcWorker {
	LeafRecalcShared *shared;
	Search *search;
} LeafRecalcWorker;

/** Count a searched leaf and show the progress (as book deviate does for its expansions). */
static void leaf_recalc_count(LeafRecalcShared *s, const Link *old_leaf, const Link *leaf)
{
	LeafRecalcStats *stats = s->stats;
	const int d = leaf->score - old_leaf->score;

	++stats->n_done; ++s->n_round;
	if (d) ++stats->n_score;
	if (d > 0) { ++stats->n_up; if (d > stats->max_up) stats->max_up = d; }
	if (d < 0) { ++stats->n_down; if (-d > stats->max_down) stats->max_down = -d; }
	if (leaf->move != old_leaf->move) ++stats->n_move;
	bprint("%s...%lld/%lld done: %lld scores changed, %lld moves changed\r", s->name, s->n_round, s->book->stats.n_todo, stats->n_score, stats->n_move);
	book_save_timed(s->book, s->tmp_file, &s->t); // (the other threads wait for the lock)
}

/**
 * @brief Search the leaves of the todo positions again, in a thread.
 *
 * As book_correct_worker: the position is searched on a copy, without the lock (it stays where it is:
 * nothing is added to the book); the book is only read and written with the lock held. Each search
 * starts with clean hash tables: with one thread, its result does not depend on the searches before it.
 */
static void* leaf_recalc_worker(void *v)
{
	LeafRecalcWorker *w = (LeafRecalcWorker*) v;
	LeafRecalcShared *s = w->shared;
	Book *book = s->book;

	for (;;) {
		Position copy, *p = NULL;
		Link old_leaf;

		lock(s);
		while (s->next < book->todo_list.n) {
			p = book_position(book, book->todo_list.item[s->next++]);
			if (leaf_recalc_wanted(p)) break;
			p = NULL;
		}
		if (p == NULL) { unlock(s); break; }
		old_leaf = p->leaf;
		if (position_copy(&copy, p)) {
			unlock(s);
			position_forget_leaf(&copy); // (a pass without link: its score too, to be searched)
			search_cleanup(w->search);
			position_search_with(&copy, w->search);
			lock(s);
			p->leaf = copy.leaf;
			p->score.value = copy.score.value;
			position_free(&copy);
		} else { // no memory for the copy: search the position itself, with the lock held
			position_forget_leaf(p);
			search_cleanup(w->search);
			position_search_with(p, w->search);
		}
		p->marks = (unsigned char) (p->marks | POSITION_MARK_RECALCULATED); // (saved with the position: a new run does not search it again)
		book->need_saving = true;
#ifdef BOOK_TEST_LEAF_LOG // test builds: every searched leaf, before and after
		fprintf(stderr, "<leaf %016llx %016llx %d %d %d %d>\n", p->board.player, p->board.opponent, old_leaf.move, old_leaf.score, p->leaf.move, p->leaf.score);
#endif
		leaf_recalc_count(s, &old_leaf, &p->leaf);
		unlock(s);
	}
	return NULL;
}

/**
 * @brief Search the leaves of the todo positions again.
 *
 * Several at the same time, with the searches, threads and hash tables that book deviate uses to
 * expand as many positions (book-expand-tasks); one after the other with the main search otherwise.
 *
 * @param book opening book.
 * @param name Name of the command (output).
 * @param tmp_file File of the timed saves.
 * @param stats Counters (updated).
 */
static void book_leaf_recalc_search(Book *book, const char *name, const char *tmp_file, LeafRecalcStats *stats)
{
	LeafRecalcShared shared;
	LeafRecalcWorker *w = NULL, one;
	const long long n = book->todo_list.n;
	int n_workers = 1, i;

	if (n == 0) return;
	shared.book = book; shared.next = 0; shared.n_round = 0; shared.stats = stats;
	shared.name = name; shared.tmp_file = tmp_file; shared.t = real_clock();
	lock_init(&shared);
	bprint("%s...\r", name);

	if (n > 1 && options.n_task > 1 && book_expand_task_count(book, n) > 1) {
		n_workers = book_expand_pool_get(book, n); // (the searches of book_expand_concurrent)
		if (n_workers >= 2) w = (LeafRecalcWorker*) calloc(n_workers, sizeof *w);
	}
	if (w) {
		for (i = 0; i < n_workers; ++i) {
			w[i].shared = &shared;
			w[i].search = book_expand_pool_search(book, i);
		}
		thread_run_workers(leaf_recalc_worker, w, sizeof *w, n_workers, false, false);
		free(w);
	} else { // one after the other, with the main search
		one.shared = &shared; one.search = book_search(book);
		leaf_recalc_worker(&one);
	}
	lock_free(&shared);
	bprint("%s...%lld/%lld done: %lld scores changed, %lld moves changed\n", name, shared.n_round, book->stats.n_todo, stats->n_score, stats->n_move);
}

/**
 * @brief Search again the leaves that book deviate or book deviate2 reaches (book leaf recalculate, 2, 3, 4).
 *
 * A round walks once (the commands do not loop as book deviate does, unless the setting below asks for it):
 * - kind 1: as book deviate, the leaves of the positions that it would expand,
 * - kind 2: as book deviate2, the leaves of the positions that it would expand,
 * - kind 3: as book deviate, the leaves of all the positions of its walks,
 * - kind 4: as book deviate2, the leaves of all the positions of its walk.
 * The leaves of the solved positions are never searched again. As a round of book deviate, the kinds 1
 * and 3 walk twice: with the deviation for the player, then (after the first leaves were searched again)
 * with the deviation for the opponent; a leaf is searched once in a round.
 *
 * The scores that a round changes move the walks: they then reach other leaves, that the round did not
 * search. With book-leaf-recalculate-rounds = n > 1, up to n rounds are done (each one after the negamax
 * of the round before); a round that changes no leaf is the last one (the next would walk the same
 * positions). 1, the default, is the single round of v4.5.5-nikque.12.
 *
 * Each leaf searched again gets the mark POSITION_MARK_RECALCULATED, and a marked leaf is not searched
 * again, in the next rounds nor in a new run: run on a book saved before it ended (the timed saves, or the
 * save at the exit), the command goes on with the leaves left. When the command ends, the marks are
 * removed. (Up to v4.5.5-nikque.28, a new run searched every leaf again, and a round searched again the
 * leaves of the rounds before that its walk reached: the same results, with one-thread searches.)
 *
 * @param book opening book.
 * @param board Position to start from.
 * @param kind Command (1 to 4).
 * @param x Relative error (kinds 1, 3) or loss for one move (kinds 2, 4).
 * @param y Absolute error (kinds 1, 3) or cumulative loss (kinds 2, 4).
 */
void book_leaf_recalculate(Book *book, Board *board, const int kind, const int x, const int y)
{
	static const char *const names[] = {"Book leaf recalculate", "Book leaf recalculate2", "Book leaf recalculate3", "Book leaf recalculate4"};
	static const char *const exts[] = {".leaf", ".leaf2", ".leaf3", ".leaf4"};
	Position *root = book_probe(book, board);
	const char *name;
	LeafRecalcStats stats = {0, 0, 0, 0, 0, 0, 0};
	char file[FILENAME_MAX + 1];
	const int n_rounds = MAX(1, options.book_leaf_recalculate_rounds);
	int i_round;
	bool complete = true; // every walk got its list of leaves (else the marks are kept, for a new run)
	long long n_marked = 0;
	PositionArray *a;
	Position *p;

	if (kind < 1 || kind > 4 || root == NULL) return;
	name = names[kind - 1];
	file_add_ext(options.book_file, exts[kind - 1], file);
	foreach_position(p, a, book) if (p->marks & POSITION_MARK_RECALCULATED) ++n_marked;
	if (n_marked) bprint("%s: %lld leaves already searched again by a run that did not end: they are not searched again\n", name, n_marked);
	book_clean(book);
	book_negamax_position(root, book);

	for (i_round = 1; ; ++i_round) {
		const long long n_changed = stats.n_score + stats.n_move;
		bool listed = true; // every walk of the round got its list of leaves

		if (n_rounds > 1) bprint("%s %d %d: round %d/%d\n", name, x, y, i_round, n_rounds);
		leaf_recalc_mode = (kind >= 3 ? LEAF_RECALC_WALKED : LEAF_RECALC_SELECTED);
		if (kind == 2 || kind == 4) {
			bprint("%s %d %d:\n", name, x, y);
			book_clean(book);
			book_select_deviate_total(book, root, x, y, true);
			if (book->todo_list.valid) todo_sort(book->todo_list.item, book->todo_list.n);
			bprint("%s %lld todo\n", name, book->stats.n_todo);
			if (book->todo_list.valid) book_leaf_recalc_search(book, name, file, &stats);
		} else {
			const int score = root->score.value;
			unsigned long long *first = NULL;
			long long n_first, i, n;

			bprint("%s %d %d:\n", name, x, y);
			book_clean(book);
			book_select_deviate(book, root, x, 0, score - y, score + y);
			if (book->todo_list.valid) todo_sort(book->todo_list.item, book->todo_list.n);
			else listed = false; // (the leaves of this walk are not searched: told below)
			bprint("%s %lld todo\n", name, book->stats.n_todo);
			n_first = book->todo_list.valid ? book->todo_list.n : 0;
			if (n_first) {
				first = (unsigned long long*) malloc(n_first * sizeof *first);
				if (first) memcpy(first, book->todo_list.item, n_first * sizeof *first);
				book_leaf_recalc_search(book, name, file, &stats);
			}

			// the second walk of a round of book deviate, without the leaves already searched
			bprint("%s %d %d:\n", name, x, y);
			book_clean(book);
			book_select_deviate(book, root, 0, x, score - y, score + y);
			if (book->todo_list.valid) {
				keys_sort(book->todo_list.item, (long long) book->todo_list.n);
				if (first) {
					for (i = n = 0; i < book->todo_list.n; ++i) {
						if (!bsearch(book->todo_list.item + i, first, n_first, sizeof *first, todo_item_cmp)) book->todo_list.item[n++] = book->todo_list.item[i];
					}
					book->todo_list.n = n;
					book->stats.n_todo = n;
				}
			}
			bprint("%s %lld todo\n", name, book->stats.n_todo);
			if (book->todo_list.valid) book_leaf_recalc_search(book, name, file, &stats);
			free(first);
		}
		leaf_recalc_mode = LEAF_RECALC_OFF;
		if (!book->todo_list.valid) listed = false;
		if (!listed) warn("%s: not enough memory for the list of the leaves\n", name);
		if (!listed) complete = false;

		book_clean(book);
		book_negamax_position(root, book);
		// another round only if this one changed a leaf (else the walks would be the same), and got its lists
		if (i_round >= n_rounds || !listed || stats.n_score + stats.n_move == n_changed) break;
	}
	if (complete && !book->failed) n_marked = book_unmark(book, POSITION_MARK_RECALCULATED); // (the command ended)
	else n_marked = 0;
	if (stats.n_done || n_marked) book_save_progress(book, file);
	if (n_rounds > 1) {
		bprint("%s %d %d...finished: %lld leaves, %lld scores changed (%lld up, %lld down, largest +%d / -%d), %lld moves changed, %d rounds\n",
			name, x, y, stats.n_done, stats.n_score, stats.n_up, stats.n_down, stats.max_up, stats.max_down, stats.n_move, i_round);
	} else {
		bprint("%s %d %d...finished: %lld leaves, %lld scores changed (%lld up, %lld down, largest +%d / -%d), %lld moves changed\n",
			name, x, y, stats.n_done, stats.n_score, stats.n_up, stats.n_down, stats.max_up, stats.max_down, stats.n_move);
	}
	search_pool_release(&expand_pool);
}

/**
 * @brief Give back the memory of the positions removed from the book pool.
 *
 * A loaded book keeps its positions in one block (the pool), bucket after bucket. After many
 * positions were removed, the kept positions are moved down to the start of the pool, in the same
 * order, and the end of the block is released in place (the positions of a bucket stay together
 * and in the same order: the book and what is saved do not change).
 *
 * @param book opening book.
 */
static void book_pool_compact(Book *book)
{
#if !defined(BOOK_TEST_NO_POOL_COMPACT)
	PositionArray *a;
	Position *end = book->pool;
	long long in_pool = 0;

	if (book->pool == NULL) return;
	for (a = book->array; a < book->array + book->n; ++a) {
		if (a->size < 0 && a->n > 0) {
			if (a->positions < end) return; // not in the order of the buckets (never the case after book_load): leave the rest as it is
			if (a->positions != end) memmove(end, a->positions, a->n * sizeof (Position));
			a->positions = end;
			end += a->n;
			in_pool += a->n;
		} else if (a->size < 0) { // an emptied bucket no longer points into the pool
			a->positions = NULL; a->size = 0;
		}
	}
	book_pool_shrink(book, in_pool);
	book->pool_unused = 0;
#else
	(void) book;
#endif
}

/**
 * @brief Give back the memory of the pool that the book no longer uses, when it is a large part of the pool.
 *
 * A bucket of the pool to which a position is added moves to its own block, and its place in the pool
 * is no longer used (as the places of the positions removed). Called when a command that changes the
 * book is over (no position is pointed to then): the kept positions keep their bucket and their index.
 *
 * @param book opening book.
 */
void book_pool_reclaim(Book *book)
{
	if (book->pool != NULL && book->pool_unused > book->pool_size / 4) book_pool_compact(book);
}

/** Remove the links to missing positions in a range of buckets (see book_remove_missing_links). */
static void book_remove_links_range(BookTask *task)
{
	int b, k;
	for (b = task->first; b < task->last; ++b) {
		PositionArray *a = task->book->array + b;
		for (k = 0; k < a->n; ++k) if (position_remove_links(a->positions + k, task->book)) ++task->n_changed;
	}
}

/**
 * @brief Remove the links to the positions removed from the book (after pruning).
 *
 * With several threads, each thread does a range of buckets: removing the links of a position
 * only reads the boards of the other positions (no position is added, removed or moved) and
 * only changes the links and the leaf of this position, so the book is the same as with one thread.
 *
 * @param book opening book.
 * @return true if a link was removed.
 */
static bool book_remove_missing_links(Book *book)
{
	bool changed = false;
#ifndef BOOK_TEST_REMOVE_LINKS_OLD
	if (book_n_task() > 1) {
		BookTask task[MAX_THREADS];
		const int n = book_parallel(book, book_remove_links_range, task, NULL);
		int i;
		for (i = 0; i < n; ++i) if (task[i].n_changed) changed = true;
		return changed;
	}
#endif
	{
		PositionArray *a;
		Position *p;
		foreach_position(p, a, book) if (position_remove_links(p, book)) changed = true;
	}
	return changed;
}

#ifdef BOOK_TEST_PRUNE_OLD
#define PRUNE_SEEN_A 0
#define PRUNE_SEEN_B 0
#else
#define PRUNE_SEEN_A POSITION_TODO
#define PRUNE_SEEN_B POSITION_BUSY
#endif

/**
 * @brief Negamax a book from the position it is cut from (book prune, book subtree), then start the marks
 * of the positions to keep.
 *
 * @param book Opening book.
 * @param root Position the book is cut from.
 */
static void book_cut_negamax(Book *book, Position *root)
{
#ifdef BOOK_TEST_SUBTREE_STAT
	subtree_stat_new = 0; subtree_stat_t0 = real_clock();
#endif
	book_clean(book);
#ifdef BOOK_TEST_SUBTREE_NEGAMAX_OLD
	position_negamax(root, book);
#else
	book_negamax_position(root, book); // with n-tasks threads (the same values)
#endif
	SUBTREE_STAT("negamax");

	book_clean(book);
}

/**
 * @brief End the cut of a book (book prune, book subtree), once the positions not kept are removed: remove the
 * links to them, and give back the memory of the pool.
 *
 * @param book Opening book.
 * @param n_nodes Number of positions before the cut.
 */
static void book_cut_end(Book *book, const unsigned int n_nodes)
{
	SUBTREE_STAT("remove");
	// the book that was cut is saved when Edax ends (up to v4.5.5-nikque.12 only if the negamax changed a score)
	if (book_remove_missing_links(book) || book->n_nodes != n_nodes) book->need_saving = true;
	SUBTREE_STAT("remove_links");
	book_pool_compact(book);
	SUBTREE_STAT("compact");
	bprint("done\n");
}

/**
 * @brief Prune a book.
 *
 * Remove positions Edax cannot reach.
 *
 * @param book opening book.
 */
void book_prune(Book *book)
{
	PositionArray *a;
	Position *root = book_root(book);
	int i;

	if (root) {
		const unsigned int n_nodes = book->n_nodes;
		book_cut_negamax(book, root);
		// the walks only have two sets of deviations (all the moves of a side, the best moves of the other):
		// a state flag for each tells that a position was already walked with it (cleared below)
#ifdef BOOK_TEST_PRUNE_MARK_OLD
		position_prune(root, book, 2*SCORE_INF, 0, -SCORE_INF, SCORE_INF, PRUNE_SEEN_A, PRUNE_SEEN_B);
		position_print(root, &root->board, stdout);
		bprint("Book prune %lld... done\n", book->stats.n_todo);

		position_prune(root, book, 0, 2*SCORE_INF, -SCORE_INF, SCORE_INF, PRUNE_SEEN_B, PRUNE_SEEN_A);
		bprint("Book prune %lld... done\n", book->stats.n_todo);
#else
		{
			// with n-tasks threads (the same marks and counts); with one thread, or without memory, as before
			const bool marked = book_prune_mark(book, root, 2*SCORE_INF, 0, PRUNE_SEEN_A, PRUNE_SEEN_B, 0);
			if (!marked) position_prune(root, book, 2*SCORE_INF, 0, -SCORE_INF, SCORE_INF, PRUNE_SEEN_A, PRUNE_SEEN_B);
			position_print(root, &root->board, stdout);
			bprint("Book prune %lld... done\n", book->stats.n_todo);

			if (!marked || !book_prune_mark(book, root, 2*SCORE_INF, 0, PRUNE_SEEN_A, PRUNE_SEEN_B, 1)) {
				if (marked) position_prune(root, book, 2*SCORE_INF, 0, -SCORE_INF, SCORE_INF, PRUNE_SEEN_A, PRUNE_SEEN_B); // (the marks were cleared)
				position_prune(root, book, 0, 2*SCORE_INF, -SCORE_INF, SCORE_INF, PRUNE_SEEN_B, PRUNE_SEEN_A);
			}
			bprint("Book prune %lld... done\n", book->stats.n_todo);
		}
#endif
#ifdef BOOK_TEST_SUBTREE_STAT
		fprintf(stderr, "<subtree-stat visits %lld, marked %lld>\n", book->stats.n_todo, subtree_stat_new);
#endif
		SUBTREE_STAT("mark");
		for (a = book->array; a < book->array + book->n; ++a)
		for (i = 0; i < a->n; ++i) {
			if (!position_is_done(a->positions + i, book)) {book_remove_at(book, a, i); --i;}
			else a->positions[i].state &= (unsigned char) ~(PRUNE_SEEN_A | PRUNE_SEEN_B);
		}
		book_cut_end(book, n_nodes);
	}
}

/**
 * @brief Prune a book.
 *
 * Remove positions Edax cannot reach.
 *
 * @param book opening book.
 */
void book_subtree(Book *book, const Board *board)
{
	PositionArray *a;
	Position *root = book_probe(book, board);
	int i;

	if (root && board_count_empties(&root->board) < book->options.n_empties - 1) {
		// nothing is kept under the depth of the book: up to v4.5.5-nikque.12 every position was removed without a word
		warn("book subtree: this position is deeper than the book depth; the book is not changed\n");
		return;
	}
	if (root) {
		const unsigned int n_nodes = book->n_nodes;
		book_cut_negamax(book, root);
#ifdef BOOK_TEST_SUBTREE_OLD
		position_prune(root, book, 2*SCORE_INF, 2*SCORE_INF, -SCORE_INF, SCORE_INF, 0, 0);
#else
		// every walk has the same deviations and bounds: a kept position is not walked again
#ifndef BOOK_TEST_PRUNE_MARK_OLD
		if (!book_prune_mark(book, root, 2*SCORE_INF, 2*SCORE_INF, POSITION_DONE, POSITION_DONE, 0)) // (with n-tasks threads: the same marks and count)
#endif
		position_prune(root, book, 2*SCORE_INF, 2*SCORE_INF, -SCORE_INF, SCORE_INF, POSITION_DONE, POSITION_DONE);
#endif
		position_print(root, &root->board, stdout);
		bprint("Book subtree %lld... done\n", book->stats.n_todo);
#ifdef BOOK_TEST_SUBTREE_STAT
		fprintf(stderr, "<subtree-stat visits %lld, marked %lld>\n", book->stats.n_todo, subtree_stat_new);
#endif
		SUBTREE_STAT("mark");
		for (a = book->array; a < book->array + book->n; ++a)
		for (i = 0; i < a->n; ++i) if (!position_is_done(a->positions + i, book)) {book_remove_at(book, a, i); --i;}
		book_cut_end(book, n_nodes);
	}
}


/**
 * @brief Enhance a book.
 *
 * @param book opening book.
 * @param board Position to start from.
 * @param midgame_error Error in midgame search.
 * @param endcut_error Error in endgame search.
 */
void book_enhance(Book *book, Board *board, const int midgame_error, const int endcut_error)
{
	Position *root = book_probe(book, board);
	if (root) {
		long long n_diffs;
		int rounds_since_save = 0;
		char file[FILENAME_MAX + 1];

		file_add_ext(options.book_file, ".enh", file);

		book->options.midgame_error = midgame_error;
		book->options.endcut_error = endcut_error;

		book_clean(book);
#ifdef BOOK_TEST_ENHANCE_NEGAMAX_OLD
		position_negamax(root, book);
#else
		book_negamax_position(root, book); // with n-tasks threads (the same values), as book deviate
#endif

		do {
			bprint("Book enhance %d %d...%lld %lld:\n", midgame_error, endcut_error, book->stats.n_nodes, book->stats.n_links);
			book_clean(book);
			if (!book_enhance_mark(book, root)) position_enhance(root, book); // (with n-tasks threads: the same marks)
			if (book->todo_list.valid) book->stats.n_todo = book->todo_list.n;
			book_expand(book, "Book enhance", file);
			n_diffs = book->stats.n_nodes + book->stats.n_links;

			root = book_probe(book, board);
			book_clean(book);
#ifdef BOOK_TEST_ENHANCE_NEGAMAX_OLD
			position_negamax(root, book);
#else
			book_negamax_position(root, book);
#endif
			book_deviate_save_progress(book, file, n_diffs, &rounds_since_save); // (book-deviate-save-rounds, as book deviate)
		} while (n_diffs && !book->failed); // stop if a position cannot be added
		bprint("Book enhance %d %d...finished\n", midgame_error, endcut_error);
	}
	search_pool_release(&expand_pool);
	book_store_release(); // (the searches of the positions linked)
}

/**
 * @brief Fingerprint of the saved position data: board, level, progress marks, scores, counts, leaf and links.
 * Position and link order, the save timestamp, and transient search state do not affect it.
 * Book learn separately checks the book level and depth.
 *
 * @param book Opening book.
 * @return the fingerprint.
 */
unsigned long long book_fingerprint(const Book *book)
{
	PositionArray *a;
	Position *p;
	unsigned long long sum = 0;

	foreach_position(p, a, book) {
		const Link *l = position_links(p);
		unsigned long long h = hash_mix(p->board.player ^ hash_mix(p->board.opponent)), links = 0;
		int i;
		const unsigned int n[4] = {p->n_wins, p->n_draws, p->n_losses, p->n_lines};
		h = hash_mix(h ^ ((unsigned long long) n[0] << 32 | n[1]));
		h = hash_mix(h ^ ((unsigned long long) n[2] << 32 | n[3]));
		h = hash_mix(h ^ ((unsigned long long) (unsigned char) p->score.value << 24
			| (unsigned long long) (unsigned char) p->score.lower << 16
			| (unsigned long long) (unsigned char) p->score.upper << 8 | p->marks));

		for (i = 0; i < p->n_link; ++i) links += hash_mix(h ^ ((unsigned long long) (unsigned char) l[i].score << 8 | l[i].move)); // (in any order)
		sum += hash_mix(h ^ links ^ ((unsigned long long) p->level << 24 | (unsigned long long) (unsigned char) p->leaf.score << 16
			| (unsigned long long) p->leaf.move << 8 | p->n_link));
	}
	return sum;
}

/**
 * @brief Count the links of a book.
 *
 * @param book Opening book.
 * @return number of links.
 */
long long book_count_links(const Book *book)
{
	PositionArray *a;
	Position *p;
	long long n = 0;

	foreach_position(p, a, book) n += p->n_link;
	return n;
}

/**
 * @brief display some book's informations.
 *
 * @param book opening book.
 */
void book_info(Book *book)
{
	PositionArray *a;
	Position *p;
	unsigned long long n_links = 0;
	unsigned long long n_leaves = 0;
	unsigned long long n_level[61] = {0};
	unsigned long long n_other = 0; // positions of another level than the book
	const unsigned long long max_shown = 10; // (they were all printed: millions of lines for a book merged from a book of another level)
	int min_array = INT_MAX, max_array = 0;
	int i;

	foreach_position(p, a, book) {
		n_links += p->n_link;
		if (p->leaf.move != NOMOVE) ++n_leaves;
		if (p->level <= 60) ++n_level[p->level]; // else: damaged position (book fix recomputes it)
		if (p->level != book->options.level) {
			if (++n_other <= max_shown) position_print(p, &p->board, stdout);
		}
	}
	if (n_other > max_shown) printf("(%llu positions have another level than the book: only the first %llu are shown)\n", n_other, max_shown);

	for (a = book->array; a < book->array + book->n; ++a) {
		if (a->n > max_array) max_array = a->n;
		if (a->n < min_array) min_array = a->n;
	}

	bprint("Edax Book %d.%d; ", VERSION, RELEASE);
	bprint("%d-%d-%d ", book->date.year, book->date.month, book->date.day);
	bprint("%d:%02d:%02d;\n", book->date.hour, book->date.minute, book->date.second);
	bprint("Positions: %u (moves = %lld links + %lld leaves);\n", book->n_nodes, n_links, n_leaves);
	for (i = 0; i < 61; ++i) {
		if (n_level[i]) {
			bprint("Level %d : %lld nodes\n", i, n_level[i]);
		}
	}
	bprint("Depth: %d\n", 61 - book->options.n_empties);
	bprint("Memory occupation: %llu\n", (unsigned long long) book->n_nodes * sizeof (Position) + (unsigned long long) book->n * sizeof (PositionArray) + n_links * sizeof (Link));
	bprint("Hash balance: %d < %d < %d\n", min_array, (int) (book->n_nodes / book->n), max_array);
}

/**
 * @brief Display a position from the book.
 *
 * @param book opening book.
 * @param board position to display.
 */
void book_show(Book *book, Board *board)
{
	GameStats stat = {0,0,0,0};
	Position *position = book_probe(book, board);
	unsigned long long n_games;

	if (position) {
		position_show(position, board, stdout);
		book_get_game_stats(book, board, &stat);
		n_games = stat.n_wins + stat.n_draws + stat.n_losses;
		if (n_games) {
			bprint("\nLines: %lld full games", n_games);
			bprint(" with %.2f%% win, %.2f%% draw, %.2f%% loss",
				100.0 * stat.n_wins / n_games, 100.0 * stat.n_draws / n_games, 100.0 * stat.n_losses / n_games);
		}
		bprint("\n       %lld incomplete lines.\n\n", stat.n_lines - n_games);
	}
}

/**
 * @brief Get a list of moves from the book.
 *
 * @param book Opening book.
 * @param board Position to display.
 * @param movelist List of moves.
 */
bool book_get_moves(Book *book, const Board *board, MoveList *movelist)
{
	Position *position = book_probe(book, board);
	if (position) {
		position_get_moves(position, board, movelist);
		return true;
	}

	return false;
}

/**
 * @brief Get a variation from the book.
 *
 * @param book Opening book.
 * @param board Position.
 * @param move First move;
 * @param line Bariation.
 */
void book_get_line(Book *book, const Board *board, const Move *move, Line *line)
{
	Position *position;
	Board b;
	Move m;

	line_push(line, move->x);
	board_next(board, move->x, &b);

	while ((position = book_probe(book, &b)) != NULL && !board_is_game_over(&position->board)) {
		position_get_random_move(position, &b, &m, &book->random, 0);
		if (m.x == NOMOVE) break; // no legal move in the book
		if (!line_push(line, m.x)) break; // a full line (the links of a damaged book may go round)
		board_update(&b, &m);
	}
}


/**
 * @brief Get a move at random from the opening book.
 *
 * @param book Opening book.
 * @param board Position to find a move from.
 * @param move Chosen move.
 * @param randomness Randomness.
 */
#if 0
#include "srbook.c"
#else
bool book_get_random_move(Book *book, const Board *board, Move *move, const int randomness)
{
	return book_get_random_move_with(book, board, move, randomness, &book->random);
}

/**
 * @brief Get a move at random from the opening book, with a given random generator.
 *
 * The book is only read: several threads can call it at the same time, each one with its generator.
 *
 * @param book Opening book.
 * @param board Position to find a move from.
 * @param move Chosen move.
 * @param randomness Randomness.
 * @param random Random generator.
 */
bool book_get_random_move_with(Book *book, const Board *board, Move *move, const int randomness, Random *random)
{
	Position *position = book_probe(book, board);
	if (position) {
		position_get_random_move(position, board, move, random, randomness);
		return move->x != NOMOVE; // (no legal move within the margin: the caller searches)
	}

	return false;
}
#endif

/**
 * Positions whose statistics were recounted during one call of book_get_game_stats().
 *
 * The statistics of a position are saturated (UINT_MAX) near the root of a large book: they are recounted from
 * its links, and a position reached by several sequences of moves was recounted each time (up to
 * v4.5.5-nikque.15: as many times as the sequences, millions near the root of a large book). Each one is
 * recounted once now. A position being recounted counts as 0 when a damaged book leads back to it (the
 * recursion was endless).
 */
typedef struct {
	const Position *position; /**< position (NULL: free entry) */
	bool done;                /**< false while it is being recounted */
	GameStats stat;           /**< its statistics */
} GameStatsMemoEntry;

typedef struct {
	GameStatsMemoEntry *entry;
	size_t size;              /**< entries (a power of 2; 0: no memory, nothing is kept) */
	size_t n;                 /**< used entries */
} GameStatsMemo;

static GameStatsMemoEntry* game_stats_memo_find(GameStatsMemo *memo, const Position *position, const bool add)
{
	size_t i;

	if (memo->size == 0) return NULL;
	if (add && 2 * (memo->n + 1) > memo->size) { // grow
		GameStatsMemo bigger;
		bigger.size = 2 * memo->size;
		bigger.n = 0;
		bigger.entry = (GameStatsMemoEntry*) calloc(bigger.size, sizeof (GameStatsMemoEntry));
		if (bigger.entry == NULL) return NULL; // (not kept: recounted again)
		for (i = 0; i < memo->size; ++i) {
			if (memo->entry[i].position) *game_stats_memo_find(&bigger, memo->entry[i].position, true) = memo->entry[i];
		}
		free(memo->entry);
		*memo = bigger;
	}
	for (i = ((size_t) position >> 4) & (memo->size - 1); memo->entry[i].position; i = (i + 1) & (memo->size - 1)) {
		if (memo->entry[i].position == position) return memo->entry + i;
	}
	if (!add) return NULL;
	++memo->n;
	memo->entry[i].position = position;
	return memo->entry + i;
}

static void position_get_game_stats(Book *book, Position *position, GameStats *stat, GameStatsMemo *memo)
{
	stat->n_wins = stat->n_losses = stat->n_draws = stat->n_lines = 0;

	if (position->n_wins == UINT_MAX || position->n_losses == UINT_MAX || position->n_draws == UINT_MAX || position->n_lines == UINT_MAX) {
		Board target;
		Link *l;
		GameStats child;
		Position *next;
		GameStatsMemoEntry *e = game_stats_memo_find(memo, position, false);
		const int n_empties = board_count_empties(&position->board);

		if (e) { // recounted before, or being recounted (a damaged book that leads back to it: 0)
			if (e->done) *stat = e->stat;
			return;
		}
		if ((e = game_stats_memo_find(memo, position, true)) != NULL) e->done = false;

		// the leaf counts as position_negamax() counts it (up to v4.5.5-nikque.15 it was left out)
		if (position->leaf.score > -SCORE_INF) {
			++stat->n_lines;
			if (n_empties <= 60 && LEVEL[position->level][n_empties].depth == n_empties && LEVEL[position->level][n_empties].selectivity == NO_SELECTIVITY) {
				if (position->leaf.score > 0) ++stat->n_wins;
				else if (position->leaf.score < 0) ++stat->n_losses;
				else ++stat->n_draws;
			}
		}
		foreach_link(l, position) {
			board_next(&position->board, l->move, &target);
			next = book_probe(book, &target);
			if (next == NULL) continue;
			position_get_game_stats(book, next, &child, memo);
			stat->n_wins += child.n_losses;
			stat->n_draws += child.n_draws;
			stat->n_losses += child.n_wins;
			stat->n_lines += child.n_lines;
		}
		// (found again: the table may have grown during the recursion)
		if ((e = game_stats_memo_find(memo, position, false)) != NULL) {
			e->stat = *stat;
			e->done = true;
		}
	} else {
		stat->n_wins = position->n_wins;
		stat->n_draws = position->n_draws;
		stat->n_losses = position->n_losses;
		stat->n_lines = position->n_lines;
	}
}

/**
 * @brief Get game statistics from a position.
 *
 * @param book Opening book.
 * @param board Position to find a move from.
 * @param stat Game statistics output.
 */
void book_get_game_stats(Book *book, const Board *board, GameStats *stat)
{
	Position *position;
	GameStatsMemo memo;

	assert(book != NULL);
	assert(board !=NULL);
	assert(stat != NULL);
	
	stat->n_wins = stat->n_losses = stat->n_draws = stat->n_lines = 0;

	position = book_probe(book, board);
	if (position) {
		memo.n = 0;
		memo.size = 1024;
		memo.entry = (GameStatsMemoEntry*) calloc(memo.size, sizeof (GameStatsMemoEntry));
		if (memo.entry == NULL) memo.size = 0;
		position_get_game_stats(book, position, stat, &memo);
		free(memo.entry);
	}
}


/*
 * Learning games with several threads (book-store-tasks > 1).
 *
 * book_add_board() searches the positions of a game one after the other. Which positions it is
 * going to search, and which moves of each one are links (so excluded from its search), only
 * depend on the positions that are in the book and on the ones that the games add. So:
 * 1. plan: go through the boards as book_add_board() will, without changing the book, and note
 *    each search (book_plan_board);
 * 2. search: do all these searches at the same time, each one with its own search and with
 *    empty hash tables (book_plan_search);
 * 3. add: call book_add_board() as usual. position_search() takes the result of the planned
 *    search of the same board with the same links, or searches as usual if there is none.
 * The book is changed by the third step only, in the same order as without a plan.
 */

/**
 * @brief Release the searches used to learn games (store_pool), and the one-thread search of book-reproducible.
 */
void book_store_release(void)
{
	search_pool_release(store_pool);
	search_pool_release(store_pool + 1);
	search_pool_release(&single_pool);
}

/**
 * @brief Size of the hash tables of a search used to learn games.
 *
 * hash-table-size = n: this size. auto: the size for the threads of the search.
 * A one-thread search starts with empty tables and is short at a low level, and there are as
 * many of them as threads: it never gets more than 19 bits (14 MB) up to level 18, 20 bits up
 * to level 21, 21 bits above, whatever hash-table-size is (32 searches with the tables of
 * hash-table-size = 24 would take 14 GB). At level 18, the searches of 30 games visited 0.7%
 * more nodes with 19 bits than with 21 bits (1.5% more with 18 bits).
 *
 * @param book Opening book.
 * @param n_tasks Threads of the search.
 * @return size (in number of bits).
 */
static int book_store_hash_bits(const Book *book, const int n_tasks)
{
	const int level = MAX(book->options.level, options.level);
	int bits;

#ifdef BOOK_TEST_HASH_BITS
	if (n_tasks == 1) return BOOK_TEST_HASH_BITS; // test builds: to choose the size
#endif
	bits = options.hash_table_auto ? hash_table_size_auto(n_tasks) : options.hash_table_size;
	if (n_tasks == 1) bits = MIN(bits, level <= 18 ? 19 : (level <= 21 ? 20 : 21));
	return bits;
}

/**
 * @brief Get searches to play games at the same time (see play_learn_games).
 *
 * @param book Opening book.
 * @param n Number of searches.
 * @param n_tasks Threads of each search.
 * @return the searches, NULL if they cannot be allocated.
 */
Search** book_store_searches(const Book *book, const int n, const int n_tasks)
{
	// with one thread each, they are the searches of the positions
	SearchPool *pool = store_pool + (n_tasks > 1);

	if (search_pool_get(pool, n, n_tasks, book_store_hash_bits(book, n_tasks)) < n) {
		// not all of them: the games are played one after the other, without these searches. The ones that were
		// created are released (they kept their memory, that the searches of the positions then lacked)
		search_pool_release(pool);
		return NULL;
	}
	return pool->search;
}

/**
 * @brief Get searches to complete games at the same time (base complete).
 *
 * They are searches of book_store_searches, with tables of the size of the searches of the planned positions
 * (book_plan_hash_bits) at the level setting. They are released by book_store_release.
 *
 * @param n Number of searches.
 * @param n_tasks Threads of each search at first.
 * @return the searches, NULL if they cannot be allocated.
 */
Search** book_game_searches(const int n, const int n_tasks)
{
	Book settings; // (only its level is read: the searches are those of the level setting)
	SearchPool *pool = store_pool;

	settings.options.level = options.level;
	if (search_pool_get(pool, n, 1, book_plan_hash_bits(&settings, n_tasks)) < n) {
		search_pool_release(pool);
		return NULL;
	}
	return pool->search;
}

/**
 * @brief Number of games learned at the same time.
 *
 * book-store-tasks = n, or auto (0): each game gets 1 thread.
 * Always 1 with the cpu option where it binds the threads (see book_one_search_at_a_time).
 *
 * @return the number of games learned at the same time; 1 = one position after the other, as
 * Edax always did.
 */
int book_store_task_count(void)
{
#ifdef BOOK_TEST_STORE
	return BOOK_TEST_STORE; // test builds: whatever the search uses
#else
	int n = options.book_store_tasks;

	if (book_one_search_at_a_time()) return 1;
	if (n <= 0) n = options.n_task;
	return MAX(1, MIN(n, options.n_task));
#endif
}

/**
 * @brief Number of threads used to learn games (book-store-tasks > 1).
 * @return number of threads.
 */
int book_store_thread_count(void)
{
	return MAX(1, MIN(book_n_task(), MAX_THREADS - 1));
}

#define PLAN_LEAF_UNKNOWN 0xfe /**< leaf of a position whose search is planned */

/** a planned search */
typedef struct PlanJob {
	Board board;               /**< board to search */
	unsigned long long links;  /**< moves that are links (excluded from the search) */
	Link leaf;                 /**< result of the search */
	unsigned char level;       /**< level of the search */
	bool pass;                 /**< the pass is a link */
	bool done;                 /**< the search was done */
} PlanJob;

/** a position as it will be after the boards already planned (in the book before the plan, or added by it) */
typedef struct PlanNode {
	Board board;               /**< unique board */
	unsigned long long links;  /**< moves that are links */
	unsigned char leaf;        /**< leaf move (NOMOVE: none, PLAN_LEAF_UNKNOWN: given by a planned search) */
	unsigned char level;       /**< level of the position */
	bool pass;                 /**< the pass is a link */
	bool no_score;             /**< its score is -SCORE_INF */
} PlanNode;

struct PlanWorker;

typedef struct BookPlan {
	Book *book;
	PlanJob *job;
	PlanNode *node;
	int n_job, job_size, n_node, node_size;
	int *job_index, *node_index;   /**< hash tables of the jobs and of the nodes: index + 1 (0: free slot) */
	unsigned int job_mask, node_mask;
	bool failed;                   /**< out of memory: nothing more is planned */
	bool usual;                    /**< a single search: it is done as usual, when the position is added */
	int n_used, n_missed;          /**< searches taken from the plan, searches that were not planned */
	Lock lock;                     /**< guards next, n_done, and the workers */
	int next, n_done;
	struct PlanWorker *worker;     /**< threads doing the searches (book_plan_search) */
	int n_worker, n_threads;       /**< workers started, threads that they share */
	int n_continued;               /**< searches stopped and continued with more threads (only counted in the test builds) */
	int max_workers;               /**< searches at the same time at most (book fill: book-expand-tasks); 0: one for each thread */
	bool isolate;                  /**< book play, extend, exhaust: every search starts with empty hash tables, planned or not (see learn_plan_begin) */
} BookPlan;

/** a thread doing planned searches */
typedef struct PlanWorker {
	BookPlan *plan;
	Search *search;
	Thread thread;
	bool progress;                 /**< this one shows the progress */
	int n_tasks;                   /**< threads of its search */
	/* guarded by the lock of the plan: */
	int want;                      /**< threads that its search should have */
	int run_tasks;                 /**< threads of the search that it is running (0: it is not searching) */
	bool busy;                     /**< it has a job */
} PlanWorker;

#define plan_slot(board, mask) ((unsigned int) (board_get_hash_code(board) >> 24) & (mask))

/** Add an item to a hash table of the plan (growing it if needed). */
static bool plan_index_add(BookPlan *plan, int **index, unsigned int *mask, const int n, const bool is_job)
{
	unsigned int j;
	int i;

	if (*index == NULL || (unsigned int) n * 2 > *mask) { // rebuild a larger table
		const unsigned int size = *index ? (*mask + 1) * 2 : 256;
		int *t = (int*) calloc(size, sizeof *t);
		if (t == NULL) return false;
		free(*index);
		*index = t; *mask = size - 1;
		i = 0;
	} else {
		i = n - 1;
	}
	for (; i < n; ++i) {
		const Board *board = is_job ? &plan->job[i].board : &plan->node[i].board;
		for (j = plan_slot(board, *mask); (*index)[j]; j = (j + 1) & *mask) ;
		(*index)[j] = i + 1;
	}
	return true;
}

static PlanNode* plan_node_find(const BookPlan *plan, const Board *unique)
{
	unsigned int j;

	if (plan->node_index == NULL) return NULL;
	for (j = plan_slot(unique, plan->node_mask); plan->node_index[j]; j = (j + 1) & plan->node_mask) {
		PlanNode *node = plan->node + plan->node_index[j] - 1;
		if (board_equal(&node->board, unique)) return node;
	}
	return NULL;
}

static PlanNode* plan_node_add(BookPlan *plan, const PlanNode *node)
{
	if (plan->n_node == plan->node_size) {
		const int size = plan->node_size * 2 + 64;
		PlanNode *n = (PlanNode*) realloc(plan->node, size * sizeof *n);
		if (n == NULL) { plan->failed = true; return NULL; }
		plan->node = n; plan->node_size = size;
	}
	plan->node[plan->n_node] = *node;
	if (!plan_index_add(plan, &plan->node_index, &plan->node_mask, plan->n_node + 1, false)) { plan->failed = true; return NULL; }
	return plan->node + plan->n_node++;
}

static PlanJob* plan_job_find(const BookPlan *plan, const Board *board, const unsigned long long links, const bool pass, const int level)
{
	unsigned int j;

	if (plan->job_index == NULL) return NULL;
	for (j = plan_slot(board, plan->job_mask); plan->job_index[j]; j = (j + 1) & plan->job_mask) {
		PlanJob *job = plan->job + plan->job_index[j] - 1;
		if (board_equal(&job->board, board) && job->links == links && job->pass == pass && job->level == level) return job;
	}
	return NULL;
}

static void plan_job_add(BookPlan *plan, const Board *board, const unsigned long long links, const bool pass, const int level)
{
	PlanJob *job;

	if (plan_job_find(plan, board, links, pass, level)) return;
	if (plan->n_job == plan->job_size) {
		const int size = plan->job_size * 2 + 64;
		job = (PlanJob*) realloc(plan->job, size * sizeof *job);
		if (job == NULL) { plan->failed = true; return; }
		plan->job = job; plan->job_size = size;
	}
	job = plan->job + plan->n_job;
	job->board = *board;
	job->links = links;
	job->leaf = BAD_LINK;
	job->level = (unsigned char) level;
	job->pass = pass;
	job->done = false;
	if (!plan_index_add(plan, &plan->job_index, &plan->job_mask, plan->n_job + 1, true)) { plan->failed = true; return; }
	++plan->n_job;
}

/** @return true if a board is in the book, or will be when the boards already planned are added. */
static bool plan_has_board(const BookPlan *plan, const Book *book, const Board *board)
{
	Board unique;

	board_unique(board, &unique);
	return plan_node_find(plan, &unique) != NULL || position_array_probe(book_array(book, &unique), &unique) != NULL;
}

/**
 * @brief Moves of a board that position_link() would link.
 * @param pass Set to true if the pass would be linked.
 * @return the moves, as a bitboard.
 */
static unsigned long long plan_links(const BookPlan *plan, const Book *book, const Board *board, bool *pass)
{
	unsigned long long moves = board_get_moves(board), links = 0;
	Board next;
	int x;

	*pass = false;
	if (moves) {
		foreach_bit(x, moves) {
			board_next(board, x, &next);
			if (plan_has_board(plan, book, &next)) links |= x_to_bit(x);
		}
	} else if (can_move(board->opponent, board->player)) {
		next.player = board->opponent;
		next.opponent = board->player;
		*pass = plan_has_board(plan, book, &next);
	}
	return links;
}

/**
 * @brief Start a plan: the searches of the next calls to book_add_board() are done ahead, at the same time.
 *
 * Call book_plan_board() with the same boards, in the same order, as book_add_board() will get,
 * then book_plan_search(), then book_add_board() for each board, then book_plan_end().
 *
 * @param book Opening book.
 * @return false if it cannot be allocated (book_add_board() searches as usual).
 */
bool book_plan_begin(Book *book)
{
	BookPlan *plan = (BookPlan*) calloc(1, sizeof *plan);

	if (plan == NULL) return false;
	plan->book = book;
	lock_init(plan);
	book_plan = plan;
	return true;
}

/**
 * @brief Plan the search that book_add_board() will do for a board.
 *
 * @param book Opening book (not changed).
 * @param board Board that book_add_board() will get.
 */
void book_plan_board(Book *book, const Board *board)
{
	BookPlan *plan = book_plan;
	Board unique;
	PlanNode *node, new_node;
	unsigned long long links;
	int n_moves, n_link;
	bool pass;

	if (plan == NULL || plan->failed) return;
	// (book play <n> links positions beyond the depth of the book)
	if (!plan->isolate && board_count_empties(board) < book->options.n_empties - 1) return;
	board_unique(board, &unique);
	node = plan_node_find(plan, &unique);
	if (node == NULL) {
		const Position *p = position_array_probe(book_array(book, &unique), &unique);

		new_node.board = unique;
		if (p == NULL) { // a new position: linked and searched as it is given, then added
			links = plan_links(plan, book, board, &pass);
			n_link = bit_count(links) + pass;
			n_moves = get_mobility(board->player, board->opponent);
			if (leaf_search_needed(n_link, n_moves, true)) plan_job_add(plan, board, links, pass, book->options.level); // (a new position has no score)
			new_node.links = plan_links(plan, book, &unique, &new_node.pass);
			new_node.leaf = PLAN_LEAF_UNKNOWN;
			new_node.level = (unsigned char) book->options.level;
			new_node.no_score = false;
			plan_node_add(plan, &new_node);
			return;
		}
		new_node.links = position_link_set(p, &new_node.pass);
		new_node.leaf = p->leaf.move;
		new_node.level = p->level;
		new_node.no_score = (p->score.value == -SCORE_INF);
		node = plan_node_add(plan, &new_node);
		if (node == NULL) return;
	}

	// a position of the book: linked, then searched if its leaf is (now) a link
	links = plan_links(plan, book, &unique, &pass);
	if (node->leaf <= H8 ? (links & ~node->links & x_to_bit(node->leaf)) != 0 : (node->leaf == PASS && pass && !node->pass)) node->leaf = NOMOVE;
	// the leaf that a planned search will find is not known yet: if the position gets a new link,
	// it may be that leaf. Search it with the new links as well, in case it is (else this search is not used)
	if (node->leaf == PLAN_LEAF_UNKNOWN && ((links & ~node->links) != 0 || (pass && !node->pass))) node->leaf = NOMOVE;
	node->links |= links;
	node->pass = node->pass || pass;
	if (node->leaf == NOMOVE) {
		n_link = bit_count(node->links) + node->pass;
		n_moves = get_mobility(unique.player, unique.opponent);
		if (leaf_search_needed(n_link, n_moves, node->no_score)) {
			plan_job_add(plan, &unique, node->links, node->pass, node->level);
			node->leaf = PLAN_LEAF_UNKNOWN;
			node->no_score = false;
		}
	}
}

/**
 * Give the threads of the workers that have no job to the searches that are still running.
 *
 * Called with the lock of the plan, when there is no search left to start. A search that can get
 * at least twice its threads is stopped: its worker runs it again with more threads and the same
 * hash tables, which give back what was already searched. A level 24 search that takes 25 s with
 * one thread no longer keeps 31 threads idle until it ends.
 */
static void plan_share_threads(BookPlan *plan)
{
#ifndef BOOK_TEST_ONE_THREAD
	int i, n_busy = 0, n;

	if (options.book_reproducible) return; // (one-thread searches)
	for (i = 0; i < plan->n_worker; ++i) if (plan->worker[i].busy) ++n_busy;
	if (n_busy == 0) return;
	n = MIN(plan->n_threads / n_busy, MAX_THREADS - 1);
	for (i = 0; i < plan->n_worker; ++i) {
		PlanWorker *w = plan->worker + i;
		if (w->busy && n >= 2 * w->want) w->want = n;
		// (also asked again when a first request came before the search started)
		if (w->busy && w->run_tasks > 0 && w->run_tasks < w->want) search_stop_all(w->search, STOP_ON_DEMAND);
	}
#else
	(void) plan; // test builds: every search has one thread, to compare with the searches done one after the other
#endif
}

/** Thread doing the planned searches. */
static void* plan_worker_run(void *v)
{
	PlanWorker *w = (PlanWorker*) v;
	BookPlan *plan = w->plan;
	long long next = real_clock() + 1000;

	for (;;) {
		PlanJob *job;
		Position p;
		Link link;
		unsigned long long links;
		int i, x, n, n_done;
		bool done, linked;

		lock(plan);
		i = plan->next < plan->n_job ? plan->next++ : -1;
		w->busy = (i >= 0);
		if (plan->next >= plan->n_job) plan_share_threads(plan); // no search left to start
		unlock(plan);
		if (i < 0) break;

		job = plan->job + i;
		search_cleanup(w->search); // empty hash tables: the result does not depend on the searches done before
		w->search->options.keep_date = false;
		for (;;) {
			position_init(&p);
			p.board = job->board;
			p.level = job->level;
			link.score = -SCORE_INF; // the search does not use the scores of the links
			links = job->links;
			linked = true;
			foreach_bit(x, links) {
				link.move = (unsigned char) x;
				if (position_add_link(&p, &link) == LINK_FAILED) linked = false;
			}
			if (job->pass) {
				link.move = PASS;
				if (position_add_link(&p, &link) == LINK_FAILED) linked = false;
			}
			if (!linked) { // no memory: a search would not exclude every linked move. The job is not done:
				done = false; // the position is searched when it is added (position_search_planned)
				break;
			}

			for (;;) { // the threads of this search
				lock(plan);
				n = w->want;
				if (n == w->n_tasks) w->run_tasks = n; // (a stop is only asked while run_tasks is set)
				unlock(plan);
				if (n == w->n_tasks) break;
				search_set_task_number(w->search, n);
				w->n_tasks = n;
			}
			done = (position_search_with(&p, w->search) & LEAF_SEARCHED) != 0;
			lock(plan);
			w->run_tasks = 0;
#ifdef BOOK_TEST_STORE
			if (done && w->search->stop == STOP_ON_DEMAND) ++plan->n_continued;
#endif
			unlock(plan);
			if (!done || w->search->stop != STOP_ON_DEMAND) break; // the search ended by itself
			// stopped to get more threads: search again, with what the hash tables kept
			position_free(&p);
			w->search->options.keep_date = true;
		}
		w->search->options.keep_date = false;
		if (done) {
			job->leaf = p.leaf;
			job->done = true;
		}
		position_free(&p);

		lock(plan);
		n_done = ++plan->n_done;
		unlock(plan);
		if (w->progress && book_progress_due(&next)) bprint("Searching positions...%d/%d\r", n_done, plan->n_job);
	}
	return NULL;
}

/**
 * @brief Size of the hash tables of a planned search that starts with several threads.
 *
 * The size of a one-thread search (book_store_hash_bits), doubled each time the threads are: the
 * searches done at the same time never take more memory together than as many one-thread searches
 * as threads. It is never more than the size that hash-table-size gives to a search with these threads.
 *
 * @param book Opening book.
 * @param n_tasks Threads of the search.
 * @return size (in number of bits).
 */
static int book_plan_hash_bits(const Book *book, const int n_tasks)
{
	int bits = book_store_hash_bits(book, 1), n;

	if (n_tasks > 1) {
		for (n = 2; n <= n_tasks; n *= 2) ++bits;
		bits = MIN(bits, book_store_hash_bits(book, n_tasks));
	}
	return bits;
}

/**
 * @brief Do the planned searches at the same time.
 *
 * The threads (n-tasks) are shared between the searches:
 * - as many searches as threads, or more: one thread each;
 * - fewer searches: each one starts with n-tasks / searches threads;
 * - when no search is left to start, the searches that are still running get the threads of the
 *   ones that ended (plan_share_threads).
 * A one-thread search only depends on its position: it starts with empty hash tables. A search with
 * several threads can give another leaf from a run to the next, as any search with several threads.
 * The searches of the pool are created while there are searches left to start: a few short searches
 * do not pay for the memory of as many searches as threads.
 * A single search is not done here: it is done as usual when its position is added (all the threads,
 * the hash tables of the main search), exactly as with book-store-tasks = 1 (except in an isolated plan: it
 * is then searched here as the others, so that a book does not depend on the number of threads).
 *
 * @param book Opening book (not changed).
 */
void book_plan_search(Book *book)
{
	BookPlan *plan = book_plan;
	PlanWorker w[MAX_THREADS];
	Search *search;
	int i, n, n_tasks, bits;
	bool self = false; // the last worker has no thread: this thread did its searches

	if (plan == NULL || plan->failed || plan->n_job == 0) return;
#ifndef BOOK_TEST_ONE_THREAD
	if (plan->n_job == 1 && !plan->isolate) { plan->usual = true; return; }
#endif
	plan->n_threads = book_store_thread_count();
	n = MIN(plan->n_threads, plan->n_job);
	if (plan->max_workers > 0) n = MIN(n, plan->max_workers);
	n_tasks = MIN(plan->n_threads / n, MAX_THREADS - 1);
#ifdef BOOK_TEST_ONE_THREAD
	n_tasks = 1;
#endif
	if (options.book_reproducible) n_tasks = 1;
	bits = book_plan_hash_bits(book, n_tasks);

	bprint("Searching positions...\r");
	plan->worker = w;
	plan->n_worker = 0;
	for (i = 0; i < n; ++i) {
		bool left;
		if (search_pool_get(store_pool, i + 1, 1, bits) <= i) { // not enough memory
			if (i == 0) { // no search at all: the positions are searched as usual, when they are added
				warn("not enough memory to search the positions at the same time\n");
				plan->worker = NULL;
				return;
			}
			break; // the workers already started do all the searches
		}
		search = store_pool->search[i];
		w[i].plan = plan;
		w[i].search = search;
		w[i].progress = (i == 0);
		w[i].n_tasks = search_count_tasks(search);
		w[i].run_tasks = 0;
		w[i].busy = true; // until it finds no job
		search->options.verbosity = 0;
		lock(plan);
		w[i].want = n_tasks;
		plan->n_worker = i + 1;
		left = (plan->next < plan->n_job);
		unlock(plan);
		if (!thread_create(&w[i].thread, plan_worker_run, w + i)) { // no thread (memory exhausted):
			plan_worker_run(w + i); // this thread does the searches of this worker, and no other worker is started
			self = true;
			break;
		}
		if (!left) break; // the searches are short: no more worker is needed
	}
	n = plan->n_worker;
	for (i = 0; i < (self ? n - 1 : n); ++i) thread_join(w[i].thread);
	for (i = 0; i < n; ++i) { // back to the state of the pool
		if (w[i].n_tasks != 1) search_set_task_number(w[i].search, 1);
		w[i].search->options.keep_date = false;
	}
	plan->worker = NULL;
	plan->n_worker = 0;
#ifdef BOOK_TEST_STORE
	fprintf(stderr, "<plan search: %d searches, %d workers, %d threads each at first, %d continued with more threads>\n", plan->n_job, n, n_tasks, plan->n_continued);
#endif
	bprint("Searching positions...%d done\n", plan->n_job);
}

/**
 * @brief Search a position as position_search_with() does, with the result of the planned search.
 *
 * @param position Position to search.
 * @param book Opening book.
 * @return LEAF_LINKED if the leaf became a link, | LEAF_SEARCHED if a search was done (or its result taken
 *         from the plan); LEAF_NOT_LINKED if the leaf could not become a link (no memory: nothing was done).
 */
static int position_search_planned(Position *position, Book *book)
{
	BookPlan *plan = book_plan;
	const int n_moves = get_mobility(position->board.player, position->board.opponent);
	int r = 0;

	if (position->leaf.move != NOMOVE) {
		const int added = position_add_link(position, &position->leaf);
		if (added == LINK_FAILED) return LEAF_NOT_LINKED; // the leaf is kept: a new search would replace it
		if (added == LINK_ADDED) r = LEAF_LINKED;
	}

	if (leaf_search_needed(position->n_link, n_moves, position->score.value == -SCORE_INF)) {
		bool pass;
		const unsigned long long links = position_link_set(position, &pass);
		const PlanJob *job = (plan->book == book) ? plan_job_find(plan, &position->board, links, pass, position->level) : NULL;

		if (job && job->done) {
			position->leaf = job->leaf;
			if (position->leaf.score > position->score.value) {
				position->score.value = position->leaf.score;
			}
			++plan->n_used;
			r |= LEAF_SEARCHED;
		} else if (job && plan->usual) { // the single search of the plan
			++plan->n_used;
			r |= position_search_with(position, book_search(book));
		} else { // not planned: search now
			++plan->n_missed;
			if (plan->isolate) search_cleanup(book_search(book)); // (as the planned searches)
#ifdef BOOK_TEST_STORE
			fprintf(stderr, "<not planned: %016llx %016llx links %016llx pass %d level %d n_link %d n_moves %d leaf %d empties %d>\n", position->board.player, position->board.opponent, links, pass, position->level, position->n_link, n_moves, position->leaf.move, board_count_empties(&position->board));
#endif
			r |= position_search_with(position, book_search(book));
		}
	}
	return r;
}

/**
 * @brief End a plan: book_add_board() searches as usual again.
 *
 * @param book Opening book.
 */
void book_plan_end(Book *book)
{
	BookPlan *plan = book_plan;

	if (plan == NULL) return;
#ifdef BOOK_TEST_STORE
	fprintf(stderr, "<book plan: %d searches planned, %d used, %d not planned>\n", plan->n_job, plan->n_used, plan->n_missed);
#endif
	if (plan->n_missed) {
		info("<book plan: %d searches planned, %d used, %d not planned>\n", plan->n_job, plan->n_used, plan->n_missed);
	}
	book_plan = NULL;
	lock_free(plan);
	free(plan->job); free(plan->node); free(plan->job_index); free(plan->node_index);
	free(plan);
	(void) book;
}

/**
 * @brief Add a position.
 *
 * @param book opening book.
 * @param board position to add.
 */
void book_add_board(Book *book, const Board *board)
{
	Position position;
	Position *probe;

	if (board_count_empties(board) >= book->options.n_empties - 1) {
		probe = book_probe(book, board);
		if (probe) {
			position_link(probe, book);
			if (probe->leaf.move == NOMOVE) position_search(probe, book);
			if (BOOK_DEBUG) {printf("update: "); position_print(probe, board, stdout);}
		} else {
			position_init(&position);
			position.board = *board;
			position.level = book->options.level;
			position_link(&position, book);
			position_search(&position, book);
			if (BOOK_DEBUG) {printf("new: "); position_print(&position, board, stdout);}
			position_unique(&position);
			if (book_add(book, &position) <= 0) position_free(&position);
		}
	}
}

/**
 * @brief Add positions from a game.
 *
 * @param book opening book.
 * @param game game to add.
 */
void book_add_game(Book *book, const Game *game)
{
	char file[FILENAME_MAX + 1];
	const long long n_stats = book->stats.n_nodes + book->stats.n_links;

	file_add_ext(options.book_file, ".gam", file);

	if (!book_game_boards(book, game, false)) return; // skip non standard game

	if (book->stats.n_nodes + book->stats.n_links > n_stats && book_get_age(book) > 3600) book_save_progress(book, file);
}

/**
 * @brief Add the positions of a game, or plan their searches (see book_plan_begin).
 *
 * @param book opening book.
 * @param game game to add.
 * @param plan Only plan the searches.
 * @return false if the game is skipped (it does not start from the standard position).
 */
static bool book_game_boards(Book *book, const Game *game, const bool plan)
{
	Board board;
	Move stack[128]; // (60 moves and at most one pass before each of them)
	int i, n_moves;

	board_init(&board);
	if (!board_equal(&board, &game->initial_board)) return false; // skip non standard game
	for (i = n_moves = 0; i < 60 && i < 60 - book->options.n_empties && game->move[i] != NOMOVE; ++i) {
		if (!can_move(board.player, board.opponent)) {
			stack[n_moves++] = MOVE_PASS;
			board_pass(&board);
		}
		if (!board_is_occupied(&board, game->move[i]) && board_get_move_flip(&board, game->move[i], &stack[n_moves])) {
			board_update(&board, stack + n_moves);
			++n_moves;
		} else {
			if (!plan) warn("illegal move in game");
			break; // stop, illegal moves
		}
	}

	if (!plan) search_cleanup(book_search(book));
	while (--n_moves >= 0) {
		if (plan) book_plan_board(book, &board);
		else book_add_board(book, &board);
		board_restore(&board, stack + n_moves);
	}
	return true;
}

/**
 * @brief Add positions from a game database.
 *
 * @param book opening book.
 * @param base games to add.
 */
void book_add_base(Book *book, const Base *base)
{
	int i, j;
	char file[FILENAME_MAX + 1];
	long long t0, t;
	const int n_tasks = book_store_task_count();

	file_add_ext(options.book_file, ".gam", file);

	book_clean(book);
	bprint("Adding %d games to book...\n", base->n_games);
	t0 = real_clock();
	for (i = 0; i < base->n_games; ++i) {
		if (n_tasks > 1 && i % n_tasks == 0) { // book-store-tasks: the searches of the next n_tasks games are done ahead, at the same time
			book_plan_end(book);
			if (book_plan_begin(book)) {
				for (j = i; j < base->n_games && j < i + n_tasks; ++j) book_game_boards(book, base->game + j, true);
				book_plan_search(book);
			}
		}
		book_add_game(book, base->game + i);
		t = real_clock();
		if (t - t0 > 1000) {
		    bprint("Adding games...%d/%d done: %lld positions, %lld links\r", i + 1, base->n_games, book->stats.n_nodes, book->stats.n_links);
			t0 = t;
		}
		if (book->search->options.verbosity) putchar('\n');
		
	}
	book_plan_end(book);
	bprint("Adding games...%d/%d done: %lld positions, %lld links\n", i, base->n_games, book->stats.n_nodes, book->stats.n_links);
	bprint("%d games added to book\n", i);

	book_save_progress(book, file);
}

typedef struct BookCheckGame {
	unsigned long long missing;
	unsigned long long good;
	unsigned long long bad;
} BookCheckGame;

/**
 * @brief Check positions from a game.
 *
 * @param book opening book.
 * @param hash Board + Move hash table.
 * @param game game to check.
 * @param stat Count statistics.
 */
void book_check_game(Book *book, MoveHash *hash, const Game *game, BookCheckGame *stat)
{
	Board board;
	Move stack[128], *iter; // (60 moves and at most one pass before each of them)
	MoveList movelist;
	int i, n_moves;
	int bestscore;

	board_init(&board);
	if (!board_equal(&board, &game->initial_board)) return; // skip non standard game
	for (i = n_moves = 0; i < 60 && i <= 60 - book->options.n_empties && game->move[i] != NOMOVE; ++i) {
		if (!can_move(board.player, board.opponent)) {
			stack[n_moves++] = MOVE_PASS;
			board_pass(&board);
		}
		if (!board_is_occupied(&board, game->move[i]) && board_get_move_flip(&board, game->move[i], &stack[n_moves])) {
			board_update(&board, stack + n_moves);
			++n_moves;
		} else {
			warn("illegal move in game");
			break; // stop, illegal moves
		}
	}

	while (--n_moves >= 0) {
		board_restore(&board, stack + n_moves);
		if (movehash_append(hash, &board, stack[n_moves].x)) {
			if (book_get_moves(book, &board, &movelist) && movelist.n_moves > 0) { // (a position without any move: missing)
				bestscore = movelist_first(&movelist)->score;
				foreach_move(iter, movelist) {
					if (iter->x == stack[n_moves].x) {
						if (iter->score < bestscore) ++stat->bad;
						else ++stat->good;
						break;
					}
				}
			} else ++stat->missing;
		}
	}
}

/**
 * @brief Check positions from a game database.
 *
 * @param book opening book.
 * @param base games to add.
 */
void book_check_base(Book *book, const Base *base)
{
	int i;
	BookCheckGame stat = {0, 0, 0};
	MoveHash hash;

	bprint("Checking %d games to book...\n", base->n_games);
	movehash_init(&hash, options.hash_table_size);
	for (i = 0; i < base->n_games; ++i) {
		book_check_game(book, &hash, base->game + i, &stat);
	}
	movehash_delete(&hash);
    bprint("Positions : %llu missing, %llu good, %llu bad (%.2f%% bad)\n", stat.missing, stat.good, stat.bad, stat.bad + stat.good ? (100.0 * stat.bad)/(stat.bad + stat.good) : 0.0); // (no move found in the book: 0%, not 0/0)
}


/**
 * @brief Extract book lines to a game base 
 *
 * Recursively add move to a PV until no move are available, 
 * where we dump the PV to a game data base.
 *
 * @param book Opening book.
 * @param board Starting position.
 * @param pv Previous moves leading to the starting  position.
 * @param base game database.
 */
static void extract_skeleton(Book *book, Board *board, Line *pv, Base *base)
{
	MoveList movelist;
	Move *move;
	Board init;
	Game game;
	int bestscore;
	Position *p = book_probe(book, board);
	const bool joins = (p && position_is_done(p, book)); // another line went on from here: this one ends here

	if (p && !joins) position_set_done(p, book);
	// a position of the book without any move (the end of a game) ends a line as a position out of the book
	// (up to v4.5.5-nikque.15 the best of its empty list of moves was read: a null pointer)
	if (!joins && book_get_moves(book, board, &movelist) && movelist.n_moves > 0) {
		bestscore = movelist_best(&movelist)->score;
		
		foreach_move(move, movelist) {
			if (move->score == bestscore) {
				if (!line_push(pv, move->x)) continue; // a full line (the links of a damaged book may go round)
				board_update(board, move);
					extract_skeleton(book, board, pv, base);
				board_restore(board, move); line_pop(pv);
			}
		}
	} else if (pv->n_moves) {
		board_init(&init);
		line_to_game(&init, pv, &game);
		if (!base_append(base, &game)) book->failed = true; // (the games are not saved then: see the callers)
		if (base->n_games % 1000 == 0) {
			bprint("extracting %d games\r", base->n_games);
		}
	}
}

/**
 * @brief Extract book draws to a game base 
 *
 * This function supposes that f5d6c4 & f5f6e6f4 are draws and the only draws, excluding the transpositions
 * f5d6c3d3c4 & f5f6e6c6 & c4..., d3..., e6...
 *
 * @param book Opening book.
 * @param base game database.
 */
void book_extract_skeleton(Book *book, Base *base)
{
	Line pv;
	Board board;

	book_clean(book); // (done: a position whose line is written. Without this, each join wrote the rest again)
	line_init(&pv, BLACK);
	line_push(&pv, F5); line_push(&pv, D6); line_push(&pv, C4);
	board_init(&board);
	board_next(&board, F5, &board); board_next(&board, D6, &board); board_next(&board, C4, &board);
	extract_skeleton(book, &board, &pv, base);
	
	line_init(&pv, BLACK);
	line_push(&pv, F5); line_push(&pv, F6); line_push(&pv, E6); line_push(&pv, F4);
	board_init(&board);
	board_next(&board, F5, &board); board_next(&board, F6, &board);
	board_next(&board, E6, &board); board_next(&board, F4, &board);	
	extract_skeleton(book, &board, &pv, base);
	bprint("%d games extracted   \n", base->n_games);
}


/** the walk of book extract all */
typedef struct ExtractAll {
	Book *book;
	BaseWriter *writer;
	Board init;
	Line pv;
	long long n_lines;
	bool ok;
} ExtractAll;

/**
 * @brief Follow the moves of the book from a board, and write a game where a line ends.
 *
 * A line ends where it leaves the book (after the leaf of a position, which is written), at a position without
 * any move (the end of a game: Edax 4.7 wrote nothing there), or at a position that another line went through.
 *
 * @param x Walk.
 * @param board Board.
 */
static void extract_all(ExtractAll *x, const Board *board)
{
	Position *position;

	if (!x->ok) return;
	position = book_probe(x->book, board);
	if (position && !position_is_done(position, x->book)) {
		MoveList movelist;
		Move *move;

		position_set_done(position, x->book);
		position_get_moves(position, board, &movelist);
		if (movelist.n_moves > 0) {
			bool full = false;

			foreach_move(move, movelist) {
				Board next = *board;

				if (!line_push(&x->pv, move->x)) { // a full line (the links of a damaged book may go round)
					full = true;
					break;
				}
				board_update(&next, move);
				extract_all(x, &next);
				line_pop(&x->pv);
			}
			if (!full) return;
		}
	}
	{
		Game game;

		line_to_game(&x->init, &x->pv, &game);
		if (!base_writer_add(x->writer, &game)) x->ok = false;
		else if (++x->n_lines % 1000 == 0) bprint("extracting %lld games\r", x->n_lines);
	}
}

/**
 * @brief Extract all the lines of a book to a game file (book extract all; book extract of Edax 4.7).
 *
 * The lines start by f5 (the others are their symmetries). They are written one by one, after the games that
 * the file holds, to another file that replaces it at the end.
 *
 * @param book Opening book.
 * @param file Game filename.
 * @return true if the lines were written.
 */
bool book_extract_all(Book *book, const char *file)
{
	ExtractAll x;
	Board board;

	x.book = book;
	x.n_lines = 0;
	x.ok = true;
	board_init(&x.init);
	board_next(&x.init, F5, &board);
	if (book_probe(book, &board) == NULL) {
		warn("book extract all: the book has no position after f5: 0 games, %s is left as it is\n", file);
		return false;
	}
	x.writer = base_writer_open(file);
	if (x.writer == NULL) return false;
	line_init(&x.pv, BLACK);
	line_push(&x.pv, F5);
	book_clean(book);
	extract_all(&x, &board);
	if (!x.ok) {
		warn("book extract all: %s is left as it was\n", file);
		base_writer_abort(x.writer);
		return false;
	}
	if (!base_writer_close(x.writer)) return false;
	bprint("%lld games extracted   \n", x.n_lines);
	return true;
}

/**
 * @brief print a set of position.
 *
 * @param book Opening book.
 * @param n_empties Game stage.
 * @param n_positions Number of positions to extract.
 */
void book_extract_positions(Book *book, const int n_empties, const int n_positions)
{
	PositionArray *a;
	Position *p;
	MoveList movelist;
	Move *best, *second_best;
	int i = 0;
	char s[80];

	bprint("Extracting %d positions at %d ...\n", n_positions, n_empties); 
	foreach_position(p, a, book) {
		if (i == n_positions) break;
		if (board_count_empties(&p->board) == n_empties) {
			position_get_moves(p, &p->board, &movelist);
			best = movelist_first(&movelist);
			if (best) {
				second_best = move_next(best);
				if (second_best && best->score > second_best->score) {
					++i;
					board_to_string(&p->board, n_empties & 1, s);
					bprint("%s %% bm ", s);
					move_print(best->x, n_empties & 1, stdout);
					bprint(":%+2d; ba ", best->score);
					move_print(second_best->x, n_empties & 1, stdout);
					bprint(":%+2d;\n", second_best->score);
				}
			}
		}
	}
}

/**
 * @brief print book statistics.
 *
 * @param book Opening book.
 */
void book_stats(Book *book)
{
	PositionArray *a;
	Position *p;
	int i;
	unsigned long long n_hash[256];
	unsigned long long n_pos[61], n_leaf[61], n_link[61], n_terminal[61];
	unsigned long long n_score[129];
	unsigned long long n_bad_stage, n_bad_score;

	printf("\n\nBook statistics:\n");

	printf("\nHash distribution:\n");
	for (i = 0; i < 256; ++i) n_hash[i] = 0;
	for (a = book->array; a < book->array + book->n; ++a) {
		if (a->n < 256) ++n_hash[a->n];
		else ++n_hash[255];
	}
	printf("index    positions\n");
	for (i = 0; i < 255; ++i) if (n_hash[i]) printf("%5d %12llu\n", i, n_hash[i]);
	if (n_hash[i]) printf(">%4d %12llu\n", i - 1, n_hash[i]);

	printf("\nStage distribution:\n");
	printf("stage    positions        links       leaves      terminal nodes\n");
	for (i = 0; i < 61; ++i) n_pos[i] = n_leaf[i] = n_link[i] = n_terminal[i] = 0;
	// a damaged book may hold boards with more than 60 empties, and scores out of -64..64 (up to +/-127,
	// what the book file can hold): they are counted apart (up to v4.5.5-nikque.15 they were counted out of
	// the tables)
	n_bad_stage = n_bad_score = 0;
	foreach_position(p, a, book) {
		i = board_count_empties(&p->board);
		if (i > 60) { ++n_bad_stage; continue; }
		++n_pos[i];
		if (p->leaf.move != NOMOVE) ++n_leaf[i];
		if (p->n_link == 0) ++n_terminal[i];
		n_link[i] += p->n_link;
	}
	for (i = 0; i < 61; ++i) if (n_pos[i]) printf("%5d %12llu %12llu %12llu %12llu\n", i, n_pos[i], n_link[i], n_leaf[i], n_terminal[i]);
	if (n_bad_stage) printf("more than 60 empties (damaged): %llu positions\n", n_bad_stage);
		
	printf("\nBest Score Distribution:\n");
	printf("Score    positions\n");
	for (i = 0; i < 129; ++i) n_score[i] = 0;
	foreach_position(p, a, book) {
		if (p->score.value < SCORE_MIN || p->score.value > SCORE_MAX) ++n_bad_score;
		else ++n_score[64 + p->score.value];
	}
	for (i = 0; i < 129; ++i) if (n_score[i]) printf("%+5d %12llu\n", i - 64, n_score[i]);
	if (n_bad_score) printf("out of -64..+64: %llu positions\n", n_bad_score);
	printf("\n\n");
	fflush(stdout);
}

/**
 * @brief feed hash table from the opening book.
 * 
 * @param book Opening book.
 * @param board Position to start from.
 * @param search HashTables container.
 */
void book_feed_hash(const Book *book, Board *board, Search *search)
{
	board_feed_hash(board, book, search, true);
}

/*
 * book fill and book fill2, without or with loss limits; book leaf expand.
 *
 * book fill <n> looks at every move of every position: the positions of a line of <n> moves that ends
 * in the book are added. book fill2 <n> only follows the leaf of a position: the position after the leaf
 * move is added when a position <n> - 1 moves further is in the book (with the positions between
 * them). The positions added in a round are filled from in the next round. With a loss limit for one
 * move and a cumulative loss limit (as book deviate3), the positions filled from are those of the walk
 * of book deviate3 from the current position: all of them (book fill), or those whose leaf book
 * deviate3 would expand (book fill2). The leaves of the solved positions are followed too.
 *
 * When positions are added from a position, the position gets its links to them at once, and when
 * its leaf became a link it is searched again for a new leaf, as when book deviate expands a leaf.
 * Every round first links the positions with a move that leads to a position of the book but is not a
 * link (book_link_moves), so that the commands end with no such move (book fix links them too, but does
 * not fill nor expand). Up to v4.5.5-nikque.13, book fill linked them too: every position next to a
 * position filled from got its missing links (book_add_board).
 *
 * As book deviate, the positions to fill from are found by several threads (the walk of book
 * deviate3, or a scan of the book by bucket ranges), and only those from which a position would be
 * added are kept. With book-expand-tasks other than 1 (as book deviate), the searches of a batch of these
 * positions are planned and done at the same time (as book store: see book_plan_begin), then the
 * positions are added one after the other, in the same order as without a plan, with the results of the
 * planned searches. Up to v4.5.5-nikque.13, book fill used one thread, filled at once from the positions
 * added in the same round, and did not link the position filled from (BOOK_TEST_FILL_OLD: that way).
 * book fill 1 adds no position and keeps that way: the positions one move after the positions of the
 * book get their missing links. book fill2 1 and the limited commands with a depth of 1 only tell that
 * there is nothing to fill.
 *
 * book leaf expand: a position whose leaf (its best move that is not a link yet) has a better score than
 * all its links has its best move out of the book. book leaf expand expands these leaves as book deviate
 * expands a leaf (book_expand, with book-expand-tasks): the position after the leaf move is added and
 * searched, and the position is searched again for its next leaf. This is done again until no such leaf
 * is left (the next leaf may be better than the links again). Without limits: every position of the book
 * that is not under its depth. With a per-move and a cumulative loss limit: the positions of the walk of
 * book deviate3 from the current position (the leaves of the solved positions too). Every round first
 * links the positions whose leaf leads to a position of the book (all the missing links of these positions;
 * the other positions are linked by book fix).
 *
 * The positions of every command are found without any copy of the book: by several threads that scan
 * the book by bucket ranges (book_pick_scan), or by the walk of book deviate3, whose marks are then kept
 * for the positions looked for only (book_pick_todo). They are kept by their place in the book
 * (bucket and index, see position_key), as the todo list of book deviate.
 */

/** planned searches for each thread in a batch of book fill / fill2 */
#ifndef BOOK_FILL_PLAN_SEARCHES
#define BOOK_FILL_PLAN_SEARCHES 64
#endif

static bool board_fill_probe(const Board*, const Book*, const int, const bool);

/**
 * @brief The next board of a line that board_fill_probe() follows.
 *
 * @param next Board after the move.
 * @param book Opening book.
 * @param depth Moves left before the move.
 * @param missing A position of the line before this board is not in the book.
 * @return true if a position would be added.
 */
static bool board_fill_probe_next(const Board *next, const Book *book, const int depth, const bool missing)
{
	if (depth > 1 && book_probe(book, next) == NULL) { // (a position of the line, not in the book)
		if (board_count_empties(next) < book->options.n_empties - 1) return false; // (book_add_board() would not add it)
		return board_fill_probe(next, book, depth - 1, true);
	}
	return board_fill_probe(next, book, depth - 1, missing);
}

/**
 * @brief Tell, without changing the book, if board_fill() would add a position from a board:
 * a line of <depth> moves ends in the book and goes through positions not in the book, that can be added.
 *
 * @param board Board.
 * @param book Opening book.
 * @param depth Moves left.
 * @param missing A position of the line before this board is not in the book.
 * @return true if a position would be added.
 */
static bool board_fill_probe(const Board *board, const Book *book, const int depth, const bool missing)
{
	unsigned long long moves;
	Board next;
	int x;

	if (depth == 0) return missing && book_probe(book, board) != NULL;
	moves = board_get_moves(board);
	if (moves) {
		foreach_bit(x, moves) {
			board_next(board, x, &next);
			if (board_fill_probe_next(&next, book, depth, missing)) return true;
		}
	} else if (can_move(board->opponent, board->player)) {
		next.player = board->opponent;
		next.opponent = board->player;
		return board_fill_probe_next(&next, book, depth, missing);
	}
	return false;
}

/** @return the board after the leaf move of a position. */
static Board position_leaf_board(const Position *p)
{
	Board next;

	if (p->leaf.move == PASS) {
		next.player = p->board.opponent;
		next.opponent = p->board.player;
	} else {
		board_next(&p->board, p->leaf.move, &next);
	}
	return next;
}

/** @return true if the leaf of a position is better than every link of it (and the position can be expanded). */
static bool position_leaf_better(const Position *p, const Book *book)
{
	const Link *l;
	int best = -SCORE_INF;

	if (p->n_link == 0 || p->leaf.move == NOMOVE) return false;
	if (board_count_empties(&p->board) < book->options.n_empties || board_is_game_over(&p->board)) return false;
	foreach_link(l, p) if (l->score > best) best = l->score;
	return p->leaf.score > best;
}

/**
 * @return true if a move of a position that is not a link leads to a position of the book (position_link()
 * would link it): the moves looked at as by book_link_find_missing (book merge).
 */
static bool position_has_unlinked_child(const Position *p, const Book *book)
{
	unsigned long long moves = board_get_moves(&p->board);
	Board next;
	int x;
#ifndef BOOK_TEST_LINK_NO_PREFETCH
	ChildProbe c;
	int i;

	c.n = 0;
	if (moves) {
		foreach_bit(x, moves) {
			if (!position_has_link(p, x)) {
				board_next(&p->board, x, &next);
				child_probe_add(&c, book, &next, x);
			}
		}
	} else if (can_move(p->board.opponent, p->board.player) && !position_has_link(p, PASS)) {
		next.player = p->board.opponent;
		next.opponent = p->board.player;
		child_probe_add(&c, book, &next, PASS);
	}
	child_probe_fetch(&c);
	for (i = 0; i < c.n; ++i) if (position_array_probe(c.array[i], c.unique + i)) return true;
#else // test builds: one child after the other
	if (moves) {
		foreach_bit(x, moves) {
			if (!position_has_link(p, x)) {
				board_next(&p->board, x, &next);
				if (book_probe(book, &next)) return true;
			}
		}
	} else if (can_move(p->board.opponent, p->board.player) && !position_has_link(p, PASS)) {
		next.player = p->board.opponent;
		next.opponent = p->board.player;
		return book_probe(book, &next) != NULL;
	}
#endif
	return false;
}

/** the positions that a command looks for */
typedef struct BookPick {
	int depth;          /**< book fill / fill2: distance to fill (2 at least); 0: a position to link (see all_moves) */
	bool leaf_only;     /**< book fill2: only through the leaf */
	bool leaf_better;   /**< book leaf expand: a leaf better than every link */
	bool all_moves;     /**< with a depth of 0: every move that leads to a position of the book but is not a link (book fill, fill2); else the leaf only (book leaf expand) */
	bool no_link;       /**< book play: no link, within the depth of the book */
} BookPick;

/**
 * @brief Tell, without changing the book, if a position is one that a command looks for.
 *
 * @param p Position of the book.
 * @param book Opening book.
 * @param pick What is looked for: a position from which a fill (depth 2 at least) would add a position;
 * with a depth of 0, a position with a move that leads to a position of the book but is not a link (any move,
 * or the leaf only: to link it, and search its leaf again if the leaf becomes a link); a position whose
 * leaf is better than every link (book leaf expand); or a position without any link (book play).
 * @return true if it is.
 */
static bool position_picked(const Position *p, const Book *book, const BookPick *pick)
{
	Board next;

	if (pick->leaf_better) return position_leaf_better(p, book);
	if (pick->no_link) return p->n_link == 0 && board_count_empties(&p->board) >= book->options.n_empties && !board_is_game_over(&p->board);
	if (pick->depth == 0) {
		if (pick->all_moves) return position_has_unlinked_child(p, book);
		if (p->leaf.move == NOMOVE) return false;
		next = position_leaf_board(p);
		return book_probe(book, &next) != NULL;
	}
	if (pick->depth < 2 || board_count_empties(&p->board) < book->options.n_empties) return false;
	if (!pick->leaf_only) return board_fill_probe(&p->board, book, pick->depth, false);
	if (p->leaf.move == NOMOVE) return false;
	next = position_leaf_board(p);
	return board_fill_probe_next(&next, book, pick->depth, false);
}

/**
 * @brief Fill a book from one of its positions.
 *
 * The positions are only appended to their bucket while a book is filled: the place of a position
 * (bucket and index, see position_key) stays valid, as for the todo list of book deviate.
 *
 * @param book Opening book.
 * @param key Place of the position (see position_key).
 * @param depth Distance to fill; 0: only link the position (a move leads to a position of the book).
 * @param leaf_only Only through the leaf of the position (book fill2), else every move.
 * @return true if positions were added.
 */
static bool position_fill(Book *book, const unsigned long long key, const int depth, const bool leaf_only)
{
	const long long n_nodes = book->stats.n_nodes;
	Position *p = book_position(book, key);
	Board board;
	long long n_links;
	bool filled;
	int x;

	if (depth == 0) { // (the step that links: a move leads to a position of the book)
		filled = true;
	} else if (leaf_only) {
		if (p->leaf.move == NOMOVE) return false;
		board = position_leaf_board(p);
		filled = board_fill(&board, book, depth - 1);
		if (filled) book_add_board(book, &board);
	} else {
		board = p->board; // board_fill() changes its board, and adding positions may move p
		filled = board_fill(&board, book, depth);
	}
	// a line reaches the book: the position gets its links, even when the positions of the line were
	// added just before from another position (the same position reached by another line)
	if (!filled) return false;

	p = book_position(book, key); // (the positions of its bucket may have moved to another block)
	x = p->leaf.move;
	n_links = book->stats.n_links;
	position_link(p, book);
	if (book->stats.n_links != n_links) book->need_saving = true; // (only links may be added: the book is saved at the end)
	// the leaf is a link now (position_add_link has removed the leaf): search the next leaf, if a move is left
	if (x != NOMOVE && p->leaf.move == NOMOVE) position_search(p, book);
	return book->stats.n_nodes != n_nodes;
}

/**
 * @brief board_fill() as it will run when the boards already planned are added: plan the searches
 * of the boards that book_add_board() will get (see book_plan_board).
 */
static bool board_fill_plan(Board *board, Book *book, int depth)
{
	if (depth > 0) {
		MoveList movelist;
		Move *m;
		bool filled = false;

		movelist_get_moves(&movelist, board);
		if (movelist.n_moves == 0 && can_move(board->opponent, board->player)) {
			board_pass(board);
			if (board_fill_plan(board, book, depth - 1)) {
				book_plan_board(book, board);
				filled = true;
			}
			board_pass(board);
		} else {
			foreach_move(m, movelist) {
				board_update(board, m);
				if (board_fill_plan(board, book, depth - 1)) {
					book_plan_board(book, board);
					filled = true;
				}
				board_restore(board, m);
			}
		}
		return filled;
	}
	return plan_has_board(book_plan, book, board);
}

/** @brief position_fill() as it will run when the boards already planned are added: plan its searches. */
static void position_fill_plan(Book *book, const unsigned long long key, const int depth, const bool leaf_only)
{
	const Position *p = book_position(book, key);
	Board board;

	if (depth > 0) { // (with a depth of 0, nothing is added: the position is linked only)
		if (leaf_only) {
			if (p->leaf.move == NOMOVE) return;
			board = position_leaf_board(p);
			if (board_fill_plan(&board, book, depth - 1)) book_plan_board(book, &board);
		} else {
			board = p->board;
			board_fill_plan(&board, book, depth);
		}
	}
	book_plan_board(book, &p->board); // its new links, and the search of a new leaf if its leaf became a link
}

/**
 * @brief Fill a book from a list of its positions, in this order.
 *
 * @param book Opening book.
 * @param name Name of the command.
 * @param file File of the timed saves.
 * @param keys Places of the positions (see position_key).
 * @param n Number of positions.
 * @param depth Distance to fill; 0: link the positions only.
 * @param leaf_only Only through the leaves (book fill2).
 */
static void book_fill_keys(Book *book, const char *name, const char *file, const unsigned long long *keys, const long long n, const int depth, const bool leaf_only)
{
	// book-expand-tasks (as book deviate): 1 = one search after the other; n = n searches at the same time;
	// auto = a search for each thread (the threads of the searches that end first go to the others)
	const bool planned = (n > 1 && options.n_task > 1 && options.book_expand_tasks != 1 && !book_one_search_at_a_time());
	const int max_jobs = BOOK_FILL_PLAN_SEARCHES * book_store_thread_count();
	const long long max_positions = 16LL * max_jobs; // (the step that links: most positions need no search, but each one is planned)
	long long i = 0, j, end;
	unsigned long long t = real_clock();

	while (i < n && !book->failed) {
		end = n;
		if (planned && book_plan_begin(book)) {
			book_plan->max_workers = MAX(0, options.book_expand_tasks);
			for (j = i; j < n; ++j) {
				position_fill_plan(book, keys[j], depth, leaf_only);
				if (book_plan->failed || book_plan->n_job >= max_jobs || j + 1 - i >= max_positions) { ++j; break; }
			}
			end = j;
			book_plan_search(book);
		}
		for (j = i; j < end && !book->failed; ++j) {
			position_fill(book, keys[j], depth, leaf_only);
			if ((j & 1023) == 1023) {
				bprint("%s...%lld/%lld: %lld positions, %lld links\r", name, j + 1, n, book->stats.n_nodes, book->stats.n_links);
				book_save_timed(book, file, &t);
			}
		}
		book_plan_end(book);
		i = end;
		bprint("%s...%lld/%lld: %lld positions, %lld links\r", name, i, n, book->stats.n_nodes, book->stats.n_links);
		book_save_timed(book, file, &t);
	}
	bprint("%s...%lld/%lld: %lld positions, %lld links\n", name, i, n, book->stats.n_nodes, book->stats.n_links);
}

/** what the scan of the book looks for (book_pick_scan: one scan at a time) */
static BookPick scan_pick;

static void book_pick_scan_task(BookTask *task)
{
	Book *book = task->book;
	int b, k;

	for (b = task->first; b < task->last; ++b) {
		const PositionArray *a = book->array + b;
		for (k = 0; k < a->n; ++k) {
			if (position_picked(a->positions + k, book, &scan_pick)) book_task_push(task, TASK_ITEM(b, k, 0));
		}
		book_task_add_done(task, a->n);
	}
}

/**
 * @brief The places of all the positions of the book that a command looks for, found by several threads (by bucket ranges).
 *
 * @param book Opening book.
 * @param name Name of the command (progress).
 * @param pick What is looked for (see position_picked).
 * @param n Number of positions (out).
 * @return the places, in the order of the book (to free), or NULL if the memory is exhausted.
 */
static unsigned long long* book_pick_scan(Book *book, const char *name, const BookPick *pick, long long *n)
{
	BookTask task[MAX_THREADS];
	unsigned long long *keys;
	long long j;
	int i, n_tasks;
	bool oom = false;

	scan_pick = *pick;
	book_test_add(book_test_scans);
	n_tasks = book_parallel(book, book_pick_scan_task, task, name);
	*n = 0;
	for (i = 0; i < n_tasks; ++i) { *n += task[i].n; oom = oom || task[i].oom; }
	// the list of the first task grows to hold all the places: the lists of the other tasks are released one after the other
	keys = oom ? NULL : (unsigned long long*) realloc(task[0].item, (size_t) (*n + 1) * sizeof *keys);
	if (keys) {
		task[0].item = NULL; // (now the list returned)
		for (j = 0; j < task[0].n; ++j) keys[j] = TASK_KEY(keys[j]);
		*n = task[0].n;
		for (i = 1; i < n_tasks; ++i) {
			for (j = 0; j < task[i].n; ++j) keys[(*n)++] = TASK_KEY(task[i].item[j]);
			free(task[i].item);
			task[i].item = NULL;
		}
	}
	book_tasks_free(task, n_tasks);
	return keys;
}

/** a thread that tells which positions marked by a walk a command looks for */
typedef struct PickFilter {
	Book *book;
	unsigned long long *key;
	long long first, last;
	long long n_kept;   /**< positions kept, moved to key[first .. first + n_kept) */
	const BookPick *pick;
} PickFilter;

static void* pick_filter_run(void *v)
{
	PickFilter *f = (PickFilter*) v;
	long long i;

	long long w = f->first;

	// the positions of the range are kept in place, in the same order; the marks of the others are cleared
	// (position_picked reads no mark: the threads do not change what the other threads look at)
	for (i = f->first; i < f->last; ++i) {
		Position *p = book_position(f->book, f->key[i]);
		if (position_picked(p, f->book, f->pick)) f->key[w++] = f->key[i];
		else position_clear_todo(p);
	}
	f->n_kept = w - f->first;
	return NULL;
}

/**
 * @brief Keep the positions marked by a walk (todo) that a command looks for, in the order of the book.
 *
 * The todo list of the walk is kept for these positions only, in place: it is the list returned (no copy).
 * Without a todo list (memory exhausted during the walk), the book is scanned for the marks into a new list.
 * The marks of the other positions are cleared (book_expand reads them when there is no list).
 *
 * @param book Opening book.
 * @param pick What is looked for (see position_picked).
 * @param n Number of positions (out).
 * @param owned Set to true if the list returned is a new one, to free (out).
 * @return the places (see position_key), or NULL if the memory is exhausted.
 */
static unsigned long long* book_pick_todo(Book *book, const BookPick *pick, long long *n, bool *owned)
{
	unsigned long long *keys = NULL;

	*n = 0;
	*owned = false;
	if (book->todo_list.valid) {
		const long long n_todo = book->todo_list.n;
		PickFilter f[MAX_THREADS];
		int m = book_n_task(), k;

		if (m > MAX_THREADS) m = MAX_THREADS;
		if (m < 1 || n_todo < 4096) m = 1;
		for (k = 0; k < m; ++k) {
			f[k].book = book; f[k].key = book->todo_list.item; f[k].n_kept = 0;
			f[k].first = n_todo * k / m; f[k].last = n_todo * (k + 1) / m;
			f[k].pick = pick;
		}
		book_run_workers(pick_filter_run, f, sizeof *f, m);
		keys = book->todo_list.item;
		for (k = 0; k < m; ++k) { // the kept positions of the ranges, one after the other
			if (f[k].first != *n) memmove(keys + *n, keys + f[k].first, (size_t) f[k].n_kept * sizeof *keys);
			*n += f[k].n_kept;
		}
		// in the order of the book (sorted after the filter: only the positions kept)
		keys_sort(keys, (long long) *n);
		book->todo_list.n = *n;
		if (*n < book->todo_list.size / 4) { // most positions were left out: the memory of the list is given back while the positions are searched
			unsigned long long *item = (unsigned long long*) realloc(keys, (size_t) (*n + 1) * sizeof *keys);
			if (item) {
				book->todo_list.item = keys = item;
				book->todo_list.size = *n + 1;
			}
		}
	} else { // (no list: the whole book is scanned for the marks)
		PositionArray *a;
		Position *p;
		foreach_position(p, a, book) if (position_is_todo(p, book)) {
			if (position_picked(p, book, pick)) ++*n;
			else position_clear_todo(p);
		}
		keys = (unsigned long long*) malloc((*n + 1) * sizeof *keys);
		if (keys) {
			*n = 0;
			*owned = true;
			foreach_position(p, a, book) if (position_is_todo(p, book)) keys[(*n)++] = position_key(book, p);
		}
	}
	book->stats.n_todo = *n;
	return keys;
}

/**
 * @brief Link the positions with a move that leads to a position of the book but is not a link, and search
 * their leaf again if it became a link (the first step of every round of book fill, fill2 and leaf expand).
 *
 * Such a move leads to a position that the book holds but the position does not link: the position added
 * by another position (the same position reached by another line), by book deviate as by these commands.
 * book fix links them too, but searching a leaf again may make it better than every link: the commands
 * that fill or expand do it themselves, so that they end with neither kind of leaf. book fill and fill2 link
 * every such move (as up to v4.5.5-nikque.13, book fill linked every position next to the positions it
 * filled from); book leaf expand picks the positions whose leaf leads to a position of the book (and adds all their
 * missing links: position_link). The searches are done as those of book fill
 * (book-expand-tasks).
 *
 * @param book Opening book.
 * @param name Name of the command.
 * @param file File of the timed saves.
 * @param root Position to start the walk from (with limits).
 * @param limited With loss limits: the positions of the walk of book deviate3 only; else the whole book.
 * @param move_loss Maximum loss for one move.
 * @param total_loss Maximum cumulative loss for both players.
 * @param all_moves Every move (book fill, fill2), else the positions whose leaf leads to a position of the book (book leaf expand).
 * @return the number of links added (0: nothing was done).
 */
static long long book_link_moves(Book *book, const char *name, const char *file, Position *root, const bool limited, const int move_loss, const int total_loss, const bool all_moves)
{
	const BookPick link_pick = {0, false, false, all_moves, false};
	unsigned long long *keys;
	long long n = 0;
	bool owned = true;
	char label[64];

#ifdef BOOK_TEST_NO_LINK_LEAVES
	(void) book; (void) name; (void) file; (void) root; (void) limited; (void) move_loss; (void) total_loss; (void) all_moves;
	(void) link_pick; (void) keys; (void) n; (void) owned; (void) label;
	return 0; // test builds: this step is skipped, to time the other step of the commands alone
#endif
	if (limited) {
		book_clean(book);
		deviate_walk_all = true;
		book_select_deviate_total(book, root, move_loss, total_loss, false);
		deviate_walk_all = false;
		keys = book_pick_todo(book, &link_pick, &n, &owned);
	} else {
		keys = book_pick_scan(book, name, &link_pick, &n);
	}
	if (keys == NULL) {
		error("cannot allocate the list of the positions to link\n");
		book->failed = true; // (the command stops: not the same as nothing to link)
		return 0;
	}
	book->stats.n_nodes = book->stats.n_links = 0;
	if (n) {
		snprintf(label, sizeof label, "%s (links)", name);
		if (all_moves) bprint("%s: %lld positions have a move to a position of the book that is not a link\n", label, n);
		else bprint("%s: %lld leaves lead to a position of the book\n", label, n);
		book_fill_keys(book, label, file, keys, n, 0, false);
	}
	if (owned) free(keys);
	return book->stats.n_links; // (the links added: 0 if none could be, a link that cannot be allocated is not added)
}

/*
 * The rounds of book play, book play <n> and book extend.
 *
 * A round expands the leaves of a list of positions. A leaf that leads to a position of the book is only linked,
 * and when the leaves of several positions lead to the same position (the 8 symmetries are the same position),
 * the first one in the order of the book is expanded and the others are linked to the new position: no position
 * is searched twice. The searches of the links are planned as those of book fill, but each one starts with empty
 * hash tables, alone or not and with any number of threads (an isolated plan): with book-reproducible on, the
 * book does not depend on the number of threads.
 *
 * Only the first round looks at the whole book (or walks it from the current position): the next ones start from
 * the positions that the round before reached. The book is saved when book-save-interval is over, with one clock
 * for the command, and at the end. It is negamaxed at the end, and between two rounds before such a save (not
 * inside a round: negamax starts a new todo list, and the expansions would stop).
 */

/** the leaf of a position to expand: where it leads (unique board), and the position */
typedef struct LearnChild {
	Board board;
	unsigned long long key;    /**< place of the parent (see position_key) */
} LearnChild;

/** @brief Order of the boards (player, then opponent). */
static int learn_board_cmp(const void *a, const void *b)
{
	const Board *x = (const Board*) a, *y = (const Board*) b;

	if (x->player != y->player) return x->player < y->player ? -1 : 1;
	if (x->opponent != y->opponent) return x->opponent < y->opponent ? -1 : 1;
	return 0;
}

/** @brief Order of the children: their boards, then the order of their parents in the book. */
static int learn_child_cmp(const void *a, const void *b)
{
	const LearnChild *x = (const LearnChild*) a, *y = (const LearnChild*) b;
	const int r = learn_board_cmp(&x->board, &y->board);

	return r ? r : (x->key > y->key) - (x->key < y->key);
}

#define BOARD_LESS(a, b) ((a).player != (b).player ? (a).player < (b).player : (a).opponent < (b).opponent)
#define CHILD_LESS(a, b) (!board_equal(&(a).board, &(b).board) ? BOARD_LESS((a).board, (b).board) : (a).key < (b).key)
DEFINE_SORT(boards_sort, Board, BOARD_LESS, learn_board_cmp)
DEFINE_SORT(children_sort, LearnChild, CHILD_LESS, learn_child_cmp)

/** @brief Sort boards (player, then opponent). */
static void learn_boards_sort(Board *board, const long long n)
{
	boards_sort(board, n);
}

/** @brief Sort children (see learn_child_cmp). */
static void learn_children_sort(LearnChild *child, const long long n)
{
	children_sort(child, n);
}

/** @brief Sort places of positions in the order of the book. */
static void learn_keys_sort(unsigned long long *key, const long long n)
{
	keys_sort(key, n);
}

/*
 * book play <n> can go beyond the depth of the book, where the next run could not find the ends of its lines
 * (it only starts from positions within this depth). With each save of the book, the ends are saved to
 * <book-file>.play-to, as book learn saves its progress to <book-file>.learn: the ends at this save and at
 * the save before (the book of the file is then found by one or the other, wherever a run stops). The file
 * is written before the book. It is read by the same command from the same position (a board that is not in
 * the book in use, or that has a link, is left out), and removed when the command ends.
 */

/** the state of book play <n> */
typedef struct PlayTo {
	Board start;               /**< position to start from (unique board) */
	int n_empties;             /**< positions with as many empty squares or more are expanded */
	const unsigned long long *keys; /**< the positions of the round (NULL: between two rounds) */
	long long n_keys;
	Board *saved;              /**< the ends at the last save */
	long long n_saved;
	bool owned;                /**< the file is of this command */
	char file[FILENAME_MAX + 1];
} PlayTo;

/** one command that learns by rounds */
typedef struct LearnRun {
	Book *book;
	const char *name;          /**< name of the command (progress) */
	const char *file;          /**< file of the saves */
	LearnChild *child;         /**< children of the last round, sorted */
	long long n_child;
	long long n_diffs;         /**< positions and links added by the last round */
	long long n_total;         /**< positions and links added by the command */
	unsigned long long t;      /**< time of the last save */
	PlayTo *to;                /**< book play <n> (NULL: another command) */
} LearnRun;

/**
 * @brief Write the ends of the lines of book play <n>.
 *
 * @param run Command.
 * @param current The ends now (out: to free), NULL if the memory is exhausted.
 * @param n_current Their number (out).
 * @return true if the file was written.
 */
static bool play_to_write(LearnRun *run, Board **current, long long *n_current)
{
	PlayTo *to = run->to;
	const Book *book = run->book;
	Board *now = (Board*) malloc((size_t) (to->n_keys + run->n_child + 1) * sizeof *now);
	char tmp_file[FILENAME_MAX + 1];
	long long i, n = 0;
	FILE *f;
	bool ok = false;

	*current = now;
	*n_current = 0;
	if (now) {
		for (i = 0; i < to->n_keys; ++i) now[n++] = book_position(book, to->keys[i])->board;
		for (i = 0; i < run->n_child; ++i) {
			if (i == 0 || !board_equal(&run->child[i - 1].board, &run->child[i].board)) now[n++] = run->child[i].board;
		}
		*n_current = n;
		file_add_ext(to->file, ".tmp", tmp_file);
		if ((f = fopen(tmp_file, "w")) != NULL) {
			fprintf(f, "edax-play-to 1 %d %016llx %016llx\n", to->n_empties, to->start.player, to->start.opponent);
			for (i = 0; i < to->n_saved; ++i) fprintf(f, "%016llx %016llx\n", to->saved[i].player, to->saved[i].opponent);
			for (i = 0; i < n; ++i) fprintf(f, "%016llx %016llx\n", now[i].player, now[i].opponent);
			fprintf(f, "end %lld\n", to->n_saved + n);
			ok = !ferror(f);
			if (fclose(f) != 0) ok = false;
			if (ok) ok = file_replace(tmp_file, to->file);
			if (!ok) remove(tmp_file);
		}
	}
	if (ok) to->owned = true;
	else warn("cannot write %s: book play %d could not go on from a book saved now\n", to->file, to->n_empties);
	return ok;
}

/**
 * @brief Read the ends of the lines of a book play <n> that did not end.
 *
 * @param to State of the command (out: the ends, as if they were saved by it).
 */
static void play_to_read(PlayTo *to)
{
	FILE *f = fopen(to->file, "r");
	char line[128];
	Board start, board;
	long long size = 0, count;
	int n_empties;
	bool complete = false;

	if (f == NULL) return;
	if (fgets(line, sizeof line, f) == NULL
	 || sscanf(line, "edax-play-to 1 %d %llx %llx", &n_empties, &start.player, &start.opponent) != 3) {
		warn("%s is not a file of book play <n>: it is not used\n", to->file);
	} else if (n_empties != to->n_empties || !board_equal(&start, &to->start)) {
		warn("%s is of book play %d from another position or to other empty squares: it is not used, and replaced by the next save\n", to->file, n_empties);
	} else {
		while (fgets(line, sizeof line, f) != NULL) {
			if (sscanf(line, "end %lld", &count) == 1) {
				complete = (count == to->n_saved);
				break;
			}
			if (sscanf(line, "%llx %llx", &board.player, &board.opponent) != 2 || (board.player & board.opponent)) break;
			if (to->n_saved == size) {
				Board *p;

				size += size / 2 + 1024;
				p = (Board*) realloc(to->saved, (size_t) size * sizeof *p);
				if (p == NULL) break;
				to->saved = p;
			}
			board_unique(&board, to->saved + to->n_saved);
			++to->n_saved;
		}
		if (complete) {
			to->owned = true;
			bprint("Book play: going on from the %lld boards of %s\n", to->n_saved, to->file);
		} else {
			warn("%s could not be read completely: it is not used\n", to->file);
			to->n_saved = 0;
		}
	}
	fclose(f);
}

/**
 * @brief Save the book when the interval of the timed saves is over since the last save of the command.
 *
 * @param run Command.
 */
static void learn_save_timed(LearnRun *run)
{
	if (book_save_interval_elapsed((long long) run->t)) {
		PlayTo *to = run->to;
		Board *now = NULL;
		long long n_now = 0;
		const bool written = (to != NULL && play_to_write(run, &now, &n_now));
		const bool saved = book_save_progress(run->book, run->file);

		if (now) {
			if (written && saved) { // the book of the file has these ends
				free(to->saved);
				to->saved = now;
				to->n_saved = n_now;
			} else { // it has those of this save or of one before: all are kept
				Board *all = (Board*) realloc(to->saved, (size_t) (to->n_saved + n_now + 1) * sizeof *all);

				if (all) {
					memcpy(all + to->n_saved, now, (size_t) n_now * sizeof *all);
					to->saved = all;
					to->n_saved += n_now;
				}
				free(now);
			}
		}
#ifdef BOOK_TEST_COUNTS
		if (book_test_saved) book_test_saved(run->file);
#endif
		run->t = real_clock();
	}
}

/** @brief Start a command that learns by rounds. */
static void learn_begin(LearnRun *run, Book *book, const char *name, const char *file)
{
	run->book = book;
	run->name = name;
	run->file = file;
	run->child = NULL;
	run->n_child = run->n_diffs = run->n_total = 0;
	run->t = real_clock();
	run->to = NULL;
}

/** @brief End a command that learns by rounds. */
static void learn_end(LearnRun *run)
{
	free(run->child);
	run->child = NULL;
	run->n_child = 0;
	search_pool_release(&expand_pool);
	book_store_release(); // (the searches of the links)
}

/** @brief Report that the memory of a command that learns is exhausted: the command stops. */
static void learn_no_memory(Book *book)
{
	error("cannot allocate the lists of the positions to learn\n");
	book->failed = true;
}

/**
 * @brief Start an isolated plan.
 *
 * @param book Opening book.
 * @return false if it cannot be allocated (the command stops).
 */
static bool learn_plan_begin(Book *book)
{
	if (!book_plan_begin(book)) {
		learn_no_memory(book);
		return false;
	}
	book_plan->isolate = true;
	book_plan->max_workers = book_one_search_at_a_time() ? 1 : MAX(0, options.book_expand_tasks);
	return true;
}

/**
 * @brief Link positions to all their children in the book, and search again the leaves that became links.
 *
 * @param run Command.
 * @param keys Places of the positions (see position_key), in the order of the book.
 * @param n Number of positions.
 */
static void learn_link(LearnRun *run, const unsigned long long *keys, const long long n)
{
	Book *book = run->book;
	const int max_jobs = BOOK_FILL_PLAN_SEARCHES * book_store_thread_count();
	const long long max_positions = 16LL * max_jobs;
	long long i = 0, j, end;

	while (i < n && !book->failed) {
		if (!learn_plan_begin(book)) break;
		for (j = i; j < n; ) {
			book_plan_board(book, &book_position(book, keys[j])->board); // (a board of the book is unique)
			++j;
			if (book_plan->failed || book_plan->n_job >= max_jobs || j - i >= max_positions) break;
		}
		end = j;
		book_plan_search(book);
		for (j = i; j < end && !book->failed; ++j) {
			Position *p = book_position(book, keys[j]);
			const int x = p->leaf.move;
			const long long n_links = book->stats.n_links;

			position_link(p, book);
			if (book->stats.n_links != n_links) book->need_saving = true;
			// the leaf is a link now (position_add_link has removed it): search the next leaf, if a move is left
			if (x != NOMOVE && p->leaf.move == NOMOVE) position_search(p, book);
		}
		book_plan_end(book);
		i = end;
		bprint("%s (links)...%lld/%lld: %lld links\r", run->name, i, n, book->stats.n_links);
		learn_save_timed(run);
	}
}

/**
 * @brief A round: expand the leaves of some positions.
 *
 * @param run Command (out: the children of the round, and what it added).
 * @param keys Places of the positions (see position_key).
 * @param n Number of positions.
 */
static void learn_round(LearnRun *run, const unsigned long long *keys, const long long n)
{
	Book *book = run->book;
	LearnChild *child = (LearnChild*) malloc((size_t) (n + 1) * sizeof *child);
	unsigned long long *link_keys = (unsigned long long*) malloc((size_t) (n + 1) * sizeof *link_keys);
	long long i, m = 0, n_link = 0;

	run->n_diffs = 0;
	if (child == NULL || link_keys == NULL) {
		free(child); free(link_keys);
		learn_no_memory(book);
		return;
	}
	for (i = 0; i < n; ++i) {
		const Position *p = book_position(book, keys[i]);
		Board next;

		if (p->leaf.move == NOMOVE) continue; // nothing to expand
		next = position_leaf_board(p);
		board_unique(&next, &child[m].board);
		child[m].key = keys[i];
		++m;
	}
	learn_children_sort(child, m);
	free(run->child);
	run->child = child;
	run->n_child = m;

	book_clean(book);
	for (i = 0; i < m; ++i) {
		const bool first = (i == 0 || !board_equal(&child[i - 1].board, &child[i].board));

		if (first && book_probe(book, &child[i].board) == NULL) {
			book_mark_todo(book, book_position(book, child[i].key));
			++book->stats.n_todo;
		} else { // the position is in the book, or will be when the first one is expanded
			link_keys[n_link++] = child[i].key;
		}
	}
	if (book->stats.n_todo) book_expand_learn(book, run->name, run->file, run);
	if (n_link && !book->failed) {
		learn_keys_sort(link_keys, n_link);
		learn_link(run, link_keys, n_link);
	}
	free(link_keys);
	run->n_diffs = book->stats.n_nodes + book->stats.n_links;
	run->n_total += run->n_diffs;
}

/**
 * @brief Expand the positions to do (book deviate, deviate2, deviate3, enhance, leaf expand).
 *
 * As in book play, a new position is searched once: when the leaves of several positions lead to one position,
 * or to one that the book has, the others are linked to it, and their next leaves are searched. (Up to
 * v4.5.5-nikque.32 each of them searched the position again, and all these searches but one were thrown away.)
 *
 * @param book opening book.
 * @param action String with a description of current action.
 * @param tmp_file Temporary file name.
 */
static void book_expand(Book *book, const char *action, const char *tmp_file)
{
	LearnRun run;
	unsigned long long *keys = NULL;
	long long i, n = 0;

	if (book->todo_list.valid && book->todo_list.n > 0) {
		keys = (unsigned long long*) malloc((size_t) book->todo_list.n * sizeof *keys);
	}
	if (keys) {
		todo_sort(book->todo_list.item, book->todo_list.n);
		for (i = 0; i < book->todo_list.n; ++i) {
			const unsigned long long key = book->todo_list.item[i];
			const Position *p = book_position(book, key);

			if (position_is_todo(p, book) && p->leaf.move != NOMOVE && (n == 0 || keys[n - 1] != key)) keys[n++] = key;
		}
	}
	if (n == 0) { // nothing to expand, or no memory for the list: one search for each position to do
		free(keys);
		book_expand_learn(book, action, tmp_file, NULL);
		return;
	}
	learn_begin(&run, book, action, tmp_file);
	learn_round(&run, keys, n);
	free(run.child);
	free(keys);
}
/**
 * @brief The positions to expand in the next round of book play: the positions that the last round reached
 * (added, or already in the book) that have no link.
 *
 * The other positions without link were expanded by the rounds before, and links are only added: none is missed.
 *
 * @param run Command.
 * @param limit A position with fewer empty squares is not expanded.
 * @param n Number of positions (out).
 * @return their places, in the order of the book (to free), or NULL if the memory is exhausted.
 */
static unsigned long long* learn_next(LearnRun *run, const int limit, long long *n)
{
	Book *book = run->book;
	unsigned long long *keys = (unsigned long long*) malloc((size_t) (run->n_child + 1) * sizeof *keys);
	long long i, m = 0;

	*n = 0;
	if (keys == NULL) return NULL;
	for (i = 0; i < run->n_child; ++i) {
		const Board *board = &run->child[i].board;
		const Position *p;

		if (i > 0 && board_equal(&run->child[i - 1].board, board)) continue;
		p = book_probe(book, board);
		if (p && p->n_link == 0 && p->leaf.move != NOMOVE && board_count_empties(board) >= limit && !board_is_game_over(board)) {
			keys[m++] = position_key(book, p);
		}
	}
	learn_keys_sort(keys, m);
	*n = m;
	return keys;
}

/** what learn_walk() looks for */
enum {
	LEARN_NO_LINK, /**< book play <n>: a position without link */
	LEARN_EXTEND   /**< book extend: a leaf that no link is better than */
};

/**
 * @brief The position that a link leads to.
 *
 * Only the links that make the game progress are followed, as by the parallel negamax: the links of a damaged
 * book may go round.
 *
 * @param position Position.
 * @param link Link of the position.
 * @param book Opening book.
 * @return the position, or NULL if it is not in the book, or if the link is not followed.
 */
static Position* learn_link_child(const Position *position, const Link *link, const Book *book)
{
#ifdef BOOK_TEST_NEGAMAX_NO_PREFETCH
	return negamax_link_target(position, link, book);
#else
	Board unique;
	PositionArray *a = negamax_link_array(position, link, book, &unique);

	return a ? position_array_probe(a, &unique) : NULL;
#endif
}

/** @return true if a walk looks for this position. */
static bool learn_selected(const Position *p, const int kind)
{
	const Link *l;

	if (p->leaf.move == NOMOVE) return false;
	if (kind == LEARN_NO_LINK) return p->n_link == 0;
	foreach_link(l, p) if (l->score > p->leaf.score) return false;
	return true;
}

/** A thread of learn_walk(): the positions of a level (w.a: what is looked for; w.b: the fewest empty squares). */
static void* learn_walk_run(void *v)
{
	LossWorker *lw = (LossWorker*) v;
	DeviateWorker *w = &lw->w;
	Book *book = w->book;
	long long k;

	for (k = lw->first; k < lw->last; ++k) {
		Position *position = lw->cur[k];
		const Link *l;

		foreach_link(l, position) {
			Position *child = learn_link_child(position, l, book);

			if (child && board_count_empties(&child->board) >= w->b && !board_is_game_over(&child->board)) {
				unsigned char *visit = book_visit(book, child);

				if (atomic_load_uchar(visit) == 0 && atomic_cas_uchar(visit, 0, 1) && !position_list_push(&lw->same, child)) w->oom = true;
			}
		}
		if (learn_selected(position, w->a)) deviate_worker_todo(w, position);
	}
	return NULL;
}

/**
 * @brief The positions that a command looks for, among those that links reach from a position (several threads).
 *
 * @param book Opening book.
 * @param board Position to start from.
 * @param kind LEARN_NO_LINK or LEARN_EXTEND.
 * @param limit The positions with fewer empty squares are left out.
 * @param n Number of positions (out).
 * @return their places, in the order of the book (to free), or NULL if the memory is exhausted.
 */
static unsigned long long* learn_walk(Book *book, const Board *board, const int kind, const int limit, long long *n)
{
	LossWorker lw[MAX_THREADS];
	PositionList cur = {NULL, 0, 0}, next = {NULL, 0, 0};
	const int n_threads = book_walk_threads();
	Position *root;
	unsigned long long *keys;
	long long j, m = 0;
	int i, level;
	bool ok = true;

	*n = 0;
	book_clean(book);
	root = book_probe(book, board);
	if (root == NULL || board_count_empties(board) < limit || board_is_game_over(board)) {
		return (unsigned long long*) malloc(sizeof *keys);
	}
	if (!book_visit_init(book)) return NULL;
	for (i = 0; i < n_threads; ++i) {
		memset(lw + i, 0, sizeof lw[i]);
		lw[i].w.book = book;
		lw[i].w.a = kind;
		lw[i].w.b = limit;
	}
	*book_visit(book, root) = 1;
	ok = position_list_push(&cur, root);
	for (level = 0; ok && cur.n > 0; ++level) {
#ifdef BOOK_TEST_COUNTS
		if (cur.n >= 4096) {
			++book_test_wide_walks;
			if (n_threads > 1) ++book_test_wide_threaded;
		}
#endif
		ok = deviate_walk_level(lw, n_threads, learn_walk_run, &cur, level, &next);
		{ PositionList t = cur; cur = next; next = t; }
	}
	for (i = 0; i < n_threads; ++i) {
		if (lw[i].w.oom) ok = false;
		m += lw[i].w.n;
	}
	keys = ok ? (unsigned long long*) malloc((size_t) (m + 1) * sizeof *keys) : NULL;
	if (keys) {
		for (i = 0, m = 0; i < n_threads; ++i) {
			for (j = 0; j < lw[i].w.n; ++j) keys[m++] = lw[i].w.item[j];
		}
		learn_keys_sort(keys, m);
		*n = m;
	}
	for (i = 0; i < n_threads; ++i) { free(lw[i].same.item); free(lw[i].w.item); }
	free(cur.item); free(next.item);
	book_visit_free(book);
	return keys;
}

/**
 * @brief Extend.
 *
 * Expand, from a position, the leaves that no link is better than (as good as the best link, or the leaf of a
 * position without link), until there is none left within the depth of the book (book extend of Edax 4.7).
 * The choice depends on the scores: each round negamaxes and walks the positions under this one, with several
 * threads, as book deviate.
 *
 * @param book opening book.
 * @param board Position to start from.
 */
void book_extend(Book *book, const Board *board)
{
	const Board start = *board;
	LearnRun run;
	unsigned long long *keys;
	long long n;
	char file[FILENAME_MAX + 1];

	if (book_probe(book, &start) == NULL) {
		warn("book extend: the current position is not in the book\n");
		return;
	}
	file_add_ext(options.book_file, ".ext", file);
	learn_begin(&run, book, "Book extend", file);
	book_negamax_from(book, &start);
	for (;;) {
		keys = learn_walk(book, &start, LEARN_EXTEND, book->options.n_empties, &n);
		if (keys == NULL) {
			learn_no_memory(book);
			break;
		}
		bprint("Book extend...%lld todo\n", n);
		if (n == 0) {
			free(keys);
			break;
		}
		learn_round(&run, keys, n);
		free(keys);
		book_negamax_from(book, &start);
		if (!run.n_diffs || book->failed) break; // stop if a position cannot be added
		learn_save_timed(&run);
	}
	if (run.n_total) book_save_progress(book, file);
	bprint("Book extend... finished\n");
	learn_end(&run);
}

/**
 * @brief Play to a number of empty squares.
 *
 * book play <n> (as book play of Edax 4.7): from a position, the positions without link within the depth of the
 * book that links reach, then the lines from their leaves, one move after the other, while the position to
 * expand has n empty squares or more (the last position added has n - 1). A line that reaches a position of the
 * book that has a link ends there; one that reaches a position without link goes on from it. Every line gets
 * one move by round: they are searched at the same time.
 *
 * @param book opening book.
 * @param board Position to start from.
 * @param n_empties The positions with fewer empty squares are not expanded.
 */
void book_play_to(Book *book, const Board *board, const int n_empties)
{
	const Board start = *board;
	LearnRun run;
	PlayTo to;
	unsigned long long *keys;
	long long n, i;
	bool saved = true;
	char file[FILENAME_MAX + 1];

	if (book_probe(book, &start) == NULL) {
		warn("book play %d: the current position is not in the book\n", n_empties);
		return;
	}
	if (n_empties < book->options.n_empties) {
		bprint("Book play %d: beyond the depth of the book (book prune removes these positions)\n", n_empties);
	}
	file_add_ext(options.book_file, ".play", file);
	memset(&to, 0, sizeof to);
	board_unique(&start, &to.start);
	to.n_empties = n_empties;
	file_add_ext(options.book_file, ".play-to", to.file);
	play_to_read(&to);

	learn_begin(&run, book, "Book play", file);
	run.to = &to;
	keys = learn_walk(book, &start, LEARN_NO_LINK, MAX(book->options.n_empties, n_empties), &n);
	if (keys && to.n_saved) { // and the ends of the run that did not end
		unsigned long long *all = (unsigned long long*) realloc(keys, (size_t) (n + to.n_saved + 1) * sizeof *all);

		if (all) {
			long long m;

			keys = all;
			for (i = 0; i < to.n_saved; ++i) {
				const Position *p = book_probe(book, to.saved + i);

				if (p && p->n_link == 0 && p->leaf.move != NOMOVE && board_count_empties(&p->board) >= n_empties && !board_is_game_over(&p->board)) {
					keys[n++] = position_key(book, p);
				}
			}
			learn_keys_sort(keys, n);
			for (i = m = 0; i < n; ++i) if (m == 0 || keys[m - 1] != keys[i]) keys[m++] = keys[i];
			n = m;
		} else {
			free(keys);
			keys = NULL;
		}
	}
	for (;;) {
		if (keys == NULL) {
			learn_no_memory(book);
			break;
		}
		bprint("Book play %d...%lld todo\n", n_empties, n);
		if (n == 0) break;
		free(run.child); // (the children of the round before are the positions of this one)
		run.child = NULL;
		run.n_child = 0;
		to.keys = keys;
		to.n_keys = n;
		learn_round(&run, keys, n);
		to.keys = NULL;
		to.n_keys = 0;
		free(keys);
		keys = NULL;
		if (!run.n_diffs || book->failed) break; // stop if a position cannot be added
		if (book_save_interval_elapsed((long long) run.t)) {
			book_negamax_from(book, &start);
			learn_save_timed(&run);
		}
		keys = learn_next(&run, n_empties, &n);
	}
	free(keys);
	book_negamax_from(book, &start);
	if (run.n_total) saved = book_save_progress(book, file);
	if (saved && !book->failed && to.owned) remove(to.file);
	free(to.saved);
	bprint("Book play %d... finished\n", n_empties);
	learn_end(&run);
}

/*
 * book exhaust: every move from a position to the depth of the book (with the passes).
 *
 * The boards are found level by level, from this position down, by several threads: each board once (the 8
 * symmetries are one board), whether it is in the book or not. The book is not changed meanwhile. The boards
 * of a level have as many empty squares; those without a move (a pass, or the end of a game) come before the
 * others, as their children have as many empty squares. The new boards are then added from the bottom up: a
 * position is added after all its children, so it has all its links and is not searched (only the boards at the
 * bottom are, once each). Last, the positions of the book get their new links. An interrupted run leaves whole
 * positions: the next one finds them by the moves, whether they are linked or not.
 */

#define EXHAUST_SHARDS 64            /**< parts of a list sorted apart (always as many: the order does not depend on the threads) */
#define EXHAUST_CHUNK (1LL << 22)    /**< boards walked before the lists of their children are reduced */

/** a list of boards */
typedef struct BoardList {
	Board *item;
	long long n, size;
	long long n_unique;        /**< boards left by the last board_list_unique */
} BoardList;

/** a list of the places of positions */
typedef struct KeyList {
	unsigned long long *item;
	long long n, size;
} KeyList;

static bool board_list_reserve(BoardList *list, const long long n)
{
	if (n > list->size) {
		const long long size = MAX(n, list->size + list->size / 2 + 1024);
		Board *item = (Board*) realloc(list->item, (size_t) size * sizeof *item);

		if (item == NULL) return false;
		list->item = item;
		list->size = size;
	}
	return true;
}

static bool board_list_push(BoardList *list, const Board *board)
{
	if (list->n == list->size && !board_list_reserve(list, list->n + 1)) return false;
	list->item[list->n++] = *board;
	return true;
}

static bool board_list_append(BoardList *dest, const BoardList *src)
{
	if (src->n == 0) return true;
	if (!board_list_reserve(dest, dest->n + src->n)) return false;
	memcpy(dest->item + dest->n, src->item, (size_t) src->n * sizeof *src->item);
	dest->n += src->n;
	return true;
}

static bool key_list_push(KeyList *list, const unsigned long long key)
{
	if (list->n == list->size) {
		const long long size = list->size + list->size / 2 + 1024;
		unsigned long long *item = (unsigned long long*) realloc(list->item, (size_t) size * sizeof *item);

		if (item == NULL) return false;
		list->item = item;
		list->size = size;
	}
	list->item[list->n++] = key;
	return true;
}

/** a thread of board_list_unique */
typedef struct ShardWorker {
	Board *item;
	const long long *first;    /**< start of each part */
	long long *kept;           /**< boards left in each part */
	int id, n_workers;
} ShardWorker;

static void* shard_worker_run(void *v)
{
	ShardWorker *w = (ShardWorker*) v;
	int s;

	for (s = w->id; s < EXHAUST_SHARDS; s += w->n_workers) {
		Board *part = w->item + w->first[s];
		const long long n = w->first[s + 1] - w->first[s];
		long long i, k = 0;

		learn_boards_sort(part, n);
		for (i = 0; i < n; ++i) if (k == 0 || !board_equal(part + k - 1, part + i)) part[k++] = part[i];
		w->kept[s] = k;
	}
	return NULL;
}

/**
 * @brief Leave each board once in a list, with several threads. The order only depends on the boards.
 *
 * @param list List of unique boards.
 * @return false if the memory is exhausted.
 */
static bool board_list_unique(BoardList *list)
{
	long long count[EXHAUST_SHARDS] = {0}, first[EXHAUST_SHARDS + 1], next[EXHAUST_SHARDS], kept[EXHAUST_SHARDS];
	ShardWorker w[MAX_THREADS];
	Board *part;
	long long i, n = 0;
	int s, m = MIN(book_walk_threads(), EXHAUST_SHARDS);

	if (list->n == 0) {
		list->n_unique = 0;
		return true;
	}
	if (list->n < 4096) m = 1;
	part = (Board*) malloc((size_t) list->n * sizeof *part);
	if (part == NULL) return false;
#define exhaust_shard(b) ((int) (board_get_hash_code(b) >> 58) & (EXHAUST_SHARDS - 1))
	for (i = 0; i < list->n; ++i) ++count[exhaust_shard(list->item + i)];
	first[0] = 0;
	for (s = 0; s < EXHAUST_SHARDS; ++s) {
		next[s] = first[s];
		first[s + 1] = first[s] + count[s];
	}
	for (i = 0; i < list->n; ++i) part[next[exhaust_shard(list->item + i)]++] = list->item[i];
#undef exhaust_shard
	for (s = 0; s < m; ++s) {
		w[s].item = part; w[s].first = first; w[s].kept = kept;
		w[s].id = s; w[s].n_workers = m;
	}
	book_run_workers(shard_worker_run, w, sizeof *w, m);
	for (s = 0; s < EXHAUST_SHARDS; ++s) {
		memcpy(list->item + n, part + first[s], (size_t) kept[s] * sizeof *part);
		n += kept[s];
	}
	free(part);
	list->n = list->n_unique = n;
	return true;
}

/** a thread of the walk of book exhaust */
typedef struct ExhaustWorker {
	const Book *book;
	const Board *board;        /**< boards to walk */
	long long first, last;
	BoardList child[2];        /**< children without a move [0], with moves [1] */
	BoardList fresh;           /**< boards that are not in the book */
	KeyList link;              /**< positions of the book that do not have all their links */
	bool oom;
} ExhaustWorker;

static void* exhaust_worker_run(void *v)
{
	ExhaustWorker *w = (ExhaustWorker*) v;
	const Book *book = w->book;
	long long i;
	int x;

	for (i = w->first; i < w->last && !w->oom; ++i) {
		const Board *board = w->board + i;
		const Position *p = book_probe(book, board);
		unsigned long long moves;
		Board next, unique;
		int n_moves;

		if (p == NULL && !board_list_push(&w->fresh, board)) w->oom = true;
		if (board_count_empties(board) < book->options.n_empties) continue; // the bottom
		moves = board_get_moves(board);
		n_moves = bit_count(moves);
		if (moves) {
			foreach_bit(x, moves) {
				board_next(board, x, &next);
				board_unique(&next, &unique);
				if (!board_list_push(&w->child[can_move(unique.player, unique.opponent) ? 1 : 0], &unique)) w->oom = true;
			}
		} else if (can_move(board->opponent, board->player)) { // pass
			next.player = board->opponent;
			next.opponent = board->player;
			board_unique(&next, &unique);
			if (!board_list_push(&w->child[1], &unique)) w->oom = true;
			n_moves = 1;
		} else {
			continue; // the end of a game
		}
		if (p && p->n_link < n_moves && !key_list_push(&w->link, position_key(book, p))) w->oom = true;
	}
	return NULL;
}

/** the lists of book exhaust: by number of empty squares, without [0] and with [1] a move */
typedef struct Exhaust {
	BoardList found[61][2];
	BoardList fresh[61][2];
	KeyList link;
} Exhaust;

/**
 * @brief Walk the boards of a level: their children, those that are not in the book, the links to add.
 *
 * @param book Opening book (not changed).
 * @param ex Lists.
 * @param e Number of empty squares.
 * @param movers 1: the boards with a move; 0: without.
 * @return false if the memory is exhausted.
 */
static bool exhaust_walk(const Book *book, Exhaust *ex, const int e, const int movers)
{
	ExhaustWorker w[MAX_THREADS];
	BoardList *level = &ex->found[e][movers];
	BoardList *target[2];
	const int n_threads = book_walk_threads();
	long long from, to, j;
	int i, c;
	bool ok;

	if (level->n == 0) return true;
	if (!board_list_unique(level)) return false;
#ifdef BOOK_TEST_COUNTS
	if (level->n >= 4096) {
		++book_test_wide_walks;
		if (n_threads > 1) ++book_test_wide_threaded;
	}
#endif
	// the children of a pass have as many empty squares (and a move); the others one less
	target[0] = (movers && e > 0) ? &ex->found[e - 1][0] : NULL;
	target[1] = movers ? (e > 0 ? &ex->found[e - 1][1] : NULL) : &ex->found[e][1];
	memset(w, 0, n_threads * sizeof *w);
	ok = true;
	for (from = 0; ok && from < level->n; from = to) {
		const long long n = MIN(level->n - from, EXHAUST_CHUNK);
		const int m = (n < 4096) ? 1 : n_threads;

		to = from + n;
		for (i = 0; i < m; ++i) {
			w[i].book = book;
			w[i].board = level->item;
			w[i].first = from + n * i / m;
			w[i].last = from + n * (i + 1) / m;
		}
		book_run_workers(exhaust_worker_run, w, sizeof *w, m);
		for (i = 0; i < m; ++i) {
			if (w[i].oom) ok = false;
			for (c = 0; c < 2 && ok; ++c) {
				if (w[i].child[c].n && target[c] && !board_list_append(target[c], &w[i].child[c])) ok = false;
				w[i].child[c].n = 0;
			}
			if (ok && !board_list_append(&ex->fresh[e][movers], &w[i].fresh)) ok = false;
			w[i].fresh.n = 0;
			for (j = 0; ok && j < w[i].link.n; ++j) ok = key_list_push(&ex->link, w[i].link.item[j]);
			w[i].link.n = 0;
		}
		// (a board has as many parents as moves lead to it: the duplicates are dropped along the way)
		for (c = 0; c < 2 && ok; ++c) {
			if (target[c] && target[c]->n > 2 * target[c]->n_unique + EXHAUST_CHUNK) ok = board_list_unique(target[c]);
		}
	}
	for (i = 0; i < n_threads; ++i) {
		free(w[i].child[0].item); free(w[i].child[1].item);
		free(w[i].fresh.item); free(w[i].link.item);
	}
	free(level->item); // (walked)
	level->item = NULL;
	level->n = level->size = 0;
	return ok;
}

/**
 * @brief Add new boards whose children are in the book (or in the lists that were added before), in this order.
 *
 * @param run Command.
 * @param list Unique boards.
 * @param done Boards added by the command (progress).
 * @param total Boards to add.
 */
static void exhaust_add(LearnRun *run, const BoardList *list, long long *done, const long long total)
{
	Book *book = run->book;
	const int max_jobs = BOOK_FILL_PLAN_SEARCHES * book_store_thread_count();
	const long long max_positions = 16LL * max_jobs;
	long long i = 0, j, end;

	while (i < list->n && !book->failed) {
		if (!learn_plan_begin(book)) break;
		for (j = i; j < list->n; ) {
			book_plan_board(book, list->item + j);
			++j;
			if (book_plan->failed || book_plan->n_job >= max_jobs || j - i >= max_positions) break;
		}
		end = j;
		book_plan_search(book);
		for (j = i; j < end && !book->failed; ++j) book_add_board(book, list->item + j);
		book_plan_end(book);
		book->need_saving = true;
		*done += end - i;
		i = end;
		bprint("%s...%lld/%lld done: %lld positions, %lld links\r", run->name, *done, total, book->stats.n_nodes, book->stats.n_links);
		learn_save_timed(run);
	}
}

/**
 * @brief Exhaust.
 *
 * Add every move from a position to the depth of the book (book exhaust of Edax 4.7, with the passes and the
 * positions without a leaf).
 *
 * @param book opening book.
 * @param board Position to start from.
 */
void book_exhaust(Book *book, const Board *board)
{
	const Board start = *board;
	Board root;
	LearnRun run;
	Exhaust *ex;
	long long n_fresh = 0, done = 0;
	int e, c;
	bool ok;
	char file[FILENAME_MAX + 1];
	const int top = board_count_empties(board);

	if (book_probe(book, &start) == NULL) {
		warn("book exhaust: the current position is not in the book\n");
		return;
	}
	if (top < book->options.n_empties || top > 60 || board_is_game_over(&start)) {
		bprint("Book exhaust... nothing to add within the depth of the book\n");
		return;
	}
	ex = (Exhaust*) calloc(1, sizeof *ex);
	if (ex == NULL) {
		learn_no_memory(book);
		return;
	}
	file_add_ext(options.book_file, ".exh", file);
	learn_begin(&run, book, "Book exhaust", file);

	// down: the boards, level by level (the book is not changed)
	board_unique(&start, &root);
	ok = board_list_push(&ex->found[top][can_move(root.player, root.opponent) ? 1 : 0], &root);
	for (e = top; ok && e >= 0; --e) {
		for (c = 0; ok && c < 2; ++c) ok = exhaust_walk(book, ex, e, c);
		if (ok) {
			n_fresh += ex->fresh[e][0].n + ex->fresh[e][1].n;
			if (ex->fresh[e][0].n + ex->fresh[e][1].n) bprint("Book exhaust...%lld positions to add\r", n_fresh);
		}
	}

	if (!ok) {
		learn_no_memory(book); // (nothing was changed)
	} else {
		bprint("Book exhaust...%lld positions to add, %lld positions to link\n", n_fresh, ex->link.n);
		book_clean(book);
		// up: the children before their parents
		for (e = 0; e <= top && !book->failed; ++e) {
			for (c = 1; c >= 0 && !book->failed; --c) {
				exhaust_add(&run, &ex->fresh[e][c], &done, n_fresh);
				free(ex->fresh[e][c].item);
				ex->fresh[e][c].item = NULL;
			}
		}
		if (ex->link.n && !book->failed) {
			learn_keys_sort(ex->link.item, ex->link.n);
			learn_link(&run, ex->link.item, ex->link.n);
		}
		run.n_total = book->stats.n_nodes + book->stats.n_links;
		book_negamax_from(book, &start);
		if (run.n_total) book_save_progress(book, file);
	}
	for (e = 0; e <= 60; ++e) {
		for (c = 0; c < 2; ++c) {
			free(ex->found[e][c].item);
			free(ex->fresh[e][c].item);
		}
	}
	free(ex->link.item);
	free(ex);
	bprint("Book exhaust... finished\n");
	learn_end(&run);
}

/**
 * @brief Play.
 *
 * Add positions to the opening book by adding links
 * to position with no links.
 *
 * The positions are found by several threads and listed, so that book_expand() searches several of them at
 * the same time (book-expand-tasks). (Up to v4.5.5-nikque.32 the list was set invalid here, and the positions
 * were always expanded one after the other.)
 *
 * @param book opening book.
 */
void book_play(Book *book)
{
	static const BookPick play_pick = {0, false, false, false, true};
	LearnRun run;
	unsigned long long *keys;
	long long n;
	char file[FILENAME_MAX + 1];

	file_add_ext(options.book_file, ".play", file);
	learn_begin(&run, book, "Book play", file);
	keys = book_pick_scan(book, "Book play", &play_pick, &n);
	for (;;) {
		if (keys == NULL) {
			learn_no_memory(book);
			break;
		}
		bprint("Book play...%lld todo\n", n);
		if (n == 0) break;
		learn_round(&run, keys, n);
		free(keys);
		keys = NULL;
		if (!run.n_diffs || book->failed) break; // stop if a position cannot be added
		keys = learn_next(&run, book->options.n_empties, &n);
		if (book_save_interval_elapsed((long long) run.t)) {
			book_negamax(book);
			learn_save_timed(&run);
		}
	}
	free(keys);
	book_negamax(book); // (also when nothing was added: a run that did not end saved its book without it)
	if (run.n_total) book_save_progress(book, file);
	bprint("Book play... finished\n");
	learn_end(&run);
}

/**
 * @brief Fill a book from all its positions (book fill / fill2 without limits).
 *
 * @param book opening book.
 * @param depth Distance to fill.
 * @param leaf_only Only through the leaves (book fill2).
 */
static void book_fill_all(Book *book, const int depth, const bool leaf_only)
{
	const char *name = leaf_only ? "Book fill2" : "Book fill";
	const BookPick pick = {depth, leaf_only, false, false, false};
	unsigned long long *keys;
	long long n_diffs = 0, n, n_linked;
	int rounds_since_save = 0;
	char file[FILENAME_MAX + 1];

	if (depth < 2) { // (no position is between a position and the next one)
		bprint("%s %d: nothing to fill (a distance of 2 at least is needed)\n", name, depth);
		return;
	}
	file_add_ext(options.book_file, leaf_only ? ".fill2" : ".fill", file);
	do {
		n_linked = book_link_moves(book, name, file, NULL, false, 0, 0, true); // (the moves that lead to a position of the book)
		book->stats.n_nodes = book->stats.n_links = 0;
		keys = book_pick_scan(book, name, &pick, &n);
		if (keys == NULL) {
			error("cannot allocate the list of the positions to fill from\n");
			book->failed = true;
			break;
		}
		bprint("%s %d: %lld positions to fill from\n", name, depth, n);
		book_fill_keys(book, name, file, keys, n, depth, leaf_only);
		free(keys);
		n_diffs = n_linked + book->stats.n_nodes + book->stats.n_links;
		if (n_diffs) book_negamax(book);
		book_deviate_save_progress(book, file, n_diffs, &rounds_since_save); // (as book deviate: book-deviate-save-rounds)
	} while (n_diffs && !book->failed); // stop if a position cannot be added
	book_store_release(); // (the searches done at the same time)
	bprint("%s %d... finished\n", name, depth);
}

/**
 * @brief Fill a book (book fill <n> without limits).
 *
 * @param book opening book.
 * @param depth Distance to fill between two positions.
 */
void book_fill(Book *book, const int depth)
{
#ifdef BOOK_TEST_FILL_OLD
	book_fill_old(book, depth); // test builds: as up to v4.5.5-nikque.13, to compare
#else
	if (depth < 2) book_fill_old(book, depth); // (book fill 1 adds no position: the links only, as before)
	else book_fill_all(book, depth, false);
#endif
}

/**
 * @brief Fill a book from the leaves of all its positions (book fill2 <n> without limits).
 *
 * @param book opening book.
 * @param depth Distance to fill.
 */
void book_fill2(Book *book, const int depth)
{
	book_fill_all(book, depth, true);
}

/**
 * @brief Fill a book from the positions of a deviate3 walk (book fill / fill2 with loss limits).
 *
 * @param book opening book.
 * @param board Position to start from.
 * @param depth Distance to fill.
 * @param leaf_only book fill2: from the leaves that book deviate3 would expand. book fill: from every move of the positions of the walk.
 * @param move_loss Maximum loss for one move.
 * @param total_loss Maximum cumulative loss for both players.
 */
void book_fill_walk(Book *book, Board *board, const int depth, const bool leaf_only, const int move_loss, const int total_loss)
{
	const char *name = leaf_only ? "Book fill2" : "Book fill";
	const BookPick pick = {depth, leaf_only, false, false, false};
	Position *root = book_probe(book, board);
	unsigned long long *keys;
	long long n_diffs, n, n_selected, n_linked;
	int rounds_since_save = 0;
	bool owned;
	char file[FILENAME_MAX + 1];

	if (root == NULL) return;
	if (depth < 2) { // (no position is between a position and the next one)
		bprint("%s %d %d %d: nothing to fill (a distance of 2 at least is needed)\n", name, depth, move_loss, total_loss);
		return;
	}
	file_add_ext(options.book_file, leaf_only ? ".fill2" : ".fill", file);
	book_clean(book);
	book_negamax_position(root, book);

	do {
		bprint("%s %d %d %d:\n", name, depth, move_loss, total_loss);
		n_linked = book_link_moves(book, name, file, root, true, move_loss, total_loss, true); // (the moves that lead to a position of the book)
		if (n_linked) root = book_negamax_from(book, board); // (the scores that the walk follows)
		book_clean(book);
		deviate_walk_all = !leaf_only;
		book_select_deviate_total(book, root, move_loss, total_loss, false);
		deviate_walk_all = false;
		n_selected = book->stats.n_todo;
		keys = book_pick_todo(book, &pick, &n, &owned);
		if (keys == NULL) {
			error("cannot allocate the list of the positions to fill from\n");
			book->failed = true;
			break;
		}
		bprint("%s %lld positions selected, %lld to fill from\n", name, n_selected, n);

		book->stats.n_nodes = book->stats.n_links = 0;
		book_fill_keys(book, name, file, keys, n, depth, leaf_only);
		if (owned) free(keys);
		n_diffs = n_linked + book->stats.n_nodes + book->stats.n_links;

		root = book_negamax_from(book, board); // (book_clean: the todo list is empty again)
		book_deviate_save_progress(book, file, n_diffs, &rounds_since_save); // (as book deviate: book-deviate-save-rounds)
	} while (n_diffs && !book->failed); // stop if a position cannot be added
	book_store_release(); // (the searches done at the same time)
	bprint("%s %d %d %d...finished\n", name, depth, move_loss, total_loss);
}

/**
 * @brief Expand the leaves better than every link of their position (book leaf expand).
 *
 * @param book opening book.
 * @param board Position to start from (with limits).
 * @param limited With loss limits: only in the walk of book deviate3.
 * @param move_loss Maximum loss for one move.
 * @param total_loss Maximum cumulative loss for both players.
 */
void book_leaf_expand(Book *book, Board *board, const bool limited, const int move_loss, const int total_loss)
{
	static const BookPick leaf_pick = {0, false, true, false, false};
	const char *name = "Book leaf expand";
	Position *root = NULL;
	unsigned long long *keys;
	long long n_diffs, n_linked, n, i;
	int rounds_since_save = 0;
	bool owned, listed;
	char file[FILENAME_MAX + 1];

	file_add_ext(options.book_file, ".lexp", file);
	if (limited) {
		root = book_probe(book, board);
		if (root == NULL) return;
		book_clean(book);
		book_negamax_position(root, book);
	}

	do {
		if (limited) bprint("%s %d %d:\n", name, move_loss, total_loss);
		else bprint("%s:\n", name);
		// the leaves that lead to a position of the book are linked first: their positions are searched again,
		// and a new leaf better than every link is expanded below
		n_linked = book_link_moves(book, name, file, root, limited, move_loss, total_loss, false);
		if (n_linked && limited) root = book_negamax_from(book, board); // (the scores that the walk follows)
		book_clean(book);
		if (limited) { // the walk marks every position with such a leaf (the loss of the leaf is 0): keep these marks only
			book_select_deviate_total(book, root, move_loss, total_loss, false);
			keys = book_pick_todo(book, &leaf_pick, &n, &owned);
			listed = (keys != NULL);
			if (listed && owned) free(keys); // (book_expand reads the todo list, or the marks)
		} else { // the positions with such a leaf are marked (and listed) as book deviate marks them
			keys = book_pick_scan(book, name, &leaf_pick, &n);
			listed = (keys != NULL);
			if (listed) {
				for (i = 0; i < n; ++i) book_mark_todo(book, book_position(book, keys[i]));
				book->stats.n_todo = n;
				free(keys);
			}
		}
		if (!listed) {
			error("cannot allocate the list of the positions to expand\n");
			book->failed = true;
			break;
		}
		bprint("%s %lld todo\n", name, book->stats.n_todo);

		book_expand(book, name, file);
		n_diffs = n_linked + book->stats.n_nodes + book->stats.n_links;

		if (limited) root = book_negamax_from(book, board);
		else if (n_diffs) book_negamax(book);
		book_deviate_save_progress(book, file, n_diffs, &rounds_since_save);
	} while (n_diffs && !book->failed); // stop if a position cannot be added
	if (limited) bprint("%s %d %d...finished\n", name, move_loss, total_loss);
	else bprint("%s...finished\n", name);
	search_pool_release(&expand_pool);
	book_store_release(); // (the searches of the leaves linked, done at the same time)
}

#ifdef BOOK_TEST_PICK_COUNT
/**
 * @brief Test builds (book pick-count <x> <y>): count the positions that the first round of book fill 2,
 * book fill2 2 and book leaf expand would start from, and the positions that they would link (any move for
 * book fill and fill2, the leaf only for book leaf expand), in the whole
 * book and in the walk of book deviate3 x y from the current position, with the time each selection takes.
 * Nothing is added nor searched (the book is negamaxed from the current position, as the commands do).
 */
void book_test_pick_count(Book *book, Board *board, const int move_loss, const int total_loss)
{
	static const BookPick picks[5] = {{0, false, false, false}, {2, true, false, false}, {2, false, false, false}, {0, false, true, false}, {0, false, false, true}};
	static const char *const names[5] = {"leaves to link (leaf expand)", "fill2 2: positions to fill from", "fill 2: positions to fill from", "leaf expand: leaves better than every link", "positions to link (fill, fill2)"};
	static const bool walk_all[5] = {true, false, true, false, true};
	unsigned long long *keys;
	long long n, n_marked, t;
	Position *root;
	bool owned;
	int i;

	static const int whole_order[5] = {3, 0, 4, 1, 2}, walk_order[5] = {0, 4, 3, 1, 2}; // (the quick counts first: a run stopped early keeps them)
	int j;

	root = book_probe(book, board);
	if (root == NULL) return;
	t = real_clock();
	keys = book_pick_scan(book, names[3], &picks[3], &n);
	bprint("pick-count, whole book, %s: %lld (%.1f s)\n", names[3], n, (real_clock() - t) / 1000.0);
	free(keys);
	t = real_clock();
	book_clean(book);
	book_negamax_position(root, book);
	bprint("pick-count, negamax: %.1f s\n", (real_clock() - t) / 1000.0);
	for (j = 0; j < 5; ++j) { // the walk of book deviate3 x y
		i = walk_order[j];
		t = real_clock();
		book_clean(book);
		deviate_walk_all = walk_all[i];
		book_select_deviate_total(book, root, move_loss, total_loss, false);
		deviate_walk_all = false;
		n_marked = book->stats.n_todo;
		keys = book_pick_todo(book, &picks[i], &n, &owned);
		bprint("pick-count, walk of deviate3 %d %d, %s: %lld of %lld marked (%.1f s)\n", move_loss, total_loss, names[i], n, n_marked, (real_clock() - t) / 1000.0);
		if (owned) free(keys);
	}
	book_clean(book);
	for (j = 1; j < 5; ++j) { // the whole book (the scan of book fill 2 is long)
		i = whole_order[j];
		t = real_clock();
		keys = book_pick_scan(book, names[i], &picks[i], &n);
		bprint("pick-count, whole book, %s: %lld (%.1f s)\n", names[i], n, (real_clock() - t) / 1000.0);
		free(keys);
	}
}
#endif

/* analyses for a user interface (book moveinfo, book missing, book levels) */
#include "book_analyze.c"
