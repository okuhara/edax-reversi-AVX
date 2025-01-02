/**
 * @file count_last_flip_32.c
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

#define LODWORD(l) ((unsigned int)(l))
#define HIDWORD(l) ((unsigned int)((l)>>32))

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
	int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_0[(((LODWORD(P) & 0x01010101u) + ((HIDWORD(P) & 0x01010101u) << 4)) * 0x01020408u) >> 25];
	n_flipped += (unsigned char) COUNT_FLIP_0[(LODWORD(P) >> 1) & 0x7f];
	n_flipped += (unsigned char) COUNT_FLIP_0[(((LODWORD(P) & 0x08040200u) + (HIDWORD(P) & 0x80402010u)) * 0x01010101u) >> 25];

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
	int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_0[(((LODWORD(P) & 0x02020202u) + ((HIDWORD(P) & 0x02020202u) << 4)) * 0x00810204u) >> 25];
	n_flipped += (unsigned char) COUNT_FLIP_1[(LODWORD(P) >> 2) & 0x3f];
	n_flipped += (unsigned char) COUNT_FLIP_1[(((LODWORD(P) & 0x10080400u) + (HIDWORD(P) & 0x00804020u)) * 0x01010101u) >> 26];

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
	int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_0[(((LODWORD(P) & 0x04040404u) + ((HIDWORD(P) & 0x04040404u) << 4)) * 0x00408102u) >> 25];
	n_flipped += (unsigned char) COUNT_FLIP_2[LODWORD(P) & 0xff];
	n_flipped += (unsigned char) COUNT_FLIP_2[(((LODWORD(P) & 0x20110A04u) + (HIDWORD(P) & 0x00008040u)) * 0x01010101u) >> 24];	// A3C1H6

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
	int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_0[(((LODWORD(P) & 0x08080808u) + ((HIDWORD(P) & 0x08080808u) << 4)) * 0x00204081u) >> 25];
	n_flipped += (unsigned char) COUNT_FLIP_3[LODWORD(P) & 0xff];
	n_flipped += (unsigned char) COUNT_FLIP_3[(((LODWORD(P) & 0x41221408u) + (HIDWORD(P) & 0x00000080u)) * 0x01010101u) >> 24];	// A4D1H5

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
	int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_0[((((LODWORD(P) & 0x10101010u) >> 4) + (HIDWORD(P) & 0x10101010u)) * 0x01020408u) >> 25];
	n_flipped += (unsigned char) COUNT_FLIP_4[LODWORD(P) & 0xff];
	n_flipped += (unsigned char) COUNT_FLIP_4[(((LODWORD(P) & 0x82442810u) + (HIDWORD(P) & 0x00000001u)) * 0x01010101u) >> 24];	// A5E1H4

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
	int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_0[((((LODWORD(P) & 0x20202020u) >> 4) + (HIDWORD(P) & 0x20202020u)) * 0x00810204u) >> 25];
	n_flipped += (unsigned char) COUNT_FLIP_5[LODWORD(P) & 0xff];
	n_flipped += (unsigned char) COUNT_FLIP_5[(((LODWORD(P) & 0x04885020u) + (HIDWORD(P) & 0x00000102u)) * 0x01010101u) >> 24];	// A6F1H3

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
	int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_0[((((LODWORD(P) & 0x40404040u) >> 4) + (HIDWORD(P) & 0x40404040u)) * 0x00408102u) >> 25];
	n_flipped += (unsigned char) COUNT_FLIP_6[LODWORD(P) & 0x3f];
	n_flipped += (unsigned char) COUNT_FLIP_6[(((LODWORD(P) & 0x08102000u) + (HIDWORD(P) & 0x00010204u)) * 0x02020202u) >> 25];

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
	int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_0[((((LODWORD(P) & 0x80808080u) >> 4) + (HIDWORD(P) & 0x80808080u)) * 0x00204081u) >> 25];
	n_flipped += (unsigned char) COUNT_FLIP_7[LODWORD(P) & 0x7f];
	n_flipped += (unsigned char) COUNT_FLIP_7[(((LODWORD(P) & 0x10204000u) + (HIDWORD(P) & 0x01020408u)) * 0x01010101u) >> 24];

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
	int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_1[(((LODWORD(P) & 0x01010000u) + ((HIDWORD(P) & 0x01010101u) << 4)) * 0x01020408u) >> 26];
	n_flipped += (unsigned char) COUNT_FLIP_0[(LODWORD(P) >> 9) & 0x7f];
	n_flipped += (unsigned char) COUNT_FLIP_1[(((LODWORD(P) & 0x04020000u) + (HIDWORD(P) & 0x40201008u)) * 0x01010101u) >> 25];

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
	int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_1[(((LODWORD(P) & 0x02020000u) + ((HIDWORD(P) & 0x02020202u) << 4)) * 0x00810204u) >> 26];
	n_flipped += (unsigned char) COUNT_FLIP_1[(LODWORD(P) >> 10) & 0x3f];
	n_flipped += (unsigned char) COUNT_FLIP_1[(((LODWORD(P) & 0x08040000u) + (HIDWORD(P) & 0x80402010u)) * 0x01010101u) >> 26];

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
	int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_1[(((LODWORD(P) & 0x04040000u) + ((HIDWORD(P) & 0x04040404u) << 4)) * 0x00408102u) >> 26];
	n_flipped += (unsigned char) COUNT_FLIP_2[(LODWORD(P) >> 8) & 0xff];
	n_flipped += (unsigned char) COUNT_FLIP_2[(((LODWORD(P) & 0x110A0400u) + (HIDWORD(P) & 0x00804020u)) * 0x01010101u) >> 24];	// A4C2H7

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
	int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_1[(((LODWORD(P) & 0x08080000u) + ((HIDWORD(P) & 0x08080808u) << 4)) * 0x00204081u) >> 26];
	n_flipped += (unsigned char) COUNT_FLIP_3[(LODWORD(P) >> 8) & 0xff];
	n_flipped += (unsigned char) COUNT_FLIP_3[(((LODWORD(P) & 0x22140800u) + (HIDWORD(P) & 0x00008041u)) * 0x01010101u) >> 24];	// A5D2H6

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
	int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_1[((((LODWORD(P) & 0x10100000u) >> 4) + (HIDWORD(P) & 0x10101010u)) * 0x01020408u) >> 26];
	n_flipped += (unsigned char) COUNT_FLIP_4[(LODWORD(P) >> 8) & 0xff];
	n_flipped += (unsigned char) COUNT_FLIP_4[(((LODWORD(P) & 0x44281000u) + (HIDWORD(P) & 0x00000182u)) * 0x01010101u) >> 24];	// A6E2H5

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
	int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_1[(((HIDWORD(P) & 0x20202020u) + ((LODWORD(P) & 0x20200000u) >> 4)) * 0x00810204u) >> 26];
	n_flipped += (unsigned char) COUNT_FLIP_5[(LODWORD(P) >> 8) & 0xff];
	n_flipped += (unsigned char) COUNT_FLIP_5[(((LODWORD(P) & 0x88502000u) + (HIDWORD(P) & 0x00010204u)) * 0x01010101u) >> 24];	// A7F2H4

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
	int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_1[((((LODWORD(P) & 0x40400000u) >> 4) + (HIDWORD(P) & 0x40404040u)) * 0x00408102u) >> 26];
	n_flipped += (unsigned char) COUNT_FLIP_6[(LODWORD(P) >> 8) & 0x3f];
	n_flipped += (unsigned char) COUNT_FLIP_6[(((LODWORD(P) & 0x10200000u) + (HIDWORD(P) & 0x01020408u)) * 0x02020202u) >> 25];

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
	int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_1[((((LODWORD(P) & 0x80800000u) >> 4) + (HIDWORD(P) & 0x80808080u)) * 0x00204081u) >> 26];
	n_flipped += (unsigned char) COUNT_FLIP_7[(LODWORD(P) >> 8) & 0x7f];
	n_flipped += (unsigned char) COUNT_FLIP_6[(((LODWORD(P) & 0x20400000u) + (HIDWORD(P) & 0x02040810u)) * 0x01010101u) >> 25];

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
	int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_2[((LODWORD(P) & 0x02010101u) * 0x01020404u + (HIDWORD(P) & 0x20100804u) * 0x04040404u) >> 24];	// A1A3F8
	n_flipped += (unsigned char) COUNT_FLIP_0[(LODWORD(P) >> 17) & 0x7f];
	n_flipped += (unsigned char) COUNT_FLIP_5[((LODWORD(P) & 0x01010204u) * 0x20202010u + (HIDWORD(P) & 0x01010101u) * 0x08040201u) >> 24];	// C1A3A8

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
	int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_2[((LODWORD(P) & 0x04020202u) * 0x00810202u + (HIDWORD(P) & 0x40201008u) * 0x02020202u) >> 24];	// B1B3G8
	n_flipped += (unsigned char) COUNT_FLIP_1[(LODWORD(P) >> 18) & 0x3f];
	n_flipped += (unsigned char) COUNT_FLIP_5[((LODWORD(P) & 0x02020408u) * 0x10101008u + ((HIDWORD(P) & 0x02020202u) >> 1) * 0x08040201u) >> 24];	// D1B3B8

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
	int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_2[(((LODWORD(P) & 0x04040404u) + ((HIDWORD(P) & 0x04040404u) << 4)) * 0x00408102u) >> 24];
	n_flipped += (unsigned char) COUNT_FLIP_2[(LODWORD(P) >> 16) & 0xff];
	n_flipped += (unsigned char) COUNT_FLIP_2[(((LODWORD(P) & 0x02040810u) + (HIDWORD(P) & 0x00000001u)) * 0x01010101u) >> 24];
	n_flipped += (unsigned char) COUNT_FLIP_2[(((LODWORD(P) & 0x08040201u) + (HIDWORD(P) & 0x80402010u)) * 0x01010101u) >> 24];

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
	int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_2[(((LODWORD(P) & 0x08080808u) + ((HIDWORD(P) & 0x08080808u) << 4)) * 0x00204081u) >> 24];
	n_flipped += (unsigned char) COUNT_FLIP_3[(LODWORD(P) >> 16) & 0xff];
	n_flipped += (unsigned char) COUNT_FLIP_3[(((LODWORD(P) & 0x04081020u) + (HIDWORD(P) & 0x00000102u)) * 0x01010101u) >> 24];
	n_flipped += (unsigned char) COUNT_FLIP_3[(((LODWORD(P) & 0x10080402u) + (HIDWORD(P) & 0x00804020u)) * 0x01010101u) >> 24];

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
	int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_2[((((LODWORD(P) & 0x10101010u) >> 4) + (HIDWORD(P) & 0x10101010u)) * 0x01020408u) >> 24];
	n_flipped += (unsigned char) COUNT_FLIP_4[(LODWORD(P) >> 16) & 0xff];
	n_flipped += (unsigned char) COUNT_FLIP_4[(((LODWORD(P) & 0x08102040u) + (HIDWORD(P) & 0x00010204u)) * 0x01010101u) >> 24];
	n_flipped += (unsigned char) COUNT_FLIP_4[(((LODWORD(P) & 0x20100804u) + (HIDWORD(P) & 0x00008040u)) * 0x01010101u) >> 24];

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
	int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_2[(((HIDWORD(P) & 0x20202020u) + ((LODWORD(P) & 0x20202020u) >> 4)) * 0x00810204u) >> 24];
	n_flipped += (unsigned char) COUNT_FLIP_5[(LODWORD(P) >> 16) & 0xff];
	n_flipped += (unsigned char) COUNT_FLIP_5[(((LODWORD(P) & 0x10204080u) + (HIDWORD(P) & 0x01020408u)) * 0x01010101u) >> 24];
	n_flipped += (unsigned char) COUNT_FLIP_5[(((LODWORD(P) & 0x40201008u) + (HIDWORD(P) & 0x00000080u)) * 0x01010101u) >> 24];

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
	int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_2[(((LODWORD(P) & 0x40402010u) >> 4) * 0x01010102u + (HIDWORD(P) & 0x40404040u) * 0x00408102u) >> 24];	// E1G3G8
	n_flipped += (unsigned char) COUNT_FLIP_6[(LODWORD(P) >> 16) & 0x3f];
	n_flipped += (unsigned char) COUNT_FLIP_5[(((LODWORD(P) & 0x20404040u) >> 1) * 0x04020101u + ((HIDWORD(P) & 0x02040810u) >> 1) * 0x01010101u) >> 24];	// G1G3B8

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
	int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_2[(((LODWORD(P) & 0x80804020u) >> 4) * 0x00808081u + (HIDWORD(P) & 0x80808080u) * 0x00204081u) >> 24];	// F1H3H8
	n_flipped += (unsigned char) COUNT_FLIP_7[(LODWORD(P) >> 16) & 0x7f];
	n_flipped += (unsigned char) COUNT_FLIP_5[(((LODWORD(P) & 0x40808080u) >> 2) * 0x04020101u + ((HIDWORD(P) & 0x04081020u) >> 2) * 0x01010101u) >> 24];	// H1H3C8

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
	int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_3[((LODWORD(P) & 0x01010101u) * 0x01020408u + (HIDWORD(P) & 0x10080402u) * 0x08080808u) >> 24];	// A1A4E8
	n_flipped += (unsigned char) COUNT_FLIP_0[LODWORD(P) >> 25];
	n_flipped += (unsigned char) COUNT_FLIP_4[((LODWORD(P) & 0x01020408u) * 0x10101010u + (HIDWORD(P) & 0x01010101u) * 0x08040201u) >> 24];	// D1A4A8

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
	int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_3[((LODWORD(P) & 0x02020202u) * 0x00810204u + (HIDWORD(P) & 0x20100804u) * 0x04040404u) >> 24];	// B1B4F8
	n_flipped += (unsigned char) COUNT_FLIP_1[LODWORD(P) >> 26];
	n_flipped += (unsigned char) COUNT_FLIP_4[((LODWORD(P) & 0x02040810u) * 0x08080808u + ((HIDWORD(P) & 0x02020202u) >> 1) * 0x08040201u) >> 24];	// E1B4B8

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
	int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_3[(((LODWORD(P) & 0x04040404u) + ((HIDWORD(P) & 0x04040404u) << 4)) * 0x00408102u) >> 24];
	n_flipped += (unsigned char) COUNT_FLIP_2[LODWORD(P) >> 24];
	n_flipped += (unsigned char) COUNT_FLIP_2[(((LODWORD(P) & 0x04081020u) + (HIDWORD(P) & 0x00000102u)) * 0x01010101u) >> 24];
	n_flipped += (unsigned char) COUNT_FLIP_2[(((LODWORD(P) & 0x04020100u) + (HIDWORD(P) & 0x40201008u)) * 0x01010101u) >> 24];

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
	int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_3[(((LODWORD(P) & 0x08080808u) + ((HIDWORD(P) & 0x08080808u) << 4)) * 0x00204081u) >> 24];
	n_flipped += (unsigned char) COUNT_FLIP_3[LODWORD(P) >> 24];
	n_flipped += (unsigned char) COUNT_FLIP_3[(((LODWORD(P) & 0x08102040u) + (HIDWORD(P) & 0x00010204u)) * 0x01010101u) >> 24];
	n_flipped += (unsigned char) COUNT_FLIP_3[(((LODWORD(P) & 0x08040201u) + (HIDWORD(P) & 0x80402010u)) * 0x01010101u) >> 24];

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
	int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_3[((((LODWORD(P) & 0x10101010u) >> 4) + (HIDWORD(P) & 0x10101010u)) * 0x01020408u) >> 24];
	n_flipped += (unsigned char) COUNT_FLIP_4[LODWORD(P) >> 24];
	n_flipped += (unsigned char) COUNT_FLIP_4[(((LODWORD(P) & 0x10204080u) + (HIDWORD(P) & 0x01020408u)) * 0x01010101u) >> 24];
	n_flipped += (unsigned char) COUNT_FLIP_4[(((LODWORD(P) & 0x10080402u) + (HIDWORD(P) & 0x00804020u)) * 0x01010101u) >> 24];

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
	int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_3[(((HIDWORD(P) & 0x20202020u) + ((LODWORD(P) & 0x20202020u) >> 4)) * 0x00810204u) >> 24];
	n_flipped += (unsigned char) COUNT_FLIP_5[LODWORD(P) >> 24];
	n_flipped += (unsigned char) COUNT_FLIP_5[(((LODWORD(P) & 0x20408000u) + (HIDWORD(P) & 0x02040810u)) * 0x01010101u) >> 24];
	n_flipped += (unsigned char) COUNT_FLIP_5[(((LODWORD(P) & 0x20100804u) + (HIDWORD(P) & 0x00008040u)) * 0x01010101u) >> 24];

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
	int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_3[(((LODWORD(P) & 0x40201008u) >> 3) * 0x01010101u + (HIDWORD(P) & 0x40404040u) * 0x00408102u) >> 24];	// D1G4G8
	n_flipped += (unsigned char) COUNT_FLIP_6[(LODWORD(P) >> 24) & 0x3f];
	n_flipped += (unsigned char) COUNT_FLIP_4[(((LODWORD(P) & 0x40404040u) >> 2) * 0x08040201u + ((HIDWORD(P) & 0x04081020u) >> 2) * 0x01010101u) >> 24];	// G1G4C8

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
	int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_3[(((LODWORD(P) & 0x80402010u) >> 4) * 0x01010101u + (HIDWORD(P) & 0x80808080u) * 0x00204081u) >> 24];	// E1H4H8
	n_flipped += (unsigned char) COUNT_FLIP_7[(LODWORD(P) >> 24) & 0x7f];
	n_flipped += (unsigned char) COUNT_FLIP_4[(((LODWORD(P) & 0x80808080u) >> 3) * 0x08040201u + ((HIDWORD(P) & 0x08102040u) >> 3) * 0x01010101u) >> 24];	// H1H4D8

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
	int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_4[((LODWORD(P) & 0x01010101u) * 0x01020408u + (HIDWORD(P) & 0x08040201u) * 0x10101010u) >> 24];	// A1A5D8
	n_flipped += (unsigned char) COUNT_FLIP_0[(HIDWORD(P) >> 1) & 0x7f];
	n_flipped += (unsigned char) COUNT_FLIP_3[((LODWORD(P) & 0x02040810u) * 0x08080808u + (HIDWORD(P) & 0x01010101u) * 0x08040201u) >> 24];	// E1A5A8

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
	int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_4[((LODWORD(P) & 0x02020202u) * 0x00810204u + (HIDWORD(P) & 0x10080402u) * 0x08080808u) >> 24];	// B1B5E8
	n_flipped += (unsigned char) COUNT_FLIP_1[(HIDWORD(P) >> 2) & 0x3f];
	n_flipped += (unsigned char) COUNT_FLIP_3[((LODWORD(P) & 0x04081020u) * 0x04040404u + ((HIDWORD(P) & 0x02020202u) >> 1) * 0x08040201u) >> 24];	// F1B5B8

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
	int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_4[(((LODWORD(P) & 0x04040404u) + ((HIDWORD(P) & 0x04040404u) << 4)) * 0x00408102u) >> 24];
	n_flipped += (unsigned char) COUNT_FLIP_2[HIDWORD(P) & 0xff];
	n_flipped += (unsigned char) COUNT_FLIP_2[(((LODWORD(P) & 0x08102040u) + (HIDWORD(P) & 0x00010204u)) * 0x01010101u) >> 24];
	n_flipped += (unsigned char) COUNT_FLIP_2[(((LODWORD(P) & 0x02010000u) + (HIDWORD(P) & 0x20100804u)) * 0x01010101u) >> 24];

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
	int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_4[(((LODWORD(P) & 0x08080808u) + ((HIDWORD(P) & 0x08080808u) << 4)) * 0x00204081u) >> 24];
	n_flipped += (unsigned char) COUNT_FLIP_3[HIDWORD(P) & 0xff];
	n_flipped += (unsigned char) COUNT_FLIP_3[(((LODWORD(P) & 0x10204080u) + (HIDWORD(P) & 0x01020408u)) * 0x01010101u) >> 24];
	n_flipped += (unsigned char) COUNT_FLIP_3[(((LODWORD(P) & 0x04020100u) + (HIDWORD(P) & 0x40201008u)) * 0x01010101u) >> 24];

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
	int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_4[((((LODWORD(P) & 0x10101010u) >> 4) + (HIDWORD(P) & 0x10101010u)) * 0x01020408u) >> 24];
	n_flipped += (unsigned char) COUNT_FLIP_4[HIDWORD(P) & 0xff];
	n_flipped += (unsigned char) COUNT_FLIP_4[(((LODWORD(P) & 0x20408000u) + (HIDWORD(P) & 0x02040810u)) * 0x01010101u) >> 24];
	n_flipped += (unsigned char) COUNT_FLIP_4[(((LODWORD(P) & 0x08040201u) + (HIDWORD(P) & 0x80402010u)) * 0x01010101u) >> 24];

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
	int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_4[(((HIDWORD(P) & 0x20202020u) + ((LODWORD(P) & 0x20202020u) >> 4)) * 0x00810204u) >> 24];
	n_flipped += (unsigned char) COUNT_FLIP_5[HIDWORD(P) & 0xff];
	n_flipped += (unsigned char) COUNT_FLIP_5[(((LODWORD(P) & 0x40800000u) + (HIDWORD(P) & 0x04081020u)) * 0x01010101u) >> 24];
	n_flipped += (unsigned char) COUNT_FLIP_5[(((LODWORD(P) & 0x10080402u) + (HIDWORD(P) & 0x00804020u)) * 0x01010101u) >> 24];

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
	int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_4[(((LODWORD(P) & 0x20100804u) >> 2) * 0x01010101u + (HIDWORD(P) & 0x40404040u) * 0x00408102u) >> 24];	// C1G5G8
	n_flipped += (unsigned char) COUNT_FLIP_6[HIDWORD(P) & 0x3f];
	n_flipped += (unsigned char) COUNT_FLIP_3[(((LODWORD(P) & 0x40404040u) >> 3) * 0x10080402u + ((HIDWORD(P) & 0x08102040u) >> 3) * 0x01010101u) >> 24];	// G1G5D8

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
	int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_4[(((LODWORD(P) & 0x40201008u) >> 3) * 0x01010101u + (HIDWORD(P) & 0x80808080u) * 0x00204081u) >> 24];	// D1H5H8
	n_flipped += (unsigned char) COUNT_FLIP_7[HIDWORD(P) & 0x7f];
	n_flipped += (unsigned char) COUNT_FLIP_3[(((LODWORD(P) & 0x80808080u) >> 4) * 0x10080402u + ((HIDWORD(P) & 0x10204080u) >> 4) * 0x01010101u) >> 24];	// H1H5E8

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
	int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_5[((LODWORD(P) & 0x01010101u) * 0x01020408u + (HIDWORD(P) & 0x04020101u) * 0x10202020u) >> 24];	// A1A6C8
	n_flipped += (unsigned char) COUNT_FLIP_0[(HIDWORD(P) >> 9) & 0x7f];
	n_flipped += (unsigned char) COUNT_FLIP_2[((LODWORD(P) & 0x04081020u) * 0x04040404u + (HIDWORD(P) & 0x01010102u) * 0x04040201u) >> 24];	// F1A6A8

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
	int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_5[((LODWORD(P) & 0x02020202u) * 0x00810204u + (HIDWORD(P) & 0x08040202u) * 0x08101010u) >> 24];	// B1B6D8
	n_flipped += (unsigned char) COUNT_FLIP_1[(HIDWORD(P) >> 10) & 0x3f];
	n_flipped += (unsigned char) COUNT_FLIP_2[((LODWORD(P) & 0x08102040u) * 0x02020202u + ((HIDWORD(P) & 0x02020204u) >> 1) * 0x04040201u) >> 24];	// G1B6B8

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
	int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_5[(((LODWORD(P) & 0x04040404u) + ((HIDWORD(P) & 0x04040404u) << 4)) * 0x00408102u) >> 24];
	n_flipped += (unsigned char) COUNT_FLIP_2[(HIDWORD(P) >> 8) & 0xff];
	n_flipped += (unsigned char) COUNT_FLIP_2[(((LODWORD(P) & 0x10204080u) + (HIDWORD(P) & 0x01020408u)) * 0x01010101u) >> 24];
	n_flipped += (unsigned char) COUNT_FLIP_2[(((LODWORD(P) & 0x01000000u) + (HIDWORD(P) & 0x10080402u)) * 0x01010101u) >> 24];

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
	int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_5[(((LODWORD(P) & 0x08080808u) + ((HIDWORD(P) & 0x08080808u) << 4)) * 0x00204081u) >> 24];
	n_flipped += (unsigned char) COUNT_FLIP_3[(HIDWORD(P) >> 8) & 0xff];
	n_flipped += (unsigned char) COUNT_FLIP_3[(((LODWORD(P) & 0x20408000u) + (HIDWORD(P) & 0x02040810u)) * 0x01010101u) >> 24];
	n_flipped += (unsigned char) COUNT_FLIP_3[(((LODWORD(P) & 0x02010000u) + (HIDWORD(P) & 0x20100804u)) * 0x01010101u) >> 24];

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
	int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_5[((((LODWORD(P) & 0x10101010u) >> 4) + (HIDWORD(P) & 0x10101010u)) * 0x01020408u) >> 24];
	n_flipped += (unsigned char) COUNT_FLIP_4[(HIDWORD(P) >> 8) & 0xff];
	n_flipped += (unsigned char) COUNT_FLIP_4[(((LODWORD(P) & 0x40800000u) + (HIDWORD(P) & 0x04081020u)) * 0x01010101u) >> 24];
	n_flipped += (unsigned char) COUNT_FLIP_4[(((LODWORD(P) & 0x04020100u) + (HIDWORD(P) & 0x40201008u)) * 0x01010101u) >> 24];

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
	int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_5[(((HIDWORD(P) & 0x20202020u) + ((LODWORD(P) & 0x20202020u) >> 4)) * 0x00810204u) >> 24];
	n_flipped += (unsigned char) COUNT_FLIP_5[(HIDWORD(P) >> 8) & 0xff];
	n_flipped += (unsigned char) COUNT_FLIP_5[(((LODWORD(P) & 0x80000000u) + (HIDWORD(P) & 0x08102040u)) * 0x01010101u) >> 24];
	n_flipped += (unsigned char) COUNT_FLIP_5[(((LODWORD(P) & 0x08040201u) + (HIDWORD(P) & 0x80402010u)) * 0x01010101u) >> 24];

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
	int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_5[(((LODWORD(P) & 0x10080402u) >> 1) * 0x01010101u + (HIDWORD(P) & 0x40404020u) * 0x00808102u) >> 24];	// B1G6G8
	n_flipped += (unsigned char) COUNT_FLIP_6[(HIDWORD(P) >> 8) & 0x3f];
	n_flipped += (unsigned char) COUNT_FLIP_2[(((LODWORD(P) & 0x40404040u) >> 4) * 0x20100804u + ((HIDWORD(P) & 0x10204040u) >> 4 ) * 0x02010101u) >> 24];	// G1G6E8

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
	int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_5[(((LODWORD(P) & 0x20100804u) >> 2) * 0x01010101u + (HIDWORD(P) & 0x80808040u) * 0x00404081u) >> 24];	// C1H6H8
	n_flipped += (unsigned char) COUNT_FLIP_7[(HIDWORD(P) >> 8) & 0x7f];
	n_flipped += (unsigned char) COUNT_FLIP_2[(((LODWORD(P) & 0x80808080u) >> 5) * 0x20100804u + ((HIDWORD(P) & 0x20408080u) >> 5) * 0x02010101u) >> 24];	// H1H6F8

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
	int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_6[((((HIDWORD(P) & 0x00000101u) << 4) + (LODWORD(P) & 0x01010101u)) * 0x01020408u) >> 24];
	n_flipped += (unsigned char) COUNT_FLIP_0[(HIDWORD(P) >> 17) & 0x7f];
	n_flipped += (unsigned char) COUNT_FLIP_1[(((HIDWORD(P) & 0x00000204u) + (LODWORD(P) & 0x08102040u)) * 0x01010101u) >> 25];

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
	int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_6[((((HIDWORD(P) & 0x00000202u) << 4) + (LODWORD(P) & 0x02020202u)) * 0x00810204u) >> 24];
	n_flipped += (unsigned char) COUNT_FLIP_1[(HIDWORD(P) >> 18) & 0x3f];
	n_flipped += (unsigned char) COUNT_FLIP_1[(((HIDWORD(P) & 0x00000408u) + (LODWORD(P) & 0x10204080u)) * 0x01010101u) >> 26];

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
	int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_6[(((LODWORD(P) & 0x04040404u) + ((HIDWORD(P) & 0x00000404u) << 4)) * 0x00408102u) >> 24];
	n_flipped += (unsigned char) COUNT_FLIP_2[(HIDWORD(P) >> 16) & 0xff];
	n_flipped += (unsigned char) COUNT_FLIP_2[(((HIDWORD(P) & 0x00040A11u) + (LODWORD(P) & 0x20408000u)) * 0x01010101u) >> 24];	// A5C7H2

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
	int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_6[((((HIDWORD(P) & 0x00000808u) << 4) + (LODWORD(P) & 0x08080808u)) * 0x00204081u) >> 24];
	n_flipped += (unsigned char) COUNT_FLIP_3[(HIDWORD(P) >> 16) & 0xff];
	n_flipped += (unsigned char) COUNT_FLIP_3[(((HIDWORD(P) & 0x00081422u) + (LODWORD(P) & 0x41800000u)) * 0x01010101u) >> 24];	// A4D7H3

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
	int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_6[(((HIDWORD(P) & 0x00001010u) + ((LODWORD(P) & 0x10101010u) >> 4)) * 0x01020408u) >> 24];
	n_flipped += (unsigned char) COUNT_FLIP_4[(HIDWORD(P) >> 16) & 0xff];
	n_flipped += (unsigned char) COUNT_FLIP_4[(((HIDWORD(P) & 0x00102844u) + (LODWORD(P) & 0x82010000u)) * 0x01010101u) >> 24];	// A3E7H4

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
	int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_6[(((HIDWORD(P) & 0x00002020u) + ((LODWORD(P) & 0x20202020u) >> 4)) * 0x00810204u) >> 24];
	n_flipped += (unsigned char) COUNT_FLIP_5[(HIDWORD(P) >> 16) & 0xff];
	n_flipped += (unsigned char) COUNT_FLIP_5[(((HIDWORD(P) & 0x00205088u) + (LODWORD(P) & 0x04020100u)) * 0x01010101u) >> 24];	// A2F7H5

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
	int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_6[(((HIDWORD(P) & 0x00004040u) + ((LODWORD(P) & 0x40404040u) >> 4)) * 0x00408102u) >> 24];
	n_flipped += (unsigned char) COUNT_FLIP_6[(HIDWORD(P) >> 16) & 0x3f];
	n_flipped += (unsigned char) COUNT_FLIP_6[(((HIDWORD(P) & 0x00002010u) + (LODWORD(P) & 0x08040201u)) * 0x01010101u) >> 24];

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
	int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_6[(((HIDWORD(P) & 0x00008080u) + ((LODWORD(P) & 0x80808080u) >> 4)) * 0x00204081u) >> 24];
	n_flipped += (unsigned char) COUNT_FLIP_7[(HIDWORD(P) >> 16) & 0x7f];
	n_flipped += (unsigned char) COUNT_FLIP_6[(((HIDWORD(P) & 0x00004020u) + (LODWORD(P) & 0x10080402u)) * 0x01010101u) >> 25];

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
	int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_7[((((HIDWORD(P) & 0x00010101u) << 4) + (LODWORD(P) & 0x01010101u)) * 0x01020408u) >> 24];
	n_flipped += (unsigned char) COUNT_FLIP_0[HIDWORD(P) >> 25];
	n_flipped += (unsigned char) COUNT_FLIP_0[(((HIDWORD(P) & 0x00020408u) + (LODWORD(P) & 0x10204080u)) * 0x01010101u) >> 25];

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
	int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_7[((((HIDWORD(P) & 0x00020202u) << 4) + (LODWORD(P) & 0x02020202u)) * 0x00810204u) >> 24];
	n_flipped += (unsigned char) COUNT_FLIP_1[HIDWORD(P) >> 26];
	n_flipped += (unsigned char) COUNT_FLIP_1[(((HIDWORD(P) & 0x00040810u) + (LODWORD(P) & 0x20408000u)) * 0x01010101u) >> 26];

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
	int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_7[(((LODWORD(P) & 0x04040404u) + ((HIDWORD(P) & 0x00040404u) << 4)) * 0x00408102u) >> 24];
	n_flipped += (unsigned char) COUNT_FLIP_2[HIDWORD(P) >> 24];
	n_flipped += (unsigned char) COUNT_FLIP_2[(((HIDWORD(P) & 0x040A1120u) + (LODWORD(P) & 0x40800000u)) * 0x01010101u) >> 24];	// A6C8H3

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
	int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_7[((((HIDWORD(P) & 0x00080808u) << 4) + (LODWORD(P) & 0x08080808u)) * 0x00204081u) >> 24];
	n_flipped += (unsigned char) COUNT_FLIP_3[HIDWORD(P) >> 24];
	n_flipped += (unsigned char) COUNT_FLIP_3[(((HIDWORD(P) & 0x08142241u) + (LODWORD(P) & 0x80000000u)) * 0x01010101u) >> 24];	// A5D8H4

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
	int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_7[(((HIDWORD(P) & 0x00101010u) + ((LODWORD(P) & 0x10101010u) >> 4)) * 0x01020408u) >> 24];
	n_flipped += (unsigned char) COUNT_FLIP_4[HIDWORD(P) >> 24];
	n_flipped += (unsigned char) COUNT_FLIP_4[(((HIDWORD(P) & 0x10284482u) + (LODWORD(P) & 0x01000000u)) * 0x01010101u) >> 24];	// A4E8H5

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
	int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_7[(((HIDWORD(P) & 0x00202020u) + ((LODWORD(P) & 0x20202020u) >> 4)) * 0x00810204u) >> 24];
	n_flipped += (unsigned char) COUNT_FLIP_5[HIDWORD(P) >> 24];
	n_flipped += (unsigned char) COUNT_FLIP_5[(((HIDWORD(P) & 0x00508804u) + (LODWORD(P) & 0x02010000u)) * 0x01010101u) >> 24];	// A3F8H6

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
	int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_7[(((HIDWORD(P) & 0x00404040u) + ((LODWORD(P) & 0x40404040u) >> 4)) * 0x00408102u) >> 24];
	n_flipped += (unsigned char) COUNT_FLIP_6[(HIDWORD(P) >> 24) & 0x3f];
	n_flipped += (unsigned char) COUNT_FLIP_6[(((HIDWORD(P) & 0x00201008u) + (LODWORD(P) & 0x04020100u)) * 0x01010101u) >> 24];

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
	int n_flipped;

	n_flipped  = (unsigned char) COUNT_FLIP_7[(((HIDWORD(P) & 0x00808080u) + ((LODWORD(P) & 0x80808080) >> 4)) * 0x00204081u) >> 24];
	n_flipped += (unsigned char) COUNT_FLIP_7[(HIDWORD(P) >> 24) & 0x7f];
	n_flipped += (unsigned char) COUNT_FLIP_7[(((HIDWORD(P) & 0x00402010u) + (LODWORD(P) & 0x08040201u)) * 0x01010101u) >> 24];

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
			op_flip  = COUNT_FLIP_0[(((LODWORD(P) & 0x01010101u) + ((HIDWORD(P) & 0x01010101u) << 4)) * 0x01020408u) >> 25];
			op_flip += COUNT_FLIP_0[(LODWORD(P) >> 1) & 0x7f];
			op_flip += COUNT_FLIP_0[(((LODWORD(P) & 0x08040200u) + (HIDWORD(P) & 0x80402010u)) * 0x01010101u) >> 25];
			break;

		case  1: // B1 (6%)
			op_flip  = COUNT_FLIP_0[(((LODWORD(P) & 0x02020202u) + ((HIDWORD(P) & 0x02020202u) << 4)) * 0x00810204u) >> 25];
			op_flip += COUNT_FLIP_1[(LODWORD(P) >> 2) & 0x3f];
			op_flip += COUNT_FLIP_1[(((LODWORD(P) & 0x10080400u) + (HIDWORD(P) & 0x00804020u)) * 0x01010101u) >> 26];
			break;

		case  2: // C1 (0.2%)
			op_flip  = COUNT_FLIP_0[(((LODWORD(P) & 0x04040404u) + ((HIDWORD(P) & 0x04040404u) << 4)) * 0x00408102u) >> 25];
			op_flip += COUNT_FLIP_2[LODWORD(P) & 0xff];
			op_flip += COUNT_FLIP_2[(((LODWORD(P) & 0x20110A04u) + (HIDWORD(P) & 0x00008040u)) * 0x01010101u) >> 24];	// A3C1H6
			break;

		case  3: // D1 (0.4%)
			op_flip  = COUNT_FLIP_0[(((LODWORD(P) & 0x08080808u) + ((HIDWORD(P) & 0x08080808u) << 4)) * 0x00204081u) >> 25];
			op_flip += COUNT_FLIP_3[LODWORD(P) & 0xff];
			op_flip += COUNT_FLIP_3[(((LODWORD(P) & 0x41221408u) + (HIDWORD(P) & 0x00000080u)) * 0x01010101u) >> 24];	// A4D1H5
			break;

		case  4: // E1 (0.7%)
			op_flip  = COUNT_FLIP_0[((((LODWORD(P) & 0x10101010u) >> 4) + (HIDWORD(P) & 0x10101010u)) * 0x01020408u) >> 25];
			op_flip += COUNT_FLIP_4[LODWORD(P) & 0xff];
			op_flip += COUNT_FLIP_4[(((LODWORD(P) & 0x82442810u) + (HIDWORD(P) & 0x00000001u)) * 0x01010101u) >> 24];	// A5E1H4
			break;

		case  5: // F1 (0.3%)
			op_flip  = COUNT_FLIP_0[((((LODWORD(P) & 0x20202020u) >> 4) + (HIDWORD(P) & 0x20202020u)) * 0x00810204u) >> 25];
			op_flip += COUNT_FLIP_5[LODWORD(P) & 0xff];
			op_flip += COUNT_FLIP_5[(((LODWORD(P) & 0x04885020u) + (HIDWORD(P) & 0x00000102u)) * 0x01010101u) >> 24];	// A6F1H3
			break;

		case  6: // G1 (4%)
			op_flip  = COUNT_FLIP_0[((((LODWORD(P) & 0x40404040u) >> 4) + (HIDWORD(P) & 0x40404040u)) * 0x00408102u) >> 25];
			op_flip += COUNT_FLIP_6[LODWORD(P) & 0x3f];
			op_flip += COUNT_FLIP_6[(((LODWORD(P) & 0x08102000u) + (HIDWORD(P) & 0x00010204u)) * 0x02020202u) >> 25];
			break;

		case  7: // H1 (4%)
			op_flip  = COUNT_FLIP_0[((((LODWORD(P) & 0x80808080u) >> 4) + (HIDWORD(P) & 0x80808080u)) * 0x00204081u) >> 25];
			op_flip += COUNT_FLIP_7[LODWORD(P) & 0x7f];
			op_flip += COUNT_FLIP_7[(((LODWORD(P) & 0x10204000u) + (HIDWORD(P) & 0x01020408u)) * 0x01010101u) >> 24];
			break;

		case  8: // A2 (4%)
			op_flip  = COUNT_FLIP_1[(((LODWORD(P) & 0x01010000u) + ((HIDWORD(P) & 0x01010101u) << 4)) * 0x01020408u) >> 26];
			op_flip += COUNT_FLIP_0[(LODWORD(P) >> 9) & 0x7f];
			op_flip += COUNT_FLIP_1[(((LODWORD(P) & 0x04020000u) + (HIDWORD(P) & 0x40201008u)) * 0x01010101u) >> 25];
			break;

		case  9: // B2 (2%)
			op_flip  = COUNT_FLIP_1[(((LODWORD(P) & 0x02020000u) + ((HIDWORD(P) & 0x02020202u) << 4)) * 0x00810204u) >> 26];
			op_flip += COUNT_FLIP_1[(LODWORD(P) >> 10) & 0x3f];
			op_flip += COUNT_FLIP_1[(((LODWORD(P) & 0x08040000u) + (HIDWORD(P) & 0x80402010u)) * 0x01010101u) >> 26];
			break;

		case 14: // G2 (4%)
			op_flip  = COUNT_FLIP_1[((((LODWORD(P) & 0x40400000u) >> 4) + (HIDWORD(P) & 0x40404040u)) * 0x00408102u) >> 26];
			op_flip += COUNT_FLIP_6[(LODWORD(P) >> 8) & 0x3f];
			op_flip += COUNT_FLIP_6[(((LODWORD(P) & 0x10200000u) + (HIDWORD(P) & 0x01020408u)) * 0x02020202u) >> 25];
			break;

		case 15: // H2 (6%)
			op_flip  = COUNT_FLIP_1[((((LODWORD(P) & 0x80800000u) >> 4) + (HIDWORD(P) & 0x80808080u)) * 0x00204081u) >> 26];
			op_flip += COUNT_FLIP_7[(LODWORD(P) >> 8) & 0x7f];
			op_flip += COUNT_FLIP_6[(((LODWORD(P) & 0x20400000u) + (HIDWORD(P) & 0x02040810u)) * 0x01010101u) >> 25];
			break;

		case 16: // A3 (0.4%)
			op_flip  = COUNT_FLIP_2[((LODWORD(P) & 0x02010101u) * 0x01020404u + (HIDWORD(P) & 0x20100804u) * 0x04040404u) >> 24];	// A1A3F8
			op_flip += COUNT_FLIP_0[(LODWORD(P) >> 17) & 0x7f];
			op_flip += COUNT_FLIP_5[((LODWORD(P) & 0x01010204u) * 0x20202010u + (HIDWORD(P) & 0x01010101u) * 0x08040201u) >> 24];	// C1A3A8
			break;

		case 23: // H3 (1%)
			op_flip  = COUNT_FLIP_2[(((LODWORD(P) & 0x80804020u) >> 4) * 0x00808081u + (HIDWORD(P) & 0x80808080u) * 0x00204081u) >> 24];	// F1H3H8
			op_flip += COUNT_FLIP_7[(LODWORD(P) >> 16) & 0x7f];
			op_flip += COUNT_FLIP_5[(((LODWORD(P) & 0x40808080u) >> 2) * 0x04020101u + ((HIDWORD(P) & 0x04081020u) >> 2) * 0x01010101u) >> 24];	// H1H3C8
			break;

		case 24: // A4 (0.2%)
			op_flip  = COUNT_FLIP_3[((LODWORD(P) & 0x01010101u) * 0x01020408u + (HIDWORD(P) & 0x10080402u) * 0x08080808u) >> 24];	// A1A4E8
			op_flip += COUNT_FLIP_0[LODWORD(P) >> 25];
			op_flip += COUNT_FLIP_4[((LODWORD(P) & 0x01020408u) * 0x10101010u + (HIDWORD(P) & 0x01010101u) * 0x08040201u) >> 24];	// D1A4A8
			break;

		case 31: // H4 (0.8%)
			op_flip  = COUNT_FLIP_3[(((LODWORD(P) & 0x80402010u) >> 4) * 0x01010101u + (HIDWORD(P) & 0x80808080u) * 0x00204081u) >> 24];	// E1H4H8
			op_flip += COUNT_FLIP_7[(LODWORD(P) >> 24) & 0x7f];
			op_flip += COUNT_FLIP_4[(((LODWORD(P) & 0x80808080u) >> 3) * 0x08040201u + ((HIDWORD(P) & 0x08102040u) >> 3) * 0x01010101u) >> 24];	// H1H4D8
			break;

		case 32: // A5 (0.5%)
			op_flip  = COUNT_FLIP_4[((LODWORD(P) & 0x01010101u) * 0x01020408u + (HIDWORD(P) & 0x08040201u) * 0x10101010u) >> 24];	// A1A5D8
			op_flip += COUNT_FLIP_0[(HIDWORD(P) >> 1) & 0x7f];
			op_flip += COUNT_FLIP_3[((LODWORD(P) & 0x02040810u) * 0x08080808u + (HIDWORD(P) & 0x01010101u) * 0x08040201u) >> 24];	// E1A5A8
			break;

		case 39: // H5 (1%)
			op_flip  = COUNT_FLIP_4[(((LODWORD(P) & 0x40201008u) >> 3) * 0x01010101u + (HIDWORD(P) & 0x80808080u) * 0x00204081u) >> 24];	// D1H5H8
			op_flip += COUNT_FLIP_7[HIDWORD(P) & 0x7f];
			op_flip += COUNT_FLIP_3[(((LODWORD(P) & 0x80808080u) >> 4) * 0x10080402u + ((HIDWORD(P) & 0x10204080u) >> 4) * 0x01010101u) >> 24];	// H1H5E8
			break;

		case 40: // A6 (0.5%)
			op_flip  = COUNT_FLIP_5[((LODWORD(P) & 0x01010101u) * 0x01020408u + (HIDWORD(P) & 0x04020101u) * 0x10202020u) >> 24];	// A1A6C8
			op_flip += COUNT_FLIP_0[(HIDWORD(P) >> 9) & 0x7f];
			op_flip += COUNT_FLIP_2[((LODWORD(P) & 0x04081020u) * 0x04040404u + (HIDWORD(P) & 0x01010102u) * 0x04040201u) >> 24];	// F1A6A8
			break;

		case 47: // H6 (1%)
			op_flip  = COUNT_FLIP_5[(((LODWORD(P) & 0x20100804u) >> 2) * 0x01010101u + (HIDWORD(P) & 0x80808040u) * 0x00404081u) >> 24];	// C1H6H8
			op_flip += COUNT_FLIP_7[(HIDWORD(P) >> 8) & 0x7f];
			op_flip += COUNT_FLIP_2[(((LODWORD(P) & 0x80808080u) >> 5) * 0x20100804u + ((HIDWORD(P) & 0x20408080u) >> 5) * 0x02010101u) >> 24];	// H1H6F8
			break;

		case 48: // A7 (6%)
			op_flip  = COUNT_FLIP_6[((((HIDWORD(P) & 0x00000101u) << 4) + (LODWORD(P) & 0x01010101u)) * 0x01020408u) >> 24];
			op_flip += COUNT_FLIP_0[(HIDWORD(P) >> 17) & 0x7f];
			op_flip += COUNT_FLIP_1[(((HIDWORD(P) & 0x00000204u) + (LODWORD(P) & 0x08102040u)) * 0x01010101u) >> 25];
			break;

		case 49: // B7 (5%)
			op_flip  = COUNT_FLIP_6[((((HIDWORD(P) & 0x00000202u) << 4) + (LODWORD(P) & 0x02020202u)) * 0x00810204u) >> 24];
			op_flip += COUNT_FLIP_1[(HIDWORD(P) >> 18) & 0x3f];
			op_flip += COUNT_FLIP_1[(((HIDWORD(P) & 0x00000408u) + (LODWORD(P) & 0x10204080u)) * 0x01010101u) >> 26];
			break;

		case 54: // G7 (5%)
			op_flip  = COUNT_FLIP_6[(((HIDWORD(P) & 0x00004040u) + ((LODWORD(P) & 0x40404040u) >> 4)) * 0x00408102u) >> 24];
			op_flip += COUNT_FLIP_6[(HIDWORD(P) >> 16) & 0x3f];
			op_flip += COUNT_FLIP_6[(((HIDWORD(P) & 0x00002010u) + (LODWORD(P) & 0x08040201u)) * 0x02020202u) >> 25];
			break;

		case 55: // H7 (8%)
			op_flip  = COUNT_FLIP_6[(((HIDWORD(P) & 0x00008080u) + ((LODWORD(P) & 0x80808080u) >> 4)) * 0x00204081u) >> 24];
			op_flip += COUNT_FLIP_7[(HIDWORD(P) >> 16) & 0x7f];
			op_flip += COUNT_FLIP_6[(((HIDWORD(P) & 0x00004020u) + (LODWORD(P) & 0x10080402u)) * 0x01010101u) >> 25];
			break;

		case 56: // A8 (4%)
			op_flip  = COUNT_FLIP_7[((((HIDWORD(P) & 0x00010101u) << 4) + (LODWORD(P) & 0x01010101u)) * 0x01020408u) >> 24];
			op_flip += COUNT_FLIP_0[HIDWORD(P) >> 25];
			op_flip += COUNT_FLIP_0[(((HIDWORD(P) & 0x00020408u) + (LODWORD(P) & 0x10204080u)) * 0x01010101u) >> 25];
			break;

		case 57: // B8 (9%)
			op_flip  = COUNT_FLIP_7[((((HIDWORD(P) & 0x00020202u) << 4) + (LODWORD(P) & 0x02020202u)) * 0x00810204u) >> 24];
			op_flip += COUNT_FLIP_1[HIDWORD(P) >> 26];
			op_flip += COUNT_FLIP_1[(((HIDWORD(P) & 0x00040810u) + (LODWORD(P) & 0x20408000u)) * 0x01010101u) >> 26];
			break;

		case 58: // C8 (1%)
			op_flip  = COUNT_FLIP_7[(((LODWORD(P) & 0x04040404u) + ((HIDWORD(P) & 0x00040404u) << 4)) * 0x00408102u) >> 24];
			op_flip += COUNT_FLIP_2[HIDWORD(P) >> 24];
			op_flip += COUNT_FLIP_2[(((HIDWORD(P) & 0x040A1120u) + (LODWORD(P) & 0x40800000u)) * 0x01010101u) >> 24];	// A6C8H3
			break;

		case 59: // D8 (2%)
			op_flip  = COUNT_FLIP_7[((((HIDWORD(P) & 0x00080808u) << 4) + (LODWORD(P) & 0x08080808u)) * 0x00204081u) >> 24];
			op_flip += COUNT_FLIP_3[HIDWORD(P) >> 24];
			op_flip += COUNT_FLIP_3[(((HIDWORD(P) & 0x08142241u) + (LODWORD(P) & 0x80000000u)) * 0x01010101u) >> 24];	// A5D8H4
			break;

		case 60: // E8 (2%)
			op_flip  = COUNT_FLIP_7[(((HIDWORD(P) & 0x00101010u) + ((LODWORD(P) & 0x10101010u) >> 4)) * 0x01020408u) >> 24];
			op_flip += COUNT_FLIP_4[HIDWORD(P) >> 24];
			op_flip += COUNT_FLIP_4[(((HIDWORD(P) & 0x10284482u) + (LODWORD(P) & 0x01000000u)) * 0x01010101u) >> 24];	// A4E8H5
			break;

		case 61: // F8 (2%)
			op_flip  = COUNT_FLIP_7[(((HIDWORD(P) & 0x00202020u) + ((LODWORD(P) & 0x20202020u) >> 4)) * 0x00810204u) >> 24];
			op_flip += COUNT_FLIP_5[HIDWORD(P) >> 24];
			op_flip += COUNT_FLIP_5[(((HIDWORD(P) & 0x00508804u) + (LODWORD(P) & 0x02010000u)) * 0x01010101u) >> 24];	// A3F8H6
			break;

		case 62: // G8 (8%)
			op_flip  = COUNT_FLIP_7[(((HIDWORD(P) & 0x00404040u) + ((LODWORD(P) & 0x40404040u) >> 4)) * 0x00408102u) >> 24];
			op_flip += COUNT_FLIP_6[(HIDWORD(P) >> 24) & 0x3f];
			op_flip += COUNT_FLIP_6[(((HIDWORD(P) & 0x00201008u) + (LODWORD(P) & 0x04020100u)) * 0x02020202u) >> 25];
			break;

		case 63: // H8 (4%)
			op_flip  = COUNT_FLIP_7[(((HIDWORD(P) & 0x00808080u) + ((LODWORD(P) & 0x80808080) >> 4)) * 0x00204081u) >> 24];
			op_flip += COUNT_FLIP_7[(HIDWORD(P) >> 24) & 0x7f];
			op_flip += COUNT_FLIP_7[(((HIDWORD(P) & 0x00402010u) + (LODWORD(P) & 0x08040201u)) * 0x01010101u) >> 24];
			break;

		case 10: case 11: case 12: case 13: case 17: case 22: case 25: case 30: // (0.4%)
		case 33: case 38: case 41: case 46: case 50: case 51: case 52: case 53: // (2.3%)
		case 18: case 19: case 20: case 21: case 26: case 27: case 28: case 29:	// (0%)
		case 34: case 35: case 36: case 37: case 42: case 43: case 44: case 45:	// (0%)
			p_flips = last_flip(pos, P);
  #ifdef SIMULLASTFLIP
			o_flips = last_flip(pos, ~P);
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
					if ((o_flips = last_flip(pos, ~P)) != 0)	// (98%)
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
