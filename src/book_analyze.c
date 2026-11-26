/**
 * @file book_analyze.c
 *
 * Book analysis for a user interface (included at the end of book.c, which it needs the positions of).
 *
 * - book moveinfo: for every legal move, the book value and level, the precision stage, the leaves missing
 *   to reach the next stage, the memorization numbers (umigame numbers, a transposition counted once)
 *   and the number of book positions after the move.
 * - book missing: the lines to the leaves that a deviate walk would expand.
 * - book levels: the number of positions of each level after the current position.
 * - book bestlines: the lines of best moves (both sides) from the current position, counted and written.
 * - book findline: the lines from the current position to a target position through book positions only, the
 *   smallest sum of the changes of the book value (seen by black) along the line first.
 * - book reducecount: the positions that book reduce x y would keep and remove (book reduce itself, which changes the
 *   book, is a book command of the main thread: book_reduce below).
 * - book deviate x y export (deviate2, deviate3), a book command of the main thread (book_deviate_export below): the
 *   lines of the walk of book deviate x y written to a file, without expanding the book.
 *
 * The analyses run in a thread of their own, one after the other, and only read the book: the commands
 * that change the book first stop them (book_analysis_quiesce). They can be stopped at any time with
 * "stop id=<n>". Their answers are whole lines that start with '@', of words "key=value".
 *
 * Precision stage of a move (the position after it is the root):
 * - 0: the position is not in the book;
 * - 1: in the book, but no line of best moves (both sides) reaches a position solved exactly;
 * - 2: a line of best moves reaches a solved position;
 * - 3 to 2 + n: "book deviate x y" (or deviate2, deviate3) with the n ranges of the stages, from the narrowest,
 *   would expand no leaf (the selection walks of book_deviate, book_deviate2 and book_deviate3, without
 *   their expansion).
 * The stages are kept while the book does not change. The stage of the position after a move takes over what the kept
 * stages of the current position and of the next positions tell (BaStageInfo): the walks that would find nothing
 * are left out, with the same stages and missing leaves.
 *
 * Umigame number (memorization number) of a side: the smallest number of line ends that this side has to
 * learn to play every line of the condition (best moves only by default): at its turn it chooses one move,
 * at the other side's turn it has to know every move. The same position reached by different lines is
 * learnt once (its ends are counted once): the exact minimum is computed with expressions of shared
 * positions (an end count plus a set of shared positions), see ba_umi_count.
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_MSC_VER)
#include <intrin.h>
#endif

/* ------------------------------------------------------------------ output ------------------------------------------------------------------ */

/** The lines of the analyses and the lines of the main thread are written whole, one after the other. */
static struct {
	Lock lock;
	bool init;   /**< the lock is ready (edax protocol) */
	bool open;   /**< the last characters written by the main thread do not end the line (the prompt) */
} ba_output;

/** @brief Take the output (the lines of the main thread are written between ui_output_lock and ui_output_unlock). */
void ui_output_lock(void)
{
	if (ba_output.init) lock(&ba_output);
}

/**
 * @brief Give the output back.
 * @param line_open true if the characters written do not end the line.
 */
void ui_output_unlock(const bool line_open)
{
	if (ba_output.init) {
		ba_output.open = line_open;
		unlock(&ba_output);
	}
}

/** @brief Write a line of an analysis (format ends with a new line). */
static void ba_print(const char *format, ...)
{
	char s[8192];
	va_list args;

	va_start(args, format);
	vsnprintf(s, sizeof s, format, args);
	va_end(args);
	ui_output_lock();
	if (ba_output.open) putchar('\n');
	fputs(s, stdout);
	fflush(stdout);
	ui_output_unlock(false);
}

/* ------------------------------------------------------------------ state ------------------------------------------------------------------- */

enum { BA_MOVEINFO, BA_MISSING, BA_LEVELS, BA_BESTLINES, BA_FINDLINE, BA_REDUCECOUNT };
enum { BA_DEVIATE = 1, BA_DEVIATE2 = 2, BA_DEVIATE3 = 3 };
enum { BA_UMI_OFF, BA_UMI_BEST, BA_UMI_LOSS, BA_UMI_RANGE };
enum { BA_PART_STAGE = 1, BA_PART_UMI = 2, BA_PART_COUNT = 4 };

#define BA_MAX_STAGES 8

/** An analysis to do. */
typedef struct BaJob {
	struct BaJob *next;
	int id;                 /**< number given by the user interface */
	int kind;               /**< BA_MOVEINFO, BA_MISSING, BA_LEVELS, BA_BESTLINES, BA_FINDLINE or BA_REDUCECOUNT */
	char *param;            /**< words after the command */
	char *line;             /**< moves of the game up to the current position (passes left out) */
	Board board;            /**< current position */
	int player;             /**< side to move (BLACK or WHITE) */
	Book *book;
	volatile bool cancel;   /**< stop asked (ba_cancelled, ba_cancel) */
} BaJob;

/** @return true if the stop of an analysis was asked (read by its threads while the input thread may set it). */
static inline bool ba_cancelled(const BaJob *job)
{
#if defined(_MSC_VER)
	return *(const volatile bool*) &job->cancel; // (an aligned volatile access is not torn)
#else
	return __atomic_load_n(&job->cancel, __ATOMIC_RELAXED);
#endif
}

/** @brief Ask an analysis to stop. */
static inline void ba_cancel(BaJob *job)
{
#if defined(_MSC_VER)
	*(volatile bool*) &job->cancel = true;
#else
	__atomic_store_n(&job->cancel, true, __ATOMIC_RELAXED);
#endif
}

/** Place of each bucket in the numbering of the positions (bucket, then place in the bucket). */
typedef struct BaIndex {
	unsigned int *first;    /**< number of the first position of each bucket */
	int n_bucket;
	unsigned long long n;   /**< positions */
	unsigned int *table;    /**< book-reduce-index table: the bucket of the number j << BA_INDEX_SHIFT (see ba_index_table) */
	unsigned int n_table;
} BaIndex;

#define BA_INDEX_SHIFT 6        /**< one bucket number in the table of book-reduce-index for 64 numbers */

static struct {
	Lock lock;
	Condition cond;
	Thread thread;
	bool init, started, quit;
	BaJob *head, *tail;     /**< analyses waiting */
	BaJob *current;         /**< analysis being done */
	BaIndex index;          /**< numbering of the positions of the book (kept while the book does not change) */
	const Book *index_book;
	long long dense_bytes;  /**< memory of the dense marks in use */
} ba;

/** @brief Prepare the analyses (edax protocol). */
void book_analysis_init(void)
{
	if (ba.init) return;
	lock_init(&ba);
	condition_init(&ba);
	lock_init(&ba_output);
	ba_output.init = true;
	ba.init = true;
}

/** @return the threads of an analysis: n-tasks, 1 to MAX_THREADS. */
static int ba_n_thread(void)
{
	return options.n_task < 1 ? 1 : (options.n_task > MAX_THREADS ? MAX_THREADS : options.n_task);
}

/* ------------------------------------------------------------------ parameters -------------------------------------------------------------- */

typedef struct BaParams {
	int base;                                   /**< BA_DEVIATE, BA_DEVIATE2, BA_DEVIATE3 */
	int n_stage;                                /**< ranges of the stages 3 and above */
	int stage_x[BA_MAX_STAGES], stage_y[BA_MAX_STAGES];
	int umi;                                    /**< BA_UMI_* */
	int x, y;                                   /**< loss limits (umi=loss; book missing; book reducecount) */
	bool xy_set;
	int lo, hi;                                 /**< value range, for the side to move at the current position (umi=range) */
	int depth;                                  /**< last move number followed by the umigame numbers */
	int parts;                                  /**< BA_PART_* (book moveinfo) */
	bool moves_set;                             /**< only some moves (book moveinfo) */
	unsigned long long moves;
	bool moves_pass;
	int sq;                                     /**< first move (book missing), NOMOVE if none */
	int stage;                                  /**< stage to reach (book missing), 0 if none */
	long long n;                                /**< lines to show (book missing, book findline) */
	bool n_set;
	bool all;                                   /**< scope=all (book levels) */
	int loss;                                   /**< largest loss of a move (book bestlines) */
	bool follow_solved;                         /**< solved=follow: the lines go on after a solved position (book bestlines) */
	bool incomplete_only;                       /**< which=incomplete: only the ends that are not solved are written (book bestlines) */
	char file[512];                             /**< file the lines are written to, "" if none (book bestlines) */
	Board target;                               /**< position to reach, side to move first (book findline) */
	int target_color;                           /**< its side to move */
	bool target_set;
	char error[64];                             /**< first wrong word */
} BaParams;

/** @return the square of a word ("a1" to "h8", "pa" or "ps" for a pass), NOMOVE if it is none. */
static int ba_parse_square(const char *s)
{
	const int c = (s[0] | 0x20), r = s[1];
	if ((c == 'p') && ((s[1] | 0x20) == 'a' || (s[1] | 0x20) == 's')) return PASS;
	if ('a' <= c && c <= 'h' && '1' <= r && r <= '8') return (r - '1') * 8 + (c - 'a');
	return NOMOVE;
}

/** @brief Write a square ("pa" for a pass). */
static void ba_square(const int x, char *s)
{
	if (x == PASS) { s[0] = 'p'; s[1] = 'a'; }
	else if (A1 <= x && x <= H8) { s[0] = (char) ('a' + x % 8); s[1] = (char) ('1' + x / 8); }
	else { s[0] = '-'; s[1] = '-'; }
	s[2] = '\0';
}

/**
 * @brief Read a position: 64 squares from a1 to h8 ('X' black, 'O' white, '-' empty), then the side to move ('X' or 'O').
 * @return false if the word is not one.
 */
static bool ba_parse_target(const char *s, Board *board, int *color)
{
	unsigned long long black = 0, white = 0;
	int i;

	if (strlen(s) != 65) return false;
	for (i = 0; i < 64; ++i) {
		const int c = s[i] | 0x20;
		if (c == 'x' || c == 'b' || s[i] == '*') black |= 1ULL << i;
		else if (c == 'o' || c == 'w') white |= 1ULL << i;
		else if (s[i] != '-' && s[i] != '.') return false;
	}
	i = s[64] | 0x20;
	if (i == 'x' || i == 'b') { *color = BLACK; board->player = black; board->opponent = white; }
	else if (i == 'o' || i == 'w') { *color = WHITE; board->player = white; board->opponent = black; }
	else return false;
	return true;
}

/** @return true if a whole word is a number (with a sign), read in *v. */
static bool ba_parse_number(const char *s, int *v)
{
	char *end;
	long x;
	if (*s == '\0') return false;
	x = strtol(s, &end, 10);
	if (*end != '\0' || x < -100000 || x > 100000) return false;
	*v = (int) x;
	return true;
}

/** @return true if a whole word is an int (with a sign), read in *v: the number of an analysis (a user interface counts them up). */
static bool ba_parse_id(const char *s, int *v)
{
	char *end;
	long long x;
	if (*s == '\0') return false;
	x = strtoll(s, &end, 10);
	if (*end != '\0' || x < INT_MIN || x > INT_MAX) return false;
	*v = (int) x;
	return true;
}

static void ba_params_default(BaParams *p)
{
	int i;
	memset(p, 0, sizeof *p);
	p->base = BA_DEVIATE;
	p->n_stage = 5;
	for (i = 0; i < 5; ++i) p->stage_x[i] = p->stage_y[i] = i + 1;
	p->umi = BA_UMI_BEST;
	p->x = 2; p->y = 4;
	p->lo = -SCORE_MAX; p->hi = SCORE_MAX;
	p->depth = 60;
	p->parts = BA_PART_STAGE | BA_PART_UMI | BA_PART_COUNT;
	p->sq = NOMOVE;
	p->n = 100;
}

/**
 * @brief Cut the next word "key=value" of a command: a value between double quotes may hold spaces
 * (file="C:\a b\lines.txt"; the quotes are dropped), the key is in small letters.
 * @param s The words (moved after the word).
 * @param cut Set if the word is longer than size - 1 letters (it is cut).
 * @return false at the end of the words.
 */
static bool ba_next_word(const char **s, char *word, const int size, bool *cut)
{
	const char *t = *s;
	bool quoted = false;
	int i;

	while (*t == ' ' || *t == '\t' || *t == '\r' || *t == '\n') ++t;
	if (*t == '\0') { *s = t; return false; }
	*cut = false;
	for (i = 0; *t && (quoted || (*t != ' ' && *t != '\t' && *t != '\r' && *t != '\n')); ++t) {
		if (*t == '"') { quoted = !quoted; continue; }
		if (i < size - 1) word[i++] = *t; else *cut = true;
	}
	word[i] = '\0';
	for (i = 0; word[i] && word[i] != '='; ++i) word[i] = (char) (('A' <= word[i] && word[i] <= 'Z') ? word[i] + 32 : word[i]);
	*s = t;
	return true;
}

/**
 * @brief Read the words "key=value" of an analysis.
 * @return false on a wrong word (p->error).
 */
static bool ba_params_read(const char *param, BaParams *p)
{
	char word[512], *value;
	const char *s = param;
	bool cut;
	int i, v;

	ba_params_default(p);
	while (ba_next_word(&s, word, (int) sizeof word, &cut)) {
		if (cut) goto wrong; // a word too long is not used cut (a file name would name another file)
		value = strchr(word, '=');
		if (value == NULL) goto wrong;
		*value++ = '\0';

		if (strcmp(word, "id") == 0) {
			if (!ba_parse_id(value, &v)) goto wrong;
		} else if (strcmp(word, "base") == 0) {
			if (strcmp(value, "deviate") == 0) p->base = BA_DEVIATE;
			else if (strcmp(value, "deviate2") == 0) p->base = BA_DEVIATE2;
			else if (strcmp(value, "deviate3") == 0) p->base = BA_DEVIATE3;
			else goto wrong;
		} else if (strcmp(word, "stages") == 0) {
			const char *t = value;
			p->n_stage = 0;
			while (*t) {
				char *end;
				long a, b;
				if (p->n_stage == BA_MAX_STAGES) goto wrong;
				a = strtol(t, &end, 10);
				if (end == t || *end != '/') goto wrong;
				t = end + 1;
				b = strtol(t, &end, 10);
				if (end == t || (*end != ',' && *end != '\0')) goto wrong;
				t = (*end == ',') ? end + 1 : end;
				if (a < 0 || a > 129 || b < 0 || b > 254) goto wrong;
				p->stage_x[p->n_stage] = (int) a;
				p->stage_y[p->n_stage] = (int) b;
				++p->n_stage;
			}
			if (p->n_stage == 0) goto wrong;
		} else if (strcmp(word, "umi") == 0) {
			if (strcmp(value, "best") == 0) p->umi = BA_UMI_BEST;
			else if (strcmp(value, "loss") == 0) p->umi = BA_UMI_LOSS;
			else if (strcmp(value, "range") == 0) p->umi = BA_UMI_RANGE;
			else if (strcmp(value, "off") == 0) p->umi = BA_UMI_OFF;
			else goto wrong;
		} else if (strcmp(word, "x") == 0) {
			if (!ba_parse_number(value, &v) || v < 0 || v > 129) goto wrong;
			p->x = v; p->xy_set = true;
		} else if (strcmp(word, "y") == 0) {
			if (!ba_parse_number(value, &v) || v < 0 || v > 254) goto wrong;
			p->y = v; p->xy_set = true;
		} else if (strcmp(word, "lo") == 0) {
			if (!ba_parse_number(value, &v) || v < -SCORE_MAX || v > SCORE_MAX) goto wrong;
			p->lo = v;
		} else if (strcmp(word, "hi") == 0) {
			if (!ba_parse_number(value, &v) || v < -SCORE_MAX || v > SCORE_MAX) goto wrong;
			p->hi = v;
		} else if (strcmp(word, "depth") == 0) {
			if (!ba_parse_number(value, &v) || v < 1 || v > 60) goto wrong;
			p->depth = v;
		} else if (strcmp(word, "parts") == 0) {
			const char *t = value;
			p->parts = 0;
			while (*t) {
				if (strncmp(t, "stage", 5) == 0) { p->parts |= BA_PART_STAGE; t += 5; }
				else if (strncmp(t, "umi", 3) == 0) { p->parts |= BA_PART_UMI; t += 3; }
				else if (strncmp(t, "count", 5) == 0) { p->parts |= BA_PART_COUNT; t += 5; }
				else goto wrong;
				if (*t == ',') ++t;
				else if (*t) goto wrong;
			}
		} else if (strcmp(word, "moves") == 0) {
			const char *t = value;
			p->moves_set = true;
			while (*t) {
				const int x = ba_parse_square(t);
				if (x == NOMOVE || t[1] == '\0') goto wrong;
				if (x == PASS) p->moves_pass = true; else p->moves |= 1ULL << x;
				t += 2;
				if (*t == ',') ++t;
			}
		} else if (strcmp(word, "sq") == 0) {
			if ((p->sq = ba_parse_square(value)) == NOMOVE || value[1] == '\0' || value[2] != '\0') goto wrong;
		} else if (strcmp(word, "stage") == 0) {
			if (!ba_parse_number(value, &v) || v < 2 || v > 2 + BA_MAX_STAGES) goto wrong;
			p->stage = v;
		} else if (strcmp(word, "n") == 0) {
			if (!ba_parse_number(value, &v) || v < 0) goto wrong;
			p->n = v;
			p->n_set = true;
		} else if (strcmp(word, "loss") == 0) {
			if (!ba_parse_number(value, &v) || v < 0 || v > 129) goto wrong;
			p->loss = v;
		} else if (strcmp(word, "solved") == 0) {
			if (strcmp(value, "stop") == 0) p->follow_solved = false;
			else if (strcmp(value, "follow") == 0) p->follow_solved = true;
			else goto wrong;
		} else if (strcmp(word, "which") == 0) {
			if (strcmp(value, "all") == 0) p->incomplete_only = false;
			else if (strcmp(value, "incomplete") == 0) p->incomplete_only = true;
			else goto wrong;
		} else if (strcmp(word, "file") == 0) {
			if (*value == '\0' || strlen(value) >= sizeof p->file) goto wrong;
			strcpy(p->file, value);
		} else if (strcmp(word, "target") == 0) {
			if (!ba_parse_target(value, &p->target, &p->target_color)) goto wrong;
			p->target_set = true;
		} else if (strcmp(word, "scope") == 0) {
			if (strcmp(value, "all") == 0) p->all = true;
			else if (strcmp(value, "from") == 0) p->all = false;
			else goto wrong;
		} else goto wrong;
	}
	if (p->stage > 2 + p->n_stage) { snprintf(p->error, sizeof p->error, "stage"); return false; }
	return true;

wrong:
	snprintf(p->error, sizeof p->error, "%.63s", word); // (a long word is cut)
	for (i = 0; p->error[i]; ++i) if (p->error[i] == ' ' || p->error[i] == '=') p->error[i] = '_';
	return false;
}

/**
 * @brief Read the number of the word "id=" of a command (the words cut as ba_params_read cuts them; the whole
 * value, of 31 letters at most, as ba_parse_id reads it).
 * @param id The number (0 if there is no "id=").
 * @return 1 if the value is an int, 0 if there is no "id=", -1 if the value is not an int.
 */
static int ba_read_id(const char *param, int *id)
{
	char word[35]; // "id=" and 31 letters
	bool cut;

	*id = 0;
	while (ba_next_word(&param, word, (int) sizeof word, &cut)) {
		if (strncmp(word, "id=", 3) == 0) return !cut && ba_parse_id(word + 3, id) ? 1 : -1;
	}
	return 0;
}

/* ------------------------------------------------------------------ book access ------------------------------------------------------------- */

/** @brief Number the positions of the book (as book_visit_init does), unless they are numbered already. */
static bool ba_index_get(const Book *book)
{
	unsigned int i, n = 0;

	if (ba.index.first && ba.index_book == book && ba.index.n_bucket == book->n) return true;
	free(ba.index.table);
	ba.index.table = NULL;
	ba.index.n_table = 0;
	free(ba.index.first);
	ba.index.first = (unsigned int*) malloc((size_t) book->n * sizeof *ba.index.first);
	if (ba.index.first == NULL) return false;
	for (i = 0; i < (unsigned int) book->n; ++i) {
		ba.index.first[i] = n;
		n += (unsigned int) book->array[i].n;
	}
	ba.index.n_bucket = book->n;
	ba.index.n = n;
	ba.index_book = book;
	return true;
}

/** @return the hash of a number (the hash tables of the analyses). */
static inline unsigned long long ba_hash(const unsigned long long i)
{
	unsigned long long h = (i + 1) * 0x9E3779B97F4A7C15ULL;
	return h ^ (h >> 29);
}

/* results kept while the book does not change (going back to a position gives them at once) */
enum { BA_CACHE_STAGE = 1, BA_CACHE_UMI = 2 };
#define BA_CACHE_SIZE (1 << 15)
#define BA_CACHE_PROBE 16

/**
 * What is known of the precision stage of a position (the root of the walks).
 *
 * B_k, for the range k of deviate2 / deviate3: the smallest d + e of the leaves that the walk of the range reaches
 * (d: smallest accumulated loss of the position of the leaf, e: loss of the leaf move; the links and the leaves of a
 * move loss above x_k are left out, except when ba_stage_total_ok, whose single walk has no move loss limit). The
 * range is complete (no leaf to expand) when B_k > y_k. B_k of a position is the smallest of the loss of its leaf and
 * of e + B_k of its next positions (e: loss of the link; walkable next positions only), so it is found from the next
 * positions, and B_k of a next position is at least B_k - e.
 * For deviate, lb[k] is y_k + 1 when the range is known complete, 0 otherwise.
 */
typedef struct BaStageInfo {
	int stage;
	long long missing;
	bool found;                         /**< a line of best moves reaches a solved position */
	bool recursive;                     /**< a walk of book deviate went recursive (a position reached with both parities) */
	unsigned char lb[BA_MAX_STAGES];    /**< B_k is at least lb[k] (255: complete) */
	unsigned char exact;                /**< ranges whose B_k is lb[k] */
} BaStageInfo;

typedef struct BaCacheEntry {
	unsigned long long key;     /**< number of the position << 8 | kind << 1 | side; 0: empty */
	unsigned long long cond;    /**< hash of the parameters the result depends on */
	long long a, b;
	int c;
	unsigned char lb[BA_MAX_STAGES], exact; /**< BaStageInfo (BA_CACHE_STAGE) */
} BaCacheEntry;

static BaCacheEntry *ba_cache;

static inline unsigned long long ba_cache_key(const unsigned int idx, const int kind, const int color)
{
	return (((unsigned long long) idx + 1) << 8) | ((unsigned long long) kind << 1) | (unsigned long long) color;
}

/** @return true if a result is kept (the lock of the analyses is taken here); lb and exact may be NULL. */
static bool ba_cache_get(const unsigned long long key, const unsigned long long cond, long long *a, long long *b, int *c, unsigned char *lb, unsigned char *exact)
{
	bool found = false;
	int i;
	lock(&ba);
	if (ba_cache) for (i = 0; i < BA_CACHE_PROBE; ++i) {
		const BaCacheEntry *e = ba_cache + ((ba_hash(key ^ cond) + (unsigned long long) i) & (BA_CACHE_SIZE - 1));
		if (e->key == 0) break;
		if (e->key == key && e->cond == cond) {
			*a = e->a; *b = e->b; *c = e->c;
			if (lb) memcpy(lb, e->lb, sizeof e->lb);
			if (exact) *exact = e->exact;
			found = true;
			break;
		}
	}
	unlock(&ba);
	return found;
}

static void ba_cache_put(const unsigned long long key, const unsigned long long cond, const long long a, const long long b, const int c, const unsigned char *lb, const unsigned char exact)
{
	int i;
	lock(&ba);
	if (ba_cache == NULL) ba_cache = (BaCacheEntry*) calloc(BA_CACHE_SIZE, sizeof *ba_cache);
	if (ba_cache) {
		for (i = 0; i < BA_CACHE_PROBE; ++i) {
			BaCacheEntry *e = ba_cache + ((ba_hash(key ^ cond) + (unsigned long long) i) & (BA_CACHE_SIZE - 1));
			if (e->key == 0 || (e->key == key && e->cond == cond)) {
				e->key = key; e->cond = cond; e->a = a; e->b = b; e->c = c;
				if (lb) memcpy(e->lb, lb, sizeof e->lb); else memset(e->lb, 0, sizeof e->lb);
				e->exact = exact;
				break;
			}
		}
		if (i == BA_CACHE_PROBE) memset(ba_cache, 0, BA_CACHE_SIZE * sizeof *ba_cache); // (full: start again)
	}
	unlock(&ba);
}

static unsigned long long ba_mix(unsigned long long h, const long long x)
{
	return (h ^ (unsigned long long) x) * 0x100000001B3ULL;
}

/** @return the hash of the parameters of the stages. */
static unsigned long long ba_cond_stage(const BaParams *p)
{
	unsigned long long h = ba_mix(0xcbf29ce484222325ULL, p->base);
	int k;
	h = ba_mix(h, p->n_stage);
	for (k = 0; k < p->n_stage; ++k) h = ba_mix(ba_mix(h, p->stage_x[k]), p->stage_y[k]);
	return h;
}

