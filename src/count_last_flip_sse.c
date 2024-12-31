/**
 * @file count_last_flip_sse.c
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
 * SSE PMOVMSKB or PSADBW instruction.
 * Once we get our 8-bits disc patterns, we directly get the number of
 * flipped discs from the precomputed array, and add them from each flipping
 * lines.
 * For optimization purpose, the value returned is twice the number of flipped
 * disc, to facilitate the computation of disc difference.
 *
 * @date 1998 - 2023
 * @author Richard Delorme
 * @author Toshihiko Okuhara
 * @version 4.5
 * 
 */

#include "bit.h"
#include <stdint.h>

/** precomputed count flip array */
static const uint8_t COUNT_FLIP[8][256] = {
	{
		 0,  0,  0,  0,  2,  2,  0,  0,  4,  4,  0,  0,  2,  2,  0,  0,  6,  6,  0,  0,  2,  2,  0,  0,  4,  4,  0,  0,  2,  2,  0,  0,
		 8,  8,  0,  0,  2,  2,  0,  0,  4,  4,  0,  0,  2,  2,  0,  0,  6,  6,  0,  0,  2,  2,  0,  0,  4,  4,  0,  0,  2,  2,  0,  0,
		10, 10,  0,  0,  2,  2,  0,  0,  4,  4,  0,  0,  2,  2,  0,  0,  6,  6,  0,  0,  2,  2,  0,  0,  4,  4,  0,  0,  2,  2,  0,  0,
		 8,  8,  0,  0,  2,  2,  0,  0,  4,  4,  0,  0,  2,  2,  0,  0,  6,  6,  0,  0,  2,  2,  0,  0,  4,  4,  0,  0,  2,  2,  0,  0,
		12, 12,  0,  0,  2,  2,  0,  0,  4,  4,  0,  0,  2,  2,  0,  0,  6,  6,  0,  0,  2,  2,  0,  0,  4,  4,  0,  0,  2,  2,  0,  0,
		 8,  8,  0,  0,  2,  2,  0,  0,  4,  4,  0,  0,  2,  2,  0,  0,  6,  6,  0,  0,  2,  2,  0,  0,  4,  4,  0,  0,  2,  2,  0,  0,
		10, 10,  0,  0,  2,  2,  0,  0,  4,  4,  0,  0,  2,  2,  0,  0,  6,  6,  0,  0,  2,  2,  0,  0,  4,  4,  0,  0,  2,  2,  0,  0,
		 8,  8,  0,  0,  2,  2,  0,  0,  4,  4,  0,  0,  2,  2,  0,  0,  6,  6,  0,  0,  2,  2,  0,  0,  4,  4,  0,  0,  2,  2,  0,  0,
	},
	{
		 0,  0,  0,  0,  0,  0,  0,  0,  2,  2,  2,  2,  0,  0,  0,  0,  4,  4,  4,  4,  0,  0,  0,  0,  2,  2,  2,  2,  0,  0,  0,  0,
		 6,  6,  6,  6,  0,  0,  0,  0,  2,  2,  2,  2,  0,  0,  0,  0,  4,  4,  4,  4,  0,  0,  0,  0,  2,  2,  2,  2,  0,  0,  0,  0,
		 8,  8,  8,  8,  0,  0,  0,  0,  2,  2,  2,  2,  0,  0,  0,  0,  4,  4,  4,  4,  0,  0,  0,  0,  2,  2,  2,  2,  0,  0,  0,  0,
		 6,  6,  6,  6,  0,  0,  0,  0,  2,  2,  2,  2,  0,  0,  0,  0,  4,  4,  4,  4,  0,  0,  0,  0,  2,  2,  2,  2,  0,  0,  0,  0,
		10, 10, 10, 10,  0,  0,  0,  0,  2,  2,  2,  2,  0,  0,  0,  0,  4,  4,  4,  4,  0,  0,  0,  0,  2,  2,  2,  2,  0,  0,  0,  0,
		 6,  6,  6,  6,  0,  0,  0,  0,  2,  2,  2,  2,  0,  0,  0,  0,  4,  4,  4,  4,  0,  0,  0,  0,  2,  2,  2,  2,  0,  0,  0,  0,
		 8,  8,  8,  8,  0,  0,  0,  0,  2,  2,  2,  2,  0,  0,  0,  0,  4,  4,  4,  4,  0,  0,  0,  0,  2,  2,  2,  2,  0,  0,  0,  0,
		 6,  6,  6,  6,  0,  0,  0,  0,  2,  2,  2,  2,  0,  0,  0,  0,  4,  4,  4,  4,  0,  0,  0,  0,  2,  2,  2,  2,  0,  0,  0,  0,
	},
	{
		 0,  2,  0,  0,  0,  2,  0,  0,  0,  2,  0,  0,  0,  2,  0,  0,  2,  4,  2,  2,  2,  4,  2,  2,  0,  2,  0,  0,  0,  2,  0,  0,
		 4,  6,  4,  4,  4,  6,  4,  4,  0,  2,  0,  0,  0,  2,  0,  0,  2,  4,  2,  2,  2,  4,  2,  2,  0,  2,  0,  0,  0,  2,  0,  0,
		 6,  8,  6,  6,  6,  8,  6,  6,  0,  2,  0,  0,  0,  2,  0,  0,  2,  4,  2,  2,  2,  4,  2,  2,  0,  2,  0,  0,  0,  2,  0,  0,
		 4,  6,  4,  4,  4,  6,  4,  4,  0,  2,  0,  0,  0,  2,  0,  0,  2,  4,  2,  2,  2,  4,  2,  2,  0,  2,  0,  0,  0,  2,  0,  0,
		 8, 10,  8,  8,  8, 10,  8,  8,  0,  2,  0,  0,  0,  2,  0,  0,  2,  4,  2,  2,  2,  4,  2,  2,  0,  2,  0,  0,  0,  2,  0,  0,
		 4,  6,  4,  4,  4,  6,  4,  4,  0,  2,  0,  0,  0,  2,  0,  0,  2,  4,  2,  2,  2,  4,  2,  2,  0,  2,  0,  0,  0,  2,  0,  0,
		 6,  8,  6,  6,  6,  8,  6,  6,  0,  2,  0,  0,  0,  2,  0,  0,  2,  4,  2,  2,  2,  4,  2,  2,  0,  2,  0,  0,  0,  2,  0,  0,
		 4,  6,  4,  4,  4,  6,  4,  4,  0,  2,  0,  0,  0,  2,  0,  0,  2,  4,  2,  2,  2,  4,  2,  2,  0,  2,  0,  0,  0,  2,  0,  0,
	},
	{
		 0,  4,  2,  2,  0,  0,  0,  0,  0,  4,  2,  2,  0,  0,  0,  0,  0,  4,  2,  2,  0,  0,  0,  0,  0,  4,  2,  2,  0,  0,  0,  0,
		 2,  6,  4,  4,  2,  2,  2,  2,  2,  6,  4,  4,  2,  2,  2,  2,  0,  4,  2,  2,  0,  0,  0,  0,  0,  4,  2,  2,  0,  0,  0,  0,
		 4,  8,  6,  6,  4,  4,  4,  4,  4,  8,  6,  6,  4,  4,  4,  4,  0,  4,  2,  2,  0,  0,  0,  0,  0,  4,  2,  2,  0,  0,  0,  0,
		 2,  6,  4,  4,  2,  2,  2,  2,  2,  6,  4,  4,  2,  2,  2,  2,  0,  4,  2,  2,  0,  0,  0,  0,  0,  4,  2,  2,  0,  0,  0,  0,
		 6, 10,  8,  8,  6,  6,  6,  6,  6, 10,  8,  8,  6,  6,  6,  6,  0,  4,  2,  2,  0,  0,  0,  0,  0,  4,  2,  2,  0,  0,  0,  0,
		 2,  6,  4,  4,  2,  2,  2,  2,  2,  6,  4,  4,  2,  2,  2,  2,  0,  4,  2,  2,  0,  0,  0,  0,  0,  4,  2,  2,  0,  0,  0,  0,
		 4,  8,  6,  6,  4,  4,  4,  4,  4,  8,  6,  6,  4,  4,  4,  4,  0,  4,  2,  2,  0,  0,  0,  0,  0,  4,  2,  2,  0,  0,  0,  0,
		 2,  6,  4,  4,  2,  2,  2,  2,  2,  6,  4,  4,  2,  2,  2,  2,  0,  4,  2,  2,  0,  0,  0,  0,  0,  4,  2,  2,  0,  0,  0,  0,
	},
	{
		 0,  6,  4,  4,  2,  2,  2,  2,  0,  0,  0,  0,  0,  0,  0,  0,  0,  6,  4,  4,  2,  2,  2,  2,  0,  0,  0,  0,  0,  0,  0,  0,
		 0,  6,  4,  4,  2,  2,  2,  2,  0,  0,  0,  0,  0,  0,  0,  0,  0,  6,  4,  4,  2,  2,  2,  2,  0,  0,  0,  0,  0,  0,  0,  0,
		 2,  8,  6,  6,  4,  4,  4,  4,  2,  2,  2,  2,  2,  2,  2,  2,  2,  8,  6,  6,  4,  4,  4,  4,  2,  2,  2,  2,  2,  2,  2,  2,
		 0,  6,  4,  4,  2,  2,  2,  2,  0,  0,  0,  0,  0,  0,  0,  0,  0,  6,  4,  4,  2,  2,  2,  2,  0,  0,  0,  0,  0,  0,  0,  0,
		 4, 10,  8,  8,  6,  6,  6,  6,  4,  4,  4,  4,  4,  4,  4,  4,  4, 10,  8,  8,  6,  6,  6,  6,  4,  4,  4,  4,  4,  4,  4,  4,
		 0,  6,  4,  4,  2,  2,  2,  2,  0,  0,  0,  0,  0,  0,  0,  0,  0,  6,  4,  4,  2,  2,  2,  2,  0,  0,  0,  0,  0,  0,  0,  0,
		 2,  8,  6,  6,  4,  4,  4,  4,  2,  2,  2,  2,  2,  2,  2,  2,  2,  8,  6,  6,  4,  4,  4,  4,  2,  2,  2,  2,  2,  2,  2,  2,
		 0,  6,  4,  4,  2,  2,  2,  2,  0,  0,  0,  0,  0,  0,  0,  0,  0,  6,  4,  4,  2,  2,  2,  2,  0,  0,  0,  0,  0,  0,  0,  0,
	},
	{
		 0,  8,  6,  6,  4,  4,  4,  4,  2,  2,  2,  2,  2,  2,  2,  2,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
		 0,  8,  6,  6,  4,  4,  4,  4,  2,  2,  2,  2,  2,  2,  2,  2,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
		 0,  8,  6,  6,  4,  4,  4,  4,  2,  2,  2,  2,  2,  2,  2,  2,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
		 0,  8,  6,  6,  4,  4,  4,  4,  2,  2,  2,  2,  2,  2,  2,  2,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
		 2, 10,  8,  8,  6,  6,  6,  6,  4,  4,  4,  4,  4,  4,  4,  4,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
		 2, 10,  8,  8,  6,  6,  6,  6,  4,  4,  4,  4,  4,  4,  4,  4,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
		 0,  8,  6,  6,  4,  4,  4,  4,  2,  2,  2,  2,  2,  2,  2,  2,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
		 0,  8,  6,  6,  4,  4,  4,  4,  2,  2,  2,  2,  2,  2,  2,  2,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	},
	{
		 0, 10,  8,  8,  6,  6,  6,  6,  4,  4,  4,  4,  4,  4,  4,  4,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
		 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
		 0, 10,  8,  8,  6,  6,  6,  6,  4,  4,  4,  4,  4,  4,  4,  4,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
		 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
		 0, 10,  8,  8,  6,  6,  6,  6,  4,  4,  4,  4,  4,  4,  4,  4,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
		 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
		 0, 10,  8,  8,  6,  6,  6,  6,  4,  4,  4,  4,  4,  4,  4,  4,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
		 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	},
	{
		 0, 12, 10, 10,  8,  8,  8,  8,  6,  6,  6,  6,  6,  6,  6,  6,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
		 2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
		 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
		 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
		 0, 12, 10, 10,  8,  8,  8,  8,  6,  6,  6,  6,  6,  6,  6,  6,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
		 2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
		 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
		 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	},
};

