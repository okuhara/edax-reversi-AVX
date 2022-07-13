/**
 * @file board.h
 *
 * Board management header file.
 *
 * @date 1998 - 2022
 * @author Richard Delorme
 * @version 4.5
 */

#ifndef EDAX_BOARD_H
#define EDAX_BOARD_H

#include "const.h"
#include "settings.h"
#include "bit.h"

#include <stdio.h>
#include <stdbool.h>

/** Board : board representation */
typedef struct Board {
	unsigned long long player, opponent;     /**< bitboard representation */
} Board;

struct Move;
struct Random;

/* function declarations */
void board_init(Board*);
int board_set(Board*, const char*);
int board_from_FEN(Board*, const char*);
bool board_lesser(const Board*, const Board*);
bool board_equal(const Board*, const Board*);
void board_symetry(const Board*, const int, Board*);
int board_unique(const Board*, Board*);
void board_check(const Board*);
void board_rand(Board*, int, struct Random*);

int board_count_last_flips(const Board*, const int);
unsigned long long board_get_move(const Board*, const int, struct Move*);
bool board_check_move(const Board*, struct Move*);
void board_swap_players(Board*);
void board_update(Board*, const struct Move*);
void board_restore(Board*, const struct Move*);
void board_pass(Board*);
unsigned long long board_next(const Board*, const int, Board*);
unsigned long long board_get_hash_code(const Board*);
int board_get_square_color(const Board*, const int);
bool board_is_occupied(const Board*, const int);
void board_print(const Board*, const int, FILE*);
char* board_to_string(const Board*, const int, char *);
void board_print_FEN(const Board*, const int, FILE*);
char* board_to_FEN(const Board*, const int, char*);
bool board_is_pass(const Board*);
bool board_is_game_over(const Board*);
int board_count_empties(const Board *board);

unsigned long long get_moves(const unsigned long long, const unsigned long long);
bool can_move(const unsigned long long, const unsigned long long);
unsigned long long get_moves_6x6(const unsigned long long, const unsigned long long);
bool can_move_6x6(const unsigned long long, const unsigned long long);
int get_mobility(const unsigned long long, const unsigned long long);
int get_weighted_mobility(const unsigned long long, const unsigned long long);
int get_potential_mobility(const unsigned long long, const unsigned long long);
void edge_stability_init(void);
unsigned long long get_stable_edge(const unsigned long long, const unsigned long long);
void get_all_full_lines(const unsigned long long, unsigned long long [5]);
int get_stability(const unsigned long long, const unsigned long long);
int get_stability_fulls(const unsigned long long, const unsigned long long, unsigned long long [5]);
int get_edge_stability(const unsigned long long, const unsigned long long);
int get_corner_stability(const unsigned long long);

#if defined(USE_GAS_MMX) || defined(USE_MSVC_X86)
void init_mmx (void);
unsigned long long get_moves_mmx(const unsigned long long, const unsigned long long);
unsigned long long get_moves_sse(const unsigned long long, const unsigned long long);
int get_potential_mobility_mmx(unsigned long long, unsigned long long);

#elif defined(ANDROID) && !defined(hasNeon) && !defined(hasSSE2)
void init_neon (void);
unsigned long long get_moves_sse(unsigned long long, unsigned long long);
#endif

extern unsigned char edge_stability[256 * 256];

#if 0 // defined(__BMI2__) && !defined(__bdver4__) && !defined(__znver1__) && !defined(__znver2__) // pdep is slow on AMD before Zen3
#define	unpackA1A8(x)	_pdep_u64((x), 0x0101010101010101)
#define	unpackH1H8(x)	_pdep_u64((x), 0x8080808080808080)
#else
#define	unpackA1A8(x)	(((unsigned long long)((((x) >> 4) * 0x00204081) & 0x01010101) << 32) | ((((x) & 0x0f) * 0x00204081) & 0x01010101))
#define	unpackH1H8(x)	(((unsigned long long)((((x) >> 4) * 0x10204080) & 0x80808080) << 32) | ((((x) & 0x0f) * 0x10204080) & 0x80808080))
#endif

