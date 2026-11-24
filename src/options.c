/**
 * @file options.c
 *
 * Options reader.
 *
 * @date 1998 - 2023
 * @author Richard Delorme
 * @version 4.5
 */

#include "options.h"
#include "stats.h"
#include "util.h"
#include "search.h"

#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>

/** global options with default value */
Options options = {
	21, // hash table size (2^21 * 24 * 1.125 = 57MB)

	{0,-2,-3}, // inc_sort_depth

	1, // n_task (will be set to system available cpus at run-time)
	false, // cpu_affinity

	1, // verbosity
	0, // noise
	80, // width
	false, // echo
	false, // info
	false, // debug cassio
	true, // transgress cassio

	18, // level
	TIME_MAX, // infinite time
	EDAX_FIXED_LEVEL, // play-type
	true, // can ponder
	-1, // depth
	-1, // selectivity

	3, // mode

	10000000,  // speed (default = 10e6)
	0,         // nps (default = 0)

	SCORE_MIN, // alpha
	SCORE_MAX, // beta

	false, // all_best

	NULL, // evaluation function's weights file.

	NULL, // book file
	true,            // book usage allowed
	0,               // book randomness

	NULL, // ggs host name
	NULL, // ggs login name
	NULL, // ggs password
	NULL, // port
	1,    // open

	0.25, // probcut depth reduction (/2)
	false, // pv debug
	false, // pv check
	false, // pv guess

	NULL, // game file.

	NULL, // search log file.
	NULL, // ui log file.
	NULL, // ggs log file.

	NULL, // edax name

	false, //auto start
	false, //auto store
	false, //auto learn
	false, //auto quit
	0, //repeat
	60, // minutes between timed book saves
	1, // save after every productive deviate round by default
	true, // save the merged book after book merge
	false, // hash table size set by hash-table-size (auto: from the thread count and the memory size)
	1, // book positions expanded at the same time
	false, // speed given by the user (else measured)
	false, // level given by the user (else no level cap with a time control)
	0, // probcut model: standard
	0, // book depth: auto (the depth of the loaded book)
	0, // games learned at the same time: auto
	true, // save the book to <book-file>.store after book store
	1, // passes of book leaf recalculate
	false, // book random numbers from the clock
	0, // book seed (when set)
	false, // book functions as fast as possible (not reproducible)
	true, // book reduce: fewer buckets after it
	true, // book reduce: the count of book reducecount used
	1, // book reducecount: a table of the buckets (twice as fast as the binary search with a book of 700 million positions)
	0, // book reducecount: walk by accumulated loss
	// search-b (2026-11-22): all off, the search of v4.5.5-nikque.32
	// v4.5.5-nikque.33: the time of Edax a move, stretched after a fail low (time_ext), and the sort with the evaluation from
	// the depth 7: fixed, with no option (as in Heath; they change the time and the order of the moves only)
	.time_ext = 1, .time_ext_max = 2.0, .time_ext_h = 0.5,
	.time_stop_master = true, // (a fault of nikque.32 and before: see search_check_timeout)
	.sort_eval_shift = {1, 0, 0}, .sort_min_depth = 7,
	.split_probe = true,
	.pc_r = 1.0, .pc_slope = 1.0, .pc_slope_limit = 12, .pc_slope_min = 0, .pc_end = {0.0}, .pc_dev_d0 = 0, .pc_dev_k = 0.0, .pc_dev_fmin = 1.0, .pc_dev_root = true, .pc_mob = {{0.0}}, .pc_gate = true,
	.lmr = 0,
	.lazy = 0, .lazy_min = 16,
};

/**
 * @brief Print options usage.
 */
