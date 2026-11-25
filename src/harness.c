/**
 * @file harness.c
 *
 * This module verifies if the move generator is correct.
 *
 * It compares board_get_moves() and board_get_move_flip() with a slow
 * generator, on the starting position (whose legal moves are d3, c4, f5
 * and e6) and on every position of random games. It is a program of its
 * own, built with the whole engine (all.c) from the root of the repository:
 *
 *   cl /nologo /utf-8 /O1 /MT /D_CRT_SECURE_NO_DEPRECATE /D HAS_CPU_64 /D POPCOUNT /arch:AVX2 /Iinclude src\harness.c ws2_32.lib /Fe:harness.exe
 *   gcc -O1 -std=gnu11 -D_GNU_SOURCE=1 -DUNICODE -DHAS_CPU_64 -DPOPCOUNT -march=x86-64-v3 -Iinclude src/harness.c -lm -lpthread -o harness
 *
 * Usage: harness [games] (1000 by default). The exit code is 0 when every
 * move is right, 1 otherwise. The case core of tests/regression.c makes the
 * same comparison, with exact searches.
 *
 * @date 1998 - 2017
 * @author Richard Delorme
 * @version 4.4
 */

#define main edax_main
#include "all.c"
#undef main

/** the squares flipped by a move at x0 of the player P against the opponent O (0 when the move is not legal) */
static unsigned long long flip_slow(const unsigned long long P, const unsigned long long O, const int x0)
{
	int x, d;
	const int dir[8] = {-9,-8,-7,-1,1,7,8,9};
	const unsigned long long edge[8] = {
		0x01010101010101ffull,
		0x00000000000000ffull,
		0x80808080808080ffull,
		0x0101010101010101ull,
		0x8080808080808080ull,
		0xff01010101010101ull,
		0xff00000000000000ull,
		0xff80808080808080ull
	};
	unsigned long long flipped = 0, f;

	if (x0 < A1 || x0 > H8 || ((P | O) & x_to_bit(x0))) return 0;

	for (d = 0; d < 8; ++d) {
		if ((x_to_bit(x0) & edge[d]) == 0) {
			f = 0;
			for (x = x0 + dir[d]; (O & x_to_bit(x)) && (x_to_bit(x) & edge[d]) == 0; x += dir[d]) {
				f |= x_to_bit(x);
			}
			if (P & x_to_bit(x)) flipped |= f;
		}
	}
	return flipped;
}

/** the legal moves of the player of the board, by flip_slow */
static unsigned long long moves_slow(const Board *board)
{
	unsigned long long moves = 0;
	int x;

	for (x = A1; x <= H8; ++x) if (flip_slow(board->player, board->opponent, x)) moves |= x_to_bit(x);
	return moves;
}

/** compares the generators on a board; false (said) at the first difference */
static bool check_board(const Board *board, unsigned long long *n_checked)
{
	unsigned long long moves = moves_slow(board), flipped;
	Move move;
	char s[4];
	int x;

	if (board_get_moves(board) != moves) {
		printf("Bug found in board_get_moves()\n");
		board_print(board, BLACK, stdout);
		bitboard_write(board_get_moves(board), stdout);
		bitboard_write(moves, stdout);
		return false;
	}
	for (x = A1; x <= H8; ++x) {
		if ((board->player | board->opponent) & x_to_bit(x)) continue;
		flipped = board_get_move_flip(board, x, &move);
		++*n_checked;
		if (flipped != flip_slow(board->player, board->opponent, x)) {
			printf("Bug found in board_get_move_flip(%s)\n", move_to_string(x, BLACK, s));
			board_print(board, BLACK, stdout);
			bitboard_write(flipped, stdout);
			bitboard_write(flip_slow(board->player, board->opponent, x), stdout);
			return false;
		}
	}
	return true;
}

int main(int argc, char **argv)
{
	const unsigned long long start_moves = x_to_bit(D3) | x_to_bit(C4) | x_to_bit(F5) | x_to_bit(E6);
	unsigned long long n_checked = 0, n_positions = 0, moves;
	unsigned int seed = 0x651197;
	int n_games = argc > 1 ? atoi(argv[1]) : 1000, i, k;
	Board board;

	bit_init(); // (the table X_TO_BIT of board_next)
	board_init(&board);
	if (moves_slow(&board) != start_moves || board_get_moves(&board) != start_moves) {
		printf("Bug found: the moves of the starting position are not d3, c4, f5 and e6\n");
		return 1;
	}

	for (i = 0; i < n_games; ++i) {
		board_init(&board);
		for (;;) {
			if (!check_board(&board, &n_checked)) return 1;
			++n_positions;
			moves = board_get_moves(&board);
			if (moves == 0) {
				board_pass(&board);
				if (board_get_moves(&board) == 0) break;
				continue;
			}
			seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
			k = (int) (seed % (unsigned int) bit_count(moves));
			while (k--) moves &= moves - 1;
			board_next(&board, first_bit(moves), &board);
		}
	}
	printf("harness: %d games, %llu positions, %llu moves checked: ok\n", n_games, n_positions, n_checked);
	return 0;
}
