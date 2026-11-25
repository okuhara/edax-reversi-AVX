/**
 * @file hash.c
 *
 * @brief Locked transposition table.
 *
 * The hash table is an efficient memory system to remember the previously
 * analysed positions and re-use the collected data when needed.
 * The hash table contains entries of analysed data where the board is stored
 * and the results of the recorded analysis are two score bounds, the level of
 * the analysis and the best move found.
 * The implementation is now a multi-way (or bucket based) hashtable. It both
 * tries to keep the deepest records and to always add the latest one.
 * The following implementation store the whole board to avoid collision. 
 * When doing parallel search with a shared hashtable, a locked implementation
 * avoid concurrency collisions.
 *
 * @date 1998 - 2025
 * @author Richard Delorme
 * @version 4.5
 */

#include "bit.h"
#include "hash.h"
#include "options.h"
#include "stats.h"
#include "search.h"
#include "util.h"
#include "settings.h"

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

/** HashData init value */
const HashData HASH_DATA_INIT = {{{ 0, 0, 0, 0 }}, -SCORE_INF, SCORE_INF, { NOMOVE, NOMOVE }};

/**
 * @brief Initialise the hashtable.
 *
 * Allocate the hash table entries and initialise the hash masks.
 *
 * @param hash_table Hash table to setup.
 * @param size Requested size for the hash table in number of entries.
 */
void hash_init(HashTable *hash_table, const unsigned long long size)
{
	int i, n_way;

	for (n_way = 1; n_way < HASH_N_WAY; n_way <<= 1);	// round up HASH_N_WAY to 2 ^ n

	assert(hash_table != NULL);
	assert((n_way & -n_way) == n_way);

	info("< init hashtable of %llu entries>\n", size);
	if (hash_table->hash != NULL) { // a new size: the former table and its locks are released
		free(hash_table->memory);
		for (i = 0; i < hash_table->n_lock; ++i) spin_free(hash_table->lock + i);
		free(hash_table->lock);
	}
	hash_table->memory = malloc((size + n_way + 1) * sizeof (Hash));
	if (hash_table->memory == NULL) {
		fatal_error("hash_init: cannot allocate the hash table\n");
	}

	if (HASH_ALIGNED) {
		size_t alignment = n_way * sizeof (Hash);	// (4 * 24)
		alignment = (alignment & -alignment) - 1;	// LS1B - 1 (0x1f)
		hash_table->hash = (Hash*) (((size_t) hash_table->memory + alignment) & ~alignment);
		hash_table->hash_mask = size - n_way;
	} else {
		hash_table->hash = (Hash*) hash_table->memory;
		hash_table->hash_mask = size - 1;
	}

	hash_table->n_hash = hash_table->hash_mask + HASH_N_WAY;	// (4.5.5)
	hash_wipe(hash_table);

	hash_table->n_lock = 1 << (31 - lzcnt_u32(get_cpu_number() | 1) + 8);	// round down to 2 ^ n, then * 256
	hash_table->lock_mask = hash_table->n_lock - 1;
	// hash_table->n_lock += n_way + 1;
	hash_table->lock = (HashLock*) malloc(hash_table->n_lock * sizeof (HashLock));
	if (hash_table->lock == NULL) {
		fatal_error("hash_init: cannot allocate the hash table locks\n");
	}

	for (i = 0; i < hash_table->n_lock; ++i) spin_init(hash_table->lock + i);
}

/**
 * @brief Clear the hashtable.
 *
 * Make all hash table entries empty.
 * The entries are not rewritten: the table base date moves past
 * every stored date, and entries dated before the base are handled exactly as empty ones
 * (no match, lowest level). The memory is only wiped when the date range is used up.
 *
 * @param hash_table Hash table to clear.
 */
void hash_cleanup(HashTable *hash_table)
{
	// newer than any stored date (hash_feed may store date 1 while date is 0)
	const unsigned int next_base = hash_table->base + hash_table->date + 2;

	assert(hash_table != NULL && hash_table->hash != NULL);
	if (next_base <= 255 - 127) {	// room for dates up to 127 after the base
		info("< cleaning hashtable (base %d) >\n", next_base);
		hash_table->base = (unsigned char) next_base;
		hash_table->date = 0;
		return;
	}
	hash_wipe(hash_table);
}