void options_usage(void)
{
	fprintf(stderr, "\nCommon options:\n"
		"  -?|help                       show this message.\n"
		"  -o|option-file                read options from this file.\n"
		"  -v|version                    display the version number.\n"
		"  -name <string>                set Edax name to <string>.\n"
		"  -verbose <n>                  verbosity level.\n"
		"  -q                            silent mode (eq. -verbose 0).\n"
		"  -vv                           very verbose mode (eq. -verbose 2).\n"
		"  -noise <n>                    noise level (print search output from ply <n>).\n"
		"  -width <n>                    line width.\n"
		"  -h|hash-table-size <nbits>    hash table size (2^nbits entries), or auto.\n"
		"  -n|n-tasks <n|auto>           search in parallel using n tasks (auto: the number of logical CPUs).\n"
		"  -cpu                          search using 1 cpu/thread.\n"
#ifdef __APPLE__
		"\nCassio protocol options:\n"
		"  -debug-cassio                 print extra-information in cassio.\n"
		"  -follow-cassio                follow more closely cassio requests.\n"
		"\nOptions unavailable to Cassio protocol\n:"
#endif
		"  -l|level <n>                  search using limited depth.\n"
		"  -t|game-time <n>              search using limited time per game.\n"
		"  -move-time <n>                search using limited time per move.\n"
		"                                (with a time, -l caps the level; without -l, no cap)\n"
		"  -speed <n|auto>               search speed (nodes/s) used to share the time; auto: measured.\n"
		"  -ponder <on/off>              search during opponent time.\n"
		"  -probcut-model <standard|refit> probcut error model (refit: experimental, see README).\n");
	fprintf(stderr, "  -time-ext-max <x>             longest stretch of the time of a move (2.0).\n"
		"  -time-ext-h <x>               part of the next iteration that must fit in the time left to start it (0.5).\n"
		"  -time-stop-master <on/off>    split searches obey the extended time of the master (on).\n"
		"  -split-probe <on/off>         split the searches of probcut between threads (on).\n"
		"  -pc-r <x>                     probcut: factor of the margins (1).\n"
		"  -pc-end <x|e:x,...>           probcut: factor of the searches to the end, by empties.\n"
		"  -pc-slope <on|off|a[,limit[,min]]> probcut: slope of the searches to the end against their probes\n"
		"                                (off: 1, the default; on: 1.3,12, the search of Heath; with a slope, an\n"
		"                                unset pc-end is 0.85).\n"
		"  -pc-dev <on|off|d0,k,fmin>    probcut: narrower off the principal variation (off, the default;\n"
		"                                on: 2,0.04,0.75, the search of Heath).\n"
		"  -pc-dev-root <on/off>         probcut: the later moves of the root are off the principal variation (on).\n"
		"  -pc-mob <on|off|a,b,c[,...]>  probcut: factor of the mobility, one set or three by empties (off, the\n"
		"                                default; on: 2.219,-0.184,-0.350,0,0,0,0,0,0, the search of Heath).\n"
		"  -pc-gate <on/off>             probcut: shallow searches chosen by the evaluation (on).\n"
		"  -lmr <0|1|2>                  late move reductions (0: off).\n"
		"  -lazy <n|auto>                helper threads at the root (0: off, default); auto: 1 in the\n"
		"                                midgame with 4 threads or more.\n"
		"  -lazy-min <n>                 lowest depth of an iteration with helpers (16).\n"
		"  -eval-file                    read eval weight from this file.\n");
	fprintf(stderr, "  -book-file                    load opening book from this file.\n"
		"  -book-usage <on/off>          play from the opening book.\n"
		"  -book-depth <n|auto>          depth of the opening book at startup (auto: the depth of the loaded book).\n"
		"  -book-randomness <n>          play various but worse moves from the opening book.\n"
		"  -auto-start <on/off>          automatically restart a new game.\n"
		"  -auto-swap <on/off>           automatically Edax's color between games\n"
		"  -auto-store <on/off>          automatically save played games\n"
		"  -game-file <file>             file to store all played game/s.\n"
		"  -book-save-interval <minutes> minutes between timed book saves (0 disables them).\n"
		"  -book-deviate-save-rounds <n> save deviate progress every n rounds; 0 means completion only.\n"
		"  -book-merge-auto-save <on/off> save the book to <book-file>.mrg after book merge.\n"
		"  -book-expand-tasks <n|auto>   expand n book positions at the same time (n-tasks / n threads each).\n"
		"  -book-store-tasks <n|auto>    learn n games at the same time (n-tasks / n threads each); auto (default): n-tasks; 1: as before.\n"
		"  -book-store-auto-save <on/off> save the book to <book-file>.store after book store and book learn (default on).\n"
		"  -book-leaf-recalculate-rounds <n> passes of book leaf recalculate (2, 3, 4) at most; a pass that changes no leaf is the last (default 1).\n"
		"  -book-seed <n|auto>           seed of the random choices of the book (auto, the default: from the clock).\n"
		"  -book-reproducible <on/off>   book functions give the same book from a run to the next (default off).\n"
		"  -book-reduce-shrink <on/off>  book reduce: fewer buckets when most positions are removed (default on).\n"
		"  -book-reduce-reuse <on/off>   book reduce: use the positions that book reducecount found just before (default on).\n"
		"  -book-reduce-index <binary/table> book reducecount: find a position from its number by binary search or a table (default).\n"
		"  -book-reduce-walk <loss/group> book reducecount with threads: walk by loss (default) or in the order of the groups.\n"
		"  -search-log-file <file>       file to store search detailed output/s.\n"
		"  -ui-log-file <file>           file to store input/output to the (U)ser (I)nterface.\n");

	exit(EXIT_SUCCESS);
}

/** true while reading a file of default settings (edax.ini, config.ini) */
static bool reading_defaults = false;

/**
 * @brief Read the integer value of an option.
 *
 * @param option Option name (for the message).
 * @param value Option value: a whole number (spaces after it are accepted).
 * @param current Current value of the option.
 * @return the value, or the current value (with a warning) if the string is not a number.
 */
static int option_int(const char *option, const char *value, const int current)
{
	char *end;
	const long n = strtol(value, &end, 10);

	if (end != value) {
		while (*end == ' ' || *end == '\t') ++end;
		if (*end == '\0') return (int) MAX(INT_MIN, MIN(INT_MAX, n));
	}
	warn("%s: \"%s\" is not a number; ignored\n", option, value);
	return current;
}

/** @brief Same as option_int(), where the word "auto" gives auto_value. */
static int option_int_or_auto(const char *option, const char *value, const int current, const int auto_value)
{
	return strcmp(value, "auto") == 0 ? auto_value : option_int(option, value, current);
}

/**
 * @brief Read book-seed: a whole number from 0 to 2^64 - 1, or auto (the clock).
 *
 * @param option Option name (for the message).
 * @param value Option value.
 */
static void option_seed(const char *option, const char *value)
{
	char *end;
	unsigned long long n;

	if (strcmp(value, "auto") == 0) { options.book_seed_set = false; return; }
	errno = 0;
	n = strtoull(value, &end, 10);
	if (end != value && *value != '-' && errno == 0) {
		while (*end == ' ' || *end == '\t') ++end;
		if (*end == '\0') { options.book_seed = n; options.book_seed_set = true; return; }
	}
	errno = 0;
	warn("%s: \"%s\" is not a number from 0 to %llu, or auto; ignored\n", option, value, ULLONG_MAX);
}

