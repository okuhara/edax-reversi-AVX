/**
 * @file flip_avx_ppseq.c
 *
 * This module deals with flipping discs.
 *
 * For LSB to MSB directions, isolate LS1B can be used to determine
 * contiguous opponent discs.
 * For MSB to LSB directions, sequencial search with parallel prefix
 * is used.
 *
 * @date 1998 - 2020
 * @author Toshihiko Okuhara
 * @version 4.4
 */

#include "bit.h"

const V4DI lmask_v4[66] = {
	{{ 0x00000000000000fe, 0x0101010101010100, 0x8040201008040200, 0x0000000000000000 }},
	{{ 0x00000000000000fc, 0x0202020202020200, 0x0080402010080400, 0x0000000000000100 }},
	{{ 0x00000000000000f8, 0x0404040404040400, 0x0000804020100800, 0x0000000000010200 }},
	{{ 0x00000000000000f0, 0x0808080808080800, 0x0000008040201000, 0x0000000001020400 }},
	{{ 0x00000000000000e0, 0x1010101010101000, 0x0000000080402000, 0x0000000102040800 }},
	{{ 0x00000000000000c0, 0x2020202020202000, 0x0000000000804000, 0x0000010204081000 }},
	{{ 0x0000000000000080, 0x4040404040404000, 0x0000000000008000, 0x0001020408102000 }},
	{{ 0x0000000000000000, 0x8080808080808000, 0x0000000000000000, 0x0102040810204000 }},
	{{ 0x000000000000fe00, 0x0101010101010000, 0x4020100804020000, 0x0000000000000000 }},
	{{ 0x000000000000fc00, 0x0202020202020000, 0x8040201008040000, 0x0000000000010000 }},
	{{ 0x000000000000f800, 0x0404040404040000, 0x0080402010080000, 0x0000000001020000 }},
	{{ 0x000000000000f000, 0x0808080808080000, 0x0000804020100000, 0x0000000102040000 }},
	{{ 0x000000000000e000, 0x1010101010100000, 0x0000008040200000, 0x0000010204080000 }},
	{{ 0x000000000000c000, 0x2020202020200000, 0x0000000080400000, 0x0001020408100000 }},
	{{ 0x0000000000008000, 0x4040404040400000, 0x0000000000800000, 0x0102040810200000 }},
	{{ 0x0000000000000000, 0x8080808080800000, 0x0000000000000000, 0x0204081020400000 }},
	{{ 0x0000000000fe0000, 0x0101010101000000, 0x2010080402000000, 0x0000000000000000 }},
	{{ 0x0000000000fc0000, 0x0202020202000000, 0x4020100804000000, 0x0000000001000000 }},
	{{ 0x0000000000f80000, 0x0404040404000000, 0x8040201008000000, 0x0000000102000000 }},
	{{ 0x0000000000f00000, 0x0808080808000000, 0x0080402010000000, 0x0000010204000000 }},
	{{ 0x0000000000e00000, 0x1010101010000000, 0x0000804020000000, 0x0001020408000000 }},
	{{ 0x0000000000c00000, 0x2020202020000000, 0x0000008040000000, 0x0102040810000000 }},
	{{ 0x0000000000800000, 0x4040404040000000, 0x0000000080000000, 0x0204081020000000 }},
	{{ 0x0000000000000000, 0x8080808080000000, 0x0000000000000000, 0x0408102040000000 }},
	{{ 0x00000000fe000000, 0x0101010100000000, 0x1008040200000000, 0x0000000000000000 }},
	{{ 0x00000000fc000000, 0x0202020200000000, 0x2010080400000000, 0x0000000100000000 }},
	{{ 0x00000000f8000000, 0x0404040400000000, 0x4020100800000000, 0x0000010200000000 }},
	{{ 0x00000000f0000000, 0x0808080800000000, 0x8040201000000000, 0x0001020400000000 }},
	{{ 0x00000000e0000000, 0x1010101000000000, 0x0080402000000000, 0x0102040800000000 }},
	{{ 0x00000000c0000000, 0x2020202000000000, 0x0000804000000000, 0x0204081000000000 }},
	{{ 0x0000000080000000, 0x4040404000000000, 0x0000008000000000, 0x0408102000000000 }},
	{{ 0x0000000000000000, 0x8080808000000000, 0x0000000000000000, 0x0810204000000000 }},
	{{ 0x000000fe00000000, 0x0101010000000000, 0x0804020000000000, 0x0000000000000000 }},
	{{ 0x000000fc00000000, 0x0202020000000000, 0x1008040000000000, 0x0000010000000000 }},
	{{ 0x000000f800000000, 0x0404040000000000, 0x2010080000000000, 0x0001020000000000 }},
	{{ 0x000000f000000000, 0x0808080000000000, 0x4020100000000000, 0x0102040000000000 }},
	{{ 0x000000e000000000, 0x1010100000000000, 0x8040200000000000, 0x0204080000000000 }},
	{{ 0x000000c000000000, 0x2020200000000000, 0x0080400000000000, 0x0408100000000000 }},
	{{ 0x0000008000000000, 0x4040400000000000, 0x0000800000000000, 0x0810200000000000 }},
	{{ 0x0000000000000000, 0x8080800000000000, 0x0000000000000000, 0x1020400000000000 }},
	{{ 0x0000fe0000000000, 0x0101000000000000, 0x0402000000000000, 0x0000000000000000 }},
	{{ 0x0000fc0000000000, 0x0202000000000000, 0x0804000000000000, 0x0001000000000000 }},
	{{ 0x0000f80000000000, 0x0404000000000000, 0x1008000000000000, 0x0102000000000000 }},
	{{ 0x0000f00000000000, 0x0808000000000000, 0x2010000000000000, 0x0204000000000000 }},
	{{ 0x0000e00000000000, 0x1010000000000000, 0x4020000000000000, 0x0408000000000000 }},
	{{ 0x0000c00000000000, 0x2020000000000000, 0x8040000000000000, 0x0810000000000000 }},
	{{ 0x0000800000000000, 0x4040000000000000, 0x0080000000000000, 0x1020000000000000 }},
	{{ 0x0000000000000000, 0x8080000000000000, 0x0000000000000000, 0x2040000000000000 }},
	{{ 0x00fe000000000000, 0x0100000000000000, 0x0200000000000000, 0x0000000000000000 }},
	{{ 0x00fc000000000000, 0x0200000000000000, 0x0400000000000000, 0x0100000000000000 }},
	{{ 0x00f8000000000000, 0x0400000000000000, 0x0800000000000000, 0x0200000000000000 }},
	{{ 0x00f0000000000000, 0x0800000000000000, 0x1000000000000000, 0x0400000000000000 }},
	{{ 0x00e0000000000000, 0x1000000000000000, 0x2000000000000000, 0x0800000000000000 }},
	{{ 0x00c0000000000000, 0x2000000000000000, 0x4000000000000000, 0x1000000000000000 }},
	{{ 0x0080000000000000, 0x4000000000000000, 0x8000000000000000, 0x2000000000000000 }},
	{{ 0x0000000000000000, 0x8000000000000000, 0x0000000000000000, 0x4000000000000000 }},
	{{ 0xfe00000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000 }},
	{{ 0xfc00000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000 }},
	{{ 0xf800000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000 }},
	{{ 0xf000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000 }},
	{{ 0xe000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000 }},
	{{ 0xc000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000 }},
	{{ 0x8000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000 }},
	{{ 0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000 }},
	{{ 0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000 }},	// pass
	{{ 0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000 }}
};

