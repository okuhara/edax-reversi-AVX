/**
 * @file util.c
 *
 * @brief Various utilities.
 *
 * This should be the only file with linux/windows
 * dedicated code.
 *
 * @date 1998 - 2023
 * @author Richard Delorme
 * @version 4.5
 */

#include "bit.h"
#include "board.h"
#include "move.h"
#include "util.h"
#include "options.h"
#include "const.h"

#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include <time.h>

#if defined(__unix__) || defined(__APPLE__)

#include <unistd.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/stat.h>

#endif // __unix__ || __APPLE__

#if defined(__linux__)

#include <sys/sysinfo.h>
#include <sched.h>

#endif // __linux__

#if defined(_WIN32)

#include <winsock2.h>
#include <windows.h>
#include <sys/types.h>
#include <sys/stat.h>

#define fileno _fileno

#ifndef S_ISREG
#define S_ISREG(x) (x & _S_IFREG)
#endif
#ifndef S_ISFIFO
#define S_ISFIFO(x) (x & _S_IFIFO)
#endif

#endif // _WIN32

#if defined(__unix__) || defined(__APPLE__)

/**
 * @brief real_clock
 *
 * Measure wall clock time.
 * @return time in milliseconds.
 */
long long real_clock(void)
{
#if _POSIX_TIMERS > 0
	struct timespec tv;
	clock_gettime(CLOCK_MONOTONIC, &tv);
	return tv.tv_sec * 1000ULL + tv.tv_nsec / 1000000ULL;
#else
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec * 1000 + tv.tv_usec / 1000;
#endif
}

/**
 * @brief cpu_clock
 *
 * Measure cpu time.
 * @return time in milliseconds.
 */
long long cpu_clock(void)
{
	struct rusage u;
	getrusage(RUSAGE_SELF, &u);
	return 1000 * u.ru_utime.tv_sec + u.ru_utime.tv_usec / 1000;
}

#elif defined (_WIN32)

long long real_clock(void)
{
	return (long long) GetTickCount64(); // GetTickCount() wraps after 49.7 days
}

long long cpu_clock(void)
{
	return (long long) GetTickCount64();
}

#endif

/**
 * @brief Time clock.
 *
 * Can be set as a real_clock or a cpu_clock.
 *
 * @return time in milliseconds.
 */
long long (*time_clock)(void) = real_clock;

/**
 * @brief Print time as "D:HH:MM:SS.CC".
 *
 * @param t Time in milliseconds.
 * @param justified add spaces or not before the text.
 * @param f Stream.
 */
void time_print(long long t, bool justified, FILE* f)
{
	int d, h, m, s, c;
	int sign;
	const char *space = justified ? "   " : "";


	if (t < 0) {sign = -1; t = -t;} else sign = +1;
	d = t / 86400000; t -= d * 86400000LL;
	h = t / 3600000; t -= h * 3600000;
	m = t / 60000; t -= m * 60000;
	s = t / 1000;
	c = (t - s * 1000);

	// the sign is printed apart: up to v4.5.5-nikque.15 it was lost under a minute (or an hour, a day), as -0
	if (d) fprintf(f, "%s%d:%02d:%02d:%02d.%03d", sign < 0 ? "-" : (d < 10 ? " " : ""), d, h, m, s, c);
	else if (h) fprintf(f, "%s%s%d:%02d:%02d.%03d", space, sign < 0 ? "-" : (h < 10 ? " " : ""), h, m, s, c);
	else fprintf(f, "%s%s%s%d:%02d.%03d", space, space, sign < 0 ? "-" : (m < 10 ? " " : ""), m, s, c);
}

/**
 * @brief Print local time.
 * @param f Output stream.
 */
void time_stamp(FILE *f)
{
	time_t t = time(NULL);
	struct tm *tm;
	tm = localtime(&t);

	fprintf(f, "[%4d/%2d/%2d %2d:%2d:%2d] ", tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);
}


