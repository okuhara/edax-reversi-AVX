/**
 * @file histogram.c
 *
 * Hash table's header.
 *
 * @date 1998 - 2017
 * @author Richard Delorme
 * @version 4.4
 */

#include "util.h"

#include <stdio.h>
#include <math.h>
#include <stdlib.h>

#ifndef RCD
#define RCD 0.5
#endif

void histogram_init(unsigned long long h[129][65])
{
	int i, j;

	for (i = 0; i < 129; ++i)
	for (j = 0; j < 65; ++j)
		h[i][j] = 0;
}

void histogram_print(unsigned long long h[129][65]) 
{
	int i, j;
	unsigned long long n, N;
	double x, y, s, s2;
	double mean[65], variance[65], median[65], bias[65], precision[65];

	for (j = 0; j < 65; ++j) printf("; %d", j * 2 - 64);
	printf("; total; mean; error; bias; precision");  
	for (i = 0; i < 129; ++i) {
		printf("\n%d", i - 64);
		n = s = s2 = 0;
			x = (i - 64);
		for (j = 0; j < 65; ++j) {
			y = 2 * j - 64;
			printf("; %llu", h[i][j]);
			n += h[i][j];
			s += y * h[i][j];
			s2 += (x - y) * (x - y) * h[i][j];
		}
		// (a row of 0 or 1 position: up to v4.5.5-nikque.26 divided by 0, nan and inf were printed)
		s = n > 0 ? s / n : 0.0;
		s2 = n > 1 ? s2 / (n - 1) : 0.0;
		if (n > 5) printf("; %llu; %.2f; %.2f; %.2f; %.2f", n, s, sqrt(s2), x - s, s2 > (x - s) * (x - s) ? sqrt(s2 - (x - s) * (x - s)) : 0.0);
		else if (n > 0) printf("; %llu; %.2f; %.2f", n, s, sqrt(s2));
		else printf("; %llu", n);
	}

	printf("\n");
	N = 0;
	printf("total");
	for (j = 0; j < 65; ++j) {
		n = s = s2 = 0;
		y = 2 * j - 64;
		for (i = 0; i < 129; ++i) {
			x = (i - 64);
			n += h[i][j];
			s += x * h[i][j];
			s2 += (x - y) * (x - y) * h[i][j];
		}
		N += n;
		// (a column of 0 or 1 position: 0, as for the rows)
		mean[j] = n > 0 ? s / n : 0.0;
		variance[j] = n > 1 ? s2 / (n - 1) : 0.0;
		bias[j] = n > 0 ? y - mean[j] : 0.0;
		precision[j] = n > 1 && variance[j] > bias[j] * bias[j] ? sqrt(variance[j] - bias[j] * bias[j]) : 0.0;
		// the mean of the values of rank (n - 1) / 2 and n / 2 (up to v4.5.5-nikque.15, with an even count, the bin
		// after the lower one was taken for the upper one, even when it was empty)
		median[j] = 0.0;
		if (n > 0) {
			long long lower = -1, upper = -1;
			s = 0;
			for (i = 0; i < 129 && upper < 0; ++i) {
				s += h[i][j];
				if (lower < 0 && s > (n - 1) / 2) lower = i;
				if (s > n / 2) upper = i;
			}
			median[j] = (lower + upper) * 0.5 - 64;
		}
		printf("; %llu", n);
	}
	printf("; %llu\n", N);
	printf("mean"); for (j = 0; j < 65; ++j) printf("; %.2f", mean[j]); printf("\n");
	printf("median"); for (j = 0; j < 65; ++j) printf("; %.2f", median[j]); printf("\n");
	printf("error"); for (j = 0; j < 65; ++j) printf("; %.2f", sqrt(variance[j])); printf("\n");
	printf("bias"); for (j = 0; j < 65; ++j) printf("; %.2f", bias[j]); printf("\n");
	printf("precision"); for (j = 0; j < 65; ++j) printf("; %.2f", precision[j]); printf("\n");
}

