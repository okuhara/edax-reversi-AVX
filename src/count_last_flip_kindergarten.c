/**
 * @file count_last_flip_kindergarten.c
 *
 *
 * A function is provided to count the number of fipped disc of the last move
 * for each square of the board. These functions are gathered into an array of
 * functions, so that a fast access to each function is allowed. The generic
 * form of the function take as input the player bitboard and return twice
 * the number of flipped disc of the last move.
 *
 * The basic principle is to read into an array a precomputed result. Doing
 * this is easy for a single line ; as we can use arrays of the form:
 *  - COUNT_FLIP[square where we play][8-bits disc pattern].
 * The problem is thus to convert any line of a 64-bits disc pattern into an
 * 8-bits disc pattern. A fast way to do this is to select the right line,
 * with a bit-mask, to gather the masked-bits into a continuous set by a simple
 * multiplication and to right-shift the result to scale it into a number
 * between 0 and 255.
 * Once we get our 8-bits disc patterns, we directly get the number of
 * flipped discs from the precomputed array, and add them from each flipping
 * lines.
 * For optimization purpose, the value returned is twice the number of flipped
 * disc, to facilitate the computation of disc difference.
 *
 * With 135 degree merge, instead of Valery ClaudePierre's modification.
 *
 * @date 1998 - 2025
 * @author Richard Delorme
 * @author Toshihiko Okuhara
 * @version 4.5
 *
 */

#include "bit.h"

/** precomputed count flip array */
/** lower byte for P, upper byte for O */
static const unsigned short COUNT_FLIP_0[128] = {
        0x0000, 0x0200, 0x0002, 0x0400, 0x0004, 0x0200, 0x0002, 0x0600,
        0x0006, 0x0200, 0x0002, 0x0400, 0x0004, 0x0200, 0x0002, 0x0800,
        0x0008, 0x0200, 0x0002, 0x0400, 0x0004, 0x0200, 0x0002, 0x0600,
        0x0006, 0x0200, 0x0002, 0x0400, 0x0004, 0x0200, 0x0002, 0x0a00,
        0x000a, 0x0200, 0x0002, 0x0400, 0x0004, 0x0200, 0x0002, 0x0600,
        0x0006, 0x0200, 0x0002, 0x0400, 0x0004, 0x0200, 0x0002, 0x0800,
        0x0008, 0x0200, 0x0002, 0x0400, 0x0004, 0x0200, 0x0002, 0x0600,
        0x0006, 0x0200, 0x0002, 0x0400, 0x0004, 0x0200, 0x0002, 0x0c00,
        0x000c, 0x0200, 0x0002, 0x0400, 0x0004, 0x0200, 0x0002, 0x0600,
        0x0006, 0x0200, 0x0002, 0x0400, 0x0004, 0x0200, 0x0002, 0x0800,
        0x0008, 0x0200, 0x0002, 0x0400, 0x0004, 0x0200, 0x0002, 0x0600,
        0x0006, 0x0200, 0x0002, 0x0400, 0x0004, 0x0200, 0x0002, 0x0a00,
        0x000a, 0x0200, 0x0002, 0x0400, 0x0004, 0x0200, 0x0002, 0x0600,
        0x0006, 0x0200, 0x0002, 0x0400, 0x0004, 0x0200, 0x0002, 0x0800,
        0x0008, 0x0200, 0x0002, 0x0400, 0x0004, 0x0200, 0x0002, 0x0600,
        0x0006, 0x0200, 0x0002, 0x0400, 0x0004, 0x0200, 0x0002, 0x0000
};

static const unsigned short COUNT_FLIP_1[64] = {
        0x0000, 0x0200, 0x0002, 0x0400, 0x0004, 0x0200, 0x0002, 0x0600,
        0x0006, 0x0200, 0x0002, 0x0400, 0x0004, 0x0200, 0x0002, 0x0800,
        0x0008, 0x0200, 0x0002, 0x0400, 0x0004, 0x0200, 0x0002, 0x0600,
        0x0006, 0x0200, 0x0002, 0x0400, 0x0004, 0x0200, 0x0002, 0x0a00,
        0x000a, 0x0200, 0x0002, 0x0400, 0x0004, 0x0200, 0x0002, 0x0600,
        0x0006, 0x0200, 0x0002, 0x0400, 0x0004, 0x0200, 0x0002, 0x0800,
        0x0008, 0x0200, 0x0002, 0x0400, 0x0004, 0x0200, 0x0002, 0x0600,
        0x0006, 0x0200, 0x0002, 0x0400, 0x0004, 0x0200, 0x0002, 0x0000,
};

static const unsigned short COUNT_FLIP_2[256] = {
	0x0000, 0x0002, 0x0200, 0x0000, 0x0000, 0x0002, 0x0200, 0x0000,
	0x0200, 0x0202, 0x0400, 0x0200, 0x0200, 0x0202, 0x0400, 0x0200,
	0x0002, 0x0004, 0x0202, 0x0002, 0x0002, 0x0004, 0x0202, 0x0002,
	0x0400, 0x0402, 0x0600, 0x0400, 0x0400, 0x0402, 0x0600, 0x0400,
	0x0004, 0x0006, 0x0204, 0x0004, 0x0004, 0x0006, 0x0204, 0x0004,
	0x0200, 0x0202, 0x0400, 0x0200, 0x0200, 0x0202, 0x0400, 0x0200,
	0x0002, 0x0004, 0x0202, 0x0002, 0x0002, 0x0004, 0x0202, 0x0002,
	0x0600, 0x0602, 0x0800, 0x0600, 0x0600, 0x0602, 0x0800, 0x0600,
	0x0006, 0x0008, 0x0206, 0x0006, 0x0006, 0x0008, 0x0206, 0x0006,
	0x0200, 0x0202, 0x0400, 0x0200, 0x0200, 0x0202, 0x0400, 0x0200,
	0x0002, 0x0004, 0x0202, 0x0002, 0x0002, 0x0004, 0x0202, 0x0002,
	0x0400, 0x0402, 0x0600, 0x0400, 0x0400, 0x0402, 0x0600, 0x0400,
	0x0004, 0x0006, 0x0204, 0x0004, 0x0004, 0x0006, 0x0204, 0x0004,
	0x0200, 0x0202, 0x0400, 0x0200, 0x0200, 0x0202, 0x0400, 0x0200,
	0x0002, 0x0004, 0x0202, 0x0002, 0x0002, 0x0004, 0x0202, 0x0002,
	0x0800, 0x0802, 0x0a00, 0x0800, 0x0800, 0x0802, 0x0a00, 0x0800,
	0x0008, 0x000a, 0x0208, 0x0008, 0x0008, 0x000a, 0x0208, 0x0008,
	0x0200, 0x0202, 0x0400, 0x0200, 0x0200, 0x0202, 0x0400, 0x0200,
	0x0002, 0x0004, 0x0202, 0x0002, 0x0002, 0x0004, 0x0202, 0x0002,
	0x0400, 0x0402, 0x0600, 0x0400, 0x0400, 0x0402, 0x0600, 0x0400,
	0x0004, 0x0006, 0x0204, 0x0004, 0x0004, 0x0006, 0x0204, 0x0004,
	0x0200, 0x0202, 0x0400, 0x0200, 0x0200, 0x0202, 0x0400, 0x0200,
	0x0002, 0x0004, 0x0202, 0x0002, 0x0002, 0x0004, 0x0202, 0x0002,
	0x0600, 0x0602, 0x0800, 0x0600, 0x0600, 0x0602, 0x0800, 0x0600,
	0x0006, 0x0008, 0x0206, 0x0006, 0x0006, 0x0008, 0x0206, 0x0006,
	0x0200, 0x0202, 0x0400, 0x0200, 0x0200, 0x0202, 0x0400, 0x0200,
	0x0002, 0x0004, 0x0202, 0x0002, 0x0002, 0x0004, 0x0202, 0x0002,
	0x0400, 0x0402, 0x0600, 0x0400, 0x0400, 0x0402, 0x0600, 0x0400,
	0x0004, 0x0006, 0x0204, 0x0004, 0x0004, 0x0006, 0x0204, 0x0004,
	0x0200, 0x0202, 0x0400, 0x0200, 0x0200, 0x0202, 0x0400, 0x0200,
	0x0002, 0x0004, 0x0202, 0x0002, 0x0002, 0x0004, 0x0202, 0x0002,
	0x0000, 0x0002, 0x0200, 0x0000, 0x0000, 0x0002, 0x0200, 0x0000
};