/* bit masks for diagonal lines */
const V4DI mask_dvhd[64] = {
	{{ 0x0000000000000001, 0x00000000000000ff, 0x0101010101010101, 0x8040201008040201 }},
	{{ 0x0000000000000102, 0x00000000000000ff, 0x0202020202020202, 0x0080402010080402 }},
	{{ 0x0000000000010204, 0x00000000000000ff, 0x0404040404040404, 0x0000804020100804 }},
	{{ 0x0000000001020408, 0x00000000000000ff, 0x0808080808080808, 0x0000008040201008 }},
	{{ 0x0000000102040810, 0x00000000000000ff, 0x1010101010101010, 0x0000000080402010 }},
	{{ 0x0000010204081020, 0x00000000000000ff, 0x2020202020202020, 0x0000000000804020 }},
	{{ 0x0001020408102040, 0x00000000000000ff, 0x4040404040404040, 0x0000000000008040 }},
	{{ 0x0102040810204080, 0x00000000000000ff, 0x8080808080808080, 0x0000000000000080 }},
	{{ 0x0000000000000102, 0x000000000000ff00, 0x0101010101010101, 0x4020100804020100 }},
	{{ 0x0000000000010204, 0x000000000000ff00, 0x0202020202020202, 0x8040201008040201 }},
	{{ 0x0000000001020408, 0x000000000000ff00, 0x0404040404040404, 0x0080402010080402 }},
	{{ 0x0000000102040810, 0x000000000000ff00, 0x0808080808080808, 0x0000804020100804 }},
	{{ 0x0000010204081020, 0x000000000000ff00, 0x1010101010101010, 0x0000008040201008 }},
	{{ 0x0001020408102040, 0x000000000000ff00, 0x2020202020202020, 0x0000000080402010 }},
	{{ 0x0102040810204080, 0x000000000000ff00, 0x4040404040404040, 0x0000000000804020 }},
	{{ 0x0204081020408000, 0x000000000000ff00, 0x8080808080808080, 0x0000000000008040 }},
	{{ 0x0000000000010204, 0x0000000000ff0000, 0x0101010101010101, 0x2010080402010000 }},
	{{ 0x0000000001020408, 0x0000000000ff0000, 0x0202020202020202, 0x4020100804020100 }},
	{{ 0x0000000102040810, 0x0000000000ff0000, 0x0404040404040404, 0x8040201008040201 }},
	{{ 0x0000010204081020, 0x0000000000ff0000, 0x0808080808080808, 0x0080402010080402 }},
	{{ 0x0001020408102040, 0x0000000000ff0000, 0x1010101010101010, 0x0000804020100804 }},
	{{ 0x0102040810204080, 0x0000000000ff0000, 0x2020202020202020, 0x0000008040201008 }},
	{{ 0x0204081020408000, 0x0000000000ff0000, 0x4040404040404040, 0x0000000080402010 }},
	{{ 0x0408102040800000, 0x0000000000ff0000, 0x8080808080808080, 0x0000000000804020 }},
	{{ 0x0000000001020408, 0x00000000ff000000, 0x0101010101010101, 0x1008040201000000 }},
	{{ 0x0000000102040810, 0x00000000ff000000, 0x0202020202020202, 0x2010080402010000 }},
	{{ 0x0000010204081020, 0x00000000ff000000, 0x0404040404040404, 0x4020100804020100 }},
	{{ 0x0001020408102040, 0x00000000ff000000, 0x0808080808080808, 0x8040201008040201 }},
	{{ 0x0102040810204080, 0x00000000ff000000, 0x1010101010101010, 0x0080402010080402 }},
	{{ 0x0204081020408000, 0x00000000ff000000, 0x2020202020202020, 0x0000804020100804 }},
	{{ 0x0408102040800000, 0x00000000ff000000, 0x4040404040404040, 0x0000008040201008 }},
	{{ 0x0810204080000000, 0x00000000ff000000, 0x8080808080808080, 0x0000000080402010 }},
	{{ 0x0000000102040810, 0x000000ff00000000, 0x0101010101010101, 0x0804020100000000 }},
	{{ 0x0000010204081020, 0x000000ff00000000, 0x0202020202020202, 0x1008040201000000 }},
	{{ 0x0001020408102040, 0x000000ff00000000, 0x0404040404040404, 0x2010080402010000 }},
	{{ 0x0102040810204080, 0x000000ff00000000, 0x0808080808080808, 0x4020100804020100 }},
	{{ 0x0204081020408000, 0x000000ff00000000, 0x1010101010101010, 0x8040201008040201 }},
	{{ 0x0408102040800000, 0x000000ff00000000, 0x2020202020202020, 0x0080402010080402 }},
	{{ 0x0810204080000000, 0x000000ff00000000, 0x4040404040404040, 0x0000804020100804 }},
	{{ 0x1020408000000000, 0x000000ff00000000, 0x8080808080808080, 0x0000008040201008 }},
	{{ 0x0000010204081020, 0x0000ff0000000000, 0x0101010101010101, 0x0402010000000000 }},
	{{ 0x0001020408102040, 0x0000ff0000000000, 0x0202020202020202, 0x0804020100000000 }},
	{{ 0x0102040810204080, 0x0000ff0000000000, 0x0404040404040404, 0x1008040201000000 }},
	{{ 0x0204081020408000, 0x0000ff0000000000, 0x0808080808080808, 0x2010080402010000 }},
	{{ 0x0408102040800000, 0x0000ff0000000000, 0x1010101010101010, 0x4020100804020100 }},
	{{ 0x0810204080000000, 0x0000ff0000000000, 0x2020202020202020, 0x8040201008040201 }},
	{{ 0x1020408000000000, 0x0000ff0000000000, 0x4040404040404040, 0x0080402010080402 }},
	{{ 0x2040800000000000, 0x0000ff0000000000, 0x8080808080808080, 0x0000804020100804 }},
	{{ 0x0001020408102040, 0x00ff000000000000, 0x0101010101010101, 0x0201000000000000 }},
	{{ 0x0102040810204080, 0x00ff000000000000, 0x0202020202020202, 0x0402010000000000 }},
	{{ 0x0204081020408000, 0x00ff000000000000, 0x0404040404040404, 0x0804020100000000 }},
	{{ 0x0408102040800000, 0x00ff000000000000, 0x0808080808080808, 0x1008040201000000 }},
	{{ 0x0810204080000000, 0x00ff000000000000, 0x1010101010101010, 0x2010080402010000 }},
	{{ 0x1020408000000000, 0x00ff000000000000, 0x2020202020202020, 0x4020100804020100 }},
	{{ 0x2040800000000000, 0x00ff000000000000, 0x4040404040404040, 0x8040201008040201 }},
	{{ 0x4080000000000000, 0x00ff000000000000, 0x8080808080808080, 0x0080402010080402 }},
	{{ 0x0102040810204080, 0xff00000000000000, 0x0101010101010101, 0x0100000000000000 }},
	{{ 0x0204081020408000, 0xff00000000000000, 0x0202020202020202, 0x0201000000000000 }},
	{{ 0x0408102040800000, 0xff00000000000000, 0x0404040404040404, 0x0402010000000000 }},
	{{ 0x0810204080000000, 0xff00000000000000, 0x0808080808080808, 0x0804020100000000 }},
	{{ 0x1020408000000000, 0xff00000000000000, 0x1010101010101010, 0x1008040201000000 }},
	{{ 0x2040800000000000, 0xff00000000000000, 0x2020202020202020, 0x2010080402010000 }},
	{{ 0x4080000000000000, 0xff00000000000000, 0x4040404040404040, 0x4020100804020100 }},
	{{ 0x8000000000000000, 0xff00000000000000, 0x8080808080808080, 0x8040201008040201 }}
};

