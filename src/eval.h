/**
 * @file eval.h
 *
 * Evaluation function's header.
 *
 * @date 1998 - 2023
 * @author Richard Delorme
 * @version 4.4
 */

#ifndef EDAX_EVAL_H
#define EDAX_EVAL_H

#include "bit.h"

/** number of features */
enum { EVAL_N_FEATURE = 47 };

/**
 * struct Eval
 * @brief evaluation function
 */
typedef union {
	unsigned short us[48];
	unsigned long long ull[12];	// SWAR
#ifdef __ARM_NEON__
	int16x8_t v8[6];
#elif defined(hasSSE2) || defined(USE_MSVC_X86)
	__m128i	v8[6];
#endif
#ifdef __AVX2__
	__m256i	v16[3];
#endif
}
#if defined(__GNUC__) && !defined(hasSSE2)
__attribute__ ((aligned (16)))
#endif
EVAL_FEATURE_V;

typedef struct Eval {
	EVAL_FEATURE_V feature;                       /**!< discs' features */
	int n_empties;                                /**< number of empty squares */
	unsigned int parity;                          /**< parity */
} Eval;

struct Board;
struct Move;

/** number of (unpacked) weights */
enum { EVAL_N_WEIGHT = 226315 };

/** number of plies */
enum { EVAL_N_PLY = 61 };

extern short (*EVAL_WEIGHT)[EVAL_N_PLY][EVAL_N_WEIGHT];

#ifndef SELECTIVE_EVAL_UPDATE

extern const EVAL_FEATURE_V EVAL_FEATURE[65];
extern const EVAL_FEATURE_V EVAL_FEATURE_all_opponent;

#endif

/* function declaration */
void eval_open(const char*);
void eval_close(void);
// void eval_init(Eval*);
// void eval_free(Eval*);
void eval_set(Eval*, const struct Board*);
void eval_update(int, unsigned long long, Eval*);
void eval_update_leaf(int, unsigned long long, Eval*, const Eval*);
void eval_restore(Eval*, const struct Move*);
void eval_pass(Eval*);
double eval_sigma(const int, const int, const int);

#endif

