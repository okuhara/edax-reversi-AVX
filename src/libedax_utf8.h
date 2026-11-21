/**
 * @file libedax_utf8.h
 *
 * @brief File names in UTF-8 for the library on Windows.
 *
 * The programs using the library (e.g. through libedax4dart) give their file names in UTF-8,
 * but the C functions of Windows expect them in the ANSI code page. When the library is built,
 * this file is included before the sources of Edax, and replaces the functions which take a
 * file name: a name is first read as UTF-8, then as ANSI (as the original libedax does) if it is not
 * UTF-8 or if no file has the name read as UTF-8. A file found under the name read as UTF-8 that
 * cannot be opened (locked, a directory...) keeps the error of this name (errno, GetLastError).
 *
 * @date 2026
 * @author Nikque
 */

#ifndef EDAX_LIBEDAX_UTF8_H
#define EDAX_LIBEDAX_UTF8_H

#if defined(LIB_BUILD) && defined(_WIN32)

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <winsock2.h>
#include <windows.h>

/**
 * @brief Convert a string to wide characters.
 * @param s String.
 * @param code_page CP_UTF8 (an invalid string is refused) or CP_ACP.
 * @return a new string, or NULL.
 */
static wchar_t* lib_to_wide(const char *s, const unsigned int code_page)
{
	const DWORD flags = (code_page == CP_UTF8 ? MB_ERR_INVALID_CHARS : 0);
	wchar_t *w;
	int n;

	if (s == NULL) return NULL;
	n = MultiByteToWideChar(code_page, flags, s, -1, NULL, 0);
	if (n <= 0) return NULL;
	w = (wchar_t*) malloc(n * sizeof *w);
	if (w && MultiByteToWideChar(code_page, flags, s, -1, w, n) <= 0) {
		free(w);
		w = NULL;
	}
	return w;
}

/** code pages of a file name, in the order they are tried */
static const unsigned int lib_code_page[2] = {CP_UTF8, CP_ACP};

/** @brief fopen with a file name in UTF-8 (or ANSI). */
static FILE* lib_fopen(const char *path, const char *mode)
{
	wchar_t *wmode = lib_to_wide(mode, CP_ACP);
	FILE *f = NULL;
	int i, missing = 1;

	for (i = 0; i < 2 && missing && wmode != NULL; ++i) {
		wchar_t *wpath = lib_to_wide(path, lib_code_page[i]);
		if (wpath) {
			f = _wfopen(wpath, wmode);
			missing = (f == NULL && errno == ENOENT);
			free(wpath);
		}
	}
	free(wmode);
	return f;
}

/** @brief remove with a file name in UTF-8 (or ANSI). */
static int lib_remove(const char *path)
{
	int r = -1, i, missing = 1;

	for (i = 0; i < 2 && missing; ++i) {
		wchar_t *wpath = lib_to_wide(path, lib_code_page[i]);
		if (wpath) {
			r = _wremove(wpath);
			missing = (r != 0 && errno == ENOENT);
			free(wpath);
		}
	}
	return r;
}

/** @brief MoveFileEx with file names in UTF-8 (or ANSI). */
static BOOL lib_move_file(const char *from, const char *to, const DWORD flags)
{
	BOOL r = FALSE;
	int i, missing = 1;

	for (i = 0; i < 2 && missing; ++i) {
		wchar_t *wfrom = lib_to_wide(from, lib_code_page[i]);
		wchar_t *wto = lib_to_wide(to, lib_code_page[i]);
		if (wfrom && wto) {
			const DWORD e = (r = MoveFileExW(wfrom, wto, flags)) ? ERROR_SUCCESS : GetLastError();
			missing = (e == ERROR_FILE_NOT_FOUND || e == ERROR_PATH_NOT_FOUND);
		}
		free(wfrom);
		free(wto);
	}
	return r;
}

/** @brief CreateFile with a file name in UTF-8 (or ANSI). */
static HANDLE lib_create_file(const char *path, const DWORD access, const DWORD share, LPSECURITY_ATTRIBUTES security, const DWORD disposition, const DWORD flags, HANDLE template_file)
{
	HANDLE h = INVALID_HANDLE_VALUE;
	int i, missing = 1;

	for (i = 0; i < 2 && missing; ++i) {
		wchar_t *wpath = lib_to_wide(path, lib_code_page[i]);
		if (wpath) {
			const DWORD e = (h = CreateFileW(wpath, access, share, security, disposition, flags, template_file)) != INVALID_HANDLE_VALUE ? ERROR_SUCCESS : GetLastError();
			missing = (e == ERROR_FILE_NOT_FOUND || e == ERROR_PATH_NOT_FOUND);
			free(wpath);
		}
	}
	return h;
}

#define fopen(path, mode) lib_fopen(path, mode)
#define remove(path) lib_remove(path)
#undef MoveFileExA
#define MoveFileExA(from, to, flags) lib_move_file(from, to, flags)
#undef CreateFileA
#define CreateFileA(path, access, share, security, disposition, flags, template_file) lib_create_file(path, access, share, security, disposition, flags, template_file)

#endif

#endif /* EDAX_LIBEDAX_UTF8_H */