static const unsigned short COUNT_FLIP_3[256] = {
	0x0000, 0x0004, 0x0002, 0x0002, 0x0200, 0x0200, 0x0400, 0x0000,
	0x0000, 0x0004, 0x0002, 0x0002, 0x0200, 0x0200, 0x0400, 0x0000,
	0x0200, 0x0204, 0x0202, 0x0202, 0x0400, 0x0400, 0x0600, 0x0200,
	0x0200, 0x0204, 0x0202, 0x0202, 0x0400, 0x0400, 0x0600, 0x0200,
	0x0002, 0x0006, 0x0004, 0x0004, 0x0202, 0x0202, 0x0402, 0x0002,
	0x0002, 0x0006, 0x0004, 0x0004, 0x0202, 0x0202, 0x0402, 0x0002,
	0x0400, 0x0404, 0x0402, 0x0402, 0x0600, 0x0600, 0x0800, 0x0400,
	0x0400, 0x0404, 0x0402, 0x0402, 0x0600, 0x0600, 0x0800, 0x0400,
	0x0004, 0x0008, 0x0006, 0x0006, 0x0204, 0x0204, 0x0404, 0x0004,
	0x0004, 0x0008, 0x0006, 0x0006, 0x0204, 0x0204, 0x0404, 0x0004,
	0x0200, 0x0204, 0x0202, 0x0202, 0x0400, 0x0400, 0x0600, 0x0200,
	0x0200, 0x0204, 0x0202, 0x0202, 0x0400, 0x0400, 0x0600, 0x0200,
	0x0002, 0x0006, 0x0004, 0x0004, 0x0202, 0x0202, 0x0402, 0x0002,
	0x0002, 0x0006, 0x0004, 0x0004, 0x0202, 0x0202, 0x0402, 0x0002,
	0x0600, 0x0604, 0x0602, 0x0602, 0x0800, 0x0800, 0x0a00, 0x0600,
	0x0600, 0x0604, 0x0602, 0x0602, 0x0800, 0x0800, 0x0a00, 0x0600,
	0x0006, 0x000a, 0x0008, 0x0008, 0x0206, 0x0206, 0x0406, 0x0006,
	0x0006, 0x000a, 0x0008, 0x0008, 0x0206, 0x0206, 0x0406, 0x0006,
	0x0200, 0x0204, 0x0202, 0x0202, 0x0400, 0x0400, 0x0600, 0x0200,
	0x0200, 0x0204, 0x0202, 0x0202, 0x0400, 0x0400, 0x0600, 0x0200,
	0x0002, 0x0006, 0x0004, 0x0004, 0x0202, 0x0202, 0x0402, 0x0002,
	0x0002, 0x0006, 0x0004, 0x0004, 0x0202, 0x0202, 0x0402, 0x0002,
	0x0400, 0x0404, 0x0402, 0x0402, 0x0600, 0x0600, 0x0800, 0x0400,
	0x0400, 0x0404, 0x0402, 0x0402, 0x0600, 0x0600, 0x0800, 0x0400,
	0x0004, 0x0008, 0x0006, 0x0006, 0x0204, 0x0204, 0x0404, 0x0004,
	0x0004, 0x0008, 0x0006, 0x0006, 0x0204, 0x0204, 0x0404, 0x0004,
	0x0200, 0x0204, 0x0202, 0x0202, 0x0400, 0x0400, 0x0600, 0x0200,
	0x0200, 0x0204, 0x0202, 0x0202, 0x0400, 0x0400, 0x0600, 0x0200,
	0x0002, 0x0006, 0x0004, 0x0004, 0x0202, 0x0202, 0x0402, 0x0002,
	0x0002, 0x0006, 0x0004, 0x0004, 0x0202, 0x0202, 0x0402, 0x0002,
	0x0000, 0x0004, 0x0002, 0x0002, 0x0200, 0x0200, 0x0400, 0x0000,
	0x0000, 0x0004, 0x0002, 0x0002, 0x0200, 0x0200, 0x0400, 0x0000
};

static const unsigned short COUNT_FLIP_4[256] = {
	0x0000, 0x0006, 0x0004, 0x0004, 0x0002, 0x0002, 0x0002, 0x0002,
	0x0200, 0x0200, 0x0200, 0x0200, 0x0400, 0x0400, 0x0600, 0x0000,
	0x0000, 0x0006, 0x0004, 0x0004, 0x0002, 0x0002, 0x0002, 0x0002,
	0x0200, 0x0200, 0x0200, 0x0200, 0x0400, 0x0400, 0x0600, 0x0000,
	0x0200, 0x0206, 0x0204, 0x0204, 0x0202, 0x0202, 0x0202, 0x0202,
	0x0400, 0x0400, 0x0400, 0x0400, 0x0600, 0x0600, 0x0800, 0x0200,
	0x0200, 0x0206, 0x0204, 0x0204, 0x0202, 0x0202, 0x0202, 0x0202,
	0x0400, 0x0400, 0x0400, 0x0400, 0x0600, 0x0600, 0x0800, 0x0200,
	0x0002, 0x0008, 0x0006, 0x0006, 0x0004, 0x0004, 0x0004, 0x0004,
	0x0202, 0x0202, 0x0202, 0x0202, 0x0402, 0x0402, 0x0602, 0x0002,
	0x0002, 0x0008, 0x0006, 0x0006, 0x0004, 0x0004, 0x0004, 0x0004,
	0x0202, 0x0202, 0x0202, 0x0202, 0x0402, 0x0402, 0x0602, 0x0002,
	0x0400, 0x0406, 0x0404, 0x0404, 0x0402, 0x0402, 0x0402, 0x0402,
	0x0600, 0x0600, 0x0600, 0x0600, 0x0800, 0x0800, 0x0a00, 0x0400,
	0x0400, 0x0406, 0x0404, 0x0404, 0x0402, 0x0402, 0x0402, 0x0402,
	0x0600, 0x0600, 0x0600, 0x0600, 0x0800, 0x0800, 0x0a00, 0x0400,
	0x0004, 0x000a, 0x0008, 0x0008, 0x0006, 0x0006, 0x0006, 0x0006,
	0x0204, 0x0204, 0x0204, 0x0204, 0x0404, 0x0404, 0x0604, 0x0004,
	0x0004, 0x000a, 0x0008, 0x0008, 0x0006, 0x0006, 0x0006, 0x0006,
	0x0204, 0x0204, 0x0204, 0x0204, 0x0404, 0x0404, 0x0604, 0x0004,
	0x0200, 0x0206, 0x0204, 0x0204, 0x0202, 0x0202, 0x0202, 0x0202,
	0x0400, 0x0400, 0x0400, 0x0400, 0x0600, 0x0600, 0x0800, 0x0200,
	0x0200, 0x0206, 0x0204, 0x0204, 0x0202, 0x0202, 0x0202, 0x0202,
	0x0400, 0x0400, 0x0400, 0x0400, 0x0600, 0x0600, 0x0800, 0x0200,
	0x0002, 0x0008, 0x0006, 0x0006, 0x0004, 0x0004, 0x0004, 0x0004,
	0x0202, 0x0202, 0x0202, 0x0202, 0x0402, 0x0402, 0x0602, 0x0002,
	0x0002, 0x0008, 0x0006, 0x0006, 0x0004, 0x0004, 0x0004, 0x0004,
	0x0202, 0x0202, 0x0202, 0x0202, 0x0402, 0x0402, 0x0602, 0x0002,
	0x0000, 0x0006, 0x0004, 0x0004, 0x0002, 0x0002, 0x0002, 0x0002,
	0x0200, 0x0200, 0x0200, 0x0200, 0x0400, 0x0400, 0x0600, 0x0000,
	0x0000, 0x0006, 0x0004, 0x0004, 0x0002, 0x0002, 0x0002, 0x0002,
	0x0200, 0x0200, 0x0200, 0x0200, 0x0400, 0x0400, 0x0600, 0x0000
};

static const unsigned short COUNT_FLIP_5[256] = {
	0x0000, 0x0008, 0x0006, 0x0006, 0x0004, 0x0004, 0x0004, 0x0004,
	0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002,
	0x0200, 0x0200, 0x0200, 0x0200, 0x0200, 0x0200, 0x0200, 0x0200,
	0x0400, 0x0400, 0x0400, 0x0400, 0x0600, 0x0600, 0x0800, 0x0000,
	0x0000, 0x0008, 0x0006, 0x0006, 0x0004, 0x0004, 0x0004, 0x0004,
	0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002,
	0x0200, 0x0200, 0x0200, 0x0200, 0x0200, 0x0200, 0x0200, 0x0200,
	0x0400, 0x0400, 0x0400, 0x0400, 0x0600, 0x0600, 0x0800, 0x0000,
	0x0200, 0x0208, 0x0206, 0x0206, 0x0204, 0x0204, 0x0204, 0x0204,
	0x0202, 0x0202, 0x0202, 0x0202, 0x0202, 0x0202, 0x0202, 0x0202,
	0x0400, 0x0400, 0x0400, 0x0400, 0x0400, 0x0400, 0x0400, 0x0400,
	0x0600, 0x0600, 0x0600, 0x0600, 0x0800, 0x0800, 0x0a00, 0x0200,
	0x0200, 0x0208, 0x0206, 0x0206, 0x0204, 0x0204, 0x0204, 0x0204,
	0x0202, 0x0202, 0x0202, 0x0202, 0x0202, 0x0202, 0x0202, 0x0202,
	0x0400, 0x0400, 0x0400, 0x0400, 0x0400, 0x0400, 0x0400, 0x0400,
	0x0600, 0x0600, 0x0600, 0x0600, 0x0800, 0x0800, 0x0a00, 0x0200,
	0x0002, 0x000a, 0x0008, 0x0008, 0x0006, 0x0006, 0x0006, 0x0006,
	0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004,
	0x0202, 0x0202, 0x0202, 0x0202, 0x0202, 0x0202, 0x0202, 0x0202,
	0x0402, 0x0402, 0x0402, 0x0402, 0x0602, 0x0602, 0x0802, 0x0002,
	0x0002, 0x000a, 0x0008, 0x0008, 0x0006, 0x0006, 0x0006, 0x0006,
	0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004,
	0x0202, 0x0202, 0x0202, 0x0202, 0x0202, 0x0202, 0x0202, 0x0202,
	0x0402, 0x0402, 0x0402, 0x0402, 0x0602, 0x0602, 0x0802, 0x0002,
	0x0000, 0x0008, 0x0006, 0x0006, 0x0004, 0x0004, 0x0004, 0x0004,
	0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002,
	0x0200, 0x0200, 0x0200, 0x0200, 0x0200, 0x0200, 0x0200, 0x0200,
	0x0400, 0x0400, 0x0400, 0x0400, 0x0600, 0x0600, 0x0800, 0x0000,
	0x0000, 0x0008, 0x0006, 0x0006, 0x0004, 0x0004, 0x0004, 0x0004,
	0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002,
	0x0200, 0x0200, 0x0200, 0x0200, 0x0200, 0x0200, 0x0200, 0x0200,
	0x0400, 0x0400, 0x0400, 0x0400, 0x0600, 0x0600, 0x0800, 0x0000
};

