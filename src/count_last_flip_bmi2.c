/**
 * @file count_last_flip_bmi2.c
 *
 *
 * A function is provided to count the number of fipped disc of the last move.
 *
 * The basic principle is to read into an array a precomputed result. Doing
 * this is easy for a single line ; as we can use arrays of the form:
 *  - COUNT_FLIP[square where we play][8-bits disc pattern].
 * The problem is thus to convert any line of a 64-bits disc pattern into an
 * 8-bits disc pattern. A fast way to do this is to select the right line,
 * with a bit-mask, to gather the masked-bits into a continuous set by the 
 * BMI2 PEXT instruction.
 * Once we get our 8-bits disc patterns, we directly get the number of
 * flipped discs from the precomputed array, and add them from each flipping
 * lines.
 * For optimization purpose, the value returned is twice the number of flipped
 * disc, to facilitate the computation of disc difference.
 *
 * @date 1998 - 2025
 * @author Richard Delorme
 * @author Toshihiko Okuhara
 * @version 4.5
 * 
 */

#include "bit.h"
#include <stdint.h>

/** precomputed count flip array */
/** lower byte for P, upper byte for O */
const uint16_t COUNT_FLIP[] = {
	// 8[0]: 0
	0x0000, 0x0000, 0x0200, 0x0200, 0x0002, 0x0002, 0x0400, 0x0400,
	0x0004, 0x0004, 0x0200, 0x0200, 0x0002, 0x0002, 0x0600, 0x0600,
	0x0006, 0x0006, 0x0200, 0x0200, 0x0002, 0x0002, 0x0400, 0x0400,
	0x0004, 0x0004, 0x0200, 0x0200, 0x0002, 0x0002, 0x0800, 0x0800,
	0x0008, 0x0008, 0x0200, 0x0200, 0x0002, 0x0002, 0x0400, 0x0400,
	0x0004, 0x0004, 0x0200, 0x0200, 0x0002, 0x0002, 0x0600, 0x0600,
	0x0006, 0x0006, 0x0200, 0x0200, 0x0002, 0x0002, 0x0400, 0x0400,
	0x0004, 0x0004, 0x0200, 0x0200, 0x0002, 0x0002, 0x0a00, 0x0a00,
	0x000a, 0x000a, 0x0200, 0x0200, 0x0002, 0x0002, 0x0400, 0x0400,
	0x0004, 0x0004, 0x0200, 0x0200, 0x0002, 0x0002, 0x0600, 0x0600,
	0x0006, 0x0006, 0x0200, 0x0200, 0x0002, 0x0002, 0x0400, 0x0400,
	0x0004, 0x0004, 0x0200, 0x0200, 0x0002, 0x0002, 0x0800, 0x0800,
	0x0008, 0x0008, 0x0200, 0x0200, 0x0002, 0x0002, 0x0400, 0x0400,
	0x0004, 0x0004, 0x0200, 0x0200, 0x0002, 0x0002, 0x0600, 0x0600,
	0x0006, 0x0006, 0x0200, 0x0200, 0x0002, 0x0002, 0x0400, 0x0400,
	0x0004, 0x0004, 0x0200, 0x0200, 0x0002, 0x0002, 0x0c00, 0x0c00,
	0x000c, 0x000c, 0x0200, 0x0200, 0x0002, 0x0002, 0x0400, 0x0400,
	0x0004, 0x0004, 0x0200, 0x0200, 0x0002, 0x0002, 0x0600, 0x0600,
	0x0006, 0x0006, 0x0200, 0x0200, 0x0002, 0x0002, 0x0400, 0x0400,
	0x0004, 0x0004, 0x0200, 0x0200, 0x0002, 0x0002, 0x0800, 0x0800,
	0x0008, 0x0008, 0x0200, 0x0200, 0x0002, 0x0002, 0x0400, 0x0400,
	0x0004, 0x0004, 0x0200, 0x0200, 0x0002, 0x0002, 0x0600, 0x0600,
	0x0006, 0x0006, 0x0200, 0x0200, 0x0002, 0x0002, 0x0400, 0x0400,
	0x0004, 0x0004, 0x0200, 0x0200, 0x0002, 0x0002, 0x0a00, 0x0a00,
	0x000a, 0x000a, 0x0200, 0x0200, 0x0002, 0x0002, 0x0400, 0x0400,
	0x0004, 0x0004, 0x0200, 0x0200, 0x0002, 0x0002, 0x0600, 0x0600,
	0x0006, 0x0006, 0x0200, 0x0200, 0x0002, 0x0002, 0x0400, 0x0400,
	0x0004, 0x0004, 0x0200, 0x0200, 0x0002, 0x0002, 0x0800, 0x0800,
	0x0008, 0x0008, 0x0200, 0x0200, 0x0002, 0x0002, 0x0400, 0x0400,
	0x0004, 0x0004, 0x0200, 0x0200, 0x0002, 0x0002, 0x0600, 0x0600,
	0x0006, 0x0006, 0x0200, 0x0200, 0x0002, 0x0002, 0x0400, 0x0400,
	0x0004, 0x0004, 0x0200, 0x0200, 0x0002, 0x0002, 0x0000, 0x0000,
	// 8[1]: 256
	0x0000, 0x0000, 0x0000, 0x0000, 0x0200, 0x0200, 0x0200, 0x0200,
	0x0002, 0x0002, 0x0002, 0x0002, 0x0400, 0x0400, 0x0400, 0x0400,
	0x0004, 0x0004, 0x0004, 0x0004, 0x0200, 0x0200, 0x0200, 0x0200,
	0x0002, 0x0002, 0x0002, 0x0002, 0x0600, 0x0600, 0x0600, 0x0600,
	0x0006, 0x0006, 0x0006, 0x0006, 0x0200, 0x0200, 0x0200, 0x0200,
	0x0002, 0x0002, 0x0002, 0x0002, 0x0400, 0x0400, 0x0400, 0x0400,
	0x0004, 0x0004, 0x0004, 0x0004, 0x0200, 0x0200, 0x0200, 0x0200,
	0x0002, 0x0002, 0x0002, 0x0002, 0x0800, 0x0800, 0x0800, 0x0800,
	0x0008, 0x0008, 0x0008, 0x0008, 0x0200, 0x0200, 0x0200, 0x0200,
	0x0002, 0x0002, 0x0002, 0x0002, 0x0400, 0x0400, 0x0400, 0x0400,
	0x0004, 0x0004, 0x0004, 0x0004, 0x0200, 0x0200, 0x0200, 0x0200,
	0x0002, 0x0002, 0x0002, 0x0002, 0x0600, 0x0600, 0x0600, 0x0600,
	0x0006, 0x0006, 0x0006, 0x0006, 0x0200, 0x0200, 0x0200, 0x0200,
	0x0002, 0x0002, 0x0002, 0x0002, 0x0400, 0x0400, 0x0400, 0x0400,
	0x0004, 0x0004, 0x0004, 0x0004, 0x0200, 0x0200, 0x0200, 0x0200,
	0x0002, 0x0002, 0x0002, 0x0002, 0x0a00, 0x0a00, 0x0a00, 0x0a00,
	0x000a, 0x000a, 0x000a, 0x000a, 0x0200, 0x0200, 0x0200, 0x0200,
	0x0002, 0x0002, 0x0002, 0x0002, 0x0400, 0x0400, 0x0400, 0x0400,
	0x0004, 0x0004, 0x0004, 0x0004, 0x0200, 0x0200, 0x0200, 0x0200,
	0x0002, 0x0002, 0x0002, 0x0002, 0x0600, 0x0600, 0x0600, 0x0600,
	0x0006, 0x0006, 0x0006, 0x0006, 0x0200, 0x0200, 0x0200, 0x0200,
	0x0002, 0x0002, 0x0002, 0x0002, 0x0400, 0x0400, 0x0400, 0x0400,
	0x0004, 0x0004, 0x0004, 0x0004, 0x0200, 0x0200, 0x0200, 0x0200,
	0x0002, 0x0002, 0x0002, 0x0002, 0x0800, 0x0800, 0x0800, 0x0800,
	0x0008, 0x0008, 0x0008, 0x0008, 0x0200, 0x0200, 0x0200, 0x0200,
	0x0002, 0x0002, 0x0002, 0x0002, 0x0400, 0x0400, 0x0400, 0x0400,
	0x0004, 0x0004, 0x0004, 0x0004, 0x0200, 0x0200, 0x0200, 0x0200,
	0x0002, 0x0002, 0x0002, 0x0002, 0x0600, 0x0600, 0x0600, 0x0600,
	0x0006, 0x0006, 0x0006, 0x0006, 0x0200, 0x0200, 0x0200, 0x0200,
	0x0002, 0x0002, 0x0002, 0x0002, 0x0400, 0x0400, 0x0400, 0x0400,
	0x0004, 0x0004, 0x0004, 0x0004, 0x0200, 0x0200, 0x0200, 0x0200,
	0x0002, 0x0002, 0x0002, 0x0002, 0x0000, 0x0000, 0x0000, 0x0000,
	// 8[2]: 512
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
	0x0000, 0x0002, 0x0200, 0x0000, 0x0000, 0x0002, 0x0200, 0x0000,
	// 8[3]: 768
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
	0x0000, 0x0004, 0x0002, 0x0002, 0x0200, 0x0200, 0x0400, 0x0000,
	// 8[4]: 1024
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
	0x0200, 0x0200, 0x0200, 0x0200, 0x0400, 0x0400, 0x0600, 0x0000,
	// 8[5]: 1280
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
	0x0400, 0x0400, 0x0400, 0x0400, 0x0600, 0x0600, 0x0800, 0x0000,
	// 8[6]: 1536
	0x0000, 0x000a, 0x0008, 0x0008, 0x0006, 0x0006, 0x0006, 0x0006,
	0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004,
	0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002,
	0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002,
	0x0200, 0x0200, 0x0200, 0x0200, 0x0200, 0x0200, 0x0200, 0x0200,
	0x0200, 0x0200, 0x0200, 0x0200, 0x0200, 0x0200, 0x0200, 0x0200,
	0x0400, 0x0400, 0x0400, 0x0400, 0x0400, 0x0400, 0x0400, 0x0400,
	0x0600, 0x0600, 0x0600, 0x0600, 0x0800, 0x0800, 0x0a00, 0x0000,
	0x0000, 0x000a, 0x0008, 0x0008, 0x0006, 0x0006, 0x0006, 0x0006,
	0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004,
	0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002,
	0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002,
	0x0200, 0x0200, 0x0200, 0x0200, 0x0200, 0x0200, 0x0200, 0x0200,
	0x0200, 0x0200, 0x0200, 0x0200, 0x0200, 0x0200, 0x0200, 0x0200,
	0x0400, 0x0400, 0x0400, 0x0400, 0x0400, 0x0400, 0x0400, 0x0400,
	0x0600, 0x0600, 0x0600, 0x0600, 0x0800, 0x0800, 0x0a00, 0x0000,
	0x0000, 0x000a, 0x0008, 0x0008, 0x0006, 0x0006, 0x0006, 0x0006,
	0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004,
	0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002,
	0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002,
	0x0200, 0x0200, 0x0200, 0x0200, 0x0200, 0x0200, 0x0200, 0x0200,
	0x0200, 0x0200, 0x0200, 0x0200, 0x0200, 0x0200, 0x0200, 0x0200,
	0x0400, 0x0400, 0x0400, 0x0400, 0x0400, 0x0400, 0x0400, 0x0400,
	0x0600, 0x0600, 0x0600, 0x0600, 0x0800, 0x0800, 0x0a00, 0x0000,
	0x0000, 0x000a, 0x0008, 0x0008, 0x0006, 0x0006, 0x0006, 0x0006,
	0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004,
	0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002,
	0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002,
	0x0200, 0x0200, 0x0200, 0x0200, 0x0200, 0x0200, 0x0200, 0x0200,
	0x0200, 0x0200, 0x0200, 0x0200, 0x0200, 0x0200, 0x0200, 0x0200,
	0x0400, 0x0400, 0x0400, 0x0400, 0x0400, 0x0400, 0x0400, 0x0400,
	0x0600, 0x0600, 0x0600, 0x0600, 0x0800, 0x0800, 0x0a00, 0x0000,
	// 8[7]: 1792
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
	0x0800, 0x0800, 0x0800, 0x0800, 0x0a00, 0x0a00, 0x0c00, 0x0000,
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
	0x0800, 0x0800, 0x0800, 0x0800, 0x0a00, 0x0a00, 0x0c00, 0x0000,
	// 7[0]: 2048
	0x0000, 0x0200, 0x0002, 0x0400, 0x0004, 0x0200, 0x0002, 0x0600,
	0x0006, 0x0200, 0x0002, 0x0400, 0x0004, 0x0200, 0x0002, 0x0800,
	0x0008, 0x0200, 0x0002, 0x0400, 0x0004, 0x0200, 0x0002, 0x0600,
	0x0006, 0x0200, 0x0002, 0x0400, 0x0004, 0x0200, 0x0002, 0x0a00,
	0x000a, 0x0200, 0x0002, 0x0400, 0x0004, 0x0200, 0x0002, 0x0600,
	0x0006, 0x0200, 0x0002, 0x0400, 0x0004, 0x0200, 0x0002, 0x0800,
	0x0008, 0x0200, 0x0002, 0x0400, 0x0004, 0x0200, 0x0002, 0x0600,
	0x0006, 0x0200, 0x0002, 0x0400, 0x0004, 0x0200, 0x0002, 0x0000,
	// 7[2]: 2112
	0x0000, 0x0002, 0x0200, 0x0000, 0x0200, 0x0202, 0x0400, 0x0200,
	0x0002, 0x0004, 0x0202, 0x0002, 0x0400, 0x0402, 0x0600, 0x0400,
	0x0004, 0x0006, 0x0204, 0x0004, 0x0200, 0x0202, 0x0400, 0x0200,
	0x0002, 0x0004, 0x0202, 0x0002, 0x0600, 0x0602, 0x0800, 0x0600,
	0x0006, 0x0008, 0x0206, 0x0006, 0x0200, 0x0202, 0x0400, 0x0200,
	0x0002, 0x0004, 0x0202, 0x0002, 0x0400, 0x0402, 0x0600, 0x0400,
	0x0004, 0x0006, 0x0204, 0x0004, 0x0200, 0x0202, 0x0400, 0x0200,
	0x0002, 0x0004, 0x0202, 0x0002, 0x0000, 0x0002, 0x0200, 0x0000,
	// 7[3]: 2176
	0x0000, 0x0004, 0x0002, 0x0002, 0x0200, 0x0200, 0x0400, 0x0000,
	0x0200, 0x0204, 0x0202, 0x0202, 0x0400, 0x0400, 0x0600, 0x0200,
	0x0002, 0x0006, 0x0004, 0x0004, 0x0202, 0x0202, 0x0402, 0x0002,
	0x0400, 0x0404, 0x0402, 0x0402, 0x0600, 0x0600, 0x0800, 0x0400,
	0x0004, 0x0008, 0x0006, 0x0006, 0x0204, 0x0204, 0x0404, 0x0004,
	0x0200, 0x0204, 0x0202, 0x0202, 0x0400, 0x0400, 0x0600, 0x0200,
	0x0002, 0x0006, 0x0004, 0x0004, 0x0202, 0x0202, 0x0402, 0x0002,
	0x0000, 0x0004, 0x0002, 0x0002, 0x0200, 0x0200, 0x0400, 0x0000,
	// 7[4]: 2240
	0x0000, 0x0006, 0x0004, 0x0004, 0x0002, 0x0002, 0x0002, 0x0002,
	0x0200, 0x0200, 0x0200, 0x0200, 0x0400, 0x0400, 0x0600, 0x0000,
	0x0200, 0x0206, 0x0204, 0x0204, 0x0202, 0x0202, 0x0202, 0x0202,
	0x0400, 0x0400, 0x0400, 0x0400, 0x0600, 0x0600, 0x0800, 0x0200,
	0x0002, 0x0008, 0x0006, 0x0006, 0x0004, 0x0004, 0x0004, 0x0004,
	0x0202, 0x0202, 0x0202, 0x0202, 0x0402, 0x0402, 0x0602, 0x0002,
	0x0000, 0x0006, 0x0004, 0x0004, 0x0002, 0x0002, 0x0002, 0x0002,
	0x0200, 0x0200, 0x0200, 0x0200, 0x0400, 0x0400, 0x0600, 0x0000,
	// 7[6]: 2304
	0x0000, 0x000a, 0x0008, 0x0008, 0x0006, 0x0006, 0x0006, 0x0006,
	0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004,
	0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002,
	0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002,
	0x0200, 0x0200, 0x0200, 0x0200, 0x0200, 0x0200, 0x0200, 0x0200,
	0x0200, 0x0200, 0x0200, 0x0200, 0x0200, 0x0200, 0x0200, 0x0200,
	0x0400, 0x0400, 0x0400, 0x0400, 0x0400, 0x0400, 0x0400, 0x0400,
	0x0600, 0x0600, 0x0600, 0x0600, 0x0800, 0x0800, 0x0a00, 0x0000,
	// 6[0]: 2368
	0x0000, 0x0200, 0x0002, 0x0400, 0x0004, 0x0200, 0x0002, 0x0600,
	0x0006, 0x0200, 0x0002, 0x0400, 0x0004, 0x0200, 0x0002, 0x0800,
	0x0008, 0x0200, 0x0002, 0x0400, 0x0004, 0x0200, 0x0002, 0x0600,
	0x0006, 0x0200, 0x0002, 0x0400, 0x0004, 0x0200, 0x0002, 0x0000,
	// 6[2]: 2400
	0x0000, 0x0002, 0x0200, 0x0000, 0x0200, 0x0202, 0x0400, 0x0200,
	0x0002, 0x0004, 0x0202, 0x0002, 0x0400, 0x0402, 0x0600, 0x0400,
	0x0004, 0x0006, 0x0204, 0x0004, 0x0200, 0x0202, 0x0400, 0x0200,
	0x0002, 0x0004, 0x0202, 0x0002, 0x0000, 0x0002, 0x0200, 0x0000,
	// 6[3]: 2432
	0x0000, 0x0004, 0x0002, 0x0002, 0x0200, 0x0200, 0x0400, 0x0000,
	0x0200, 0x0204, 0x0202, 0x0202, 0x0400, 0x0400, 0x0600, 0x0200,
	0x0002, 0x0006, 0x0004, 0x0004, 0x0202, 0x0202, 0x0402, 0x0002,
	0x0000, 0x0004, 0x0002, 0x0002, 0x0200, 0x0200, 0x0400, 0x0000,
	// 6[5]: 2464
	0x0000, 0x0008, 0x0006, 0x0006, 0x0004, 0x0004, 0x0004, 0x0004,
	0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002,
	0x0200, 0x0200, 0x0200, 0x0200, 0x0200, 0x0200, 0x0200, 0x0200,
	0x0400, 0x0400, 0x0400, 0x0400, 0x0600, 0x0600, 0x0800, 0x0000,
	// 5[0]: 2496
	0x0000, 0x0200, 0x0002, 0x0400, 0x0004, 0x0200, 0x0002, 0x0600,
	0x0006, 0x0200, 0x0002, 0x0400, 0x0004, 0x0200, 0x0002, 0x0000,
	// 5[2]: 2512
	0x0000, 0x0002, 0x0200, 0x0000, 0x0200, 0x0202, 0x0400, 0x0200, 
	0x0002, 0x0004, 0x0202, 0x0002, 0x0000, 0x0002, 0x0200, 0x0000, 
	// 5[4]: 2528
	0x0000, 0x0006, 0x0004, 0x0004, 0x0002, 0x0002, 0x0002, 0x0002,
	0x0200, 0x0200, 0x0200, 0x0200, 0x0400, 0x0400, 0x0600, 0x0000,
	// 4[0]: 2544
	0x0000, 0x0200, 0x0002, 0x0400, 0x0004, 0x0200, 0x0002, 0x0000,
	// 4[3]: 2552
	0x0000, 0x0004, 0x0002, 0x0002, 0x0200, 0x0200, 0x0400, 0x0000,
	// 3[0]: 2560
	0x0000, 0x0200, 0x0002, 0x0000,
	// 3[2]: 2564
	0x0000, 0x0002, 0x0200, 0x0000,
};