#if defined(__AVX512VL__) || defined(__AVX10_1__)
	#define	TEST_EPI8_MASK32(X,Y)	_cvtmask32_u32(_mm256_test_epi8_mask((X), (Y)))
	#define	TEST1_EPI8_MASK32(X)	_cvtmask32_u32(_mm256_test_epi8_mask((X), (X)))
	#define	TEST_EPI8_MASK16(X,Y)	_cvtmask16_u32(_mm_test_epi8_mask((X), (Y)))
#else	// AVX2,SSE2
	#define	TEST_EPI8_MASK32(X,Y)	_mm256_movemask_epi8(_mm256_sub_epi8(_mm256_setzero_si256(), _mm256_and_si256((X),(Y))))
	#define	TEST1_EPI8_MASK32(X)	_mm256_movemask_epi8(_mm256_sub_epi8(_mm256_setzero_si256(), (X)))
	#define	TEST_EPI8_MASK16(X,Y)	_mm_movemask_epi8(_mm_sub_epi8(_mm_setzero_si128(), _mm_and_si128((X),(Y))))
#endif

/**
 * Count last flipped discs when playing on the last empty.
 *
 * @param pos the last empty square.
 * @param P player's disc pattern.
 * @return flipped disc count.
 */
int last_flip(int pos, unsigned long long P)
{
	uint_fast8_t	n_flips;
	unsigned int	t;
	const uint8_t *COUNT_FLIP_X = COUNT_FLIP[pos & 7];
	const uint8_t *COUNT_FLIP_Y = COUNT_FLIP[pos >> 3];
  #ifdef AVXLASTFLIP	// no gain
	__m256i PP = _mm256_set1_epi64x(P);

	n_flips  = COUNT_FLIP_X[(P >> (pos & 0x38)) & 0xFF];
	t = TEST_EPI8_MASK32(PP, mask_dvhd[pos].v4);
	n_flips += COUNT_FLIP_Y[t & 0xFF];
	t >>= 16;

  #else
	__m128i PP = _mm_set1_epi64x(P);
	__m128i II = _mm_sad_epu8(_mm_and_si128(PP, mask_dvhd[pos].v2[0]), _mm_setzero_si128());

	n_flips  = COUNT_FLIP_X[_mm_extract_epi16(II, 4)];
	n_flips += COUNT_FLIP_X[_mm_cvtsi128_si32(II)];
	t = TEST_EPI8_MASK16(PP, mask_dvhd[pos].v2[1]);
  #endif
	n_flips += COUNT_FLIP_Y[t >> 8];
	n_flips += COUNT_FLIP_Y[t & 0xFF];

	return n_flips;
}