static const unsigned short COUNT_FLIP_6[64] = {
        000000, 0x000a, 0x0008, 0x0008, 0x0006, 0x0006, 0x0006, 0x0006,
        0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004,
        0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002,
        0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002,
        0x0200, 0x0200, 0x0200, 0x0200, 0x0200, 0x0200, 0x0200, 0x0200,
        0x0200, 0x0200, 0x0200, 0x0200, 0x0200, 0x0200, 0x0200, 0x0200,
        0x0400, 0x0400, 0x0400, 0x0400, 0x0400, 0x0400, 0x0400, 0x0400,
        0x0600, 0x0600, 0x0600, 0x0600, 0x0800, 0x0800, 0x0a00, 000000
};

static const unsigned short COUNT_FLIP_7[128] = {
        0x0000, 0x000c, 0x000a, 0x000a, 0x0008, 0x0008, 0x0008, 0x0008,
        0x0006, 0x0006, 0x0006, 0x0006, 0x0006, 0x0006, 0x0006, 0x0006,
        0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004,
        0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004,
        0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002,
        0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002,
        0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002,
        0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002,
        0x0200, 0x0200, 0x0200, 0x0200, 0x0200, 0x0200, 0x0200, 0x0200,
        0x0200, 0x0200, 0x0200, 0x0200, 0x0200, 0x0200, 0x0200, 0x0200,
        0x0200, 0x0200, 0x0200, 0x0200, 0x0200, 0x0200, 0x0200, 0x0200,
        0x0200, 0x0200, 0x0200, 0x0200, 0x0200, 0x0200, 0x0200, 0x0200,
        0x0400, 0x0400, 0x0400, 0x0400, 0x0400, 0x0400, 0x0400, 0x0400,
        0x0400, 0x0400, 0x0400, 0x0400, 0x0400, 0x0400, 0x0400, 0x0400,
        0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600,
        0x0800, 0x0800, 0x0800, 0x0800, 0x0a00, 0x0a00, 0x0c00, 0x0000
};

/**
 * Count last flipped discs when playing on square A1.
 *
 * @param P player's disc pattern.
 * @return flipped disc count.
 */
static int count_last_flip_A1(const unsigned long long P)
{
	register int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_0[((P & 0x0101010101010100ULL) * 0x0102040810204080ULL) >> 57];
	n_flipped += (unsigned char) COUNT_FLIP_0[(P >> 1) & 0x7f];
	n_flipped += (unsigned char) COUNT_FLIP_0[((P & 0x8040201008040200ULL) * 0x0101010101010101ULL) >> 57];

	return n_flipped;
}

/**
 * Count last flipped discs when playing on square B1.
 *
 * @param P player's disc pattern.
 * @return flipped disc count.
 */
static int count_last_flip_B1(const unsigned long long P)
{
	register int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_0[((P & 0x0202020202020200ULL) * 0x0081020408102040ULL) >> 57];
	n_flipped += (unsigned char) COUNT_FLIP_1[(P >> 2) & 0x3f];
	n_flipped += (unsigned char) COUNT_FLIP_1[((P & 0x0080402010080400ULL) * 0x0101010101010101ULL) >> 58];

	return n_flipped;
}

/**
 * Count last flipped discs when playing on square C1.
 *
 * @param P player's disc pattern.
 * @return flipped disc count.
 */
static int count_last_flip_C1(const unsigned long long P)
{
	register int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_0[((P & 0x0404040404040400ULL) * 0x0040810204081020ULL) >> 57];
	n_flipped += (unsigned char) COUNT_FLIP_2[P & 0xff];
	n_flipped += (unsigned char) COUNT_FLIP_2[(P & 0x0000804020110a04ULL) * 0x0101010101010101ULL >> 56];	// A3C1H6

	return n_flipped;
}

/**
 * Count last flipped discs when playing on square D1.
 *
 * @param P player's disc pattern.
 * @return flipped disc count.
 */
static int count_last_flip_D1(const unsigned long long P)
{
	register int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_0[((P & 0x0808080808080800ULL) * 0x0020408102040810ULL) >> 57];
	n_flipped += (unsigned char) COUNT_FLIP_3[P & 0xff];
	n_flipped += (unsigned char) COUNT_FLIP_3[(P & 0x0000008041221408ULL) * 0x0101010101010101ULL >> 56];	// A4D1H5

	return n_flipped;
}

/**
 * Count last flipped discs when playing on square E1.
 *
 * @param P player's disc pattern.
 * @return flipped disc count.
 */
static int count_last_flip_E1(const unsigned long long P)
{
	register int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_0[((P & 0x1010101010101000ULL) * 0x0010204081020408ULL) >> 57];
	n_flipped += (unsigned char) COUNT_FLIP_4[P & 0xff];
	n_flipped += (unsigned char) COUNT_FLIP_4[(P & 0x0000000182442810ULL) * 0x0101010101010101ULL >> 56];	// A5E1H4

	return n_flipped;
}

/**
 * Count last flipped discs when playing on square F1.
 *
 * @param P player's disc pattern.
 * @return flipped disc count.
 */
static int count_last_flip_F1(const unsigned long long P)
{
	register int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_0[((P & 0x2020202020202000ULL) * 0x0008102040810204ULL) >> 57];
	n_flipped += (unsigned char) COUNT_FLIP_5[P & 0xff];
	n_flipped += (unsigned char) COUNT_FLIP_5[(P & 0x0000010204885020ULL) * 0x0101010101010101ULL >> 56];	// A6F1H3

	return n_flipped;
}

/**
 * Count last flipped discs when playing on square G1.
 *
 * @param P player's disc pattern.
 * @return flipped disc count.
 */
static int count_last_flip_G1(const unsigned long long P)
{
	register int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_0[((P & 0x4040404040404000ULL) * 0x0004081020408102ULL) >> 57];
	n_flipped += (unsigned char) COUNT_FLIP_6[P & 0x3f];
	n_flipped += (unsigned char) COUNT_FLIP_6[((P & 0x0001020408102000ULL) * 0x0101010101010101ULL) >> 56];

	return n_flipped;
}

/**
 * Count last flipped discs when playing on square H1.
 *
 * @param P player's disc pattern.
 * @return flipped disc count.
 */
static int count_last_flip_H1(const unsigned long long P)
{
	register int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_0[((P & 0x8080808080808000ULL) * 0x0002040810204081ULL) >> 57];
	n_flipped += (unsigned char) COUNT_FLIP_7[P & 0x7f];
	n_flipped += (unsigned char) COUNT_FLIP_7[((P & 0x0102040810204000ULL) * 0x0101010101010101ULL) >> 56];

	return n_flipped;
}

/**
 * Count last flipped discs when playing on square A2.
 *
 * @param P player's disc pattern.
 * @return flipped disc count.
 */
static int count_last_flip_A2(const unsigned long long P)
{
	register int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_1[((P & 0x0101010101010000ULL) * 0x0102040810204080ULL) >> 58];
	n_flipped += (unsigned char) COUNT_FLIP_0[(P >> 9) & 0x7f];
	n_flipped += (unsigned char) COUNT_FLIP_1[((P & 0x4020100804020000ULL) * 0x0101010101010101ULL) >> 57];

	return n_flipped;
}

/**
 * Count last flipped discs when playing on square B2.
 *
 * @param P player's disc pattern.
 * @return flipped disc count.
 */
static int count_last_flip_B2(const unsigned long long P)
{
	register int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_1[((P & 0x0202020202020000ULL) * 0x0081020408102040ULL) >> 58];
	n_flipped += (unsigned char) COUNT_FLIP_1[(P >> 10) & 0x3f];
	n_flipped += (unsigned char) COUNT_FLIP_1[((P & 0x8040201008040000ULL) * 0x0101010101010101ULL) >> 58];

	return n_flipped;
}

/**
 * Count last flipped discs when playing on square C2.
 *
 * @param P player's disc pattern.
 * @return flipped disc count.
 */
static int count_last_flip_C2(const unsigned long long P)
{
	register int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_1[((P & 0x0404040404040000ULL) * 0x0040810204081020ULL) >> 58];
	n_flipped += (unsigned char) COUNT_FLIP_2[(P >> 8) & 0xff];
	n_flipped += (unsigned char) COUNT_FLIP_2[(P & 0x00804020110a0400ULL) * 0x0101010101010101ULL >> 56];	// A4C2H7

	return n_flipped;
}

/**
 * Count last flipped discs when playing on square D2.
 *
 * @param P player's disc pattern.
 * @return flipped disc count.
 */
static int count_last_flip_D2(const unsigned long long P)
{
	register int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_1[((P & 0x0808080808080000ULL) * 0x0020408102040810ULL) >> 58];
	n_flipped += (unsigned char) COUNT_FLIP_3[(P >> 8) & 0xff];
	n_flipped += (unsigned char) COUNT_FLIP_3[(P & 0x0000804122140800ULL) * 0x0101010101010101ULL >> 56];	// A5D2H6

	return n_flipped;
}

/**
 * Count last flipped discs when playing on square E2.
 *
 * @param P player's disc pattern.
 * @return flipped disc count.
 */
static int count_last_flip_E2(const unsigned long long P)
{
	register int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_1[((P & 0x1010101010100000ULL) * 0x0010204081020408ULL) >> 58];
	n_flipped += (unsigned char) COUNT_FLIP_4[(P >> 8) & 0xff];
	n_flipped += (unsigned char) COUNT_FLIP_4[(P & 0x0000018244281000ULL) * 0x0101010101010101ULL >> 56];	// A6E2H5

	return n_flipped;
}

/**
 * Count last flipped discs when playing on square F2.
 *
 * @param P player's disc pattern.
 * @return flipped disc count.
 */
static int count_last_flip_F2(const unsigned long long P)
{
	register int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_1[((P & 0x2020202020200000ULL) * 0x0008102040810204ULL) >> 58];
	n_flipped += (unsigned char) COUNT_FLIP_5[(P >> 8) & 0xff];
	n_flipped += (unsigned char) COUNT_FLIP_5[(P & 0x0001020488502000ULL) * 0x0101010101010101ULL >> 56];	// A7F2H4

	return n_flipped;
}