enum {
	CF80 =    0, CF81 =  256, CF82 =  512, CF83 =  768, CF84 = 1024, CF85 = 1280, CF86 = 1536, CF87 = 1792,
	CF70 = 2048, CF72 = 2112, CF73 = 2176, CF74 = 2240, CF76 = 2304,
	CF60 = 2368, CF62 = 2400, CF63 = 2432, CF65 = 2464,
	CF50 = 2496, CF52 = 2512, CF54 = 2528,
	CF40 = 2544, CF43 = 2552,
	CF30 = 2560, CF32 = 2564
};

const unsigned short cf_ofs_d[2][64] = {{
	   0,    0, CF30, CF40, CF50, CF60, CF70, CF80,
           0,    0, CF30, CF40, CF50, CF60, CF81, CF70,	// CF31 -> 0, CF41..CF71 -> CF30..CF60
	CF32, CF32, CF52, CF62, CF72, CF82, CF60, CF60,	// CF42 -> CF32, CF71 -> CF60
	CF43, CF43, CF63, CF73, CF83, CF72, CF50, CF50,	// CF53 -> CF43, CF61 -> CF50
	CF54, CF54, CF74, CF84, CF73, CF62, CF40, CF40,	// CF64 -> CF54, CF51 -> CF40
	CF65, CF65, CF85, CF74, CF63, CF52, CF30, CF30,	// CF75 -> CF65, CF41 -> CF30
	CF76, CF86, CF65, CF54, CF43, CF32,    0,    0,	// CF31 -> 0, CF75..CF42 -> CF65..CF32
	CF87, CF76, CF65, CF54, CF43, CF32,    0,    0
}, {
	CF80, CF70, CF60, CF50, CF40, CF30,    0,    0,
	CF70, CF81, CF60, CF50, CF40, CF30,    0,    0,
	CF60, CF60, CF82, CF72, CF62, CF52, CF32, CF32,
	CF50, CF50, CF72, CF83, CF73, CF63, CF43, CF43,
	CF40, CF40, CF62, CF73, CF84, CF74, CF54, CF54,
	CF30, CF30, CF52, CF63, CF74, CF85, CF65, CF65,
	   0,    0, CF32, CF43, CF54, CF65, CF86, CF76,
           0,    0, CF32, CF43, CF54, CF65, CF76, CF87
}};