/**
 * @brief Clear the hashtable memory.
 *
 * Set all hash table entries to zero.
 * @param hash_table Hash table to clear.
 */
void hash_wipe(HashTable *hash_table)
{
	unsigned int i = 0, imax = hash_table->n_hash;
	Hash *pHash = hash_table->hash;

	assert(hash_table != NULL && hash_table->hash != NULL);

	info("< cleaning hashtable >\n");

  #if defined(hasSSE2) || defined(USE_MSVC_X86)
	if (hasSSE2 && (sizeof(Hash) == 24) && (((size_t) pHash & 0x1f) == 0) && (imax > 7)) {
		for (; i < 4; ++i, ++pHash) {
			HASH_COLLISIONS(pHash->key = 0;)
			pHash->board.player = pHash->board.opponent = 0;
			pHash->data = HASH_DATA_INIT;
		}
    #ifdef __AVX__
		__m256i d0 = _mm256_load_si256((__m256i *)(pHash - 4));
		__m256i d1 = _mm256_load_si256((__m256i *)(pHash - 4) + 1);
		__m256i d2 = _mm256_load_si256((__m256i *)(pHash - 4) + 2);
		for (i = 4; i < imax - 3; i += 4, pHash += 4) {
			_mm256_stream_si256((__m256i *) pHash, d0);
			_mm256_stream_si256((__m256i *) pHash + 1, d1);
			_mm256_stream_si256((__m256i *) pHash + 2, d2);
		}
    #else
		__m128i d0 = _mm_load_si128((__m128i *)(pHash - 4));
		__m128i d1 = _mm_load_si128((__m128i *)(pHash - 4) + 1);
		__m128i d2 = _mm_load_si128((__m128i *)(pHash - 4) + 2);
		for (i = 4; i < imax - 1; i += 2, pHash += 2) {
			_mm_stream_si128((__m128i *) pHash, d0);
			_mm_stream_si128((__m128i *) pHash + 1, d1);
			_mm_stream_si128((__m128i *) pHash + 2, d2);
		}
    #endif
		_mm_sfence();
	}
  #endif
	for (; i < imax; ++i, ++pHash) {
		HASH_COLLISIONS(pHash->key = 0;)
		pHash->board.player = pHash->board.opponent = 0; 
		pHash->data = HASH_DATA_INIT;
	}
	hash_table->date = 0;
	hash_table->base = 0;
}

/**
 * @brief Clear the hashtable.
 *
 * Change the date of the hash table.
 * @param hash_table Hash table to clear.
 */
void hash_clear(HashTable *hash_table)
{
	assert(hash_table != NULL);

	if (hash_table->date == 127) hash_cleanup(hash_table);
	++hash_table->date;
	info("< clearing hashtable -> date = %d>\n", hash_table->date);
	assert(hash_table->date > 0 && hash_table->date <= 127);
}

/**
 * @brief Free the hashtable.
 *
 * Free the memory allocated by the hash table entries
 * @param hash_table hash_table to free.
 */
void hash_free(HashTable *hash_table)
{
	int i;

	assert(hash_table != NULL && hash_table->hash != NULL);
	free(hash_table->memory);
	hash_table->hash = NULL;
	for (i = 0; i < hash_table->n_lock; ++i) spin_free(hash_table->lock + i);
	free(hash_table->lock);
}

/**
 * @brief make a level from date, cost, depth & selectivity.
 *
 * @param data Hash data.
 * @return A level.
 */
inline unsigned int writeable_level(HashData *data)
{
#if USE_TYPE_PUNING	// HACK
	return (data->wl.ui);
#else	// slow but more portable implementation.
	return (data->wl.c.date << 24) + (data->wl.c.cost << 16) + (data->wl.c.selectivity << 8) + data->wl.c.depth;
#endif
}