/**
 * Compute flipped discs when playing on square pos.
 *
 * @param pos player's move.
 * @param P player's disc pattern.
 * @param O opponent's disc pattern.
 * @return flipped disc pattern.
 */

__m256i vectorcall mm_Flip(const __m128i OP, int pos)
{
	__m256i	PP, mOO, flip, shift2, pre, outflank, mask, ocontig;
	const __m256i shift1897 = _mm256_set_epi64x(7, 9, 8, 1);

	PP = _mm256_broadcastq_epi64(OP);
	mOO = _mm256_and_si256(_mm256_broadcastq_epi64(_mm_unpackhi_epi64(OP, OP)),
		_mm256_set_epi64x(0x007e7e7e7e7e7e00, 0x007e7e7e7e7e7e00, 0x00ffffffffffff00, 0x7e7e7e7e7e7e7e7e));	// (sentinel on the edge)

	ocontig = _mm256_set1_epi64x(X_TO_BIT[pos]);
	ocontig = _mm256_and_si256(mOO, _mm256_srlv_epi64(ocontig, shift1897));
	ocontig = _mm256_or_si256(ocontig, _mm256_and_si256(mOO, _mm256_srlv_epi64(ocontig, shift1897)));
	pre = _mm256_and_si256(mOO, _mm256_srlv_epi64(mOO, shift1897));	// parallel prefix
	shift2 = _mm256_add_epi64(shift1897, shift1897);
	ocontig = _mm256_or_si256(ocontig, _mm256_and_si256(pre, _mm256_srlv_epi64(ocontig, shift2)));
	ocontig = _mm256_or_si256(ocontig, _mm256_and_si256(pre, _mm256_srlv_epi64(ocontig, shift2)));
	outflank = _mm256_and_si256(_mm256_srlv_epi64(ocontig, shift1897), PP);
	flip = _mm256_andnot_si256(_mm256_cmpeq_epi64(outflank, _mm256_setzero_si256()), ocontig);

	mask = lmask_v4[pos].v4;
		// look for non-opponent (or edge) bit
	ocontig = _mm256_andnot_si256(mOO, mask);
	ocontig = _mm256_and_si256(ocontig, _mm256_sub_epi64(_mm256_setzero_si256(), ocontig));	// LS1B
	outflank = _mm256_and_si256(ocontig, PP);
		// set all bits lower than outflank (depends on ocontig != 0)
	outflank = _mm256_add_epi64(outflank, _mm256_cmpeq_epi64(outflank, ocontig));
	flip = _mm256_or_si256(flip, _mm256_and_si256(outflank, mask));

	return flip;
}

