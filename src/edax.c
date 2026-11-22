/**
 * @file edax.c
 *
 * @brief Edax protocol.
 *
 * This is version 4.5 of Edax User Interface. Several changes
 * occurred between this version and 3.x previous versions because of the
 * evolution of the search engine. Here is a summary of the commands:
 *
 * Options:
 * Options must be entered in the form '[set] <option> [=] <value>', with [set] and\n[=] being optional.
 *   -verbose [n]          set Edax verbosity (default 1).
 *   -noise [n]            start displaying Edax search result from this depth\n  (default 0).
 *   -width [n]            display edax search results using <width> characters\n  (default 80).
 *   -hash-table-size [n]  set hashtable size (default 21 bits).
 *   -n-tasks [n]          control the number of parallel threads used in searching\n  (default: the number of logical CPUs).
 *   -l|level [n]          search using limited depth (default 18).
 *   -t|game-time <time>   search using limited time per game.
 *   -move-time <time>     search using limited time per move.
 *   -ponder [on/off]      set pondering on/off.
 *   -book-file [file]     use [file] as default book file (default data/book.dat).
 *   -book-usage [on/off]  use or do not use the opening book.
 *   -book-randomness [n]  play various but worse moves from the opening book.
 *   -auto-start [on/off]  automatically start a new game.
 *   -auto-swap [on/off]   automatically swap players between each game.
 *   -auto-store [on/off]  automatically store each game into the opening book.
 *
 * Commands:
 * Commands must be entered in the form '<command> <parameters>'.
 *   -i|nit               start a new game from standard initial position.
 *   -n|ew                start a new game from a personalized position.
 *   -setboard <board>    set a personalized position to start from.
 *   -o|open|load <file>  load a played game.
 *   -s|save <file>       save a played game.
 *   -q|quit|exit         quit from edax.
 *   -u|undo              undo the last played move.
 *   -r|redo              redo the last played move.
 *   -play <moves>        play a sequence of moves.
 *   -force <moves>       force to play an opening.
 *   -go                  ask edax to play.
 *   -stop                stop edax search.
 *   -hint [n]            ask edax to search the first bestmoves.
 *   -m|mode [n]          ask edax to automatically play (default = 3).
 *   -a|analyze [n]       retro-analyze the game.
 *   -?|help              show this message.
 *   -v|version           display the version number.
 *
 * Book Commands:
 * Book Commands must be entered in the form 'b|book <command> <parameters>'.
 *   -new <n1> <n2>       create a new empty book with level <n1> and depth <n2>.
 *   -load [file]         load an opening book from a binary opening file.
 *   -merge [file]        merge an opening book with the current opening book.
 *   -save [file]         save an opening book to a binary opening file.
 *   -import [file]       load an opening book from a portable text file.
 *   -export [file]       save an opening book to a portable text file.
 *   -on                  use the opening book.
 *   -off                 do not use the opening book.
 *   -show                display details about the current position.
 *   -info                display book general information.
 *   -a|analyze [n]       retro-analyze the game using the opening book.
 *   -randomness [n]      play more various but worse move from the opening book.
 *   -depth [n]           change book depth (up to which to add positions).
 *   -deepen              search again with the level of the book the positions of a lower level.
 *   -fix                 fix the opening book: add missing links and negamax the\n  whole book tree.
 *   -store               add the last played game to the opening book.
 *   -deviate <n1> <n2>   add positions by deviating with a relative error <n1> and\n  an absolute error <n2>.
 *   -deviate2 <n1> <n2>  add positions with a per-move loss limit <n1> and\n  a cumulative loss limit for both players <n2>.
 *   -enhance <n1> <n2>   add positions by improving score accuracy with a midgame\n  error <n1> and an endcut error <n2>.
 *   -fill [n]            add positions between existing positions.
 *   -prune               remove unreachable positions.
 *   -add [file]          add positions from a game base file (txt, ggf, sgf or\n  wthor format).
 *
 * Game DataBase Commands:
 *   -convert [file_in] [file_out]     convert between different format.
 *   -unique [file_in] [file_out]      remove doublons in the base.
 *   -check [file_in] [n]              check error in the last <n> moves.
 *   -correct [file_in] [n]            correct error in the last <n> moves.
 *   -complete [file_in]               complete a database by playing the last\n  missing moves.
 *   -problem [file_in] [n] [file_out] build a set of <n> problems from a game\n  database.
 *
 * Tests commands:
 *   -solve <file> [hard-file]   Solve problems from an OBF file.
 *   -script-to-obf [file]Convert a script to an obf file.
 *   -wtest [file]        check the theoric scores of a wthor base file.
 *   -count games [d]     compute the number of moves from the current position up\n  to depth [d].
 *   -perft [d]           same as above, but without hash table.
 *   -estimate [d] [n]    estimate the number of moves from the current position up\n  to depth [d].
 *   -count positions [d] compute the number of positions from the current position\n  up to depth [d].
 *   -count shapes [d]    compute the number of shapes from the current position up\n  to depth [d].
 *
 *
 * @date 1998 - 2018
 * @author Richard Delorme
 * @version 4.4
 *
 */

#include "cassio.h"
#include "event.h"
#include "histogram.h"
#include "options.h"
#include "opening.h"
#include "obftest.h"
#include "perft.h"
#include "play.h"
#include "search.h"
#include "util.h"
#include "ui.h"

#include <ctype.h>
#ifdef __linux__
	#include <sys/time.h>
	#include <sys/resource.h>
#endif

static Log edax_log[1];
extern bool book_verbose;

void version(void);
void bench(void);

/**
 * @brief default search oberver.
 * @param result Search Result.
 */
static void edax_observer(Result *result)
{
	ui_output_lock(); // (whole lines, between the lines of the book analyses)
	search_observer(result);
	ui_output_unlock(false);
	if (log_is_open(edax_log)) {
		result_print(result, edax_log->f);
		putc('\n', edax_log->f);
	}
}

/**
 * @brief initialize edax protocol.
 * @param ui user interface.
 */
void ui_init_edax(UI *ui)
{
	Play *play = ui->play;

	book_verbose = true;
	book_analysis_init();
	play_init(play, &ui->book);
	ui->book.search = &play->search;
	book_load_at_startup(&ui->book);
	play->search.id = 1;
	search_set_observer(&play->search, edax_observer);
	ui->mode = options.mode;
	play->type = ui->type;

	log_open(edax_log, options.ui_log_file);
}

/**
 * @brief free resources used by edax protocol.
 * @param ui user interface.
 */
void ui_free_edax(UI *ui)
{
	book_analysis_end();
	if (ui->book.need_saving) book_save_to_book_file(&ui->book); // (a book file replaced by "book new" is kept under another name)
	book_free(&ui->book);
	play_free(ui->play);
	log_close(edax_log);
	book_verbose = false;
}

/**
 * @brief print help about options.
 */