/**
 * @brief Get the final score.
 *
 * Get the final score, when 1 empty square remain.
 * The original code has been adapted from Zebra by Gunnar Anderson.
 *
 * @param PO     Board to evaluate. (O ignored)
 * @param alpha  Alpha bound. (beta - 1)
 * @param pos    Last empty square to play.
 * @return       The final score, as a disc difference.
 */
#if defined(__AVX2__) && defined(LASTFLIP_HIGHCUT)
// AVX2 NWS lazy high cut-off version
// http://www.amy.hi-ho.ne.jp/okuhara/edaxopt.htm#lazycutoff
// lazy high cut-off idea was in Zebra by Gunnar Anderson (http://radagast.se/othello/zebra.html),
// but commented out because mobility check was no faster than counting flips.
// Now using AVX2, mobility check can be faster than counting flips.
inline int vectorcall board_score_sse_1(__m128i OP, const int alpha, const int pos)
{
	int_fast8_t n_flips;
	uint32_t t;
	unsigned long long P = _mm_cvtsi128_si64(OP);
	int score = 2 * bit_count(P) - SCORE_MAX + 2;	// = (bit_count(P) + 1) - (SCORE_MAX - 1 - bit_count(P))
		// if player can move, final score > this score.
		// if player pass then opponent play, final score < score - 1 (cancel P) - 1 (last O).
		// if both pass, score - 1 (cancel P) - 1 (empty for O) <= final score <= score (empty for P).

	if (score > alpha) {	// if player can move, high cut-off will occur regardress of n_flips.
  #if 0 // def __AVX512VL__	// may trigger license base downclocking, wrong fingerprint on MSVC
		__m512i P8 = _mm512_broadcastq_epi64(OP);
		__m512i M = lrmask[pos].v8;
		__m512i mO = _mm512_andnot_epi64(P8, M);
		if (!_mm512_mask_test_epi64_mask(_mm512_test_epi64_mask(P8, M), mO, _mm512_set1_epi64(NEIGHBOUR[pos]))) {	// pass (16%)
			// n_flips = last_flip(pos, ~P);
			t = _cvtmask32_u32(_mm256_cmpneq_epi8_mask(_mm512_castsi512_si256(mO), _mm512_extracti64x4_epi64(mO, 1)));	// eq only if l = r = 0

  #elif defined(__AVX512VL__) || defined(__AVX10_1__)	// 256bit AVX512 (2.61s on skylake, 2.37 on icelake, 2.16s on Zen4)
		__m256i P4 = _mm256_broadcastq_epi64(OP);
		__m256i M = lrmask[pos].v4[0];
		__m256i F = _mm256_maskz_andnot_epi64(_mm256_test_epi64_mask(P4, M), P4, M);	// clear if all O
		M = lrmask[pos].v4[1];
		// F = _mm256_mask_or_epi64(F, _mm256_test_epi64_mask(P4, M), F, _mm256_andnot_si256(P4, M));
		F = _mm256_mask_ternarylogic_epi64(F, _mm256_test_epi64_mask(P4, M), P4, M, 0xF2);
		if (_mm256_testz_si256(F, _mm256_set1_epi64x(NEIGHBOUR[pos]))) {	// pass (16%)
			// n_flips = last_flip(pos, ~P);
			// t = _cvtmask32_u32(_mm256_cmpneq_epi8_mask(_mm256_andnot_si256(P4, lM), _mm256_andnot_si256(P4, rM)));
			t = _cvtmask32_u32(_mm256_test_epi8_mask(F, F));	// all O = all P = 0 flip

  #else	// AVX2
		__m256i P4 = _mm256_broadcastq_epi64(OP);
		__m256i M = lrmask[pos].v4[0];
		__m256i lmO = _mm256_andnot_si256(P4, M);
		__m256i F = _mm256_andnot_si256(_mm256_cmpeq_epi64(lmO, M), lmO);	// clear if all O
		M = lrmask[pos].v4[1];
		__m256i rmO = _mm256_andnot_si256(P4, M);
		F = _mm256_or_si256(F, _mm256_andnot_si256(_mm256_cmpeq_epi64(rmO, M), rmO));
		if (_mm256_testz_si256(F, _mm256_set1_epi64x(NEIGHBOUR[pos]))) {	// pass (16%)
			// n_flips = last_flip(pos, ~P);
			t = ~_mm256_movemask_epi8(_mm256_cmpeq_epi8(lmO, rmO));	// eq only if l = r = 0
  #endif
			const uint8_t *COUNT_FLIP_Y = COUNT_FLIP[pos >> 3];

			n_flips = -COUNT_FLIP[pos & 7][(~P >> (pos & 0x38)) & 0xFF];	// h
			n_flips -= COUNT_FLIP_Y[(t >> 8) & 0xFF];	// v
			n_flips -= COUNT_FLIP_Y[(t >> 16) & 0xFF];	// d
			n_flips -= COUNT_FLIP_Y[t >> 24];	// d
				// last square for O if O can move or score <= 0
			score += n_flips - (int)((n_flips | (score - 1)) < 0) * 2;
		} else	score += 2;	// min flip

	} else {	// if player cannot move, low cut-off will occur whether opponent can move.
		const uint8_t *COUNT_FLIP_Y = COUNT_FLIP[pos >> 3];

		// n_flips = last_flip(pos, P);
		t = TEST_EPI8_MASK32(_mm256_broadcastq_epi64(OP), mask_dvhd[pos].v4);
		n_flips  = COUNT_FLIP[pos & 7][(P >> (pos & 0x38)) & 0xFF];	// h
		n_flips += COUNT_FLIP_Y[t & 0xFF];	// d
		n_flips += COUNT_FLIP_Y[(t >> 16) & 0xFF];	// v
		n_flips += COUNT_FLIP_Y[t >> 24];	// d
		score += n_flips;

		// if n_flips == 0, score <= alpha so lazy low cut-off
	}

	return score;
}

#elif defined(__AVX2__) && defined(SIMULLASTFLIP)
// experimental branchless AVX2 MOVMSK version (slower on icc, par on msvc)
// https://eukaryote.hateblo.jp/entry/2020/05/10/033228
inline int vectorcall board_score_sse_1(__m128i OP, const int alpha, const int pos)
{
	int_fast8_t	p_flip, o_flip;
	unsigned int	tP, tO, h;
	unsigned long long P = _mm_cvtsi128_si64(OP);
	int	score, score2;
	const uint8_t *COUNT_FLIP_X = COUNT_FLIP[pos & 7];
	const uint8_t *COUNT_FLIP_Y = COUNT_FLIP[pos >> 3];

	__m256i M = mask_dvhd[pos].v4;
	__m256i PP = _mm256_broadcastq_epi64(OP);

	(void) alpha;	// no lazy cut-off
	h = (P >> (pos & 0x38)) & 0xFF;
	tP = TEST_EPI8_MASK32(PP, M);			tO = tP ^ TEST1_EPI8_MASK32(M);
	p_flip  = COUNT_FLIP_X[h];			o_flip = -COUNT_FLIP_X[h ^ 0xFF];
	p_flip += COUNT_FLIP_Y[tP & 0xFF];		o_flip -= COUNT_FLIP_Y[tO & 0xFF];
	p_flip += COUNT_FLIP_Y[(tP >> 16) & 0xFF];	o_flip -= COUNT_FLIP_Y[(tO >> 16) & 0xFF];
	p_flip += COUNT_FLIP_Y[tP >> 24];		o_flip -= COUNT_FLIP_Y[tO >> 24];

	score = 2 * bit_count(P) - SCORE_MAX + 2;	// = (bit_count(P) + 1) - (SCORE_MAX - 1 - bit_count(P))
	score2 = score + o_flip - (int)((o_flip | (score - 1)) < 0) * 2;	// last square for O if O can move or score <= 0
	score += p_flip;
	return p_flip ? score : score2;	// gcc/icc inserts branch here, since score2 may be wholly skipped.
}

#else
// COUNT_LAST_FLIP_SSE - reasonably fast on all platforms (2.61s on skylake, 2.16s on Zen4)
inline int vectorcall board_score_sse_1(__m128i OP, const int alpha, const int pos)
{
	uint_fast8_t	n_flips;
	unsigned int	t;
	int	score, score2;
	const uint8_t *COUNT_FLIP_X = COUNT_FLIP[pos & 7];
	const uint8_t *COUNT_FLIP_Y = COUNT_FLIP[pos >> 3];

	// n_flips = last_flip(pos, P);
  #ifdef AVXLASTFLIP	// no gain
	__m256i M = mask_dvhd[pos].v4;
	__m256i P4 = _mm256_broadcastq_epi64(OP);
	unsigned int h = (_mm_cvtsi128_si64(OP) >> (pos & 0x38)) & 0xFF;

	t = TEST_EPI8_MASK32(P4, M);
	n_flips  = COUNT_FLIP_X[h];
	n_flips += COUNT_FLIP_Y[t & 0xFF];
	t >>= 16;

  #else
	__m128i M0 = mask_dvhd[pos].v2[0];
	__m128i M1 = mask_dvhd[pos].v2[1];
	__m128i P2 = _mm_unpacklo_epi64(OP, OP);
	__m128i II = _mm_sad_epu8(_mm_and_si128(P2, M0), _mm_setzero_si128());

	n_flips  = COUNT_FLIP_X[_mm_extract_epi16(II, 4)];
	n_flips += COUNT_FLIP_X[_mm_cvtsi128_si32(II)];
	t = TEST_EPI8_MASK16(P2, M1);
  #endif
	n_flips += COUNT_FLIP_Y[t >> 8];
	n_flips += COUNT_FLIP_Y[t & 0xFF];

	score = 2 * bit_count_si64(OP) - SCORE_MAX + 2;	// = (bit_count(P) + 1) - (SCORE_MAX - 1 - bit_count(P))
	score += n_flips;

	if (n_flips == 0) {
		score2 = score - 2;	// empty for player
		if (score <= 0)
			score = score2;

		if (score > alpha) {	// lazy cut-off
			// n_flips = last_flip(pos, ~P);
  #ifdef AVXLASTFLIP
			t = TEST1_EPI8_MASK32(_mm256_andnot_si256(P4, M));
			n_flips  = COUNT_FLIP_X[h ^ 0xFF];
			n_flips += COUNT_FLIP_Y[t & 0xFF];
			t >>= 16;
  #else
			II = _mm_sad_epu8(_mm_andnot_si128(P2, M0), _mm_setzero_si128());
			n_flips  = COUNT_FLIP_X[_mm_extract_epi16(II, 4)];
			n_flips += COUNT_FLIP_X[_mm_cvtsi128_si32(II)];
			t = _mm_movemask_epi8(_mm_sub_epi8(_mm_setzero_si128(), _mm_andnot_si128(P2, M1)));
  #endif
			n_flips += COUNT_FLIP_Y[t >> 8];
			n_flips += COUNT_FLIP_Y[t & 0xFF];

			if (n_flips != 0)
				score = score2 - n_flips;
		}
	}

	return score;
}
#endif

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
int board_score_1(const unsigned long long player, const int alpha, const int x)
{
	int score, score2, n_flips;

	score = 2 * bit_count(player) - SCORE_MAX + 2;	// = (bit_count(P) + 1) - (SCORE_MAX - 1 - bit_count(P))

	n_flips = last_flip(x, player);
	score += n_flips;

	if (n_flips == 0) {	// (23%)
		score2 = score - 2;	// empty for opponent
		if (score <= 0)
			score = score2;
		if (score > alpha) {	// lazy cut-off (40%)
			if ((n_flips = last_flip(x, ~player)) != 0)	// (98%)
				score = score2 - n_flips;
		}
	}

	return score;
}
