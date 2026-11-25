/**
 * @file ui.c
 *
 * @brief User interface.
 *
 * @date 1998 - 2020
 * @author Richard Delorme
 * @version 4.4
 */

#include "options.h"
#include "ui.h"
#include "util.h"
#include <assert.h>
#ifdef _WIN32
#include <io.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

void gtp_preprocess(char *line);
bool event_is_secret(const char *line);

/**
 * @brief Check if a line switches the Edax protocol to Cassio ("engine-protocol init", read as ui_loop_edax reads it).
 * @param line Line read.
 * @return true if the Cassio engine reads the standard input after this line.
 */
static bool ui_is_cassio_switch(const char *line)
{
	const size_t n = strlen(line) + 1;
	char *cmd = (char*) malloc(n), *param = (char*) malloc(n);
	bool ok = false;

	if (cmd && param) {
		parse_command(line, cmd, param, (unsigned int) n);
		ok = (strcmp(cmd, "engine-protocol") == 0 && strcmp(param, "init") == 0);
	} else {
		fatal_error("cannot allocate a command of %d characters\n", (int) n);
	}
	free(cmd);
	free(param);
	return ok;
}

/** a save of the book when a user interface ended failed: the process ends with the exit code 1 */
static bool ui_save_failed = false;

/**
 * @brief Save the book when a user interface ends, if it changed.
 *
 * The result is always written to the standard output, also with -q: "@save ok=<0|1> file=<book file>".
 *
 * @param ui User interface.
 * @param to_book_file true: book_save_to_book_file() (a book file replaced by "book new" is kept under another
 * name); false: book_save() to the book file of the settings.
 * @return false if the book could not be saved.
 */
bool ui_book_save_at_end(UI *ui, const bool to_book_file)
{
	bool ok;

	if (!ui->book.need_saving) return true;
	ok = to_book_file ? book_save_to_book_file(&ui->book) : book_save(&ui->book, options.book_file);
	printf("@save ok=%d file=%s\n", ok ? 1 : 0, options.book_file ? options.book_file : "");
	fflush(stdout);
	if (!ok) ui_save_failed = true;
	return ok;
}

/** @return true if a save of the book at the end of a user interface failed. */
bool ui_book_save_failed(void)
{
	return ui_save_failed;
}

/** @return true if the standard input is a file (commands given with "edax < commands.txt"). */
static bool stdin_is_file(void)
{
#ifdef _WIN32
	return GetFileType((HANDLE) _get_osfhandle(_fileno(stdin))) == FILE_TYPE_DISK;
#else
	struct stat st;
	return fstat(fileno(stdin), &st) == 0 && S_ISREG(st.st_mode);
#endif
}

/**
 * @brief Switch between different User Interface
 *
 * @param ui User Interface.
 * @param ui_type A string describing the chosen user interface.
 * @return true if successful.
 */
bool ui_switch(UI *ui, const char *ui_type)
{
	if (strcmp(ui_type, "edax") == 0) {
		ui->type = UI_EDAX;
		ui->init = ui_init_edax;
		ui->free = ui_free_edax;
		ui->loop = ui_loop_edax;
		return true;
	} else if (strcmp(ui_type, "gtp") == 0) {
		ui->type = UI_GTP;
		ui->init = ui_init_gtp;
		ui->free = ui_free_gtp;
		ui->loop = ui_loop_gtp;
		return true;
	} else if (strcmp(ui_type, "nboard") == 0) {
		ui->type = UI_NBOARD;
		ui->init = ui_init_nboard;
		ui->free = ui_free_nboard;
		ui->loop = ui_loop_nboard;
		return true;
	} else if (strcmp(ui_type, "xboard") == 0) {
		ui->type = UI_XBOARD;
		ui->init = ui_init_xboard;
		ui->free = ui_free_xboard;
		ui->loop = ui_loop_xboard;
		return true;
	} else if (strcmp(ui_type, "ggs") == 0) {
		ui->type = UI_GGS;
		ui->init = ui_init_ggs;
		ui->free = ui_free_ggs;
		ui->loop = ui_loop_ggs;
		return true;
	} else if (strcmp(ui_type, "cassio") == 0) {
		ui->type = UI_CASSIO;
		return true;
	}

	return false;
}

