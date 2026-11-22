/**
 * @file book-learn-stop.c
 *
 * Edax that stops, or fails a save, at a given write of book learn (see book-resume.py --stop-exe).
 *
 * The writes are numbered from 1: before and after each file opened to write and each file replaced, for
 * the files with .store or .learn in their names.
 * - EDAX_TEST_STOP_AT=n: the process ends at the write n, without flushing anything (exit code 77).
 * - EDAX_TEST_FAIL_STORE=n: the n-th replacement of a .store file fails.
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

static int test_n_writes, test_n_stores;

static int test_setting(const char *name)
{
	const char *value = getenv(name);

	return value ? atoi(value) : 0;
}

static void test_point(const char *path, const char *what)
{
	if (strstr(path, ".store") == NULL && strstr(path, ".learn") == NULL) return;
	if (++test_n_writes != test_setting("EDAX_TEST_STOP_AT")) return;
	fprintf(stderr, "stopped %s %s\n", what, path);
	fflush(stderr);
#ifdef _WIN32
	TerminateProcess(GetCurrentProcess(), 77);
#else
	_exit(77);
#endif
}

static bool test_fails(const char *to)
{
	return strstr(to, ".store") != NULL && ++test_n_stores == test_setting("EDAX_TEST_FAIL_STORE");
}

static FILE* test_fopen(const char *path, const char *mode)
{
	const bool writes = (mode[0] == 'w');
	FILE *f;

	if (writes) test_point(path, "before the opening of");
	f = fopen(path, mode);
	if (writes) test_point(path, "after the opening of");
	return f;
}

#ifdef _WIN32
static BOOL test_move_file(const char *from, const char *to, DWORD flags)
{
	BOOL ok = FALSE;

	test_point(to, "before the replacement of");
	if (!test_fails(to)) ok = MoveFileExA(from, to, flags);
	test_point(to, "after the replacement of");
	return ok;
}
#define MoveFileExA test_move_file
#else
static int test_rename(const char *from, const char *to)
{
	int r = -1;

	test_point(to, "before the replacement of");
	if (!test_fails(to)) r = rename(from, to);
	test_point(to, "after the replacement of");
	return r;
}
#define rename test_rename
#endif
#define fopen test_fopen

#include "../src/all.c"