/* bit masks for diagonal lines */
const unsigned long long cf_mask_d[2][64] = {{
	0x0000000000000000, 0x0000000000000000, 0x0000000000010200, 0x0000000001020400, 0x0000000102040800, 0x0000010204081000, 0x0001020408102000, 0x0102040810204080,
	0x0000000000000000, 0x0000000000000000, 0x0000000001020000, 0x0000000102040000, 0x0000010204080000, 0x0001020408100000, 0x0102040810204080, 0x0204081020400000,
	0x0000000000000204, 0x0000000000000408, 0x0000000102000810, 0x0000010204001020, 0x0001020408002040, 0x0102040810204080, 0x0204081020000000, 0x0408102040000000,
	0x0000000000020408, 0x0000000000040810, 0x0000010200081020, 0x0001020400102040, 0x0102040810204080, 0x0204081000408000, 0x0408102000000000, 0x0810204000000000,
	0x0000000002040810, 0x0000000004081020, 0x0001020008102040, 0x0102040810204080, 0x0204080020408000, 0x0408100040800000, 0x0810200000000000, 0x1020400000000000,
	0x0000000204081020, 0x0000000408102040, 0x0102040810204080, 0x0204001020408000, 0x0408002040800000, 0x0810004080000000, 0x1020000000000000, 0x2040000000000000,
	0x0000020408102040, 0x0102040810204080, 0x0000081020408000, 0x0000102040800000, 0x0000204080000000, 0x0000408000000000, 0x0000000000000000, 0x0000000000000000,
	0x0102040810204080, 0x0004081020408000, 0x0008102040800000, 0x0010204080000000, 0x0020408000000000, 0x0040800000000000, 0x0000000000000000, 0x0000000000000000
}, {
	0x8040201008040201, 0x0080402010080400, 0x0000804020100800, 0x0000008040201000, 0x0000000080402000, 0x0000000000804000, 0x0000000000000000, 0x0000000000000000,
	0x4020100804020000, 0x8040201008040201, 0x0080402010080000, 0x0000804020100000, 0x0000008040200000, 0x0000000080400000, 0x0000000000000000, 0x0000000000000000,
	0x2010080402000000, 0x4020100804000000, 0x8040201008040201, 0x0080402010000402, 0x0000804020000804, 0x0000008040001008, 0x0000000000002010, 0x0000000000004020,
	0x1008040200000000, 0x2010080400000000, 0x4020100800020100, 0x8040201008040201, 0x0080402000080402, 0x0000804000100804, 0x0000000000201008, 0x0000000000402010,
	0x0804020000000000, 0x1008040000000000, 0x2010080002010000, 0x4020100004020100, 0x8040201008040201, 0x0080400010080402, 0x0000000020100804, 0x0000000040201008,
	0x0402000000000000, 0x0804000000000000, 0x1008000201000000, 0x2010000402010000, 0x4020000804020100, 0x8040201008040201, 0x0000002010080402, 0x0000004020100804,
	0x0000000000000000, 0x0000000000000000, 0x0000020100000000, 0x0000040201000000, 0x0000080402010000, 0x0000100804020100, 0x8040201008040201, 0x0000402010080402,
	0x0000000000000000, 0x0000000000000000, 0x0002010000000000, 0x0004020100000000, 0x0008040201000000, 0x0010080402010000, 0x0020100804020100, 0x8040201008040201
}};

