/**
 * @file empty.h
 *
 * List of empty squares.
 *
 * @date 1998 - 2026
 * @author Richard Delorme
 * @version 4.5
 */

#ifndef EDAX_EMPTY_H
#define EDAX_EMPTY_H

/** double linked list of squares */
typedef struct SquareList {
	unsigned char previous;	/*!< link to previous square */
	unsigned char next;	/*!< link to next square */
} SquareList;

/**
 * @brief Remove an empty square from the list.
 *
 * @param empty  empty square.
 * @param x Square to remove.
 */
static inline void empty_remove(SquareList *empty, int x)
{
	int next = empty[x].next;
	int prev = empty[x].previous;
	empty[prev].next = next;
	empty[next].previous = prev;
}

/**
 * @brief Restore an empty square into the list of empty squares.
 *
 * @param empty  empty square.
 * @param x Square to restore.
 */
static inline void empty_restore(SquareList *empty, int x)
{
	empty[empty[x].previous].next = x;
	empty[empty[x].next].previous = x;
}

/** Loop over all empty squares */
#define foreach_empty(x, empty)\
	for ((x) = (empty)[NOMOVE].next; (x) != NOMOVE; (x) = (empty)[(x)].next)

#endif