/** true if the entry was stored after the last cleanup (else it is empty). */
#define	hash_is_live(hash_table, hash)	((hash)->data.wl.c.date >= (hash_table)->base)

/*
 * The board and the date of an entry are read before the lock of the entry is taken (hash_get,
 * hash_exclude_move), so that the lock is only taken for an entry that may hold the position; the
 * entry is then read again under the lock, and only that second reading is used. These two fields
 * are read and written as relaxed atomics: on 64-bit processors they are the plain loads and stores,
 * with no fence. All the other fields are only read and written under the lock.
 */
#if (defined(__GNUC__) || defined(__clang__)) && defined(__SIZEOF_POINTER__) && (__SIZEOF_POINTER__ == 8)
static inline unsigned long long hash_peek_u64(const unsigned long long *p) { return __atomic_load_n(p, __ATOMIC_RELAXED); }
static inline void hash_poke_u64(unsigned long long *p, const unsigned long long v) { __atomic_store_n(p, v, __ATOMIC_RELAXED); }
static inline unsigned char hash_peek_u8(const unsigned char *p) { return __atomic_load_n(p, __ATOMIC_RELAXED); }
static inline void hash_poke_u8(unsigned char *p, const unsigned char v) { __atomic_store_n(p, v, __ATOMIC_RELAXED); }
#else
// (volatile: one load or store each time. A 64-bit value may be read in two halves on a 32-bit processor: the
// test before the lock is then wrong for this reading only, and the test under the lock decides.)
static inline unsigned long long hash_peek_u64(const unsigned long long *p) { return *(const volatile unsigned long long *) p; }
static inline void hash_poke_u64(unsigned long long *p, const unsigned long long v) { *(volatile unsigned long long *) p = v; }
static inline unsigned char hash_peek_u8(const unsigned char *p) { return *(const volatile unsigned char *) p; }
static inline void hash_poke_u8(unsigned char *p, const unsigned char v) { *(volatile unsigned char *) p = v; }
#endif

/**
 * @brief Test, without the lock, if an entry may hold a board.
 *
 * @param hash_table Hash table.
 * @param hash Hash entry.
 * @param board Bitboard.
 * @return true if the entry looked like the board when it was read (to be tested again under the lock).
 */
static inline bool hash_peek(const HashTable *hash_table, const Hash *hash, const Board *board)
{
	return hash_peek_u64(&hash->board.player) == board->player && hash_peek_u64(&hash->board.opponent) == board->opponent
		&& hash_peek_u8(&hash->data.wl.c.date) >= hash_table->base;
}

/** Write the board of an entry (under its lock). */
static inline void hash_put_board(Hash *hash, const Board *board)
{
	hash_poke_u64(&hash->board.player, board->player);
	hash_poke_u64(&hash->board.opponent, board->opponent);
}

/** Write the date of an entry (under its lock). */
static inline void hash_put_date(HashData *data, const unsigned char date)
{
	hash_poke_u8(&data->wl.c.date, date);
}

/** Write the level (depth, selectivity, cost, date) of an entry (under its lock). */
static inline void hash_put_level(HashData *data, const HashData *from)
{
	data->wl.c.depth = from->wl.c.depth;
	data->wl.c.selectivity = from->wl.c.selectivity;
	data->wl.c.cost = from->wl.c.cost;
	hash_put_date(data, from->wl.c.date);
}

/** Write all the data of an entry (under its lock). */
static inline void hash_put_data(HashData *data, const HashData *from)
{
	data->lower = from->lower;
	data->upper = from->upper;
	data->move[0] = from->move[0];
	data->move[1] = from->move[1];
	hash_put_level(data, from);
}

/**
 * @brief Level of an entry, as an eagerly cleaned table would have it.
 *
 * @param hash_table Hash table.
 * @param hash Hash entry.
 * @return level with the date relative to the base, 0 for an empty entry.
 */
static inline unsigned int entry_level(const HashTable *hash_table, Hash *hash)
{
	return hash_is_live(hash_table, hash) ? writeable_level(&hash->data) - ((unsigned int) hash_table->base << 24) : 0;
}