/**
 * Count last flipped discs when playing on square G2.
 *
 * @param P player's disc pattern.
 * @return flipped disc count.
 */
static int count_last_flip_G2(const unsigned long long P)
{
	register int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_1[((P & 0x4040404040400000ULL) * 0x0004081020408102ULL) >> 58];
	n_flipped += (unsigned char) COUNT_FLIP_6[(P >> 8) & 0x3f];
	n_flipped += (unsigned char) COUNT_FLIP_6[((P & 0x0102040810200000ULL) * 0x0101010101010101ULL) >> 56];

	return n_flipped;
}

/**
 * Count last flipped discs when playing on square H2.
 *
 * @param P player's disc pattern.
 * @return flipped disc count.
 */
static int count_last_flip_H2(const unsigned long long P)
{
	register int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_1[((P & 0x8080808080800000ULL) * 0x0002040810204081ULL) >> 58];
	n_flipped += (unsigned char) COUNT_FLIP_7[(P >> 8) & 0x7f];
	n_flipped += (unsigned char) COUNT_FLIP_6[((P & 0x0204081020400000ULL) * 0x0101010101010101ULL) >> 57];

	return n_flipped;
}

/**
 * Count last flipped discs when playing on square A3.
 *
 * @param P player's disc pattern.
 * @return flipped disc count.
 */
static int count_last_flip_A3(const unsigned long long P)
{
	register int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_2[((P & 0x2010080402010101ULL) * 0x0102040404040404ULL) >> 56];	// A1A3F8
	n_flipped += (unsigned char) COUNT_FLIP_0[(P >> 17) & 0x7f];
	n_flipped += (unsigned char) COUNT_FLIP_5[((P & 0x0101010101010204ULL) * 0x2020201008040201ULL) >> 56];	// C1A3A8

	return n_flipped;
}

/**
 * Count last flipped discs when playing on square B3.
 *
 * @param P player's disc pattern.
 * @return flipped disc count.
 */
static int count_last_flip_B3(const unsigned long long P)
{
	register int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_2[((P & 0x4020100804020202ULL) * 0x0081020202020202ULL) >> 56];	// B1B3G8
	n_flipped += (unsigned char) COUNT_FLIP_1[(P >> 18) & 0x3f];
	n_flipped += (unsigned char) COUNT_FLIP_5[(((P & 0x0202020202020408ULL) >> 1) * 0x2020201008040201ULL) >> 56];	// D1B3B8

	return n_flipped;
}

/**
 * Count last flipped discs when playing on square C3.
 *
 * @param P player's disc pattern.
 * @return flipped disc count.
 */
static int count_last_flip_C3(const unsigned long long P)
{
	register int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_2[((P & 0x0404040404040404ULL) * 0x0040810204081020ULL) >> 56];
	n_flipped += (unsigned char) COUNT_FLIP_2[(P >> 16) & 0xff];
	n_flipped += (unsigned char) COUNT_FLIP_2[((P & 0x0000000102040810ULL) * 0x0101010101010101ULL) >> 56];
	n_flipped += (unsigned char) COUNT_FLIP_2[((P & 0x8040201008040201ULL) * 0x0101010101010101ULL) >> 56];

	return n_flipped;
}

/**
 * Count last flipped discs when playing on square D3.
 *
 * @param P player's disc pattern.
 * @return flipped disc count.
 */
static int count_last_flip_D3(const unsigned long long P)
{
	register int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_2[((P & 0x0808080808080808ULL) * 0x0020408102040810ULL) >> 56];
	n_flipped += (unsigned char) COUNT_FLIP_3[(P >> 16) & 0xff];
	n_flipped += (unsigned char) COUNT_FLIP_3[((P & 0x0000010204081020ULL) * 0x0101010101010101ULL) >> 56];
	n_flipped += (unsigned char) COUNT_FLIP_3[((P & 0x0080402010080402ULL) * 0x0101010101010101ULL) >> 56];

	return n_flipped;
}

/**
 * Count last flipped discs when playing on square E3.
 *
 * @param P player's disc pattern.
 * @return flipped disc count.
 */
static int count_last_flip_E3(const unsigned long long P)
{
	register int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_2[((P & 0x1010101010101010ULL) * 0x0010204081020408ULL) >> 56];
	n_flipped += (unsigned char) COUNT_FLIP_4[(P >> 16) & 0xff];
	n_flipped += (unsigned char) COUNT_FLIP_4[((P & 0x0001020408102040ULL) * 0x0101010101010101ULL) >> 56];
	n_flipped += (unsigned char) COUNT_FLIP_4[((P & 0x0000804020100804ULL) * 0x0101010101010101ULL) >> 56];

	return n_flipped;
}

/**
 * Count last flipped discs when playing on square F3.
 *
 * @param P player's disc pattern.
 * @return flipped disc count.
 */
static int count_last_flip_F3(const unsigned long long P)
{
	register int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_2[((P & 0x2020202020202020ULL) * 0x0008102040810204ULL) >> 56];
	n_flipped += (unsigned char) COUNT_FLIP_5[(P >> 16) & 0xff];
	n_flipped += (unsigned char) COUNT_FLIP_5[((P & 0x0102040810204080ULL) * 0x0101010101010101ULL) >> 56];
	n_flipped += (unsigned char) COUNT_FLIP_5[((P & 0x0000008040201008ULL) * 0x0101010101010101ULL) >> 56];

	return n_flipped;
}

/**
 * Count last flipped discs when playing on square G3.
 *
 * @param P player's disc pattern.
 * @return flipped disc count.
 */
static int count_last_flip_G3(const unsigned long long P)
{
	register int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_2[((P & 0x4040404040402010ULL) * 0x0010101020408102ULL) >> 56];	// E1G3G8
	n_flipped += (unsigned char) COUNT_FLIP_6[(P >> 16) & 0x3f];
	n_flipped += (unsigned char) COUNT_FLIP_5[(((P & 0x0204081020404040ULL) >> 1) * 0x0402010101010101ULL) >> 56];	// G1G3B8

	return n_flipped;
}

/**
 * Count last flipped discs when playing on square H3.
 *
 * @param P player's disc pattern.
 * @return flipped disc count.
 */
static int count_last_flip_H3(const unsigned long long P)
{
	register int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_2[((P & 0x8080808080804020ULL) * 0x0008080810204081ULL) >> 56];	// F1H3H8
	n_flipped += (unsigned char) COUNT_FLIP_7[(P >> 16) & 0x7f];
	n_flipped += (unsigned char) COUNT_FLIP_5[(((P & 0x0408102040808080ULL) >> 2) * 0x0402010101010101ULL) >> 56];	// H1H3C8

	return n_flipped;
}

/**
 * Count last flipped discs when playing on square A4.
 *
 * @param P player's disc pattern.
 * @return flipped disc count.
 */
static int count_last_flip_A4(const unsigned long long P)
{
	register int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_3[((P & 0x1008040201010101ULL) * 0x0102040808080808ULL) >> 56];	// A1A4E8
	n_flipped += (unsigned char) COUNT_FLIP_0[(P >> 25) & 0x7f];
	n_flipped += (unsigned char) COUNT_FLIP_4[((P & 0x0101010101020408ULL) * 0x1010101008040201ULL) >> 56];	// D1A4A8

	return n_flipped;
}

/**
 * Count last flipped discs when playing on square B4.
 *
 * @param P player's disc pattern.
 * @return flipped disc count.
 */
static int count_last_flip_B4(const unsigned long long P)
{
	register int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_3[((P & 0x2010080402020202ULL) * 0x0081020404040404ULL) >> 56];	// B1B4F8
	n_flipped += (unsigned char) COUNT_FLIP_1[(P >> 26) & 0x3f];
	n_flipped += (unsigned char) COUNT_FLIP_4[(((P & 0x0202020202040810ULL) >> 1) * 0x1010101008040201ULL) >> 56];	// E1B4B8

	return n_flipped;
}

/**
 * Count last flipped discs when playing on square C4.
 *
 * @param P player's disc pattern.
 * @return flipped disc count.
 */
static int count_last_flip_C4(const unsigned long long P)
{
	register int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_3[((P & 0x0404040404040404ULL) * 0x0040810204081020ULL) >> 56];
	n_flipped += (unsigned char) COUNT_FLIP_2[(P >> 24) & 0xff];
	n_flipped += (unsigned char) COUNT_FLIP_2[((P & 0x0000010204081020ULL) * 0x0101010101010101ULL) >> 56];
	n_flipped += (unsigned char) COUNT_FLIP_2[((P & 0x4020100804020100ULL) * 0x0101010101010101ULL) >> 56];

	return n_flipped;
}

/**
 * Count last flipped discs when playing on square D4.
 *
 * @param P player's disc pattern.
 * @return flipped disc count.
 */
static int count_last_flip_D4(const unsigned long long P)
{
	register int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_3[((P & 0x0808080808080808ULL) * 0x0020408102040810ULL) >> 56];
	n_flipped += (unsigned char) COUNT_FLIP_3[(P >> 24) & 0xff];
	n_flipped += (unsigned char) COUNT_FLIP_3[((P & 0x0001020408102040ULL) * 0x0101010101010101ULL) >> 56];
	n_flipped += (unsigned char) COUNT_FLIP_3[((P & 0x8040201008040201ULL) * 0x0101010101010101ULL) >> 56];

	return n_flipped;
}

/**
 * Count last flipped discs when playing on square E4.
 *
 * @param P player's disc pattern.
 * @return flipped disc count.
 */
static int count_last_flip_E4(const unsigned long long P)
{
	register int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_3[((P & 0x1010101010101010ULL) * 0x0010204081020408ULL) >> 56];
	n_flipped += (unsigned char) COUNT_FLIP_4[(P >> 24) & 0xff];
	n_flipped += (unsigned char) COUNT_FLIP_4[((P & 0x0102040810204080ULL) * 0x0101010101010101ULL) >> 56];
	n_flipped += (unsigned char) COUNT_FLIP_4[((P & 0x0080402010080402ULL) * 0x0101010101010101ULL) >> 56];

	return n_flipped;
}