/**
 * @brief Set a string option: the previous string is released (it was lost each time an option
 * was set again, by a second file of settings or by edax_set_option).
 *
 * @param option Option to set.
 * @param value New value (copied).
 */
static void option_string(char **option, const char *value)
{
	char *s = string_duplicate(value);
	free(*option);
	*option = s;
}

/**
 * @brief Read the value of an on/off option (on, off, true, false, yes, no, 1, 0).
 *
 * @param option Option name (for the message).
 * @param value Option value.
 * @param result Option to set (unchanged, with a warning, if the value is not one of these words).
 */
static void option_boolean(const char *option, const char *value, bool *result)
{
	char word[6];
	bool r;

	parse_word(value, word, sizeof word);
	errno = 0;
	r = string_to_boolean(word);
	if (errno == EINVAL) {
		warn("%s: \"%s\" is not on or off; ignored\n", option, value);
		errno = 0;
	} else *result = r;
}

/**
 * @brief Read the value of an option that is one of two words (the first one is 0, the second one 1).
 *
 * @param option Option name (for the message).
 * @param value Option value.
 * @param word_0 Word of the value 0.
 * @param word_1 Word of the value 1.
 * @param result Option to set (unchanged, with a warning, if the value is neither word).
 */
static void option_choice(const char *option, const char *value, const char *word_0, const char *word_1, int *result)
{
	if (strcmp(value, word_0) == 0) *result = 0;
	else if (strcmp(value, word_1) == 0) *result = 1;
	else warn("%s: unknown value \"%s\" (%s or %s)\n", option, value, word_0, word_1);
}

/**
 * @brief Read an option.
 *
 * @param option Option name.
 * @param value Option value.
 * @return The number of arguments read (0, 1 or 2).
 */
/**
 * @brief Read pc-slope: slope[,limit[,fewest empties]] (slope 1 to 2, limit 1 to 64, empties 0 to 60).
 */
static void option_pc_slope(const char *option, const char *value)
{
	double a = 1.0;
	int limit = 12, low = 0, used = 0;
	if (value && strcmp(value, "on") == 0) value = "1.3,12"; // (the search of Heath)
	if (value && strcmp(value, "off") == 0) value = "1";
	bool ok = value && sscanf(value, "%lf%n", &a, &used) == 1;
	const char *p = ok ? value + used : "";
	if (ok && *p == ',') {
		ok = sscanf(p, ",%d%n", &limit, &used) == 1;
		if (ok) p += used;
		if (ok && *p == ',') {
			ok = sscanf(p, ",%d%n", &low, &used) == 1;
			if (ok) p += used;
		}
	}
	if (!ok || *p || !(a >= 1.0 && a <= 2.0) || limit < 1 || limit > 64 || low < 0 || low > 60) {
		warn("option %s needs slope[,limit[,empties]] (slope 1 to 2, limit 1 to 64, empties 0 to 60); ignored\n", option);
		return;
	}
	options.pc_slope = a;
	options.pc_slope_limit = limit;
	options.pc_slope_min = low;
}

/**
 * @brief Read the factors of pc-end: one factor for all the empties, or empties:factor,... (increasing empties;
 * straight lines between them, the first and the last factor beyond them).
 */
static void option_pc_end(const char *option, const char *value, double table[61])
{
	int e[61], n = 0, used = 0;
	double f[61], x;
	char end;
	const char *p = value;

	if (p && sscanf(p, "%lf%c", &x, &end) == 1) {
		e[n] = 0; f[n++] = x;
	} else {
		while (p && n < 61 && sscanf(p, "%d:%lf%n", e + n, f + n, &used) == 2) {
			if (e[n] < 0 || e[n] > 60 || (n && e[n] <= e[n - 1])) { n = 0; break; }
			++n;
			p += used;
			if (*p != ',') break;
			++p;
			if (*p == 0) { n = 0; break; } // (a comma with nothing after it)
		}
		if (n && *p) n = 0;
	}
	bool ok = (n > 0);
	for (int i = 0; i < n; ++i) if (!(f[i] >= 0.1 && f[i] <= 10.0)) ok = false;
	if (!ok) {
		warn("option %s needs a factor, or empties:factor,... (factors 0.1 to 10, increasing empties); ignored\n", option);
		return;
	}
	for (int i = 0, k = 0; i <= 60; ++i) {
		while (k + 1 < n && e[k + 1] <= i) ++k;
		if (i <= e[0]) table[i] = f[0];
		else if (k + 1 >= n) table[i] = f[n - 1];
		else table[i] = f[k] + (f[k + 1] - f[k]) * (i - e[k]) / (e[k + 1] - e[k]);
	}
}