/**
 * @brief Lock of the entries a hash code goes to.
 *
 * The lock is chosen from the entries (hash_code & hash_mask), not from the hash code: the
 * hash codes that only differ by their lowest bits go to the same HASH_N_WAY entries, and must
 * take the same lock. Up to v4.5.5-nikque.15 the lock was hash_code & lock_mask: two threads could
 * write the same entry under two locks, and leave a board with the data of another board.
 *
 * @param hash_table Hash table.
 * @param hash_code Hash code.
 * @return the lock.
 */
static inline HashLock* hash_lock(const HashTable *hash_table, const unsigned long long hash_code)
{
	return hash_table->lock + (((hash_code & hash_table->hash_mask) / HASH_N_WAY) & hash_table->lock_mask);
}

/**
 * @brief update an hash table item.
 *
 * This is done when the level is the same as the previous storage.
 * Best moves & bound scores are updated, other data are untouched.
 *
 * @param data Hash Data.
 * @param storedata.data.cost Search cost (log2(node count)).
 * @param storedata.alpha Alpha bound.
 * @param storedata.beta Beta bound.
 * @param storedata.score Best score.
 * @param storedata.move Best move.
 */
static void data_update(HashData *data, HashStoreData *storedata)
{
	int score = storedata->score;

	if (score < storedata->beta && score < data->upper) data->upper = (signed char) score;
	if (score > storedata->alpha && score > data->lower) data->lower = (signed char) score;
	if ((score > storedata->alpha || score == SCORE_MIN) && data->move[0] != storedata->data.move[0]) {
		data->move[1] = data->move[0];
		data->move[0] = storedata->data.move[0];
	}
	data->wl.c.cost = (unsigned char) MAX(storedata->data.wl.c.cost, data->wl.c.cost);
	HASH_STATS(++statistics.n_hash_update;)
}

/**
 * @brief Upgrade an hash table data item.
 *
 * Upgrade is done when the search level increases.
 * Best moves are updated, others data are reset to new value.
 *
 * @param data Hash Data.
 * @param storedata.data.depth Search depth.
 * @param storedata.data.selectivity Search selectivity.
 * @param storedata.data.cost Search cost (log2(node count)).
 * @param storedata.alpha Alpha bound.
 * @param storedata.beta Beta bound.
 * @param storedata.score Best score.
 * @param storedata.move Best move.
 */
static void data_upgrade(HashData *data, HashStoreData *storedata)
{
	int score = storedata->score;

	if (score < storedata->beta) data->upper = (signed char) score; else data->upper = SCORE_MAX;
	if (score > storedata->alpha) data->lower = (signed char) score; else data->lower = SCORE_MIN;
	if ((score > storedata->alpha || score == SCORE_MIN) && data->move[0] != storedata->data.move[0]) {
		data->move[1] = data->move[0];
		data->move[0] = storedata->data.move[0];
	}
	data->wl.us.selectivity_depth = storedata->data.wl.us.selectivity_depth;
	data->wl.c.cost = (unsigned char) MAX(storedata->data.wl.c.cost, data->wl.c.cost);  // this may not work well in parallel search.
	HASH_STATS(++statistics.n_hash_upgrade;)

	assert(data->upper >= data->lower);
}

/**
 * @brief Set an hash table data item.
 *
 * @param data Hash Data.
 * @param storedata.data.date Search Date.
 * @param storedata.data.depth Search depth.
 * @param storedata.data.selectivity Search selectivity.
 * @param storedata.data.cost Search cost (log2(node count)).
 * @param storedata.alpha Alpha bound.
 * @param storedata.beta Beta bound.
 * @param storedata.score Best score.
 * @param storedata.move Best move.
 */