/** @return the hash of the parameters of the umigame numbers. */
static unsigned long long ba_cond_umi(const BaParams *p)
{
	unsigned long long h = ba_mix(0xcbf29ce484222325ULL, p->umi);
	h = ba_mix(ba_mix(h, p->x), p->y);
	h = ba_mix(ba_mix(h, p->lo), p->hi);
	return ba_mix(h, p->depth);
}

/** @return true if the stage of a position is kept (BA_CACHE_STAGE). */
static bool ba_cache_get_stage(const unsigned int idx, const unsigned long long cond, BaStageInfo *s)
{
	long long a, b;
	int c;
	if (!ba_cache_get(ba_cache_key(idx, BA_CACHE_STAGE, 0), cond, &a, &b, &c, s->lb, &s->exact)) return false;
	s->stage = (int) a;
	s->missing = b;
	s->found = (c & 1) != 0;
	s->recursive = (c & 2) != 0;
	return true;
}

static void ba_cache_put_stage(const unsigned int idx, const unsigned long long cond, const BaStageInfo *s)
{
	ba_cache_put(ba_cache_key(idx, BA_CACHE_STAGE, 0), cond, s->stage, s->missing, (s->found ? 1 : 0) | (s->recursive ? 2 : 0), s->lb, s->exact);
}

/* positions reached from a position and their levels (book moveinfo parts=count, book levels), kept while the book does not change */
#define BA_REACHED_SIZE 4096
#define BA_REACHED_PROBE 8

typedef struct BaReached {
	unsigned int key;       /**< number of the position + 1; 0: empty */
	long long n;
	long long hist[61];
} BaReached;

static BaReached *ba_reached;

/** @return true if the positions reached from a position are kept. */
static bool ba_reached_get(const unsigned int idx, long long *n, long long hist[61])
{
	bool found = false;
	int i;
	lock(&ba);
	if (ba_reached) for (i = 0; i < BA_REACHED_PROBE; ++i) {
		const BaReached *e = ba_reached + ((ba_hash(idx + 1) + (unsigned long long) i) & (BA_REACHED_SIZE - 1));
		if (e->key == 0) break;
		if (e->key == idx + 1) {
			*n = e->n;
			memcpy(hist, e->hist, sizeof e->hist);
			found = true;
			break;
		}
	}
	unlock(&ba);
	return found;
}

static void ba_reached_put(const unsigned int idx, const long long n, const long long hist[61])
{
	int i;
	lock(&ba);
	if (ba_reached == NULL) ba_reached = (BaReached*) calloc(BA_REACHED_SIZE, sizeof *ba_reached);
	if (ba_reached) {
		for (i = 0; i < BA_REACHED_PROBE; ++i) {
			BaReached *e = ba_reached + ((ba_hash(idx + 1) + (unsigned long long) i) & (BA_REACHED_SIZE - 1));
			if (e->key == 0 || e->key == idx + 1) {
				e->key = idx + 1; e->n = n;
				memcpy(e->hist, hist, sizeof e->hist);
				break;
			}
		}
		if (i == BA_REACHED_PROBE) memset(ba_reached, 0, BA_REACHED_SIZE * sizeof *ba_reached); // (full: start again)
	}
	unlock(&ba);
}

/** @brief Forget the numbering of the positions (the book may change). */
static void ba_index_free(void)
{
	free(ba_cache);
	ba_cache = NULL;
	free(ba_reached);
	ba_reached = NULL;
	free(ba.index.first);
	ba.index.first = NULL;
	free(ba.index.table);
	ba.index.table = NULL;
	ba.index.n_table = 0;
	ba.index_book = NULL;
	ba.index.n = 0;
	ba.index.n_bucket = 0;
}

/** Marks of the positions of a walk: a small hash table, or one byte per position of the book when many are marked. */
typedef struct BaVisit {
	unsigned long long *slot;   /**< (number + 1) << 8 | mark; 0: empty */
	unsigned long long mask;    /**< slots - 1 */
	unsigned long long n;       /**< marked positions */
	unsigned char *dense;       /**< one mark per position (NULL: hash table) */
	unsigned long long n_index; /**< positions of the book */
} BaVisit;

static bool ba_counted_give_locked(void);

/**
 * @return true if memory for dense marks may be used (at most 6 bytes per position of the book for all the walks; when
 * it is not enough, the marks of the positions counted by book reducecount give theirs, see ba_counted).
 */
static bool ba_dense_reserve(const unsigned long long n)
{
	bool ok;
	lock(&ba);
	ok = (unsigned long long) ba.dense_bytes + n <= 6 * ba.index.n + (1u << 20);
	if (!ok && ba_counted_give_locked()) ok = (unsigned long long) ba.dense_bytes + n <= 6 * ba.index.n + (1u << 20);
	if (ok) ba.dense_bytes += (long long) n;
	unlock(&ba);
	return ok;
}

static void ba_dense_release(const unsigned long long n)
{
	lock(&ba);
	ba.dense_bytes -= (long long) n;
	unlock(&ba);
}

static void ba_visit_init(BaVisit *v, const unsigned long long n_index)
{
	memset(v, 0, sizeof *v);
	v->n_index = n_index;
}

static void ba_visit_free(BaVisit *v)
{
	free(v->slot);
	if (v->dense) { free(v->dense); ba_dense_release(v->n_index); }
	memset(v, 0, sizeof *v);
}

/** @brief Remove all the marks (the memory is kept). */
static void ba_visit_clear(BaVisit *v)
{
	if (v->dense) memset(v->dense, 0, (size_t) v->n_index);
	else if (v->slot) {
		if (v->mask >= 65536 && v->n * 16 < v->mask) { free(v->slot); v->slot = NULL; v->mask = 0; } // (a large table almost empty)
		else memset(v->slot, 0, (size_t) (v->mask + 1) * sizeof *v->slot);
	}
	v->n = 0;
}

/** @return the mark of a position (0: none). */
static inline unsigned char ba_visit_get(const BaVisit *v, const unsigned int i)
{
	unsigned long long k, s;
	if (v->dense) return v->dense[i];
	if (v->slot == NULL) return 0;
	for (k = ba_hash(i) & v->mask; (s = v->slot[k]) != 0; k = (k + 1) & v->mask) {
		if ((s >> 8) == (unsigned long long) i + 1) return (unsigned char) s;
	}
	return 0;
}

/** @brief Put marks in a new table. */
static bool ba_visit_grow(BaVisit *v)
{
	const unsigned long long size = v->slot ? 2 * (v->mask + 1) : 1024;
	unsigned long long *slot, k, j;

	if (v->slot && v->n > v->n_index / 16 && ba_dense_reserve(v->n_index)) { // as many marks as positions / 16: one byte per position
		unsigned char *dense = (unsigned char*) calloc((size_t) v->n_index + 1, 1);
		if (dense) {
			for (k = 0; k <= v->mask; ++k) if (v->slot[k]) dense[(v->slot[k] >> 8) - 1] = (unsigned char) v->slot[k];
			free(v->slot); v->slot = NULL; v->mask = 0;
			v->dense = dense;
			return true;
		}
		ba_dense_release(v->n_index);
	}
	slot = (unsigned long long*) calloc((size_t) size, sizeof *slot);
	if (slot == NULL) return false;
	if (v->slot) {
		for (k = 0; k <= v->mask; ++k) if (v->slot[k]) {
			for (j = ba_hash((v->slot[k] >> 8) - 1) & (size - 1); slot[j]; j = (j + 1) & (size - 1)) ;
			slot[j] = v->slot[k];
		}
		free(v->slot);
	}
	v->slot = slot;
	v->mask = size - 1;
	return true;
}

/** @brief Set the mark of a position (1 to 255; 0 removes it). @return false if memory is exhausted. */
static bool ba_visit_set(BaVisit *v, const unsigned int i, const unsigned char mark)
{
	unsigned long long k, s;

	if (v->dense) {
		if (v->dense[i] == 0 && mark) ++v->n;
		v->dense[i] = mark;
		return true;
	}
	if ((v->n + 1) * 2 > (v->slot ? v->mask + 1 : 0)) {
		if (!ba_visit_grow(v)) return false;
		if (v->dense) return ba_visit_set(v, i, mark);
	}
	for (k = ba_hash(i) & v->mask; (s = v->slot[k]) != 0; k = (k + 1) & v->mask) {
		if ((s >> 8) == (unsigned long long) i + 1) { v->slot[k] = (s & ~0xffULL) | mark; return true; }
	}
	v->slot[k] = (((unsigned long long) i + 1) << 8) | mark;
	++v->n;
	return true;
}

/** A position reached by a walk. */
typedef struct BaItem {
	Position *p;
	unsigned int idx;       /**< number of the position */
	int parent;             /**< item of the previous position, -1 for the root */
	unsigned char move;     /**< move (on the board of the previous position) */
	unsigned char depth;    /**< moves from the root */
	unsigned short loss;    /**< accumulated loss (deviate2, deviate3) */
} BaItem;

typedef struct BaQueue {
	BaItem *item;
	long long n, size;
} BaQueue;

#ifdef BOOK_ANALYZE_TEST
static long long ba_test_alloc_fail = -1; /**< allocations of the walks that succeed before one fails, -1: none fails (test builds) */
#define BA_TEST_ALLOC_FAILS() (ba_test_alloc_fail >= 0 && ba_test_alloc_fail-- == 0)
#else
#define BA_TEST_ALLOC_FAILS() false
#endif

/** @return false if memory is exhausted, or if the item or its parent has a number that does not fit in BaItem.parent. */
static bool ba_push(BaQueue *q, Position *p, const unsigned int idx, const long long parent, const int move, const int depth, const int loss)
{
	// (the number of an item is the parent of its next items: up to v4.5.5-nikque.26 a parent was checked only when
	// the queue grew, and a parent above INT_MAX became negative)
	if (parent > INT_MAX || q->n > INT_MAX) return false;
	if (q->n == q->size) {
		const long long size = q->size + q->size / 2 + 1024;
		BaItem *item = BA_TEST_ALLOC_FAILS() ? NULL : (BaItem*) realloc(q->item, (size_t) size * sizeof *item);
		if (item == NULL) return false;
		q->item = item; q->size = size;
	}
	q->item[q->n].p = p;
	q->item[q->n].idx = idx;
	q->item[q->n].parent = (int) parent;
	q->item[q->n].move = (unsigned char) move;
	q->item[q->n].depth = (unsigned char) (depth > 255 ? 255 : depth);
	q->item[q->n].loss = (unsigned short) loss;
	++q->n;
	return true;
}

/** A list of numbers. */
typedef struct BaList {
	long long *v;
	long long n, size;
} BaList;

static bool ba_list_push(BaList *l, const long long x)
{
	if (l->n == l->size) {
		const long long size = l->size + l->size / 2 + 256;
		long long *v = (long long*) realloc(l->v, (size_t) size * sizeof *v);
		if (v == NULL) return false;
		l->v = v; l->size = size;
	}
	l->v[l->n++] = x;
	return true;
}

/** Positions whose leaf a walk would expand. */
typedef struct BaTodo {
	BaVisit seen;
	long long n;        /**< distinct positions with a leaf move */
	bool keep;          /**< keep the lines */
	BaList ref;         /**< lines: queue << 40 | item */
	BaList added;       /**< numbers of the positions found, in order (the positions of a stopped walk are forgotten) */
} BaTodo;

/* ------------------------------------------------------------------ umigame (shared positions) ---------------------------------------------- */

/** A position of the lines of an umigame number. */
typedef struct BaUNode {
	Position *p;
	unsigned int idx;
	unsigned int edge;      /**< first next position (in edge) */
	unsigned int n_edge;
	unsigned int indeg;     /**< lines that lead to it */
	unsigned int alias;     /**< node of the same end (loss condition), else itself */
	unsigned short loss;
	unsigned char color;    /**< side to move */
	unsigned char key;      /**< 2 * discs + (side to move can move): the next positions have a greater key */
	unsigned char depth;    /**< moves from the root (the fewest; 255 at most) */
	bool end;               /**< end of the lines */
} BaUNode;

/** A term of an expression: c ends plus the ends of a set of shared positions (counted once). */
typedef struct BaTerm {
	unsigned long long c;
	unsigned int off, len;  /**< set (in the pool), increasing ranks */
} BaTerm;

/** The terms (alternatives) of an expression: its value is the smallest of them. */
typedef struct BaTerms {
	BaTerm *t;
	int n, size;
} BaTerms;

/** Hash table: key -> number. */
typedef struct BaMap {
	unsigned long long *key; /**< key + 1, 0: empty */
	unsigned int *val;
	unsigned long long mask, n;
} BaMap;

/* ------------------------------------------------------------------ walk state ------------------------------------------------------------ */

/** Walk state of a thread. */
typedef struct BaWork {
	BaJob *job;
	Book *book;
	const BaParams *prm;
	BaVisit visit;
	BaTodo todo;
	BaQueue q[2];
	BaList *bucket;         /**< deviate2/3: items per accumulated loss */
	int n_bucket;
	bool oom;
	bool recursive;         /**< a walk of book deviate went recursive (ba_deviate_pass) */
	int leaf_min;           /**< smallest d + e of the leaves found by the last ba_expansions of deviate2/3, INT_MAX if none */

	/* umigame */
	BaUNode *node;
	unsigned int n_node, size_node;
	unsigned int *edge;
	unsigned long long n_edge, size_edge;
	BaMap map, end_map;
	unsigned int *order;    /**< nodes, the next positions first */
	unsigned int *rank;     /**< place of each node in order */
	BaTerms *rep;           /**< expression of each node seen from the previous positions */
	BaTerms *def;           /**< expression of each shared node */
	long long *g;           /**< value of each shared node, -1 if not computed */
	int *uses;              /**< expressions that use each shared node (by rank) */
	unsigned int *remain;    /**< previous positions of each node not done yet */
	unsigned int *stamp, *found, stamp_now; /**< (shared nodes of an expression, each once) */
	unsigned int *pool;     /**< sets of the terms */
	unsigned long long n_pool, size_pool;
	unsigned long long pool_kept;  /**< numbers in the pool after the last ba_pool_compact */
	bool inexact;           /**< terms were dropped: the value may be too large */
	bool too_large;
	int keep_depth;         /**< the values of the nodes 1 to keep_depth moves from the root are kept (see ba_umi_keep_value) */
	long long *kept;       /**< values of the nodes as counted from each alone, by node and side; -1: not known */
} BaWork;

#define BA_TERM_MAX 128                     /**< terms kept in an expression (more: the value is an upper bound, that a choice of moves reaches) */
#define BA_POOL_MAX (1ULL << 27)            /**< numbers in the sets of the terms */
#define BA_UMI_NODE_MAX (1u << 24)          /**< positions of the lines of an umigame number */

static void ba_work_init(BaWork *w, BaJob *job, const BaParams *prm)
{
	memset(w, 0, sizeof *w);
	w->job = job;
	w->book = job->book;
	w->prm = prm;
	ba_visit_init(&w->visit, ba.index.n);
	ba_visit_init(&w->todo.seen, ba.index.n);
}

static void ba_map_free(BaMap *m)
{
	free(m->key); free(m->val);
	memset(m, 0, sizeof *m);
}

/** @brief Forget the expressions of an umigame number (the lines are kept). */
static void ba_umi_clear(BaWork *w)
{
	unsigned int i;
	if (w->rep) for (i = 0; i < w->n_node; ++i) free(w->rep[i].t);
	if (w->def) for (i = 0; i < w->n_node; ++i) free(w->def[i].t);
	free(w->rep); free(w->def); free(w->g);
	free(w->uses); free(w->remain); free(w->stamp); free(w->found);
	w->uses = NULL; w->remain = w->stamp = w->found = NULL;
	w->rep = w->def = NULL; w->g = NULL;
	w->n_pool = 0;
}

/** @brief Forget the lines of an umigame number (and their expressions). */
static void ba_umi_free(BaWork *w)
{
	ba_umi_clear(w);
	free(w->node); free(w->edge); free(w->order); free(w->rank); free(w->pool); free(w->kept);
	w->kept = NULL;
	w->node = NULL; w->edge = NULL; w->order = w->rank = NULL; w->pool = NULL;
	w->n_node = w->size_node = 0; w->n_edge = w->size_edge = 0; w->size_pool = 0;
	ba_map_free(&w->map); ba_map_free(&w->end_map);
}

static void ba_work_free(BaWork *w)
{
	int i;
	ba_visit_free(&w->visit);
	ba_visit_free(&w->todo.seen);
	free(w->todo.ref.v);
	free(w->todo.added.v);
	free(w->q[0].item); free(w->q[1].item);
	for (i = 0; i < w->n_bucket; ++i) free(w->bucket[i].v);
	free(w->bucket);
	ba_umi_free(w);
	memset(w, 0, sizeof *w);
}

/**
 * @brief Find a position in the book, with its number.
 * @param w Walk.
 * @param board Board (any symetry).
 * @param index Number of the position (out).
 * @return the position, NULL if it is not in the book.
 */
static Position* ba_probe(const BaWork *w, const Board *board, unsigned int *index)
{
	Board unique;
	const PositionArray *a;
	unsigned long long b;
	int i;

	board_unique(board, &unique);
	b = board_get_hash_code(&unique) & (unsigned long long) (w->book->n - 1);
	a = w->book->array + b;
	for (i = 0; i < a->n; ++i) {
		if (board_equal(&a->positions[i].board, &unique)) {
			*index = ba.index.first[b] + (unsigned int) i;
			return a->positions + i;
		}
	}
	return NULL;
}

static void ba_todo_reset(BaTodo *t, const bool keep)
{
	ba_visit_clear(&t->seen);
	t->n = 0;
	t->keep = keep;
	t->ref.n = 0;
	t->added.n = 0;
}

/** @brief A walk would expand the leaf of a position. @return false if memory is exhausted. */
static bool ba_todo_add(BaTodo *t, const Position *p, const unsigned int idx, const int which, const long long item)
{
	if (p->leaf.move == NOMOVE || ba_visit_get(&t->seen, idx)) return true; // (book_expand does nothing without a leaf move)
	if (!ba_visit_set(&t->seen, idx, 1)) return false;
	++t->n;
	if (!ba_list_push(&t->added, idx)) return false;
	return !t->keep || ba_list_push(&t->ref, ((long long) which << 40) | item);
}

static inline bool ba_walkable(const Book *book, const Position *p)
{
	return board_count_empties(&p->board) >= book->options.n_empties && !board_is_game_over(&p->board);
}

/* ------------------------------------------------------------------ deviate walks --------------------------------------------------------- */

/**
 * @brief One walk of book deviate (as book_deviate_by_depth, ply by ply, without threads).
 * @return 0: done, 1: a position is reached with both ply parities (the recursive walk decides), -1: stopped.
 */
static int ba_walk_deviate(BaWork *w, const int which, Position *root, const unsigned int root_idx, const int a, const int b, const int lower, const int upper)
{
	BaQueue *q = w->q + which;
	const Book *book = w->book;
	long long start = 0, end, i;
	int depth;

	ba_visit_clear(&w->visit);
	q->n = 0;
	if (!ba_push(q, root, root_idx, -1, NOMOVE, 0, 0)) return -1;
	for (depth = 0; start < q->n; ++depth) {
		const int parity = depth & 1;
		const int deviation = parity ? b : a;
		const int lo = parity ? -upper : lower, up = parity ? -lower : upper;
		const unsigned char mark = (unsigned char) (parity + 1), child_mark = (unsigned char) ((parity ^ 1) + 1);

		end = q->n;
		for (i = start; i < end; ++i) {
			Position *p = q->item[i].p;
			const unsigned int idx = q->item[i].idx;
			const int v = p->score.value;
			unsigned char m;
			Link *l;

			if (ba_cancelled(w->job)) return -1;
			if (!(lo <= v && v <= up && ba_walkable(book, p))) continue;
			m = ba_visit_get(&w->visit, idx);
			if (m) {
				if (m != mark) return 1;
				continue;
			}
			if (!ba_visit_set(&w->visit, idx, mark)) return -1;
			foreach_link(l, p) {
				if (v - l->score <= deviation && lo <= l->score && l->score <= up) {
					Board target;
					unsigned int ci;
					Position *child;
					board_next(&p->board, l->move, &target);
					child = ba_probe(w, &target, &ci);
					if (child && ba_visit_get(&w->visit, ci) != child_mark && !ba_push(q, child, ci, i, l->move, depth + 1, 0)) return -1;
				}
			}
			if (v - p->leaf.score <= deviation && lo <= p->leaf.score && p->leaf.score <= up && !ba_todo_add(&w->todo, p, idx, which, i)) return -1;
		}
		start = end;
		if (!w->todo.keep && start >= 65536) { // (the walked positions are not needed: the queue keeps the next ply only)
			memmove(q->item, q->item + start, (size_t) (q->n - start) * sizeof *q->item);
			q->n -= start;
			start = 0;
		}
	}
	return 0;
}

/** @brief One walk of book deviate, as position_deviate (the walk used when a position is reached with both parities). */
static int ba_deviate_rec(BaWork *w, const int which, const long long item, const int player_deviation, const int opponent_deviation, const int lower, const int upper)
{
	BaQueue *q = w->q + which;
	Position *p = q->item[item].p;
	const unsigned int idx = q->item[item].idx;
	const int depth = q->item[item].depth;
	const int v = p->score.value;
	Link *l;

	if (ba_cancelled(w->job)) return -1;
	if (!ba_visit_get(&w->visit, idx) && lower <= v && v <= upper && ba_walkable(w->book, p)) {
		if (!ba_visit_set(&w->visit, idx, 1)) return -1;
		foreach_link(l, p) {
			if (v - l->score <= player_deviation && lower <= l->score && l->score <= upper) {
				Board target;
				unsigned int ci;
				Position *child;
				board_next(&p->board, l->move, &target);
				child = ba_probe(w, &target, &ci);
				if (child) {
					if (!ba_push(q, child, ci, item, l->move, depth + 1, 0)) return -1;
					if (ba_deviate_rec(w, which, q->n - 1, opponent_deviation, player_deviation, -upper, -lower) < 0) return -1;
				}
			}
		}
		if (v - p->leaf.score <= player_deviation && lower <= p->leaf.score && p->leaf.score <= upper && !ba_todo_add(&w->todo, p, idx, which, item)) return -1;
	}
	return 0;
}

/**
 * @brief One walk of book deviate from a root: ply by ply, or recursive when a position is reached with both
 * ply parities (as book_select_deviate). The positions found by the stopped ply walk are forgotten first.
 * @return 0: done, -1: stopped.
 */
static int ba_deviate_pass(BaWork *w, const int which, Position *root, const unsigned int root_idx, const int a, const int b, const int lower, const int upper)
{
	BaTodo *t = &w->todo;
	const long long n_added = t->added.n, n_ref = t->ref.n;
	long long k;
	const int r = ba_walk_deviate(w, which, root, root_idx, a, b, lower, upper);

	if (r != 1) return r;
	w->recursive = true;
	for (k = n_added; k < t->added.n; ++k) if (!ba_visit_set(&t->seen, (unsigned int) t->added.v[k], 0)) return -1;
	t->n -= t->added.n - n_added;
	t->added.n = n_added;
	t->ref.n = n_ref;
	ba_visit_clear(&w->visit);
	w->q[which].n = 0;
	if (!ba_push(w->q + which, root, root_idx, -1, NOMOVE, 0, 0)) return -1;
	return ba_deviate_rec(w, which, 0, a, b, lower, upper);
}

/**
 * @brief Start a walk by accumulated loss (deviate2, deviate3) from a root: the lists of the losses 0 to max_loss
 * empty, and the root, if it is walked, in the list of loss 0.
 * @return false if memory is exhausted.
 */
static bool ba_total_start(BaWork *w, BaQueue *q, Position *root, const unsigned int root_idx, const int max_loss)
{
	int i;

	if (w->n_bucket < max_loss + 1) {
		BaList *bucket = (BaList*) realloc(w->bucket, (size_t) (max_loss + 1) * sizeof *bucket);
		if (bucket == NULL) return false;
		for (i = w->n_bucket; i <= max_loss; ++i) memset(bucket + i, 0, sizeof bucket[i]);
		w->bucket = bucket; w->n_bucket = max_loss + 1;
	}
	for (i = 0; i <= max_loss; ++i) w->bucket[i].n = 0;
	ba_visit_clear(&w->visit);
	q->n = 0;
	if (!ba_walkable(w->book, root)) return true;
	return ba_visit_set(&w->visit, root_idx, 1) && ba_push(q, root, root_idx, -1, NOMOVE, 0, 0) && ba_list_push(w->bucket, 0);
}

/**
 * @brief One walk of book deviate2 / deviate3 (as book_deviate_total_by_loss, without threads).
 * @return 0: done, -1: stopped.
 */