/**
 * Count last flipped discs when playing on square F4.
 *
 * @param P player's disc pattern.
 * @return flipped disc count.
 */
static int count_last_flip_F4(const unsigned long long P)
{
	register int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_3[((P & 0x2020202020202020ULL) * 0x0008102040810204ULL) >> 56];
	n_flipped += (unsigned char) COUNT_FLIP_5[(P >> 24) & 0xff];
	n_flipped += (unsigned char) COUNT_FLIP_5[((P & 0x0204081020408000ULL) * 0x0101010101010101ULL) >> 56];
	n_flipped += (unsigned char) COUNT_FLIP_5[((P & 0x0000804020100804ULL) * 0x0101010101010101ULL) >> 56];

	return n_flipped;
}

/**
 * Count last flipped discs when playing on square G4.
 *
 * @param P player's disc pattern.
 * @return flipped disc count.
 */
static int count_last_flip_G4(const unsigned long long P)
{
	register int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_3[((P & 0x4040404040201008ULL) * 0x0020202020408102ULL) >> 56];	// D1G4G8
	n_flipped += (unsigned char) COUNT_FLIP_6[(P >> 24) & 0x3f];
	n_flipped += (unsigned char) COUNT_FLIP_4[(((P & 0x0408102040404040ULL) >> 2) * 0x0804020101010101ULL) >> 56];	// G1G4C8

	return n_flipped;
}

/**
 * Count last flipped discs when playing on square H4.
 *
 * @param P player's disc pattern.
 * @return flipped disc count.
 */
static int count_last_flip_H4(const unsigned long long P)
{
	register int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_3[((P & 0x8080808080402010ULL) * 0x0010101010204081ULL) >> 56];	// E1H4H8
	n_flipped += (unsigned char) COUNT_FLIP_7[(P >> 24) & 0x7f];
	n_flipped += (unsigned char) COUNT_FLIP_4[(((P & 0x0810204080808080ULL) >> 3) * 0x0804020101010101ULL) >> 56];	// H1H4D8

	return n_flipped;
}

/**
 * Count last flipped discs when playing on square A5.
 *
 * @param P player's disc pattern.
 * @return flipped disc count.
 */
static int count_last_flip_A5(const unsigned long long P)
{
	register int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_4[((P & 0x0804020101010101ULL) * 0x0102040810101010ULL) >> 56];	// A1A5D8
	n_flipped += (unsigned char) COUNT_FLIP_0[(P >> 33) & 0x7f];
	n_flipped += (unsigned char) COUNT_FLIP_3[((P & 0x0101010102040810ULL) * 0x0808080808040201ULL) >> 56];	// E1A5A8

	return n_flipped;
}

/**
 * Count last flipped discs when playing on square B5.
 *
 * @param P player's disc pattern.
 * @return flipped disc count.
 */
static int count_last_flip_B5(const unsigned long long P)
{
	register int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_4[((P & 0x1008040202020202ULL) * 0x0081020408080808ULL) >> 56];	// B1B5E8
	n_flipped += (unsigned char) COUNT_FLIP_1[(P >> 34) & 0x3f];
	n_flipped += (unsigned char) COUNT_FLIP_3[(((P & 0x0202020204081020ULL) >> 1) * 0x0808080808040201ULL) >> 56];	// F1B5B8

	return n_flipped;
}

/**
 * Count last flipped discs when playing on square C5.
 *
 * @param P player's disc pattern.
 * @return flipped disc count.
 */
static int count_last_flip_C5(const unsigned long long P)
{
	register int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_4[((P & 0x0404040404040404ULL) * 0x0040810204081020ULL) >> 56];
	n_flipped += (unsigned char) COUNT_FLIP_2[(P >> 32) & 0xff];
	n_flipped += (unsigned char) COUNT_FLIP_2[((P & 0x0001020408102040ULL) * 0x0101010101010101ULL) >> 56];
	n_flipped += (unsigned char) COUNT_FLIP_2[((P & 0x2010080402010000ULL) * 0x0101010101010101ULL) >> 56];

	return n_flipped;
}

/**
 * Count last flipped discs when playing on square D5.
 *
 * @param P player's disc pattern.
 * @return flipped disc count.
 */
static int count_last_flip_D5(const unsigned long long P)
{
	register int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_4[((P & 0x0808080808080808ULL) * 0x0020408102040810ULL) >> 56];
	n_flipped += (unsigned char) COUNT_FLIP_3[(P >> 32) & 0xff];
	n_flipped += (unsigned char) COUNT_FLIP_3[((P & 0x0102040810204080ULL) * 0x0101010101010101ULL) >> 56];
	n_flipped += (unsigned char) COUNT_FLIP_3[((P & 0x4020100804020100ULL) * 0x0101010101010101ULL) >> 56];

	return n_flipped;
}

/**
 * Count last flipped discs when playing on square E5.
 *
 * @param P player's disc pattern.
 * @return flipped disc count.
 */
static int count_last_flip_E5(const unsigned long long P)
{
	register int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_4[((P & 0x1010101010101010ULL) * 0x0010204081020408ULL) >> 56];
	n_flipped += (unsigned char) COUNT_FLIP_4[(P >> 32) & 0xff];
	n_flipped += (unsigned char) COUNT_FLIP_4[((P & 0x0204081020408000ULL) * 0x0101010101010101ULL) >> 56];
	n_flipped += (unsigned char) COUNT_FLIP_4[((P & 0x8040201008040201ULL) * 0x0101010101010101ULL) >> 56];

	return n_flipped;
}

/**
 * Count last flipped discs when playing on square F5.
 *
 * @param P player's disc pattern.
 * @return flipped disc count.
 */
static int count_last_flip_F5(const unsigned long long P)
{
	register int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_4[((P & 0x2020202020202020ULL) * 0x0008102040810204ULL) >> 56];
	n_flipped += (unsigned char) COUNT_FLIP_5[(P >> 32) & 0xff];
	n_flipped += (unsigned char) COUNT_FLIP_5[((P & 0x0408102040800000ULL) * 0x0101010101010101ULL) >> 56];
	n_flipped += (unsigned char) COUNT_FLIP_5[((P & 0x0080402010080402ULL) * 0x0101010101010101ULL) >> 56];

	return n_flipped;
}

/**
 * Count last flipped discs when playing on square G5.
 *
 * @param P player's disc pattern.
 * @return flipped disc count.
 */
static int count_last_flip_G5(const unsigned long long P)
{
	register int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_4[((P & 0x4040404020100804ULL) * 0x0040404040408102ULL) >> 56];	// C1G5G8
	n_flipped += (unsigned char) COUNT_FLIP_6[(P >> 32) & 0x3f];
	n_flipped += (unsigned char) COUNT_FLIP_3[(((P & 0x0810204040404040ULL) >> 3) * 0x1008040201010101ULL) >> 56];	// G1G5D8

	return n_flipped;
}

/**
 * Count last flipped discs when playing on square H5.
 *
 * @param P player's disc pattern.
 * @return flipped disc count.
 */
static int count_last_flip_H5(const unsigned long long P)
{
	register int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_4[((P & 0x8080808040201008ULL) * 0x0020202020204081ULL) >> 56];	// D1H5H8
	n_flipped += (unsigned char) COUNT_FLIP_7[(P >> 32) & 0x7f];
	n_flipped += (unsigned char) COUNT_FLIP_3[(((P & 0x1020408080808080ULL) >> 4) * 0x1008040201010101ULL) >> 56];	// H1H5E8

	return n_flipped;
}

/**
 * Count last flipped discs when playing on square A6.
 *
 * @param P player's disc pattern.
 * @return flipped disc count.
 */
static int count_last_flip_A6(const unsigned long long P)
{
	register int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_5[((P & 0x0402010101010101ULL) * 0x0102040810202020ULL) >> 56];	// A1A6C8
	n_flipped += (unsigned char) COUNT_FLIP_0[(P >> 41) & 0x7f];
	n_flipped += (unsigned char) COUNT_FLIP_2[((P & 0x0101010204081020ULL) * 0x0404040404040201ULL) >> 56];	// F1A6A8

	return n_flipped;
}

/**
 * Count last flipped discs when playing on square B6.
 *
 * @param P player's disc pattern.
 * @return flipped disc count.
 */
static int count_last_flip_B6(const unsigned long long P)
{
	register int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_5[((P & 0x0804020202020202ULL) * 0x0081020408101010ULL) >> 56];	// B1B6D8
	n_flipped += (unsigned char) COUNT_FLIP_1[(P >> 42) & 0x3f];
	n_flipped += (unsigned char) COUNT_FLIP_2[(((P & 0x0202020408102040ULL) >> 1) * 0x0404040404040201ULL) >> 56];	// G1B6B8

	return n_flipped;
}

/**
 * Count last flipped discs when playing on square C6.
 *
 * @param P player's disc pattern.
 * @return flipped disc count.
 */
static int count_last_flip_C6(const unsigned long long P)
{
	register int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_5[((P & 0x0404040404040404ULL) * 0x0040810204081020ULL) >> 56];
	n_flipped += (unsigned char) COUNT_FLIP_2[(P >> 40) & 0xff];
	n_flipped += (unsigned char) COUNT_FLIP_2[((P & 0x0102040810204080ULL) * 0x0101010101010101ULL) >> 56];
	n_flipped += (unsigned char) COUNT_FLIP_2[((P & 0x1008040201000000ULL) * 0x0101010101010101ULL) >> 56];

	return n_flipped;
}

/**
 * Count last flipped discs when playing on square D6.
 *
 * @param P player's disc pattern.
 * @return flipped disc count.
 */
