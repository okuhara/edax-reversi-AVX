/**
 * @file libedax_test.c
 *
 * @brief Test of the Edax api (libedax).
 *
 * The program calls every function of the api and prints what it gets; a wrong structure
 * layout or a wrong answer which does not depend on the engine makes it fail (exit code 1).
 * The values which depend on the engine (scores, searched moves) are only printed.
 *
 * It is linked with the import library of the dll, and run from a directory containing
 * data/eval.dat (see build-libedax-test.cmd). Pass --original after the library name to test the original libedax
 * (copied under the name of the dll), to compare the two libraries.
 *
 * @date 2026
 * @author Nikque
 */

#include "../src/libedax.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
	#include <windows.h>
#else
	#include <dlfcn.h>
	#include <pthread.h>
	#include <unistd.h>
#endif

static int n_checks = 0, n_failures = 0;

#define CHECK(condition) do { \
	++n_checks; \
	if (!(condition)) { ++n_failures; printf("FAIL line %d: %s\n", __LINE__, #condition); } \
} while (0)

#define CHECK_INT(value, expected) do { \
	const long long v_ = (long long) (value), e_ = (long long) (expected); \
	++n_checks; \
	if (v_ != e_) { ++n_failures; printf("FAIL line %d: %s = %lld (expected %lld)\n", __LINE__, #value, v_, e_); } \
} while (0)

#define CHECK_STR(value, expected) do { \
	const char *v_ = (value), *e_ = (expected); \
	++n_checks; \
	if (v_ == NULL || strcmp(v_, e_) != 0) { ++n_failures; printf("FAIL line %d: %s = \"%s\" (expected \"%s\")\n", __LINE__, #value, v_ ? v_ : "(null)", e_); } \
} while (0)

static void section(const char *name)
{
	fflush(stdout);
	printf("\n## %s\n", name);
	fflush(stdout);
}

/** a thread that calls edax_stop after half a second (to stop a running bench) */
#ifdef _WIN32
static DWORD WINAPI stop_later(LPVOID unused)
{
	(void) unused;
	Sleep(500);
	edax_stop();
	return 0;
}
#else
static void* stop_later(void *unused)
{
	(void) unused;
	usleep(500000);
	edax_stop();
	return NULL;
}
#endif

/** a thread that calls edax_stop again and again while stop_often_on is set (a stop that must not change what a function computes) */
static volatile int stop_often_on;
#ifdef _WIN32
static DWORD WINAPI stop_often(LPVOID unused)
{
	(void) unused;
	while (stop_often_on) {
		edax_stop();
		Sleep(0);
	}
	return 0;
}
#else
static void* stop_often(void *unused)
{
	(void) unused;
	while (stop_often_on) {
		edax_stop();
		usleep(10);
	}
	return NULL;
}
#endif

/** one of the threads above: started, then waited for */
#ifdef _WIN32
typedef HANDLE TestThread;

static TestThread thread_start(LPTHREAD_START_ROUTINE f)
{
	return CreateThread(NULL, 0, f, NULL, 0, NULL);
}

static void thread_wait(TestThread t)
{
	WaitForSingleObject(t, INFINITE);
	CloseHandle(t);
}
#else
typedef pthread_t TestThread;

static TestThread thread_start(void* (*f)(void*))
{
	pthread_t t;
	pthread_create(&t, NULL, f, NULL);
	return t;
}

static void thread_wait(TestThread t)
{
	pthread_join(t, NULL);
}
#endif

/** square name */
static const char* square(const int x)
{
	static char s[8][4];
	static int i = 0;
	char *p = s[i = (i + 1) & 7];
	if (x == LIBEDAX_PASS) strcpy(p, "pa");
	else if (x == LIBEDAX_NOMOVE) strcpy(p, "--");
	else if (0 <= x && x < 64) { p[0] = (char) ('a' + x % 8); p[1] = (char) ('1' + x / 8); p[2] = '\0'; }
	else strcpy(p, "??");
	return p;
}

static void play(const char *moves)
{
	char buffer[256];
	strcpy(buffer, moves);
	edax_play(buffer);
}

static const char* moves(void)
{
	static char buffer[200];
	return edax_get_moves(buffer);
}

static LibedaxBoard board(void)
{
	LibedaxBoard b = {0, 0};
	edax_get_board(&b);
	return b;
}

static void print_position(const char *name, const LibedaxPosition *p)
{
	int i;
	printf("%s: board %016llx %016llx level %d score %d [%d, %d] leaf %s:%d links",
		name, p->board[0].player, p->board[0].opponent, p->level, p->score.value, p->score.lower, p->score.upper,
		square(p->leaf.move), p->leaf.score);
	for (i = 0; i < p->n_link; ++i) printf(" %s:%d", square(p->link[i].move), p->link[i].score);
	printf(" | lines %u wins %u draws %u losses %u | bestpaths %d %d flag %d\n",
		p->n_lines, p->n_wins, p->n_draws, p->n_losses, p->n_player_bestpaths, p->n_opponent_bestpaths, p->flag);
}

static void print_movelist(const char *name, const LibedaxMoveList *l)
{
	const LibedaxMove *m;
	int i, n = 0;
	printf("%s: %d moves, array", name, l->n_moves);
	for (i = 1; i <= l->n_moves; ++i) printf(" %s:%d", square(l->move[i].x), l->move[i].score);
	printf(", list");
	for (m = l->move[0].next; m && n <= l->n_moves; m = m->next, ++n) printf(" %s", square(m->x));
	printf("\n");
	CHECK_INT(n, l->n_moves);
}

static void print_hint(const char *name, const LibedaxHint *h)
{
	int i;
	printf("%s: %s score %d [%d, %d] depth %d@%d book %d pv", name, square(h->move), h->score, h->lower, h->upper, h->depth, h->selectivity, h->book_move);
	if (h->move != LIBEDAX_NOMOVE) for (i = 0; i < h->pv[0].n_moves && i < 8; ++i) printf(" %s", square(h->pv[0].move[i]));
	printf("\n");
}

/** the best paths of a board counted again (edax_book_stats_clean first: the original libedax needs it before counting another way) */
static void count_board_bestpath(LibedaxBoard *b, LibedaxPosition *p, const int player, const int opponent, const char *name)
{
	edax_book_stats_clean();
	memset(p, 0, sizeof *p);
	edax_book_count_board_bestpath(b, p, player, opponent, 0);
	print_position(name, p);
}

static long long file_size(const char *file)
{
	FILE *f = fopen(file, "rb");
	long long size = -1;
	if (f) {
		fseek(f, 0, SEEK_END);
		size = ftell(f);
		fclose(f);
	}
	return size;
}

/** @return true if two files exist and have the same content */
static bool same_files(const char *file_1, const char *file_2)
{
	FILE *f1 = fopen(file_1, "rb"), *f2 = fopen(file_2, "rb");
	bool same = (f1 != NULL && f2 != NULL);
	int c1 = 0, c2;

	while (same && c1 != EOF) {
		c1 = fgetc(f1);
		c2 = fgetc(f2);
		same = (c1 == c2);
	}
	if (f1) fclose(f1);
	if (f2) fclose(f2);
	return same;
}

static void write_file(const char *file, const char *text)
{
	FILE *f = fopen(file, "w");
	if (f) {
		fputs(text, f);
		fclose(f);
	}
}

/** functions added to the original api: NULL with the original libedax */
typedef void (*Deviate)(int, int);
typedef long long (*Export)(int, int, int, const char*, int);
static void* optional(const char *dll, const char *name)
{
#ifdef _WIN32
	HMODULE h = GetModuleHandleA(dll);
	return h ? (void*) GetProcAddress(h, name) : NULL;
#else
	(void) dll;
	return dlsym(RTLD_DEFAULT, name);
#endif
}

/** the layout of the structures of libedax.h */
static void test_layout(void)
{
	section("layout of the structures");
	CHECK_INT(sizeof (LibedaxBoard), 16);
	CHECK_INT(sizeof (LibedaxMove), 32);
	CHECK_INT(offsetof(LibedaxMove, next), 24);
	CHECK_INT(sizeof (LibedaxMoveList), 34 * 32 + 8);
	CHECK_INT(offsetof(LibedaxMoveList, n_moves), 34 * 32);
	CHECK_INT(sizeof (LibedaxLine), 88);
	CHECK_INT(sizeof (LibedaxHint), 136);
	CHECK_INT(offsetof(LibedaxHint, pv), 24);
	CHECK_INT(offsetof(LibedaxHint, time), 112);
	CHECK_INT(offsetof(LibedaxHint, book_move), 128);
	CHECK_INT(sizeof (LibedaxHintList), 34 * 136 + 8);
	CHECK_INT(sizeof (LibedaxLink), 2);
	CHECK_INT(sizeof (LibedaxPosition), 56);
	CHECK_INT(offsetof(LibedaxPosition, flag), 18);
	CHECK_INT(offsetof(LibedaxPosition, n_player_bestpaths), 20);
	CHECK_INT(offsetof(LibedaxPosition, link), 24);
	CHECK_INT(offsetof(LibedaxPosition, n_wins), 32);
	CHECK_INT(offsetof(LibedaxPosition, score), 48);
	CHECK_INT(offsetof(LibedaxPosition, n_link), 54);
	CHECK_INT(sizeof (LibedaxBook), 96);
	CHECK_INT(offsetof(LibedaxBook, options), 8);
	CHECK_INT(offsetof(LibedaxBook, stats), 28);
	CHECK_INT(offsetof(LibedaxBook, n), 56);
	CHECK_INT(offsetof(LibedaxBook, n_nodes), 60);
}

/**
 * book deviate3 129 254 export all writes every position of these negamaxed books once (no loss is out of the
 * limits), as in a new process: also after the book was changed, loaded or merged since the last export
 */
static void test_export_after_changes(Export export_lines)
{
	static const char *const file = "libedax-export.txt";
	LibedaxBook info;
	int n_a, n_b;

	section("export after the book changes");
	CHECK(export_lines != NULL);
	if (export_lines) {
		edax_init();
		edax_book_fix(); // (book add does not negamax the book)
		edax_book_save("libtest-export-a.dat"); // the book of the games of libtest-base.txt
		edax_book_info(&info);
		n_a = info.n_nodes;
		CHECK_INT(export_lines(3, 129, 254, file, 2), n_a);
		write_file("libtest-base2.txt", "F5F4E3F6D3E2F3F2E6D2\nF5D6C4D3C3F4F6F3E6E7\n");
		edax_book_new(2, 8);
		edax_book_add("libtest-base2.txt");
		edax_book_fix();
		edax_book_save("libtest-export-b.dat");
		edax_book_info(&info);
		n_b = info.n_nodes;
		printf("books: %d and %d positions\n", n_a, n_b);
		CHECK(n_b >= 12);
		CHECK_INT(export_lines(3, 129, 254, file, 2), n_b);
		edax_book_load("libtest-export-a.dat");
		edax_book_info(&info);
		CHECK_INT(info.n_nodes, n_a);
		CHECK_INT(export_lines(3, 129, 254, file, 2), n_a);
		edax_book_merge("libtest-export-b.dat");
		edax_book_info(&info);
		printf("after merge: %d positions\n", info.n_nodes);
		CHECK(info.n_nodes > n_a);
		CHECK_INT(export_lines(3, 129, 254, file, 2), info.n_nodes);
		remove(file);
	}
}

/** games learned together (edax_book_store_tasks, edax_book_store_games) */
static void test_store_games(const char *dll)
{
	int (*store_tasks)(void) = (int (*)(void)) optional(dll, "edax_book_store_tasks");
	int (*store_games)(const char*, char*) = (int (*)(const char*, char*)) optional(dll, "edax_book_store_games");
	LibedaxBook info;
	char status[16];
	int n_before;

	section("games learned together");
	CHECK(store_tasks != NULL && store_games != NULL);
	if (store_tasks && store_games) {
		// one game after the other (book-store-tasks = 1): as init, play, go & book store
		CHECK_INT(store_tasks(), 1);
		edax_book_new(4, 12);
		memset(status, 'x', sizeof status);
		CHECK_INT(store_games("f5d6c3\n2,F5F6E6\n// a comment\nf5f5\nf5 d6 c4\nfix\n", status), 3);
		CHECK_STR(status, "110010");
		CHECK_INT(edax_is_game_over(), 1); // the last game
		edax_book_info(&info);
		printf("after 3 games: %d positions\n", info.n_nodes);
		CHECK(info.n_nodes >= 14);
		CHECK_INT(store_games("", status), 0);
		CHECK_STR(status, "");
		CHECK_INT(store_games("a1", NULL), 0);
		CHECK_INT(store_games(NULL, status), 0);
		CHECK_STR(status, "");
		CHECK_INT(store_games("\n\r\nf5d6\r\n", status), 1); // empty lines, CR LF
		CHECK_STR(status, "001");
		{	// a line longer than 255 characters because of its spaces is a game; one with too many moves is not
			char games[1024];
			int j;
			strcpy(games, "f5d6c3");
			memset(games + 6, ' ', 300);
			strcpy(games + 306, "d3\n");
			for (j = 0; j < 150; ++j) memcpy(games + 309 + 2 * j, "f5", 2);
			strcpy(games + 609, "\nf5d6c3d3");
			CHECK_INT(store_games(games, status), 2);
			CHECK_STR(status, "101");
		}
		// the randomness of a line is any number, as the book-randomness setting
		CHECK_INT(store_games("200,f5d6\n99999999999999999999,f5f6\n-1,f5f4\n", status), 2);
		CHECK_STR(status, "110");

		// several games at the same time
		edax_set_option("n-tasks", "4");
		edax_set_option("book-store-tasks", "auto");
		CHECK(store_tasks() >= 2 && store_tasks() <= 4); // auto: n-tasks, which is 4 or the number of CPUs
		edax_set_option("book-store-tasks", "2");
		CHECK_INT(store_tasks(), 2);
		edax_init();
		play("f5f4");
		edax_book_info(&info); // (the positions of the book now: up to v4.5.5-nikque.26 the count after the first 3 games)
		n_before = info.n_nodes;
		printf("before 5 more games: %d positions\n", n_before);
		memset(status, 'x', sizeof status);
		CHECK_INT(store_games("f5f4e3\nf5d6c4d3\n1,f5d6c5f4e3\nf5d6c5f4e3\nh8\nf5f6e6f4e3", status), 5);
		CHECK_STR(status, "111101");
		CHECK_STR(moves(), "F5f4"); // the current game is not changed
		edax_book_info(&info);
		printf("after 5 more games: %d positions\n", info.n_nodes);
		CHECK(info.n_nodes > n_before);
		edax_book_store(); // a game: its positions are searched at the same time
		edax_book_fix();
		edax_set_option("book-store-tasks", "1");
		edax_set_option("n-tasks", "1");
		CHECK_INT(store_tasks(), 1);
	}
}

/**
 * book files that exist but could not be loaded (not books): each one is kept as <file>.damaged before a save replaces it,
 * also when another file could not be loaded since
 */
static void test_damaged_books(const char *dll)
{
	static const char *const damaged = "libtest-damaged.dat", *const other = "libtest-other.dat";
	char *damaged_args[] = {"", "-book-file", "libtest-damaged.dat", "-level", "6", "-n-tasks", "1"};
	int (*save_to)(const char*) = (int (*)(const char*)) optional(dll, "edax_book_save_checked");
	int (*file_status)(const char*) = (int (*)(const char*)) optional(dll, "edax_book_file_status");
	LibedaxBook info;

	section("book files that could not be loaded");
	CHECK(save_to != NULL);
	CHECK(file_status != NULL);
	write_file(damaged, "not a book: the book file of the settings\n");
	write_file("libtest-damaged.txt", "not a book: the book file of the settings\n");
	write_file(other, "not a book either\n");
	write_file("libtest-other.txt", "not a book either\n");
	libedax_initialize(7, damaged_args);
	edax_book_info(&info);
	CHECK_INT(info.n_nodes, 1); // (a new book)
	edax_book_load(other); // not loaded: the book is kept
	edax_book_info(&info);
	CHECK_INT(info.n_nodes, 1);
	if (file_status) {
		CHECK_INT(file_status(damaged), 1);
		CHECK_INT(file_status(other), 1);
		CHECK_INT(file_status("libtest-no-such-book.dat"), 0);
	}
	if (save_to) {
		CHECK_INT(save_to(damaged), 1);
		CHECK(same_files("libtest-damaged.dat.damaged", "libtest-damaged.txt")); // the book file of the settings is kept
		CHECK(file_size(damaged) > 0 && !same_files(damaged, "libtest-damaged.txt")); // (and replaced by the new book)
		CHECK_INT(save_to(other), 1);
		CHECK(same_files("libtest-other.dat.damaged", "libtest-other.txt"));
		if (file_status) CHECK_INT(file_status(damaged), 0);
	}
	libedax_terminate();
	remove(damaged); remove(other); remove("libtest-damaged.dat.damaged"); remove("libtest-other.dat.damaged");

	// the same file under a name written another way
	write_file(damaged, "not a book: the book file of the settings\n");
	libedax_initialize(7, damaged_args);
	if (save_to) {
		CHECK_INT(save_to("./libtest-damaged.dat"), 1);
		CHECK(same_files("libtest-damaged.dat.damaged", "libtest-damaged.txt"));
		if (file_status) CHECK_INT(file_status(damaged), 0); // (the file kept aside has no mark left)
		CHECK_INT(save_to(damaged), 1);
		CHECK_INT(file_size("libtest-damaged.dat.damaged.1"), -1); // the new book is replaced, not kept aside
	}
	libedax_terminate();
	remove(damaged); remove("libtest-damaged.dat.damaged");
#ifdef _WIN32
	{	// (names of files in other case are of the same file on Windows)
		int (*keep_unread)(const char*) = (int (*)(const char*)) optional(dll, "edax_book_keep_unread_file");
		write_file(damaged, "not a book: the book file of the settings\n");
		libedax_initialize(7, damaged_args);
		if (keep_unread) {
			CHECK_INT(keep_unread(".\\LIBTEST-DAMAGED.DAT"), 1);
			CHECK(same_files("libtest-damaged.dat.damaged", "libtest-damaged.txt"));
			if (file_status) CHECK_INT(file_status(damaged), 0);
		}
		libedax_terminate();
		remove(damaged); remove("libtest-damaged.dat.damaged");
	}
#endif
}

#ifdef _WIN32
/**
 * a book file that exists but cannot be opened (here: opened without sharing), in a folder whose name is not ASCII
 * (UTF-8 for the library): the book is not loaded, it is not replaced when the library ends, and it is loaded later
 */
static void test_locked_book(const char *dll)
{
	static const wchar_t folder[] = L"libtest-\u5b9a\u77f3", locked[] = L"libtest-\u5b9a\u77f3\\book.dat";
	char *locked_args[] = {"", "-book-file", "libtest-\xe5\xae\x9a\xe7\x9f\xb3/book.dat", "-level", "6", "-n-tasks", "1"};
	int (*keep_unread)(const char*) = (int (*)(const char*)) optional(dll, "edax_book_keep_unread_file");
	LibedaxBook info;
	HANDLE h;
	int n;

	section("a locked book in a folder of non-ASCII name");
	CHECK(keep_unread != NULL);
	CreateDirectoryW(folder, NULL);
	CHECK(CopyFileW(L"libtest-book2.dat", locked, FALSE));
	h = CreateFileW(locked, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	CHECK(h != INVALID_HANDLE_VALUE);
	libedax_initialize(7, locked_args);
	edax_book_info(&info);
	CHECK_INT(info.n_nodes, 1); // (a new book, not saved)
	if (keep_unread) CHECK_INT(keep_unread(locked_args[2]), 0); // the file could not be loaded, and cannot be kept under another name
	if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
	libedax_terminate();
	CHECK(CopyFileW(locked, L"libtest-locked.dat", FALSE) && same_files("libtest-locked.dat", "libtest-book2.dat"));
	libedax_initialize(7, locked_args);
	edax_book_info(&info);
	n = info.n_nodes;
	edax_book_load("libtest-book2.dat");
	edax_book_info(&info);
	CHECK(n > 1);
	CHECK_INT(n, info.n_nodes);
	libedax_terminate();
	DeleteFileW(locked);
	RemoveDirectoryW(folder);
}

/** a thread that closes a handle (of a file opened without sharing) after half a second */
static DWORD WINAPI close_later(LPVOID handle)
{
	Sleep(500);
	CloseHandle((HANDLE) handle);
	return 0;
}

/**
 * a good book file that another program holds without sharing when the library starts: the library waits a little for it;
 * when it still cannot be opened, it is not taken for a damaged file (no .damaged, never replaced) and edax_book_file_status
 * tells it
 */
static void test_busy_book(const char *dll)
{
	char *busy_args[] = {"", "-book-file", "libtest-busy.dat", "-level", "6", "-n-tasks", "1"};
	int (*file_status)(const char*) = (int (*)(const char*)) optional(dll, "edax_book_file_status");
	int (*keep_unread)(const char*) = (int (*)(const char*)) optional(dll, "edax_book_keep_unread_file");
	int (*save_to)(const char*) = (int (*)(const char*)) optional(dll, "edax_book_save_checked");
	LibedaxBook info;
	LibedaxBoard b;
	HANDLE h;
	int n;

	section("a book file held by another program at the start");
	CHECK(file_status != NULL && keep_unread != NULL && save_to != NULL);
	CHECK(CopyFileW(L"libtest-book2.dat", L"libtest-busy.dat", FALSE));
	DeleteFileW(L"libtest-busy.dat.damaged");
	libedax_initialize(7, busy_args);
	edax_book_info(&info);
	n = info.n_nodes;
	libedax_terminate();
	CHECK(n > 1);

	// released half a second after the start: the book is loaded
	h = CreateFileW(L"libtest-busy.dat", GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	CHECK(h != INVALID_HANDLE_VALUE);
	if (h != INVALID_HANDLE_VALUE) {
		HANDLE t = CreateThread(NULL, 0, close_later, h, 0, NULL);
		libedax_initialize(7, busy_args);
		thread_wait(t);
		edax_book_info(&info);
		CHECK_INT(info.n_nodes, n);
		if (file_status) CHECK_INT(file_status("libtest-busy.dat"), 0);
		libedax_terminate();
	}

	// held during the whole start: a new book, which never replaces the file
	h = CreateFileW(L"libtest-busy.dat", GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	CHECK(h != INVALID_HANDLE_VALUE);
	libedax_initialize(7, busy_args);
	edax_book_info(&info);
	CHECK_INT(info.n_nodes, 1);
	if (file_status) {
		CHECK_INT(file_status("libtest-busy.dat"), 2);
		CHECK_INT(file_status(".\\LIBTEST-BUSY.DAT"), 2); // (the same file, still held)
	}
	if (h != INVALID_HANDLE_VALUE) CloseHandle(h); // (it can be opened now)
	if (keep_unread) CHECK_INT(keep_unread("libtest-busy.dat"), 0);
	if (save_to) CHECK_INT(save_to("libtest-busy.dat"), 0);
	edax_init();
	play("f5");
	b = board();
	edax_book_add_board(&b); // the book is changed: it needs saving
	edax_book_info(&info);
	CHECK_INT(info.n_nodes, 2);
	libedax_terminate(); // (no save to the book file)
	CHECK(same_files("libtest-busy.dat", "libtest-book2.dat"));
	CHECK_INT(file_size("libtest-busy.dat.damaged"), -1);

	// the next start loads it
	libedax_initialize(7, busy_args);
	edax_book_info(&info);
	CHECK_INT(info.n_nodes, n);
	if (file_status) CHECK_INT(file_status("libtest-busy.dat"), 0);
	libedax_terminate();
	DeleteFileW(L"libtest-busy.dat");
}
#endif

int main(int argc, char **argv)
{
	const char *dll = (argc > 1 ? argv[1] : "libedax-x64.dll");
	char *args[] = {"", "-book-file", "libtest-book.dat", "-level", "6", "-n-tasks", "1"};
	LibedaxBoard b, start;
	LibedaxMove move;
	LibedaxMoveList *movelist = (LibedaxMoveList*) calloc(1, sizeof *movelist);
	LibedaxHintList *hintlist = (LibedaxHintList*) calloc(1, sizeof *hintlist);
	LibedaxHint hint;
	LibedaxPosition position;
	LibedaxBook info;
	union { LibedaxBenchResult result; char room[256]; } bench; // the original libedax keeps a lock after the result
	Deviate deviate2, deviate3;
	Export export_lines;
	bool original; // the original libedax: skip what it does not support
	char line[128];
	int i, n, sym;

	setvbuf(stdout, NULL, _IONBF, 0);

	// speed test: libedax_test <dll> bench <n problems> <n tasks> (as "edax -bench <n> -n <n tasks>")
	if (argc > 4 && strcmp(argv[2], "bench") == 0) {
		char *bench_args[] = {"", "-n-tasks", argv[4], "-book-file", "libtest-bench-book.dat"};
		libedax_initialize(5, bench_args);
		memset(&bench, 0, sizeof bench);
		edax_bench(&bench.result, atoi(argv[3]));
		printf("bench: %d positions, %llu nodes, %llu ms\n", bench.result.positions, bench.result.n_nodes, bench.result.T);
		return 0;
	}

	deviate2 = (Deviate) optional(dll, "edax_book_deviate2");
	deviate3 = (Deviate) optional(dll, "edax_book_deviate3");
	export_lines = (Export) optional(dll, "edax_book_deviate_export");
	original = argc == 3 && strcmp(argv[2], "--original") == 0;
	if (!original && !(deviate2 && deviate3)) {
		fprintf(stderr, "%s: missing edax_book_deviate2 or edax_book_deviate3 (use --original for the original library)\n", dll);
		return 1;
	}
	printf("%s: %s\n", dll, original ? "original libedax" : "libedax of Edax 4.5.5");
	if (!original) {
		int (*cpu_level)(void) = (int (*)(void)) optional(dll, "libedax_cpu_level");
		CHECK(cpu_level != NULL);
		if (cpu_level) {
			printf("cpu level: %d\n", cpu_level());
			CHECK(cpu_level() >= 0 && cpu_level() <= 4);
		}
	}

	test_layout();

	section("bit & board utilities");
	CHECK_INT(bit_count(0x0000001818000000ULL), 4);
	CHECK_INT(bit_count(0xffffffffffffffffULL), 64);
	CHECK_INT(first_bit(0x0000001818000000ULL), 27);
	CHECK_INT(last_bit(0x0000001818000000ULL), 36);
	CHECK(get_moves(0x0000000810000000ULL, 0x0000001008000000ULL) == 0x0000102004080000ULL);
	CHECK(can_move(0x0000000810000000ULL, 0x0000001008000000ULL));
	CHECK(!can_move(0x1ULL, 0x8000000000000000ULL));

	section("before the initialization");
	edax_init();
	CHECK_INT(edax_is_game_over(), 0);
	CHECK_INT(edax_get_current_player(), -1);
	CHECK(edax_opening() == NULL);

	section("initialization");
	remove("libtest-book.dat");
	libedax_initialize(7, args);
	if (!original) libedax_initialize(7, args); // ignored
	edax_version();
	edax_init();
	start = board();
	CHECK(start.player == 0x0000000810000000ULL && start.opponent == 0x0000001008000000ULL);
	CHECK_INT(edax_get_current_player(), 0);
	CHECK_STR(moves(), "");
	if (!original) {
		edax_get_last_move(&move);
		CHECK_INT(move.x, LIBEDAX_NOMOVE);
	}

	section("game");
	edax_mode(3);
	play("F5d6C5f4d3");
	CHECK_STR(moves(), "F5d6C5f4D3");
	CHECK_INT(edax_get_current_player(), 1);
	CHECK_INT(edax_get_disc(0), 6);
	CHECK_INT(edax_get_disc(1), 3);
	CHECK_INT(edax_get_mobility_count(1), 8);
	CHECK_INT(edax_get_mobility_count(0), 6);
	CHECK_INT(edax_can_move(), 1);
	CHECK_INT(edax_is_game_over(), 0);
	edax_get_last_move(&move);
	CHECK_INT(move.x, 19);
	CHECK(move.flipped == 0x0000000018000000ULL); // d4 & e4
	b = board();
	CHECK_INT(edax_board_is_pass(&b), 0);
	CHECK_INT(edax_board_get_square_color(&b, 19), 1); // d3: the opponent of the player to move
	CHECK_INT(edax_board_get_square_color(&b, 0), 2);
	printf("opening: %s / %s\n", edax_opening(), edax_ouverture());
	edax_play_print();

	edax_undo();
	CHECK_STR(moves(), "F5d6C5f4");
	edax_undo();
	edax_redo();
	edax_redo();
	CHECK_STR(moves(), "F5d6C5f4D3");
	edax_redo();
	CHECK_STR(moves(), "F5d6C5f4D3");

	CHECK_INT(edax_move("c3"), 1);
	CHECK_INT(edax_move("a1"), 0);
	CHECK_STR(moves(), "F5d6C5f4D3c3");

	edax_vmirror(); b = board(); printf("vmirror: %016llx %016llx\n", b.player, b.opponent);
	edax_vmirror();
	edax_hmirror(); b = board(); printf("hmirror: %016llx %016llx\n", b.player, b.opponent);
	edax_hmirror();
	edax_rotate(90); b = board(); printf("rotate 90: %016llx %016llx\n", b.player, b.opponent);
	edax_rotate(270);
	edax_rotate(180); edax_rotate(-180);
	edax_symetry(3); edax_symetry(3);
	CHECK_STR(moves(), "F5d6C5f4D3c3");

	edax_setboard("-W----W--------------------WB------WBB-----W--------------------B");
	b = board();
	CHECK_INT(edax_get_disc(0), 3);
	CHECK_INT(edax_get_disc(1), 5);
	CHECK_INT(edax_get_current_player(), 0);
	b.player = 0x0000000810000000ULL; b.opponent = 0x0000001008000000ULL;
	edax_setboard_from_obj(&b, 1);
	CHECK_INT(edax_get_current_player(), 1);
	b = board();
	CHECK(b.player == 0x0000001008000000ULL && b.opponent == 0x0000000810000000ULL);
	CHECK_INT(edax_move("e6"), 0);
	CHECK_INT(edax_move("d6"), 1);
	edax_new();
	CHECK_STR(moves(), "");
	CHECK_INT(edax_get_current_player(), 1);
	edax_save("libtest-game.txt");
	edax_init();
	edax_load("libtest-game.txt");
	edax_init();
	{
		char forced[] = "F5F6";
		edax_force(forced);
	}
	edax_go();
	CHECK_STR(moves(), "F5");
	edax_go();
	CHECK_STR(moves(), "F5f6");

	section("options");
	edax_set_option("level", "4");
	edax_set_option("book-randomness", "0");
	edax_set_option("unknown-option", "1");
	edax_book_randomness(0);
	edax_book_off();
	edax_book_on();
	edax_options_dump();

	section("hints without book");
	edax_init();
	play("f5");
	edax_hint(2, hintlist);
	CHECK_INT(hintlist->n_hints, 2);
	for (i = 1; i <= hintlist->n_hints; ++i) print_hint("hint", hintlist->hint + i);
	CHECK(hintlist->hint[1].move != hintlist->hint[2].move);
	edax_hint(60, hintlist);
	CHECK_INT(hintlist->n_hints, 3);
	if (!original) { // edax_stop from another thread ends edax_hint; a stop before a call does not stop it
		TestThread t;
		edax_set_option("level", "20");
		stop_often_on = 1;
		t = thread_start(stop_often);
		edax_hint(60, hintlist);
		stop_often_on = 0;
		thread_wait(t);
		printf("stopped hint: %d hints\n", hintlist->n_hints);
		CHECK(hintlist->n_hints < 3);
		edax_set_option("level", "4");
		edax_stop();
		edax_hint(60, hintlist);
		CHECK_INT(hintlist->n_hints, 3);
	}
	edax_hint_prepare(NULL);
	for (n = 0; n < 10; ++n) {
		if (n & 1) edax_hint_next(&hint); else edax_hint_next_no_multipv_depth(&hint);
		print_hint("hint next", &hint);
		if (hint.move == LIBEDAX_NOMOVE) break;
	}
	CHECK_INT(n, 3);
	// exclude d6 (43) & f6 (45): only f4 is left
	memset(movelist, 0, sizeof *movelist);
	movelist->move[1].x = 43; movelist->move[2].x = 45;
	movelist->move[0].next = movelist->move + 1; movelist->move[1].next = movelist->move + 2;
	movelist->n_moves = 2;
	edax_hint_prepare(movelist);
	edax_hint_next(&hint);
	CHECK_INT(hint.move, 29);
	edax_hint_next(&hint);
	CHECK_INT(hint.move, LIBEDAX_NOMOVE);

	section("a game played by edax, stored in a new book");
	edax_book_new(4, 12);
	edax_enable_book_verbose();
	edax_book_verbose(1);
	edax_book_verbose(0);
	edax_init();
	for (n = 0; !edax_is_game_over() && n < 80; ++n) edax_go();
	CHECK_INT(edax_is_game_over(), 1);
	printf("game: %s\n", moves());
	edax_book_store();
	edax_book_info(&info);
	printf("book: level %d depth %d positions %d\n", info.options.level, 61 - info.options.n_empties, info.n_nodes);
	CHECK_INT(info.options.level, 4);
	CHECK_INT(info.options.n_empties, 49);
	CHECK(info.n_nodes >= 13);
	CHECK(info.date.year >= 2026 && info.date.month >= 1 && info.date.month <= 12);
	edax_book_depth(10);
	edax_book_info(&info);
	CHECK_INT(info.options.n_empties, 51);

	section("book positions");
	edax_init();
	memset(&position, 0, sizeof position);
	edax_book_show(&position);
	print_position("root", &position);
	CHECK(position.board[0].player == 0x0000000810000000ULL);
	CHECK_INT(position.level, 4);
	CHECK(position.n_link >= 1);
	CHECK(position.n_lines >= 1);
	memset(movelist, 0, sizeof *movelist);
	edax_get_bookmove(movelist);
	print_movelist("book moves", movelist);
	CHECK(movelist->n_moves >= 1);
	memset(movelist, 0, sizeof *movelist);
	memset(&position, 0, sizeof position);
	sym = edax_get_bookmove_with_position(movelist, &position);
	printf("symetry %d\n", sym);
	CHECK(sym >= 0 && sym < 8);
	print_movelist("book moves", movelist);
	print_position("root", &position);
	memset(movelist, 0, sizeof *movelist);
	memset(&position, 0, sizeof position);
	strcpy(line, "F5"); // the original libedax changes the string
	sym = edax_get_bookmove_with_position_by_moves(line, movelist, &position);
	printf("by moves F5: symetry %d\n", sym);
	print_movelist("book moves", movelist);
	print_position("f5", &position);
	CHECK(sym >= 0 && sym < 8);
	strcpy(line, "F5F6E6F4G5E7E3F3C5C4G3C6D6D7");
	CHECK_INT(edax_get_bookmove_with_position_by_moves(line, movelist, &position), -1); // deeper than the book
	edax_book_off();
	CHECK_INT(edax_get_bookmove_with_position(movelist, &position), -1);
	CHECK_INT(movelist->n_moves, 0);
	edax_book_on();

	section("hints with book");
	edax_init();
	edax_hint(4, hintlist);
	CHECK_INT(hintlist->n_hints, 4);
	for (i = 1; i <= hintlist->n_hints; ++i) print_hint("hint", hintlist->hint + i);
	CHECK_INT(hintlist->hint[1].book_move, 1);
	edax_hint_prepare(NULL);
	for (n = 0; n < 10; ++n) {
		edax_hint_next(&hint);
		print_hint("hint next", &hint);
		if (hint.move == LIBEDAX_NOMOVE) break;
	}
	CHECK_INT(n, 4);

	section("best paths");
	b = start;
	memset(&position, 0, sizeof position);
	edax_book_count_bestpath(&b, &position);
	print_position("count bestpath", &position);
	CHECK(position.n_player_bestpaths >= 1 && position.n_opponent_bestpaths >= 1);
	CHECK(b.player == start.player && b.opponent == start.opponent);
	count_board_bestpath(&b, &position, LIBEDAX_BESTPATH_BEST, LIBEDAX_BESTPATH_BEST, "count board bestpath (best, best)");
	CHECK(position.n_player_bestpaths >= 1 && position.n_opponent_bestpaths >= 1);
	CHECK(position.flag & LIBEDAX_FLAG_BESTPATH_BLACK);
	count_board_bestpath(&b, &position, -64, LIBEDAX_BESTPATH_BEST, "count board bestpath (-64, best)");
	count_board_bestpath(&b, &position, LIBEDAX_BESTPATH_BEST, -64, "count board bestpath (best, -64)");
	edax_book_stop_count_bestpath();
	edax_book_stats_clean();
	memset(&position, 0, sizeof position);
	edax_book_show(&position);
	CHECK_INT(position.n_player_bestpaths, 0);
	b.player = 1; b.opponent = 2; // not in the book
	memset(&position, 0, sizeof position);
	edax_book_count_bestpath(&b, &position);
	CHECK_INT(position.n_player_bestpaths, 1);
	CHECK_INT(position.n_opponent_bestpaths, 1);

	section("book learning");
	edax_init();
	play("f5d6");
	edax_book_deviate(1, 1);
	edax_book_info(&info);
	printf("after deviate 1 1: %d positions\n", info.n_nodes);
	n = info.n_nodes;
	if (!original) {
		deviate2(1, 1);
		deviate3(1, 1);
		edax_book_info(&info);
		printf("after deviate2 & deviate3 1 1: %d positions\n", info.n_nodes);
		CHECK(info.n_nodes >= n);
	}
	{
		// book leaf-recalculate (v4.5.5-nikque.12): the leaves are searched again, no position is added
		static const char *const leaf_recalculate[] = {"edax_book_leaf_recalculate", "edax_book_leaf_recalculate2", "edax_book_leaf_recalculate3", "edax_book_leaf_recalculate4"};
		edax_book_info(&info);
		n = info.n_nodes;
		for (i = 0; i < 4; ++i) {
			Deviate f = (Deviate) optional(dll, leaf_recalculate[i]);
			if (f) {
				f(2, 4);
				edax_book_info(&info);
				CHECK_INT(info.n_nodes, n);
			}
		}
	}
	{
		// book deviate, deviate2, deviate3 ... export (v4.5.5-nikque.26): the lines are written, no position is added
		static const char *const file = "libedax-export.txt";
		if (!original) CHECK(export_lines != NULL);
		if (export_lines) {
			edax_book_info(&info);
			n = info.n_nodes;
			for (i = 1; i <= 3; ++i) {
				const long long all = export_lines(i, 2, 4, file, 2), ends = export_lines(i, 2, 4, file, 1), leaf = export_lines(i, 2, 4, file, 0);
				FILE *f = fopen(file, "r");
				int lines = 0, c;
				CHECK(all >= 1 && ends >= 1 && ends <= all && leaf >= 0);
				CHECK(f != NULL);
				if (f) {
					char first[256];
					if (fgets(first, sizeof first, f)) printf("export %d 2 4 leaf, first line: %s", i, first); // (the moves of the game first)
					rewind(f);
					while ((c = fgetc(f)) != EOF) lines += c == '\n';
					fclose(f);
				}
				CHECK_INT(lines, (int) leaf);
				edax_book_info(&info);
				CHECK_INT(info.n_nodes, n);
			}
			CHECK(export_lines(4, 2, 4, file, 0) == -1 && export_lines(2, 2, 4, file, 3) == -1);
			remove(file);
		}
	}
	{
		// book fill2, book fill / fill2 with loss limits, book leaf expand: positions may be added, none is removed
		void (*fill2)(int) = (void (*)(int)) optional(dll, "edax_book_fill2");
		void (*fill_limited)(int, int, int) = (void (*)(int, int, int)) optional(dll, "edax_book_fill_limited");
		void (*fill2_limited)(int, int, int) = (void (*)(int, int, int)) optional(dll, "edax_book_fill2_limited");
		void (*leaf_expand)(void) = (void (*)(void)) optional(dll, "edax_book_leaf_expand");
		Deviate leaf_expand_limited = (Deviate) optional(dll, "edax_book_leaf_expand_limited");
		if (!original) CHECK(fill2 && fill_limited && fill2_limited && leaf_expand && leaf_expand_limited);
		edax_book_info(&info);
		n = info.n_nodes;
		if (fill2) { fill2(2); edax_book_info(&info); CHECK(info.n_nodes >= n); n = info.n_nodes; }
		if (fill_limited) { fill_limited(2, 2, 4); edax_book_info(&info); CHECK(info.n_nodes >= n); n = info.n_nodes; }
		if (fill2_limited) { fill2_limited(2, 2, 4); edax_book_info(&info); CHECK(info.n_nodes >= n); n = info.n_nodes; }
		if (leaf_expand_limited) { leaf_expand_limited(2, 4); edax_book_info(&info); CHECK(info.n_nodes >= n); n = info.n_nodes; }
		if (leaf_expand) { leaf_expand(); edax_book_info(&info); CHECK(info.n_nodes >= n); }
		printf("after fill2, fill and fill2 2 2 4, leaf expand 2 4 and leaf expand: %d positions\n", info.n_nodes);
	}
	edax_book_enhance(4, 4);
	edax_book_fill(1);
	edax_book_fix();
	edax_book_negamax();
	edax_book_correct();
	edax_book_feed_hash();
	edax_book_info(&info);
	printf("after enhance, fill, fix, negamax, correct: %d positions\n", info.n_nodes);
	n = info.n_nodes;

	edax_book_add_board_pre_process();
	b.player = 0x0000000810000000ULL; b.opponent = 0x0000001008000000ULL;
	edax_book_add_board(&b); // already in the book
	edax_book_add_board_post_process();
	edax_book_info(&info);
	CHECK_INT(info.n_nodes, n);

	section("book files");
	edax_book_save("libtest-book2.dat");
	CHECK(file_size("libtest-book2.dat") > 100);
	if (!original) { // book save that tells whether the book was saved
		int (*save_to)(const char*) = (int (*)(const char*)) optional(dll, "edax_book_save_checked");
		CHECK(save_to != NULL);
		if (save_to) {
			CHECK_INT(save_to("libtest-book3.dat"), 1);
			CHECK_INT((int) file_size("libtest-book3.dat"), (int) file_size("libtest-book2.dat"));
			CHECK_INT(save_to("libtest-no-such-folder/book.dat"), 0);
			CHECK_INT(save_to(NULL), 0);
			{	// no position failed to be added by the last book function
				int (*failed)(void) = (int (*)(void)) optional(dll, "edax_book_failed");
				CHECK(failed != NULL);
				if (failed) CHECK_INT(failed(), 0);
			}
			remove("libtest-book3.dat");
		}
	}
	edax_book_export("libtest-book.txt");
	CHECK(file_size("libtest-book.txt") > 100);
	edax_book_new(4, 12);
	edax_book_info(&info);
	CHECK_INT(info.n_nodes, 1);
	edax_book_load("libtest-book2.dat");
	edax_book_info(&info);
	CHECK_INT(info.n_nodes, n);
	edax_book_load("libtest-no-such-book.dat");
	edax_book_import("libtest-book.txt");
	edax_book_info(&info);
	CHECK_INT(info.n_nodes, n);
	edax_book_new(4, 12);
	edax_book_merge("libtest-book2.dat");
	edax_book_info(&info);
	CHECK_INT(info.n_nodes, n);
	edax_init();
	play("f5d6");
	edax_book_subtree();
	edax_book_info(&info);
	printf("after subtree: %d positions\n", info.n_nodes);
	CHECK(info.n_nodes <= n);
	edax_book_load("libtest-book2.dat");
	edax_book_prune();
	edax_book_info(&info);
	printf("after prune: %d positions\n", info.n_nodes);
	CHECK(info.n_nodes <= n && info.n_nodes >= 1);
	edax_book_new(2, 4);
	edax_book_play();
	edax_book_info(&info);
	printf("after book new 2 4 & play: %d positions\n", info.n_nodes);
	CHECK(info.n_nodes > 1);
	edax_book_deepen();

	if (!original) { // the original libedax crashes, or loses the book or the file
		section("wrong calls");
		// NULL instead of a pointer: nothing is done
		edax_init();
		play("f5d6");
		edax_book_info(&info);
		i = info.n_nodes;
		edax_load(NULL); edax_save(NULL); edax_setboard(NULL); edax_setboard_from_obj(NULL, 0); edax_play(NULL); edax_force(NULL);
		edax_bench(NULL, 1); edax_bench_get_result(NULL); edax_hint(2, NULL); edax_get_bookmove(NULL);
		CHECK_INT(edax_get_bookmove_with_position(NULL, &position), -1);
		CHECK_INT(edax_get_bookmove_with_position(movelist, NULL), -1);
		CHECK_INT(edax_get_bookmove_with_position_by_moves(NULL, movelist, &position), -1);
		CHECK_INT(edax_get_bookmove_with_position_by_moves("f5", NULL, &position), -1);
		CHECK_INT(edax_get_bookmove_with_position_by_moves("f5", movelist, NULL), -1);
		edax_hint_prepare(NULL); edax_hint_next(NULL); edax_hint_next_no_multipv_depth(NULL);
		CHECK_INT(edax_move(NULL), 0);
		edax_book_load(NULL); edax_book_save(NULL); edax_book_import(NULL); edax_book_export(NULL); edax_book_merge(NULL);
		edax_book_show(NULL); edax_book_info(NULL); edax_book_count_bestpath(NULL, &position); edax_book_count_bestpath(&b, NULL);
		edax_book_count_board_bestpath(NULL, &position, 0, 0, 0); edax_book_count_board_bestpath(&b, NULL, 0, 0, 0);
		edax_book_add(NULL); edax_book_check(NULL); edax_book_extract(NULL); edax_book_add_board(NULL);
		edax_base_problem(NULL, 50, "libtest-null.obf"); edax_base_problem("libtest-null.txt", 50, NULL);
		edax_base_tofen(NULL, 50, "libtest-null.fen"); edax_base_tofen("libtest-null.txt", 50, NULL);
		edax_base_correct(NULL, 4); edax_base_complete(NULL);
		edax_base_convert(NULL, "libtest-null.ggf"); edax_base_convert("libtest-null.txt", NULL);
		edax_base_unique(NULL, "libtest-null.txt"); edax_base_unique("libtest-null.txt", NULL);
		edax_set_option(NULL, "1"); edax_set_option("level", NULL);
		CHECK(edax_get_moves(NULL) == NULL);
		edax_get_last_move(NULL); edax_get_board(NULL);
		CHECK_INT(edax_board_is_pass(NULL), 0);
		CHECK_INT(edax_board_get_square_color(NULL, 0), -1);
		CHECK_STR(moves(), "F5d6");
		edax_book_info(&info);
		CHECK_INT(info.n_nodes, i);
		// values out of range
		b = board();
		CHECK_INT(edax_board_get_square_color(&b, -1), 2);
		CHECK_INT(edax_board_get_square_color(&b, 64), 2);
		edax_hint(-1, hintlist);
		CHECK_INT(hintlist->n_hints, 0);
		edax_hint(0, hintlist);
		CHECK_INT(hintlist->n_hints, 0);
		// nothing to give: the list is empty
		memset(line, '-', 64); memcpy(line, "WWWWWWWW", 8); strcpy(line + 64, " B");
		edax_setboard(line); // game over
		CHECK_INT(edax_is_game_over(), 1);
		memset(movelist, 0x55, sizeof *movelist);
		edax_get_bookmove(movelist);
		CHECK_INT(movelist->n_moves, 0);
		CHECK(movelist->move[0].next == NULL);
		memset(movelist, 0x55, sizeof *movelist);
		CHECK_INT(edax_get_bookmove_with_position(movelist, &position), -1);
		CHECK_INT(movelist->n_moves, 0);
		memset(movelist, 0x55, sizeof *movelist);
		CHECK_INT(edax_get_bookmove_with_position_by_moves("f5f6e6f4e3c5c4e7c6e2", movelist, &position), -1); // not in the book
		CHECK_INT(movelist->n_moves, 0);
		edax_init();
		// a level out of range: the book is kept
		edax_book_new(100, 12);
		edax_book_new(-3, 12);
		edax_book_info(&info);
		CHECK_INT(info.options.level, 2);
		CHECK_INT(info.n_nodes, i);
		// a depth out of range: the nearest limit, as edax_book_depth (1 to 61)
		edax_book_new(4, -1);
		edax_book_info(&info);
		CHECK_INT(info.options.n_empties, 60);
		CHECK_INT(info.n_nodes, 1);
		edax_book_new(4, 100);
		edax_book_info(&info);
		CHECK_INT(info.options.n_empties, 0);
		CHECK_INT(info.n_nodes, 1);
		// a file which cannot be imported does not replace the book
		edax_book_load("libtest-book2.dat");
		edax_book_info(&info);
		i = info.n_nodes;
		CHECK(i > 1);
		edax_book_import("libtest-no-such-book.txt");
		edax_book_info(&info);
		CHECK_INT(info.n_nodes, i);
		// a file which cannot be loaded is not removed
		write_file("libtest-games.dat", "F5D6C3D3C4\n");
		edax_base_complete("libtest-games.dat");
		CHECK(file_size("libtest-games.dat") > 0);
		edax_base_correct("libtest-games.dat", 4);
		CHECK(file_size("libtest-games.dat") > 0);
	}

	if (!original) {
		section("book play <n>, extend, exhaust, extract all, base append");
		void (*play_to)(int) = (void (*)(int)) optional(dll, "edax_book_play_to");
		void (*extend)(void) = (void (*)(void)) optional(dll, "edax_book_extend");
		void (*exhaust)(void) = (void (*)(void)) optional(dll, "edax_book_exhaust");
		void (*extract_all)(const char*) = (void (*)(const char*)) optional(dll, "edax_book_extract_all");
		void (*append)(const char*, const char*) = (void (*)(const char*, const char*)) optional(dll, "edax_base_append");

		CHECK(play_to && extend && exhaust && extract_all && append);
		if (play_to && extend && exhaust && extract_all && append) {
			int before;

			edax_init();
			edax_book_new(2, 4);
			exhaust();
			edax_book_info(&info);
			printf("after book exhaust: %d positions\n", info.n_nodes);
			CHECK(info.n_nodes > 10);
			before = info.n_nodes;
			extend();
			edax_book_info(&info);
			CHECK(info.n_nodes >= before);
			edax_book_new(2, 4);
			play_to(53);
			edax_book_info(&info);
			printf("after book play 53: %d positions\n", info.n_nodes);
			CHECK_INT(info.n_nodes, 9); // (a line from 60 empty squares to 52)
			play_to(99);
			edax_book_info(&info);
			CHECK_INT(info.n_nodes, 9);

			remove("libtest-lines.txt");
			extract_all("libtest-lines.txt");
			CHECK(file_size("libtest-lines.txt") > 0);
			remove("libtest-append.txt");
			write_file("libtest-append-in.txt", "F5D6C3D3C4F4F6F3E6E7\n");
			append("libtest-append-in.txt", "libtest-append.txt");
			before = (int) file_size("libtest-append.txt");
			CHECK(before > 0);
			append("libtest-append-in.txt", "libtest-append.txt");
			CHECK_INT(file_size("libtest-append.txt"), 2 * before);
			append("libtest-append.txt", "./libtest-append.txt");
			CHECK_INT(file_size("libtest-append.txt"), 2 * before);
#ifdef _WIN32
			{ // names of UTF-8 (the Japanese words for "game records" and "append")
				const wchar_t *lines = L"棋譜.ggf", *added = L"追記.txt";

				DeleteFileW(lines);
				DeleteFileW(added);
				extract_all("\xe6\xa3\x8b\xe8\xad\x9c.ggf");
				CHECK(GetFileAttributesW(lines) != INVALID_FILE_ATTRIBUTES);
				append("libtest-append-in.txt", "\xe8\xbf\xbd\xe8\xa8\x98.txt");
				CHECK(GetFileAttributesW(added) != INVALID_FILE_ATTRIBUTES);
			}
#endif
		}
	}

	section("game database");
	write_file("libtest-base.txt", "F5D6C3D3C4F4F6F3E6E7\nF5F6E6F4E3C5C4E7C6E2\nF5D6C3D3C4F4F6F3E6E7\n");
	edax_book_new(2, 8);
	edax_book_add("libtest-base.txt");
	edax_book_info(&info);
	printf("after book add: %d positions\n", info.n_nodes);
	CHECK(info.n_nodes >= 12);
	edax_book_check("libtest-base.txt");
	edax_book_extract("libtest-extract.txt");
	CHECK(file_size("libtest-extract.txt") > 0);
	edax_base_convert("libtest-base.txt", "libtest-base.ggf");
	CHECK(file_size("libtest-base.ggf") > 0);
	edax_base_unique("libtest-base.txt", "libtest-unique.txt");
	CHECK(file_size("libtest-unique.txt") > 0 && file_size("libtest-unique.txt") < file_size("libtest-base.txt"));
	edax_base_problem("libtest-base.txt", 52, "libtest-problem.obf");
	CHECK(file_size("libtest-problem.obf") > 0);
	edax_base_tofen("libtest-base.txt", 52, "libtest-problem.fen");
	CHECK(file_size("libtest-problem.fen") > 0);
	write_file("libtest-short.txt", "F5F6E6F4G5E7E3F3C5C4G3C6D6D7C3C2B5D3B4E2F2H3F1D1D2G4G6C1E1B6C7D8F7H4F8G7\n");
	edax_base_complete("libtest-short.txt");
	edax_base_correct("libtest-short.txt", 4);
	CHECK(file_size("libtest-short.txt") > 0);
	if (!original) { // edax_stop from another thread does not cut the searches of edax_base_complete: the games are completed as without it
		const char *unfinished =
			"F5D6C3D3C4F4F6F3E6E7C6F7E8C5B4C7D7B6B5E3C8A3A4A5D2F8F2F1A7C1\n"
			"F5F6E6F4E3C5G6F3G5G3D3E2F1H5C4C6D6D7E7C7H6C2G4H4D2F8E8D8B5F7\n"
			"F5D6C4D3C3F4F6F3E6E7C6F7E8C5B4C7D7B6B5E3C8A3A4A5D2F8F2F1A7C1\n"
			"F5F4E3F6D3E2F3F2E6D2G5H6G4H5D1D6G3H4E1C3C2C6F1G6B3C1B1B4C5C4\n";
		TestThread t;
		write_file("libtest-stop1.txt", unfinished);
		write_file("libtest-stop2.txt", unfinished);
		edax_base_complete("libtest-stop1.txt");
		stop_often_on = 1;
		t = thread_start(stop_often);
		edax_base_complete("libtest-stop2.txt");
		stop_often_on = 0;
		thread_wait(t);
		CHECK(file_size("libtest-stop1.txt") > 100 && same_files("libtest-stop1.txt", "libtest-stop2.txt"));
	}

	if (!original) test_export_after_changes(export_lines);

	if (!original) test_store_games(dll);

	section("bench");
	memset(&bench, 0, sizeof bench);
	edax_bench_get_result(&bench.result);
	CHECK_INT(bench.result.positions, 0);
	edax_bench(&bench.result, 2);
	printf("bench: %d positions, %llu nodes\n", bench.result.positions, bench.result.n_nodes);
	CHECK_INT(bench.result.positions, 2);
	CHECK(bench.result.n_nodes > 0);
	{ // edax_stop from another thread ends the bench: the problem that was cut is not counted, the time is the time of the others
		const TestThread t = thread_start(stop_later);
		memset(&bench, 0, sizeof bench);
		edax_bench(&bench.result, 100);
		thread_wait(t);
		printf("stopped bench: %d positions, %llu nodes, %llu ms\n", bench.result.positions, bench.result.n_nodes, bench.result.T);
		CHECK(bench.result.positions < 100);
		CHECK(bench.result.T < 60000);
	}


	section("stop & termination");
	edax_stop();
	edax_init();
	edax_go(); // a stop when nothing runs does not stop the next search
	CHECK_STR(moves(), "F5");
	edax_disable_book_verbose();
	libedax_terminate();
	if (!original) libedax_terminate(); // ignored
	CHECK_INT(edax_is_game_over(), 0);
	CHECK(file_size("libtest-book.dat") > 0); // the new book is saved

	section("second initialization");
	libedax_initialize(3, args);
	edax_init();
	play("f5");
	CHECK_STR(moves(), "F5");
	edax_book_info(&info);
	printf("book: level %d depth %d positions %d\n", info.options.level, 61 - info.options.n_empties, info.n_nodes);
	CHECK(info.n_nodes >= 12);
	if (!original && export_lines) { // (the book of the last export was freed by libedax_terminate)
		edax_init();
		CHECK_INT(export_lines(3, 129, 254, "libedax-export.txt", 2), info.n_nodes);
		remove("libedax-export.txt");
	}
	libedax_terminate();

	if (!original) test_damaged_books(dll);
#ifdef _WIN32
	if (!original) test_locked_book(dll);
	if (!original) test_busy_book(dll);
#endif

	free(movelist);
	free(hintlist);
	printf("\n%d checks, %d failures\n", n_checks, n_failures);
	return n_failures ? 1 : 0;
}