static void data_new(HashData *data, HashStoreData *storedata)
{
	int score = storedata->score;

	if (score < storedata->beta) data->upper = (signed char) score; else data->upper = SCORE_MAX;
	if (score > storedata->alpha) data->lower = (signed char) score; else data->lower = SCORE_MIN;
	if (score > storedata->alpha || score == SCORE_MIN) data->move[0] = storedata->data.move[0];
	else data->move[0] = NOMOVE;
	data->move[1] = NOMOVE;
	hash_put_level(data, &storedata->data);
	assert(data->upper >= data->lower);
}

/**
 * @brief Initialize a new hash table item (under the lock of the entry).
 *
 * @param hash Hash Entry.
 * @param board Bitboard.
 * @param storedata.data.date Hash date.
 * @param storedata.data.depth Search depth.
 * @param storedata.data.selectivity Search selectivity.
 * @param storedata.data.cost Search cost.
 * @param storedata.alpha Alpha bound.
 * @param storedata.beta Beta bound.
 * @param storedata.score Best score.
 * @param storedata.move Best move.
 */
static void hash_new(Hash *hash, const Board *board, HashStoreData *storedata)
{
	HASH_STATS(if (storedata->data.wl.c.date == hash->data.wl.c.date) ++statistics.n_hash_remove;)
	HASH_STATS(++statistics.n_hash_new;)
	HASH_COLLISIONS(hash->key = storedata->hash_code;)
	hash_put_board(hash, board);
	data_new(&hash->data, storedata);
}

/**
 * @brief Set a new hash table item (under the lock of the entry).
 *
 * @param hash Hash Entry.
 * @param board Bitboard.
 * @param storedata.data.date Hash date.
 * @param storedata.data.depth Search depth.
 * @param storedata.data.selectivity Search selectivity.
 * @param storedata.data.cost Search cost.
 * @param storedata.data.lower Lower score bound.
 * @param storedata.data.upper Upper score bound.
 * @param storedata.move Best move.
 */
static void hash_set(Hash *hash, const Board *board, HashStoreData *storedata)
{
	storedata->data.move[1] = NOMOVE;
	HASH_STATS(if (storedata->data.wl.c.date == hash->data.wl.c.date) ++statistics.n_hash_remove;)
	HASH_STATS(++statistics.n_hash_new;)
	HASH_COLLISIONS(hash->key = storedata->hash_code;)
	hash_put_board(hash, board);
	hash_put_data(&hash->data, &storedata->data);
	assert(hash->data.upper >= hash->data.lower);
}

/**
 * @brief update the hash entry that holds the board (under the lock of the entry).
 *
 * @param hash Hash Entry.
 * @param storedata.data.date Hash date.
 * @param storedata.data.depth Search depth.
 * @param storedata.data.selectivity Search selectivity.
 * @param storedata.data.cost Hash Cost (log2(node count)).
 * @param storedata.alpha Alpha bound.
 * @param storedata.beta Beta bound.
 * @param storedata.score Best score.
 * @param storedata.move Best move.
 */
static void hash_update(Hash *hash, HashStoreData *storedata)
{
	if (hash->data.wl.us.selectivity_depth == storedata->data.wl.us.selectivity_depth)
		data_update(&hash->data, storedata);
	else	data_upgrade(&hash->data, storedata);
	hash_put_date(&hash->data, storedata->data.wl.c.date);
	if (hash->data.lower > hash->data.upper) { // reset the hash-table...
		data_new(&hash->data, storedata);
	}
}

/**
 * @brief Reset the hash entry that holds the board from new data values (under the lock of the entry).
 *
 * @param hash Hash Entry.
 * @param storedata.data.date Hash date.
 * @param storedata.data.depth Search depth.
 * @param storedata.data.selectivity Search selectivity.
 * @param storedata.data.lower Lower score bound.
 * @param storedata.data.upper Upper score bound.
 * @param storedata.move Best move.
 */