/**
 * @brief sleep for t ms.
 * @param t time in ms.
 */
void relax(int t)
{
#if defined(__unix__) || defined(__APPLE__)
	struct timespec ts;
	ts.tv_sec = t / 1000;
	ts.tv_nsec = (t % 1000) * 1000000;
	nanosleep(&ts, NULL);
#elif defined(_WIN32)
	Sleep(t);
#endif
}

/**
 * @brief Format a value with a unit.
 *
 * @param v Value to print.
 * @param unit Unit.
 * @param f Output string.
 */
char* format_scientific(double v, const char *unit, char *f)
{
#ifdef UNICODE
	static const wchar_t multiple[] = L"EPTGMK mμnpfa"; // μ:U+03BC
	static const char fmt[] = " %5.*f %lc%s";
#else
	static const char multiple[] = "EPTGMK mμnpfa"; // μ:B5@CP1252
	static const char fmt[] = " %5.*f %c%s";
#endif
	int u, d;

	if (fabs(v) < 1e-24) {
		u = 0;
	} else {
		u = floor(log10(fabs(v)) / 3.0);
		if (u > 6) u = 6; else if (u < -6) u = -6;
		v /= pow(10, 3 * u);
	}

	if (fabs(v) - floor(fabs(v)) < 0.01) d = 1;
	else if (fabs(v + 0.05) < 10.0) d = 3;
	else if (fabs(v + 0.5) < 100.0) d = 2;
	else d = 1;
	sprintf(f, fmt, d, v, multiple[6 - u], unit);

	return f;
}

/**
 * @brief Print a value with a unit.
 *
 * @param v Value to print.
 * @param unit Unit.
 * @param f Output stream.
 */
void print_scientific(double v, const char *unit, FILE *f)
{
	char s[32];
	fputs(format_scientific(v, unit, s), f);
}


/**
 * @brief Read a line.
 *
 * Read a line from an input string. The needed memory is allocated.
 *
 * @param f Input stream.
 * @return NULL if the line is empty, the read line otherwise.
 */
char* string_read_line(FILE *f)
{
	int c, i, n;
	char *line;

	n = 256; i = 0;
	line = (char*) malloc(n);
	if (line == NULL) fatal_error("Allocation error\n");

	while ((c = getc(f)) != EOF && c != '\n') {
		line[i++] = (char) c;
		if (i == n) {
			n *= 2;
			line = (char*) realloc(line, n);
			if (line == NULL) fatal_error("Allocation error\n");
		}
	}
	line[i] = '\0';

	if (i == 0 && c == EOF) {
		free(line);
		line = NULL;
	}

	return line;
}

/**
 * @brief Replace a file by another one, that was written under another name first.
 *
 * The book, the game files and the OBF files are saved this way: a file is never left half written.
 *
 * @param from File written.
 * @param to File to replace (it may not exist).
 * @return true if the file was replaced.
 */