/**
 * Count last flipped discs when playing on the last empty.
 *
 * @param pos the last empty square.
 * @param P player's disc pattern.
 * @return flipped disc count.
 */

inline int last_flip(int pos, unsigned long long P)
{
	uint_fast8_t n_flipped;
	int x = pos & 7;

	// n_flipped = (uint8_t) COUNT_FLIP[x * 256 + _bextr_u64(P, pos & 0x38, 8)];
	n_flipped  = (uint8_t) COUNT_FLIP[x * 256 + ((P >> (pos & 0x38)) & 0xFF)];
	n_flipped += (uint8_t) COUNT_FLIP[cf_ofs_d[0][pos] + _pext_u64(P, cf_mask_d[0][pos])];
	n_flipped += (uint8_t) COUNT_FLIP[cf_ofs_d[1][pos] + _pext_u64(P, cf_mask_d[1][pos])];
	n_flipped += (uint8_t) COUNT_FLIP[((pos & 0x38) << 5) + _pext_u64(P, 0x0101010101010101 << x)];

	return n_flipped;
}

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
	uint_fast16_t	op_flip;
	int p_flips, o_flips;
	int score = 2 * bit_count(P) - 64 + 2;	// = (bit_count(P) + 1) - (SCORE_MAX - 1 - bit_count(P))
	int x = pos & 7;

	// op_flip = COUNT_FLIP[x * 256 + _bextr_u64(P, pos & 0x38, 8)];
	op_flip  = COUNT_FLIP[x * 256 + ((P >> (pos & 0x38)) & 0xFF)];
	op_flip += COUNT_FLIP[cf_ofs_d[0][pos] + _pext_u64(P, cf_mask_d[0][pos])];
	op_flip += COUNT_FLIP[cf_ofs_d[1][pos] + _pext_u64(P, cf_mask_d[1][pos])];
	op_flip += COUNT_FLIP[((pos & 0x38) << 5) + _pext_u64(P, 0x0101010101010101 << x)];

	o_flips = op_flip >> 8;
	p_flips = op_flip & 0xFF;
	if (p_flips == 0)	// (23%)
		score -= o_flips + (int)((-o_flips | (score - 1)) < 0) * 2;	// last square for O if O can move or score <= 0
	(void) alpha;	// no lazy cut-off
	return score + p_flips;
}

inline int vectorcall board_score_sse_1(__m128i OP, const int alpha, const int x) {
	return board_score_1(_mm_cvtsi128_si64(OP), alpha, x);
}
