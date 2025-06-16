/**
 * @file settings.h
 *
 * Various macro / constants to control algorithm usage.
 *
 * @date 1998 - 2024
 * @author Richard Delorme
 * @version 4.5
 */


#ifndef EDAX_SETTINGS_H
#define EDAX_SETTINGS_H

#include <stdbool.h>

#define MOVE_GENERATOR_KINDERGARTEN 1	// 31.1Mnps
#define MOVE_GENERATOR_32 2		// 31.3Mnps	// best for 32bit X86
#define MOVE_GENERATOR_ROXANE 3		// 29.0Mnps
#define MOVE_GENERATOR_CARRY 4		// 32.6Mnps
#define MOVE_GENERATOR_BITSCAN 5	// 32.7Mnps	// 40.7Mnps@armv8
#define MOVE_GENERATOR_SSE 6		// 34.4Mnps	// best for generic X64
#define MOVE_GENERATOR_SSE_ACEPCK 7
#define MOVE_GENERATOR_AVX 8		// 34.7Mnps	// best for modern X64
#define MOVE_GENERATOR_AVX512 9
#define MOVE_GENERATOR_NEON 10				// 31.0Mnps@armv8
#define MOVE_GENERATOR_SVE 11

#define COUNT_LAST_FLIP_KINDERGARTEN 1	// SIMULLASTFLIP	// 33.5Mnps
#define COUNT_LAST_FLIP_32 2		// SIMULLASTFLIP	// 33.1Mnps
#define COUNT_LAST_FLIP_CARRY 3		// LASTFLIP_LOWCUT	// 33.8Mnps
#define COUNT_LAST_FLIP_BITSCAN 4	// LASTFLIP_LOWCUT	// 33.9Mnps	// 36.7Mnps@armv8
#define COUNT_LAST_FLIP_BMI 5		// LASTFLIP_LOWCUT
#define COUNT_LAST_FLIP_PLAIN 6		// SIMULLASTFLIP	// 33.3Mnps
#define COUNT_LAST_FLIP_BMI2 7		// SIMULLASTFLIP	// 34.7Mnps	// slow on AMD
#define COUNT_LAST_FLIP_SSE 8		// SIMULLASTFLIP [LASTFLIP_HIGHCUT] [LASTFLIP_LOWCUT] [AVX512_PREFER512]	// 34.7Mnps
#define COUNT_LAST_FLIP_AVX_PPFILL 9	// LASTFLIP_LOWCUT [LASTFLIP_HIGHCUT]
#define COUNT_LAST_FLIP_AVX512 10	// SIMULLASTFLIP [LASTFLIP_HIGHCUT] [LASTFLIP_LOWCUT] [AVX512_PREFER512]
#define COUNT_LAST_FLIP_NEON 11		// SIMULLASTFLIP [LASTFLIP_LOWCUT]	// 31.0Mnps@armv8
#define COUNT_LAST_FLIP_SVE 12		// SIMULLASTFLIP [LASTFLIP_LOWCUT]

/**move generation. */
#ifndef MOVE_GENERATOR
  #if defined(__AVX512VL__) || defined(__AVX10_1__)
	#define MOVE_GENERATOR MOVE_GENERATOR_AVX512
  #elif defined(__AVX2__)
	#define MOVE_GENERATOR MOVE_GENERATOR_AVX
  #elif defined(__SSE2__) || defined(_M_X64) || defined(hasSSE2)
	#define MOVE_GENERATOR MOVE_GENERATOR_SSE
  #elif defined(__ARM_FEATURE_SVE) && (__ARM_FEATURE_SVE_BITS > 128)
	#define MOVE_GENERATOR MOVE_GENERATOR_SVE
  #elif defined(__aarch64__) || defined(_M_ARM64)
	#define MOVE_GENERATOR MOVE_GENERATOR_BITSCAN
  #else
	#define MOVE_GENERATOR MOVE_GENERATOR_32
  #endif