#if (LAST_FLIP_COUNTER == COUNT_LAST_FLIP_PLAIN) || (LAST_FLIP_COUNTER == COUNT_LAST_FLIP_SSE) || (LAST_FLIP_COUNTER == COUNT_LAST_FLIP_BMI2)
	extern int last_flip(int pos, unsigned long long P);
#else
	extern int (*count_last_flip[BOARD_SIZE + 1])(const unsigned long long);
	#define	last_flip(x,P)	count_last_flip[x](P)
#endif

#if (MOVE_GENERATOR == MOVE_GENERATOR_AVX) || (MOVE_GENERATOR == MOVE_GENERATOR_AVX512)
	extern __m128i vectorcall mm_Flip(const __m128i OP, int pos);
  #ifdef HAS_CPU_64
	#define	Flip(x,P,O)	((unsigned long long) _mm_cvtsi128_si64(mm_Flip(_mm_insert_epi64(_mm_cvtsi64_si128(P), (O), 1), (x))))
  #else
	#define	Flip(x,P,O)	((unsigned long long) _mm_cvtsi128_si64(mm_Flip(_mm_insert_epi32(_mm_insert_epi32(_mm_insert_epi32(\
		_mm_cvtsi32_si128(P), ((P) >> 32), 1), (O), 2), (O >> 32), 3), (x))))
  #endif
	#define	board_flip(board,x)	((unsigned long long) _mm_cvtsi128_si64(mm_Flip(_mm_loadu_si128((__m128i *) (board)), (x))))

#elif MOVE_GENERATOR == MOVE_GENERATOR_SSE
	extern __m128i (vectorcall *mm_flip[BOARD_SIZE + 2])(const __m128i);
	#define	Flip(x,P,O)	((unsigned long long) _mm_cvtsi128_si64(mm_flip[x](_mm_unpacklo_epi64(_mm_cvtsi64_si128(P), _mm_cvtsi64_si128(O)))))
	#define mm_Flip(OP,x)	mm_flip[x](OP)
	#define	board_flip(board,x)	((unsigned long long) _mm_cvtsi128_si64(mm_flip[x](_mm_loadu_si128((__m128i *) (board)))))

#elif MOVE_GENERATOR == MOVE_GENERATOR_NEON
	extern uint64x2_t mm_Flip(uint64x2_t OP, int pos);
	#define	Flip(x,P,O)	vgetq_lane_u64(mm_Flip(vcombine_u64(vcreate_u64(P), vcreate_u64(O)), (x)), 0)
	#define	board_flip(board,x)	vgetq_lane_u64(mm_Flip(vld1q_u64((uint64_t *) (board)), (x)), 0)

#elif MOVE_GENERATOR == MOVE_GENERATOR_32
	extern unsigned long long (*flip[BOARD_SIZE + 2])(unsigned int, unsigned int, unsigned int, unsigned int);
	#define Flip(x,P,O)	flip[x]((unsigned int)(P), (unsigned int)((P) >> 32), (unsigned int)(O), (unsigned int)((O) >> 32))
  #ifdef __BIG_ENDIAN__
	#define	board_flip(board,x)	flip[x]((unsigned int)((board)->player), ((unsigned int *) &(board)->player)[0], (unsigned int)((board)->opponent), ((unsigned int *) &(board)->opponent)[0])
  #else
	#define	board_flip(board,x)	flip[x]((unsigned int)((board)->player), ((unsigned int *) &(board)->player)[1], (unsigned int)((board)->opponent), ((unsigned int *) &(board)->opponent)[1])
  #endif
	#if defined(USE_GAS_MMX) && !defined(hasSSE2)
		extern void init_flip_sse(void);
	#endif

#else
	#if MOVE_GENERATOR == MOVE_GENERATOR_SSE_BSWAP
		extern unsigned long long Flip(int, unsigned long long, unsigned long long);
	#else
		extern unsigned long long (*flip[BOARD_SIZE + 2])(const unsigned long long, const unsigned long long);
		#define	Flip(x,P,O)	flip[x]((P), (O))
	#endif

	#define	board_flip(board,x)	Flip((x), (board)->player, (board)->opponent)
#endif

#endif