static int ba_walk_total(BaWork *w, const int which, Position *root, const unsigned int root_idx, const int mode, const int move_loss, const int total_loss)
{
	BaQueue *q = w->q + which;
	const Book *book = w->book;
	int L;

	if (!ba_total_start(w, q, root, root_idx, total_loss)) return -1;
	for (L = 0; L <= total_loss; ++L) {
		BaList *cur = w->bucket + L;
		long long k;
		for (k = 0; k < cur->n; ++k) { // (positions reached with the same loss are added while the list is read)
			const long long item = cur->v[k];
			Position *p = q->item[item].p;
			const unsigned int idx = q->item[item].idx;
			const int depth = q->item[item].depth;
			const int v = p->score.value;
			int move_error;
			Link *l;

			if (ba_cancelled(w->job)) return -1;
			if (ba_visit_get(&w->visit, idx) != L + 1) continue; // reached later with a smaller loss
			foreach_link(l, p) {
				move_error = v - l->score;
				if (0 <= move_error && move_error <= move_loss && L + move_error <= total_loss) {
					Board target;
					unsigned int ci;
					Position *child;
					unsigned char old;
					board_next(&p->board, l->move, &target);
					child = ba_probe(w, &target, &ci);
					if (child == NULL || !ba_walkable(book, child)) continue;
					old = ba_visit_get(&w->visit, ci);
					if (old && old <= L + move_error + 1) continue;
					if (!ba_visit_set(&w->visit, ci, (unsigned char) (L + move_error + 1))
					 || !ba_push(q, child, ci, item, l->move, depth + 1, L + move_error)
					 || !ba_list_push(w->bucket + L + move_error, q->n - 1)) return -1;
					cur = w->bucket + L; // (the list may have moved)
				}
			}
			if (mode == BA_DEVIATE2 && position_is_solved(p)) continue;
			move_error = v - p->leaf.score;
			if (p->leaf.move != NOMOVE && 0 <= move_error && move_error <= move_loss && L + move_error <= total_loss) {
				if (!ba_todo_add(&w->todo, p, idx, which, item)) return -1;
				if (L + move_error < w->leaf_min) w->leaf_min = L + move_error;
			}
		}
	}
	return 0;
}

/**
 * @brief Positions whose leaf "book deviate x y" (deviate2, deviate3) started from a root would expand in its first round.
 * @return their number, -1 if stopped.
 */
static long long ba_expansions(BaWork *w, Position *root, const unsigned int root_idx, const int base, const int x, const int y, const bool keep)
{
	ba_todo_reset(&w->todo, keep);
	w->leaf_min = INT_MAX;
	if (base == BA_DEVIATE) {
		// book_deviate: the deviations of the side to move at the root, then those of the other side
		// (when the first walk expands nothing, the second walk sees the same book)
		const int s = root->score.value;
		if (ba_deviate_pass(w, 0, root, root_idx, x, 0, s - y, s + y) < 0) return -1;
		if (ba_deviate_pass(w, 1, root, root_idx, 0, x, s - y, s + y) < 0) return -1;
	} else {
		if (ba_walk_total(w, 0, root, root_idx, base, x, y) < 0) return -1;
	}
	return w->todo.n;
}

/**
 * @brief Lines of best moves of both sides from a root.
 * @param found true if one reaches a position solved exactly (out).
 * @return the number of their ends that are not solved (positions without a best move in the book), -1 if stopped.
 */
static long long ba_best_lines(BaWork *w, Position *root, const unsigned int root_idx, bool *found, const bool keep)
{
	BaQueue *q = w->q;
	long long i, ends = 0;

	*found = false;
	ba_todo_reset(&w->todo, keep);
	ba_visit_clear(&w->visit);
	q->n = 0;
	if (!ba_visit_set(&w->visit, root_idx, 1) || !ba_push(q, root, root_idx, -1, NOMOVE, 0, 0)) return -1;
	for (i = 0; i < q->n; ++i) {
		Position *p = q->item[i].p;
		const int v = p->score.value, depth = q->item[i].depth;
		bool next = false;
		Link *l;

		if (ba_cancelled(w->job)) return -1;
		if (board_is_game_over(&p->board) || position_is_solved(p)) { *found = true; continue; }
		foreach_link(l, p) {
			if (l->score == v) {
				Board target;
				unsigned int ci;
				Position *child;
				board_next(&p->board, l->move, &target);
				child = ba_probe(w, &target, &ci);
				if (child) {
					next = true;
					if (!ba_visit_get(&w->visit, ci) && (!ba_visit_set(&w->visit, ci, 1) || !ba_push(q, child, ci, i, l->move, depth + 1, 0))) return -1;
				}
			}
		}
		if (!next) {
			++ends;
			if (keep && !ba_list_push(&w->todo.ref, i)) return -1;
		}
	}
	return ends;
}

/** @return true if the stages of deviate2 / deviate3 can be found by one walk (see ba_stage_total). */
static bool ba_stage_total_ok(const BaParams *prm)
{
	int k;
	if (prm->base == BA_DEVIATE) return false;
	for (k = 0; k < prm->n_stage; ++k) {
		if (prm->stage_x[k] < prm->stage_y[k]) return false;
		if (k > 0 && prm->stage_y[k] < prm->stage_y[k - 1]) return false;
	}
	return true;
}

/**
 * @brief Stage of deviate2 / deviate3 with one walk, when each range allows a move loss as large as its total loss
 * (the default ranges 1/1 to 5/5).
 *
 * A path whose total loss is at most y has no move loss above y, so with x >= y the positions walked by
 * "deviate2 x y" are those whose smallest accumulated loss d is at most y, and the leaf of such a position is
 * expanded if d + e <= y (e: loss of the leaf move). The walk of the widest range thus gives every stage:
 * the first stage whose y reaches the smallest d + e is the first one that is not complete. The positions
 * are walked by increasing d, up to the y of that stage only.
 *
 * @param limit0 largest d + e to look for (the y of the last range, or of the stage when the smallest d + e is known).
 * @param leaf_min smallest d + e of the leaves (B of BaStageInfo), INT_MAX if none is at most *reach (out).
 * @param reach largest d + e looked for (out).
 * @return the stage (2 to 2 + n_stage), -1 if stopped.
 */
static int ba_stage_total(BaWork *w, Position *root, const unsigned int root_idx, const int limit0, long long *missing, int *leaf_min, int *reach)
{
	const BaParams *prm = w->prm;
	const Book *book = w->book;
	BaQueue *q = w->q;
	const int y_max = prm->stage_y[prm->n_stage - 1];
	int limit = limit0, best = INT_MAX, L, k;
	BaList found = {NULL, 0, 0}; // d + e of the leaves (one per position)

	if (!ba_total_start(w, q, root, root_idx, y_max)) goto stop;
	for (L = 0; L <= limit; ++L) {
		long long j;
		for (j = 0; j < w->bucket[L].n; ++j) {
			const long long item = w->bucket[L].v[j];
			Position *p = q->item[item].p;
			const int v = p->score.value, depth = q->item[item].depth;
			int e;
			Link *l;

			if (ba_cancelled(w->job)) goto stop;
			if (ba_visit_get(&w->visit, q->item[item].idx) != L + 1) continue; // reached later with a smaller loss
			foreach_link(l, p) {
				e = v - l->score;
				if (0 <= e && L + e <= limit) {
					Board target;
					unsigned int ci;
					Position *child;
					unsigned char old;
					board_next(&p->board, l->move, &target);
					child = ba_probe(w, &target, &ci);
					if (child == NULL || !ba_walkable(book, child)) continue;
					old = ba_visit_get(&w->visit, ci);
					if (old && old <= L + e + 1) continue;
					if (!ba_visit_set(&w->visit, ci, (unsigned char) (L + e + 1))
					 || !ba_push(q, child, ci, item, l->move, depth + 1, L + e)
					 || !ba_list_push(w->bucket + L + e, q->n - 1)) goto stop;
				}
			}
			if (prm->base == BA_DEVIATE2 && position_is_solved(p)) continue;
			e = v - p->leaf.score;
			if (p->leaf.move != NOMOVE && 0 <= e && L + e <= limit) {
				if (!ba_list_push(&found, L + e)) goto stop;
				if (L + e < best) { // the first stage that is not complete, and how far to walk for its leaves
					best = L + e;
					for (k = 0; k < prm->n_stage && prm->stage_y[k] < best; ++k) ;
					limit = prm->stage_y[k];
				}
			}
		}
	}
	for (k = 0; k < prm->n_stage && prm->stage_y[k] < best; ++k) ;
	*missing = 0;
	if (k < prm->n_stage) {
		long long j;
		for (j = 0; j < found.n; ++j) if (found.v[j] <= prm->stage_y[k]) ++*missing;
	}
	free(found.v);
	*leaf_min = best;
	*reach = limit;
	return 2 + k;

stop:
	free(found.v);
	return -1;
}

#ifdef BOOK_ANALYZE_TEST
static long long ba_test_best_walks, ba_test_range_walks; /**< walks of ba_stage_info (test builds) */
static long long ba_test_reach_walks; /**< walks of ba_reach (test builds) */
static long long ba_test_umi_builds, ba_test_umi_nodes, ba_test_umi_keeps; /**< lines of umigame numbers found, their nodes, values kept (test builds) */
static unsigned long long ba_test_shuffle; /**< not 0: the terms are put in a random order before they are sorted (test builds) */
#define BA_TEST_COUNT(n) (++(n))
#define BA_TEST_ADD(n, k) ((n) += (k))
#define BA_TEST_SHUFFLE(v, n) ba_test_mix((v), (n), sizeof *(v))

/** @brief Put n items of an array in a random order, when ba_test_shuffle is set (test builds). */
static void ba_test_mix(void *v, const int n, const size_t size)
{
	unsigned char *a = (unsigned char*) v, tmp[64];
	int i;
	if (ba_test_shuffle == 0 || size > sizeof tmp) return;
	for (i = n - 1; i > 0; --i) {
		int j;
		ba_test_shuffle ^= ba_test_shuffle << 13; ba_test_shuffle ^= ba_test_shuffle >> 7; ba_test_shuffle ^= ba_test_shuffle << 17;
		j = (int) (ba_test_shuffle % (unsigned long long) (i + 1));
		memcpy(tmp, a + (size_t) i * size, size);
		memcpy(a + (size_t) i * size, a + (size_t) j * size, size);
		memcpy(a + (size_t) j * size, tmp, size);
	}
}
#else
#define BA_TEST_COUNT(n) ((void) 0)
#define BA_TEST_ADD(n, k) ((void) 0)
#define BA_TEST_SHUFFLE(v, n) ((void) 0)
#endif

/** @brief B_k of a range, as a lower bound, in s (exact if exact). */
static void ba_stage_bound(BaStageInfo *s, const int k, const int b, const bool exact)
{
	if (k < 0 || k >= BA_MAX_STAGES) return; // (n_stage is at most BA_MAX_STAGES)
	if (exact && b < 255) {
		s->lb[k] = (unsigned char) b;
		s->exact |= (unsigned char) (1u << k);
	} else if (!(s->exact & (1u << k)) && b > s->lb[k]) {
		s->lb[k] = (unsigned char) (b > 255 ? 255 : b);
	}
}

/**
 * @brief Precision stage of a position, the walks known to give nothing left out (see BaStageInfo).
 * @param s What is known (in: lb and exact; out: all).
 * @param found_known A line of best moves is known to reach a solved position (s->found).
 * @return the stage (1 to 2 + n_stage), -1 if stopped.
 */
static int ba_stage_info(BaWork *w, Position *root, const unsigned int root_idx, BaStageInfo *s, const bool found_known)
{
	const BaParams *prm = w->prm;
	const int n_stage = prm->n_stage, y_max = prm->stage_y[n_stage - 1];
	long long n;
	int k;

	w->recursive = false;
	s->recursive = false;
	s->missing = 0;
	if (!found_known || (root->score.value & 1)) {
		bool found;
		BA_TEST_COUNT(ba_test_best_walks);
		n = ba_best_lines(w, root, root_idx, &found, false);
		if (n < 0) return -1;
		s->found = found;
		if ((root->score.value & 1) || !found) { // (an odd value is not the value of a solved position)
			s->missing = n;
			return s->stage = 1;
		}
	} else s->found = true;

	for (k = 0; k < n_stage && s->lb[k] > prm->stage_y[k]; ++k) ;
	if (k == n_stage) return s->stage = 2 + n_stage; // (every range is complete)

	if (ba_stage_total_ok(prm)) {
		int limit = y_max, best, reach, stage;
		if ((s->exact & 1) && s->lb[0] <= y_max) { // (the single walk of all the ranges: the same B for each)
			for (k = 0; prm->stage_y[k] < s->lb[0]; ++k) ;
			limit = prm->stage_y[k];
		}
		BA_TEST_COUNT(ba_test_range_walks);
		stage = ba_stage_total(w, root, root_idx, limit, &s->missing, &best, &reach);
		if (stage < 0) return -1;
		for (k = 0; k < n_stage; ++k) ba_stage_bound(s, k, best == INT_MAX ? reach + 1 : best, best != INT_MAX);
		return s->stage = stage;
	}
	for (k = 0; k < n_stage; ++k) {
		if (s->lb[k] > prm->stage_y[k]) continue; // complete
		BA_TEST_COUNT(ba_test_range_walks);
		n = ba_expansions(w, root, root_idx, prm->base, prm->stage_x[k], prm->stage_y[k], false);
		if (n < 0) return -1;
		s->recursive = s->recursive || w->recursive;
		if (n > 0) {
			if (prm->base != BA_DEVIATE) ba_stage_bound(s, k, w->leaf_min, true);
			s->missing = n;
			return s->stage = 2 + k;
		}
		ba_stage_bound(s, k, prm->stage_y[k] + 1, false);
	}
	return s->stage = 2 + n_stage;
}

/**
 * @brief Precision stage of a position.
 * @param missing leaves to expand for the next stage (out).
 * @return the stage (1 to 2 + n_stage), -1 if stopped.
 */
static int ba_stage(BaWork *w, Position *root, const unsigned int root_idx, long long *missing)
{
	BaStageInfo s;
	memset(&s, 0, sizeof s);
	if (ba_stage_info(w, root, root_idx, &s, false) < 0) return -1;
	*missing = s.missing;
	return s.stage;
}

/* ------------------------------------------------------------------ positions after a position ------------------------------------------ */

typedef struct BaReachThread {
	BaWork *w;
	const long long *cur;
	long long first, last;
	BaList next;                    /**< (Position*) */
	long long n, hist[61];
	volatile unsigned int *bits;
	bool oom;
} BaReachThread;

static inline bool ba_bit_set(volatile unsigned int *bits, const unsigned int i)
{
	const unsigned int m = 1u << (i & 31);
	volatile unsigned int *word = bits + (i >> 5);
#if defined(_MSC_VER)
	if (*word & m) return false; // (an aligned volatile access is not torn)
	return (_InterlockedOr((volatile long*) word, (long) m) & (long) m) == 0;
#else
	if (__atomic_load_n(word, __ATOMIC_RELAXED) & m) return false; // (other threads set bits of the same word)
	return (__atomic_fetch_or(word, m, __ATOMIC_RELAXED) & m) == 0;
#endif
}

/** Positions looked for together: their places in the book are asked for (prefetch) before they are read. */
#define BA_PROBE_BATCH 64

typedef struct BaProbe {
	Board unique;
	unsigned long long bucket;
} BaProbe;

/** @brief Look for a batch of positions (see BA_PROBE_BATCH) and keep those not reached yet. */
static void ba_reach_batch(BaReachThread *t, BaProbe *probe, const int n)
{
	const Book *book = t->w->book;
	int i, j;

	for (i = 0; i < n; ++i) { // (the arrays of the buckets are in memory: ask for their positions)
		const PositionArray *a = book->array + probe[i].bucket;
		const char *s = (const char*) a->positions, *e = s + (size_t) a->n * sizeof (Position);
		for (s -= (uintptr_t) s & 63; s < e; s += 64) PREFETCH(s); // (every cache line of the positions, the last one too)
	}
	for (i = 0; i < n; ++i) {
		const PositionArray *a = book->array + probe[i].bucket;
		for (j = 0; j < a->n; ++j) {
			if (board_equal(&a->positions[j].board, &probe[i].unique)) {
				const unsigned int ci = ba.index.first[probe[i].bucket] + (unsigned int) j;
				if (ba_bit_set(t->bits, ci)) {
					Position *child = a->positions + j;
					++t->n;
					++t->hist[child->level];
					if (!ba_list_push(&t->next, (long long) (intptr_t) child)) t->oom = true;
				}
				break;
			}
		}
	}
}

static void* ba_reach_run(void *v)
{
	BaReachThread *t = (BaReachThread*) v;
	const Book *book = t->w->book;
	BaProbe probe[BA_PROBE_BATCH + MAX_MOVE];
	long long k;
	int n = 0;

	for (k = t->first; k < t->last && !ba_cancelled(t->w->job); ++k) {
		Position *p = (Position*)(intptr_t) t->cur[k];
		Link *l;
		foreach_link(l, p) {
			Board target;
			board_next(&p->board, l->move, &target);
			board_unique(&target, &probe[n].unique);
			probe[n].bucket = board_get_hash_code(&probe[n].unique) & (unsigned long long) (book->n - 1);
			PREFETCH(book->array + probe[n].bucket);
			++n;
		}
		if (n >= BA_PROBE_BATCH) { ba_reach_batch(t, probe, n); n = 0; }
	}
	if (n) ba_reach_batch(t, probe, n);
	return NULL;
}

/**
 * @brief Number of book positions reached from a root through the links (the root included), and their levels.
 * @return the number, -1 if stopped.
 */
static long long ba_reach(BaWork *w, Position *root, const unsigned int root_idx, long long hist[61])
{
	BaReachThread t[MAX_THREADS];
	BaList cur = {NULL, 0, 0};
	volatile unsigned int *bits = BA_TEST_ALLOC_FAILS() ? NULL : (volatile unsigned int*) calloc((size_t) (ba.index.n / 32 + 1), sizeof (unsigned int));
	const int n_thread = ba_n_thread();
	long long n = 1;
	int i;

	if (bits == NULL) return -1;
	BA_TEST_COUNT(ba_test_reach_walks);
	memset(t, 0, sizeof t);
	for (i = 0; i < 61; ++i) hist[i] = 0;
	ba_bit_set(bits, root_idx);
	++hist[root->level];
	if (!ba_list_push(&cur, (long long) (intptr_t) root)) { free((void*) bits); return -1; }
	while (cur.n > 0 && !ba_cancelled(w->job)) {
		const int m = cur.n < 4096 ? 1 : n_thread; // (a small ply is not worth threads)
		bool oom = false;
		for (i = 0; i < m; ++i) {
			t[i].w = w;
			t[i].cur = cur.v;
			t[i].first = cur.n * i / m;
			t[i].last = cur.n * (i + 1) / m;
			t[i].next.n = 0;
			t[i].n = 0;
			t[i].bits = bits;
		}
		thread_run_workers(ba_reach_run, t, sizeof *t, m, true, false);
		cur.n = 0;
		for (i = 0; i < m; ++i) {
			long long k;
			if (t[i].oom) oom = true;
			n += t[i].n;
			for (k = 0; k <= 60; ++k) { hist[k] += t[i].hist[k]; t[i].hist[k] = 0; }
			for (k = 0; k < t[i].next.n && !oom; ++k) if (!ba_list_push(&cur, t[i].next.v[k])) oom = true;
		}
		if (oom) { n = -1; break; }
	}
	for (i = 0; i < MAX_THREADS; ++i) free(t[i].next.v);
	free(cur.v);
	free((void*) bits);
	return ba_cancelled(w->job) ? -1 : n;
}

/** @brief ba_reach, kept (see BaReached). @return the number, -1 if stopped. */
static long long ba_reach_kept(BaWork *w, Position *root, const unsigned int root_idx, long long hist[61])
{
	long long n;
	if (ba_reached_get(root_idx, &n, hist)) return n;
	n = ba_reach(w, root, root_idx, hist);
	if (n >= 0) ba_reached_put(root_idx, n, hist);
	return n;
}

/* ------------------------------------------------------------------ umigame numbers --------------------------------------------------------- */

static bool ba_map_grow(BaMap *m)
{
	const unsigned long long size = m->key ? 2 * (m->mask + 1) : 4096;
	unsigned long long *key = (unsigned long long*) calloc((size_t) size, sizeof *key);
	unsigned int *val = (unsigned int*) malloc((size_t) size * sizeof *val);
	unsigned long long k, j;

	if (key == NULL || val == NULL) { free(key); free(val); return false; }
	if (m->key) {
		for (k = 0; k <= m->mask; ++k) if (m->key[k]) {
			for (j = ba_hash(m->key[k]) & (size - 1); key[j]; j = (j + 1) & (size - 1)) ;
			key[j] = m->key[k];
			val[j] = m->val[k];
		}
	}
	free(m->key); free(m->val);
	m->key = key; m->val = val; m->mask = size - 1;
	return true;
}

/**
 * @brief Find a key, or add it with a value.
 * @return the value of the key; *added tells if it was added. UINT_MAX if memory is exhausted.
 */
static unsigned int ba_map_find(BaMap *m, const unsigned long long key, const unsigned int value, bool *added)
{
	unsigned long long k;
	*added = false;
	if ((m->n + 1) * 2 > (m->key ? m->mask + 1 : 0) && !ba_map_grow(m)) return UINT_MAX;
	for (k = ba_hash(key + 1) & m->mask; m->key[k]; k = (k + 1) & m->mask) {
		if (m->key[k] == key + 1) return m->val[k];
	}
	m->key[k] = key + 1;
	m->val[k] = value;
	++m->n;
	*added = true;
	return value;
}

/** @return the key of a node of the lines (the loss only matters for the loss condition). */
static inline unsigned long long ba_umi_key(const unsigned int idx, const int color, const int loss)
{
	return ((unsigned long long) idx << 9) | ((unsigned long long) color << 8) | (unsigned long long) loss;
}

static unsigned int ba_umi_node(BaWork *w, Position *p, const unsigned int idx, const int color, const int loss, bool *added)
{
	const unsigned long long key = ba_umi_key(idx, color, w->prm->umi == BA_UMI_LOSS ? loss : 0);
	const unsigned int i = ba_map_find(&w->map, key, w->n_node, added);
	BaUNode *u;

	if (i == UINT_MAX || !*added) return i;
	if (w->n_node == BA_UMI_NODE_MAX) { w->too_large = true; return UINT_MAX; }
	if (w->n_node == w->size_node) {
		const unsigned int size = w->size_node + w->size_node / 2 + 1024;
		BaUNode *node = (BaUNode*) realloc(w->node, (size_t) size * sizeof *node);
		if (node == NULL) return UINT_MAX;
		w->node = node; w->size_node = size;
	}
	u = w->node + w->n_node;
	memset(u, 0, sizeof *u);
	u->p = p;
	u->idx = idx;
	u->color = (unsigned char) color;
	u->loss = (unsigned short) loss;
	u->alias = w->n_node;
	u->key = (unsigned char) (2 * (64 - board_count_empties(&p->board)) + (can_move(p->board.player, p->board.opponent) ? 1 : 0));
	return w->n_node++;
}

static bool ba_edge_push(BaWork *w, const unsigned int child)
{
	if (w->n_edge == w->size_edge) {
		const unsigned long long size = w->size_edge + w->size_edge / 2 + 4096;
		unsigned int *edge;
		if (size > UINT_MAX) { w->too_large = true; return false; }
		edge = (unsigned int*) realloc(w->edge, (size_t) size * sizeof *edge);
		if (edge == NULL) return false;
		w->edge = edge; w->size_edge = size;
	}
	w->edge[w->n_edge++] = child;
	return true;
}

/**
 * @brief An order of the nodes where the next positions come first (the greater keys first, see BaUNode), and, if
 * with_rank, the place of each node in it.
 * @param count Nodes of each key.
 * @return false if memory is exhausted.
 */
static bool ba_node_order(BaWork *w, const unsigned int count[131], const bool with_rank)
{
	unsigned int start[131], i;

	w->order = (unsigned int*) malloc((size_t) w->n_node * sizeof *w->order);
	if (with_rank) w->rank = (unsigned int*) malloc((size_t) w->n_node * sizeof *w->rank);
	if (w->order == NULL || (with_rank && w->rank == NULL)) return false;
	start[130] = 0;
	for (i = 130; i-- > 0;) start[i] = start[i + 1] + count[i + 1];
	for (i = 0; i < w->n_node; ++i) {
		const unsigned int r = start[w->node[i].key]++;
		w->order[r] = i;
		if (with_rank) w->rank[i] = r;
	}
	return true;
}

/**
 * @brief Positions of the lines of an umigame number (the root is the position after the move).
 * @param gui_color Side to move at the current position (the values of the range are seen by it).
 * @return 0: done, 1: the root is out of the range, -1: stopped (or memory, or too large).
 */