static int count_last_flip_D6(const unsigned long long P)
{
	register int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_5[((P & 0x0808080808080808ULL) * 0x0020408102040810ULL) >> 56];
	n_flipped += (unsigned char) COUNT_FLIP_3[(P >> 40) & 0xff];
	n_flipped += (unsigned char) COUNT_FLIP_3[((P & 0x0204081020408000ULL) * 0x0101010101010101ULL) >> 56];
	n_flipped += (unsigned char) COUNT_FLIP_3[((P & 0x2010080402010000ULL) * 0x0101010101010101ULL) >> 56];

	return n_flipped;
}

/**
 * Count last flipped discs when playing on square E6.
 *
 * @param P player's disc pattern.
 * @return flipped disc count.
 */
static int count_last_flip_E6(const unsigned long long P)
{
	register int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_5[((P & 0x1010101010101010ULL) * 0x0010204081020408ULL) >> 56];
	n_flipped += (unsigned char) COUNT_FLIP_4[(P >> 40) & 0xff];
	n_flipped += (unsigned char) COUNT_FLIP_4[((P & 0x0408102040800000ULL) * 0x0101010101010101ULL) >> 56];
	n_flipped += (unsigned char) COUNT_FLIP_4[((P & 0x4020100804020100ULL) * 0x0101010101010101ULL) >> 56];

	return n_flipped;
}

/**
 * Count last flipped discs when playing on square F6.
 *
 * @param P player's disc pattern.
 * @return flipped disc count.
 */
static int count_last_flip_F6(const unsigned long long P)
{
	register int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_5[((P & 0x2020202020202020ULL) * 0x0008102040810204ULL) >> 56];
	n_flipped += (unsigned char) COUNT_FLIP_5[(P >> 40) & 0xff];
	n_flipped += (unsigned char) COUNT_FLIP_5[((P & 0x0810204080000000ULL) * 0x0101010101010101ULL) >> 56];
	n_flipped += (unsigned char) COUNT_FLIP_5[((P & 0x8040201008040201ULL) * 0x0101010101010101ULL) >> 56];

	return n_flipped;
}

/**
 * Count last flipped discs when playing on square G6.
 *
 * @param P player's disc pattern.
 * @return flipped disc count.
 */
static int count_last_flip_G6(const unsigned long long P)
{
	register int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_5[((P & 0x4040402010080402ULL) * 0x0080808080808102ULL) >> 56];	// B1G6G8
	n_flipped += (unsigned char) COUNT_FLIP_6[(P >> 40) & 0x3f];
	n_flipped += (unsigned char) COUNT_FLIP_2[(((P & 0x1020404040404040ULL) >> 4) * 0x2010080402010101ULL) >> 56];	// G1G6E8

	return n_flipped;
}

/**
 * Count last flipped discs when playing on square H6.
 *
 * @param P player's disc pattern.
 * @return flipped disc count.
 */
static int count_last_flip_H6(const unsigned long long P)
{
	register int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_5[((P & 0x8080804020100804ULL) * 0x0040404040404081ULL) >> 56];	// C1H6H8
	n_flipped += (unsigned char) COUNT_FLIP_7[(P >> 40) & 0x7f];
	n_flipped += (unsigned char) COUNT_FLIP_2[(((P & 0x2040808080808080ULL) >> 5) * 0x2010080402010101ULL) >> 56];	// H1H6F8

	return n_flipped;
}

/**
 * Count last flipped discs when playing on square A7.
 *
 * @param P player's disc pattern.
 * @return flipped disc count.
 */
static int count_last_flip_A7(const unsigned long long P)
{
	register int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_6[((P & 0x0000010101010101ULL) * 0x0102040810204080ULL) >> 56];
	n_flipped += (unsigned char) COUNT_FLIP_0[(P >> 49) & 0x7f];
	n_flipped += (unsigned char) COUNT_FLIP_1[((P & 0x0000020408102040ULL) * 0x0101010101010101ULL) >> 57];

	return n_flipped;
}

/**
 * Count last flipped discs when playing on square B7.
 *
 * @param P player's disc pattern.
 * @return flipped disc count.
 */
static int count_last_flip_B7(const unsigned long long P)
{
	register int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_6[((P & 0x0000020202020202ULL) * 0x0081020408102040ULL) >> 56];
	n_flipped += (unsigned char) COUNT_FLIP_1[(P >> 50) & 0x3f];
	n_flipped += (unsigned char) COUNT_FLIP_1[((P & 0x0000040810204080ULL) * 0x0101010101010101ULL) >> 58];

	return n_flipped;
}

/**
 * Count last flipped discs when playing on square C7.
 *
 * @param P player's disc pattern.
 * @return flipped disc count.
 */
static int count_last_flip_C7(const unsigned long long P)
{
	register int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_6[((P & 0x0000040404040404ULL) * 0x0040810204081020ULL) >> 56];
	n_flipped += (unsigned char) COUNT_FLIP_2[(P >> 48) & 0xff];
	n_flipped += (unsigned char) COUNT_FLIP_2[(P & 0x00040a1120408000ULL) * 0x0101010101010101ULL >> 56];	// A5C7H2

	return n_flipped;
}

/**
 * Count last flipped discs when playing on square D7.
 *
 * @param P player's disc pattern.
 * @return flipped disc count.
 */
static int count_last_flip_D7(const unsigned long long P)
{
	register int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_6[((P & 0x0000080808080808ULL) * 0x0020408102040810ULL) >> 56];
	n_flipped += (unsigned char) COUNT_FLIP_3[(P >> 48) & 0xff];
	n_flipped += (unsigned char) COUNT_FLIP_3[(P & 0x0008142241800000ULL) * 0x0101010101010101ULL >> 56];	// A4D7H3

	return n_flipped;
}

/**
 * Count last flipped discs when playing on square E7.
 *
 * @param P player's disc pattern.
 * @return flipped disc count.
 */
static int count_last_flip_E7(const unsigned long long P)
{
	register int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_6[((P & 0x0000101010101010ULL) * 0x0010204081020408ULL) >> 56];
	n_flipped += (unsigned char) COUNT_FLIP_4[(P >> 48) & 0xff];
	n_flipped += (unsigned char) COUNT_FLIP_4[(P & 0x0010284482010000ULL) * 0x0101010101010101ULL >> 56];	// A3E7H4

	return n_flipped;
}

/**
 * Count last flipped discs when playing on square F7.
 *
 * @param P player's disc pattern.
 * @return flipped disc count.
 */
static int count_last_flip_F7(const unsigned long long P)
{
	register int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_6[((P & 0x0000202020202020ULL) * 0x0008102040810204ULL) >> 56];
	n_flipped += (unsigned char) COUNT_FLIP_5[(P >> 48) & 0xff];
	n_flipped += (unsigned char) COUNT_FLIP_5[(P & 0x0020508804020100ULL) * 0x0101010101010101ULL >> 56];	// A2F7H5

	return n_flipped;
}

/**
 * Count last flipped discs when playing on square G7.
 *
 * @param P player's disc pattern.
 * @return flipped disc count.
 */
static int count_last_flip_G7(const unsigned long long P)
{
	register int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_6[((P & 0x0000404040404040ULL) * 0x0004081020408102ULL) >> 56];
	n_flipped += (unsigned char) COUNT_FLIP_6[(P >> 48) & 0x3f];
	n_flipped += (unsigned char) COUNT_FLIP_6[((P & 0x0000201008040201ULL) * 0x0101010101010101ULL) >> 56];

	return n_flipped;
}

/**
 * Count last flipped discs when playing on square H7.
 *
 * @param P player's disc pattern.
 * @return flipped disc count.
 */
static int count_last_flip_H7(const unsigned long long P)
{
	register int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_6[((P & 0x0000808080808080ULL) * 0x0002040810204081ULL) >> 56];
	n_flipped += (unsigned char) COUNT_FLIP_7[(P >> 48) & 0x7f];
	n_flipped += (unsigned char) COUNT_FLIP_6[((P & 0x0000402010080402ULL) * 0x0101010101010101ULL) >> 57];

	return n_flipped;
}

/**
 * Count last flipped discs when playing on square A8.
 *
 * @param P player's disc pattern.
 * @return flipped disc count.
 */
static int count_last_flip_A8(const unsigned long long P)
{
	register int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_7[((P & 0x0001010101010101ULL) * 0x0102040810204080ULL) >> 56];
	n_flipped += (unsigned char) COUNT_FLIP_0[P >> 57];
	n_flipped += (unsigned char) COUNT_FLIP_0[((P & 0x0002040810204080ULL) * 0x0101010101010101ULL) >> 57];

	return n_flipped;
}

/**
 * Count last flipped discs when playing on square B8.
 *
 * @param P player's disc pattern.
 * @return flipped disc count.
 */
static int count_last_flip_B8(const unsigned long long P)
{
	register int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_7[((P & 0x0002020202020202ULL) * 0x0081020408102040ULL) >> 56];
	n_flipped += (unsigned char) COUNT_FLIP_1[P >> 58];
	n_flipped += (unsigned char) COUNT_FLIP_1[((P & 0x0004081020408000ULL) * 0x0101010101010101ULL) >> 58];

	return n_flipped;
}

/**
 * Count last flipped discs when playing on square C8.
 *
 * @param P player's disc pattern.
 * @return flipped disc count.
 */
static int count_last_flip_C8(const unsigned long long P)
{
	register int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_7[((P & 0x0004040404040404ULL) * 0x0040810204081020ULL) >> 56];
	n_flipped += (unsigned char) COUNT_FLIP_2[P >> 56];
	n_flipped += (unsigned char) COUNT_FLIP_2[(P & 0x040a112040800000ULL) * 0x0101010101010101ULL >> 56];	// A6C8H3

	return n_flipped;
}

/**
 * Count last flipped discs when playing on square D8.
 *
 * @param P player's disc pattern.
 * @return flipped disc count.
 */
static int count_last_flip_D8(const unsigned long long P)
{
	register int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_7[((P & 0x0008080808080808ULL) * 0x0020408102040810ULL) >> 56];
	n_flipped += (unsigned char) COUNT_FLIP_3[P >> 56];
	n_flipped += (unsigned char) COUNT_FLIP_3[(P & 0x0814224180000000ULL) * 0x0101010101010101ULL >> 56];	// A5D8H4

	return n_flipped;
}