static void hash_reset(Hash *hash, HashStoreData *storedata)
{
	if (hash->data.wl.us.selectivity_depth == storedata->data.wl.us.selectivity_depth) {
		if (hash->data.lower < storedata->data.lower) hash->data.lower = storedata->data.lower;
		if (hash->data.upper > storedata->data.upper) hash->data.upper = storedata->data.upper;
	} else {
		hash->data.lower = storedata->data.lower;
		hash->data.upper = storedata->data.upper;
	}
	hash_put_level(&hash->data, &storedata->data);
	if (storedata->data.move[0] != NOMOVE) {
		hash->data.move[1] = hash->data.move[0];
		hash->data.move[0] = storedata->data.move[0];
	}
}

/**
 * @brief Look for the board in the HASH_N_WAY entries of a hash code (under their lock).
 *
 * The entries are read under the lock: two threads that store the same board at the same time do not
 * both take a new entry for it, and the entry to replace is chosen from the levels the entries have.
 * (Up to v4.5.5-nikque.33 they were read before the lock was taken.)
 *
 * @param hash_table Hash table.
 * @param hash First entry of the hash code.
 * @param board Bitboard.
 * @param worst Output: the entry of the lowest level, to be replaced when the board is not found.
 * @return the entry that holds the board, or NULL.
 */
static Hash* hash_find_locked(const HashTable *hash_table, Hash *hash, const Board *board, Hash **worst)
{
	int i;

	*worst = hash;
	for (i = 0; i < HASH_N_WAY; ++i, ++hash) {
		if (board_equal(&hash->board, board) && hash_is_live(hash_table, hash)) return hash;
		if (entry_level(hash_table, *worst) > entry_level(hash_table, hash)) *worst = hash;
	}
	return NULL;
}

/**
 * @brief feed hash table (from Cassio).
 *
 * @param hash_table Hash Table.
 * @param board Bitboard.
 * @param storedata.data.depth Search depth.
 * @param storedata.data.selectivity Selectivity level.
 * @param storedata.data.lower Alpha bound.
 * @param storedata.data.upper Beta bound.
 * @param storedata.move best move.
 */
void hash_feed(HashTable *hash_table, const Board *board, const unsigned long long hash_code, HashStoreData *storedata)
{
	Hash *hash, *worst;
	HashLock *lock = hash_lock(hash_table, hash_code);

	storedata->data.wl.c.date = (hash_table->date ? hash_table->date : 1) + hash_table->base;
	storedata->data.wl.c.cost = 0;

	spin_lock(lock);
	hash = hash_find_locked(hash_table, hash_table->hash + (hash_code & hash_table->hash_mask), board, &worst);
	if (hash) hash_reset(hash, storedata);
	else {
		HASH_COLLISIONS(storedata->hash_code = hash_code;)
		hash_set(worst, board, storedata);
	}
	spin_unlock(lock);
}

/**
 * @brief Store an hashtable item
 *
 * Find an hash table entry according to the evaluated board hash codes. Then
 * update the entry if it already exists otherwise create a new one. Collisions
 * are managed in such a way that better existing entries are always preserved
 * and the new evaluated data is always added. Lower and  upper score bounds
 * are then updated/set from the alpha, beta and score values according to the
 * following alphabeta property (where alpha < beta):
 *     -if (score >= beta) score is a lower bound of the real score
 *     -if (score <= alpha) score is an upper bound of the real score
 *     -if (alpha < score && score < beta) score equals the real score
 * So:
 *     -if (score < beta) update the upper bound of the hash entry
 *     -if (score > alpha) update the lower bound of the hash entry
 * The best move is also stored, but only if score >= alpha. In case the entry
 * already exists with better data, nothing is stored.
 *
 * @param hash_table Hash table to update.
 * @param board Bitboard.
 * @param hash_code  Hash code of an othello board.
 * @param storedata.data.depth      Search depth.
 * @param storedata.data.selectivity   Search selectivity.
 * @param storedata.data.cost       Search cost (i.e. log2(node count)).
 * @param storedata.alpha      Alpha bound when calling the alphabeta function.
 * @param storedata.beta       Beta bound when calling the alphabeta function.
 * @param storedata.score      Best score found.
 * @param storedata.move       Best move found.
 */