/**
 * @brief A stop is for the command being executed, and for those that wait: remember it until they are done.
 *
 * search_run() forgets a stop that came before it began: it looks at play->stop_pending. A book or base command
 * is not stopped (see play_stop).
 *
 * @param ui User interface (its event is locked).
 */
static void ui_stop_waiting(UI *ui)
{
	Play *const play = ui->play;
	Event *const event = &ui->event;
	const int n = (event->busy ? 1 : 0) + event_count_messages(event);

	lock(&play->no_stop);
	if (play->no_stop.n == 0 && n > 0) {
		event->stop_left = n;
		play->stop_pending = true;
	}
	unlock(&play->no_stop);
}


/**
 * @brief Get an event
 *
 * Wait for an event from the standard input.
 *
 * @param ui User Interface.
 */
static void ui_read_input(UI *ui)
{
	char *buffer;
	char cmd[8];
	Event *const event = &ui->event;
	Play *const play = ui->play;

	buffer = string_read_line(stdin);

	// the play and the event are only used under the lock of the event, while the user interface goes on: when it
	// ends, it may free them (up to v4.5.5-nikque.15 a line read then could use them once freed)
	lock(event);
	if (event->closing) {
		event->loop = false;
		unlock(event);
		free(buffer);
		return;
	}
	if (buffer == NULL) {
		if (ferror(stdin)) {
			if (errno == EINTR) { // interrupted: read again (up to v4.5.5-nikque.15 the input ended without "eof")
				clearerr(stdin);
				unlock(event);
				return;
			}
			unlock(event);
			fatal_error("stdin is broken\n");
		} else if (feof(stdin) && ui->type != UI_GGS) {
			// GGS can continue receiving server events after local input closes.
			buffer = string_duplicate("eof");
		}
		event->loop = false;
	} else {
		if (ui->type == UI_GTP) gtp_preprocess(buffer);
		parse_word(buffer, cmd, sizeof cmd);
		string_to_lowercase(cmd);
		if (strcmp(cmd, "stop") == 0 && ui->type == UI_EDAX && book_analysis_stop(buffer)) {
			// "stop id=<n>": only the book analysis <n> stops (the commands waiting are kept)
			free(buffer);
			buffer = NULL;
		} else if (strcmp(cmd, "stop") == 0) {
			event_clear_messages(event);
			info("<stop>\n");
			play->nboard_go = 0; // (the "go" that waited are cleared: none of them is left to answer)
			play->stop_pending = false;
			event->stop_left = 0;
			if (ui->type != UI_GGS) ui_stop_waiting(ui);
			play_stop(play);
			if (ui->type == UI_GGS) play_stop(play + 1);
		} else if (ui->type == UI_NBOARD && strcmp(cmd, "ping") == 0) {
			// ping stops the thinking: also the one of a "go" that still waits, or whose search has not begun
			if (play->nboard_go > 0) play->stop_pending = true;
			play_stop(play);
		} else if (ui->type == UI_NBOARD && strcmp(cmd, "go") == 0) {
			++play->nboard_go;
		} else if (ui->type == UI_XBOARD && strcmp(cmd, "?") == 0) {
			ui_stop_waiting(ui);
			play_stop(play);
		} else if (ui->type == UI_EDAX && ui_is_cassio_switch(buffer)) {
			// the Cassio engine reads the standard input from now on (engine_loop): this thread reads no more
			event->loop = false;
		} else {
			if (strcmp(cmd, "quit") == 0 || (strcmp(cmd, "q") == 0 &&
			    (ui->type == UI_EDAX || ui->type == UI_XBOARD || ui->type == UI_GGS))) {
				// Commands read from a file are all read at once: there, quit is done in its turn, after the
				// commands before it. Typed (or sent through a pipe), it stops the search and quits at once.
				if (!(ui->type == UI_EDAX && stdin_is_file())) {
					event_clear_messages(event);
					play_stop(play);
					if (ui->type == UI_GGS) play_stop(play + 1);
				}
				event->loop = false;
			}
		}
	}
	if (buffer) {
		event_add_message(event, buffer);
		condition_signal(event);
	}
	unlock(event);
}