void help_options(void) 
{
	printf(	"Options:\n"
		"Options must be entered in the form '[set] <option> [=] <value>', with [set] and\n[=] being optional.\n"
		"  verbose [n]          set Edax verbosity (default 1).\n"
		"  noise [n]            start displaying Edax search result from this depth\n  (default 0).\n"
		"  width [n]            display edax search results using <width> characters\n  (default 80).\n"
		"  hash-table-size [n]  set hashtable size (default 21 bits).\n"
		"  n-tasks [n]          control the number of parallel threads used in searching\n  (default: the number of logical CPUs).\n"
		"  l|level [n]          search using limited depth (default 18).\n"
		"  t|game-time <time>   search using limited time per game.\n"
		"  move-time <time>     search using limited time per move.\n"
		"  ponder [on/off]      set pondering on/off.\n"
		"  book-file [file]     use [file] as default book file (default data/book.dat).\n"
		"  book-usage [on/off]  use or do not use the opening book.\n"
		"  book-randomness [n]  play various but worse moves from the opening book.\n"
		"  auto-start [on/off]  automatically start a new game.\n"
		"  auto-swap [on/off]   automatically swap players between each game.\n"
		"  auto-store [on/off]  automatically store each game into the opening book.\n");
}

/**
 * @brief print help commands.
 */
void help_commands(void) 
{
	printf(	"\nCommands:\n"
		"Commands must be entered in the form '<command> <parameters>'.\n"
		"  i|nit               start a new game from standard initial position.\n"
		"  n|ew                start a new game from a personalized position.\n"
		"  setboard <board>    set a personalized position to start from.\n"
		"  o|open|load <file>  load a played game.\n"
		"  s|save <file>       save a played game.\n"
		"  q|quit|exit         quit from edax.\n"
		"  u|undo              undo the last played move.\n"
		"  r|redo              redo the last played move.\n"
		"  play <moves>        play a sequence of moves.\n"
		"  force <moves>       force to play an opening.\n"
		"  go                  ask edax to play.\n"
		"  stop                stop edax search.\n"
		"  hint [n]            ask edax to search the first bestmoves.\n"
		"  m|mode [n]          ask edax to automatically play (default = 3).\n"
		"  a|analyze [n]       retro-analyze the game.\n"
		"  ?|help              show this message.\n"
		"  v|version           display the version number.\n");
}

/**
 * @brief print book's help.
 */
void help_book(void) 
{
	printf(	"\nBook Commands:\n"
		"Book Commands must be entered in the form 'b|book <command> <parameters>'.\n"
		"  new <n1> <n2>       create a new empty book with level <n1> and depth <n2>.\n"
		"  load [file]         load an opening book from a binary opening file.\n"
		"  merge [file]        merge an opening book with the current opening book.\n"
		"  save [file]         save an opening book to a binary opening file.\n"
		"  import [file]       load an opening book from a portable text file.\n"
		"  export [file]       save an opening book to a portable text file.\n"
		"  on                  use the opening book.\n"
		"  off                 do not use the opening book.\n"
		"  show                display details about the current position.\n"
		"  info                display book general information.\n"
		"  a|analyze [n]       retro-analyze the game using the opening book.\n"
		"  randomness [n]      play more various but worse move from the opening book.\n"
		"  depth [n]           change book depth (up to which to add positions).\n"
		"  deepen              search again with the level of the book the positions of a lower level.\n"
		"  fix                 fix the opening book: add missing links and negamax the\n  whole book tree.\n"
		"  store               add the last played game to the opening book.\n"
		"  deviate <n1> <n2>   add positions by deviating with a relative error <n1> and\n  an absolute error <n2>.\n"
		"  deviate2 <n1> <n2>  add positions with a per-move loss limit <n1> and\n  a cumulative loss limit for both players <n2>; skip solved leaves.\n"
		"  deviate3 <n1> <n2>  same loss limits as deviate2, including solved leaves.\n"
		"  deviate.. <n1> <n2> export <file> [leaf|ends|all]  write the lines that deviate,\n  deviate2 or deviate3 walks to a text file, without adding positions: the\n  positions whose leaf it would expand (leaf, default), the ends of its lines\n  (ends), or every position of its lines (all).\n"
		"  enhance <n1> <n2>   add positions by improving score accuracy with a midgame\n  error <n1> and an endcut error <n2>.\n"
		"  fill [n] [n1 n2]    add positions between existing positions exactly <n> moves\n  apart (default 1); with <n1> <n2>, only from the positions that deviate3\n  <n1> <n2> walks.\n"
		"  fill2 [n] [n1 n2]   same as fill (default 2), only through the leaf (best move\n  not in the book yet) of a position; with <n1> <n2>, only from the leaves\n  that deviate3 <n1> <n2> would expand.\n"
		"  play [n]            expand the positions without link to the depth of the book;\n  with <n>, only those from the current position, then play on from them while\n  a position has <n> empty squares or more.\n"
		"  exhaust             add every move from the current position to the depth of\n  the book (the number of positions grows very fast with the depth).\n"
		"  extract [draw] <file> write the lines of the best moves (the draws) to a game\n  file; extract all <file>: all the lines of the book. The games of the file\n  are kept.\n"
		"  extend              from the current position, expand the leaves that no link\n  is better than (as good as the best link, or no link), until none is left.\n"
		"  leaf recalculate <n1> <n2>  search again the leaves that deviate <n1> <n2> would\n  expand (leaf recalculate2: deviate2; 3, 4: the leaves of all the positions\n  of the walk of deviate, deviate2).\n"
		"  leaf expand [n1 n2] expand the leaves better than every link of their\n  position; with <n1> <n2>, only in the walk of deviate3 <n1> <n2>.\n"
		"  prune               remove unreachable positions.\n"
		"  subtree             only keep positions from the current position.\n"
		"  reduce <n1> <n2>    only keep the positions that deviate3 <n1> <n2> walks from\n  the current position, and those its moves lead to.\n"
		"  add [file]          add positions from a game base file (txt, ggf, sgf or\n  wthor format).\n"
		"  moveinfo id=<n> ... for every legal move: book value, precision stage, missing\n  leaves, umigame numbers, positions (answers: lines starting with '@').\n"
		"  missing id=<n> ...  the lines to the leaves missing for the next stage.\n"
		"  levels id=<n> ...   the positions of each level after the current position.\n"
		"  bestlines id=<n> ... the lines of best moves: counts, and a file of their ends.\n"
		"  findline id=<n> target=<position> ... the lines from the current position to\n  a position, through book positions only.\n"
		"  reducecount id=<n> x=<n1> y=<n2>  the positions that reduce <n1> <n2> would\n  keep and remove.\n"
		"  (these six commands run beside the others and only read the book;\n  \"stop id=<n>\" stops one of them.)\n");
}

/**
 * @brief print base's help.
 */
void help_base(void) 
{
	printf(	"\nGame DataBase :\n"
		"  convert [file_in] [file_out]     convert between different format.\n"
		"  unique [file_in] [file_out]      remove doublons in the base.\n"
		"  check [file_in] [n]              check error in the last <n> moves.\n"
		"  correct [file_in] [n]            correct error in the last <n> moves.\n"
		"  complete [file_in]               complete a database by playing the last\n  missing moves.\n"
		"  problem [file_in] [n] [file_out] build a set of <n> problems from a game\n  database.\n");
}