/**
 * Count last flipped discs when playing on square E8.
 *
 * @param P player's disc pattern.
 * @return flipped disc count.
 */
static int count_last_flip_E8(const unsigned long long P)
{
	register int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_7[((P & 0x0010101010101010ULL) * 0x0010204081020408ULL) >> 56];
	n_flipped += (unsigned char) COUNT_FLIP_4[P >> 56];
	n_flipped += (unsigned char) COUNT_FLIP_4[(P & 0x1028448201000000ULL) * 0x0101010101010101ULL >> 56];	// A4E8H5

	return n_flipped;
}

/**
 * Count last flipped discs when playing on square F8.
 *
 * @param P player's disc pattern.
 * @return flipped disc count.
 */
static int count_last_flip_F8(const unsigned long long P)
{
	register int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_7[((P & 0x0020202020202020ULL) * 0x0008102040810204ULL) >> 56];
	n_flipped += (unsigned char) COUNT_FLIP_5[P >> 56];
	n_flipped += (unsigned char) COUNT_FLIP_5[(P & 0x2050880402010000ULL) * 0x0101010101010101ULL >> 56];	// A3F8H6

	return n_flipped;
}

/**
 * Count last flipped discs when playing on square G8.
 *
 * @param P player's disc pattern.
 * @return flipped disc count.
 */
static int count_last_flip_G8(const unsigned long long P)
{
	register int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_7[((P & 0x0040404040404040ULL) * 0x0004081020408102ULL) >> 56];
	n_flipped += (unsigned char) COUNT_FLIP_6[(P >> 56) & 0x3f];
	n_flipped += (unsigned char) COUNT_FLIP_6[((P & 0x0020100804020100ULL) * 0x0101010101010101ULL) >> 56];

	return n_flipped;
}

/**
 * Count last flipped discs when playing on square H8.
 *
 * @param P player's disc pattern.
 * @return flipped disc count.
 */
static int count_last_flip_H8(const unsigned long long P)
{
	register int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_7[((P & 0x0080808080808080ULL) * 0x0002040810204081ULL) >> 56];
	n_flipped += (unsigned char) COUNT_FLIP_7[(P >> 56) & 0x7f];
	n_flipped += (unsigned char) COUNT_FLIP_7[((P & 0x0040201008040201ULL) * 0x0101010101010101ULL) >> 56];

	return n_flipped;
}

/**
 * Count last flipped discs when plassing.
 *
 * @param P player's disc pattern (unused).
 * @return zero.
 */
static int count_last_flip_pass(const unsigned long long P)
{
	(void) P; // useless code to shut-up compiler warning
	return 0;
}

/** Array of functions to count flipped discs of the last move */
int (*count_last_flip[])(const unsigned long long) = {
	count_last_flip_A1, count_last_flip_B1, count_last_flip_C1, count_last_flip_D1,
	count_last_flip_E1, count_last_flip_F1, count_last_flip_G1, count_last_flip_H1,
	count_last_flip_A2, count_last_flip_B2, count_last_flip_C2, count_last_flip_D2,
	count_last_flip_E2, count_last_flip_F2, count_last_flip_G2, count_last_flip_H2,
	count_last_flip_A3, count_last_flip_B3, count_last_flip_C3, count_last_flip_D3,
	count_last_flip_E3, count_last_flip_F3, count_last_flip_G3, count_last_flip_H3,
	count_last_flip_A4, count_last_flip_B4, count_last_flip_C4, count_last_flip_D4,
	count_last_flip_E4, count_last_flip_F4, count_last_flip_G4, count_last_flip_H4,
	count_last_flip_A5, count_last_flip_B5, count_last_flip_C5, count_last_flip_D5,
	count_last_flip_E5, count_last_flip_F5, count_last_flip_G5, count_last_flip_H5,
	count_last_flip_A6, count_last_flip_B6, count_last_flip_C6, count_last_flip_D6,
	count_last_flip_E6, count_last_flip_F6, count_last_flip_G6, count_last_flip_H6,
	count_last_flip_A7, count_last_flip_B7, count_last_flip_C7, count_last_flip_D7,
	count_last_flip_E7, count_last_flip_F7, count_last_flip_G7, count_last_flip_H7,
	count_last_flip_A8, count_last_flip_B8, count_last_flip_C8, count_last_flip_D8,
	count_last_flip_E8, count_last_flip_F8, count_last_flip_G8, count_last_flip_H8,
	count_last_flip_pass,
};

/**
 * @brief Get the final score.
 *
 * Get the final score, when 1 empty square remain.
 * The following code has been adapted from Zebra by Gunnar Anderson.
 *
 * @param player Board.player to evaluate.
 * @param alpha  Alpha bound. (beta - 1)
 * @param x      Last empty square to play.
 * @return       The final score, as a disc difference.
 */
