/**
 * @file main.c
 *
 * @brief Main file.
 *
 * @date 1998 - 2024
 * @author Richard Delorme
 * @version 4.5
 */

#include "board.h"
#include "cassio.h"
#include "hash.h"
#include "obftest.h"
#include "options.h"
#include "perft.h"
#include "search.h"
#include "stats.h"
#include "ui.h"

bool ui_book_save_failed(void);
#include "util.h"

#include <locale.h>
#include <stdio.h>
#include <string.h>

/**
 * @brief Build the path of config.ini next to the executable.
 *
 * @param executable Executable path from argv[0].
 * @param config_file Output path (empty when the name of the folder is too long).
 * @return false if the name of the folder of the executable is too long for the path (up to v4.5.5-nikque.26 the
 * config.ini of the current folder was then read, with the warning of a program started without its folder name).
 */
static bool get_config_file_path(const char *executable, char *config_file)
{
	const char *slash = strrchr(executable, '/');
	const char *backslash = strrchr(executable, '\\');
	const char *separator = slash;
	size_t directory_length;

	if (backslash && (!separator || backslash > separator)) separator = backslash;
	directory_length = separator ? (size_t)(separator - executable + 1) : 0;
	if (directory_length + sizeof("config.ini") > FILENAME_MAX + 1) {
		*config_file = '\0';
		return false;
	}
	memcpy(config_file, executable, directory_length);
	strcpy(config_file + directory_length, "config.ini");
	return true;
}

/**
 * @brief Print version & copyright.
 */
void version(void)
{
	fprintf(stderr, "Edax version " VERSION_STRING " " __DATE__ " " __TIME__
#if defined(__linux__)
		" for Linux"
#elif defined(_WIN32)
		" for Windows"
#elif defined(__APPLE__)
		" for Apple"
#endif
		"\ncopyright 1998 - 2018 Richard Delorme, 2014 - 25 Toshihiko Okuhara\n\n");
}


/**
 * @brief Programme usage.
 */
void usage(void)
{
	fprintf(stderr, "Usage: edax <protocol> <options>\n"
		"User Interface Protocols:\n"
		" -edax     Edax's user interface (default)\n"
		" -ggs      Generic Game Server interface (play through internet)\n"
		" -gtp      Go Text Protocol.\n"
		" -xboard xboard/winboard protocol.\n"
		" -nboard NBoard protocol.\n"
		" -cassio Cassio protocol.\n"
		" -solve <problem_file>    Automatic problem solver/checker.\n"
		" -wtest <wthor_file>      Test edax using WThor's theoric score.\n"
		" -count <level>           Count positions up to <level>.\n");
	options_usage();
}

/**
 * @brief edax main function.
 *
 * Do a global initialization and choose a User Interface protocol.
 *
 * @param argc Number of arguments.
 * @param argv Command line arguments.
 */
int main(int argc, char **argv)
{
	UI *ui;
	bool ui_freed = true;
	int i, r, level = 0, size = 8;
	char config_file[FILENAME_MAX + 1];
	char *problem_file = NULL;
	char *wthor_file = NULL;
	char *count_type = NULL;
	int n_bench = 0;

	// options.n_task default to system cpu number
	options.n_task = get_cpu_number();

	// options from edax.ini
	options_parse_defaults("edax.ini");
	if (!get_config_file_path(argv[0], config_file)) {
		warn("the name of the folder of Edax is too long: its config.ini is not read, default settings are used\n");
	} else {
		options_parse_defaults(config_file);
		if (strcmp(config_file, "config.ini") == 0) {
			// started by its name alone (found through the PATH): the folder of the executable is not known,
			// and config.ini is only looked for in the current folder
			FILE *f = fopen(config_file, "r");
			if (f) fclose(f);
			else warn("config.ini was not found in the current folder (Edax was started without its folder name): default settings are used\n");
		}
	}

	// allocate ui
	ui = (UI*) mm_malloc(sizeof *ui);	// Eval in Search in Play in UI
	if (ui == NULL) fatal_error("Cannot allocate a user interface.\n");
	ui->type = UI_EDAX;
	ui->init = ui_init_edax;
	ui->free = ui_free_edax;
	ui->loop = ui_loop_edax;

	// parse arguments
	for (i = 1; i < argc; i++) {
		char *arg = argv[i];
		while (*arg == '-') ++arg;
		if (strcmp(arg, "v") == 0 || strcmp(arg, "version") == 0) version();
		else if (ui_switch(ui, arg)) ;
		else if ((r = (options_read(arg, argv[i + 1]))) > 0) i += r - 1;
		else if (strcmp(arg, "solve") == 0 && argv[i + 1]) problem_file = argv[++i];
		else if (strcmp(arg, "wtest") == 0 && argv[i + 1]) wthor_file = argv[++i];
		else if (strcmp(arg, "bench") == 0 && argv[i + 1]) n_bench = atoi(argv[++i]);
		else if (strcmp(arg, "count") == 0 && argv[i + 1]) {
			count_type = argv[++i];
			if (argv[i + 1]) level = string_to_int(argv[++i], 0);
			if (argv[i + 1] && strcmp(argv[i + 1], "6x6") == 0) {
				size = 6;
				++i;
			}
			
	
		}
		else usage();
	}
	options_bound();

	// initialize
	bit_init();
	edge_stability_init();
	statistics_init();
	eval_open(options.eval_file);
	search_global_init();

	// solver & tester
	if (problem_file || wthor_file || n_bench) {
		Search search;
		search_init(&search);
		search.options.header = " depth|score|       time   |  nodes (N)  |   N/s    | principal variation";
		search.options.separator = "------+-----+--------------+-------------+----------+---------------------";
		if (options.verbosity) version();
		if (problem_file) obf_test(&search, problem_file, NULL);
		if (wthor_file) wthor_test(wthor_file, &search);
		if (n_bench) obf_speed(&search, n_bench);
		search_free(&search);

	} else if (count_type){
		Board board;
		board_init(&board);
		if (strcmp(count_type, "games") == 0) quick_count_games(&board, level, size);
		else if (strcmp(count_type, "positions") == 0) count_positions(&board, level, size);
		else if (strcmp(count_type, "shapes") == 0) count_shapes(&board, level, size);

	} else if (ui->type == UI_CASSIO) {
		engine_loop();

	// other protocols
	} else {
		// initialize the engine (and load the book) before the input thread may
		// call play_stop() on a not yet initialized search (piped "quit" crash)
		ui->init(ui);
		ui_event_init(ui);
		ui->loop(ui);
		ui_event_close(ui); // (before the play is freed: see ui_read_input)
		if (ui->free) ui->free(ui);
		ui_freed = ui_event_free(ui);
	}

	// display statistics
	statistics_print(stdout);


	// free;
	eval_close();
	options_free();
	if (ui_freed) mm_free(ui); // (else the input thread still uses it: it ends with the process)

	return ui_book_save_failed() ? 1 : 0; // (the book could not be saved at the end: "@save ok=0" was written)
}