/**
 * @brief print base's help.
 */
void help_test(void) 
{
	printf(	"\nTests:\n"
		"  bench               test edax speed.\n"
		"  microbench          test CPU cycle speed of some major functions.\n"
		"  solve <file> [hard-file]  Solve problems from an OBF file.\n"
		"  script-to-obf [file]Convert a script to an obf file.\n"
		"  wtest [file]        check the theoric scores of a wthor base file.\n"
		"  count games [d]     compute the number of moves from the current position up\n  to depth [d].\n"
		"  perft [d]           same as above, but without hash table.\n"
		"  estimate [d] [n]    estimate the number of moves from the current position up\n  to depth [d].\n"
		"  count positions [d] compute the number of positions from the current position\n  up to depth [d].\n"
		"  count shapes [d]    compute the number of shapes from the current position up\n  to depth [d].\n");
}

/**
 * @brief The hint of the warning of an unknown command that is the name of a book command ("leaf expand 30 30"
 * for "book leaf expand 30 30"). The names of top-level commands, of options and of base commands are left out.
 * @param cmd Command (small letters).
 * @param param Its parameters.
 * @return " (book command: type ...)", to free; NULL if the command is no book command.
 */
static char* book_command_hint(const char *cmd, const char *param)
{
	static const char *const names[] = {
		"store", "on", "off", "randomness", "import", "export", "merge", "fix", "negamax", "prune", "subtree", "reduce",
		"show", "stats", "add", "learn", "extract", "deviate", "deviate2", "deviate3", "leaf", "pick-count", "enhance",
		"fill", "fill2", "extend", "exhaust", "deepen", "feed-hash", "moveinfo", "missing", "levels", "bestlines", "findline", "reducecount"
	};
	const char *space = (param && *param) ? " " : "";
	char *s;
	size_t i, size;

	if (param == NULL) param = "";
	for (i = 0; i < sizeof names / sizeof *names && strcmp(cmd, names[i]) != 0; ++i) ;
	if (i == sizeof names / sizeof *names) return NULL;
	size = 2 * (strlen(cmd) + strlen(param)) + 64;
	s = (char*) malloc(size);
	if (s) snprintf(s, size, " (book command: type \"book %s%s%s\" or \"b %s%s%s\")", cmd, space, param, cmd, space, param);
	return s;
}

/** @return true if a word starts with a number, signed or not (as parse_int reads it). */
static bool starts_with_number(const char *s)
{
	return isdigit((unsigned char) s[0]) || ((s[0] == '+' || s[0] == '-') && isdigit((unsigned char) s[1]));
}

/**
 * @brief Read the loss limits of book deviate2, deviate3 (and of the commands that walk as they do): 2 and 4 when
 * they are not given.
 * @param param Parameters.
 * @param move_loss Per-move loss limit (out).
 * @param total_loss Cumulative loss limit for both players (out).
 * @return the rest of the parameters.
 */
static char* parse_loss_limits(char *param, int *move_loss, int *total_loss)
{
	*move_loss = 2; param = parse_int(param, move_loss); BOUND(*move_loss, 0, 129, "per-move loss");
	*total_loss = 4; param = parse_int(param, total_loss); BOUND(*total_loss, 0, 7740, "cumulative loss");
	return param;
}

/**
 * @brief Read a file name: a word, or a name between double quotes, which may hold spaces (the quotes are dropped).
 * @param s Words.
 * @param name File name (out: empty if there is none).
 * @param size Size of name.
 * @param wrong What is wrong with the name (out): NULL, or the name is longer than size - 1 characters (it is not cut),
 * or its quote is not closed.
 * @return the rest of the words.
 */
static const char* parse_file_name(const char *s, char *name, const size_t size, const char **wrong)
{
	size_t n = 0;
	const bool quoted = (*(s = parse_skip_spaces(s)) == '"');

	*wrong = NULL;
	if (quoted) ++s;
	for (; *s && (quoted ? *s != '"' : !isspace((unsigned char) *s)); ++s, ++n) if (n + 1 < size) name[n] = *s;
	name[n + 1 < size ? n : size - 1] = '\0';
	if (n + 1 > size) *wrong = "the file name is too long";
	else if (quoted && *s != '"') *wrong = "the quote of the file name is not closed";
	if (quoted && *s == '"') ++s;
	return s;
}

/**
 * @brief "export <file> [leaf|ends|all]" after the numbers of book deviate, deviate2, deviate3: the lines of its walk
 * are written to the file, nothing is expanded (see book_deviate_export). The file name may be written between double
 * quotes ("C:\a b\lines.txt"). Another word after the numbers is an error (up to v4.5.5-nikque.26 it was skipped, and
 * the book expanded: "exprot" for "export"), and so is a file name too long (it was cut).
 * @return false if nothing follows the numbers (the command expands the book).
 */
static bool deviate_export(Play *play, const char *param, const int base, const int x, const int y)
{
	static const char *names[] = {"", "deviate", "deviate2", "deviate3"};
	char word[64], file[FILENAME_MAX], game_line[2 * PLAY_GAME_SIZE + 1];
	const char *wrong;
	int which = BOOK_EXPORT_LEAF;

	param = parse_skip_spaces(param);
	if (*param == '\0') return false;
	param = parse_word(param, word, sizeof word);
	string_to_lowercase(word);
	if (strcmp(word, "export") != 0) {
		printf("ERROR: book %s: unknown word %s after the numbers (export, or nothing); the book is not expanded\n", names[base], word);
		return true;
	}
	param = parse_file_name(param, file, sizeof file, &wrong);
	if (wrong) {
		printf("ERROR: book deviate export: %s\n", wrong);
		return true;
	}
	parse_word(param, word, sizeof word);
	string_to_lowercase(word);
	if (strcmp(word, "ends") == 0) which = BOOK_EXPORT_ENDS;
	else if (strcmp(word, "all") == 0) which = BOOK_EXPORT_ALL;
	else if (*word && strcmp(word, "leaf") != 0) {
		printf("ERROR: book deviate export: unknown word %s (leaf, ends or all)\n", word);
		return true;
	}
	if (!*file) {
		printf("ERROR: book deviate export: no file\n");
		return true;
	}
	book_deviate_export(play->book, &play->board, play_get_moves(play, game_line), base, x, y, file, which);
	return true;
}

/**
 * @brief Loop event.
 * @param ui user interface.
 */
static void ui_play_print(Play *play, FILE *f)
{
	putc('\n', f);
	play_print(play, f);
	if (play_is_game_over(play)) fputs("\n*** Game Over ***\n", f);
	putc('\n', f);
}