void hash_store(HashTable *hash_table, const Board *board, const unsigned long long hash_code, HashStoreData *storedata)
{
	Hash *hash, *worst;
	HashLock *lock = hash_lock(hash_table, hash_code);

	storedata->data.wl.c.date = hash_table->date + hash_table->base;

	spin_lock(lock);
	hash = hash_find_locked(hash_table, hash_table->hash + (hash_code & hash_table->hash_mask), board, &worst);
	if (hash) hash_update(hash, storedata);
	else {
		HASH_COLLISIONS(storedata->hash_code = hash_code;)
		hash_new(worst, board, storedata);
	}
	spin_unlock(lock);
}

/**
 * @brief Thead local (lockfree), 1-way version of hash_store
 * http://www.amy.hi-ho.ne.jp/okuhara/edaxopt.htm#localhash
 * https://github.com/Nyanyan/Egaroucid/pull/294
 *
 * @param hash Hash table entry to update.
 * @param board Bitboard.
 * @param alpha      Alpha bound when calling the alphabeta function.
 * @param beta       Beta bound when calling the alphabeta function.
 * @param score      Best score found.
 * @param move       Best move found.
 */
void hash_store_local(Hash *hash, V2DI vboard, int alpha, int beta, int score, int move)
{
	if (vboard_equal(vboard, &hash->board)) {	// data_update
		if (score < beta && score < hash->data.upper) hash->data.upper = score;
		if (score > alpha) {
			if (score > hash->data.lower) hash->data.lower = score;
			hash->data.move[0] = move;
		}

	} else {	// data_new
		int upper = SCORE_MAX;
		if (score < beta) upper = score;
		if (score <= alpha) {
			score = SCORE_MIN;
			move = NOMOVE;
		}
		hash->board = vboard.board;
		hash->data.upper = upper;
		hash->data.lower = score;
		hash->data.move[0] = move;
	}
}

/**
 * @brief Store an hashtable item.
 *
 * Does the same as hash_store() except it always store the current search state
 *
 * @param hash_table Hash table to update.
 * @param board Bitboard.
 * @param hash_code  Hash code of an othello board.
 * @param storedata.data.depth      Search depth.
 * @param storedata.data.selectivity   Search selectivity.
 * @param storedata.data.cost       Search cost (i.e. log2(node count)).
 * @param storedata.alpha      Alpha bound when calling the alphabeta function.
 * @param storedata.beta       Beta bound when calling the alphabeta function.
 * @param storedata.score      Best score found.
 * @param storedata.move       Best move found.
 */
void hash_force(HashTable *hash_table, const Board *board, const unsigned long long hash_code, HashStoreData *storedata)
{
	Hash *hash, *worst;
	HashLock *lock = hash_lock(hash_table, hash_code);

	storedata->data.wl.c.date = hash_table->date + hash_table->base;

	spin_lock(lock);
	hash = hash_find_locked(hash_table, hash_table->hash + (hash_code & hash_table->hash_mask), board, &worst);
	if (hash) data_new(&hash->data, storedata);
	else {
		HASH_COLLISIONS(storedata->hash_code = hash_code;)
		hash_new(worst, board, storedata);
	}
	spin_unlock(lock);
}

/**
 * @brief Find an hash table entry according to the evaluated board hash codes.
 *
 * @param hash_table Hash table.
 * @param board Bitboard.
 * @param hash_code Hash code of an othello board.
 * @param data Output hash data.
 * @return True the board was found, false otherwise.
 */
