/**
 * @file board.h
 *
 * Board management header file.
 *
 * @date 1998 - 2023
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
void board_symetry(const Board*, const int, Board*);
int board_unique(const Board*, Board*);
void board_check(const Board*);
void board_rand(Board*, int, struct Random*);

// Compare two board for equality
#ifdef __AVX2__
inline bool board_equal(const Board *b1, const Board *b2)
{
	__m128i b = _mm_xor_si128(*(__m128i *) b1, *(__m128i *) b2);
	return _mm_testz_si128(b, b);
}
#else
#define	board_equal(b1,b2)	((b1)->player == (b2)->player && (b1)->opponent == (b2)->opponent)
#endif

int board_count_last_flips(const Board*, const int);
unsigned long long board_get_move_flip(const Board*, const int, struct Move*);
bool board_check_move(const Board*, struct Move*);
void board_swap_players(Board*);
void board_update(Board*, const struct Move*);
void board_restore(Board*, const struct Move*);
void board_pass(Board*);
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

bool can_move(const unsigned long long, const unsigned long long);
unsigned long long get_moves_6x6(const unsigned long long, const unsigned long long);
bool can_move_6x6(const unsigned long long, const unsigned long long);
int get_mobility(const unsigned long long, const unsigned long long);
int get_weighted_mobility(const unsigned long long, const unsigned long long);
int get_potential_mobility(const unsigned long long, const unsigned long long);
void edge_stability_init(void);
unsigned long long get_stable_edge(const unsigned long long, const unsigned long long);
unsigned long long get_all_full_lines(const unsigned long long);
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

#ifdef __AVX2__
__m128i vectorcall get_moves_and_potential(__m256i, __m256i);
#endif

extern unsigned char edge_stability[256 * 256];

// a1/a8/h1/h8 are already stable in horizontal line, so omit them in vertical line to ease kindergarten for CPU_64
#if 0 // defined(__BMI2__) && !defined(__bdver4__) && !defined(__znver1__) && !defined(__znver2__) // pdep is slow on AMD before Zen3
#define	unpackA2A7(x)	_pdep_u64((x), 0x0101010101010101)
#define	unpackH2H7(x)	_pdep_u64((x), 0x8080808080808080)
#elif defined(HAS_CPU_64)
#define	unpackA2A7(x)	((((x) & 0x7e) * 0x0000040810204080ULL) & 0x0001010101010100ULL)
#define	unpackH2H7(x)	((((x) & 0x7e) * 0x0002040810204000ULL) & 0x0080808080808000ULL)
#else
#define	unpackA2A7(x)	(((unsigned long long)((((x) >> 4) * 0x00204081) & 0x01010101) << 32) | ((((x) & 0x0f) * 0x00204081) & 0x01010101))
#define	unpackH2H7(x)	(((unsigned long long)((((x) >> 4) * 0x10204080) & 0x80808080) << 32) | ((((x) & 0x0f) * 0x10204080) & 0x80808080))
#endif

#if (LAST_FLIP_COUNTER == COUNT_LAST_FLIP_CARRY) || (LAST_FLIP_COUNTER == COUNT_LAST_FLIP_KINDERGARTEN) || (LAST_FLIP_COUNTER == COUNT_LAST_FLIP_BITSCAN) || (LAST_FLIP_COUNTER == COUNT_LAST_FLIP_32)
	extern int (*count_last_flip[BOARD_SIZE + 1])(const unsigned long long);
	#define	last_flip(x,P)	count_last_flip[x](P)
#else
	extern int last_flip(int pos, unsigned long long P);
#endif

#if (MOVE_GENERATOR == MOVE_GENERATOR_AVX) || (MOVE_GENERATOR == MOVE_GENERATOR_AVX512)
	extern const V4DI lmask_v4[66], rmask_v4[66];
	extern __m256i vectorcall mm_Flip(const __m128i OP, int pos);
	inline __m128i vectorcall reduce_vflip(__m256i flip4) {
		__m128i flip2 = _mm_or_si128(_mm256_castsi256_si128(flip4), _mm256_extracti128_si256(flip4, 1));
		return _mm_or_si128(flip2, _mm_shuffle_epi32(flip2, 0x4e));	// SWAP64
	}
  #ifdef HAS_CPU_64
	#define	Flip(x,P,O)	((unsigned long long) _mm_cvtsi128_si64(reduce_vflip(mm_Flip(_mm_insert_epi64(_mm_cvtsi64_si128(P), (O), 1), (x)))))
  #else
	#define	Flip(x,P,O)	((unsigned long long) _mm_cvtsi128_si64(reduce_vflip(mm_Flip(_mm_insert_epi32(_mm_insert_epi32(_mm_insert_epi32(\
		_mm_cvtsi32_si128(P), ((P) >> 32), 1), (O), 2), (O >> 32), 3), (x)))))
  #endif
	#define	board_flip(board,x)	((unsigned long long) _mm_cvtsi128_si64(reduce_vflip(mm_Flip(_mm_loadu_si128((__m128i *) (board)), (x)))))
	#define	vboard_flip(board,x)	((unsigned long long) _mm_cvtsi128_si64(reduce_vflip(mm_Flip((board), (x)))))

#elif MOVE_GENERATOR == MOVE_GENERATOR_SSE
	extern __m128i (vectorcall *mm_flip[BOARD_SIZE + 2])(const __m128i);
	#define	Flip(x,P,O)	((unsigned long long) _mm_cvtsi128_si64(mm_flip[x](_mm_unpacklo_epi64(_mm_cvtsi64_si128(P), _mm_cvtsi64_si128(O)))))
	#define mm_Flip(OP,x)	mm_flip[x](OP)
	#define reduce_vflip(x)	(x)
	#define	board_flip(board,x)	((unsigned long long) _mm_cvtsi128_si64(mm_flip[x](_mm_loadu_si128((__m128i *) (board)))))
	#define	vboard_flip(board,x)	((unsigned long long) _mm_cvtsi128_si64(mm_flip[x]((board))))

#elif MOVE_GENERATOR == MOVE_GENERATOR_NEON
	extern uint64x2_t mm_Flip(uint64x2_t OP, int pos);
	#define	Flip(x,P,O)	vgetq_lane_u64(mm_Flip(vcombine_u64(vcreate_u64(P), vcreate_u64(O)), (x)), 0)
	#define	board_flip(board,x)	vgetq_lane_u64(mm_Flip(vld1q_u64((uint64_t *) (board)), (x)), 0)
	#define	vboard_flip(board,x)	vgetq_lane_u64(mm_Flip((board), (x)), 0)

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

// Keep Board backup in a non-volatile vector register if available
#ifdef hasSSE2
	#define	rBoard	__m128i
	#define	load_rboard(board)	_mm_loadu_si128((__m128i *) &(board))
	#define	store_rboard(dst,board)	_mm_storeu_si128((__m128i *) &(dst), (board))
	#define	rboard_update(pboard,board0,move)	_mm_storeu_si128((__m128i *) (pboard), _mm_shuffle_epi32(_mm_xor_si128((board0), _mm_or_si128(_mm_set1_epi64x((move)->flipped), _mm_cvtsi64_si128(X_TO_BIT[(move)->x]))), 0x4e));
#elif defined(hasNeon)
	#define	rBoard	uint64x2_t
	#define	load_rboard(board)	vld1q_u64((uint64_t *) &(board))
	#define	store_rboard(dst,board)	vst1q_u64((uint64_t *) &(dst), (board))
	#define	rboard_update(pboard,board0,move)	board_update((pboard), (move))
#else
	#define	rBoard	Board
	#define	load_rboard(board)	(board)
	#define	store_rboard(dst,board)	((dst) = (board))
	#define	rboard_update(pboard,board0,move)	board_update((pboard), (move))
#endif

// Pass Board in a vector register to Flip
#if (MOVE_GENERATOR == MOVE_GENERATOR_AVX) || (MOVE_GENERATOR == MOVE_GENERATOR_AVX512) || (MOVE_GENERATOR == MOVE_GENERATOR_SSE)
	#define	vBoard	__m128i
	unsigned long long vectorcall vboard_next(__m128i OP, const int x, Board *next);
	#define	board_next(board,x,next)	vboard_next(_mm_loadu_si128((__m128i *) (board)), (x), (next))
	#define	load_vboard(board)	_mm_loadu_si128((__m128i *) &(board))
	#define	store_vboard(dst,board)	_mm_storeu_si128((__m128i *) &(dst), (board))
#elif MOVE_GENERATOR == MOVE_GENERATOR_NEON
	#define	vBoard	uint64x2_t
	unsigned long long vboard_next(uint64x2_t OP, const int x, Board *next);
	#define	board_next(board,x,next)	vboard_next(vld1q_u64((uint64_t *) (board)), (x), (next))
	#define	load_vboard(board)	vld1q_u64((uint64_t *) &(board))
	#define	store_vboard(dst,board)	vst1q_u64((uint64_t *) &(dst), (board))
#else
	#define	vBoard	Board
	unsigned long long board_next(const Board *board, const int x, Board *next);
	#define	vboard_next(board,x,next)	board_next(&(board), (x), (next))
	#define	vboard_flip(board,x)	board_flip(&(board), (x))
	#define	load_vboard(board)	(board)
	#define	store_vboard(dst,board)	((dst) = (board))
#endif

// Pass vboard to get_moves if vectorcall available, otherwise board
#if defined(__AVX2__) && (vBoard == __m128i) && (defined(_MSC_VER) || defined(__linux__))
	unsigned long long vectorcall get_moves_avx(__m256i PP, __m256i OO);
	#define	get_moves(P,O)	get_moves_avx(_mm256_broadcastq_epi64(_mm_cvtsi64_si128(P)), _mm256_broadcastq_epi64(_mm_cvtsi64_si128(O)))
	#define	board_get_moves(board)	get_moves_avx(_mm256_broadcastq_epi64(*(__m128i *) &(board)->player), _mm256_broadcastq_epi64(*(__m128i *) &(board)->opponent))
	#define	vboard_get_moves(vboard,board)	get_moves_avx(_mm256_broadcastq_epi64(vboard), _mm256_permute4x64_epi64(_mm256_castsi128_si256(vboard), 0x55))
#else
	unsigned long long get_moves(const unsigned long long, const unsigned long long);
	#define	board_get_moves(board)	get_moves((board)->player, (board)->opponent)
	#define	vboard_get_moves(vboard,board)	get_moves((board).player, (board).opponent)
#endif

#endif