static int ba_umi_build(BaWork *w, Position *root, const unsigned int root_idx, const int root_color, const int gui_color)
{
	const BaParams *prm = w->prm;
	unsigned int i;
	bool added;

	ba_umi_free(w);
	w->too_large = false;
	if (prm->umi == BA_UMI_RANGE) {
		const int v = (root_color == gui_color) ? root->score.value : -root->score.value;
		if (v < prm->lo || v > prm->hi) return 1;
	}
	if (ba_umi_node(w, root, root_idx, root_color, 0, &added) == UINT_MAX) return -1;

	for (i = 0; i < w->n_node; ++i) { // (the nodes are added while they are read)
		Position *p = w->node[i].p;
		const int color = w->node[i].color, loss = w->node[i].loss;
		const int v = p->score.value;
		const unsigned long long first = w->n_edge;
		Link *l;

		if (ba_cancelled(w->job)) return -1;
		if (60 - board_count_empties(&p->board) < prm->depth) {
			foreach_link(l, p) {
				int child_loss = 0;
				Board target;
				unsigned int ci, c;
				Position *child;

				if (prm->umi == BA_UMI_BEST) {
					if (l->score != v) continue;
				} else if (prm->umi == BA_UMI_LOSS) {
					const int e = v - l->score;
					if (e < 0 || e > prm->x || loss + e > prm->y) continue;
					child_loss = loss + e;
				} else {
					const int s = (color == gui_color) ? l->score : -l->score;
					if (s < prm->lo || s > prm->hi) continue;
				}
				board_next(&p->board, l->move, &target);
				child = ba_probe(w, &target, &ci);
				if (child == NULL) continue;
				c = ba_umi_node(w, child, ci, !color, child_loss, &added);
				if (c == UINT_MAX || !ba_edge_push(w, c)) return -1;
				if (added) w->node[c].depth = (unsigned char) (w->node[i].depth < 255 ? w->node[i].depth + 1 : 255);
			}
		}
		w->node[i].edge = (unsigned int) first;
		w->node[i].n_edge = (unsigned int) (w->n_edge - first);
		if (w->node[i].n_edge == 0) {
			w->node[i].end = true;
			if (prm->umi == BA_UMI_LOSS) { // the same end (position and side to move) reached with different losses is one end
				const unsigned int e = ba_map_find(&w->end_map, ba_umi_key(w->node[i].idx, color, 0), i, &added);
				if (e == UINT_MAX) return -1;
				w->node[i].alias = e;
			}
		}
	}

	// lines leading to each node, and an order where the next positions come first
	{
		unsigned int count[131];
		unsigned long long k;
		memset(count, 0, sizeof count);
		for (i = 0; i < w->n_node; ++i) {
			for (k = w->node[i].edge; k < (unsigned long long) w->node[i].edge + w->node[i].n_edge; ++k) {
				w->edge[k] = w->node[w->edge[k]].alias;
				++w->node[w->edge[k]].indeg;
			}
			++count[w->node[i].key];
		}
		if (!ba_node_order(w, count, true)) return -1;
	}
	BA_TEST_COUNT(ba_test_umi_builds);
	BA_TEST_ADD(ba_test_umi_nodes, w->n_node);
	return 0;
}

/* expressions */

static bool ba_pool_reserve(BaWork *w, const unsigned long long n)
{
	if (w->n_pool + n > w->size_pool) {
		unsigned long long size = w->size_pool + w->size_pool / 2 + 65536;
		unsigned int *pool;
		if (size < w->n_pool + n) size = w->n_pool + n;
		if (size > BA_POOL_MAX) { w->too_large = true; return false; }
		pool = (unsigned int*) realloc(w->pool, (size_t) size * sizeof *pool);
		if (pool == NULL) return false;
		w->pool = pool; w->size_pool = size;
	}
	return true;
}

static bool ba_terms_push(BaTerms *e, const unsigned long long c, const unsigned int off, const unsigned int len)
{
	if (e->n == e->size) {
		const int size = e->size + e->size / 2 + 4;
		BaTerm *t = (BaTerm*) realloc(e->t, (size_t) size * sizeof *t);
		if (t == NULL) return false;
		e->t = t; e->size = size;
	}
	e->t[e->n].c = c;
	e->t[e->n].off = off;
	e->t[e->n].len = len;
	++e->n;
	return true;
}

static long long ba_umi_value(BaWork *w, const unsigned int v, const int counter);

/** @return true if the set of a is in the set of b. */
static bool ba_subset(const unsigned int *pool, const BaTerm *a, const BaTerm *b)
{
	unsigned int i = 0, j = 0;
	if (a->len > b->len) return false;
	while (i < a->len) {
		if (b->len - j < a->len - i) return false;
		if (pool[a->off + i] == pool[b->off + j]) ++i, ++j;
		else if (pool[a->off + i] > pool[b->off + j]) ++j;
		else return false;
	}
	return true;
}

/**
 * @brief Tell if a term is never worse than another, whatever the other parts of the expression are:
 * a.c plus the values of the shared positions of a that b does not have is at most b.c
 * (the ends of these shared positions, chosen alone, cost at most their values).
 * @return 1 if a is never worse, 0 if not, -1 if stopped.
 */
static int ba_dominates(BaWork *w, const BaTerm *a, const BaTerm *b, const int counter)
{
	unsigned long long extra = 0;
	unsigned int i = 0, j = 0;

	if (a->c > b->c) return 0;
	if (ba_subset(w->pool, a, b)) return 1;
	if (a->c == b->c) return 0;
	while (i < a->len) {
		const unsigned int x = w->pool[a->off + i];
		if (j < b->len && w->pool[b->off + j] < x) { ++j; continue; }
		if (j < b->len && w->pool[b->off + j] == x) { ++i; ++j; continue; }
		{
			const long long g = ba_umi_value(w, w->order[x], counter); // (the pool may move)
			if (g < 0) return -1;
			extra += (unsigned long long) g;
			if (a->c + extra > b->c) return 0;
		}
		++i;
	}
	return 1;
}

/** @brief Order of the terms: ends, then size of the set, then place of the set (terms equal in all three are the same term). */
static int ba_term_cmp(const void *x, const void *y)
{
	const BaTerm *a = (const BaTerm*) x, *b = (const BaTerm*) y;
	if (a->c != b->c) return a->c < b->c ? -1 : 1;
	if (a->len != b->len) return a->len < b->len ? -1 : 1;
	return (a->off > b->off) - (a->off < b->off);
}

typedef struct BaBound { unsigned long long lb, ub; int i; } BaBound;

/** @brief Order of the bounds: lower bound, then upper bound, then the order of the terms. */
static int ba_bound_cmp(const void *x, const void *y)
{
	const BaBound *a = (const BaBound*) x, *b = (const BaBound*) y;
	if (a->lb != b->lb) return a->lb < b->lb ? -1 : 1;
	if (a->ub != b->ub) return a->ub < b->ub ? -1 : 1;
	return (a->i > b->i) - (a->i < b->i);
}

/**
 * @brief Remove the terms that cannot be the smallest: a term with more ends and more shared positions than another,
 * and, with bound, a term whose lower bound is above the upper bound of another. Keep BA_TERM_MAX terms at most
 * (the value is then inexact).
 *
 * The bounds only hold for an expression whose value is wanted (the root, a shared position): the expression of
 * a position is a part of the expressions of the previous positions, where a term that is not the smallest alone
 * may share positions with the other parts and give the smallest sum.
 * @return false if stopped.
 */
static bool ba_prune(BaWork *w, BaTerms *e, const int counter, const bool bound)
{
	int i, j, n;
	bool vars = false;

	if (e->n <= 1) return true;
	BA_TEST_SHUFFLE(e->t, e->n);
	qsort(e->t, (size_t) e->n, sizeof *e->t, ba_term_cmp);
	for (n = 0, j = 0; j < e->n; ++j) {
		int dominated = 0;
		for (i = 0; i < n && dominated == 0; ++i) dominated = ba_dominates(w, e->t + i, e->t + j, counter); // (e->t[i].c <= e->t[j].c)
		if (dominated < 0) return false;
		if (!dominated) {
			e->t[n++] = e->t[j];
			if (e->t[j].len) vars = true;
		}
	}
	e->n = n;
	if (bound && vars && n > 1) {
		BaBound *b = (BaBound*) malloc((size_t) n * sizeof *b);
		unsigned long long best = ULLONG_MAX;
		if (b == NULL) return false;
		for (i = 0; i < n; ++i) {
			unsigned long long lb = 0, ub = 0;
			unsigned int k;
			for (k = 0; k < e->t[i].len; ++k) {
				const long long g = ba_umi_value(w, w->order[w->pool[e->t[i].off + k]], counter);
				if (g < 0) { free(b); return false; }
				if ((unsigned long long) g > lb) lb = (unsigned long long) g;
				ub += (unsigned long long) g;
			}
			b[i].lb = e->t[i].c + lb;
			b[i].ub = e->t[i].c + ub;
			b[i].i = i;
			if (b[i].ub < best) best = b[i].ub;
		}
		BA_TEST_SHUFFLE(b, n);
		qsort(b, (size_t) n, sizeof *b, ba_bound_cmp);
		for (j = 0; j < n && b[j].lb <= best; ++j) ;
		if (j > BA_TERM_MAX) { j = BA_TERM_MAX; w->inexact = true; }
		{
			BaTerm *t = (BaTerm*) malloc((size_t) j * sizeof *t);
			if (t == NULL) { free(b); return false; }
			for (i = 0; i < j; ++i) t[i] = e->t[b[i].i];
			memcpy(e->t, t, (size_t) j * sizeof *t);
			free(t);
		}
		e->n = j;
		free(b);
	} else if (n > BA_TERM_MAX) {
		e->n = BA_TERM_MAX;
		w->inexact = true;
	}
	return !ba_cancelled(w->job);
}

/** @brief out = a + b (the ends of both, the shared positions counted once). */
static bool ba_sum(BaWork *w, const BaTerms *a, const BaTerms *b, BaTerms *out, const int counter, const bool bound)
{
	int i, j;

	out->n = 0;
	if (a->n == 1 && a->t[0].len == 0) {
		for (j = 0; j < b->n; ++j) if (!ba_terms_push(out, a->t[0].c + b->t[j].c, b->t[j].off, b->t[j].len)) return false;
		return true;
	}
	if (b->n == 1 && b->t[0].len == 0) {
		for (i = 0; i < a->n; ++i) if (!ba_terms_push(out, a->t[i].c + b->t[0].c, a->t[i].off, a->t[i].len)) return false;
		return true;
	}
	for (i = 0; i < a->n; ++i) for (j = 0; j < b->n; ++j) {
		const BaTerm *x = a->t + i, *y = b->t + j;
		unsigned int p = 0, q = 0, off;
		if (!ba_pool_reserve(w, (unsigned long long) x->len + y->len)) return false;
		off = (unsigned int) w->n_pool;
		while (p < x->len || q < y->len) { // union of two increasing lists
			unsigned int z;
			if (q == y->len || (p < x->len && w->pool[x->off + p] < w->pool[y->off + q])) z = w->pool[x->off + p++];
			else if (p == x->len || w->pool[y->off + q] < w->pool[x->off + p]) z = w->pool[y->off + q++];
			else { z = w->pool[x->off + p]; ++p; ++q; }
			w->pool[w->n_pool++] = z;
		}
		if (!ba_terms_push(out, x->c + y->c, off, (unsigned int) (w->n_pool - off))) return false;
		if (out->n >= 2 * BA_TERM_MAX && !ba_prune(w, out, counter, bound)) return false;
	}
	return ba_prune(w, out, counter, bound);
}

/** @brief Replace the shared positions of an expression by their own expressions, the latest first, and give its value. */
static long long ba_resolve(BaWork *w, const BaTerms *e, const int counter)
{
	BaTerms cur = {NULL, 0, 0}, next = {NULL, 0, 0}, part = {NULL, 0, 0}, one = {NULL, 0, 0};
	long long value = -1;
	int i, k;

	for (i = 0; i < e->n; ++i) if (!ba_terms_push(&cur, e->t[i].c, e->t[i].off, e->t[i].len)) goto end;
	for (;;) {
		unsigned int top = 0;
		bool any = false;
		const BaTerms *d;
		for (i = 0; i < cur.n; ++i) if (cur.t[i].len) {
			const unsigned int last = w->pool[cur.t[i].off + cur.t[i].len - 1];
			if (!any || last > top) top = last;
			any = true;
		}
		if (!any) break;
		d = w->def + w->order[top];
		next.n = 0;
		for (i = 0; i < cur.n; ++i) {
			if (cur.t[i].len && w->pool[cur.t[i].off + cur.t[i].len - 1] == top) {
				one.n = 0;
				if (!ba_terms_push(&one, cur.t[i].c, cur.t[i].off, cur.t[i].len - 1) || !ba_sum(w, &one, d, &part, counter, true)) goto end;
				for (k = 0; k < part.n; ++k) if (!ba_terms_push(&next, part.t[k].c, part.t[k].off, part.t[k].len)) goto end;
			} else if (!ba_terms_push(&next, cur.t[i].c, cur.t[i].off, cur.t[i].len)) goto end;
		}
		if (!ba_prune(w, &next, counter, true)) goto end;
		{ BaTerms t = cur; cur = next; next = t; }
	}
	for (i = 0; i < cur.n; ++i) if (value < 0 || cur.t[i].c < (unsigned long long) value) value = (long long) cur.t[i].c;
end:
	free(cur.t); free(next.t); free(part.t); free(one.t);
	return value;
}

/** @return the value of a shared node (computed once), -1 if stopped. */
static long long ba_umi_value(BaWork *w, const unsigned int v, const int counter)
{
	if (w->g[v] < 0) w->g[v] = ba_resolve(w, w->def + v, counter);
	return w->g[v];
}

/** @brief Copy the sets of the expressions kept (the sets of the terms removed are left behind). @return false if memory is exhausted. */
static bool ba_pool_compact(BaWork *w)
{
	unsigned long long n = 0, m = 0;
	unsigned int *pool;
	unsigned int v;
	int i, s;

	for (v = 0; v < w->n_node; ++v) for (s = 0; s < 2; ++s) {
		const BaTerms *e = s ? w->def + v : w->rep + v;
		for (i = 0; i < e->n; ++i) n += e->t[i].len;
	}
	pool = (unsigned int*) malloc((size_t) (n + 65536) * sizeof *pool);
	if (pool == NULL) return false;
	for (v = 0; v < w->n_node; ++v) for (s = 0; s < 2; ++s) {
		BaTerms *e = s ? w->def + v : w->rep + v;
		for (i = 0; i < e->n; ++i) {
			memcpy(pool + m, w->pool + e->t[i].off, e->t[i].len * sizeof *pool);
			e->t[i].off = (unsigned int) m;
			m += e->t[i].len;
		}
	}
	free(w->pool);
	w->pool = pool;
	w->n_pool = m;
	w->size_pool = n + 65536;
	w->pool_kept = m;
	return true;
}

/**
 * @brief Add delta to the uses of the shared positions of an expression (each counted once).
 * @param n If not NULL, the shared positions are also listed in w->found (*n of them).
 */
static void ba_uses(BaWork *w, const BaTerms *e, const int delta, unsigned int *n)
{
	int i;
	unsigned int k;
	if (++w->stamp_now == 0) { memset(w->stamp, 0, w->n_node * sizeof *w->stamp); w->stamp_now = 1; }
	if (n) *n = 0;
	for (i = 0; i < e->n; ++i) for (k = 0; k < e->t[i].len; ++k) {
		const unsigned int x = w->pool[e->t[i].off + k];
		if (w->stamp[x] != w->stamp_now) {
			w->stamp[x] = w->stamp_now;
			w->uses[x] += delta;
			if (n) w->found[(*n)++] = x;
		}
	}
}

/** @brief Replace a shared position (rank x) of an expression by its own expression. @return false if stopped. */
static bool ba_substitute(BaWork *w, BaTerms *e, const unsigned int x, const int counter)
{
	BaTerms out = {NULL, 0, 0}, one = {NULL, 0, 0}, part = {NULL, 0, 0};
	const BaTerms *d = w->def + w->order[x];
	bool ok = true;
	int i, k;

	for (i = 0; i < e->n && ok; ++i) {
		const BaTerm t = e->t[i];
		unsigned int j, at = t.len;
		for (j = 0; j < t.len; ++j) if (w->pool[t.off + j] == x) { at = j; break; }
		if (at == t.len) { ok = ba_terms_push(&out, t.c, t.off, t.len); continue; }
		// the term without x
		if (!ba_pool_reserve(w, t.len)) { ok = false; break; }
		{
			const unsigned int off = (unsigned int) w->n_pool;
			for (j = 0; j < t.len; ++j) if (j != at) w->pool[w->n_pool++] = w->pool[t.off + j];
			one.n = 0;
			ok = ba_terms_push(&one, t.c, off, t.len - 1) && ba_sum(w, &one, d, &part, counter, false);
		}
		for (k = 0; k < part.n && ok; ++k) ok = ba_terms_push(&out, part.t[k].c, part.t[k].off, part.t[k].len);
	}
	if (ok) ok = ba_prune(w, &out, counter, false);
	free(one.t); free(part.t);
	if (!ok) { free(out.t); return false; }
	free(e->t);
	*e = out;
	return true;
}

/**
 * @brief Replace the shared positions of an expression that no other expression uses any more
 * (all the positions before them are done): the value cannot change, and the expressions stay small.
 * @return false if stopped.
 */
static bool ba_eliminate(BaWork *w, BaTerms *e, const int counter)
{
	for (;;) {
		unsigned int n, i, x = UINT_MAX;
		ba_uses(w, e, 0, &n);
		for (i = 0; i < n; ++i) {
			const unsigned int y = w->found[i];
			if (w->remain[w->order[y]] == 0 && w->uses[y] == 1) { x = y; break; }
		}
		if (x == UINT_MAX) return true;
		ba_uses(w, e, -1, NULL);
		ba_uses(w, w->def + w->order[x], -1, NULL);
		if (!ba_substitute(w, e, x, counter)) return false;
		ba_uses(w, e, 1, NULL);
		w->uses[x] = 0;
		free(w->def[w->order[x]].t);
		memset(w->def + w->order[x], 0, sizeof w->def[0]);
	}
}

/**
 * @brief Keep the value of a node as counted from it alone: its expression only holds its next positions, whose
 * values do not depend on the lines before it. Not known (-1) if terms were dropped, then or before. The numbers of
 * the pool used are given back and the flags left as they were, so that the value of the root is found as without
 * it; if terms were dropped, the values of the shared nodes found then are forgotten (found again when needed).
 * @return false if stopped.
 */
static bool ba_umi_keep_value(BaWork *w, const unsigned int v, const int counter)
{
	const unsigned long long n_pool = w->n_pool;
	const bool too_large = w->too_large;
	long long value = -1;

	if (!w->inexact) {
		BA_TEST_COUNT(ba_test_umi_keeps);
		value = ba_resolve(w, w->rep + v, counter);
		if (ba_cancelled(w->job)) return false;
		if (w->inexact) {
			unsigned int u;
			for (u = 0; u < w->n_node; ++u) w->g[u] = -1;
			w->inexact = false;
			value = -1;
		}
	}
	w->n_pool = n_pool;
	w->too_large = too_large;
	w->kept[2 * v + counter] = value;
	return true;
}

/**
 * @brief Umigame number of a side, the shared positions counted once.
 *
 * The positions are done from the last ones. The expression of a position is a list of terms (alternatives):
 * c ends plus the ends of a set of shared positions, counted once. A shared position (several lines lead to
 * it) is seen from the previous positions as its own name; its name is replaced by its expression as soon as
 * one expression only uses it (see ba_eliminate), and the names left are replaced at the end (ba_resolve).
 *
 * @param counter Side that learns the lines.
 * @return the number, -1 if stopped.
 */
static long long ba_umi_count(BaWork *w, const int counter)
{
	unsigned int r, v;
	unsigned long long k;
	long long value = -1;

	w->rep = (BaTerms*) calloc(w->n_node, sizeof *w->rep);
	w->def = (BaTerms*) calloc(w->n_node, sizeof *w->def);
	w->g = (long long*) malloc((size_t) w->n_node * sizeof *w->g);
	w->uses = (int*) calloc(w->n_node, sizeof *w->uses);
	w->remain = (unsigned int*) malloc((size_t) w->n_node * sizeof *w->remain);
	w->stamp = (unsigned int*) calloc(w->n_node, sizeof *w->stamp);
	w->found = (unsigned int*) malloc((size_t) w->n_node * sizeof *w->found);
	if (w->rep == NULL || w->def == NULL || w->g == NULL || w->uses == NULL || w->remain == NULL || w->stamp == NULL || w->found == NULL) return -1;
	for (v = 0; v < w->n_node; ++v) w->remain[v] = w->node[v].indeg;
	w->stamp_now = 0;
	w->n_pool = 0;
	w->pool_kept = 0;
	w->inexact = false;
	if (w->keep_depth > 0 && w->kept == NULL) {
		w->kept = (long long*) malloc((size_t) w->n_node * 2 * sizeof *w->kept);
		if (w->kept == NULL) return -1;
		for (v = 0; v < 2 * w->n_node; ++v) w->kept[v] = -1;
	}

	for (r = 0; r < w->n_node; ++r) {
		BaTerms e = {NULL, 0, 0}, sum = {NULL, 0, 0};
		v = w->order[r];
		w->g[v] = -1;
		if (w->node[v].alias != v) continue;
		if (ba_cancelled(w->job)) return -1;
		if (w->n_pool > (1u << 22) && w->n_pool > 2 * w->pool_kept && !ba_pool_compact(w)) return -1;
		if (w->node[v].end) {
			if (!ba_terms_push(&e, 1, 0, 0)) return -1;
		} else {
			const bool choice = (w->node[v].color == counter); // the side that learns chooses one move
			for (k = w->node[v].edge; k < (unsigned long long) w->node[v].edge + w->node[v].n_edge; ++k) {
				const unsigned int u = w->edge[k];
				BaTerms *ru = w->rep + u;
				if (k == w->node[v].edge) {
					int i;
					for (i = 0; i < ru->n; ++i) if (!ba_terms_push(&e, ru->t[i].c, ru->t[i].off, ru->t[i].len)) return -1;
				} else if (choice) {
					int i;
					for (i = 0; i < ru->n; ++i) if (!ba_terms_push(&e, ru->t[i].c, ru->t[i].off, ru->t[i].len)) return -1;
					if (!ba_prune(w, &e, counter, false)) { free(e.t); return -1; }
				} else {
					if (!ba_sum(w, &e, ru, &sum, counter, false)) { free(e.t); free(sum.t); return -1; }
					{ BaTerms t = e; e = sum; sum = t; }
				}
				if (w->node[u].indeg <= 1 || --w->remain[u] == 0) { // (the expression of the next position is not used any more)
					ba_uses(w, ru, -1, NULL);
					free(ru->t); ru->t = NULL; ru->n = ru->size = 0;
				}
			}
			free(sum.t);
			if (choice && !ba_prune(w, &e, counter, false)) { free(e.t); return -1; }
		}
		ba_uses(w, &e, 1, NULL);
		if (!ba_eliminate(w, &e, counter)) { free(e.t); return -1; }
		if (w->node[v].indeg >= 2) { // shared: the previous positions see its own name
			w->def[v] = e;
			if (!ba_pool_reserve(w, 1)) return -1;
			w->pool[w->n_pool] = r;
			memset(&e, 0, sizeof e);
			if (!ba_terms_push(&e, 0, (unsigned int) w->n_pool, 1)) return -1;
			++w->n_pool;
			w->uses[r] = 1;
		}
		w->rep[v] = e;
		if (w->node[v].depth >= 1 && w->node[v].depth <= w->keep_depth && !ba_umi_keep_value(w, v, counter)) return -1;
	}
	value = ba_resolve(w, w->rep, counter); // (the root is node 0)
	return value;
}

#ifdef BOOK_ANALYZE_TEST
/** @return the umigame number of a side without the shared positions (each line counted, test builds). */
static unsigned long long ba_umi_tree(BaWork *w, const int counter)
{
	unsigned long long *n = (unsigned long long*) malloc((size_t) w->n_node * sizeof *n), result;
	unsigned int r;
	if (n == NULL) return 0;
	for (r = 0; r < w->n_node; ++r) {
		const unsigned int v = w->order[r];
		unsigned long long k, x = 0;
		if (w->node[v].alias != v) continue;
		if (w->node[v].end) x = 1;
		else if (w->node[v].color == counter) {
			x = ULLONG_MAX;
			for (k = w->node[v].edge; k < (unsigned long long) w->node[v].edge + w->node[v].n_edge; ++k) if (n[w->edge[k]] < x) x = n[w->edge[k]];
		} else {
			for (k = w->node[v].edge; k < (unsigned long long) w->node[v].edge + w->node[v].n_edge; ++k) x = (x > ULLONG_MAX - n[w->edge[k]]) ? ULLONG_MAX : x + n[w->edge[k]];
		}
		n[v] = x;
	}
	result = n[0];
	free(n);
	return result;
}
#endif

/* ------------------------------------------------------------------ book moveinfo ----------------------------------------------------------- */

/** A legal move of the current position. */
typedef struct BaMove {
	int x;                  /**< move */
	char sq[3];
	Position *p;            /**< position after the move, NULL if it is not in the book */
	unsigned int idx;
	bool has_value, leaf;   /**< value known (position after the move, or leaf of the current position); leaf move */
	bool has_link;          /**< link of the current position */
	int value, level;
	int link;               /**< score of the link (it differs from the value when the current position was not negamaxed again) */
	int stage;
	long long missing;
	long long umi[2];       /**< umigame numbers of black and white */
	bool umi_exact;
	int umi_state;          /**< 0: done, 1: the position is out of the range, 2: too many positions */
	int same;               /**< an earlier move to the same position (a symetry): its results are given again; -1 if none */
} BaMove;

typedef struct BaTeam {
	BaJob *job;
	const BaParams *prm;
	BaMove *move;
	int n_move;
	int next;               /**< next move to take */
	int part;               /**< BA_PART_STAGE or BA_PART_UMI */
	const Position *current;    /**< current position, NULL if it is not in the book */
	BaStageInfo current_info;   /**< its stage, if current_known (kept when it was the position after a move) */
	bool current_known;
	bool failed;                /**< a walk had no memory (the analysis ends with an error) */
	Lock lock;
} BaTeam;

typedef struct BaThread {
	BaTeam *team;
} BaThread;