bool hash_get(HashTable *hash_table, const Board *board, const unsigned long long hash_code, HashData *data)
{
	int i;
	Hash *hash;
	HashLock *lock;
	bool ok = false;

	HASH_STATS(++statistics.n_hash_search;)
	HASH_COLLISIONS(++statistics.n_hash_n;)
	hash = hash_table->hash + (hash_code & hash_table->hash_mask);
	for (i = 0; i < HASH_N_WAY; ++i) {
		HASH_COLLISIONS(if (hash->key == hash_code) {)
		HASH_COLLISIONS(	lock = hash_lock(hash_table, hash_code);)
		HASH_COLLISIONS(	spin_lock(lock);)
		HASH_COLLISIONS(	if (hash->key == hash_code && !board_equal(board, &hash->board)) {)
		HASH_COLLISIONS(		++statistics.n_hash_collision;)
		HASH_COLLISIONS(		printf("key = %llu\n", hash_code);)
		HASH_COLLISIONS(		board_print(board, WHITE, stdout);)
		HASH_COLLISIONS(		board_print(&hash->board, WHITE, stdout);)
		HASH_COLLISIONS(	})
		HASH_COLLISIONS(	spin_unlock(lock);)
		HASH_COLLISIONS(})
		if (hash_peek(hash_table, hash, board)) {
			lock = hash_lock(hash_table, hash_code);
			spin_lock(lock);
			if (board_equal(&hash->board, board) && hash_is_live(hash_table, hash)) {
				*data = hash->data;
				data->wl.c.date -= hash_table->base;	// as stored by an eagerly cleaned table
				HASH_STATS(++statistics.n_hash_found;)
				hash_put_date(&hash->data, (unsigned char) (hash_table->date + hash_table->base));
				ok = true;
			}
			spin_unlock(lock);
			if (ok) return true;
		}
		++hash;
	}
	*data = HASH_DATA_INIT;
	return false;
}

/**
 * @brief Find an hash table entry from the board.
 *
 * @param hash_table Hash table.
 * @param board Bitboard.
 * @param data Output hash data.
 * @return True the board was found, false otherwise.
 */
bool hash_get_from_board(HashTable *hash_table, const Board *board, HashData *data)
{
	return hash_get(hash_table, board, board_get_hash_code(board), data);
}

/**
 * @brief Erase an hash table entry.
 *
 * @param hash_table Hash table.
 * @param board Bitboard.
 * @param hash_code Hash code of an othello board.
 * @param move Move to exclude.
 */
void hash_exclude_move(HashTable *hash_table, const Board *board, const unsigned long long hash_code, const int move)
{
	int i;
	Hash *hash;
	HashLock *lock;

	hash = hash_table->hash + (hash_code & hash_table->hash_mask);
	for (i = 0; i < HASH_N_WAY; ++i) {
		if (hash_peek(hash_table, hash, board)) {
			lock = hash_lock(hash_table, hash_code);
			spin_lock(lock);
			if (board_equal(&hash->board, board) && hash_is_live(hash_table, hash)) {
				if (hash->data.move[0] == move) {
					hash->data.move[0] = hash->data.move[1];
					hash->data.move[1] = NOMOVE;
				} else if (hash->data.move[1] == move) {
					hash->data.move[1] = NOMOVE;
				}
				hash->data.lower = SCORE_MIN;
			}
			spin_unlock(lock);
			return;
		}
		++hash;
	}
}

/**
 * @brief Copy an hastable to another one.
 *
 * @param src Source hash table to copy.
 * @param dest Destination hash table.
 */
void hash_copy(const HashTable *src, HashTable *dest)
{
	unsigned int i, imax = src->hash_mask + HASH_N_WAY;
	Hash *pSrc = src->hash, *pDest = dest->hash;

	assert(src->hash_mask == dest->hash_mask);
	info("<hash copy>\n");
	for (i = 0; i <= imax; ++i) {
		*pDest++ = *pSrc++;
	}
	dest->date = src->date;
	dest->base = src->base;
}

/**
 * @brief print HashData content.
 *
 * @param data Hash Data
 * @param f Output stream
 */
void hash_print(const HashData *data, FILE *f)
{
	char s_move[4];

	fprintf(f, "moves = %s, ", move_to_string(data->move[0], WHITE, s_move));
	fprintf(f, "%s ; ", move_to_string(data->move[1], WHITE, s_move));
	fprintf(f, "score = [%+02d, %+02d] ; ", data->lower, data->upper);
	fprintf(f, "level = %2d:%2d:%2d@%3d%%", data->wl.c.date, data->wl.c.cost, data->wl.c.depth, selectivity_table[data->wl.c.selectivity].percent);
}
