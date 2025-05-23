/**
 * @file count_last_flip_sve_lzcnt.c
 *
 * A function is provided to count the number of fipped disc of the last move.
 *
 * Count last flip using the flip_sve_lzcnt way.
 * For optimization purpose, the value returned is twice the number of flipped
 * disc, to facilitate the computation of disc difference.
 *
 * @date 2024 - 25
 * @author Toshihiko Okuhara
 * @version 4.5
 * 
 */

#include <arm_sve.h>

/** precomputed count flip array */
extern const unsigned char COUNT_FLIP[8][256];
extern const uint64_t lrmask[66][8];	// in flip_sve_lzcnt.c

/**
 * Count last flipped discs when playing on the last empty.
 *
 * @param pos the last empty square.
 * @param P player's disc pattern.
 * @return flipped disc count.
 */

#ifndef __ARM_FEATURE_SVE2
	// equivalent only if no intersection between masks
#define svbsl_u64(op1,op2,op3)	svorr_u64_m(pg, (op2), svand_u64_x(pg, (op3), (op1)))
#define svbsl1n_u64(op1,op2,op3)	svorr_u64_m(pg, (op2), svbic_u64_x(pg, (op3), (op1)))
#endif

static int last_flip_sve(int pos, svuint64_t PP)
{
	svuint64_t	p_flip, p_oflank, p_eraser, p_cap, mask;
	svbool_t	pg;
	const uint64_t	(*pmask)[8];

	pmask = &lrmask[pos];
	pg = svwhilelt_b64(0, 4);

	mask = svld1_u64(pg, *pmask + 0);	// right: clear all bits lower than outflank
	p_oflank = svand_x(pg, mask, PP);
	p_oflank = svand_x(pg, svclz_z(pg, p_oflank), 63);
	p_eraser = svlsr_x(pg, svdup_u64(-1), p_oflank);
	p_flip = svbic_x(pg, mask, p_eraser);

	mask = svld1_u64(pg, *pmask + 4);	// left: look for player LS1B
	p_oflank = svand_x(pg, mask, PP);
		// set all bits lower than oflank, using satulation if oflank = 0
	p_cap = svbic_x(pg, svqsub(p_oflank, 1), p_oflank);
	p_flip = svbsl_u64(p_cap, p_flip, mask);

	if (svcntd() == 2) {	// sve128 only
		mask = svld1_u64(pg, *pmask + 2);	// right: set all bits higher than outflank
		p_oflank = svand_x(pg, mask, PP);
		p_oflank = svand_x(pg, svclz_z(pg, p_oflank), 63);
		p_eraser = svlsr_x(pg, svdup_u64(-1), p_oflank);
		p_flip = svbsl1n_u64(p_eraser, p_flip, mask);

		mask = svld1_u64(pg, *pmask + 6);	// left: look for player LS1B
		p_oflank = svand_x(pg, mask, PP);
			// set all bits lower than oflank, using satulation if oflank = 0
		p_cap = svbic_x(pg, svqsub(p_oflank, 1), p_oflank);
		p_flip = svbsl_u64(p_cap, p_flip, mask);
	}

	return svaddv_u64(pg, svcnt_u64_x(pg, p_flip)) * 2;
}

int last_flip(int pos, unsigned long long P)
{
	return last_flip_sve(pos, svdup_u64(P));
}

/**
 * @brief Get the final score.
 *
 * Get the final score, when 1 empty square remain.
 * The following code has been adapted from Zebra by Gunnar Anderson.
 *
 * @param P      Board.player to evaluate.
 * @param alpha  Alpha bound. (beta - 1)
 * @param x      Last empty square to play.
 * @return       The final score, as a disc difference.
 */
#ifdef LASTFLIP_LOWCUT
int solve_1(unsigned long long P, int alpha, int x)
{
	int score, score2, n_flips;
	svuint64_t PP = svdup_u64(P);
	svbool_t lane0 = svptrue_pat_b64(SV_VL1);

	score = 2 * svlastb_u64(lane0, svcnt_u64_x(lane0, PP)) - 64 + 2;	// = (bit_count(P) + 1) - (SCORE_MAX - 1 - bit_count(P))

	n_flips = last_flip_sve(x, PP);
	score += n_flips;

	if (n_flips == 0) {	// (23%)
		score2 = score - 2;	// empty for opponent
		if (score <= 0)
			score = score2;
		if (score > alpha) {	// lazy cut-off (40%)
			if ((n_flips = last_flip_sve(x, svnot_x(svptrue_b64(), PP))) != 0)	// (98%)
				score = score2 - n_flips;
		}
	}

	return score;
}