/** @return the unterminated lines of a position (counted by book negamax: a transposition is counted by each line). */
static unsigned long long ba_lines(const Book *book, const Position *p)
{
	(void) book;
	return p->n_lines;
}

/** @brief Write the line of a move: its book value, and its stage when with_stage. */
static void ba_print_move(const BaJob *job, const BaMove *m, const bool with_stage)
{
	char stage[64] = "", link[32] = "";

	if (m->p == NULL) {
		if (m->has_value) ba_print("@move id=%d sq=%s book=0 leaf=1 value=%d level=%d%s\n", job->id, m->sq, m->value, m->level, with_stage ? " stage=0" : "");
		else ba_print("@move id=%d sq=%s book=0%s\n", job->id, m->sq, with_stage ? " stage=0" : "");
		return;
	}
	if (with_stage) snprintf(stage, sizeof stage, " stage=%d missing=%lld", m->stage, m->missing);
	if (m->has_link && m->link != m->value) snprintf(link, sizeof link, " link=%d", m->link);
	ba_print("@move id=%d sq=%s book=1 value=%d level=%d lines=%llu%s%s\n", job->id, m->sq, m->value, m->level, ba_lines(job->book, m->p), stage, link);
}

static void ba_print_umi(const BaJob *job, const BaMove *m)
{
	if (m->umi_state == 2) ba_print("@move id=%d sq=%s part=umi umi=none\n", job->id, m->sq);
	else ba_print("@move id=%d sq=%s part=umi umi=%lld:%lld%s\n", job->id, m->sq, m->umi[BLACK], m->umi[WHITE], m->umi_exact ? "" : " exact=0");
}

/**
 * @brief What the stage of the current position tells of the position after a move (see BaStageInfo).
 *
 * deviate2 / deviate3: B_k of the position after a link of loss e is at least B_k of the current position - e.
 * deviate: the walks of a range from the position after a link of loss 0 whose value is the opposite of the value of
 * the current position are the walks from the current position after that move (the other side deviates in the
 * other walk, with the same window), so a range complete for the current position is complete for it, unless a walk
 * of the current position went recursive.
 */
static void ba_stage_down(const BaParams *prm, const Book *book, const Position *current, const BaStageInfo *c, const BaMove *m, BaStageInfo *s)
{
	const bool total = ba_stage_total_ok(prm);
	int e, k;

	if (!m->has_link || !ba_walkable(book, current) || !ba_walkable(book, m->p)) return;
	e = current->score.value - m->link;
	if (e < 0) return;
	if (prm->base == BA_DEVIATE) {
		if (e != 0 || m->p->score.value != -current->score.value || c->recursive) return;
		for (k = 0; k < prm->n_stage; ++k) if (c->lb[k] > prm->stage_y[k]) ba_stage_bound(s, k, prm->stage_y[k] + 1, false);
		return;
	}
	for (k = 0; k < prm->n_stage; ++k) if (total || e <= prm->stage_x[k]) ba_stage_bound(s, k, c->lb[k] - e, false);
}

/**
 * @brief What the kept stages of the next positions of a position tell of it: B_k (deviate2, deviate3; see
 * BaStageInfo), and whether a line of best moves reaches a solved position. A position that is not walked
 * expands no leaf.
 * @return true if a line of best moves is known to reach a solved position.
 */
static bool ba_stage_up(BaWork *w, Position *root, BaStageInfo *s)
{
	const BaParams *prm = w->prm;
	const Book *book = w->book;
	const unsigned long long cond = ba_cond_stage(prm);
	const bool total = ba_stage_total_ok(prm), walkable = ba_walkable(book, root), bound = prm->base != BA_DEVIATE && walkable;
	const int v = root->score.value, e_leaf = v - root->leaf.score;
	int b[BA_MAX_STAGES], k;
	unsigned int exact = 0;
	bool found = board_is_game_over(&root->board) || position_is_solved(root);
	Link *l;

	for (k = 0; k < prm->n_stage; ++k) { // the leaf of the position
		b[k] = INT_MAX;
		if (root->leaf.move != NOMOVE && 0 <= e_leaf && (total || e_leaf <= prm->stage_x[k]) && !(prm->base == BA_DEVIATE2 && position_is_solved(root))) {
			b[k] = e_leaf;
			exact |= 1u << k;
		}
	}
	foreach_link(l, root) {
		const int e = v - l->score;
		Board target;
		unsigned int ci;
		Position *child;
		BaStageInfo c;
		bool kept;
		board_next(&root->board, l->move, &target);
		child = ba_probe(w, &target, &ci);
		if (child == NULL) continue;
		kept = ba_cache_get_stage(ci, cond, &c);
		if (kept && c.found && l->score == v) found = true;
		if (!bound || e < 0 || !ba_walkable(book, child)) continue;
		for (k = 0; k < prm->n_stage; ++k) {
			const bool x = kept && ((c.exact >> k) & 1);
			int t;
			if (!total && e > prm->stage_x[k]) continue;
			t = e + (kept ? c.lb[k] : 0); // (a next position not kept: B_k is at least 0)
			if (t < b[k]) { b[k] = t; exact = x ? exact | (1u << k) : exact & ~(1u << k); }
			else if (t == b[k] && x) exact |= 1u << k;
		}
	}
	if (!walkable) for (k = 0; k < prm->n_stage; ++k) ba_stage_bound(s, k, 255, false);
	else if (bound) for (k = 0; k < prm->n_stage; ++k) ba_stage_bound(s, k, b[k] == INT_MAX ? 255 : b[k], ((exact >> k) & 1) != 0);
	return found;
}

/**
 * @brief Stage of the position after a move: kept, or found with what the current position and the next positions
 * of the position after the move tell (see BaStageInfo), and kept.
 * @param current The current position (NULL if it is not in the book), its stage if current_known.
 * @return the stage, -1 if stopped.
 */
static int ba_move_stage(BaWork *w, const Position *current, const BaStageInfo *current_info, const bool current_known, BaMove *m)
{
	const unsigned long long cond = ba_cond_stage(w->prm);
	BaStageInfo s;

	if (!ba_cache_get_stage(m->idx, cond, &s)) {
		bool found;
		memset(&s, 0, sizeof s);
		if (current && current_known) ba_stage_down(w->prm, w->book, current, current_info, m, &s);
		found = ba_stage_up(w, m->p, &s);
		if (ba_stage_info(w, m->p, m->idx, &s, found) < 0) return -1;
		ba_cache_put_stage(m->idx, cond, &s);
	}
	m->stage = s.stage;
	m->missing = s.missing;
	return s.stage;
}

/** @return true if the umigame numbers of a position (and its side to move) are kept (BA_CACHE_UMI), read in m. */
static bool ba_cache_get_umi(const unsigned int idx, const int color, const unsigned long long cond, BaMove *m)
{
	long long a, b;
	int c;
	if (!ba_cache_get(ba_cache_key(idx, BA_CACHE_UMI, color), cond, &a, &b, &c, NULL, NULL)) return false;
	m->umi[BLACK] = a;
	m->umi[WHITE] = b;
	m->umi_state = c & 3;
	m->umi_exact = (c & 4) != 0;
	return true;
}

/** @brief Keep the umigame numbers of a position (state: as umi_state of BaMove). */
static void ba_cache_put_umi(const unsigned int idx, const int color, const unsigned long long cond, const long long black, const long long white, const int state, const bool exact)
{
	ba_cache_put(ba_cache_key(idx, BA_CACHE_UMI, color), cond, black, white, state | (exact ? 4 : 0), NULL, 0);
}

static int ba_umi_keep_depth = 2;   /**< the numbers kept from the lines of the position after a move: 1 to this many moves after it (the tests change it) */

/**
 * @brief Keep the umigame numbers of the nodes whose values were kept (see ba_umi_keep_value) and whose own lines are
 * their lines here: umi=best, all; umi=loss, those reached with no loss; umi=range, those where the values of the
 * range are seen by the same side (the side not to move there), or all if lo = -hi. A position whose value is out of
 * the range (seen by the side not to move there) has no number, as ba_umi_build tells.
 * @param gui_color Side whose values of the range the lines followed.
 */
static void ba_umi_keep(BaWork *w, const int gui_color)
{
	const BaParams *prm = w->prm;
	const unsigned long long cond = ba_cond_umi(prm);
	unsigned int v;

	if (w->kept == NULL) return;
	for (v = 0; v < w->n_node; ++v) {
		const BaUNode *u = w->node + v;
		if (u->depth < 1 || u->depth > w->keep_depth || u->alias != v) continue;
		if (prm->umi == BA_UMI_LOSS && u->loss != 0) continue;
		if (prm->umi == BA_UMI_RANGE) {
			const int s = -u->p->score.value;
			if (s < prm->lo || s > prm->hi) { ba_cache_put_umi(u->idx, u->color, cond, 0, 0, 1, true); continue; }
			if (u->color == gui_color && prm->lo != -prm->hi) continue;
		}
		if (w->kept[2 * v + BLACK] < 0 || w->kept[2 * v + WHITE] < 0) continue;
		ba_cache_put_umi(u->idx, u->color, cond, w->kept[2 * v + BLACK], w->kept[2 * v + WHITE], 0, true);
	}
}

/**
 * @brief Umigame numbers of the position after a move: kept, or counted and kept (with the numbers of the positions
 * of its lines 1 or 2 moves after it, see ba_umi_keep).
 * @param player Side to move at the current position.
 * @return false if stopped.
 */
static bool ba_move_umi(BaWork *w, const int player, BaMove *m)
{
	const unsigned long long cond = ba_cond_umi(w->prm);
	int r;

	if (ba_cache_get_umi(m->idx, !player, cond, m)) return true;
	w->keep_depth = ba_umi_keep_depth;
	r = ba_umi_build(w, m->p, m->idx, !player, player);
	m->umi_state = 0;
	if (r == 1) { // the position after the move is out of the range
		m->umi_state = 1;
		m->umi[BLACK] = m->umi[WHITE] = 0;
		m->umi_exact = true;
	} else if (r < 0) {
		if (ba_cancelled(w->job)) return false;
		m->umi_state = 2;
	} else {
		bool exact;
		m->umi[BLACK] = ba_umi_count(w, BLACK);
		exact = !w->inexact;
		ba_umi_clear(w);
		m->umi[WHITE] = (m->umi[BLACK] >= 0) ? ba_umi_count(w, WHITE) : -1;
		exact = exact && !w->inexact;
		ba_umi_clear(w);
		if (ba_cancelled(w->job)) return false;
		if (m->umi[BLACK] < 0 || m->umi[WHITE] < 0) m->umi_state = 2;
		else ba_umi_keep(w, player);
		m->umi_exact = exact;
	}
	const bool cache_result = m->umi_state != 2 || w->too_large;
	ba_umi_free(w);
	if (cache_result) ba_cache_put_umi(m->idx, !player, cond, m->umi[BLACK], m->umi[WHITE], m->umi_state, m->umi_exact);
	return true;
}

static void* ba_team_run(void *v)
{
	BaTeam *team = ((BaThread*) v)->team;
	BaJob *job = team->job;
	BaWork w;
	int i;

	ba_work_init(&w, job, team->prm);
	for (;;) {
		BaMove *m;
		bool failed;
		lock(team);
		i = team->next++;
		failed = team->failed;
		unlock(team);
		if (i >= team->n_move || ba_cancelled(job) || failed) break;
		m = team->move + i;
		if (m->p == NULL || m->same >= 0) continue;
		if (team->part == BA_PART_STAGE) {
			if (ba_move_stage(&w, team->current, &team->current_info, team->current_known, m) < 0) {
				// no memory: the analysis ends with an error (up to v4.5.5-nikque.26 the line of the move was left
				// out of an @end without error)
				if (!ba_cancelled(job)) { lock(team); team->failed = true; unlock(team); }
				break;
			}
			ba_print_move(job, m, true);
		} else {
			if (!ba_move_umi(&w, job->player, m)) break;
			ba_print_umi(job, m);
		}
	}
	ba_work_free(&w);
	return NULL;
}

/** @brief Do a part for all the moves, with options.n_task threads. */
static void ba_team(BaTeam *team)
{
	BaThread th[MAX_THREADS];
	int i, n = ba_n_thread(), n_book = 0;

	for (i = 0; i < team->n_move; ++i) if (team->move[i].p && team->move[i].same < 0) ++n_book;
	if (n > n_book) n = n_book;
	if (n < 1) n = 1;
	team->next = 0;
	for (i = 0; i < n; ++i) th[i].team = team;
	thread_run_workers(ba_team_run, th, sizeof *th, n, true, false);
}

/** @brief Moves of the current position, with what the book holds of them. @return their number. */
static int ba_moves(const BaJob *job, const BaParams *prm, BaWork *w, BaMove *move, Position **current, unsigned int *current_idx)
{
	const Board *board = &job->board;
	unsigned long long moves = get_moves(board->player, board->opponent);
	MoveList movelist;
	unsigned int idx = 0;
	Position *p = ba_probe(w, board, &idx);
	int n = 0, x, sym = 0;

	*current = p;
	*current_idx = idx;
	if (p) sym = position_get_moves(p, board, &movelist);
	if (moves == 0 && can_move(board->opponent, board->player)) {
		if (!prm->moves_set || prm->moves_pass) move[n++].x = PASS;
	} else {
		for (; moves; moves &= moves - 1) {
			x = (int) first_bit(moves);
			if (!prm->moves_set || (prm->moves & (1ULL << x))) move[n++].x = x;
		}
	}
	for (x = 0; x < n; ++x) {
		BaMove *m = move + x;
		Board next;
		const Move *mv;
		board_next(board, m->x, &next);
		ba_square(m->x, m->sq);
		m->p = ba_probe(w, &next, &m->idx);
		m->has_value = m->leaf = m->has_link = false;
		m->level = 0;
		if (p) foreach_move(mv, movelist) if (mv->x == m->x) {
			m->leaf = (p->leaf.move == PASS) ? (m->x == PASS) : (p->leaf.move != NOMOVE && symetry(p->leaf.move, sym) == m->x);
			if (m->leaf) {
				m->has_value = true;
				m->value = mv->score;
				m->level = p->level;
			} else {
				m->has_link = true;
				m->link = mv->score;
			}
		}
		if (m->p) { // (the value of the position after the move: up to date after a deviate started from it)
			m->has_value = true;
			m->value = -m->p->score.value;
			m->level = m->p->level;
		}
		m->stage = 0;
		m->missing = 0;
		m->same = -1;
		if (m->p) {
			int i;
			for (i = 0; i < x; ++i) if (move[i].p && move[i].idx == m->idx) { m->same = i; break; }
		}
	}
	return n;
}

static int ba_moveinfo(BaJob *job, const BaParams *prm)
{
	const long long t = real_clock();
	BaMove move[MAX_MOVE + 1];
	BaTeam team;
	BaWork w;
	Position *current;
	unsigned int current_idx;
	int i, n;
	bool failed;

	memset(move, 0, sizeof move);
	ba_work_init(&w, job, prm);
	n = ba_moves(job, prm, &w, move, &current, &current_idx);
	ba_print("@begin id=%d ply=%d turn=%s moves=%d book=%d\n", job->id, 60 - board_count_empties(&job->board), job->player == BLACK ? "black" : "white", n, current != NULL);

	memset(&team, 0, sizeof team);
	team.job = job;
	team.prm = prm;
	team.move = move;
	team.n_move = n;
	team.current = current;
	team.current_known = current && ba_cache_get_stage(current_idx, ba_cond_stage(prm), &team.current_info);
	lock_init(&team);
	// the moves out of the book first, then the precision of the others
	for (i = 0; i < n; ++i) if (move[i].p == NULL || !(prm->parts & BA_PART_STAGE)) {
		ba_print_move(job, move + i, move[i].p == NULL); // (a move out of the book is written with its stage 0)
	}
	if ((prm->parts & BA_PART_STAGE) && !ba_cancelled(job)) {
		team.part = BA_PART_STAGE;
		ba_team(&team);
		for (i = 0; i < n && !ba_cancelled(job) && !team.failed; ++i) if (move[i].p && move[i].same >= 0) {
			move[i].stage = move[move[i].same].stage;
			move[i].missing = move[move[i].same].missing;
			ba_print_move(job, move + i, true);
		}
	}
	if ((prm->parts & BA_PART_UMI) && prm->umi != BA_UMI_OFF && !ba_cancelled(job) && !team.failed) {
		team.part = BA_PART_UMI;
		ba_team(&team);
		for (i = 0; i < n && !ba_cancelled(job); ++i) if (move[i].p && move[i].same >= 0) {
			const BaMove *s = move + move[i].same;
			move[i].umi[BLACK] = s->umi[BLACK];
			move[i].umi[WHITE] = s->umi[WHITE];
			move[i].umi_state = s->umi_state;
			move[i].umi_exact = s->umi_exact;
			ba_print_umi(job, move + i);
		}
	}
	failed = team.failed;
	lock_free(&team);
	if ((prm->parts & BA_PART_COUNT) && !ba_cancelled(job) && !failed) {
		long long hist[61];
		for (i = 0; i < n && !ba_cancelled(job); ++i) if (move[i].p) {
			const long long k = ba_reach_kept(&w, move[i].p, move[i].idx, hist);
			if (k < 0) { failed = !ba_cancelled(job); break; } // (no memory: an error, as for the stages)
			ba_print("@move id=%d sq=%s part=count positions=%lld\n", job->id, move[i].sq, k);
		}
	}
	ba_work_free(&w);
	if (ba_cancelled(job) || failed) return -1; // (ba_run tells it: @cancel, or @error memory=1)
	ba_print("@end id=%d ms=%lld\n", job->id, real_clock() - t);
	return 0;
}

/* ------------------------------------------------------------------ book missing ------------------------------------------------------------ */

/** @return a move of a position of the book (on its board), on a board of the user interface that is a symmetry of it (the move itself if none is). */
static int ba_ui_move(const Board *book_board, const Board *board, const int x)
{
	Board t;
	int sym;
	if (x != PASS) for (sym = 0; sym < 8; ++sym) {
		board_symetry(book_board, sym, &t);
		if (board_equal(&t, board)) return symetry(x, sym);
	}
	return x;
}

/**
 * @brief Moves of a line of a walk, on the board of the user interface.
 * @param q Queue of the walk.
 * @param item Last position of the line.
 * @param board Board of the root (out: board of the last position).
 * @param passes true if the passes are written ("pa").
 * @param s Moves (out).
 */
static void ba_line(const BaQueue *q, long long item, Board *board, const bool passes, char *s)
{
	long long chain[512];
	int n = 0, k, m = 0;

	while (item >= 0 && n < 512) { chain[n++] = item; item = q->item[item].parent; }
	for (k = n - 2; k >= 0; --k) { // (chain[n - 1] is the root)
		const BaItem *it = q->item + chain[k];
		const int x = ba_ui_move(&q->item[it->parent].p->board, board, it->move);
		Board next;
		if (x != PASS || passes) { ba_square(x, s + m); m += 2; }
		board_next(board, x, &next);
		*board = next;
	}
	s[m] = '\0';
}

/** @return the leaf move of a position, on a board of the user interface. */
static int ba_leaf_move(const Position *p, const Board *board)
{
	Board t;
	int sym;
	if (p->leaf.move == PASS || p->leaf.move == NOMOVE) return p->leaf.move;
	for (sym = 0; sym < 8; ++sym) {
		board_symetry(&p->board, sym, &t);
		if (board_equal(&t, board)) return symetry(p->leaf.move, sym);
	}
	return NOMOVE;
}

/**
 * @brief Tell if the walk of book missing of the move loss x and the total loss y from a position is known to find no
 * leaf to expand, by the kept stage of the position (see BaStageInfo): deviate, a complete range of the same losses;
 * deviate2, deviate3, a range of the same move loss whose B_k is above y.
 */
static bool ba_missing_complete(const BaParams *prm, const unsigned int idx, const int x, const int y)
{
	BaStageInfo s;
	int k;

	if (!ba_cache_get_stage(idx, ba_cond_stage(prm), &s)) return false;
	for (k = 0; k < prm->n_stage; ++k) {
		if (prm->stage_x[k] != x || s.lb[k] <= y) continue;
		if (prm->base != BA_DEVIATE || prm->stage_y[k] == y) return true;
	}
	return false;
}

static int ba_missing(BaJob *job, const BaParams *prm)
{
	const long long t = real_clock();
	BaWork w;
	Board root_board = job->board;
	char prefix[3] = "";
	Position *root;
	unsigned int root_idx;
	int stage = prm->stage, x = prm->x, y = prm->y, root_stage = -1, ply0 = 0;
	long long count = 0, shown = 0, i;
	bool best = false;

	ba_work_init(&w, job, prm);
	if (prm->sq != NOMOVE) {
		const unsigned long long moves = get_moves(root_board.player, root_board.opponent);
		Board next;
		const bool legal = (prm->sq == PASS) ? (moves == 0 && can_move(root_board.opponent, root_board.player)) : ((moves >> prm->sq) & 1) != 0;
		if (!legal) {
			ba_work_free(&w);
			ba_print("@error id=%d word=sq\n@end id=%d error=1\n", job->id, job->id);
			return 0;
		}
		board_next(&root_board, prm->sq, &next);
		root_board = next;
		ba_square(prm->sq, prefix);
		ply0 = 1;
	}
	root = ba_probe(&w, &root_board, &root_idx);
	if (root == NULL) {
		ba_work_free(&w);
		ba_print("@end id=%d count=0 shown=0 stage=0 ms=%lld\n", job->id, real_clock() - t);
		return 0;
	}
	if (stage == 0 && !prm->xy_set) { // the leaves missing for the next stage
		long long missing;
		root_stage = ba_stage(&w, root, root_idx, &missing);
		if (root_stage < 0) { ba_work_free(&w); return -1; }
		stage = root_stage + 1;
		if (stage > 2 + prm->n_stage) {
			ba_work_free(&w);
			ba_print("@end id=%d count=0 shown=0 stage=%d ms=%lld\n", job->id, root_stage, real_clock() - t);
			return 0;
		}
	}
	if (stage == 2) {
		bool found;
		best = true;
		count = ba_best_lines(&w, root, root_idx, &found, true);
	} else {
		if (stage >= 3) { x = prm->stage_x[stage - 3]; y = prm->stage_y[stage - 3]; }
		count = ba_missing_complete(prm, root_idx, x, y) ? 0 : ba_expansions(&w, root, root_idx, prm->base, x, y, true);
	}
	if (count < 0) { ba_work_free(&w); return -1; }

	for (i = 0; i < w.todo.ref.n && shown < prm->n && !ba_cancelled(job); ++i) {
		const long long ref = w.todo.ref.v[i];
		const int which = (int) (ref >> 40);
		const long long item = ref & ((1LL << 40) - 1);
		const BaItem *it = w.q[which].item + item;
		const Position *p = it->p;
		char line[2 * 512 + 3], leaf[3];
		Board board = root_board;
		int sign;

		strcpy(line, prefix);
		ba_line(w.q + which, item, &board, true, line + strlen(line));
		sign = ((ply0 + it->depth) & 1) ? -1 : 1; // (values seen by the side to move at the current position)
		ba_square(ba_leaf_move(p, &board), leaf);
		if (best) {
			ba_print("@line id=%d moves=%s leaf=%s value=%d empties=%d\n", job->id, line, leaf, sign * p->score.value, board_count_empties(&board));
		} else if (prm->base == BA_DEVIATE) {
			ba_print("@line id=%d moves=%s leaf=%s value=%d empties=%d\n", job->id, line, leaf, sign * p->leaf.score, board_count_empties(&board));
		} else {
			ba_print("@line id=%d moves=%s leaf=%s value=%d empties=%d loss=%d\n", job->id, line, leaf, sign * p->leaf.score, board_count_empties(&board), it->loss + p->score.value - p->leaf.score);
		}
		++shown;
	}
	ba_work_free(&w);
	if (ba_cancelled(job)) return -1;
	if (root_stage >= 0) ba_print("@end id=%d count=%lld shown=%lld stage=%d ms=%lld\n", job->id, count, shown, root_stage, real_clock() - t);
	else ba_print("@end id=%d count=%lld shown=%lld ms=%lld\n", job->id, count, shown, real_clock() - t);
	return 0;
}

/* ------------------------------------------------------------------ book levels ------------------------------------------------------------- */

static int ba_levels(BaJob *job, const BaParams *prm)
{
	const long long t = real_clock();
	long long hist[61], n = 0;
	int i;

	memset(hist, 0, sizeof hist);
	if (prm->all) {
		const Book *book = job->book;
		for (i = 0; i < book->n && !ba_cancelled(job); ++i) {
			const PositionArray *a = book->array + i;
			int k;
			for (k = 0; k < a->n; ++k) ++hist[a->positions[k].level];
			n += a->n;
		}
	} else {
		BaWork w;
		unsigned int idx;
		Position *root;
		ba_work_init(&w, job, prm);
		root = ba_probe(&w, &job->board, &idx);
		if (root) n = ba_reach_kept(&w, root, idx, hist);
		ba_work_free(&w);
		if (n < 0) return -1;
	}
	if (ba_cancelled(job)) return -1;
	for (i = 0; i <= 60; ++i) if (hist[i]) ba_print("@level id=%d level=%d count=%lld\n", job->id, i, hist[i]);
	ba_print("@end id=%d positions=%lld ms=%lld\n", job->id, n, real_clock() - t);
	return 0;
}

/* ------------------------------------------------------------------ book bestlines ---------------------------------------------------------- */

