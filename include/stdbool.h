/**
 * @file stdbool.h
 *
 * @brief Replacement include file for deficient C compiler.
 * Do not include this file for modern compilers (C99 or later, VS2015 or later);
 * this file is not compatible with arm SVE build.
 *
 * @date 1998 - 2017
 * @author Richard Delorme
 * @version 4.4
 */

#ifndef EDAX_STDBOOL_H
#define EDAX_STDBOOL_H

typedef enum {
	false = 0,
	true = 1
} bool;

#endif // EDAX_STDBOOL_H