/**
 * @brief Read event loop.
 *
 * @param v Engine.
 */
static void* ui_read_input_loop(void *v)
{
	UI *const ui = (UI*) v;

	// a last line without '\n' sets the end of the file while it is read: it is read again, and gives "eof"
	// (ui_read_input ends the loop at the end of the input, or when the input is broken)
	while (ui->event.loop) {
		ui_read_input(ui);
	}

	info("<exit ui_read_input>\n");
	lock(&ui->event);
	ui->event.reading = false;
	unlock(&ui->event);

	return NULL;
}


/**
 * @brief Wait input.
 *
 * @param ui User interface.
 * @param cmd Command.
 * @param param Command's parameters.
 */
void ui_event_wait(UI *ui, char **cmd, char **param)
{
	Event *const event = &ui->event;

	lock(event);
	if (event->busy) { // the command taken before is done
		event->busy = false;
		if (event->stop_left > 0 && --event->stop_left == 0) ui->play->stop_pending = false;
	}
	unlock(event);
	event_wait(&ui->event, cmd, param);
	if (options.echo && *cmd) printf(" %s %s\n", *cmd, event_is_secret(*cmd) ? "(password)" : (*param ? *param : ""));
}

/**
 * @brief Wait input.
 *
 * @param ui User interface.
 * @param cmd Command.
 * @param param Command's parameters.
 */
bool ui_event_peek(UI *ui, char **cmd, char **param)
{
	int n;
	char *message;

	if ((message = event_peek_message(&ui->event)) != NULL) {

		free(*cmd); *cmd = NULL;
		free(*param); *param = NULL;

		n = strlen(message);
		*cmd = (char*) malloc(n + 1);
		*param = (char*) malloc(n + 1);
		if (*cmd == NULL || *param == NULL) fatal_error("cannot allocate a command of %d characters\n", n);
		parse_command(message, *cmd, *param, n + 1);
		free(message);
		return true;
	} else {
		return false;
	}
}

/**
 * @brief ui_event_exist
 *
 * Locked version of event_exist.
 *
 * @param ui User interface.
 * @return true if an event is waiting.
 */
bool ui_event_exist(UI *ui)
{
	bool ok;

	spin_lock(&ui->event);
	ok = event_exist(&ui->event);
	spin_unlock(&ui->event);	

	return ok;
}

/**
 * @brief Create a new Othello User Interface.
 *
 * Allocate event resources & launch user-input event thread.
 *
 */
void ui_event_init(UI *ui)
{
	event_init(&ui->event);
	ui->event.reading = true;
	
	if (!thread_create(&ui->event.thread, ui_read_input_loop, ui)) { // no command could be read
		ui->event.reading = false;
		fatal_error("cannot create the thread that reads the input\n");
	}
	thread_detach(ui->event.thread);
}

/**
 * @brief Tell the input thread that the user interface ends: what it reads then is not used.
 *
 * Called before the user interface frees its play.
 *
 * @param ui User interface.
 */
void ui_event_close(UI *ui)
{
	lock(&ui->event);
	ui->event.closing = true;
	unlock(&ui->event);
}

/**
 * @brief Free events.
 *
 * @param ui User interface.
 * @return true if the events were freed (false: the input thread still runs, and uses them).
 */
bool ui_event_free(UI *ui)
{
	bool reading;

	lock(&ui->event);
	ui->event.closing = true;
	reading = ui->event.reading;
	unlock(&ui->event);
	// the input thread still waits for a line: it uses the event (in the user interface) when it gets one, so
	// they are left to the end of the process
	if (reading) return false;
	event_free(&ui->event);
	return true;
}