bool file_replace(const char *from, const char *to)
{
#ifdef _WIN32
	return MoveFileExA(from, to, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
	return rename(from, to) == 0;
#endif
}

/**
 * @brief Tell whether two file names are of the same file.
 *
 * A name written another way (relative or full, with '/' or '\\', in another case on Windows, through a
 * link) is of the same file: the files are compared (their volume and index on Windows, their device and
 * inode elsewhere). A name of no file is only the same as itself.
 *
 * @param a File name.
 * @param b File name.
 * @return true if the two names are of the same file.
 */
bool file_same(const char *a, const char *b)
{
	if (a == NULL || b == NULL) return false;
	if (strcmp(a, b) == 0) return true;
#ifdef _WIN32
	{
		// (the attributes only: a file opened by another program without sharing is compared too)
		const DWORD share = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
		HANDLE ha = CreateFileA(a, FILE_READ_ATTRIBUTES, share, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
		HANDLE hb = CreateFileA(b, FILE_READ_ATTRIBUTES, share, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
		BY_HANDLE_FILE_INFORMATION ia, ib;
		bool same = false;

		if (ha != INVALID_HANDLE_VALUE && hb != INVALID_HANDLE_VALUE && GetFileInformationByHandle(ha, &ia) && GetFileInformationByHandle(hb, &ib)) {
			same = (ia.dwVolumeSerialNumber == ib.dwVolumeSerialNumber && ia.nFileIndexHigh == ib.nFileIndexHigh && ia.nFileIndexLow == ib.nFileIndexLow);
		}
		if (ha != INVALID_HANDLE_VALUE) CloseHandle(ha);
		if (hb != INVALID_HANDLE_VALUE) CloseHandle(hb);
		return same;
	}
#else
	{
		struct stat sa, sb;
		return stat(a, &sa) == 0 && stat(b, &sb) == 0 && sa.st_dev == sb.st_dev && sa.st_ino == sb.st_ino;
	}
#endif
}


/**
 * @brief Duplicate a string.
 *
 * @param s string to duplicate.
 * @return a copy of the string.
 */
char* string_duplicate(const char *s)
{
	char *d;

	if (s) {
		const int n = strlen(s) + 1;
		d = (char*) malloc(n);
		if (d) memcpy(d, s, n);
	} else {
		d = NULL;
	}

	return d;
}

/**
 * @brief Read time as "D:HH:MM:SS.C".
 *
 * @param string Time as a string.
 * @return Time in milliseconds.
 */
long long string_to_time(const char *string)
{
	long long t = 0;
	int n = 0;
	double x = 0.0;

	if (string) {
		// SS, MM:SS, HH:MM:SS or D:HH:MM:SS: the fields are read first (up to v4.5.5-nikque.15 the form
		// D:HH:MM:SS was read as if a day lasted 24 minutes)
		static const int ratio[3] = {60, 60, 24}; // seconds in a minute, minutes in an hour, hours in a day
		int field[4], k = 0, i;
		const char *s = parse_skip_spaces(string);
		s = parse_int(s, &n);
		field[k++] = n;
		while (*s == ':' && k < 4) {
			n = 0; // (an empty field is 0: up to v4.5.5-nikque.26 parse_int left the field before, "5:" was 5 minutes 5 seconds)
			s = parse_int(s + 1, &n);
			field[k++] = n;
		}
		t = field[0];
		for (i = 1; i < k; ++i) t = t * ratio[k - 1 - i] + field[i];
		if (*s == '.' && isdigit((unsigned char) s[1])) {
			s = parse_real(s, &x);
		}
		t = (t + x) * 1000;
	}

	return t;
}

/**
 * @brief Change all char of a string to lowercase.
 *
 * @param s string.
 */
void string_to_lowercase(char *s)
{
	if (s) {
		do {
			*s = tolower((unsigned char) *s);
		} while (*s++);
	}
}

/**
 * @brief Change all char of a string to uppercase.
 *
 * @param s Input string.
 */
void string_to_uppercase(char *s)
{
	if (s) {
		do {
			*s = toupper((unsigned char) *s);
		} while (*s++);
	}
}

/**
 * @brief Convert the two first chars of a string into a coordinate.
 *
 * @param s String.
 * @return coordinate value (A1 to H8 or PASS or NOMOVE).
 */
int string_to_coordinate(const char *s)
{
	int x = NOMOVE;

	if (s && *s) {
		if (tolower((unsigned char) s[0]) == 'p' && s[1] == '@') s += 2;
		else if (s[0] == '@') ++s;
		if (*s == '\0') return NOMOVE; // ("P@" or "@" alone: up to v4.5.5-nikque.15 the char after the end was read)

		if (tolower((unsigned char) s[0]) == 'p' && (tolower((unsigned char) s[1]) == 'a' || tolower((unsigned char) s[1]) == 's')) x = PASS;
		else if (s[0] == '@' && s[1] == '@') x = PASS; // xboard pass = "@@@@"
		else {
			int c = tolower((unsigned char) s[0]) - 'a';
			int r = s[1] - '1';
			if (0 <= c && c <= 7 && 0 <= r && r <= 7) x = r * 8 + c;
		}
	}

	return x;
}

/**
 * @brief remove spaces from a string.
 *
 * @param s string.
 * @return a word.
 */

char* string_to_word(char *s)
{
	char *w = NULL;

	if (s) {
		while (*s && isspace((unsigned char) *s++)) ;
		w = --s;
		while (*s && !isspace((unsigned char) *s++)) ;
		*s = '\0';
	}
	return w;
}

/**
 * @brief Convert a string into a boolean.
 *
 * Parse a string into a boolean (false/true).
 *
 * @param s String.
 * @return a word.
 */
bool string_to_boolean(const char *s)
{
	if (strcmp(s, "false") == 0
	||  strcmp(s, "off") == 0
	||  strcmp(s, "no") == 0
	||  strcmp(s, "0") == 0) return false;

	if (strcmp(s, "true") == 0
	||  strcmp(s, "on") == 0
	||  strcmp(s, "yes") == 0
	||  strcmp(s, "1") == 0) return true;

	errno = EINVAL;
	return false;
}

/**
 * @brief Convert a string into an integer.
 *
 * Parse a string into an int.
 *
 * @param s String.
 * @param default_value default value if parsing failed.
 * @return an integer.
 */
int string_to_int(const char *s, const int default_value)
{
	char *end = (char*) s;
	long n = 0;

	if (s) n = strtol(s, &end, 10);

	if (end == s) {
		errno = EINVAL;
		n = default_value;
	}
	if (n < INT_MIN) {
		n = INT_MIN;
		errno = ERANGE;
	} else if (n > INT_MAX) {
		n = INT_MAX;
		errno = ERANGE;
	}

	return (int) n;
}

/**
 * @brief Convert a string into a real number.
 *
 * Parse a string into a double.
 *
 * @param s String.
 * @param default_value default value if parsing failed.
 * @return a real number.
 */
double string_to_real(const char *s, const double default_value)
{
	char *end = (char*) s;
	double x = 0.0;

	if (s) x = strtod(s, &end);

	if (end == s) {
		errno = EINVAL;
		x = default_value;
	}

	return x;
}

/* Parsing functions
 * They differs from string conversion as they convert a
 * string but return the next position from that string.
 */

/**
 * @brief Skip spaces.
 *
 * @param string String to parse.
 * @return The remaining of the input string.
 */
char* parse_skip_spaces(const char *string)
{
	if (string) {
		while (*string && isspace((unsigned char) *string)) ++string;
	}
	return (char*) string;
}

/**
 * @brief Skip word.
 *
 * @param string String to parse.
 * @return The remaining of the input string.
 */
char* parse_skip_word(const char *string)
{
	if (string) {
		while (*string && isspace((unsigned char) *string)) ++string;
		while (*string && !isspace((unsigned char) *string)) ++string;
		while (*string && isspace((unsigned char) *string)) ++string;
	}
	return (char*) string;
}

/**
 * @brief Find a char.
 *
 * @param string String to parse.
 * @param c Seeked char.
 * @return The input string at the char position or an empty string.
 */
char* parse_find(const char *string, const int c)
{
	if (string) {
		while (*string && *string != c) ++string;
	}
	return (char*) string;
}

/**
 * @brief Parse a word.
 *
 * @param string String to parse
 * @param word String receiving a copy of the parsed word.
 * @param n word string capacity.
 * NOTE: It is assumed that w is big enough to contains the word copy.
 * @return The remaining of the input string.
 */
char* parse_word(const char *string, char *word, unsigned int n)
 {
	if (n == 0) return (char*) string;
	if (string) {
		string = parse_skip_spaces(string);
		while (*string && !isspace((unsigned char)*string)) {
			if (n > 1) { *word++ = *string; --n; }
			++string;
		}
	}
	*word = '\0';
	return (char*) string;
}

/**
 * @brief Parse a field.
 *
 * @param string String to parse
 * @param word String receiving a copy of the parsed word.
 * @param n word string capacity.
 * @param separator field separator.
 * NOTE: It is assumed that w is big enough to contains the word copy.
 * @return The remaining of the input string.
 */
char* parse_field(const char *string, char *word, unsigned int n, char separator)
 {
	if (n == 0) return (char*) string;
	if (string) {
		string = parse_skip_spaces(string);
		while (*string && *string != separator) {
			if (n > 1) { *word++ = *string; --n; }
			++string;
		}
		if (*string == separator) ++string;
	}
	*word = '\0';
	return (char*) string;
}

/**
 * @brief Parse a line.
 *
 * @param string string to parse
 * @param line string receiving a copy of the parsed line.
 * @param n line string capacity.
 * NOTE: It is assumed that line is big enough to contains 'n' characters.
 * if the line to be parsed contains more than 'n' characters, only 'n' caracters will be
 * copied, but the whole line will be parsed.
 * @return The remaining of the input string until the next new line char.
 */
char* parse_line(const char *string, char *line, unsigned int n)
{
	const char *s = string;
	if (n == 0) return (char*) string;
	if (s) {
		while (*s && *s != '\n' && *s != '\r' && n > 1) { *line++ = *s++; --n; }
		if (*s == '\0') s = string;
		else {
			while (*s && *s != '\n' && *s != '\r' ) ++s;
			while (*s == '\r' || *s == '\n') ++s;
		}
	}
	*line = '\0';
	return (char*) s;
}

/**
 * @brief Parse a move.
 *
 * @param string String to parse
 * @param board Board where to play the move.
 * @param move A move.
 * @return The remaining of the input string.
 */
char* parse_move(const char *string, const Board *board, Move *move)
{
	*move = MOVE_INIT;

	if (string) {
		char *word = parse_skip_spaces(string);
		int x = string_to_coordinate(word);
		move->x = x;
		move->flipped = board_flip(board, x);
		if (move->flipped && !board_is_occupied(board, x)) {
			return word + 2;
		} else if (board_is_pass(board)) {
			if (x == PASS)
				return word + 2;
			move->x = PASS;
			move->flipped = 0;
		} else {
			move->x = NOMOVE;
			move->flipped = 0;
		}
	}
	return (char*) string;
}

/**
 * @brief Parse a sequence of moves.
 *
 * @param string String to parse.
 * @param board_init Initial board from where to play the moves.
 * @param line A move sequence.
 * @return The remaining of the input string.
 */
char* parse_game(const char *string, const Board *board_init, Line *line)
{
	const char *next;
	Board board;
	Move move;

	board = *board_init;

	while ((next = parse_move(string, &board, &move)) != string || move.x == PASS) {
		if (!line_push(line, move.x)) break; // (a full line)
		board_update(&board, &move);
		string = next;
	}

	return (char *) string;
}

/**
 * @brief Parse a board.
 *
 * @param string String to parse
 * @param board Board.
 * @param player Player to move color.
 * @return The remaining of the input string.
 */
char* parse_board(const char *string, Board *board, int *player)
{
	if (string) {
		int i;
		unsigned long long b = 1;
		const char *s = string;

		board->player = board->opponent = 0;
		for (i = A1; i <= H8; ++i) {
			s = parse_skip_spaces(s);
			switch (tolower((unsigned char) *s)) {
			case 'b':
			case 'x':
			case '*':
				board->player |= b;
				break;
			case 'o':
			case 'w':
				board->opponent |= b;
				break;
			case '-':
			case '.':
				break;
			default:
				return (char*) string;
			}
			++s;
			b <<= 1;
		}
		board_check(board);

		for (; *s; ++s) {
			switch (tolower((unsigned char) *s)) {
			case 'b':
			case 'x':
			case '*':
				*player = BLACK;
				return (char*) s + 1;
			case 'o':
			case 'w':
				board_swap_players(board);
				*player = WHITE;
				return (char*) s + 1;
			default:
				break;
			}
		}
	}

	return (char*) string;
}

/**
 * @brief Parse a boolean.
 *
 * @param string String to parse
 * @param result Boolean output value.
 * @return The remaining of the input string.
 */
char* parse_boolean(const char *string, bool *result)
{
	if (string) {
		char word[6];
		bool r;
		errno = 0;
		string = parse_word(parse_skip_spaces(string), word, 6);
		r = string_to_boolean(word);
		if (errno != EINVAL) *result = r;
	}
	return (char*) string;
}

/**
 * @brief Parse an integer.
 *
 * @param string String to parse
 * @param result Integer output value.
 * @return The remaining of the input string.
 */
char* parse_int(const char *string, int *result)
{
	if (string) {
		char *s = parse_skip_spaces(string);
		char *end = s;
		long long n = 0;

		if (*s) n = strtol(s, &end, 10);

		if (end == s) {
			errno = EINVAL;
			n = *result;
		}
		if (n < INT_MIN) {
			n = INT_MIN;
			errno = ERANGE;
		} else if (n > INT_MAX) {
			n = INT_MAX;
			errno = ERANGE;
		}

		*result = n;

		return end;
	}
	return (char*) string;
}

/**
 * @brief Parse a real number (as a double floating point).
 *
 * @param string String to parse.
 * @param result Real number (double) output value.
 * @return The remaining of the input string.
 */
char* parse_real(const char *string, double *result)
{
	if (string) {
		char *s = parse_skip_spaces(string);
		char *end = s;
		double d = 0;

		if (s) d = strtod(s, &end);

		if (end == s) {
			errno = EINVAL;
			d = *result;
		}

		*result = d;

		return end;
	}
	return (char*) string;
}

/**
 * @brief Parse a command.
 *
 * @param string String to parse.
 * @param cmd Output command string (first word of the input string).
 * @param param Output parameters (remaining of the line).
 * @param size cmd string capacity.
 * @return The remaining of the input string (next line).
 */
char* parse_command(const char *string, char *cmd, char *param, const unsigned int size)
{
	string = parse_word(string, cmd, size);
	string_to_lowercase(cmd);
	if (strcmp(cmd, "set") == 0) {
		string = parse_word(string, cmd, size);
		string_to_lowercase(cmd);
	}
	string = parse_skip_spaces(string);
	if (*string == '=') string = parse_skip_spaces(string + 1);
	string = parse_line(string, param, size);

	return (char*) string;
}

/**
 * @brief Extract the directory of a file path.
 *
 * @param path Full path.
 * @param dir directory part of the path.
 */
void path_get_dir(const char *path, char *dir)
{
	const char *c;

	for (c = path; *c; ++c) ;
#ifdef _WIN32 // (also a backslash, or the colon of a drive)
	for (;c >= path && *c != '/' && *c != '\\' && *c != ':'; --c) ;
#else
	for (;c >= path && *c != '/'; --c) ;
#endif

	while (path <= c) *dir++ = *path++;
	*dir = '\0';
}

/**
 * @brief Add an extension to a string.
 * 
 * @param base Base name.
 * @param ext  Extension (.dat, .ext, .bin, etc.)
 * @param file Output file name (FILENAME_MAX characters at most: a longer base name is cut).
 * @return The output file name.
 */
char* file_add_ext(const char *base, const char *ext, char *file)
{
	const size_t n_ext = MIN(strlen(ext), (size_t) FILENAME_MAX - 1);
	const size_t n_base = MIN(strlen(base), (size_t) FILENAME_MAX - 1 - n_ext);

	memcpy(file, base, n_base);
	memcpy(file + n_base, ext, n_ext);
	file[n_base + n_ext] = '\0';
	return file + n_base + n_ext;
}

#ifdef BOOK_TEST_THREAD_MAX
/* test builds: thread_create() fails, as if the memory was exhausted, while BOOK_TEST_THREAD_MAX threads that it
 * created are not joined yet */
static volatile long thread_test_count = 0;

static bool thread_test_add(const long n)
{
#if defined(_MSC_VER)
	return _InterlockedExchangeAdd(&thread_test_count, n) + n <= BOOK_TEST_THREAD_MAX;
#else
	return __atomic_add_fetch(&thread_test_count, n, __ATOMIC_SEQ_CST) <= BOOK_TEST_THREAD_MAX;
#endif
}
#endif

/**
 * @brief Create a thread.
 *
 * @param thread Thread.
 * @param function Function to run in parallel.
 * @param data Data for the function.
 * @return false if the thread cannot be created (memory exhausted): the function is not run, and
 * there is no thread to join.
 */
bool thread_create(Thread *thread, void* (*function)(void*), void *data)
{
#ifdef BOOK_TEST_THREAD_MAX
	if (!thread_test_add(1)) {
		thread_test_add(-1);
		return false;
	}
#endif
#if defined(__unix__) || (defined(_WIN32) && defined(USE_PTHREAD)) || defined(__APPLE__)
	return pthread_create(thread, NULL, function, data) == 0;
#elif defined(_WIN32)
	DWORD id;
	*thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)(void (*)(void)) function, data, 0, &id);
	return *thread != NULL;
#endif
}

/**
 * @brief Join a thread.
 *
 * Wait for the thread termination. this function also frees the resources
 * allocated by a thread that has not been detached.
 *
 * @param thread Thread.
 */
void thread_join(Thread thread)
{
#if defined(__unix__) || (defined(_WIN32) && defined(USE_PTHREAD)) || defined(__APPLE__)
	pthread_join(thread, NULL);
#elif defined(_WIN32)
	WaitForSingleObject(thread, INFINITE);
	CloseHandle(thread);
#endif
#ifdef BOOK_TEST_THREAD_MAX
	thread_test_add(-1);
#endif
}

/**
 * @brief Run a function on each worker of an array, at the same time.
 *
 * Each worker gets its own thread, except the first one when main_works is set: the calling
 * thread runs it. A worker whose thread cannot be created (memory exhausted) is run by the calling
 * thread, after its own worker: the work is done anyway, with fewer threads. (Such a thread used to
 * be waited for although it did not exist, and its work was not done.)
 *
 * @param function Function to run on each worker (it must not need the other workers to be running).
 * @param worker Array of workers.
 * @param size Size of a worker.
 * @param n Number of workers (MAX_THREADS at most).
 * @param main_works The calling thread runs the first worker (else it only waits for the threads).
 * @param set_cpu Give its own cpu to each thread: cpu i to the worker i (see thread_set_cpu).
 */
void thread_run_workers(void* (*function)(void*), void *worker, const size_t size, const int n, const bool main_works, const bool set_cpu)
{
	Thread thread[MAX_THREADS];
	bool created[MAX_THREADS];
	const int first = main_works ? 1 : 0;
	int i;

	assert(n <= MAX_THREADS);
	for (i = first; i < n; ++i) {
		created[i] = thread_create(thread + i, function, (char*) worker + i * size);
		if (created[i] && set_cpu) thread_set_cpu(thread[i], i);
	}
	if (main_works && n > 0) function(worker);
	for (i = first; i < n; ++i) {
		if (created[i]) thread_join(thread[i]);
		else function((char*) worker + i * size);
	}
}

/**
 * @brief Current thread.
 *
 * @return current thread.
 */
Thread thread_self(void)
{
#if defined(__unix__) || (defined(_WIN32) && defined(USE_PTHREAD)) || defined(__APPLE__)
	return pthread_self();
#elif defined(_WIN32)
	return GetCurrentThread();
#endif
}

/**
 * @brief Choose a single core or cpu to run on, under linux systems, to avoid
 * context changes
 */
void thread_set_cpu(Thread thread, int i)
{
#if defined(__linux__) && defined(CPU_SET)
	cpu_set_t cpu;

	CPU_ZERO(&cpu);
	CPU_SET(i, &cpu);
	pthread_setaffinity_np(thread, sizeof (cpu_set_t), &cpu);
#elif defined(_WIN32) && !defined(USE_PTHREAD)
	SetThreadIdealProcessor(thread, i);
#else
	(void) thread; (void) i;
#endif
}

/**
 * @brief Tell if thread_set_cpu() binds a thread to its cpu.
 *
 * Where it does (linux), a new thread also starts on the cpu of the thread that creates it:
 * with the cpu option, every thread created by the main thread stays on cpu 0 until it is given
 * a cpu. (On Windows thread_set_cpu() only gives a preferred cpu.)
 *
 * @return true if thread_set_cpu() binds the thread.
 */
bool thread_cpu_bound(void)
{
#if defined(__linux__) && defined(CPU_SET)
	return true;
#else
	return false;
#endif
}


/**
 * @brief Get the number of cpus or cores on the machine.
 * @return Cpu/Core number
 */
int get_cpu_number(void)
{
	int n = 0;

#if defined(ANDROID)
	char file[64];
	FILE *f;

	for (n = 0; n < MAX_THREADS; ++n) {
		sprintf(file, "/sys/devices/system/cpu/cpu%d", n);
		f = fopen(file, "r");
		if (f == NULL) {
			break;
		}
		fclose(f);
	}

#elif defined(_SC_NPROCESSORS_ONLN)

	n = sysconf(_SC_NPROCESSORS_ONLN);

#elif defined(_WIN32)

	SYSTEM_INFO info;

	GetSystemInfo(&info);
	n = info.dwNumberOfProcessors;

#elif defined(__APPLE__) /* should also works on any bsd system */

	int mib[4];
	size_t len; 

	mib[0] = CTL_HW;
	mib[1] = HW_AVAILCPU;
	sysctl(mib, 2, &n, &len, NULL, 0);
	if (n < 1) {
	     mib[1] = HW_NCPU;
	     sysctl( mib, 2, &n, &len, NULL, 0 );
	}

#endif

	if (n < 1) n = 1;

	return n;
}

/**
 * @brief Get the size of the physical memory.
 * @return Memory size in bytes, 0 if unknown.
 */
unsigned long long get_physical_memory(void)
{
#if defined(_WIN32)
	MEMORYSTATUSEX status;

	status.dwLength = sizeof (status);
	if (GlobalMemoryStatusEx(&status)) return status.ullTotalPhys;
	return 0;

#elif defined(_SC_PHYS_PAGES) && defined(_SC_PAGESIZE)
	long pages = sysconf(_SC_PHYS_PAGES), page_size = sysconf(_SC_PAGESIZE);

	if (pages > 0 && page_size > 0) return (unsigned long long) pages * page_size;
	return 0;

#else
	return 0;
#endif
}

/**
 * @brief Pseudo-random number generator.
 *
 * A good pseudo-random generator (derived from rand48 or Java's one) to set the
 * hash codes.
 * @param random Pseudo-Random generator state.
 * @return a 64-bits pseudo-random unsigned int integer.
 */
unsigned long long random_get(Random *random)
{
	const unsigned long long MASK48 = 0xFFFFFFFFFFFFull;
	const unsigned long long A = 0x5DEECE66Dull;
	const unsigned long long B = 0xBull;
	register unsigned long long r;

	random->x = ((A * random->x + B) & MASK48);
	r = random->x >> 16;
	random->x = ((A * random->x + B) & MASK48);
	return (r << 32) | (random->x >> 16);
}

/**
 * @brief Pseudo-random number seed.
 *
 * @param random Pseudo-Random generator state.
 * @param seed a 64-bits integer used as seed.
 */
void random_seed(Random *random, const unsigned long long seed)
{
	const unsigned long long MASK48 = 0xFFFFFFFFFFFFull;
	random->x = (seed & MASK48);
}
