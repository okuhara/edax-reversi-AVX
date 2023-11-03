/**
 * @file hash.h
 *
 * Hash table's header.
 *
 * @date 1998 - 2021
 * @author Richard Delorme
 * @version 4.5
 */

#ifndef EDAX_HASH_H
#define EDAX_HASH_H

#include "board.h"
#include "settings.h"
#include "util.h"
#include "stats.h"

#include <stdbool.h>
#include <stdio.h>

/** HashData : data stored in the hash table */
typedef struct HashData {
	union {
#ifdef __BIG_ENDIAN__
		struct {
			unsigned char date;       /*!< dating technique */
			unsigned char cost;       /*!< search cost */
			unsigned char selectivity;/*!< selectivity */
			unsigned char depth;      /*!< depth */
		} c;
		struct {
			unsigned short date_cost;
			unsigned short selectivity_depth;
		} us;
#else
		struct {
			unsigned char depth;      /*!< depth */
			unsigned char selectivity;/*!< selectivity */
			unsigned char cost;       /*!< search cost */
			unsigned char date;       /*!< dating technique */
		} c;
		struct {
			unsigned short selectivity_depth;
			unsigned short date_cost;
		} us;
#endif
		unsigned int	ui;      /*!< as writable level */
	} wl;
	signed char lower;        /*!< lower bound of the position score */
	signed char upper;        /*!< upper bound of the position score */
	unsigned char move[2];    /*!< best moves */
} HashData;

/** Hash  : item stored in the hash table */
typedef struct Hash {
	HASH_COLLISIONS(unsigned long long key;)
	Board board;
	HashData data;
} Hash;

/** HashLock : lock for table entries */
typedef struct HashLock {
	SpinLock spin;
} HashLock;

/** HashTable: position storage */
typedef struct HashTable {
	void *memory;                 /*!< allocated memory */
	Hash *hash;                   /*!< hash table */
	HashLock *lock;               /*!< table with locks */
	unsigned long long hash_mask; /*!< a bit mask for hash entries */
	unsigned int lock_mask;       /*!< a bit mask for lock entries */
	int n_hash;                   /*!< hash table size */
	int n_lock;                   /*!< number of locks */
	unsigned char date;           /*!< date */
} HashTable;

/** HashStoreData : data to store */
typedef struct HashStoreData {
	HashData data;
	HASH_COLLISIONS(unsigned long long hash_code;)
	int alpha;
	int beta;
	int score;
} HashStoreData;

/* declaration */
// use vectored board if vectorcall available
#if defined(hasSSE2) && (defined(_MSC_VER) || defined(__linux__))
	#define	HBOARD	__m128i
	#define	HBOARD_P(b)	_mm_loadu_si128((__m128i *) (b))
	#define	HBOARD_V(b)	((b).v2)
#elif defined(__aarch64__) || defined(_M_ARM64)
	#define	HBOARD	uint64x2_t
	#define	HBOARD_P(b)	vld1q_u64((uint64_t *) (b))
	#define	HBOARD_V(b)	((b).v2)
#else
	#define	HBOARD	const Board *
	#define	HBOARD_P(b)	(b)
	#define	HBOARD_V(b)	(&(b).board)
#endif

void hash_move_init(void);
void hash_init(HashTable*, const unsigned long long);
void hash_cleanup(HashTable*);
void hash_clear(HashTable*);
void hash_free(HashTable*);
void vectorcall hash_feed(HashTable*, HBOARD, const unsigned long long, HashStoreData *);
void vectorcall hash_store(HashTable*, HBOARD, const unsigned long long, HashStoreData *);
void vectorcall hash_force(HashTable*, HBOARD, const unsigned long long, HashStoreData *);
bool vectorcall hash_get(HashTable*, HBOARD, const unsigned long long, HashData *);
bool hash_get_from_board(HashTable*, const Board*, HashData *);
void vectorcall hash_exclude_move(HashTable*, HBOARD, const unsigned long long, const int);
void hash_copy(const HashTable*, HashTable*);
void hash_print(const HashData*, FILE*);
extern unsigned int writeable_level(HashData *data);

extern const HashData HASH_DATA_INIT;

inline void hash_prefetch(HashTable *hashtable, unsigned long long hashcode) {
	Hash *p = hashtable->hash + (hashcode & hashtable->hash_mask);
  #ifdef hasSSE2
	_mm_prefetch((char const *) p, _MM_HINT_T0);
	_mm_prefetch((char const *)(p + HASH_N_WAY - 1), _MM_HINT_T0);
  #elif defined(__ARM_ACLE)
	__pld(p);
	__pld(p + HASH_N_WAY - 1);
  #elif defined(__GNUC__)
	__builtin_prefetch(p);
	__builtin_prefetch(p + HASH_N_WAY - 1);
  #endif
}

#endif