#endif
#ifndef COUNT_LAST_FLIP
  #if defined(__AVX512VL__) || defined(__AVX10_1__) 
    #if defined(LASTFLIP_HIGHCUT) || defined(LASTFLIP_LOWCUT)
	#define COUNT_LAST_FLIP COUNT_LAST_FLIP_AVX512
    #else
	#define COUNT_LAST_FLIP COUNT_LAST_FLIP_BMI2
    #endif
  #elif defined(__SSE2__) || defined(_M_X64) || defined(hasSSE2)
	#define COUNT_LAST_FLIP COUNT_LAST_FLIP_SSE
  #elif defined(__aarch64__) || defined(_M_ARM64)
	#define COUNT_LAST_FLIP COUNT_LAST_FLIP_BITSCAN
  #else
	#define COUNT_LAST_FLIP COUNT_LAST_FLIP_32
  #endif
#endif

/** transposition cutoff usage. */
#define USE_TC true

/** stability cutoff usage. */
#define USE_SC true

/** enhanced transposition cutoff usage. */
#define USE_ETC true

/** probcut usage. */
#define USE_PROBCUT true

/** Use recursive probcut */
#define USE_RECURSIVE_PROBCUT true

/** limit recursive probcut level */
#define LIMIT_RECURSIVE_PROBCUT(x) x

/** kogge-stone parallel prefix algorithm usage.
 *  0 -> none, 1 -> move generator, 2 -> stability, 3 -> both.
 */
#define KOGGE_STONE 2

/** 1 stage parallel prefix algorithm usage.
 *  0 -> none, 1 -> move generator, 2 -> stability, 3 -> both.
 */
#define PARALLEL_PREFIX 1

#if (KOGGE_STONE & PARALLEL_PREFIX)
	#error "usage of 2 incompatible algorithms"
#endif

/** Internal Iterative Deepening. */
#define USE_IID false

/** Use previous search result */
#define USE_PREVIOUS_SEARCH true

/** Allow type puning */
#ifndef USE_TYPE_PUNING
// #ifndef ANDROID
#define USE_TYPE_PUNING 1
// #endif
#endif

/** Hash-n-way. */
#define HASH_N_WAY 4

/** hash align */
#define HASH_ALIGNED 1

/** Thread local hash size per thread (in number of bits) */
#define THREAD_LOCAL_HASH_SIZE 11

/** PV extension (solve PV alone sooner) */
#define USE_PV_EXTENSION true

/** Swith from endgame to shallow search (faster but less node efficient) at this depth. */
#define DEPTH_TO_SHALLOW_SEARCH 6

/** Use lock-free thread local hash (before DEPTH_TO_SHALLOW_SEARCH) */
#define DEPTH_TO_USE_LOCAL_HASH 9

/** Switch from midgame to endgame search (faster but less node efficient) at this depth. */
#define DEPTH_MIDGAME_TO_ENDGAME 15

/** Switch from midgame result (evaluated score) to endgame result (exact score) at this number of empties. */
#define ITERATIVE_MIN_EMPTIES 10

/** Store bestmoves in the pv_hash up to this height. */
#define PV_HASH_HEIGHT 5

/** Try ETC down to this depth. */
#define ETC_MIN_DEPTH 5

/** bound for usefull move sorting */
#define SORT_ALPHA_DELTA 8

/** Try Node splitting (for parallel search) down to that depth. */
#define SPLIT_MIN_DEPTH 5

/** Stop Node splitting (for parallel search) when few move remains.  */
#define SPLIT_MIN_MOVES_TODO 1

/** Stop Node splitting (for parallel search) after a few splitting.  */
#define SPLIT_MAX_SLAVES 3

/** Branching factor (to adjust alloted time). */
#define BRANCHING_FACTOR 2.24

/** Parallelisable work. */
#define SMP_W 49.0

/** Critical time. */
#define SMP_C 1.0

/** Fast perft */
#define  FAST_PERFT true

/** multi_pv depth */
#define MULTIPV_DEPTH 10

#endif /* EDAX_SETTINGS_H */