void histogram_stats(unsigned long long h[129][65])
{
	double y[65], x[129];
	double m_x, m_y, s_x, s_y, s_xy;
	double a, b, r;
	double n;
	int i, j;

	for (j = 0; j < 65; ++j) y[j] = j * 2 - 64;
	for (j = 0; j < 129; ++j) x[j] = j - 64;

	m_x = m_y = s_x = s_y = s_xy = 0.0;
	n = 0.0;

	for (i = 0; i < 129; ++i)
	for (j = 0; j < 65; ++j) {
		n += h[i][j];
		m_x += x[i] * h[i][j];
		m_y += y[j] * h[i][j];
		s_x += x[i] * x[i] * h[i][j];
		s_y += y[j] * y[j] * h[i][j];
		s_xy += x[i] * y[j] * h[i][j];
	}

	if (n == 0.0) { // (no position: up to v4.5.5-nikque.26 divided by 0, and nan was printed)
		printf("statistics summary\n");
		printf("n  = 0\n\n");
		return;
	}
	m_x /= n;
	m_y /= n;

	// a variance below 0, or a few ulps above 0, by rounding is 0
	s_x = s_x / n - m_x * m_x; if (s_x <= 1e-12 * (m_x * m_x + 1.0)) s_x = 0.0;
	s_y = s_y / n - m_y * m_y; if (s_y <= 1e-12 * (m_y * m_y + 1.0)) s_y = 0.0;
	s_x = sqrt(s_x);
	s_y = sqrt(s_y);
	s_xy = s_xy / n - m_x * m_y;

	printf("statistics summary\n");
	printf("n  = %.0f\n", n);
	printf("m_eval  = %.6f; m_score = %.6f\n", m_x, m_y);
	printf("s_eval = %.6f; s_score = %.6f; cov = %.6f\n", s_x, s_y, s_xy);
	if (s_x == 0.0 || s_y == 0.0) { // one value of the evaluation or of the score: the lines would divide by 0
		printf("regression and correlation undefined (the %s has a single value)\n\n", s_x == 0.0 ? "evaluation" : "score");
		return;
	}
	r = s_xy / (s_x * s_y);
	a = s_xy / (s_x * s_x); // regression (covariance / variance: up to v4.5.5-nikque.15 sqrt(covariance) / deviation)
	b = m_y - a * m_x;
	printf("score = %.6f * eval + %.6f; r = %.6f; r2 = %.6f (regression)\n", a, b, r, r*r);
	a = s_y / s_x; // correlation
	b = m_y - a * m_x;
	printf("score = %.6f * eval + %.6f;(correlation)\n", a, b);
	a = s_xy / (s_y * s_y); // regression (covariance / variance)
	b = m_x - a * m_y;
	printf("eval = %.6f * score + %.6f;(regression)\n", a, b);
	a = s_x / s_y; // correlation
	b = m_x - a * m_y;
	printf("eval = %.6f * score + %.6f;(correlation)\n", a, b);
	printf("\n");
}

void histogram_to_ppm(const char *file, unsigned long long histogram[129][65])
{
	FILE *f;
	int i, j, i_k, j_k, v;
	unsigned long long max;
	double w;
	unsigned char r[256], g[256], b[256];

	// open the file
	f = fopen(file, "w");
	if (f == NULL) {
		warn("cannot open ppm file %s\n", file);
		return;
	}

	// compute max_value
	max = 0; 
	for (i = 0; i < 129; ++i)
	for (j = 0; j < 65; ++j) {
		if (histogram[i][j] > max) max = histogram[i][j];
	}
	w = (max > 0 ? 255.0 / max : 0.0); // (an empty histogram: up to v4.5.5-nikque.15 a division by 0)

	// palette
	r[0] = g[0] = b[0] = 255;
	for (i = 1; i < 64; i++) { r[i +   0] = 255 - i * 4; g[i +   0] = 0;           b[i +   0] = 255;}
	for (i = 0; i < 64; i++) { r[i +  64] = 0;           g[i +  64] = i * 4;       b[i +  64] = 255 - i * 4;}
	for (i = 0; i < 64; i++) { r[i + 128] = i * 4;       g[i + 128] = 255;         b[i + 128] = 0;}
	for (i = 0; i < 64; i++) { r[i + 192] = 255;         g[i + 192] = 255 - i * 4; b[i + 192] = 0;}

	// header;
	fprintf(f, "P3\n%d %d\n%d\n", 516, 520, 255);

	// data		
	for (i = 64; i >= 0; --i) {
		for (i_k = 0; i_k < 8; ++i_k) {
			for (j = 0; j < 129; ++j) {
				v = floor(histogram[j][i] * w + RCD); if (v == 0 && histogram[j][i] > 0) v = 1;
				for (j_k = 0; j_k < 4; ++j_k) fprintf(f, "%d %d %d  ", r[v], g[v], b[v]);
			}
			fputc('\n', f);
		}
	}

	// end
	fclose(f);
}