/** @return true if a line ends at a position that is not solved (the book needs more learning there). */
static inline bool ba_end_incomplete(const Position *p)
{
	return !board_is_game_over(&p->board) && !position_is_solved(p);
}

/**
 * @brief Lines of book bestlines: from a root, the links of both sides whose loss (the value of the position minus
 * the score of the link) is at most prm->loss, up to the move number prm->depth; a solved position ends a line
 * unless prm->follow_solved. A transposition is one node. parent and move keep, for each node, the node and the move
 * it was first reached from (the nodes are added ply by ply: a shortest line).
 * @return 0: done, -1: stopped or memory exhausted.
 */
static int ba_bestlines_build(BaWork *w, Position *root, const unsigned int root_idx, unsigned int **parent, unsigned char **move)
{
	const BaParams *prm = w->prm;
	unsigned int i, size = 0;
	bool added;

	ba_umi_free(w);
	*parent = NULL;
	*move = NULL;
	if (ba_umi_node(w, root, root_idx, 0, 0, &added) == UINT_MAX) return -1;
	for (i = 0; i < w->n_node; ++i) { // (the nodes are added while they are read)
		Position *p = w->node[i].p;
		const int v = p->score.value;
		const unsigned long long first = w->n_edge;
		Link *l;

		if (ba_cancelled(w->job)) return -1;
		if (60 - board_count_empties(&p->board) < prm->depth && (prm->follow_solved || !position_is_solved(p))) {
			foreach_link(l, p) {
				const int e = v - l->score;
				Board target;
				unsigned int ci, c;
				Position *child;

				if (e < 0 || e > prm->loss) continue;
				board_next(&p->board, l->move, &target);
				child = ba_probe(w, &target, &ci);
				if (child == NULL) continue;
				c = ba_umi_node(w, child, ci, 0, 0, &added);
				if (c == UINT_MAX || !ba_edge_push(w, c)) return -1;
				if (added) {
					if (c >= size) {
						const unsigned int n = size + size / 2 + 4096;
						unsigned int *pa = (unsigned int*) realloc(*parent, (size_t) n * sizeof *pa);
						unsigned char *mo;
						if (pa == NULL) return -1;
						*parent = pa;
						mo = (unsigned char*) realloc(*move, (size_t) n);
						if (mo == NULL) return -1;
						*move = mo;
						size = n;
					}
					(*parent)[c] = i;
					(*move)[c] = l->move;
				}
			}
		}
		w->node[i].edge = (unsigned int) first;
		w->node[i].n_edge = (unsigned int) (w->n_edge - first);
		w->node[i].end = (w->node[i].n_edge == 0);
	}
	// an order where the next positions come first
	{
		unsigned int count[131];
		memset(count, 0, sizeof count);
		for (i = 0; i < w->n_node; ++i) ++count[w->node[i].key];
		if (!ba_node_order(w, count, false)) return -1;
	}
	return 0;
}

/** @return a + b, at most ULLONG_MAX. */
static inline unsigned long long ba_add_sat(const unsigned long long a, const unsigned long long b)
{
	return a > ULLONG_MAX - b ? ULLONG_MAX : a + b;
}

/**
 * @brief book bestlines: the lines of best moves (and of moves losing at most loss) of both sides from the current
 * position, counted by line (a transposition is counted by each line that reaches it, as the original count) and by
 * end position; with file, one line per end position (the shortest one), the moves of the game first.
 */
static int ba_bestlines(BaJob *job, const BaParams *prm)
{
	const long long t = real_clock();
	BaWork w;
	Position *root;
	unsigned int root_idx, *parent = NULL, r;
	unsigned char *move = NULL;
	unsigned long long *lines = NULL, *incomplete = NULL;
	long long ends = 0, incomplete_ends = 0, written = 0;
	FILE *f = NULL;
	char *tmp = NULL; // (the file is written under this name, then renamed: see book_text_create)
	int result = -1;

	ba_work_init(&w, job, prm);
	root = ba_probe(&w, &job->board, &root_idx);
	if (root == NULL) {
		ba_work_free(&w);
		ba_print("@end id=%d lines=0 ends=0 incomplete=0 incomplete_ends=0 positions=0 written=0 ms=%lld\n", job->id, real_clock() - t);
		return 0;
	}
	if (prm->file[0] && (f = book_text_create(prm->file, &tmp)) == NULL) {
		ba_work_free(&w);
		ba_print("@error id=%d word=file\n@end id=%d error=1\n", job->id, job->id);
		return 0;
	}
	if (ba_bestlines_build(&w, root, root_idx, &parent, &move) < 0) {
		if (w.too_large && !ba_cancelled(job)) { // (more positions than BA_UMI_NODE_MAX)
			ba_print("@error id=%d size=1\n@end id=%d error=1\n", job->id, job->id);
			result = 0;
		}
		goto end;
	}
	lines = (unsigned long long*) malloc((size_t) w.n_node * sizeof *lines);
	incomplete = (unsigned long long*) malloc((size_t) w.n_node * sizeof *incomplete);
	if (lines == NULL || incomplete == NULL) goto end;
	for (r = 0; r < w.n_node; ++r) { // (the next positions first)
		const unsigned int v = w.order[r];
		const BaUNode *u = w.node + v;
		if (u->end) {
			const bool inc = ba_end_incomplete(u->p);
			lines[v] = 1;
			incomplete[v] = inc;
			++ends;
			incomplete_ends += inc;
		} else {
			unsigned long long k, a = 0, b = 0;
			for (k = u->edge; k < (unsigned long long) u->edge + u->n_edge; ++k) {
				a = ba_add_sat(a, lines[w.edge[k]]);
				b = ba_add_sat(b, incomplete[w.edge[k]]);
			}
			lines[v] = a;
			incomplete[v] = b;
		}
	}
	if (f) {
		static char buffer[1 << 20];
		unsigned int v, chain[128];
		setvbuf(f, buffer, _IOFBF, sizeof buffer);
		for (v = 0; v < w.n_node && !ba_cancelled(job); ++v) {
			const BaUNode *u = w.node + v;
			char s[2 * 128 + 1];
			Board board = job->board;
			int n = 0, k, m = 0;
			if (!u->end || (prm->incomplete_only && !ba_end_incomplete(u->p))) continue;
			for (r = v; r != 0 && n < 128; r = parent[r]) chain[n++] = r; // (node 0 is the root)
			for (k = n - 1; k >= 0; --k) {
				const int x = ba_ui_move(&w.node[parent[chain[k]]].p->board, &board, move[chain[k]]);
				Board next;
				if (x != PASS) { ba_square(x, s + m); m += 2; }
				board_next(&board, x, &next);
				board = next;
			}
			s[m] = '\0';
			if (fprintf(f, "%s%s\n", job->line ? job->line : "", s) < 0) break;
			++written;
		}
		{ // (the file is in its place before the answer: the user interface may read it at once)
			const bool cancelled = ba_cancelled(job);
			const bool written = book_text_commit(f, tmp, prm->file, !cancelled); // (stopped: the former file is kept)
			f = NULL;
			if (!cancelled && !written) {
				ba_print("@error id=%d word=file\n@end id=%d error=1\n", job->id, job->id);
				result = 0;
				goto end;
			}
		}
	}
	if (!ba_cancelled(job)) {
		ba_print("@end id=%d lines=%llu ends=%lld incomplete=%llu incomplete_ends=%lld positions=%u written=%lld ms=%lld\n",
			job->id, lines[0], ends, incomplete[0], incomplete_ends, w.n_node, written, real_clock() - t);
		result = 0;
	}
end:
	if (f) book_text_commit(f, tmp, prm->file, false); // (a file stopped half way is removed: the former file is kept)
	free(lines); free(incomplete); free(parent); free(move);
	ba_work_free(&w);
	return result;
}

/* ------------------------------------------------------------------ book deviate export ------------------------------------------------------ */

#define BA_EXPORT_ON 2          /**< (marks of book deviate export) a walk goes on from the position to a position of the book */
#define BA_EXPORT_WRITTEN 4     /**< its line is written */

/**
 * @brief Mark the positions from which the walks of ba_expansions (book deviate: two walks; deviate2, deviate3: one
 * walk) go on, and add to the walk of deviate2 / deviate3 the next positions it does not walk.
 *
 * A position of the lines is the root or a position of the book that a link followed by a walk leads to (walked
 * or not: the end of the game, the depth of the book). A walk goes on from a position it walks when one of the links
 * it follows there leads to a position of the book.
 * @return false if memory is exhausted.
 */
static bool ba_export_marks(BaWork *w, Position *root, const unsigned int root_idx, const int base, const int x, const int y, BaVisit *mark)
{
	const Book *book = w->book;
	BaVisit walked;
	int which;
	bool ok = true;

	ba_visit_init(&walked, ba.index.n);
	if (base != BA_DEVIATE && w->q[0].n == 0) ok = ba_push(w->q, root, root_idx, -1, NOMOVE, 0, 0); // (a root that is not walked)
	for (which = 0; ok && which < (base == BA_DEVIATE ? 2 : 1); ++which) {
		BaQueue *q = w->q + which;
		const int s = root->score.value, lower = s - y, upper = s + y;
		long long i;

		ba_visit_clear(&walked);
		for (i = 0; ok && i < q->n; ++i) {
			const BaItem it = q->item[i]; // (q->item may move: ba_push below)
			Position *p = it.p;
			const int v = p->score.value;
			Link *l;

			if (ba_visit_get(&walked, it.idx) || !ba_walkable(book, p)) continue;
			if (base == BA_DEVIATE) { // (as ba_walk_deviate and ba_deviate_rec: the first item of a position that passes is the one walked)
				const int parity = it.depth & 1;
				const int deviation = (parity ^ which) ? 0 : x; // (walk 0: x for the side to move at the root; walk 1: for the other side)
				const int lo = parity ? -upper : lower, up = parity ? -lower : upper;
				if (!(lo <= v && v <= up)) continue;
				ok = ba_visit_set(&walked, it.idx, 1);
				foreach_link(l, p) {
					if (ok && v - l->score <= deviation && lo <= l->score && l->score <= up) {
						Board target;
						unsigned int ci;
						board_next(&p->board, l->move, &target);
						if (ba_probe(w, &target, &ci)) { ok = ba_visit_set(mark, it.idx, ba_visit_get(mark, it.idx) | BA_EXPORT_ON); break; }
					}
				}
			} else { // (as ba_walk_total: the item of the smallest loss is the one walked; the next positions not walked are added)
				if (ba_visit_get(&w->visit, it.idx) != it.loss + 1) continue;
				ok = ba_visit_set(&walked, it.idx, 1);
				foreach_link(l, p) {
					const int move_error = v - l->score;
					if (ok && 0 <= move_error && move_error <= x && it.loss + move_error <= y) {
						Board target;
						unsigned int ci;
						Position *child;
						board_next(&p->board, l->move, &target);
						child = ba_probe(w, &target, &ci);
						if (child == NULL) continue;
						ok = ba_visit_set(mark, it.idx, ba_visit_get(mark, it.idx) | BA_EXPORT_ON);
						if (ok && !ba_walkable(book, child)) ok = ba_push(q, child, ci, i, l->move, it.depth + 1, it.loss + move_error);
					}
				}
			}
		}
	}
	ba_visit_free(&walked);
	return ok;
}

/** An item of a walk that reaches a position: the line of the position is the one of the smallest key. */
typedef struct BaExportPick {
	unsigned int idx;       /**< number of the position */
	unsigned int key;       /**< moves from the root (deviate), or accumulated loss (deviate2, deviate3) */
	long long ref;          /**< walk << 40 | item */
} BaExportPick;

static int ba_export_pick_cmp(const void *a, const void *b)
{
	const BaExportPick *x = (const BaExportPick*) a, *y = (const BaExportPick*) b;
	if (x->idx != y->idx) return x->idx < y->idx ? -1 : 1;
	if (x->key != y->key) return x->key < y->key ? -1 : 1;
	return (x->ref > y->ref) - (x->ref < y->ref);
}

static int ba_export_ref_cmp(const void *a, const void *b)
{
	const long long x = *(const long long*) a, y = *(const long long*) b;
	return (x > y) - (x < y);
}

/**
 * @brief The line of each position of the lines of the walks: the fewest moves (book deviate), the smallest
 * accumulated loss (deviate2, deviate3), then the first reached; in the order of the walks.
 * @return false if memory is exhausted.
 */
static bool ba_export_order(const BaWork *w, const int base, BaList *order)
{
	const int n_walk = base == BA_DEVIATE ? 2 : 1;
	long long n = 0, i, k = 0;
	int which;
	BaExportPick *pick;

	for (which = 0; which < n_walk; ++which) n += w->q[which].n;
	pick = (BaExportPick*) malloc((size_t) (n ? n : 1) * sizeof *pick);
	if (pick == NULL) return false;
	for (which = 0; which < n_walk; ++which) {
		const BaQueue *q = w->q + which;
		for (i = 0; i < q->n; ++i, ++k) {
			pick[k].idx = q->item[i].idx;
			pick[k].key = base == BA_DEVIATE ? q->item[i].depth : q->item[i].loss;
			pick[k].ref = ((long long) which << 40) | i;
		}
	}
	qsort(pick, (size_t) n, sizeof *pick, ba_export_pick_cmp);
	for (k = 0; k < n; ++k) {
		if (k > 0 && pick[k].idx == pick[k - 1].idx) continue;
		if (!ba_list_push(order, pick[k].ref)) { free(pick); return false; }
	}
	free(pick);
	qsort(order->v, (size_t) order->n, sizeof *order->v, ba_export_ref_cmp);
	return true;
}

/**
 * @brief book deviate, deviate2, deviate3 <x> <y> export <file> [leaf|ends|all]: write the lines of the walk of the
 * command from the current position, without expanding the book: one line a position (from the start of the game,
 * without the passes), the line of the fewest moves (deviate) or of the smallest accumulated loss (deviate2,
 * deviate3).
 *
 * - BOOK_EXPORT_LEAF: the positions whose leaf the command would expand in its first round.
 * - BOOK_EXPORT_ENDS: the positions of the lines from which no walk goes on.
 * - BOOK_EXPORT_ALL: all the positions of the lines.
 *
 * @param book Opening book.
 * @param board Current position.
 * @param line Moves of the game up to the current position (written first), or NULL.
 * @param base 1: book deviate, 2: deviate2, 3: deviate3.
 * @param x Relative error (deviate), or loss of one move (deviate2, deviate3).
 * @param y Absolute error (deviate), or cumulative loss (deviate2, deviate3: above VISIT_LOSS_MAX, reduced to it with
 * the warning of book deviate2, deviate3).
 * @param file File to write.
 * @param which BOOK_EXPORT_LEAF, BOOK_EXPORT_ENDS or BOOK_EXPORT_ALL.
 * @return the number of lines written, -1 on error (said).
 */
long long book_deviate_export(Book *book, const Board *board, const char *line, const int base, const int x, int y, const char *file, const int which)
{
	static const char *names[] = {"", "deviate", "deviate2", "deviate3"}, *whats[] = {"leaf", "ends", "all"};
	const char *name = names[base], *what = whats[which];
	BaJob job;
	BaParams prm;
	BaWork w;
	BaVisit mark;
	BaList order = {NULL, 0, 0};
	Position *root;
	unsigned int root_idx;
	long long k, written = 0;
	bool ok;
	FILE *f;
	char *tmp;

	// (as book deviate2, deviate3: a mark of one byte a position; up to v4.5.5-nikque.26 edax and libedax each warned
	// in their own words)
	if (base != 1 && y > VISIT_LOSS_MAX) {
		warn("total loss %d reduced to %d\n", y, VISIT_LOSS_MAX);
		y = VISIT_LOSS_MAX;
	}
	f = book_text_create(file, &tmp); // (written under another name, then renamed)
	if (f == NULL) {
		printf("ERROR: book %s %d %d export: cannot write the file %s\n", name, x, y, file);
		return -1;
	}
	book_analysis_init();
	lock(&ba);
	ok = ba_index_get(book);
	unlock(&ba);
	if (!ok) {
		book_text_commit(f, tmp, file, false);
		printf("ERROR: book %s %d %d export: not enough memory\n", name, x, y);
		return -1;
	}
	memset(&job, 0, sizeof job);
	memset(&prm, 0, sizeof prm);
	job.book = book;
	job.board = *board;
	ba_work_init(&w, &job, &prm);
	ba_visit_init(&mark, ba.index.n);
	root = ba_probe(&w, board, &root_idx);
	ok = root == NULL || ba_expansions(&w, root, root_idx, base, x, y, true) >= 0;
	if (ok && root) ok = ba_export_marks(&w, root, root_idx, base, x, y, &mark) && ba_export_order(&w, base, &order);
	if (ok && root) {
		static char buffer[1 << 20];
		char s[1024];
		setvbuf(f, buffer, _IOFBF, sizeof buffer);
		for (k = 0; k < order.n; ++k) {
			const long long ref = order.v[k], item = ref & ((1LL << 40) - 1);
			const BaQueue *q = w.q + (ref >> 40);
			const BaItem *it = q->item + item;
			const unsigned char m = ba_visit_get(&mark, it->idx);
			Board b = *board;
			if (which == BOOK_EXPORT_ENDS && (m & BA_EXPORT_ON)) continue;
			if (which == BOOK_EXPORT_LEAF && !ba_visit_get(&w.todo.seen, it->idx)) continue; // (a leaf to expand)
			// (the passes are not written: after a pass, the line of the position before it, already written, is the same)
			if (it->move == PASS && it->parent >= 0 && (ba_visit_get(&mark, q->item[it->parent].idx) & BA_EXPORT_WRITTEN)) continue;
			if (!ba_visit_set(&mark, it->idx, m | BA_EXPORT_WRITTEN)) { ok = false; break; }
			ba_line(q, item, &b, false, s); // (from the current position, without the passes)
			if (fprintf(f, "%s%s\n", line ? line : "", s) < 0) break;
			++written;
		}
	}
	ok = book_text_commit(f, tmp, file, ok); // (the former file is kept if this one is not complete)
	f = NULL;
	ba_visit_free(&mark);
	free(order.v);
	ba_work_free(&w);
	if (!ok) {
		printf("ERROR: book %s %d %d export: not enough memory, or the file %s cannot be written\n", name, x, y, file);
		return -1;
	}
	if (root == NULL) printf("Book %s %d %d export %s...finished: the position is not in the book, 0 lines written (%s)\n", name, x, y, what, file);
	else printf("Book %s %d %d export %s...finished: %lld lines written (%s)\n", name, x, y, what, written, file);
	return written;
}

/* ------------------------------------------------------------------ book findline ----------------------------------------------------------- */

#define BA_FIND_N_MAX 100                   /**< lines of book findline */
#define BA_FIND_BEST_MAX (1ULL << 26)       /**< costs kept by book findline (all its positions) */

/** A position of the lines of book findline: the board as it is played from the current position (not its symmetry in the book). */
typedef struct BaFNode {
	Board board;                /**< side to move first */
	unsigned long long paths;   /**< lines from the current position (at most ULLONG_MAX) */
	unsigned int next;          /**< next node of the same slot of the hash table, UINT_MAX if none */
	int value;                  /**< book value, seen by black */
	unsigned short n_best;      /**< costs kept, the smallest first */
	unsigned char color;        /**< side to move */
} BaFNode;

/** One of the smallest costs of the lines to a node, and where it comes from. */
typedef struct BaFBest {
	int cost;
	unsigned int parent;        /**< previous node, UINT_MAX at the current position */
	unsigned short rank;        /**< cost of the previous node that the line continues */
	unsigned char move;         /**< move from the previous node */
} BaFBest;

typedef struct BaFind {
	BaFNode *node;
	unsigned int n_node, size_node;
	BaFBest *best;              /**< n costs per node */
	int n;
	unsigned int *slot;         /**< hash table of the nodes (first node of a slot, UINT_MAX if none) */
	unsigned int mask;
	BaList layer[131];          /**< nodes by 2 * discs + (the side to move can move): a next position has a greater key */
	bool too_large;
} BaFind;

static inline int ba_find_key(const Board *board)
{
	return 2 * (64 - board_count_empties(board)) + (can_move(board->player, board->opponent) ? 1 : 0);
}

static inline unsigned int ba_find_slot(const BaFind *f, const Board *board, const int color)
{
	return (unsigned int) (ba_hash(board_get_hash_code(board) ^ (unsigned long long) color) & f->mask);
}

static bool ba_find_rehash(BaFind *f)
{
	const unsigned int size = f->slot ? 2 * (f->mask + 1) : 4096;
	unsigned int *slot = (unsigned int*) malloc((size_t) size * sizeof *slot);
	unsigned int i;

	if (slot == NULL) return false;
	free(f->slot);
	f->slot = slot;
	f->mask = size - 1;
	memset(slot, 0xff, (size_t) size * sizeof *slot);
	for (i = 0; i < f->n_node; ++i) {
		const unsigned int s = ba_find_slot(f, &f->node[i].board, f->node[i].color);
		f->node[i].next = slot[s];
		slot[s] = i;
	}
	return true;
}

/** @return the node of a board, UINT_MAX if there is none. */
static unsigned int ba_find_get(const BaFind *f, const Board *board, const int color)
{
	unsigned int i;
	if (f->slot == NULL) return UINT_MAX;
	for (i = f->slot[ba_find_slot(f, board, color)]; i != UINT_MAX; i = f->node[i].next) {
		if (f->node[i].color == color && board_equal(&f->node[i].board, board)) return i;
	}
	return UINT_MAX;
}

/** @brief Add a node (that is not there yet). @return its number, UINT_MAX if memory is exhausted or there are too many. */
static unsigned int ba_find_add(BaFind *f, const Board *board, const int color, const int value)
{
	BaFNode *u;
	unsigned int s;

	if (f->n_node == BA_UMI_NODE_MAX || (unsigned long long) (f->n_node + 1) * (unsigned long long) f->n > BA_FIND_BEST_MAX) {
		f->too_large = true;
		return UINT_MAX;
	}
	if (f->n_node == f->size_node) {
		const unsigned int size = f->size_node + f->size_node / 2 + 1024;
		BaFNode *node = (BaFNode*) realloc(f->node, (size_t) size * sizeof *node);
		BaFBest *best;
		if (node == NULL) return UINT_MAX;
		f->node = node;
		best = (BaFBest*) realloc(f->best, (size_t) size * (size_t) f->n * sizeof *best);
		if (best == NULL) return UINT_MAX;
		f->best = best;
		f->size_node = size;
	}
	if ((f->n_node + 1) * 2 > (f->slot ? f->mask + 1 : 0) && !ba_find_rehash(f)) return UINT_MAX;
	u = f->node + f->n_node;
	u->board = *board;
	u->color = (unsigned char) color;
	u->value = value;
	u->paths = 0;
	u->n_best = 0;
	s = ba_find_slot(f, board, color);
	u->next = f->slot[s];
	f->slot[s] = f->n_node;
	return f->n_node++;
}

/** @brief Keep a cost among the n smallest of a node (after the costs that are not greater). */
static void ba_find_keep(BaFind *f, const unsigned int v, const BaFBest *x)
{
	BaFBest *b = f->best + (size_t) v * (size_t) f->n;
	BaFNode *u = f->node + v;
	int i;

	if (u->n_best == f->n) {
		if (x->cost >= b[f->n - 1].cost) return;
		i = f->n - 1;
	} else i = u->n_best++;
	while (i > 0 && b[i - 1].cost > x->cost) { b[i] = b[i - 1]; --i; }
	b[i] = *x;
}

/** A next position to look for in the book. */
typedef struct BaFProbe {
	Board next, unique;
	unsigned long long bucket;
	unsigned int u;             /**< node it comes from */
	int x;                      /**< move */
} BaFProbe;

/**
 * @brief Look for a batch of next positions in the book (their buckets were asked for, see BA_PROBE_BATCH), in their
 * order; keep the costs of those in the book.
 * @return false if memory is exhausted (or there are too many positions).
 */
static bool ba_find_batch(BaFind *f, const Book *book, const BaFProbe *probe, const int n)
{
	int i, j, k;

	for (i = 0; i < n; ++i) {
		const PositionArray *a = book->array + probe[i].bucket;
		const char *s = (const char*) a->positions, *e = s + (size_t) a->n * sizeof (Position);
		for (s -= (uintptr_t) s & 63; s < e; s += 64) PREFETCH(s);
	}
	for (i = 0; i < n; ++i) {
		const BaFProbe *b = probe + i;
		const PositionArray *a = book->array + b->bucket;
		const Position *q = NULL;
		const unsigned int u = b->u;
		const int color = f->node[u].color;
		unsigned int c;

		for (j = 0; j < a->n; ++j) if (board_equal(&a->positions[j].board, &b->unique)) { q = a->positions + j; break; }
		if (q == NULL) continue;
		c = ba_find_get(f, &b->next, !color);
		if (c == UINT_MAX) {
			c = ba_find_add(f, &b->next, !color, color == BLACK ? -q->score.value : q->score.value);
			if (c == UINT_MAX || !ba_list_push(&f->layer[ba_find_key(&b->next)], c)) return false;
		}
		{
			const int e = abs(f->node[c].value - f->node[u].value);
			const BaFBest *bu = f->best + (size_t) u * (size_t) f->n;
			const BaFBest *last = f->best + (size_t) c * (size_t) f->n + (size_t) f->n - 1;
			f->node[c].paths = ba_add_sat(f->node[c].paths, f->node[u].paths);
			for (k = 0; k < f->node[u].n_best; ++k) {
				const BaFBest y = {bu[k].cost + e, u, (unsigned short) k, (unsigned char) b->x};
				if (f->node[c].n_best == f->n && y.cost >= last->cost) break; // (the next costs of u are not smaller)
				ba_find_keep(f, c, &y);
			}
		}
	}
	return true;
}

