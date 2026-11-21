/**
 * @file libedax_example.c
 *
 * A short example of a program that uses Edax as a library (libedax).
 *
 * Build (Windows, Visual Studio x64 command prompt). The import library libedax-x64.lib is made with
 * the dll by tests\build-libedax-test.cmd (in tests\libedax); a program can also load the dll at run
 * time (LoadLibrary, or the foreign function interface of its language) and needs no import library:
 *   cl /utf-8 /I..\src libedax_example.c libedax\libedax-x64.lib
 * Build (Linux): gcc -I../src libedax_example.c -L. -l:libedax-x86-64.so -Wl,-rpath,'$ORIGIN' -o libedax_example
 * Build (macOS): clang -I../src libedax_example.c libedax.universal.dylib -Wl,-rpath,@loader_path -o libedax_example
 *
 * Run it where data/eval.dat is (the folder bin of the release), with the library next to the program.
 *
 * @author Nikque
 */

#include <stdio.h>
#include "libedax.h"

int main(void)
{
	/* The settings are read from edax.ini, then config.ini (current folder), then from these
	 * arguments (the first one is ignored), written as the options of the edax program. */
	char *args[] = {"", "-eval-file", "data/eval.dat", "-book-file", "data/book.dat", "-level", "12", "-n-tasks", "2"};
	char moves[] = "f5d6c3";                 /* edax_play() needs a string that it can modify */
	char played[2 * LIBEDAX_LINE_SIZE + 1];
	static LibedaxHintList hints;            /* (large: not on the stack) */
	LibedaxBoard board;
	LibedaxMove last;
	int i;

	libedax_initialize(9, args);
	edax_init();                             /* new game from the initial position */
	edax_play(moves);                        /* play some moves */

	edax_hint(2, &hints);                    /* the 2 best moves: hint[1] to hint[n_hints] */
	for (i = 1; i <= hints.n_hints; ++i) {
		printf("hint %d: %c%c, score %+d, depth %d%s\n", i, 'a' + hints.hint[i].move % 8, '1' + hints.hint[i].move / 8,
			hints.hint[i].score, hints.hint[i].depth, hints.hint[i].book_move ? " (book)" : "");
	}

	edax_go();                               /* Edax plays a move */
	edax_get_last_move(&last);               /* squares: A1 = 0, B1 = 1, ..., H8 = 63 */
	printf("Edax played %c%c\n", 'a' + last.x % 8, '1' + last.x / 8);

	edax_get_board(&board);                  /* discs of the player to move and of its opponent */
	printf("moves: %s, discs: %d - %d, player to move: %d, board: %016llx %016llx\n", edax_get_moves(played),
		edax_get_disc(0), edax_get_disc(1), edax_get_current_player(), board.player, board.opponent);

	libedax_terminate();
	return 0;
}