int options_read(const char *option, const char *value)
{
	int read = 0;

	if (option == NULL) return read;
	while (*option == '-') ++option;
	if (*option == '\0' || *option == '%' || *option == '#') return read;

	read = 1;
	if (strcmp(option, "vv") == 0) options.verbosity = 2;
	else if (strcmp(option, "q") == 0) options.verbosity = 0;
	else if (strcmp(option, "info") == 0) options.info = true;
	else if (strcmp(option, "debug-cassio") == 0) options.debug_cassio = true;
	else if (strcmp(option, "follow-cassio") == 0) options.transgress_cassio = false;
	else if (strcmp(option, "?") == 0 || strcmp(option, "help") == 0) usage();
	else if (strcmp(option, "cpu") == 0) options.cpu_affinity = true;
	else {
		read = 0;
		if (value == NULL || *value == '\0') return read;
		read = 2;
		if (strcmp(option, "verbose") == 0) options.verbosity = option_int(option, value, options.verbosity);
		else if (strcmp(option, "noise") == 0) options.noise = option_int(option, value, options.noise);
		else if (strcmp(option, "width") == 0) options.width = option_int(option, value, options.width);

		else if (strcmp(option, "h") == 0  || strcmp(option, "hash-table-size") == 0) {
			options.hash_table_auto = (strcmp(value, "auto") == 0);
			if (!options.hash_table_auto) options.hash_table_size = option_int(option, value, options.hash_table_size);
		}
		else if (strcmp(option, "n") == 0 || strcmp(option, "n-tasks") == 0) options.n_task = option_int_or_auto(option, value, options.n_task, get_cpu_number());
		else if (strcmp(option, "l") == 0 || strcmp(option, "level") == 0) {
			options.level = option_int(option, value, options.level);
			if (!reading_defaults) {	// a level of edax.ini / config.ini is only the initial level
				options.level_set = true;
				options.play_type = EDAX_FIXED_LEVEL;
			}
		} else if (strcmp(option, "d") == 0 || strcmp(option, "depth") == 0) {
			options.depth = option_int(option, value, options.depth);
			options.play_type = EDAX_FIXED_LEVEL;
		} else if (strcmp(option, "selectivity") == 0) {
			options.selectivity = option_int(option, value, options.selectivity);
			options.play_type = EDAX_FIXED_LEVEL;
		} else if (strcmp(option, "t") == 0 || strcmp(option, "game-time") == 0) {
			options.time = string_to_time(value);
			options.play_type = EDAX_TIME_PER_GAME;
		} else if (strcmp(option, "move-time") == 0) {
			options.time = string_to_time(value);
			options.play_type = EDAX_TIME_PER_MOVE;
		} else if (strcmp(option, "alpha") == 0) options.alpha = option_int(option, value, options.alpha);
		else if (strcmp(option, "beta") == 0) options.beta = option_int(option, value, options.beta);
		else if (strcmp(option, "all-best") == 0) option_boolean(option, value, &options.all_best);

		else if (strcmp(option, "o") == 0 || strcmp(option, "option-file") == 0) options_parse(value);
		else if (strcmp(option, "speed") == 0) {
			options.speed_set = (strcmp(value, "auto") != 0);
			if (options.speed_set) options.speed = string_to_real(value, options.speed);
		}
		else if (strcmp(option, "nps") == 0) options.nps = 0.001 * string_to_real(value, options.nps);
		else if (strcmp(option, "ponder") == 0) option_boolean(option, value, &options.can_ponder);
		else if (strcmp(option, "mode") == 0) options.mode = option_int(option, value, options.mode);

		else if (strcmp(option, "inc-pvnode-sort-depth") == 0) options.inc_sort_depth[PV_NODE] = option_int(option, value, options.inc_sort_depth[PV_NODE]);
		else if (strcmp(option, "inc-cutnode-sort-depth") == 0) options.inc_sort_depth[CUT_NODE] = option_int(option, value, options.inc_sort_depth[CUT_NODE]);
		else if (strcmp(option, "inc-allnode-sort-depth") == 0) options.inc_sort_depth[ALL_NODE] = option_int(option, value, options.inc_sort_depth[ALL_NODE]);

		else if (strcmp(option, "ggs-host") == 0) option_string(&options.ggs_host, value);
		else if (strcmp(option, "ggs-login") == 0) options.ggs_login = string_duplicate(value); // (kept: the GGS client holds this pointer)
		else if (strcmp(option, "ggs-password") == 0) option_string(&options.ggs_password, value);
		else if (strcmp(option, "ggs-port") == 0) option_string(&options.ggs_port, value);
		else if (strcmp(option, "ggs-open") == 0) option_boolean(option, value, &options.ggs_open);

		else if (strcmp(option, "probcut-d") == 0) parse_real(value, &options.probcut_d);
		else if (strcmp(option, "probcut-model") == 0) option_choice(option, value, "standard", "refit", &options.probcut_model);

		// search-b (2026-11-22)
		else if (strcmp(option, "time-ext-max") == 0) parse_real(value, &options.time_ext_max);
		else if (strcmp(option, "time-ext-h") == 0) parse_real(value, &options.time_ext_h);
		else if (strcmp(option, "time-stop-master") == 0) option_boolean(option, value, &options.time_stop_master);
		else if (strcmp(option, "split-probe") == 0) option_boolean(option, value, &options.split_probe);
		else if (strcmp(option, "pc-r") == 0) parse_real(value, &options.pc_r);
		else if (strcmp(option, "pc-end") == 0) option_pc_end(option, value, options.pc_end);
		else if (strcmp(option, "pc-slope") == 0) option_pc_slope(option, value);
		else if (strcmp(option, "pc-gate") == 0) option_boolean(option, value, &options.pc_gate);
		else if (strcmp(option, "pc-dev") == 0) {
			int d0; double k, fmin;
			if (value && strcmp(value, "on") == 0) value = "2,0.04,0.75"; // (the search of Heath)
			if (value && strcmp(value, "off") == 0) value = "0,0,1";
			if (value && sscanf(value, "%d,%lf,%lf", &d0, &k, &fmin) == 3) { options.pc_dev_d0 = d0; options.pc_dev_k = k; options.pc_dev_fmin = fmin; }
			else warn("option %s needs d0,k,fmin\n", option);
		}
		else if (strcmp(option, "pc-dev-root") == 0) option_boolean(option, value, &options.pc_dev_root);
		else if (strcmp(option, "pc-mob") == 0) {
			double m[9];
			char end;
			if (value && strcmp(value, "on") == 0) value = "2.219,-0.184,-0.350,0,0,0,0,0,0"; // (the search of Heath)
			if (value && strcmp(value, "off") == 0) value = "0,0,0";
			const int n = value ? sscanf(value, "%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf%c", m, m + 1, m + 2, m + 3, m + 4, m + 5, m + 6, m + 7, m + 8, &end) : 0;
			if (n == 3 || n == 9) for (int i = 0; i < 9; ++i) options.pc_mob[i / 3][i % 3] = m[n == 3 ? i % 3 : i];
			else warn("option %s needs a,b,c or three times a,b,c\n", option);
		}
		else if (strcmp(option, "lmr") == 0) options.lmr = option_int(option, value, options.lmr);
		else if (strcmp(option, "lazy") == 0) options.lazy = option_int_or_auto(option, value, options.lazy, LAZY_AUTO);
		else if (strcmp(option, "lazy-min") == 0) options.lazy_min = option_int(option, value, options.lazy_min);

		else if (strcmp(option, "pv-debug") == 0) option_boolean(option, value, &options.pv_debug);
		else if (strcmp(option, "pv-check") == 0) option_boolean(option, value, &options.pv_check);
		else if (strcmp(option, "pv-guess") == 0) option_boolean(option, value, &options.pv_guess);

		else if (strcmp(option, "game-file") == 0) option_string(&options.game_file, value);

		else if (strcmp(option, "eval-file") == 0) option_string(&options.eval_file, value);	// 11/13/2015

		else if (strcmp(option, "book-file") == 0) {
			// the book commands add an extension (".store", ".dev2", ...) to this name, in buffers of FILENAME_MAX characters
			if (strlen(value) > FILENAME_MAX - 8) warn("the name of the book file is too long: ignored\n");
			else option_string(&options.book_file, value);
		}
		else if (strcmp(option, "book-usage") == 0) option_boolean(option, value, &options.book_allowed);
		else if (strcmp(option, "book-randomness") == 0) options.book_randomness = option_int(option, value, options.book_randomness);

		else if (strcmp(option, "search-log-file") == 0) option_string(&options.search_log_file, value);
		else if (strcmp(option, "ui-log-file") == 0) option_string(&options.ui_log_file, value);
		else if (strcmp(option, "ggs-log-file") == 0) option_string(&options.ggs_log_file, value);

		else if (strcmp(option, "name") == 0) option_string(&options.name, value);
		else if (strcmp(option, "echo") == 0) option_boolean(option, value, &options.echo);

		else if (strcmp(option, "auto-start") == 0) option_boolean(option, value, &options.auto_start);
		else if (strcmp(option, "auto-store") == 0) option_boolean(option, value, &options.auto_store);
		else if (strcmp(option, "auto-swap") == 0) option_boolean(option, value, &options.auto_swap);
		else if (strcmp(option, "auto-quit") == 0) option_boolean(option, value, &options.auto_quit);
		else if (strcmp(option, "repeat") == 0) options.repeat = option_int(option, value, options.repeat);
		else if (strcmp(option, "book-save-interval") == 0) options.book_save_interval = option_int(option, value, options.book_save_interval);
		else if (strcmp(option, "book-deviate-save-rounds") == 0) options.book_deviate_save_rounds = option_int(option, value, options.book_deviate_save_rounds);
		else if (strcmp(option, "book-depth") == 0) options.book_depth = option_int_or_auto(option, value, options.book_depth, 0);	// 0 = auto
		else if (strcmp(option, "book-expand-tasks") == 0) options.book_expand_tasks = option_int_or_auto(option, value, options.book_expand_tasks, 0);	// 0 = auto
		else if (strcmp(option, "book-store-tasks") == 0) options.book_store_tasks = option_int_or_auto(option, value, options.book_store_tasks, 0);	// 0 = auto
		else if (strcmp(option, "book-merge-auto-save") == 0) option_boolean(option, value, &options.book_merge_auto_save);
		else if (strcmp(option, "book-store-auto-save") == 0) option_boolean(option, value, &options.book_store_auto_save);
		else if (strcmp(option, "book-leaf-recalculate-rounds") == 0) options.book_leaf_recalculate_rounds = option_int(option, value, options.book_leaf_recalculate_rounds);
		else if (strcmp(option, "book-seed") == 0) option_seed(option, value);
		else if (strcmp(option, "book-reproducible") == 0) option_boolean(option, value, &options.book_reproducible);
		else if (strcmp(option, "book-reduce-shrink") == 0) option_boolean(option, value, &options.book_reduce_shrink);
		else if (strcmp(option, "book-reduce-reuse") == 0) option_boolean(option, value, &options.book_reduce_reuse);
		else if (strcmp(option, "book-reduce-index") == 0) option_choice(option, value, "binary", "table", &options.book_reduce_index);
		else if (strcmp(option, "book-reduce-walk") == 0) option_choice(option, value, "loss", "group", &options.book_reduce_walk);

		else read = 0;
	}

	if (read) {
		info("<set option %s %s>\n", option, value);
	}

	return read;
}