static void ba_find_free(BaFind *f)
{
	int i;
	free(f->node); free(f->best); free(f->slot);
	for (i = 0; i < 131; ++i) free(f->layer[i].v);
	memset(f, 0, sizeof *f);
}

/**
 * @brief book findline: the lines from the current position to a target position, through book positions only
 * (the current position, the target and every position between them in the book). A line only plays on squares
 * that hold a disc in the target, and a pass ("pa") when the side to move has no move and the other side has one.
 * The cost of a line is the sum of the changes of the book value seen by black from a position to the next, as the
 * reverse book path of the original program: the n lines of smallest cost are written, the smallest first. As
 * a next position has more discs (or the same discs after a pass), the positions are done by their number of discs:
 * the n smallest costs of a position are known before its next positions get theirs.
 */
static int ba_findline(BaJob *job, const BaParams *prm)
{
	const long long t = real_clock();
	const unsigned long long occupied = prm->target.player | prm->target.opponent;
	const int key_target = ba_find_key(&prm->target);
	BaWork w;
	BaFind f;
	Position *p;
	unsigned int idx, v, start;
	int key, k, count = 0, result = -1;

	if (!prm->target_set) {
		ba_print("@error id=%d word=target\n@end id=%d error=1\n", job->id, job->id);
		return 0;
	}
	if (prm->n_set && (prm->n < 1 || prm->n > BA_FIND_N_MAX)) {
		ba_print("@error id=%d word=n\n@end id=%d error=1\n", job->id, job->id);
		return 0;
	}
	memset(&w, 0, sizeof w); // (ba_probe only reads the book)
	w.job = job;
	w.book = job->book;
	w.prm = prm;
	if (ba_probe(&w, &prm->target, &idx) == NULL) {
		ba_print("@end id=%d count=0 paths=0 positions=0 notinbook=1 ms=%lld\n", job->id, real_clock() - t);
		return 0;
	}
	if ((p = ba_probe(&w, &job->board, &idx)) == NULL) {
		ba_print("@end id=%d count=0 paths=0 positions=0 startnotinbook=1 ms=%lld\n", job->id, real_clock() - t);
		return 0;
	}

	memset(&f, 0, sizeof f);
	f.n = prm->n_set ? (int) prm->n : 1;
	start = ba_find_add(&f, &job->board, job->player, job->player == BLACK ? p->score.value : -p->score.value);
	if (start == UINT_MAX) goto end;
	{
		const BaFBest b0 = {0, UINT_MAX, 0, NOMOVE};
		ba_find_keep(&f, start, &b0);
		f.node[start].paths = 1;
	}
	if (!ba_list_push(&f.layer[ba_find_key(&job->board)], start)) goto end;

	for (key = ba_find_key(&job->board); key <= key_target; ++key) {
		BaFProbe probe[BA_PROBE_BATCH + MAX_MOVE];
		long long j;
		int n = 0;
		for (j = 0; j < f.layer[key].n; ++j) {
			const unsigned int u = (unsigned int) f.layer[key].v[j];
			const Board board = f.node[u].board;
			unsigned long long moves = get_moves(board.player, board.opponent);
			bool pass = false;

			if (ba_cancelled(job)) goto end;
			if (moves == 0) pass = can_move(board.opponent, board.player);
			else moves &= occupied & ~(board.player | board.opponent);
			while (moves || pass) {
				BaFProbe *b = probe + n;
				if (pass) {
					b->x = PASS;
					pass = false;
					b->next.player = board.opponent;
					b->next.opponent = board.player;
					if (ba_find_key(&b->next) > key_target) continue;
				} else {
					b->x = (int) first_bit(moves);
					moves &= moves - 1;
					board_next(&board, b->x, &b->next);
				}
				b->u = u;
				board_unique(&b->next, &b->unique);
				b->bucket = board_get_hash_code(&b->unique) & (unsigned long long) (job->book->n - 1);
				PREFETCH(job->book->array + b->bucket);
				++n;
			}
			if (n >= BA_PROBE_BATCH) {
				if (!ba_find_batch(&f, job->book, probe, n)) goto end;
				n = 0;
			}
		}
		if (n && !ba_find_batch(&f, job->book, probe, n)) goto end; // (the next layer needs every cost of this one)
	}

	v = ba_find_get(&f, &prm->target, prm->target_color);
	if (v != UINT_MAX) {
		for (k = 0; k < f.node[v].n_best && !ba_cancelled(job); ++k) {
			char s[2 * 128 + 1];
			unsigned char chain[128];
			unsigned int a = v;
			int r = k, n = 0, i;
			while (f.best[(size_t) a * (size_t) f.n + (size_t) r].parent != UINT_MAX && n < 128) {
				const BaFBest *b = f.best + (size_t) a * (size_t) f.n + (size_t) r;
				chain[n++] = b->move;
				a = b->parent;
				r = b->rank;
			}
			for (i = 0; i < n; ++i) ba_square(chain[n - 1 - i], s + 2 * i);
			s[2 * n] = '\0';
			ba_print("@line id=%d moves=%s cost=%d\n", job->id, s, f.best[(size_t) v * (size_t) f.n + (size_t) k].cost);
			++count;
		}
	}
	if (!ba_cancelled(job)) {
		ba_print("@end id=%d count=%d paths=%llu positions=%u ms=%lld\n", job->id, count, v == UINT_MAX ? 0ULL : f.node[v].paths, f.n_node, real_clock() - t);
		result = 0;
	}
end:
	if (result < 0 && f.too_large && !ba_cancelled(job)) {
		ba_print("@error id=%d size=1\n@end id=%d error=1\n", job->id, job->id);
		result = 0;
	}
	ba_find_free(&f);
	return result;
}

/* ------------------------------------------------------------------ book reduce ------------------------------------------------------------- */

/*
 * book reduce x y keeps the positions that the walk of book deviate3 x y reaches from the current position (the root):
 * a position walked with its smallest accumulated loss L keeps the positions of its links whose loss e is
 * 0 <= e <= x with L + e <= y; such a position is walked in turn when its empties are at least those of the book depth
 * and the game is not over (as book_deviate_total_by_loss). The root is always kept. The other positions are removed,
 * and the links to them are removed as after book prune (the best of them may become the leaf of the position).
 * As book deviate3, book reduce first negamaxes the positions after the root (a book learnt from later positions may
 * not be negamaxed from this one); book reducecount, which does not change the book, walks with the values of its own
 * negamax (ba_negamax_values), so that both find the same positions. The values of the positions kept do not change
 * (the best move of a position kept is kept), and book deviate3 x y from the root then selects the same leaves as before.
 * The walk of book reduce only marks the positions (deviate_mark_only): it does not list them as book deviate3 does
 * for the positions to expand. book reducecount answers an error for a damaged book (see ba_red_group).
 */

/** Numbers of the positions reached by the walk of book reducecount (4 bytes each: see ba_position). */
typedef struct BaRList {
	unsigned int *v;
	long long n, size;
} BaRList;

static bool ba_rlist_push(BaRList *l, const unsigned int idx)
{
	if (l->n == l->size) {
		const long long size = l->size + l->size / 2 + 1024;
		unsigned int *v = (unsigned int*) realloc(l->v, (size_t) size * sizeof *v);
		if (v == NULL) return false;
		l->v = v; l->size = size;
	}
	l->v[l->n++] = idx;
	return true;
}

/** @brief Give back the memory of a list (it is empty then). */
static void ba_rlist_free(BaRList *l)
{
	free(l->v);
	l->v = NULL;
	l->n = l->size = 0;
}

/**
 * @brief The table of book-reduce-index: the bucket of every number multiple of 64 (see ba_position).
 * Made before the walks of book reducecount (the threads only read it), kept with the numbering.
 * @return false if out of memory (the binary search of all the buckets is used then).
 */
static bool ba_index_table(void)
{
	unsigned long long j;
	int b = 0;
	if (ba.index.table) return true;
	ba.index.n_table = (unsigned int) (ba.index.n >> BA_INDEX_SHIFT) + 1;
	ba.index.table = (unsigned int*) malloc((size_t) ba.index.n_table * sizeof *ba.index.table);
	if (ba.index.table == NULL) return false;
	for (j = 0; j < ba.index.n_table; ++j) {
		const unsigned long long idx = j << BA_INDEX_SHIFT;
		while (b + 1 < ba.index.n_bucket && ba.index.first[b + 1] <= idx) ++b;
		ba.index.table[j] = (unsigned int) b;
	}
	return true;
}

/** @return the position of a number (see BaIndex): in the last bucket whose first number is not above it. */
static Position* ba_position(const BaWork *w, const unsigned int idx)
{
	const unsigned int *first = ba.index.first;
	int lo = 0, hi = ba.index.n_bucket;
	if (ba.index.table && options.book_reduce_index) { // (between the buckets of the numbers multiple of 64 around it)
		const unsigned int j = idx >> BA_INDEX_SHIFT;
		lo = (int) ba.index.table[j];
		if (j + 1 < ba.index.n_table) hi = (int) ba.index.table[j + 1] + 1;
	}
	while (hi - lo > 1) {
		const int mid = lo + (hi - lo) / 2;
		if (first[mid] <= idx) lo = mid;
		else hi = mid;
	}
	return w->book->array[lo].positions + (idx - first[lo]);
}

/*
 * The group of a position: 2 * empties, + 1 if it must pass. A legal move leaves one empty less, and a pass goes from a
 * position that must pass to one that can move: the children of a position are always in an earlier (smaller) group.
 * A child that is not (a link to a square already taken, a pass to a position that can move as well, a cycle) is a
 * damaged book: book reducecount does not count it (see ba_reducecount).
 */
#define BA_RED_GROUPS 122       /**< 2 * (60 empties + 1) */
#define BA_DAMAGED (-3)         /**< the result of the counts for a damaged book */

/** @return the group of a position (see above). */
static inline int ba_red_group(const Position *p)
{
	return 2 * board_count_empties(&p->board) + (can_move(p->board.player, p->board.opponent) ? 0 : 1);
}

/** A position of the depth first walk of ba_negamax_values. */
typedef struct BaNItem {
	Position *p;
	unsigned int idx;
	int link;               /**< next link */
	int best;               /**< best value so far */
	int group;              /**< its group (see ba_red_group) */
} BaNItem;

/**
 * @brief The values of the negamax of the positions after a root, as book_negamax_position computes them: the best of
 * the leaf (when it has a score) and of the values of the children seen from the position (a link without child is
 * not counted), -SCORE_INF if none.
 * @param val Marks: value + 128 of each position computed (1 to 255), 0 for the others.
 * @return 0, -1 if stopped or out of memory, BA_DAMAGED if a child is not in an earlier group (see ba_red_group).
 */
static int ba_negamax_values(BaWork *w, Position *root, const unsigned int root_idx, BaVisit *val)
{
	BaNItem *stack = NULL;
	int n = 0, size = 0, result = -1;
	Position *next = root;      /* position to put on the stack */
	unsigned int next_idx = root_idx;
	bool ok = true;

	ba_visit_clear(val);
	while (ok) {
		BaNItem *top;
		if (next) {
			if (n == size) {
				BaNItem *s = (BaNItem*) realloc(stack, (size_t) (size + 64) * sizeof *s);
				if (s == NULL) { ok = false; break; }
				stack = s; size += 64;
			}
			stack[n].p = next;
			stack[n].idx = next_idx;
			stack[n].link = 0;
			stack[n].best = next->leaf.score > -SCORE_INF ? next->leaf.score : -SCORE_INF;
			stack[n].group = ba_red_group(next);
			++n;
			next = NULL;
		}
		if (ba_cancelled(w->job)) { ok = false; break; }
		top = stack + n - 1;
		while (top->link < top->p->n_link) {
			const Link *l = position_links(top->p) + top->link;
			Board target;
			unsigned int ci;
			Position *child;
			unsigned char cv;
			board_next(&top->p->board, l->move, &target);
			child = ba_probe(w, &target, &ci);
			if (child == NULL) { ++top->link; continue; } // (a link without child is not counted)
			if (ba_red_group(child) >= top->group) { result = BA_DAMAGED; ok = false; break; }
			cv = ba_visit_get(val, ci);
			if (cv == 0) { next = child; next_idx = ci; break; } // its value first (the link is read again then)
			if (128 - cv > top->best) top->best = 128 - cv;
			++top->link;
		}
		if (!ok) break;
		if (next) continue;
		if (!ba_visit_set(val, top->idx, (unsigned char) (top->best + 128))) ok = false;
		else if (--n == 0) { result = 0; break; }
	}
	free(stack);
	return result;
}

/**
 * @brief The positions that book reduce keeps from a root (marks: smallest accumulated loss + 1 of a walked position,
 * 1 for a position kept that is not walked), with the values of ba_negamax_values.
 * @return their number, -1 if stopped or out of memory.
 */
static long long ba_reduce_walk(BaWork *w, Position *root, const unsigned int root_idx, const int move_loss, const int total_loss, const BaVisit *val)
{
	const Book *book = w->book;
	BaRList *level = (BaRList*) calloc((size_t) total_loss + 1, sizeof *level);
	long long n = -1;
	int L;
	bool ok;

	if (level == NULL) return -1;
	ba_visit_clear(&w->visit);
	ok = ba_visit_set(&w->visit, root_idx, 1);
	if (ok && ba_walkable(book, root)) ok = ba_rlist_push(level, root_idx);
	for (L = 0; ok && L <= total_loss; ++L) {
		BaRList *cur = level + L;
		long long k;
		for (k = 0; ok && k < cur->n; ++k) { // (positions reached with the same loss are added while the list is read)
			const unsigned int idx = cur->v[k];
			Position *p;
			int v;
			Link *l;

			if (ba_cancelled(w->job)) { ok = false; break; }
			if (ba_visit_get(&w->visit, idx) != L + 1) continue; // reached later with a smaller loss
			p = ba_position(w, idx);
			v = ba_visit_get(val, idx) - 128;
			foreach_link(l, p) {
				Board target;
				unsigned int ci;
				Position *child;
				board_next(&p->board, l->move, &target);
				child = ba_probe(w, &target, &ci);
				if (child) {
					const int e = v + (ba_visit_get(val, ci) - 128); // (the score of the link is minus the value of the child)
					unsigned char old;
					if (e < 0 || e > move_loss || L + e > total_loss) continue;
					old = ba_visit_get(&w->visit, ci);
					if (!ba_walkable(book, child)) {
						if (!old && !ba_visit_set(&w->visit, ci, 1)) { ok = false; break; }
						continue;
					}
					if (old && old <= L + e + 1) continue;
					if (!ba_visit_set(&w->visit, ci, (unsigned char) (L + e + 1)) || !ba_rlist_push(level + L + e, ci)) { ok = false; break; }
				}
			}
		}
		ba_rlist_free(cur);
	}
	if (ok) n = (long long) w->visit.n;
	for (L = 0; L <= total_loss; ++L) free(level[L].v);
	free(level);
	return n;
}

/*
 * book reducecount with n-tasks threads (ba_reduce_threads), with the results of the walk above: the positions after the
 * root are grouped by empties, and for the same empties a position that must pass comes after the one it passes to, so
 * that the children of a position are always in an earlier group. The values (as ba_negamax_values) are computed group
 * after group, the positions of a group by the threads. The walk goes by increasing accumulated loss, the positions of
 * the same loss by the threads, in rounds (the positions reached with the same loss are walked in the next round). The
 * marks are one byte per position, changed by compare and swap: a mark only goes down to the smallest loss, so the
 * positions kept and their losses are those of the walk with one thread. A child that is not in an earlier group (a
 * damaged book) is found before its group is used.
 */

#ifndef BA_RED_FEW
#define BA_RED_FEW 4096         /**< fewer positions are done by one thread (the tests build with 1: threads for all) */
#endif

/**
 * @brief The children of a position, in the order of its links: their positions (NULL: not in the book) and numbers.
 * Their buckets are asked for first (prefetch), as in ba_reach_batch.
 * @return the number of links.
 */
static int ba_children(const BaWork *w, const Position *p, Position **child, unsigned int *ci)
{
	const Book *book = w->book;
	BaProbe probe[256];
	const Link *l;
	int n = 0, i, j;

	foreach_link(l, p) {
		Board target;
		board_next(&p->board, l->move, &target);
		board_unique(&target, &probe[n].unique);
		probe[n].bucket = board_get_hash_code(&probe[n].unique) & (unsigned long long) (book->n - 1);
		PREFETCH(book->array + probe[n].bucket);
		++n;
	}
	for (i = 0; i < n; ++i) {
		const PositionArray *a = book->array + probe[i].bucket;
		const char *s = (const char*) a->positions, *e = s + (size_t) a->n * sizeof (Position);
		for (s -= (uintptr_t) s & 63; s < e; s += 64) PREFETCH(s);
	}
	for (i = 0; i < n; ++i) {
		const PositionArray *a = book->array + probe[i].bucket;
		child[i] = NULL;
		for (j = 0; j < a->n; ++j) {
			if (board_equal(&a->positions[j].board, &probe[i].unique)) {
				child[i] = a->positions + j;
				ci[i] = ba.index.first[probe[i].bucket] + (unsigned int) j;
				break;
			}
		}
	}
	return n;
}

/** A thread of ba_reduce_threads. */
typedef struct BaRedThread {
	BaWork *w;
	const unsigned int *cur;        /**< numbers of the positions to do (from first to last) */
	long long first, last;
	unsigned char *mark;            /**< seen (the positions after the root), then smallest loss + 1 (the walk) */
	unsigned char *val;             /**< value + 128 */
	BaRList *out;                   /**< the positions found by group, or reached by loss */
	BaRList next;                   /**< the positions found (the positions after the root) */
	int L, move_loss, total_loss;   /**< the walk */
	long long n_new;                /**< positions marked by the walk that were not */
	bool oom, broken;               /**< no memory; a child not in an earlier group (a damaged book) */
} BaRedThread;

static void* ba_red_reach_run(void *v)
{
	BaRedThread *t = (BaRedThread*) v;
	Position *child[256];
	unsigned int ci[256];
	long long k;

	for (k = t->first; k < t->last && !ba_cancelled(t->w->job) && !t->broken; ++k) {
		const Position *p = ba_position(t->w, t->cur[k]);
		const int n = ba_children(t->w, p, child, ci), g = ba_red_group(p);
		int i;
		for (i = 0; i < n; ++i) {
			if (child[i] && ba_red_group(child[i]) >= g) { t->broken = true; break; } // (its group is not used)
			if (child[i] && atomic_load_uchar(t->mark + ci[i]) == 0 && atomic_cas_uchar(t->mark + ci[i], 0, 1)) {
				if (!ba_rlist_push(t->out + ba_red_group(child[i]), ci[i]) || !ba_rlist_push(&t->next, ci[i])) t->oom = true;
			}
		}
	}
	return NULL;
}

static void* ba_red_negamax_run(void *v)
{
	BaRedThread *t = (BaRedThread*) v;
	Position *child[256];
	unsigned int ci[256];
	long long k;

	for (k = t->first; k < t->last && !ba_cancelled(t->w->job); ++k) {
		const unsigned int idx = t->cur[k];
		const Position *p = ba_position(t->w, idx);
		const int n = ba_children(t->w, p, child, ci);
		int best = p->leaf.score > -SCORE_INF ? p->leaf.score : -SCORE_INF, i;
		for (i = 0; i < n; ++i) {
			if (child[i]) { // (a link without child is not counted)
				const int cv = t->val[ci[i]];
				if (cv == 0) t->broken = true;
				else if (128 - cv > best) best = 128 - cv;
			}
		}
		t->val[idx] = (unsigned char) (best + 128);
	}
	return NULL;
}

static void* ba_red_walk_run(void *v)
{
	BaRedThread *t = (BaRedThread*) v;
	const Book *book = t->w->book;
	Position *child[256];
	unsigned int ci[256];
	long long k;

	for (k = t->first; k < t->last && !ba_cancelled(t->w->job); ++k) {
		const unsigned int idx = t->cur[k];
		int vp, n, i;
		if (atomic_load_uchar(t->mark + idx) != t->L + 1) continue; // reached later with a smaller loss
		vp = t->val[idx] - 128;
		n = ba_children(t->w, ba_position(t->w, idx), child, ci);
		for (i = 0; i < n; ++i) {
			int e;
			unsigned char old, mark;
			if (child[i] == NULL) continue;
			e = vp + (t->val[ci[i]] - 128); // (the score of the link is minus the value of the child)
			if (e < 0 || e > t->move_loss || t->L + e > t->total_loss) continue;
			if (!ba_walkable(book, child[i])) {
				if (atomic_load_uchar(t->mark + ci[i]) == 0 && atomic_cas_uchar(t->mark + ci[i], 0, 1)) ++t->n_new;
				continue;
			}
			mark = (unsigned char) (t->L + e + 1);
			for (;;) {
				old = atomic_load_uchar(t->mark + ci[i]);
				if (old && old <= mark) break;
				if (atomic_cas_uchar(t->mark + ci[i], old, mark)) {
					if (old == 0) ++t->n_new;
					if (!ba_rlist_push(t->out + t->L + e, ci[i])) t->oom = true;
					break;
				}
			}
		}
	}
	return NULL;
}

/** @brief Run the threads on the positions of a list from first to its end (one thread for a few positions). */
static void ba_red_run(BaRedThread *t, const int n_thread, const BaRList *list, const long long first, void* (*run)(void*))
{
	const long long n = list->n - first;
	const int m = n < BA_RED_FEW ? 1 : n_thread;
	int i;
	for (i = 0; i < m; ++i) {
		t[i].cur = list->v;
		t[i].first = first + n * i / m;
		t[i].last = first + n * (i + 1) / m;
	}
	thread_run_workers(run, t, sizeof *t, m, true, false);
}

/**
 * @brief Append a list to another and empty it: to an empty list, the memory of the list given goes with its numbers
 * (no copy), and the list given keeps the memory of the other one. @return false if out of memory.
 */
static bool ba_rlist_move(BaRList *to, BaRList *from)
{
	if (from->n == 0) return true;
	if (to->n == 0) {
		const BaRList empty = *to;
		*to = *from;
		*from = empty;
		return true;
	}
	if (to->n + from->n > to->size) {
		const long long size = to->n + from->n + to->size / 2 + 1024;
		unsigned int *v = (unsigned int*) realloc(to->v, (size_t) size * sizeof *v);
		if (v == NULL) return false;
		to->v = v; to->size = size;
	}
	memcpy(to->v + to->n, from->v, (size_t) from->n * sizeof *from->v);
	to->n += from->n;
	from->n = 0;
	return true;
}

/** @brief Give back the memory of a list beyond its numbers (a list that does not grow any more). */
static void ba_rlist_trim(BaRList *l)
{
	if (l->n == 0) {
		ba_rlist_free(l);
	} else if (l->size > l->n + l->n / 8 + 1024) {
		unsigned int *v = (unsigned int*) realloc(l->v, (size_t) l->n * sizeof *v);
		if (v) { l->v = v; l->size = l->n; }
	}
}

/**
 * The walk of book-reduce-walk group: the groups from the root's down (the children of a position are in later groups
 * of this order), so that the smallest accumulated loss of a position is known when its group comes (its parents were
 * walked before): each walked position of the group, with that loss, marks its children as the walk by loss does.
 */
static void* ba_red_gwalk_run(void *v)
{
	BaRedThread *t = (BaRedThread*) v;
	const Book *book = t->w->book;
	Position *child[256];
	unsigned int ci[256];
	long long k;

	for (k = t->first; k < t->last && !ba_cancelled(t->w->job); ++k) {
		const unsigned int idx = t->cur[k];
		const unsigned char m = atomic_load_uchar(t->mark + idx);
		const Position *p;
		int vp, n, i, L;
		if (m == 0) continue; // not reached by the walk
		p = ba_position(t->w, idx);
		if (!ba_walkable(book, p)) continue; // (kept, not walked)
		L = m - 1;
		vp = t->val[idx] - 128;
		n = ba_children(t->w, p, child, ci);
		for (i = 0; i < n; ++i) {
			int e;
			unsigned char old, mark;
			if (child[i] == NULL) continue;
			e = vp + (t->val[ci[i]] - 128); // (the score of the link is minus the value of the child)
			if (e < 0 || e > t->move_loss || L + e > t->total_loss) continue;
			if (!ba_walkable(book, child[i])) {
				if (atomic_load_uchar(t->mark + ci[i]) == 0 && atomic_cas_uchar(t->mark + ci[i], 0, 1)) ++t->n_new;
				continue;
			}
			mark = (unsigned char) (L + e + 1);
			for (;;) {
				old = atomic_load_uchar(t->mark + ci[i]);
				if (old && old <= mark) break;
				if (atomic_cas_uchar(t->mark + ci[i], old, mark)) {
					if (old == 0) ++t->n_new;
					break;
				}
			}
		}
	}
	return NULL;
}