#else
int solve_exact_1(unsigned long long P, int pos)
{
	int	score;
	svuint64_t	PP, p_flip, o_flip, p_oflank, o_oflank, p_eraser, o_eraser, p_cap, o_cap, mask, po_flip, po_score;
	svbool_t	pg, po_nopass;
	const uint64_t	(*pmask)[8];

	PP = svdup_u64(P);
	pmask = &lrmask[pos];
	pg = svwhilelt_b64(0, 4);

	mask = svld1_u64(pg, *pmask + 0);	// right: clear all bits lower than outflank
	p_oflank = svand_x(pg, mask, PP);			o_oflank = svbic_x(pg, mask, PP);
	p_oflank = svand_x(pg, svclz_z(pg, p_oflank), 63);	o_oflank = svand_x(pg, svclz_z(pg, o_oflank), 63);
	p_eraser = svlsr_x(pg, svdup_u64(-1), p_oflank);	o_eraser = svlsr_x(pg, svdup_u64(-1), o_oflank);
	p_flip = svbic_x(pg, mask, p_eraser);			o_flip = svbic_x(pg, mask, o_eraser);

	mask = svld1_u64(pg, *pmask + 4);	// left: look for player LS1B
	p_oflank = svand_x(pg, mask, PP);			o_oflank = svbic_x(pg, mask, PP);
		// set all bits lower than oflank, using satulation if oflank = 0
	p_cap = svbic_x(pg, svqsub(p_oflank, 1), p_oflank);	o_cap = svbic_x(pg, svqsub(o_oflank, 1), o_oflank);
	p_flip = svbsl_u64(p_cap, p_flip, mask);		o_flip = svbsl_u64(o_cap, o_flip, mask);

	if (svcntd() == 2) {	// sve128 only
		mask = svld1_u64(pg, *pmask + 2);	// right: set all bits higher than outflank
		p_oflank = svand_x(pg, mask, PP);			o_oflank = svbic_x(pg, mask, PP);
		p_oflank = svand_x(pg, svclz_z(pg, p_oflank), 63);	o_oflank = svand_x(pg, svclz_z(pg, o_oflank), 63);
		p_eraser = svlsr_x(pg, svdup_u64(-1), p_oflank);	o_eraser = svlsr_x(pg, svdup_u64(-1), o_oflank);
		p_flip = svbsl1n_u64(p_eraser, p_flip, mask);		o_flip = svbsl1n_u64(o_eraser, o_flip, mask);

		mask = svld1_u64(pg, *pmask + 6);	// left: look for player LS1B
		p_oflank = svand_x(pg, mask, PP);			o_oflank = svbic_x(pg, mask, PP);
			// set all bits lower than oflank, using satulation if oflank = 0
		p_cap = svbic_x(pg, svqsub(p_oflank, 1), p_oflank);	o_cap = svbic_x(pg, svqsub(o_oflank, 1), o_oflank);
		p_flip = svbsl_u64(p_cap, p_flip, mask);		o_flip = svbsl_u64(o_cap, o_flip, mask);
	}

	po_flip = svtrn1_u64(svdup_u64(svorv_u64(pg, o_flip)), svdup_u64(svorv_u64(pg, p_flip)));
	po_nopass = svcmpne_n_u64(pg, po_flip, 0);
	po_score = svcnt_u64_x(pg, sveor_u64_x(pg, po_flip, PP));
		// last square for P if P nopass or score >= 32
	po_score = svadd_n_u64_m(svorr_b_z(svdupq_b64(false, true), po_nopass, svcmpge_n_u64(pg, po_score, 32)), po_score, 1);
	score = svlastb_u64(po_nopass, po_score);	// use o_score if P pass and O nopass
	return score * 2 - 64;	// = bit_count(P) - (SCORE_MAX - bit_count(P))
}
#endif