/**
 * @brief Trim the spaces at both ends of a string (in place).
 * @param s String.
 * @return The trimmed string.
 */
static char* settings_trim(char *s)
{
	char *e;
	while (*s == ' ') ++s;
	e = s + strlen(s);
	while (e > s && e[-1] == ' ') --e;
	*e = '\0';
	return s;
}

/**
 * @brief Parse a line of a settings file and apply it.
 *
 * The syntax is tolerant, to make the files easy to edit by hand:
 *  - "name = value", "name=value", "name value" and "set name value" are the same;
 *  - tabs, full-width spaces and a full-width equal sign are accepted, and a UTF-8 BOM is skipped;
 *  - names ignore the case, and spaces, '_' and '-' are the same ("book depth" = "book_depth" = "book-depth");
 *  - the values on/off/true/false/yes/no/auto/standard/refit ignore the case;
 *  - with '=', the value is the rest of the line (a file name may contain spaces);
 *  - '#' starts a comment; an empty line is ignored; an unknown name is reported.
 *
 * @param line Line to parse (modified).
 * @param file File name (for the messages).
 * @param n_line Line number (for the messages).
 */
static void settings_parse_line(char *line, const char *file, int n_line)
{
	static const char *words[] = { "auto", "on", "off", "true", "false", "yes", "no", "standard", "refit", NULL };
	char *s = line, *key, *value, *eq, *p, *q;
	int i, j;

	// normalize the characters: BOM, full-width space and equal sign, tabs, CR
	if ((unsigned char) s[0] == 0xEF && (unsigned char) s[1] == 0xBB && (unsigned char) s[2] == 0xBF) s += 3;
	for (i = j = 0; s[i]; ) {
		const unsigned char c = (unsigned char) s[i];
		if (c == 0xE3 && (unsigned char) s[i + 1] == 0x80 && (unsigned char) s[i + 2] == 0x80) { s[j++] = ' '; i += 3; }
		else if (c == 0xEF && (unsigned char) s[i + 1] == 0xBC && (unsigned char) s[i + 2] == 0x9D) { s[j++] = '='; i += 3; }
		else if (c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f') { s[j++] = ' '; ++i; }
		else s[j++] = s[i++];
	}
	s[j] = '\0';
	if ((p = strchr(s, '#')) != NULL) *p = '\0';	// comment
	s = settings_trim(s);
	if (*s == '\0' || *s == '%') return;

	// split into name and value
	if ((eq = strchr(s, '=')) != NULL) {
		*eq = '\0';
		key = settings_trim(s);
		value = settings_trim(eq + 1);
	} else if ((p = strrchr(s, ' ')) != NULL) {
		*p = '\0';
		key = settings_trim(s);
		value = settings_trim(p + 1);
	} else {
		key = s;
		value = s + strlen(s);
	}

	// name: lower case, spaces and '_' as '-', without a leading "set" or '-'
	for (p = q = key; *p; ++p) {
		char c = (char) tolower((unsigned char) *p);
		if (c == ' ' || c == '_') c = '-';
		if (c == '-' && (q == key || q[-1] == '-')) continue;
		*q++ = c;
	}
	while (q > key && q[-1] == '-') --q;
	*q = '\0';
	if (strncmp(key, "set-", 4) == 0) key += 4;
	if (*key == '\0') return;

	// value: the words of the settings ignore the case
	for (i = 0; words[i]; ++i) {
		for (j = 0; words[i][j] && tolower((unsigned char) value[j]) == words[i][j]; ++j) ;
		if (words[i][j] == '\0' && value[j] == '\0') { strcpy(value, words[i]); break; }
	}

	if (options_read(key, value) == 0) {
		warn("%s:%d: unknown or incomplete setting \"%s\" ignored\n", file, n_line, key);
	}
}

/**
 * @brief parse options from a file
 *
 * @param file Option file name.
 */
void options_parse(const char *file)
{
	// an option file may read another one ("option-file"): a file that reads itself, or a loop of files, is
	// stopped (up to v4.5.5-nikque.15 the stack and the open files were exhausted)
	static int depth = 0;
	char *line;
	FILE *f;

	if (depth >= 16) {
		warn("%s: too many option files read from one another\n", file);
		return;
	}
	f = fopen(file, "r");
	if (f != NULL) {

		int n_line = 0;
		++depth;
		while ((line = string_read_line(f)) != NULL) {
			settings_parse_line(line, file, ++n_line);
			free(line);
		}
		--depth;

		fclose(f);
	}
}

/**
 * @brief parse default settings from a file (edax.ini, config.ini).
 *
 * Same as options_parse(), except that a level only sets the initial level: it does not
 * count as a level given by the user (no level cap with a time control, see play_level()).
 *
 * @param file Option file name.
 */
void options_parse_defaults(const char *file)
{
	reading_defaults = true;
	options_parse(file);
	reading_defaults = false;
}

/**
 * @brief Choose the hash table size from the number of search threads and the memory size.
 *
 * More threads fill the table faster, so it grows with the thread count (one bit when it is
 * multiplied by 4: 21 for 1-3 threads, 22 for 4-15, 23 for 16-63; the gain measured with larger
 * tables was small). The three tables (main + pv + shallow, 27 bytes per main entry) use at most
 * 1/32 of the memory.
 *
 * @param n_task Number of search threads.
 * @return hash table size (in number of bits).
 */
int hash_table_size_auto(const int n_task)
{
	const unsigned long long memory = get_physical_memory();
	int size = 21, n;

	for (n = 4; n <= n_task; n *= 4) ++size;
	if (size > 25) size = 25;
	if (memory) {
		while (size > 21 && (27ULL << size) > memory / 32) --size;
	}
	return size;
}

/**
 * @brief Keep options between realistic values.
 */
void options_bound(void) 
{
	int tmp;
	int max_threads;

	max_threads = MIN(get_cpu_number(), MAX_THREADS);
	BOUND(options.n_task, 1, max_threads, "n-tasks");

	// G2 (11/24): measured with -pc-slope 1.3,12 -pc-end 0.85 together; with a slope, an unset pc-end is 0.85
	if (options.pc_slope > 1.0) {
		bool set = false;
		for (int e = 0; e <= 60; ++e) if (options.pc_end[e] > 0.0) set = true;
		if (!set) for (int e = 0; e <= 60; ++e) options.pc_end[e] = 0.85;
	}

	if (options.hash_table_auto) options.hash_table_size = hash_table_size_auto(options.n_task);
	if (sizeof (void*) == 4) {
		BOUND(options.hash_table_size, 10, 25, "hash-table-size");	// 51KB to 1.7GB
	} else {
		BOUND(options.hash_table_size, 10, 30, "hash-table-size");	// 51KB to 53GB
	}

	// 0 = auto. The limit is the largest n-tasks, not its current value: a number given by the user is kept when
	// n-tasks is lowered then raised again (it is capped by n-tasks where it is used).
	if (options.book_expand_tasks != 0) BOUND(options.book_expand_tasks, 1, max_threads, "book-expand-tasks");
	if (options.book_store_tasks != 0) BOUND(options.book_store_tasks, 1, max_threads, "book-store-tasks");
	BOUND(options.book_depth, 0, 60, "book-depth");	// 0 = auto
	BOUND(options.verbosity, 0, 4, "verbosity");
	BOUND(options.mode, 0, 3, "mode"); // (an index of the names of the modes: up to v4.5.5-nikque.15 not bounded)
	BOUND(options.noise, 0, 60, "noise");
	BOUND(options.width, 3, 250, "width");
	BOUND(options.level, 0, 60, "level");
	BOUND(options.time, 1000, TIME_MAX, "time");
	BOUND(options.book_save_interval, 0, 525600, "book-save-interval");
	BOUND(options.book_deviate_save_rounds, 0, 1000000, "book-deviate-save-rounds");
	BOUND(options.book_leaf_recalculate_rounds, 1, 1000000, "book-leaf-recalculate-rounds");

	// search-b (2026-11-22)
	BOUND(options.lmr, 0, 2, "lmr");
	BOUND(options.lazy, LAZY_AUTO, MAX_THREADS - 1, "lazy");
	BOUND(options.lazy_min, 8, 60, "lazy-min");
	options.time_ext_max = MIN(4.0, MAX(1.0, options.time_ext_max));
	options.time_ext_h = MIN(2.0, MAX(0.0, options.time_ext_h));
	options.pc_r = MIN(10.0, MAX(0.1, options.pc_r));
	options.pc_dev_fmin = MIN(1.0, MAX(0.1, options.pc_dev_fmin));

	BOUND(options.alpha, SCORE_MIN, SCORE_MAX, "alpha");
	BOUND(options.beta, SCORE_MIN, SCORE_MAX, "beta");

	BOUND(options.speed, 1e5, 1e12, "speed");

	if (options.alpha > options.beta) {
		fprintf(stderr, "WARNING: alphabeta [%d, %d] will be inverted.\n", options.alpha, options.beta);
		tmp = options.alpha;
		options.alpha = options.beta;
		options.beta = tmp;
	}

	if (options.name == NULL) options.name = string_duplicate(EDAX_NAME);
	if (options.game_file == NULL) options.game_file = string_duplicate("data/game.ggf");
	if (options.eval_file == NULL) options.eval_file = string_duplicate("data/eval.dat");
	if (options.book_file == NULL) options.book_file = string_duplicate("data/book.dat");
}

/**
 * @brief Print all global options.
 * @param f output stream.
 */
void options_dump(FILE *f) 
{
	const char *(play_type[3]) = {"fixed depth", "fixed time per game", "fixed time per move"};
	const char *(boolean_string[2]) = {"false", "true"};
	const char *(mode[4]) = {"human/edax", "edax/human", "edax/edax", "human/human"};	

	fprintf(f, "search display options\n");
	fprintf(f, "\tverbosity: %d\n", options.verbosity);
	fprintf(f, "\tminimal depth (noise): %d\n", options.noise);
	fprintf(f, "\tline width: %d\n", options.width);
	fprintf(f, "\tuser input echo: %s\n", boolean_string[options.echo]);
	fprintf(f, "\t<detailed info>: %s\n\n", boolean_string[options.info]);
	fprintf(f, "Cassio options\n");
	fprintf(f, "\tdisplay debug info in Cassio's 'fenetre de rapport': %s\n", boolean_string[options.debug_cassio]);
	fprintf(f, "\tadapt Cassio requests to search & solve faster: %s\n\n", boolean_string[options.transgress_cassio]);

	fprintf(f, "\tsearch options\n");
	fprintf(f, "\tsize (in number of bits) of the hash table: %d\n", options.hash_table_size);
	fprintf(f, "\tsorting depth increment: pv = %d, all = %d, cut = %d\n",  options.inc_sort_depth[0], options.inc_sort_depth[1], options.inc_sort_depth[2]);
	fprintf(f, "\ttask number for parallel search: %d\n", options.n_task);
	fprintf(f, "\tsearch level: %d\n", options.level);
	fprintf(f, "\tsearch alloted time:"); time_print(options.time, false, stdout); fprintf(f, "\n");
	fprintf(f, "\tsearch with: %s\n", play_type[options.play_type]);
	fprintf(f, "\tsearch pondering: %s\n", boolean_string[options.can_ponder]);
	fprintf(f, "\tsearch depth: %d\n", options.depth);
	fprintf(f, "\tsearch selectivity: %d\n", options.selectivity);
	fprintf(f, "\tsearch speed %.0f N/s\n", options.speed);
	fprintf(f, "\tsearch nps %.0f N/s\n", options.nps);
	fprintf(f, "\tsearch alpha: %d\n", options.alpha);
	fprintf(f, "\tsearch beta: %d\n", options.beta);
	fprintf(f, "\tsearch all best moves: %s\n", boolean_string[options.all_best]);
	fprintf(f, "\teval file: %s\n", options.eval_file);
	fprintf(f, "\tbook file: %s\n", options.book_file);
	fprintf(f, "\tbook allowed: %s\n", boolean_string[options.book_allowed]);
	if (options.book_depth > 0) fprintf(f, "\tbook depth at startup: %d\n", options.book_depth);
	else fprintf(f, "\tbook depth at startup: auto (the depth of the loaded book)\n");
	fprintf(f, "\tbook randomness: %d\n", options.book_randomness);
	fprintf(f, "\tbook timed-save interval: %d minutes\n", options.book_save_interval);
	fprintf(f, "\tbook deviate-save interval: %d productive rounds (0 = completion only)\n", options.book_deviate_save_rounds);
	fprintf(f, "\tbook merge auto-save: %s\n", boolean_string[options.book_merge_auto_save]);
	fprintf(f, "\tbook store auto-save: %s\n", boolean_string[options.book_store_auto_save]);
	fprintf(f, "\tbook leaf recalculate rounds: %d\n", options.book_leaf_recalculate_rounds);
	if (options.book_seed_set) fprintf(f, "\tbook seed: %llu\n", options.book_seed);
	else fprintf(f, "\tbook seed: auto (the clock)\n");
	fprintf(f, "\tbook reproducible: %s\n", boolean_string[options.book_reproducible]);
	fprintf(f, "\tbook reduce shrink: %s, reuse: %s, index: %s, walk: %s\n", boolean_string[options.book_reduce_shrink], boolean_string[options.book_reduce_reuse],
		options.book_reduce_index ? "table" : "binary", options.book_reduce_walk ? "group" : "loss");
	if (options.book_store_tasks > 0) fprintf(f, "\tbook store tasks: %d\n\n", options.book_store_tasks);
	else fprintf(f, "\tbook store tasks: auto\n\n");

	fprintf(f, "ggs options\n");
	fprintf(f, "\thost: %s\n", options.ggs_host ? options.ggs_host : "?");
	fprintf(f, "\tport: %s\n", options.ggs_port ? options.ggs_port : "?");
	fprintf(f, "\tlogin: %s\n", options.ggs_login ? options.ggs_login : "?");
	fprintf(f, "\tpassword: %s\n", options.ggs_password ? options.ggs_password : "?");
	fprintf(f, "\topen: %s\n\n", boolean_string[options.ggs_open]);

	fprintf(f, "PV options\n");
	fprintf(f, "\tdebug: %s\n", boolean_string[options.pv_debug]);
	fprintf(f, "\tcheck: %s\n", boolean_string[options.pv_check]);
	fprintf(f, "\tguess: %s\n\n", boolean_string[options.pv_guess]);

	fprintf(f, "game file: %s\n", options.game_file ? options.game_file : "?");

	fprintf(f, "log files\n");
	fprintf(f, "\tsearch: %s\n", options.search_log_file ? options.search_log_file : "?");
	fprintf(f, "\tui: %s\n", options.ui_log_file ? options.ui_log_file : "?");
	fprintf(f, "\tggs: %s\n", options.ggs_log_file ? options.ggs_log_file : "?");

	fprintf(f, "name: %s\n", options.name ? options.name : "?");

	fprintf(f, "Game play\n");
	fprintf(f, "\tmode: %s\n", mode[options.mode]);
	fprintf(f, "\tstart a new game after a game is over: %s\n", boolean_string[options.auto_start]);
	fprintf(f, "\tstore each played game in the opening book: %s\n", boolean_string[options.auto_store]);
	fprintf(f, "\tchange computer's side after each game: %s\n", boolean_string[options.auto_swap]);
	fprintf(f, "\tquit when game is over: %s\n", boolean_string[options.auto_quit]);
	fprintf(f, "\trepeat %d games (before exiting)\n\n\n", options.repeat);
}

/**
 * @brief free allocated resources.
 */
void options_free(void)
{
	free(options.ggs_host);
	free(options.ggs_login);
	free(options.ggs_password);
	free(options.ggs_port);

	free(options.game_file);
	free(options.ui_log_file);
	free(options.search_log_file);
	free(options.ggs_log_file);
	free(options.name);
	free(options.book_file);
	free(options.eval_file);
}