void ui_loop_edax(UI *ui)
{
	char *cmd = NULL, *param = NULL;
	Play *play = ui->play;
	char book_file[FILENAME_MAX];
	char game_line[2 * PLAY_GAME_SIZE + 1]; // (moves of the game, for book bestlines)
	unsigned long long histogram[129][65];
	int repeat = options.repeat;

	histogram_init(histogram);

	// loop forever
	for (;;) {
		errno = 0;

		if (options.verbosity)
			ui_play_print(play, stdout);

		if (log_is_open(edax_log))
			ui_play_print(play, edax_log->f);

		// edax turn ? (automatic play mode)
		if (!ui_event_exist(ui) && !play_is_game_over(play) && (ui->mode == !play->player || ui->mode == 2)) {
			putchar('\n');
			play_go(play, true);
			ui_output_lock(); printf("\nEdax plays "); move_print(play_get_last_move(play)->x, 0, stdout); putchar('\n'); ui_output_unlock(false);
			if (ui->mode != 2) play_ponder(play);

		// proceed by reading a command
		} else {

			/* automatic rules after a game over*/
			if (play_is_game_over(play)) {
				if (options.auto_store) {
					book_analysis_quiesce(); // (the book changes)
					play_store(play);
				}
				if (options.auto_swap && ui->mode < 2) ui->mode = !ui->mode;
				if (options.repeat && repeat > 1) {
					--repeat;
					play_new(play);
					continue;
				}
				if (options.auto_quit) {
					return;
				}
				if (options.auto_start) {
					play_new(play);
					continue;
				}
			}

			ui_output_lock(); putchar('>'); fflush(stdout); ui_output_unlock(true);
			ui_event_wait(ui, &cmd, &param);
			log_print(edax_log, "cmd=\"%s\" ; param=\"%s\"\n", cmd, param);
			ui_output_lock(); putchar('\n'); ui_output_unlock(false);

			if (cmd == NULL) {
				warn("unexpected null command?\n");
				continue;
			}

			// skip empty lines or commented lines
			if (*cmd == '\0' || *cmd == '#') {
			
			// help
			} else if (strcmp(cmd, "help") == 0 || strcmp(cmd, "?") == 0) {
				if (*param == '\0' || strcmp(param, "options") == 0) help_options();
				if (*param == '\0' || strcmp(param, "commands") == 0) help_commands();
				if (*param == '\0' || strcmp(param, "book") == 0) help_book();
				if (*param == '\0' || strcmp(param, "base") == 0) help_base();
				if (*param == '\0' || strcmp(param, "test") == 0) help_test(); 

			// new game from standard position
			} else if (strcmp(cmd, "i") == 0 || strcmp(cmd, "init") == 0) {
				board_init(&play->initial_board);
				play->initial_player = BLACK;
				play_force_init(play, "F5");
				play_new(play);

			// new game from personnalized position
			} else if ((strcmp(cmd, "n") == 0 || strcmp(cmd, "new") == 0) && *param == '\0') {
				play_new(play);

			// open a saved game
			} else if (strcmp(cmd, "o") == 0 || strcmp(cmd, "open") == 0 || strcmp(cmd, "load") == 0) {
				if (!play_load(play, param)) warn("%s", play->error_message); // (a load that failed used to say nothing here; the "loadsgf" of GTP gives this message)

			// save a game
			} else if (strcmp(cmd, "s") == 0 || strcmp(cmd, "save") == 0) {
				play_save(play, param);

			// quit
			} else if (strcmp(cmd, "quit") == 0 || strcmp(cmd, "q") == 0 || strcmp(cmd, "exit") == 0) {
				free(cmd); free(param);
				return;

			} else if (!options.auto_quit && (strcmp(cmd, "eof") == 0 && (ui->mode != 2 || play_is_game_over(play)))){
				free(cmd); free(param);
				return;

			// undo last move
			} else if (strcmp(cmd, "u") == 0 || strcmp(cmd, "undo") == 0) {
				play_undo(play);
				if (ui->mode == 0 || ui->mode == 1) play_undo(play);

			// redo last move
			} else if (strcmp(cmd, "r") == 0 || strcmp(cmd, "redo") == 0) {
				play_redo(play);
				if (ui->mode == 0 || ui->mode == 1) play_redo(play);

			// mode
			} else if (strcmp(cmd, "m") == 0 || strcmp(cmd, "mode") == 0) {
				ui->mode = string_to_int(param, 3);
				BOUND(ui->mode, 0, 3, "mode");

			// analyze a game
			} else if (strcmp(cmd, "a") == 0 || strcmp(cmd, "analyze") == 0 || strcmp(cmd, "analyse") == 0) {
				play_analyze(play, string_to_int(param, play->n_game));

			// set a new initial position
			} else if (strcmp(cmd, "setboard") == 0) {
				play_set_board(play, param);

			// vertical mirror
			} else if (strcmp(cmd, "vmirror") == 0) {
				play_symetry(play, 2);

			// horizontal mirror
			} else if (strcmp(cmd, "hmirror") == 0) {
				play_symetry(play, 1);

			// rotate
			} else if (strcmp(cmd, "rotate") == 0) {
				int angle = string_to_int(param, 90) % 360;
				if (angle < 0) angle += 360;
				switch (angle) {
				case 90: 
					play_symetry(play, 5);
					break; 
				case 180:
					play_symetry(play, 3);
					break; 
				case 270:
					play_symetry(play, 6);
					break; 
				default:
					warn("Rotate angle should be 90°, 180° or 270°");
					break;
				}

			// direct symetry...
			} else if (strcmp(cmd, "symetry") == 0) {
				int sym = string_to_int(param, 1);
				if (sym < 0 || sym >= 16) warn("symetry parameter should be a number between 0 and 15\n");
				else {
					if (sym & 8) play->player ^= 1;
					play_symetry(play, sym & 7);
				}

			// play a serie of moves
			} else if (strcmp(cmd, "play") == 0) {
				string_to_lowercase(param);
				play_game(play, param);

			// force edax to play an opening
			} else if (strcmp(cmd, "force") == 0) {
				string_to_lowercase(param);
				play_force_init(play, param);

			// solve a set of problems
			} else if (strcmp(cmd, "solve") == 0) {
				char problem_file[FILENAME_MAX + 1], *hard_file;
				hard_file = parse_word(param, problem_file, FILENAME_MAX);
				parse_word(hard_file, hard_file, FILENAME_MAX);
				obf_test(&play->search, problem_file, hard_file);
				search_set_observer(&play->search, edax_observer);

			// convert a set of problems in a .script file to a .obf file
			} else if (strcmp(cmd, "script-to-obf") == 0) {
				char script_file[FILENAME_MAX + 1], *obf_file;
				obf_file = parse_word(param, script_file, FILENAME_MAX);
				parse_word(obf_file, obf_file, FILENAME_MAX);
				script_to_obf(&play->search, script_file, obf_file);
				search_set_observer(&play->search, edax_observer);

			} else if (strcmp(cmd, "select-hard") == 0) {
				char full_file[FILENAME_MAX + 1], *hard_file;
				hard_file = parse_word(param, full_file, FILENAME_MAX);
				parse_word(hard_file, hard_file, FILENAME_MAX);
				obf_filter(full_file, hard_file);

			// game/position enumeration
			} else if (strcmp(cmd, "count") == 0) {
				char count_cmd[16], *count_param;
				int depth = 10, size = 8;

				count_param = parse_word(param, count_cmd, 15);
				count_param = parse_int(count_param, &depth); BOUND(depth, 1, 90, "max-ply");
				/* if (count_param) */ parse_int(count_param, &size); BOUND(size, 6, 8, "board-size");

				if (strcmp(count_cmd, "games") == 0) { // game enumeration
					quick_count_games(&play->board, depth, size);
				} else if (strcmp(count_cmd, "positions") == 0) { // position enumeration
					count_positions(&play->board, depth, size);
				} else if (strcmp(count_cmd, "shapes") == 0) { // shape enumeration
					count_shapes(&play->board, depth, size);
				} else {
					warn("Unknown count command: \"%s %s\"\n", cmd, param);
				}

			} else if (strcmp(cmd, "perft") == 0) {
				int depth = 14;
				depth = string_to_int(param, 10); BOUND(depth, 1, 90, "max-ply");
				count_games(&play->board, depth);
			
			// game/position enumeration
			} else if (strcmp(cmd, "estimate") == 0) {
				int n = 1000;
				n = string_to_int(param, 10); BOUND(n, 1, 2000000000, "max-trials");

				estimate_games(&play->board, n);
	
			// seek highest mobility
			} else if (strcmp(cmd, "mobility") == 0) {
				int t = 3600; // 1 hour
				t = string_to_int(param, 10); BOUND(t, 1, 3600*24*365*10, "max time");

				seek_highest_mobility(&play->board, t);

			// seek a position
			} else if (strcmp(cmd, "seek") == 0) {
				Board target;
				Line solution;
				
				board_set(&target, param);
				line_init(&solution, play->player);
				
				if (seek_position(&target, &play->board, &solution)) {
					printf("Solution found:\n");
					line_print(&solution, 200, " ", stdout);
					putchar('\n');
				}
			
			// microbench (a serie of low level tests).
			} else if (strcmp(cmd, "microbench") == 0) {
				bench();

			// bench (a serie of low level tests).
			} else if (strcmp(cmd, "bench") == 0) {
				int n = string_to_int(param, -1); BOUND(n, -1, 100, "n_problems");
				obf_speed(&play->search, n);

			// wtest test the engine against wthor theoretical scores
			} else if (strcmp(cmd, "wtest") == 0) {
				wthor_test(param, &play->search);

			// make wthor games played by "Edax (Delorme)" as "Etudes" tournament.
			} else if (strcmp(cmd, "edaxify") == 0) {
				wthor_edaxify(param);

			// wtest test the engine against wthor theoretical scores
			} else if (strcmp(cmd, "weval") == 0) {
				wthor_eval(param, &play->search, histogram);
				histogram_print(histogram);
				histogram_stats(histogram);
				histogram_to_ppm("weval.ppm", histogram);

			// go think!
			} else if (strcmp(cmd, "go") == 0) {
				if (play_is_game_over(play)) printf("\n*** Game Over ***\n");
				else {
					play_go(play, true);
					ui_output_lock(); printf("\nEdax plays "); move_print(play_get_last_move(play)->x, 0, stdout); putchar('\n'); ui_output_unlock(false);
				}

			// hint for [n] moves
			} else if (strcmp(cmd, "hint") == 0) {
				int n = string_to_int(param, 1); BOUND(n, 1, 60, "n_moves");
				play_hint(play, n);

			// stop thinking
			} else if (strcmp(cmd, "stop") == 0) {
				ui->mode = 3;

			// stop thinking
			} else if (strcmp(cmd, "version") == 0 || strcmp(cmd, "v") == 0) {
				version();

			// user move
			} else if (play_user_move(play, cmd)) {
				ui_output_lock(); printf("\nYou play "); move_print(play_get_last_move(play)->x, 0, stdout); putchar('\n'); ui_output_unlock(false);

			// debug pv
			} else if (strcmp(cmd, "debug-pv") == 0) {
				Move move;
				if (parse_move(param, &play->board, &move) != param) {
					search_set_board(&play->search, &play->board, play->player);
					pv_debug(&play->search, &move, stdout);
				}
			} else if (strcmp(cmd, "options") == 0) {
					options_dump(stdout);
#ifdef __unix__
			} else if (strcmp(cmd, "resources") == 0) {
				struct rusage u;
				long long t;
	 			getrusage(RUSAGE_SELF, &u);
				t = 1000 * u.ru_utime.tv_sec + u.ru_utime.tv_usec / 1000;
				printf("user cpu time: "); time_print(t, false, stdout); printf("\n");	
				t = 1000 * u.ru_stime.tv_sec + u.ru_stime.tv_usec / 1000;
				printf("system cpu time: "); time_print(t, false, stdout); printf("\n");	
				printf("max resident memory: %ld\n", u.ru_maxrss); 
				printf("page fault without I/O: %ld\n", u.ru_minflt); 
				printf("page fault with I/O: %ld\n", u.ru_majflt); 
				printf("number of input: %ld\n", u.ru_inblock); 
				printf("number of output: %ld\n", u.ru_oublock); 
				printf("number of voluntary context switch: %ld\n", u.ru_nvcsw); 
				printf("number of unvoluntary context switch: %ld\n\n", u.ru_nivcsw); 
#endif		
			// opening name
			} else if (strcmp(cmd, "opening") == 0) {
				const char *name;
				name = play_show_opening_name(play, opening_get_english_name);
				if (name == NULL) name = "?";
				puts(name);  

			// opening name in french
			} else if (strcmp(cmd, "ouverture") == 0) {
				const char *name;
				name = play_show_opening_name(play, opening_get_french_name);
				if (name == NULL) name = "?";
				puts(name); 

			// book analyses for a user interface (book moveinfo, missing, levels): done by a thread of their own
			} else if ((strcmp(cmd, "book") == 0 || strcmp(cmd, "b") == 0) && book_analysis_command(play->book, param, &play->board, play->player, play_get_moves(play, game_line))) {

			// opening book commands
			} else if (strcmp(cmd, "book") == 0 || strcmp(cmd, "b") == 0) {
				char book_cmd[FILENAME_MAX + 1], *book_param;
				int val_1, val_2;
				Book *book = play->book;

				{ // the analyses only read the book: they are stopped before a command that may change it
					char word[16];
					parse_word(param, word, 15);
					string_to_lowercase(word);
					if (strcmp(word, "reduce") == 0) book_analysis_quiesce_reduce(); // (it may use the positions counted by book reducecount)
					else if (strcmp(word, "on") != 0 && strcmp(word, "off") != 0 && strcmp(word, "randomness") != 0
					 && strcmp(word, "show") != 0 && strcmp(word, "info") != 0 && strcmp(word, "stats") != 0) book_analysis_quiesce();
				}
				play_no_stop(play, true); // the word "stop" does not cut the searches of a book command short (see play_stop)
				book->search = &play->search;
				book->search->options.verbosity = book->options.verbosity;
				book->failed = false; // see book_add()
				book_param = parse_word(param, book_cmd, FILENAME_MAX);
				string_to_lowercase(book_cmd); // "book Deviate 5 5" as "book deviate 5 5" (the arguments, as file names, are kept as typed)
				// "book leaf recalculate 5 5", "book leaf expand 5 5": two words, read as the command "leaf recalculate", "leaf expand"
				// (a word never holds a space: the names with a hyphen of v4.5.5-nikque.13, as book leaf-recalculate, are unknown commands)
				if (strcmp(book_cmd, "leaf") == 0) {
					char word[32];
					book_param = parse_word(book_param, word, sizeof word - 1);
					string_to_lowercase(word);
					snprintf(book_cmd, sizeof book_cmd, "leaf %s", word);
				}

				// store the last played game
				if (strcmp(book_cmd, "store") == 0) {
					play_store(play);

				// turn book usage on
				} else if (strcmp(book_cmd, "on") == 0) { // learn
					options.book_allowed = true;

				// turn book usage off
				} else if (strcmp(book_cmd, "off") == 0) { // learn
					options.book_allowed = false;

				// set book randomness
				} else if (strcmp(book_cmd, "randomness") == 0) { // learn
					val_1 = 0; book_param = parse_int(book_param, &val_1);
					options.book_randomness = val_1;

				// set book depth (until which to learn)
				} else if (strcmp(book_cmd, "depth") == 0) { // learn
					val_1 = 36; book_param = parse_int(book_param, &val_1);
					BOUND(val_1, 1, 61, "book depth"); // (up to v4.5.5-nikque.15 any depth: a game of 60 moves was read beyond its moves)
					book->options.n_empties = 61 - val_1;

				// create a new empty book
				} else if (strcmp(book_cmd, "new") == 0) {
					val_1 = 21; book_param = parse_int(book_param, &val_1);
					val_2 = 36;	book_param = parse_int(book_param, &val_2);
					if (val_1 < 0 || val_1 > 60) { // such a level would read outside the table of the levels
						warn("book new: level %d is out of range; current book retained\n", val_1);
					} else {
						BOUND(val_2, 1, 61, "book depth"); // as book depth (up to v4.5.5-nikque.26 any depth)
						book_free(book) ;
						book_new(book, val_1, 61 - val_2);
						book_set_replaced_file(options.book_file); // the book file still holds the previous book: see book_save_to_book_file()
					}

				// load an opening book (binary format) from the disc
				} else if (strcmp(book_cmd, "load") == 0 || strcmp(book_cmd, "open") == 0) {
					parse_word(book_param, book_file, FILENAME_MAX);
					play_book_load(play, book_file); // (the current book is kept when the file cannot be loaded)

				// save an opening book (binary format) to the disc
				} else if (strcmp(book_cmd, "save") == 0) {
					parse_word(book_param, book_file, FILENAME_MAX);
					if (*book_file == '\0') {
						// without a file name: the book file of the settings (up to v4.5.5-nikque.12: an error, nothing was saved)
						if (book_save_to_book_file(book)) printf("Book saved to %s\n", options.book_file);
					} else book_save(book, book_file);

				// import an opening book (text format)
				} else if (strcmp(book_cmd, "import") == 0) {
					parse_word(book_param, book_file, FILENAME_MAX);
					play_book_import(play, book_file); // (as book load: the current book is kept when nothing is imported)

				// export an opening book (text format)
				} else if (strcmp(book_cmd, "export") == 0) {
					parse_word(book_param, book_file, FILENAME_MAX);
					book_export(book, book_file);

				// merge an opening book to the current one
				} else if (strcmp(book_cmd, "merge") == 0) {
					parse_word(book_param, book_file, FILENAME_MAX);
					play_book_merge(play, book_file);

				// fix an opening book
				} else if (strcmp(book_cmd, "fix") == 0) {
					book_fix(book); // do nothing (or edax is buggy)
					book_link(book); // links nodes
					book_negamax(book); // negamax nodes
					book_sort(book); // sort moves

				// negamax an opening book
				} else if (strcmp(book_cmd, "negamax") == 0) {
					book_negamax(book); // negamax nodes
					book_sort(book); // sort moves

				// check and correct solved positions of the book
				} else if (strcmp(book_cmd, "correct") == 0) {
					book_correct_solved(book); // do nothing (or edax is buggy)
					book_fix(book); // do nothing (or edax is buggy)
					book_link(book); // links nodes
					book_negamax(book); // negamax nodes
					book_sort(book); // sort moves

				// prune an opening book
				} else if (strcmp(book_cmd, "prune") == 0) {
					book_prune(book); // remove unreachable lines.
					book_fix(book); // do nothing (or edax is buggy)
					book_link(book); // links nodes
					book_negamax(book); // negamax nodes
					book_sort(book); // sort moves

				// subtree an opening book
				} else if (strcmp(book_cmd, "subtree") == 0) {
					book_subtree(book, &play->board); // remove unreachable lines.
					book_fix(book); // do nothing (or edax is buggy)
#ifdef BOOK_TEST_SUBTREE_LINK_OLD
					book_link(book); // links nodes
#endif
					// (up to v4.5.5-nikque.11 the book was linked again here. Cutting a book adds no position: the only links
					// to add were those that the book already lacked before, and book fix or book link still adds them.)
					book_negamax(book); // negamax nodes
					book_negamax_subtree(book, &play->board); // (from this position, when the initial one is no longer in the book)
					book_sort(book); // sort moves

				// keep only the positions that book deviate3 <n1> <n2> walks from the current position
				} else if (strcmp(book_cmd, "reduce") == 0) {
					char *s = book_param, *e;
					bool words; // two numbers, each a whole word, and nothing after them
					val_1 = val_2 = -1;
					e = parse_int(s, &val_1);
					words = e && e != parse_skip_spaces(s) && (*e == '\0' || isspace((unsigned char) *e));
					s = e;
					e = parse_int(s, &val_2);
					words = words && e && e != parse_skip_spaces(s) && *parse_skip_spaces(e) == '\0';
					if (!words || val_1 < 0 || val_1 > 129 || val_2 < 0 || val_2 > 254) {
						warn("book reduce: two loss limits are needed, from 0 to 129 and from 0 to 254, and nothing after them (book reduce <n1> <n2>); the book is not changed\n");
					} else if (book_reduce(book, &play->board, val_1, val_2)) {
						book_fix(book); // do nothing (or edax is buggy)
						book_negamax(book); // negamax nodes
						book_negamax_subtree(book, &play->board); // (from this position, when the initial one is no longer in the book)
						book_sort(book); // sort moves
					}

				// show the current position as stored in the book
				} else if (strcmp(book_cmd, "show") == 0) {
					book_show(book, &play->board);

				// show book general information
				} else if (strcmp(book_cmd, "info") == 0) {
					book_info(book);

				// show book general information
				} else if (strcmp(book_cmd, "stats") == 0) {
					book_stats(book);


				// set book verbosity
				} else if (strcmp(book_cmd, "verbose") == 0) {
					parse_int(book_param, &book->options.verbosity);
					book->search->options.verbosity = book->options.verbosity;

				// analyze a game from the opening book point of view
				} else if (strcmp(book_cmd, "a") == 0 || strcmp(book_cmd, "analyze") == 0 || strcmp(book_cmd, "analyse") == 0) {
					val_1 = string_to_int(book_param, play->n_game); BOUND(val_1, 1, play->n_game, "depth");
					play_book_analyze(play, val_1);

				// add positions from a game database
				} else if (strcmp(book_cmd, "add") == 0) {
					Base base;
					parse_word(book_param, book_file, FILENAME_MAX);
					base_init(&base);
					base_load(&base, book_file);
					book_add_base(book, &base);
					base_free(&base);

				// play the games of a file (first moves, then edax vs edax) and store them
				} else if (strcmp(book_cmd, "learn") == 0) {
					parse_word(book_param, book_file, FILENAME_MAX);
					play_learn_file(play, book_file);

				// check positions from a game database
				} else if (strcmp(book_cmd, "check") == 0) {
					Base base;
					parse_word(book_param, book_file, FILENAME_MAX);
					base_init(&base);
					base_load(&base, book_file);
					book_check_base(book, &base);
					base_free(&base);

				// extract positions
				} else if (strcmp(book_cmd, "problem") == 0) {
					val_1 = 24; book_param = parse_int(book_param, &val_1); BOUND(val_1, 0, 60, "number of empties");
					val_2 = 10; book_param = parse_int(book_param, &val_2); BOUND(val_2, 1, 1000000, "number of positions");
					book_extract_positions(book, val_1, val_2);
					
				// extract pv to a game database
				} else if (strcmp(book_cmd, "extract") == 0) {
					char word[FILENAME_MAX + 1];
					bool all = false;

					// "book extract <file>" and "book extract draw <file>": the skeleton; "book extract all <file>": every line
					book_param = parse_word(book_param, book_file, FILENAME_MAX);
					parse_word(book_param, word, FILENAME_MAX);
					if (*word && (strcmp(book_file, "all") == 0 || strcmp(book_file, "draw") == 0)) {
						all = (strcmp(book_file, "all") == 0);
						strcpy(book_file, word);
					}
					if (*book_file == '\0') {
						warn("book extract: the name of the file is missing\n");
					} else if (all) {
						book_extract_all(book, book_file);
					} else {
						Base base;

						base_init(&base);
						book_extract_skeleton(book, &base);
						if (book->failed) warn("book extract: the lines could not all be held in memory: %s is left as it is\n", book_file);
						else if (!base_save(&base, book_file)) warn("book extract: the games were not saved to %s\n", book_file);
						base_free(&base);
					}

				// add position using the "deviate algorithm"
				} else if (strcmp(book_cmd, "deviate") == 0) {
					val_1 = 2; book_param = parse_int(book_param, &val_1); BOUND(val_1, -129, 129, "relative error");
					val_2 = 4; book_param = parse_int(book_param, &val_2); BOUND(val_2, 0, 65, "absolute error");
					if (!deviate_export(play, book_param, 1, val_1, val_2)) book_deviate(book, &play->board, val_1, val_2);

				// add positions while limiting per-move and cumulative errors
				} else if (strcmp(book_cmd, "deviate2") == 0) {
					book_param = parse_loss_limits(book_param, &val_1, &val_2);
					if (!deviate_export(play, book_param, 2, val_1, val_2)) book_deviate2(book, &play->board, val_1, val_2);

				} else if (strcmp(book_cmd, "deviate3") == 0) {
					book_param = parse_loss_limits(book_param, &val_1, &val_2);
					if (!deviate_export(play, book_param, 3, val_1, val_2)) book_deviate3(book, &play->board, val_1, val_2);

				// search again the leaves that book deviate (1, 3) or book deviate2 (2, 4) reaches:
				// the leaves it would expand (1, 2), or the leaves of all the positions of its walk (3, 4)
				} else if (strcmp(book_cmd, "leaf recalculate") == 0 || strcmp(book_cmd, "leaf recalculate3") == 0) {
					val_1 = 2; book_param = parse_int(book_param, &val_1); BOUND(val_1, -129, 129, "relative error");
					val_2 = 4; book_param = parse_int(book_param, &val_2); BOUND(val_2, 0, 65, "absolute error");
					book_leaf_recalculate(book, &play->board, book_cmd[16] == '3' ? 3 : 1, val_1, val_2);

				} else if (strcmp(book_cmd, "leaf recalculate2") == 0 || strcmp(book_cmd, "leaf recalculate4") == 0) {
					book_param = parse_loss_limits(book_param, &val_1, &val_2);
					book_leaf_recalculate(book, &play->board, book_cmd[16] == '4' ? 4 : 2, val_1, val_2);

				// expand the leaves better than every link of their position: of the whole book, or with a per-move
				// and a cumulative loss limit, of the positions of a deviate3 walk
				} else if (strcmp(book_cmd, "leaf expand") == 0) {
					char *limits = parse_skip_spaces(book_param);
					if (starts_with_number(limits)) { // (the limits are numbers, signed or not: another word is ignored, as by the other commands)
						book_param = parse_loss_limits(limits, &val_1, &val_2);
						book_leaf_expand(book, &play->board, true, val_1, val_2);
					} else {
						book_leaf_expand(book, &play->board, false, 0, 0);
					}

#ifdef BOOK_TEST_PICK_COUNT
				// test builds: count what book fill 2 / fill2 2 / leaf expand would start from, without and with limits
				} else if (strcmp(book_cmd, "pick-count") == 0) {
					val_1 = 5; book_param = parse_int(book_param, &val_1); BOUND(val_1, 0, 129, "per-move loss");
					val_2 = 5; book_param = parse_int(book_param, &val_2); BOUND(val_2, 0, 7740, "cumulative loss");
					book_test_pick_count(book, &play->board, val_1, val_2);
#endif

				// add position using the "enhance algorithm"
				} else if (strcmp(book_cmd, "enhance") == 0) {
					val_1 = 2; book_param = parse_int(book_param, &val_1); BOUND(val_1, 0, 129, "midgame error");
					val_2 = 4; book_param = parse_int(book_param, &val_2); BOUND(val_2, 0, 129, "endcut error");
					book_enhance(book, &play->board, val_1, val_2);

				// add position by filling hole in the book
				// (fill2: from the leaves only; with a per-move and a cumulative loss limit: from the positions of a deviate3 walk)
				} else if (strcmp(book_cmd, "fill") == 0 || strcmp(book_cmd, "fill2") == 0) {
					const bool leaf_only = (book_cmd[4] == '2');
					char *limits;
					int total_loss;
					val_1 = leaf_only ? 2 : 1; book_param = parse_int(book_param, &val_1); BOUND(val_1, 1, 61, "fill depth"); // (fill2 1 adds no position)
					limits = parse_skip_spaces(book_param);
					if (starts_with_number(limits)) { // (the limits are numbers, signed or not: another word is ignored, as by the other commands)
						book_param = parse_loss_limits(limits, &val_2, &total_loss);
						book_fill_walk(book, &play->board, val_1, leaf_only, val_2, total_loss);
					} else if (leaf_only) {
						book_fill2(book, val_1);
					} else {
						book_fill(book, val_1);
					}

				// add positions by expanding the leaves that no link is better than, from the current position
				} else if (strcmp(book_cmd, "extend") == 0) {
					book_extend(book, &play->board);

				// add every move from the current position to the depth of the book
				} else if (strcmp(book_cmd, "exhaust") == 0) {
					char word[32];
					parse_word(book_param, word, sizeof word - 1);
					if (*word) warn("book exhaust takes no parameter; nothing is done\n");
					else book_exhaust(book, &play->board);

				// add positions by expanding positions with no-link
				} else if (strcmp(book_cmd, "play") == 0) {
					char word[32];
					parse_word(book_param, word, sizeof word - 1);
					if (*word == '\0') { // the whole book, to its depth
						book_play(book);
					} else if (starts_with_number(word)) { // from the current position, to n empty squares
						val_1 = string_to_int(word, 20); BOUND(val_1, 0, 60, "number of empties");
						book_play_to(book, &play->board, val_1);
					} else {
						warn("book play: %s is not a number of empty squares; nothing is done\n", word);
					}

				// add positions by expanding positions with no-link
				} else if (strcmp(book_cmd, "deepen") == 0) {
					book_deepen(book);

				// add book positions to the hash table
				} else if (strcmp(book_cmd, "feed-hash") == 0) {
					book_feed_hash(book, &play->board, &play->search);

				// wrong command ?
				} else {
					warn("Unknown book command: \"%s %s\"\n", cmd, param);
				}
				// the searches used to learn games or to link the book at the same time (book-store-tasks) are
				// kept from a book store to the next one (as with auto-store), and released by the other commands
				if (strcmp(book_cmd, "store") != 0) book_store_release();
				book_pool_reclaim(book); // (no position is pointed to between two commands)
				book->options.verbosity = book->search->options.verbosity;
				book->search->options.verbosity = options.verbosity;
				play_no_stop(play, false);

			/* base TODO: add more actions... */
			} else if (strcmp(cmd, "base") == 0) {
				char base_file[FILENAME_MAX + 1];
				char base_cmd[512], *base_param;
				Base base;

				play_no_stop(play, true); // (as the book commands)
				base_init(&base);
				base_param = parse_word(param, base_cmd, 511);
				string_to_lowercase(base_cmd); // "base Problem ..." as "base problem ..." (the file names are kept as typed)
				base_param = parse_word(base_param, base_file, FILENAME_MAX);

				// extract problem from a game base
				if (strcmp(base_cmd, "problem") == 0) {
					char problem_file[FILENAME_MAX + 1];
					int n_empties = 24;
					base_param = parse_int(base_param, &n_empties);
					base_param = parse_word(base_param, problem_file, FILENAME_MAX);

					base_load(&base, base_file);
					base_to_problem(&base, n_empties, problem_file);

				// extract FEN 
				} else if (strcmp(base_cmd, "tofen") == 0) {
					char problem_file[FILENAME_MAX + 1];
					int n_empties = 24;
					base_param = parse_int(base_param, &n_empties);
					base_param = parse_word(base_param, problem_file, FILENAME_MAX);

					base_load(&base, base_file);
					base_to_FEN(&base, n_empties, problem_file);
	
				// correct erroneous games
				} else if (strcmp(base_cmd, "correct") == 0) {
					int n_empties = 24;
					base_param = parse_int(base_param, &n_empties);

					if (base_load(&base, base_file)) { // a file which was not loaded (or not completely) is kept as it is
						base_analyze(&base, &play->search, n_empties, true);
						base_replace(&base, base_file); // (the file is replaced once the games are saved to another file)
					}

				// check erroneous games
				} else if (strcmp(base_cmd, "check") == 0) {
					int n_empties = 24;
					base_param = parse_int(base_param, &n_empties);

					base_load(&base, base_file);
					base_analyze(&base, &play->search, n_empties, false);

				// terminate unfinished base
				} else if (strcmp(base_cmd, "complete") == 0) {
					if (base_load(&base, base_file)) { // a file which was not loaded (or not completely) is kept as it is
						base_complete(&base, &play->search);
						base_replace(&base, base_file); // (the file is replaced once the games are saved to another file)
					}

				// convert a base to another format
				} else if (strcmp(base_cmd, "convert") == 0) {
					base_load(&base, base_file);
					base_param = parse_word(base_param, base_file, FILENAME_MAX);
					base_save(&base, base_file);

				// make a base unique by removing identical games
				} else if (strcmp(base_cmd, "unique") == 0) {
					base_load(&base, base_file);
					base_param = parse_word(base_param, base_file, FILENAME_MAX);
					base_unique(&base);
					base_save(&base, base_file);

				// compare two game bases
				} else if (strcmp(base_cmd, "compare") == 0) {
					char base_file_2[FILENAME_MAX + 1];
					base_param = parse_word(base_param, base_file_2, FILENAME_MAX);
					base_compare(base_file, base_file_2);

				} else {
					warn("Unknown base command: \"%s %s\"\n", cmd, param);
				}

				base_free(&base);
				play_no_stop(play, false);

			/* edax options */
			} else if (options_read(cmd, param)) {
				options_bound();
				// parallel search changes:
				if (search_count_tasks(&play->search) != options.n_task) {
					play_stop_pondering(play);
					search_set_task_number(&play->search, options.n_task);
				}
				// hash table size changes (as the "memory" command of XBoard does; before, the setting changed
				// and the tables of the search kept the size they had at the start):
				if (play->search.options.hash_size != options.hash_table_size) {
					play_stop_pondering(play);
					search_resize_hashtable(&play->search);
				}

			/* switch to another protocol */
			} else if (strcmp(cmd, "nboard") == 0 && strcmp(param, "1") == 0) {
				free(cmd); free(param);
				play_stop_pondering(play);
				ui->free(ui);
				ui_switch(ui, "nboard");
				ui->init(ui);
				ui->loop(ui);
				return;

			} else if (strcmp(cmd, "xboard") == 0) {
				free(cmd); free(param);
				play_stop_pondering(play);
				ui->free(ui);
				ui_switch(ui, "xboard");
				ui->init(ui);
				ui->loop(ui);
				return;

			} else if (strcmp(cmd, "engine-protocol") == 0 && strcmp(param, "init") == 0) {
				free(cmd); free(param);
				play_stop_pondering(play);
				ui->free(ui);
				ui_switch(ui, "cassio");
				engine_loop();
				return;

			} else if (strcmp(cmd, "protocol_version") == 0) {
				free(cmd); free(param);
				play_stop_pondering(play);
				ui->free(ui);
				ui_switch(ui, "gtp");
				ui->init(ui);
				puts("= 2\n"); fflush(stdout);
				ui->loop(ui);
				return;

#ifdef TUNE_EDAX
			/* edax tuning */
			} else if (strcmp(cmd, "tune") == 0) {
				char problem[FILENAME_MAX];
				char *w_name;
				play_stop_pondering(play);
				w_name = parse_word(param, problem, FILENAME_MAX);
				tune_move_evaluate(&play->search, problem, parse_skip_spaces(w_name));
				search_set_observer(&play->search, edax_observer);
#endif
			/* illegal cmd/move */
			} else {
				char *hint = book_command_hint(cmd, param);
				warn("Unknown command/Illegal move: \"%s %s\"%s\n", cmd, param, hint ? hint : "");
				free(hint);
			}
		}
	}
}

