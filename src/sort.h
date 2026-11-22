/**
 * @file sort.h
 *
 * @brief A sort with the comparison written in the loop (qsort calls a function at each comparison).
 *
 * Only for items whose order is total (two items that compare equal are the same item): any sort then gives
 * the array that qsort gives. A median of three quicksort, insertion sort for a few items, and qsort when the
 * partitions go wrong.
 */

#ifndef EDAX_SORT_H
#define EDAX_SORT_H

#include <stdlib.h>

/** Items under which a range is sorted by insertion. */
#define SORT_SMALL 16

/**
 * @brief Define name(array, n).
 *
 * @param name Name of the function.
 * @param T Type of an item.
 * @param LESS LESS(a, b): a goes before b.
 * @param CMP The same order as a comparison of qsort.
 */
#define DEFINE_SORT(name, T, LESS, CMP) \
static void name##_range(T *a, long long n, int depth) \
{ \
	long long i, j; \
	T p, t; \
	while (n > SORT_SMALL) { \
		const long long m = n / 2; \
		if (depth-- == 0) { qsort(a, (size_t) n, sizeof *a, CMP); return; } \
		if (LESS(a[m], a[0])) { t = a[m]; a[m] = a[0]; a[0] = t; } \
		if (LESS(a[n - 1], a[m])) { \
			t = a[m]; a[m] = a[n - 1]; a[n - 1] = t; \
			if (LESS(a[m], a[0])) { t = a[m]; a[m] = a[0]; a[0] = t; } \
		} \
		p = a[m]; /* (a[0] and a[n - 1] stop the two scans) */ \
		i = 0; j = n - 1; \
		for (;;) { \
			do ++i; while (LESS(a[i], p)); \
			do --j; while (LESS(p, a[j])); \
			if (i >= j) break; \
			t = a[i]; a[i] = a[j]; a[j] = t; \
		} \
		++j; /* 1 <= j < n */ \
		if (j < n - j) { \
			name##_range(a, j, depth); \
			a += j; n -= j; \
		} else { \
			name##_range(a + j, n - j, depth); \
			n = j; \
		} \
	} \
	for (i = 1; i < n; ++i) { \
		t = a[i]; \
		for (j = i; j > 0 && LESS(t, a[j - 1]); --j) a[j] = a[j - 1]; \
		a[j] = t; \
	} \
} \
static void name(T *a, const long long n) \
{ \
	long long k; \
	int depth = 2; \
	for (k = n; k > 1; k >>= 1) depth += 2; \
	name##_range(a, n, depth); \
}

#endif // EDAX_SORT_H