int board_score_1(unsigned long long P, int alpha, int pos)
{
	int op_flip, p_flips, o_flips, score2;
	int score = 2 * bit_count(P) - 64 + 2;	// = (bit_count(P) + 1) - (SCORE_MAX - 1 - bit_count(P))

	switch (pos) {
		case  0: // A1 (4%)
			op_flip  = COUNT_FLIP_0[((P & 0x0101010101010100ULL) * 0x0102040810204080ULL) >> 57];
			op_flip += COUNT_FLIP_0[(P >> 1) & 0x7f];
			op_flip += COUNT_FLIP_0[((P & 0x8040201008040200ULL) * 0x0101010101010101ULL) >> 57];
			break;

		case  1: // B1 (6%)
			op_flip  = COUNT_FLIP_0[((P & 0x0202020202020200ULL) * 0x0081020408102040ULL) >> 57];
			op_flip += COUNT_FLIP_1[(P >> 2) & 0x3f];
			op_flip += COUNT_FLIP_1[((P & 0x0080402010080400ULL) * 0x0101010101010101ULL) >> 58];
			break;

		case  2: // C1 (0.2%)
			op_flip  = COUNT_FLIP_0[((P & 0x0404040404040400ULL) * 0x0040810204081020ULL) >> 57];
			op_flip += COUNT_FLIP_2[P & 0xff];
			op_flip += COUNT_FLIP_2[(P & 0x0000804020110a04ULL) * 0x0101010101010101ULL >> 56];	// A3C1H6
			break;

		case  3: // D1 (0.4%)
			op_flip  = COUNT_FLIP_0[((P & 0x0808080808080800ULL) * 0x0020408102040810ULL) >> 57];
			op_flip += COUNT_FLIP_3[P & 0xff];
			op_flip += COUNT_FLIP_3[(P & 0x0000008041221408ULL) * 0x0101010101010101ULL >> 56];	// A4D1H5
			break;

		case  4: // E1 (0.7%)
			op_flip  = COUNT_FLIP_0[((P & 0x1010101010101000ULL) * 0x0010204081020408ULL) >> 57];
			op_flip += COUNT_FLIP_4[P & 0xff];
			op_flip += COUNT_FLIP_4[(P & 0x0000000182442810ULL) * 0x0101010101010101ULL >> 56];	// A5E1H4
			break;

		case  5: // F1 (0.3%)
			op_flip  = COUNT_FLIP_0[((P & 0x2020202020202000ULL) * 0x0008102040810204ULL) >> 57];
			op_flip += COUNT_FLIP_5[P & 0xff];
			op_flip += COUNT_FLIP_5[(P & 0x0000010204885020ULL) * 0x0101010101010101ULL >> 56];	// A6F1H3
			break;

		case  6: // G1 (4%)
			op_flip  = COUNT_FLIP_0[((P & 0x4040404040404000ULL) * 0x0004081020408102ULL) >> 57];
			op_flip += COUNT_FLIP_6[P & 0x3f];
			op_flip += COUNT_FLIP_6[((P & 0x0001020408102000ULL) * 0x0101010101010101ULL) >> 56];
			break;

		case  7: // H1 (4%)
			op_flip  = COUNT_FLIP_0[((P & 0x8080808080808000ULL) * 0x0002040810204081ULL) >> 57];
			op_flip += COUNT_FLIP_7[P & 0x7f];
			op_flip += COUNT_FLIP_7[((P & 0x0102040810204000ULL) * 0x0101010101010101ULL) >> 56];
			break;

		case  8: // A2 (4%)
			op_flip  = COUNT_FLIP_1[((P & 0x0101010101010000ULL) * 0x0102040810204080ULL) >> 58];
			op_flip += COUNT_FLIP_0[(P >> 9) & 0x7f];
			op_flip += COUNT_FLIP_1[((P & 0x4020100804020000ULL) * 0x0101010101010101ULL) >> 57];
			break;

		case  9: // B2 (2%)
			op_flip  = COUNT_FLIP_1[((P & 0x0202020202020000ULL) * 0x0081020408102040ULL) >> 58];
			op_flip += COUNT_FLIP_1[(P >> 10) & 0x3f];
			op_flip += COUNT_FLIP_1[((P & 0x8040201008040000ULL) * 0x0101010101010101ULL) >> 58];
			break;

		case 14: // G2 (4%)
			op_flip  = COUNT_FLIP_1[((P & 0x4040404040400000ULL) * 0x0004081020408102ULL) >> 58];
			op_flip += COUNT_FLIP_6[(P >> 8) & 0x3f];
			op_flip += COUNT_FLIP_6[((P & 0x0102040810200000ULL) * 0x0101010101010101ULL) >> 56];
			break;

		case 15: // H2 (6%)
			op_flip  = COUNT_FLIP_1[((P & 0x8080808080800000ULL) * 0x0002040810204081ULL) >> 58];
			op_flip += COUNT_FLIP_7[(P >> 8) & 0x7f];
			op_flip += COUNT_FLIP_6[((P & 0x0204081020400000ULL) * 0x0101010101010101ULL) >> 57];
			break;

		case 16: // A3 (0.4%)
			op_flip  = COUNT_FLIP_2[((P & 0x2010080402010101ULL) * 0x0102040404040404ULL) >> 56];	// A1A3F8
			op_flip += COUNT_FLIP_0[(P >> 17) & 0x7f];
			op_flip += COUNT_FLIP_5[((P & 0x0101010101010204ULL) * 0x2020201008040201ULL) >> 56];	// C1A3A8
			break;

		case 23: // H3 (1%)
			op_flip  = COUNT_FLIP_2[((P & 0x8080808080804020ULL) * 0x0008080810204081ULL) >> 56];	// F1H3H8
			op_flip += COUNT_FLIP_7[(P >> 16) & 0x7f];
			op_flip += COUNT_FLIP_5[(((P & 0x0408102040808080ULL) >> 2) * 0x0402010101010101ULL) >> 56];	// H1H3C8
			break;

		case 24: // A4 (0.2%)
			op_flip  = COUNT_FLIP_3[((P & 0x1008040201010101ULL) * 0x0102040808080808ULL) >> 56];	// A1A4E8
			op_flip += COUNT_FLIP_0[(P >> 25) & 0x7f];
			op_flip += COUNT_FLIP_4[((P & 0x0101010101020408ULL) * 0x1010101008040201ULL) >> 56];	// D1A4A8
			break;

		case 31: // H4 (0.8%)
			op_flip  = COUNT_FLIP_3[((P & 0x8080808080402010ULL) * 0x0010101010204081ULL) >> 56];	// E1H4H8
			op_flip += COUNT_FLIP_7[(P >> 24) & 0x7f];
			op_flip += COUNT_FLIP_4[(((P & 0x0810204080808080ULL) >> 3) * 0x0804020101010101ULL) >> 56];	// H1H4D8
			break;

		case 32: // A5 (0.5%)
			op_flip  = COUNT_FLIP_4[((P & 0x0804020101010101ULL) * 0x0102040810101010ULL) >> 56];	// A1A5D8
			op_flip += COUNT_FLIP_0[(P >> 33) & 0x7f];
			op_flip += COUNT_FLIP_3[((P & 0x0101010102040810ULL) * 0x0808080808040201ULL) >> 56];	// E1A5A8
			break;

		case 39: // H5 (1%)
			op_flip  = COUNT_FLIP_4[((P & 0x8080808040201008ULL) * 0x0020202020204081ULL) >> 56];	// D1H5H8
			op_flip += COUNT_FLIP_7[(P >> 32) & 0x7f];
			op_flip += COUNT_FLIP_3[(((P & 0x1020408080808080ULL) >> 4) * 0x1008040201010101ULL) >> 56];	// H1H5E8
			break;

		case 40: // A6 (0.5%)
			op_flip  = COUNT_FLIP_5[((P & 0x0402010101010101ULL) * 0x0102040810202020ULL) >> 56];	// A1A6C8
			op_flip += COUNT_FLIP_0[(P >> 41) & 0x7f];
			op_flip += COUNT_FLIP_2[((P & 0x0101010204081020ULL) * 0x0404040404040201ULL) >> 56];	// F1A6A8
			break;

		case 47: // H6 (1%)
			op_flip  = COUNT_FLIP_5[((P & 0x8080804020100804ULL) * 0x0040404040404081ULL) >> 56];	// C1H6H8
			op_flip += COUNT_FLIP_7[(P >> 40) & 0x7f];
			op_flip += COUNT_FLIP_2[(((P & 0x2040808080808080ULL) >> 5) * 0x2010080402010101ULL) >> 56];	// H1H6F8
			break;

		case 48: // A7 (6%)
			op_flip  = COUNT_FLIP_6[((P & 0x0000010101010101ULL) * 0x0102040810204080ULL) >> 56];
			op_flip += COUNT_FLIP_0[(P >> 49) & 0x7f];
			op_flip += COUNT_FLIP_1[((P & 0x0000020408102040ULL) * 0x0101010101010101ULL) >> 57];
			break;

		case 49: // B7 (5%)
			op_flip  = COUNT_FLIP_6[((P & 0x0000020202020202ULL) * 0x0081020408102040ULL) >> 56];
			op_flip += COUNT_FLIP_1[(P >> 50) & 0x3f];
			op_flip += COUNT_FLIP_1[((P & 0x0000040810204080ULL) * 0x0101010101010101ULL) >> 58];
			break;

		case 54: // G7 (5%)
			op_flip  = COUNT_FLIP_6[((P & 0x0000404040404040ULL) * 0x0004081020408102ULL) >> 56];
			op_flip += COUNT_FLIP_6[(P >> 48) & 0x3f];
			op_flip += COUNT_FLIP_6[((P & 0x0000201008040201ULL) * 0x0101010101010101ULL) >> 56];
			break;

		case 55: // H7 (8%)
			op_flip  = COUNT_FLIP_6[((P & 0x0000808080808080ULL) * 0x0002040810204081ULL) >> 56];
			op_flip += COUNT_FLIP_7[(P >> 48) & 0x7f];
			op_flip += COUNT_FLIP_6[((P & 0x0000402010080402ULL) * 0x0101010101010101ULL) >> 57];
			break;

		case 56: // A8 (4%)
			op_flip  = COUNT_FLIP_7[((P & 0x0001010101010101ULL) * 0x0102040810204080ULL) >> 56];
			op_flip += COUNT_FLIP_0[P >> 57];
			op_flip += COUNT_FLIP_0[((P & 0x0002040810204080ULL) * 0x0101010101010101ULL) >> 57];
			break;

		case 57: // B8 (9%)
			op_flip  = COUNT_FLIP_7[((P & 0x0002020202020202ULL) * 0x0081020408102040ULL) >> 56];
			op_flip += COUNT_FLIP_1[P >> 58];
			op_flip += COUNT_FLIP_1[((P & 0x0004081020408000ULL) * 0x0101010101010101ULL) >> 58];
			break;

		case 58: // C8 (1%)
			op_flip  = COUNT_FLIP_7[((P & 0x0004040404040404ULL) * 0x0040810204081020ULL) >> 56];
			op_flip += COUNT_FLIP_2[P >> 56];
			op_flip += COUNT_FLIP_2[(P & 0x040a112040800000ULL) * 0x0101010101010101ULL >> 56];	// A6C8H3
			break;

		case 59: // D8 (2%)
			op_flip  = COUNT_FLIP_7[((P & 0x0008080808080808ULL) * 0x0020408102040810ULL) >> 56];
			op_flip += COUNT_FLIP_3[P >> 56];
			op_flip += COUNT_FLIP_3[(P & 0x0814224180000000ULL) * 0x0101010101010101ULL >> 56];	// A5D8H4
			break;

		case 60: // E8 (2%)
			op_flip  = COUNT_FLIP_7[((P & 0x0010101010101010ULL) * 0x0010204081020408ULL) >> 56];
			op_flip += COUNT_FLIP_4[P >> 56];
			op_flip += COUNT_FLIP_4[(P & 0x1028448201000000ULL) * 0x0101010101010101ULL >> 56];	// A4E8H5
			break;

		case 61: // F8 (2%)
			op_flip  = COUNT_FLIP_7[((P & 0x0020202020202020ULL) * 0x0008102040810204ULL) >> 56];
			op_flip += COUNT_FLIP_5[P >> 56];
			op_flip += COUNT_FLIP_5[(P & 0x2050880402010000ULL) * 0x0101010101010101ULL >> 56];	// A3F8H6
			break;

		case 62: // G8 (8%)
			op_flip  = COUNT_FLIP_7[((P & 0x0040404040404040ULL) * 0x0004081020408102ULL) >> 56];
			op_flip += COUNT_FLIP_6[(P >> 56) & 0x3f];
			op_flip += COUNT_FLIP_6[((P & 0x0020100804020100ULL) * 0x0101010101010101ULL) >> 56];
			break;

		case 63: // H8 (4%)
			op_flip  = COUNT_FLIP_7[((P & 0x0080808080808080ULL) * 0x0002040810204081ULL) >> 56];
			op_flip += COUNT_FLIP_7[(P >> 56) & 0x7f];
			op_flip += COUNT_FLIP_7[((P & 0x0040201008040201ULL) * 0x0101010101010101ULL) >> 56];
			break;

		case 10: case 11: case 12: case 13: case 17: case 22: case 25: case 30: // (0.4%)
		case 33: case 38: case 41: case 46: case 50: case 51: case 52: case 53: // (2.3%)
		case 18: case 19: case 20: case 21: case 26: case 27: case 28: case 29:	// (0%)
		case 34: case 35: case 36: case 37: case 42: case 43: case 44: case 45:	// (0%)
			p_flips = (*count_last_flip[pos])(P);
  #ifdef SIMULLASTFLIP
			o_flips = (*count_last_flip[pos])(~P);
			score2 = score - o_flips - (int)((-o_flips | (score - 1)) < 0) * 2;
			score += p_flips;
			return p_flips ? score : score2;

  #else
			score += p_flips;

			if (p_flips == 0) {	// (23%)
				score2 = score - 2;	// empty for opponent
				if (score <= 0)
					score = score2;
				if (score > alpha) {	// lazy cut-off (40%)
					if ((o_flips = (*count_last_flip[pos])(~P)) != 0)	// (98%)
						score = score2 - o_flips;
				}
			}
			return score;
  #endif
		default:
			UNREACHABLE;
	}

	p_flips = op_flip & 0xff;
	o_flips = op_flip >> 8;
  #ifdef SIMULLASTFLIP
	score2 = score - o_flips - (int)((-o_flips | (score - 1)) < 0) * 2;	// last square for O if O can move or score <= 0
	score += p_flips;
	return p_flips ? score : score2;

  #else
	score += p_flips;

	if (p_flips == 0) {
		score2 = score - 2;	// empty for opponent
		if (score <= 0)
			score = score2;
		if (score > alpha) {	// lazy cut-off
			if (o_flips != 0)
				score = score2 - o_flips;
		}
	}
	return score;
  #endif
}