/**
 * @brief The positions that book reduce keeps from a root, with n-tasks threads (see above).
 * @param kept_out, val_out If not NULL: the marks of the positions kept (not 0) and the values + 128 of the positions
 * after the root, when they are counted (see ba_counted); freed otherwise.
 * @return their number, -1 if stopped, -2 if not done this way (out of memory, a root of more than 60 empties):
 * ba_reduce_walk then, BA_DAMAGED for a damaged book (see ba_red_group).
 */
static long long ba_reduce_threads(BaWork *w, Position *root, const unsigned int root_idx, const int move_loss, const int total_loss, const int n_thread, BaVisit *kept_out, BaVisit *val_out)
{
	const Book *book = w->book;
	const unsigned long long n_book = ba.index.n;
	const int n_out = total_loss + 1 > BA_RED_GROUPS ? total_loss + 1 : BA_RED_GROUPS;
	const bool by_group = options.book_reduce_walk == 1; // (book-reduce-walk group: the lists of the groups are kept for the walk)
	BaRedThread t[MAX_THREADS];
	BaRList group[BA_RED_GROUPS], front = {NULL, 0, 0}, *level;
	unsigned char *mark = NULL, *val = NULL;
	long long kept = 1, result = -2;
	bool ok = true, broken = false;
	int i, g, L;

	if (ba_red_group(root) >= BA_RED_GROUPS || !ba_dense_reserve(2 * n_book)) return -2;
	memset(t, 0, sizeof t);
	memset(group, 0, sizeof group);
	mark = (unsigned char*) calloc((size_t) n_book + 1, 1);
	val = (unsigned char*) calloc((size_t) n_book + 1, 1);
	level = (BaRList*) calloc((size_t) total_loss + 1, sizeof *level);
	if (mark == NULL || val == NULL || level == NULL) ok = false;
	for (i = 0; ok && i < n_thread; ++i) {
		t[i].w = w; t[i].mark = mark; t[i].val = val;
		t[i].move_loss = move_loss; t[i].total_loss = total_loss;
		t[i].out = (BaRList*) calloc((size_t) n_out, sizeof (BaRList));
		if (t[i].out == NULL) ok = false;
	}

	// the positions after the root, by group
	if (ok) {
		mark[root_idx] = 1;
		ok = ba_rlist_push(group + ba_red_group(root), root_idx) && ba_rlist_push(&front, root_idx);
	}
	while (ok && front.n > 0 && !ba_cancelled(w->job)) {
		ba_red_run(t, n_thread, &front, 0, ba_red_reach_run);
		front.n = 0;
		for (i = 0; ok && i < n_thread; ++i) {
			if (t[i].broken) broken = true;
			if (t[i].oom || broken) ok = false;
			for (g = 0; ok && g < BA_RED_GROUPS; ++g) ok = ba_rlist_move(group + g, t[i].out + g);
			if (ok) ok = ba_rlist_move(&front, &t[i].next);
		}
	}
	free(front.v);
	for (i = 0; i < n_thread; ++i) { // (the lists of the threads hold no position now: their memory is given back)
		if (t[i].out) for (g = 0; g < n_out; ++g) ba_rlist_free(t[i].out + g);
		ba_rlist_free(&t[i].next);
	}
	for (g = 0; g < BA_RED_GROUPS; ++g) ba_rlist_trim(group + g); // (the lists of the groups do not grow any more)

	// the values, group after group
	for (g = 0; ok && g < BA_RED_GROUPS && !ba_cancelled(w->job); ++g) {
		if (group[g].n) ba_red_run(t, n_thread, group + g, 0, ba_red_negamax_run);
		for (i = 0; i < n_thread; ++i) if (t[i].broken) broken = true;
		if (broken) ok = false;
		if (!by_group) ba_rlist_free(group + g);
	}

	// the walk
	if (ok && !ba_cancelled(w->job)) {
		memset(mark, 0, (size_t) n_book + 1);
		mark[root_idx] = 1;
		if (!by_group && ba_walkable(book, root)) ok = ba_rlist_push(level, root_idx);
	}
	for (g = BA_RED_GROUPS - 1; by_group && ok && g >= 0 && !ba_cancelled(w->job); --g) { // in the order of the groups
		if (group[g].n) ba_red_run(t, n_thread, group + g, 0, ba_red_gwalk_run);
		for (i = 0; i < n_thread; ++i) { kept += t[i].n_new; t[i].n_new = 0; }
		ba_rlist_free(group + g);
	}
	for (L = 0; !by_group && ok && L <= total_loss && !ba_cancelled(w->job); ++L) { // by increasing loss
		long long done = 0;
		while (ok && done < level[L].n && !ba_cancelled(w->job)) {
			const long long end = level[L].n;
			for (i = 0; i < n_thread; ++i) t[i].L = L;
			ba_red_run(t, n_thread, level + L, done, ba_red_walk_run);
			done = end;
			for (i = 0; ok && i < n_thread; ++i) {
				int l;
				if (t[i].oom) ok = false;
				kept += t[i].n_new; t[i].n_new = 0;
				for (l = L; ok && l <= total_loss; ++l) ok = ba_rlist_move(level + l, t[i].out + l);
			}
		}
		ba_rlist_free(level + L);
	}

	if (ba_cancelled(w->job)) result = -1;
	else if (broken) result = BA_DAMAGED;
	else if (ok) result = kept;
	for (g = 0; g < BA_RED_GROUPS; ++g) free(group[g].v);
	if (level) for (L = 0; L <= total_loss; ++L) free(level[L].v);
	free(level);
	for (i = 0; i < n_thread; ++i) {
		if (t[i].out) for (g = 0; g < n_out; ++g) free(t[i].out[g].v);
		free(t[i].out);
		free(t[i].next.v);
	}
	if (result >= 0 && kept_out && val_out) { // (the memory reserved goes with them: ba_visit_free gives it back)
		ba_visit_init(kept_out, n_book);
		kept_out->dense = mark;
		kept_out->n = (unsigned long long) kept;
		ba_visit_init(val_out, n_book);
		val_out->dense = val;
	} else {
		free(mark);
		free(val);
		ba_dense_release(2 * n_book);
	}
	return result;
}

/** @return true if a root is deeper than the book depth (book reduce, as book subtree, does not change the book then). */
static inline bool ba_reduce_deep(const Book *book, const Position *root)
{
	return board_count_empties(&root->board) < book->options.n_empties - 1;
}

/*
 * The positions found by the last book reducecount (book-reduce-reuse): book reduce from the same root with the same
 * loss limits uses them (book_reduce_counted) while the book does not change: any command that may change the book
 * stops the analyses first (book_analysis_quiesce), which forgets them; book reduce itself keeps them for its use
 * (book_analysis_quiesce_reduce). They are also forgotten by the next book reducecount and after book reduce, and when
 * a walk of another analysis needs the memory reserved for their marks (ba_counted_give_locked; book reduce then
 * walks again, with the same result: up to v4.5.5-nikque.26 they kept it until the book changed, and the walks of
 * the other analyses had their marks in hash tables, of about 16 bytes a mark).
 * Only the analysis thread (while counting, or giving their memory) or the main thread (the analyses stopped) uses them.
 */
static struct {
	bool valid;
	Board root;                 /**< the board of the root, as in the book */
	int x, y;
	int n_bucket;               /**< buckets of the book then */
	unsigned long long n;       /**< positions of the book then */
	BaVisit kept;               /**< marks of the positions kept (not 0) */
	BaVisit val;                /**< value + 128 of the positions after the root */
} ba_counted;

/** @brief Forget the positions counted by book reducecount (the lock of the analyses is not held). */
static void ba_counted_free(void)
{
	ba_visit_free(&ba_counted.kept);
	ba_visit_free(&ba_counted.val);
	ba_counted.valid = false;
}

/**
 * @brief Forget the positions counted by book reducecount if their marks hold memory reserved for dense marks, which
 * a walk of another analysis needs (see ba_dense_reserve, which holds the lock of the analyses: the threads of that
 * analysis, not the count, ask for it; the main thread does not use the count while an analysis runs).
 * @return true if memory was given back.
 */
static bool ba_counted_give_locked(void)
{
	long long n = 0;

	if (!ba_counted.valid) return false;
	if (ba_counted.kept.dense) n += (long long) ba_counted.kept.n_index;
	if (ba_counted.val.dense) n += (long long) ba_counted.val.n_index;
	if (n == 0) return false; // (marks in hash tables: no memory reserved)
	free(ba_counted.kept.slot); free(ba_counted.kept.dense);
	free(ba_counted.val.slot); free(ba_counted.val.dense);
	memset(&ba_counted.kept, 0, sizeof ba_counted.kept);
	memset(&ba_counted.val, 0, sizeof ba_counted.val);
	ba_counted.valid = false;
	ba.dense_bytes -= n;
	return true;
}

static int ba_reducecount(BaJob *job, const BaParams *prm)
{
	const long long t = real_clock();
	const unsigned long long n_book = ba.index.n;
	const int n_thread = ba_n_thread();
	BaWork w;
	BaVisit val, kept_marks;
	unsigned int idx;
	Position *root;
	long long kept = -1;
	bool marks = false;         // (kept_marks and val hold the positions counted)

	ba_counted_free(); // (the memory of the last count first)
	ba_work_init(&w, job, prm);
	root = ba_probe(&w, &job->board, &idx);
	if (root == NULL || ba_reduce_deep(job->book, root)) {
		ba_work_free(&w);
		ba_print("@end id=%d kept=%llu removed=0 positions=%llu %s=1 ms=%lld\n", job->id, n_book, n_book, root ? "deep" : "notinbook", real_clock() - t);
		return 0;
	}
	if (n_thread > 1) {
		kept = ba_reduce_threads(&w, root, idx, prm->x, prm->y, n_thread, &kept_marks, &val);
		marks = kept >= 0;
	}
	if (n_thread <= 1 || kept == -2) { // (one thread, or not enough memory for the marks)
		int r;
		ba_visit_init(&val, n_book);
		r = ba_negamax_values(&w, root, idx, &val);
		kept = r < 0 ? r : ba_reduce_walk(&w, root, idx, prm->x, prm->y, &val);
		if (kept >= 0) { // (the marks of the walk)
			kept_marks = w.visit;
			memset(&w.visit, 0, sizeof w.visit);
			marks = true;
		} else ba_visit_free(&val);
	}
	ba_work_free(&w);
	if (marks && options.book_reduce_reuse && !ba_cancelled(job)) {
		ba_counted.root = root->board;
		ba_counted.x = prm->x;
		ba_counted.y = prm->y;
		ba_counted.n_bucket = job->book->n;
		ba_counted.n = n_book;
		ba_counted.kept = kept_marks;
		ba_counted.val = val;
		ba_counted.valid = true;
	} else if (marks) {
		ba_visit_free(&kept_marks);
		ba_visit_free(&val);
	}
	if (kept == BA_DAMAGED && !ba_cancelled(job)) { // (the positions after the root do not make a book that can be counted)
		ba_print("@error id=%d damaged=1\n@end id=%d error=1\n", job->id, job->id);
		return 0;
	}
	if (kept < 0) return -1;
	ba_print("@end id=%d kept=%lld removed=%lld positions=%llu ms=%lld\n", job->id, kept, (long long) n_book - kept, n_book, real_clock() - t);
	return 0;
}

static int ba_reduce_x, ba_reduce_y; /**< loss limits of book reduce (read by the tasks of book_reduce_edge_range) */

/** @brief Mark the positions that the walked positions of a range of buckets keep without walking them (book reduce). */
static void book_reduce_edge_range(BookTask *task)
{
	Book *book = task->book;
	int b, k;

	for (b = task->first; b < task->last; ++b) {
		PositionArray *a = book->array + b;
		for (k = 0; k < a->n; ++k) {
			Position *p = a->positions + k;
			const int v = book->visit[book->visit_first[b] + (unsigned int) k];
			Link *l;
			if (v == 0) continue; // not walked
			foreach_link(l, p) {
				const int e = p->score.value - l->score;
				if (0 <= e && e <= ba_reduce_x && v - 1 + e <= ba_reduce_y) {
					Board target;
					Position *child;
					board_next(&p->board, l->move, &target);
					if (board_count_empties(&target) >= book->options.n_empties && !board_is_game_over(&target)) continue; // (walked: marked by the walk)
					child = book_probe(book, &target);
					if (child) { // (another task may mark it too)
						unsigned char s;
						do {
							s = atomic_load_uchar(&child->state);
							if (position_state_is(s, book, POSITION_TODO)) break;
						} while (!atomic_cas_uchar(&child->state, s, position_state_set(s, book->epoch, POSITION_TODO)));
					}
				}
			}
		}
	}
}

/**
 * @brief book reduce with the positions that book reducecount found just before (book-reduce-reuse, see ba_counted).
 *
 * The positions it counted are marked "todo" (they are those that the walk below keeps). The negamax of book reduce
 * is not done: of what it changes, only the scores of the links to the positions removed stay after book reduce (one
 * of them may become the leaf, see position_remove_links; the rest is computed again by the negamax that follows book
 * reduce), and they are set as it sets them, from the values that book reducecount computed in the same way.
 *
 * @return false if the count is not the one of this book reduce, or a value is missing (nothing that stays is changed).
 */
static bool book_reduce_counted(Book *book, const Position *root, const int move_loss, const int total_loss)
{
	PositionArray *a;
	unsigned int idx = 0;
	int b, k;
	bool ok;

	if (!ba_counted.valid || ba_counted.x != move_loss || ba_counted.y != total_loss || ba_counted.n_bucket != book->n
	 || ba_counted.n != book->n_nodes || !board_equal(&ba_counted.root, &root->board)) return false;
	lock(&ba);
	ok = ba_index_get(book) && ba.index.n == ba_counted.n; // (the numbering of the count: the book is the same)
	unlock(&ba);
	if (!ok) return false;
	book_clean(book);
	for (b = 0; b < book->n; ++b) {
		a = book->array + b;
		for (k = 0; k < a->n; ++k, ++idx) {
			Position *p = a->positions + k;
			Link *l;
			if (ba_visit_get(&ba_counted.kept, idx) == 0) continue;
			position_set_todo(p, book);
			foreach_link(l, p) {
				Board target;
				const Position *child;
				board_next(&p->board, l->move, &target);
				child = book_probe(book, &target);
				if (child) {
					const int c = (int) (board_get_hash_code(&child->board) & (unsigned long long) (book->n - 1));
					const unsigned int ci = ba.index.first[c] + (unsigned int) (child - book->array[c].positions);
					const int cv = ba_visit_get(&ba_counted.val, ci);
					if (cv == 0) { book_clean(book); return false; } // (not computed: the walk below does it all)
					if (ba_visit_get(&ba_counted.kept, ci) == 0 && l->score != 128 - cv) { // (the score of the link is minus the value of the child)
						l->score = (signed char) (128 - cv);
						book->need_saving = true;
					}
				}
			}
		}
	}
	return true;
}

/**
 * @brief Keep only the positions that the walk of book deviate3 x y reaches from a position (book reduce).
 *
 * The walk is the one of book deviate3 (with n-tasks threads), after a negamax from the position (as book deviate3);
 * see the notes above ba_reduce_walk, which counts the same positions (book reducecount).
 *
 * @param book opening book.
 * @param board Position to start from.
 * @param move_loss Maximum loss for one move (0 to 129).
 * @param total_loss Maximum cumulative loss for both players (0 to VISIT_LOSS_MAX).
 * @return false if the book is not changed (the position is not in the book or deeper than the book depth, no memory).
 */
bool book_reduce(Book *book, const Board *board, const int move_loss, const int total_loss)
{
	Position *root = book_probe(book, board);
	unsigned int n_nodes;

	if (root == NULL) {
		warn("book reduce: this position is not in the book; the book is not changed\n");
		ba_counted_free();
		return false;
	}
	if (ba_reduce_deep(book, root)) {
		warn("book reduce: this position is deeper than the book depth; the book is not changed\n");
		ba_counted_free();
		return false;
	}
	n_nodes = book->n_nodes;
	if (options.book_reduce_reuse && book_reduce_counted(book, root, move_loss, total_loss)) {
		bprint("Book reduce %d %d: the positions counted by book reducecount...\r", move_loss, total_loss);
	} else {
		book_clean(book);
		book_negamax_position(root, book);
		book_clean(book);
		if (!book_visit_init(book)) { ba_counted_free(); return false; }
		bprint("Book reduce %d %d: walk...\r", move_loss, total_loss);
		deviate_walk_all = true; // (every position walked is marked "todo")
		deviate_mark_only = true; // (only marked: no list of them)
		if (!book_deviate_total_by_loss(book, root, 3, move_loss, total_loss)) {
			book_clean(book);
			book_visit_clear(book);
			position_deviate_total(root, book, move_loss, total_loss, 0, false);
		}
		deviate_walk_all = deviate_mark_only = false;
		ba_reduce_x = move_loss;
		ba_reduce_y = total_loss;
		{
			BookTask task[MAX_THREADS];
			book_parallel(book, book_reduce_edge_range, task, NULL);
		}
		book_visit_free(book);
	}
	position_set_todo(root, book);
	ba_counted_free(); // (the book changes now: the numbering that book_reduce_counted may have made goes too)
	if (ba.init) {
		lock(&ba);
		ba_index_free();
		unlock(&ba);
	}

	bprint("Book reduce %d %d: remove...\r", move_loss, total_loss);
	book_remove_unmarked(book);
	free(book->todo_list.item); // (the places of the positions have changed: the memory of the list of an earlier command goes)
	book->todo_list.item = NULL;
	book->todo_list.size = 0;
	book_clean(book);
	// the book that was cut is saved when Edax ends (as book prune)
	if (book_remove_missing_links(book) || book->n_nodes != n_nodes) book->need_saving = true;
	if (!(options.book_reduce_shrink && book_shrink_buckets(book))) book_pool_compact(book);
	printf("Book reduce %d %d: %u positions kept, %u removed\n", move_loss, total_loss, book->n_nodes, n_nodes - book->n_nodes);
	return true;
}

/* ------------------------------------------------------------------ thread -------------------------------------------------------------------- */

/** @brief Free an analysis. */
static void ba_job_free(BaJob *job)
{
	free(job->param);
	free(job->line);
	free(job);
}

static void ba_run(BaJob *job)
{
	BaParams prm;
	bool ok;
	int r = 0;

	if (!ba_params_read(job->param, &prm)) {
		ba_print("@error id=%d word=%s\n@end id=%d error=1\n", job->id, prm.error, job->id);
		return;
	}
	lock(&ba);
	ok = ba_index_get(job->book);
	if (ok && job->kind == BA_REDUCECOUNT && options.book_reduce_index) ba_index_table(); // (without memory: the binary search)
	unlock(&ba);
	if (!ok) {
		ba_print("@error id=%d memory=1\n@end id=%d error=1\n", job->id, job->id);
		return;
	}
	if (job->kind == BA_MOVEINFO) r = ba_moveinfo(job, &prm);
	else if (job->kind == BA_MISSING) r = ba_missing(job, &prm);
	else if (job->kind == BA_BESTLINES) r = ba_bestlines(job, &prm);
	else if (job->kind == BA_FINDLINE) r = ba_findline(job, &prm);
	else if (job->kind == BA_REDUCECOUNT) r = ba_reducecount(job, &prm);
	else r = ba_levels(job, &prm);
	if (r < 0) {
		if (ba_cancelled(job)) ba_print("@cancel id=%d\n", job->id);
		else ba_print("@error id=%d memory=1\n@end id=%d error=1\n", job->id, job->id);
	}
}

static void* ba_thread(void *v)
{
	(void) v;
	lock(&ba);
	for (;;) {
		BaJob *job;
		while (!ba.quit && ba.head == NULL) condition_wait(&ba);
		if (ba.quit) break;
		job = ba.head;
		ba.head = job->next;
		if (ba.head == NULL) ba.tail = NULL;
		ba.current = job;
		unlock(&ba);
		ba_run(job);
		lock(&ba);
		ba.current = NULL;
		ba_job_free(job);
		condition_broadcast(&ba);
	}
	unlock(&ba);
	return NULL;
}

/**
 * @brief Read a command "book moveinfo|missing|levels|bestlines|findline|reducecount ..." and give it to the analysis thread.
 *
 * @param book Opening book.
 * @param param Words after "book".
 * @param board Current position.
 * @param player Side to move.
 * @param line Moves of the game up to the current position (book bestlines writes them first), or NULL.
 * @return false if the command is another book command.
 */
bool book_analysis_command(Book *book, const char *param, const Board *board, const int player, const char *line)
{
	char word[16];
	const char *s = param;
	int i, kind, id;
	BaJob *job;

	while (*s == ' ' || *s == '\t') ++s;
	for (i = 0; *s && *s != ' ' && *s != '\t' && i < (int) sizeof word - 1; ++s) word[i++] = (char) (('A' <= *s && *s <= 'Z') ? *s + 32 : *s);
	word[i] = '\0';
	if (*s && *s != ' ' && *s != '\t') return false;
	if (strcmp(word, "moveinfo") == 0) kind = BA_MOVEINFO;
	else if (strcmp(word, "missing") == 0) kind = BA_MISSING;
	else if (strcmp(word, "levels") == 0) kind = BA_LEVELS;
	else if (strcmp(word, "bestlines") == 0) kind = BA_BESTLINES;
	else if (strcmp(word, "findline") == 0) kind = BA_FINDLINE;
	else if (strcmp(word, "reducecount") == 0) kind = BA_REDUCECOUNT;
	else return false;

	book_analysis_init();
	if (ba_read_id(s, &id) < 0) { // (no number to answer with)
		ba_print("@error word=id\n");
		return true;
	}
	job = (BaJob*) calloc(1, sizeof *job);
	if (job == NULL || (job->param = string_duplicate(s)) == NULL) {
		free(job);
		ba_print("@error id=%d memory=1\n@end id=%d error=1\n", id, id); // (as an analysis without memory)
		return true;
	}
	job->id = id;
	job->kind = kind;
	job->board = *board;
	job->player = player;
	job->book = book;
	if (line && (job->line = string_duplicate(line)) == NULL) {
		ba_job_free(job);
		ba_print("@error id=%d memory=1\n@end id=%d error=1\n", id, id);
		return true;
	}

	lock(&ba);
	if (!ba.started) {
		ba.quit = false;
		ba.started = thread_create(&ba.thread, ba_thread, NULL);
	}
	if (!ba.started) {
		ba.current = job;
		unlock(&ba);
		ba_run(job); // (without a thread: at once)
		lock(&ba);
		ba.current = NULL;
		unlock(&ba);
		ba_job_free(job);
		return true;
	}
	if (ba.tail) ba.tail->next = job; else ba.head = job;
	ba.tail = job;
	condition_broadcast(&ba);
	unlock(&ba);
	return true;
}

/** @brief Stop the analyses waiting (lock held): each one answers "@cancel". */
static void ba_cancel_waiting(const bool all, const int id, bool *found)
{
	BaJob **j = &ba.head, *job;
	ba.tail = NULL;
	while ((job = *j) != NULL) {
		if (all || job->id == id) {
			*j = job->next;
			ba_print("@cancel id=%d\n", job->id);
			*found = true;
			ba_job_free(job);
		} else {
			ba.tail = job;
			j = &job->next;
		}
	}
}

/**
 * @brief Stop analyses ("stop" read by the input thread).
 * @param param Words after "stop": "id=<n>" stops one analysis, nothing stops them all.
 * @return true if the words named an analysis ("stop" then has no other effect).
 */
bool book_analysis_stop(const char *param)
{
	bool found = false;
	int id;
	const int read = ba_read_id(param, &id);
	const bool has_id = read != 0;

	if (read < 0) { // a wrong number stops nothing
		ba_print("@error word=id\n");
		return true;
	}
	// (export=1: the engine has book deviate ... export; a user interface asks it at the start, before any analysis)
	if (!ba.init) {
		if (has_id) ba_print("@cancel id=%d state=none export=1\n", id);
		return has_id;
	}
	lock(&ba);
	if (ba.current && (!has_id || ba.current->id == id)) {
		ba_cancel(ba.current); // (the analysis answers "@cancel" when it stops)
		found = true;
	}
	ba_cancel_waiting(!has_id, id, &found);
	unlock(&ba);
	if (has_id && !found) ba_print("@cancel id=%d state=none export=1\n", id);
	return has_id;
}

/** @brief Stop all the analyses and wait for the end of the current one; keep_counted: see ba_counted. */
static void ba_quiesce(const bool keep_counted)
{
	bool found = false;
	if (!ba.init) return;
	lock(&ba);
	ba_cancel_waiting(true, 0, &found);
	if (ba.current) ba_cancel(ba.current);
	while (ba.current) condition_wait(&ba);
	ba_index_free();
	unlock(&ba);
	if (!keep_counted) ba_counted_free();
}

/** @brief Stop all the analyses and wait for the end of the current one (before a command that may change the book). */
void book_analysis_quiesce(void)
{
	ba_quiesce(false);
}

/** @brief As book_analysis_quiesce, before book reduce: the positions counted by book reducecount are kept for it. */
void book_analysis_quiesce_reduce(void)
{
	ba_quiesce(true);
}

/** @brief End the analysis thread. */
void book_analysis_end(void)
{
	if (!ba.init) return;
	book_analysis_quiesce();
	lock(&ba);
	ba.quit = true;
	condition_broadcast(&ba);
	unlock(&ba);
	if (ba.started) thread_join(ba.thread);
	ba.started = false;
}
