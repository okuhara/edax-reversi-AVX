/*
 * Tests of the book analyses for a user interface (src/book_analyze.c).
 *
 * umigame: random graphs of positions (layers, transpositions, several links to the same position, links that
 * skip layers as passes do): the umigame numbers with the shared positions counted once are compared with a
 * search of every choice of moves, and with the numbers that count each line when no position is shared; the values
 * of the nodes kept while the root is counted are compared with the numbers of the part after each node counted alone.
 * umigame-large: larger graphs, where terms are dropped: the numbers are the same with the values of the nodes kept.
 * umigame-order: the same graphs: the numbers are the same with the terms in random orders before they are sorted.
 *
 * precision: random books (positions from the start by random moves, values, links and leaves at random, some
 * not negamaxed again): the precision stages of the moves taken over from the current position and from the next
 * positions are compared with the stages walked from each position alone, and the walks left out are counted.
 *
 * reach, missing, umi-inherit: results taken over when the user interface goes to the next positions (random books;
 * reach-book, missing-book, umi-book: along the games of a file in a book file), compared with the results found from
 * each position alone: the positions reached (book moveinfo parts=count, book levels), book missing known to find
 * nothing by the kept stages, the umigame numbers kept from the lines of the moves of the previous positions.
 *
 * bookhint: the warning of a book command typed without book tells to type it after book.
 *
 * moveinfo-memory: book moveinfo when an allocation of a walk fails ends with an error, not with lines left out.
 * push-parent: an item of a walk whose number, or the number of its parent, does not fit in an int is refused.
 * reducecount-memory: the positions counted by book reducecount give their memory to a walk of another analysis.
 *
 * Build: tests\build-book-analyze.cmd (Visual Studio 2022 x64 Developer Command Prompt), run: tests\book-analyze.exe umigame
 * (or umigame-large, umigame-order, precision, precision-book <book file> [stages], reach, reach-book <book file> <games file>, missing,
 * missing-book <book file> <games file>, umi-inherit, umi-book <book file> <games file> [umi=...], bookhint, moveinfo-memory, push-parent, reducecount-memory)
 */
#define BOOK_ANALYZE_TEST
#define main edax_original_main
#include "../src/all.c"
#undef main

static unsigned long long rng_state = 0x9e3779b97f4a7c15ULL;

static unsigned int rnd(const unsigned int n)
{
	rng_state ^= rng_state << 13; rng_state ^= rng_state >> 7; rng_state ^= rng_state << 17;
	return (unsigned int) (rng_state % n);
}

typedef struct TestGraph {
	int n;
	int layer[4096];
	int color[4096];
	int n_child[4096];
	int child[4096][8];
} TestGraph;

/** @brief A random graph: node 0 is the root, the links go to a later layer of the other side to move. */
static void test_graph(TestGraph *g, const int n_layer, const int width, const int max_child)
{
	int first[64], count[64], l, i, k;

	g->n = 0;
	for (l = 0; l < n_layer; ++l) {
		count[l] = l == 0 ? 1 : 1 + (int) rnd((unsigned) width);
		first[l] = g->n;
		for (i = 0; i < count[l]; ++i) {
			g->layer[g->n] = l;
			g->color[g->n] = l & 1;
			g->n_child[g->n] = 0;
			++g->n;
		}
	}
	for (i = 0; i < g->n; ++i) {
		const int li = g->layer[i];
		const int nc = (li == n_layer - 1) ? 0 : (int) rnd((unsigned) max_child + 1);
		for (k = 0; k < nc; ++k) {
			int lj = li + 1;
			if (li + 3 < n_layer && rnd(6) == 0) lj = li + 3; // (a pass and its answer: the same side to move as one layer later)
			g->child[i][g->n_child[i]++] = first[lj] + (int) rnd((unsigned) count[lj]);
		}
		if (i == 0 && g->n_child[0] == 0 && n_layer > 1) g->child[0][g->n_child[0]++] = first[1];
	}
}

/** @brief Put a graph in the walk state of the analyses (as ba_umi_build does). */
static void test_load(BaWork *w, const TestGraph *g)
{
	int i, k, r = 0, l;
	unsigned int e = 0;

	ba_umi_free(w);
	w->n_node = w->size_node = (unsigned int) g->n;
	w->node = (BaUNode*) calloc((size_t) g->n, sizeof *w->node);
	w->edge = (unsigned int*) malloc(4096 * 8 * sizeof *w->edge);
	w->order = (unsigned int*) malloc((size_t) g->n * sizeof *w->order);
	w->rank = (unsigned int*) malloc((size_t) g->n * sizeof *w->rank);
	for (i = 0; i < g->n; ++i) {
		w->node[i].color = (unsigned char) g->color[i];
		w->node[i].alias = (unsigned int) i;
		w->node[i].edge = e;
		for (k = 0; k < g->n_child[i]; ++k) {
			w->edge[e++] = (unsigned int) g->child[i][k];
			++w->node[g->child[i][k]].indeg;
		}
		w->node[i].n_edge = (unsigned int) g->n_child[i];
		w->node[i].end = g->n_child[i] == 0;
	}
	w->n_edge = w->size_edge = e;
	for (l = 63; l >= 0; --l) for (i = 0; i < g->n; ++i) if (g->layer[i] == l) { w->order[r] = (unsigned int) i; w->rank[i] = (unsigned int) r; ++r; }
	for (i = 0; i < g->n; ++i) w->node[i].depth = i == 0 ? 0 : 255; // (moves from the root: the nodes are numbered layer by layer)
	for (i = 0; i < g->n; ++i) if (w->node[i].depth < 255) for (k = 0; k < g->n_child[i]; ++k) {
		BaUNode *c = w->node + g->child[i][k];
		if (c->depth > w->node[i].depth + 1) c->depth = (unsigned char) (w->node[i].depth + 1);
	}
}

/** @brief The part of a graph after a node (the node is the root). */
static void test_subgraph(const TestGraph *g, const int root, TestGraph *s)
{
	static int number[4096];
	int i, k;
	for (i = 0; i < g->n; ++i) number[i] = -1;
	number[root] = 0;
	s->n = 0;
	for (i = root; i < g->n; ++i) if (number[i] >= 0) { // (the next positions come later)
		const int j = s->n++;
		number[i] = j;
		s->layer[j] = g->layer[i];
		s->color[j] = g->color[i];
		s->n_child[j] = g->n_child[i];
		for (k = 0; k < g->n_child[i]; ++k) if (number[g->child[i][k]] < 0) number[g->child[i][k]] = 0;
	}
	for (i = root; i < g->n; ++i) if (number[i] >= 0) for (k = 0; k < g->n_child[i]; ++k) s->child[number[i]][k] = number[g->child[i][k]];
}

/* every choice of one move per position of the counting side */
static int bf_best, bf_calls;

static void bf_search(const TestGraph *g, const int counter, char *active, const int i, const int ends)
{
	int k;
	if (++bf_calls > 20000000) return;
	if (ends >= bf_best) return;
	if (i == g->n) { bf_best = ends; return; }
	if (!active[i]) { bf_search(g, counter, active, i + 1, ends); return; }
	if (g->n_child[i] == 0) { bf_search(g, counter, active, i + 1, ends + 1); return; }
	if (g->color[i] == counter) {
		for (k = 0; k < g->n_child[i]; ++k) {
			const int c = g->child[i][k], was = active[c];
			int j, seen = 0;
			for (j = 0; j < k; ++j) if (g->child[i][j] == c) seen = 1;
			if (seen) continue;
			active[c] = 1;
			bf_search(g, counter, active, i + 1, ends);
			active[c] = (char) was;
		}
	} else {
		char saved[8];
		for (k = 0; k < g->n_child[i]; ++k) saved[k] = active[g->child[i][k]];
		for (k = 0; k < g->n_child[i]; ++k) active[g->child[i][k]] = 1;
		bf_search(g, counter, active, i + 1, ends);
		for (k = g->n_child[i] - 1; k >= 0; --k) active[g->child[i][k]] = saved[k];
	}
}

static int brute_force(const TestGraph *g, const int counter, bool *complete)
{
	static char active[4096];
	memset(active, 0, (size_t) g->n);
	active[0] = 1;
	bf_best = INT_MAX; bf_calls = 0;
	bf_search(g, counter, active, 0, 0); // (nodes are numbered layer by layer: a node comes before its next positions)
	*complete = bf_calls <= 20000000;
	return bf_best;
}

/** @brief A walk state of the umigame numbers without book (test_load puts a graph in it). */
static void test_umi_work(BaWork *w, BaJob *job, BaParams *prm)
{
	memset(job, 0, sizeof *job);
	ba_params_default(prm);
	memset(w, 0, sizeof *w);
	w->job = job;
	w->prm = prm;
}

/**
 * @brief The values of the nodes kept while the root is counted (ba_umi_keep_value), compared with the numbers of the
 * part of the graph after each node counted alone, and with a search of every choice of moves there.
 * @return the wrong values.
 */
static int umigame_kept(const TestGraph *g, BaWork *w, BaWork *w2, const int t, long long *n_kept)
{
	static TestGraph s;
	int u, c, n_bad = 0;
	for (u = 1; u < g->n; ++u) for (c = 0; c < 2; ++c) {
		const long long kept = w->kept[2 * u + c];
		long long alone;
		bool complete;
		int want;
		if (w->node[u].depth == 255 || kept < 0) continue;
		test_subgraph(g, u, &s);
		test_load(w2, &s);
		alone = ba_umi_count(w2, c);
		ba_umi_clear(w2);
		want = brute_force(&s, c, &complete);
		++*n_kept;
		if (kept != alone || (complete && kept != want)) {
			if (++n_bad <= 10) printf("FAIL umigame graph %d node %d side %d: kept %lld, alone %lld, search %d\n", t, u, c, kept, alone, complete ? want : -1);
		}
	}
	return n_bad;
}

static int umigame(void)
{
	static TestGraph g;
	BaJob job;
	BaParams prm;
	BaWork w, w2;
	int t, c, n_bad = 0, n_shared = 0, n_checked = 0, n_skipped = 0, n_inexact = 0;
	long long t0 = real_clock(), n_kept = 0;

	test_umi_work(&w, &job, &prm);
	w.keep_depth = 255; // (the values of all the nodes are kept: the root is counted as without)
	w2 = w;
	w2.keep_depth = 0;
	for (t = 0; t < 20000; ++t) {
		const int n_layer = 2 + (int) rnd(9), width = 1 + (int) rnd(6), max_child = 1 + (int) rnd(4);
		test_graph(&g, n_layer, width, max_child);
		test_load(&w, &g);
		for (c = 0; c < 2; ++c) {
			bool complete;
			const long long got = ba_umi_count(&w, c);
			const bool exact = !w.inexact;
			const unsigned long long tree = ba_umi_tree(&w, c);
			const int want = brute_force(&g, c, &complete);
			bool shared = false;
			unsigned int i;
			ba_umi_clear(&w);
			for (i = 0; i < w.n_node; ++i) if (w.node[i].indeg >= 2) shared = true;
			if (!complete) { ++n_skipped; continue; }
			++n_checked;
			if (shared) ++n_shared;
			if (!exact) ++n_inexact;
			if (got != want || (!shared && (unsigned long long) got != tree)) {
				if (++n_bad <= 10) printf("FAIL umigame graph %d side %d: %lld, search %d, lines %llu (%d positions)\n", t, c, got, want, tree, g.n);
			}
		}
		n_bad += umigame_kept(&g, &w, &w2, t, &n_kept);
	}
	ba_umi_free(&w);
	ba_umi_free(&w2);
	printf("umigame: %d numbers checked (%d with shared positions, %d inexact, %d too long to search), %lld values of nodes kept, %d wrong, %.1f s\n",
		n_checked, n_shared, n_inexact, n_skipped, n_kept, n_bad, (real_clock() - t0) / 1000.0);
	if (n_kept == 0) { printf("FAIL umigame: no value kept\n"); return 2; }
	return n_bad ? 2 : 0;
}

/**
 * @brief Larger graphs, where terms are dropped: the numbers counted while the values of the nodes up to 2 moves
 * from the root are kept are the numbers counted without (and as exact); a value kept is the number of the part of the
 * graph after the node counted alone, or below it when that number is inexact.
 */
static int umigame_large(void)
{
	static TestGraph g, s;
	BaJob job;
	BaParams prm;
	BaWork w, w2;
	int t, c, u, n_inexact = 0, n_bad = 0;
	long long t0 = real_clock(), n_kept = 0, n_below = 0;

	test_umi_work(&w, &job, &prm);
	w2 = w;
	for (t = 0; t < 200; ++t) {
		long long plain[2];
		bool plain_exact[2];
		test_graph(&g, 40, 60, 3);
		test_load(&w, &g);
		w.keep_depth = 0;
		for (c = 0; c < 2; ++c) { plain[c] = ba_umi_count(&w, c); plain_exact[c] = !w.inexact; if (w.inexact) ++n_inexact; ba_umi_clear(&w); }
		test_load(&w, &g);
		w.keep_depth = 2;
		for (c = 0; c < 2; ++c) {
			const long long got = ba_umi_count(&w, c);
			if (got != plain[c] || w.inexact == plain_exact[c]) {
				if (++n_bad <= 10) printf("FAIL umigame large graph %d side %d: %lld%s while kept, %lld%s without\n", t, c, got, w.inexact ? " inexact" : "", plain[c], plain_exact[c] ? "" : " inexact");
			}
			ba_umi_clear(&w);
		}
		for (u = 1; u < g.n; ++u) for (c = 0; c < 2; ++c) {
			const long long kept = w.kept[2 * u + c];
			long long alone;
			if (w.node[u].depth > 2 || kept < 0) continue;
			test_subgraph(&g, u, &s);
			test_load(&w2, &s);
			alone = ba_umi_count(&w2, c);
			++n_kept;
			if (w2.inexact && kept <= alone) ++n_below;
			else if (kept != alone && ++n_bad <= 10) printf("FAIL umigame large graph %d node %d side %d: kept %lld, alone %lld%s\n", t, u, c, kept, alone, w2.inexact ? " inexact" : "");
			ba_umi_clear(&w2);
		}
	}
	ba_umi_free(&w);
	ba_umi_free(&w2);
	printf("umigame large: 200 graphs of about %d positions, %d inexact numbers; %lld values of nodes kept (%lld below an inexact number alone), %d wrong, %.1f s\n",
		g.n, n_inexact, n_kept, n_below, n_bad, (real_clock() - t0) / 1000.0);
	return n_bad ? 2 : 0;
}

/**
 * @brief The number of a graph counted with the terms as built, then with the terms in 3 random orders before they
 * are sorted. @return the orders that give another number or another exact mark.
 */
static int umigame_orders(BaWork *w, const int c, const unsigned long long seed, const char *name, int *n_inexact)
{
	long long ref;
	bool ref_exact;
	int k, n_bad = 0;
	ba_test_shuffle = 0;
	ref = ba_umi_count(w, c);
	ref_exact = !w->inexact;
	ba_umi_clear(w);
	if (!ref_exact) ++*n_inexact;
	for (k = 1; k <= 3; ++k) {
		long long got;
		ba_test_shuffle = seed + (unsigned long long) k;
		got = ba_umi_count(w, c);
		if (got != ref || w->inexact == ref_exact) {
			++n_bad;
			printf("FAIL umigame order %s side %d order %d: %lld%s, %lld%s with the terms as built\n", name, c, k, got, w->inexact ? " inexact" : "", ref, ref_exact ? "" : " inexact");
		}
		ba_umi_clear(w);
	}
	ba_test_shuffle = 0;
	return n_bad;
}

/**
 * @brief Larger graphs, where terms are dropped: the numbers and their exact marks are the same when the terms are
 * put in random orders before they are sorted (the order that qsort gives to equal keys depends on the C library);
 * so are the numbers of the parts of the graphs after the nodes up to 2 moves from the root.
 */
static int umigame_order(void)
{
	static TestGraph g, s;
	BaJob job;
	BaParams prm;
	BaWork w, w2;
	int t, c, u, n_inexact = 0, n_part_inexact = 0, n_bad = 0;
	long long t0 = real_clock(), n_part = 0;
	char name[64];

	test_umi_work(&w, &job, &prm);
	w2 = w;
	for (t = 0; t < 200; ++t) {
		test_graph(&g, 40, 60, 3);
		test_load(&w, &g);
		for (c = 0; c < 2; ++c) {
			snprintf(name, sizeof name, "graph %d", t);
			if (n_bad < 10) n_bad += umigame_orders(&w, c, (unsigned long long) (1000000 * t + 10 * c), name, &n_inexact);
		}
		for (u = 1; u < g.n; ++u) {
			if (w.node[u].depth > 2) continue;
			test_subgraph(&g, u, &s);
			test_load(&w2, &s);
			for (c = 0; c < 2; ++c) {
				snprintf(name, sizeof name, "graph %d node %d", t, u);
				++n_part;
				if (n_bad < 10) n_bad += umigame_orders(&w2, c, (unsigned long long) (1000000 * t + 100 * u + 10 * c), name, &n_part_inexact);
			}
		}
	}
	ba_umi_free(&w);
	ba_umi_free(&w2);
	printf("umigame order: 200 graphs of about %d positions (%d inexact numbers) and %lld parts after the nodes up to 2 moves (%d inexact), each counted again with the terms in 3 random orders: %d wrong, %.1f s\n",
		g.n, n_inexact, n_part, n_part_inexact, n_bad, (real_clock() - t0) / 1000.0);
	if (n_inexact == 0) { printf("FAIL umigame order: no inexact number\n"); return 2; }
	return n_bad ? 2 : 0;
}

/* ------------------------------------------------------------------ precision stages ------------------------------------------------------ */

#define TB_MAX 8192

static Board tb_board[TB_MAX];  /**< positions of a random book, in the order they were added (by plies) */
static int tb_n;

/** @brief Order of the positions: 0 as given (the next positions after), 1 backwards, 2 at random. */
static void test_order(int *order, const int n, const int o)
{
	int i;
	for (i = 0; i < n; ++i) order[i] = o == 1 ? n - 1 - i : i;
	if (o == 2) for (i = n - 1; i > 0; --i) { const int j = (int) rnd((unsigned) i + 1), k = order[i]; order[i] = order[j]; order[j] = k; }
}

/** @brief A score of a random book, in the range of the scores. */
static signed char tb_score(const int s)
{
	return (signed char) (s < -SCORE_MAX ? -SCORE_MAX : s > SCORE_MAX ? SCORE_MAX : s);
}

static void tb_add(Book *book, const Board *board)
{
	Position p;
	Board unique;
	board_unique(board, &unique);
	if (tb_n == TB_MAX || book_probe(book, &unique)) return;
	position_init(&p);
	p.board = unique;
	book_add(book, &p);
	tb_board[tb_n++] = unique;
}

/** @brief Value, links and leaf of a position of a random book, after those of its next positions (done when score.upper is set). */
static int tb_value(Book *book, Position *p)
{
	const unsigned long long moves = get_moves(p->board.player, p->board.opponent);
	int x[MAX_MOVE + 1], n = 0, n_leaf = 0, leaf[MAX_MOVE + 1], best = -SCORE_INF, i;

	if (p->score.upper != SCORE_INF) return p->score.value;
	p->level = (unsigned char) (rnd(3) == 0 ? 60 : 1 + rnd(20)); // (60: solved)
	if (moves == 0 && !can_move(p->board.opponent, p->board.player)) { // the end of the game
		p->score.value = p->score.lower = p->score.upper = (signed char) (2 * ((int) rnd(9) - 4));
		return p->score.value;
	}
	if (moves == 0) x[n++] = PASS;
	else { unsigned long long m; for (m = moves; m; m &= m - 1) x[n++] = (int) first_bit(m); }
	for (i = 0; i < n; ++i) {
		Board next;
		Position *child;
		board_next(&p->board, x[i], &next);
		child = book_probe(book, &next);
		if (child) {
			Link link;
			int s = -tb_value(book, child);
			if (rnd(25) == 0) s += rnd(2) ? 1 : -2; // (a link not negamaxed again)
			link.move = (unsigned char) x[i];
			link.score = tb_score(s);
			position_add_link(p, &link);
			if (link.score > best) best = link.score;
		} else if (x[i] != PASS) leaf[n_leaf++] = x[i];
	}
	if (n_leaf && rnd(8)) {
		int s = best > -SCORE_INF ? best - 2 * (int) rnd(4) + (rnd(4) == 0 ? 2 : 0) : 2 * ((int) rnd(9) - 4);
		if (rnd(20) == 0) ++s; // (an odd value)
		p->leaf.move = (unsigned char) leaf[rnd((unsigned) n_leaf)];
		p->leaf.score = tb_score(s);
		if (p->leaf.score > best) best = p->leaf.score;
	}
	if (best == -SCORE_INF) best = 0;
	if (rnd(25) == 0) best += rnd(2) ? 1 : -1; // (a value not negamaxed again)
	p->score.value = p->score.lower = p->score.upper = (signed char) best;
	return best;
}

/** @brief A random book: positions from the start by random moves (transpositions, passes), up to some plies. */
static void tb_build(Book *book, const int plies, const int width)
{
	int first = 0, ply, i;
	Board board;

	book_init(book);
	book->options.n_empties = 60 - plies + (int) rnd(3); // (the last plies are not walked)
	tb_n = 0;
	board_init(&board);
	tb_add(book, &board);
	for (ply = 0; ply < plies; ++ply) {
		const int last = tb_n;
		for (i = first; i < last; ++i) {
			const Board cur = tb_board[i];
			const unsigned long long moves = get_moves(cur.player, cur.opponent);
			Board next;
			int k;
			if (moves == 0) {
				if (can_move(cur.opponent, cur.player)) { board_next(&cur, PASS, &next); tb_add(book, &next); }
				continue;
			}
			for (k = 1 + (int) rnd((unsigned) width); k > 0; --k) {
				unsigned long long m = moves;
				int j = (int) rnd((unsigned) bit_count(moves));
				while (j--) m &= m - 1;
				board_next(&cur, (int) first_bit(m), &next);
				tb_add(book, &next);
			}
		}
		first = last;
	}
	for (i = 0; i < tb_n; ++i) tb_value(book, book_probe(book, tb_board + i));
}

/** @brief A random book of 5 to 10 plies (see tb_build), its positions numbered for the analyses. */
static void tb_random_book(Book *book)
{
	tb_build(book, 5 + (int) rnd(6), 1 + (int) rnd(3));
	ba_index_free();
	ba_index_get(book);
}

/** @brief A job on a book (its position is set for each test) and its walk state. */
static void test_book_work(BaWork *w, BaJob *job, Book *book, const BaParams *prm)
{
	memset(job, 0, sizeof *job);
	job->book = book;
	ba_work_init(w, job, prm);
}

/** Counts of the comparisons of the stages. */
typedef struct PrecisionCount {
	long long moves, wrong, down, down_walked;
	long long saved[3][3], plain[3][3];     /**< walks of the ranges left out, and without taking over: base, order */
	long long best_saved[3], best_plain[3]; /**< walks of the best lines: order */
} PrecisionCount;

/**
 * @brief Compare the stages of the moves of the positions of a book, taken over (ba_move_stage, as book moveinfo
 * does), with the stages walked from the position after each move alone (ba_stage). The positions are taken in
 * the order given (the current position is known: the stages go down), backwards (the next positions are known:
 * up) and at random.
 */
static void precision_compare(Book *book, const Board *boards, const int n_board, const BaParams *prm, const char *name, PrecisionCount *c)
{
	const unsigned long long cond = ba_cond_stage(prm);
	int *ref_stage = (int*) malloc((size_t) ba.index.n * sizeof *ref_stage), *ref_walks = (int*) malloc((size_t) ba.index.n * sizeof *ref_walks);
	int *order = (int*) malloc((size_t) n_board * sizeof *order);
	long long *ref_missing = (long long*) malloc((size_t) ba.index.n * sizeof *ref_missing);
	BaJob job;
	BaWork w;
	int i, o;

	test_book_work(&w, &job, book, prm);
	for (i = 0; i < n_board; ++i) { // the stage of each position alone
		unsigned int idx;
		Position *p = ba_probe(&w, boards + i, &idx);
		const long long walks = ba_test_range_walks;
		ref_stage[idx] = ba_stage(&w, p, idx, ref_missing + idx);
		ref_walks[idx] = (int) (ba_test_range_walks - walks);
	}
	for (o = 0; o < 3; ++o) {
		test_order(order, n_board, o);
		free(ba_cache); ba_cache = NULL; // (the results are forgotten)
		for (i = 0; i < n_board; ++i) {
			BaMove move[MAX_MOVE + 1];
			BaStageInfo cur_info;
			Position *cur;
			unsigned int cur_idx;
			bool known;
			int n, k;
			memset(move, 0, sizeof move);
			job.board = boards[order[i]];
			job.player = BLACK;
			n = ba_moves(&job, prm, &w, move, &cur, &cur_idx);
			known = ba_cache_get_stage(cur_idx, cond, &cur_info);
			for (k = 0; k < n; ++k) {
				BaMove *m = move + k;
				BaStageInfo kept;
				const long long walks = ba_test_range_walks, best_walks = ba_test_best_walks;
				bool is_kept, down;
				if (m->p == NULL || m->same >= 0) continue;
				is_kept = ba_cache_get_stage(m->idx, cond, &kept);
				down =known && cur_info.stage == 2 + prm->n_stage && m->has_link && m->link == cur->score.value
				    && ba_walkable(book, cur) && ba_walkable(book, m->p)
				    && (prm->base != BA_DEVIATE || (m->p->score.value == -cur->score.value && !cur_info.recursive));
				ba_move_stage(&w, cur, &cur_info, known, m);
				++c->moves;
				if (m->stage != ref_stage[m->idx] || m->missing != ref_missing[m->idx]) {
					if (++c->wrong <= 10) printf("FAIL precision %s order %d: stage %d missing %lld, alone %d missing %lld\n",
						name, o, m->stage, m->missing, ref_stage[m->idx], ref_missing[m->idx]);
				}
				if (is_kept) continue; // (given again)
				c->plain[prm->base - 1][o] += ref_walks[m->idx];
				c->saved[prm->base - 1][o] += ref_walks[m->idx] - (ba_test_range_walks - walks);
				++c->best_plain[o];
				c->best_saved[o] += 1 - (ba_test_best_walks - best_walks);
				if (down && ref_stage[m->idx] >= 2) {
					++c->down;
					if (ba_test_range_walks != walks && ++c->down_walked <= 10) printf("FAIL precision %s order %d: a move of loss 0 from a complete position walked\n", name, o);
				}
			}
		}
	}
	ba_work_free(&w);
	free(ref_stage); free(ref_walks); free(order); free(ref_missing);
}

static void precision_print(const PrecisionCount *c, const long long t0)
{
	int base;
	printf("  %lld moves, %lld wrong; %lld moves of loss 0 from a complete position, %lld walked\n", c->moves, c->wrong, c->down, c->down_walked);
	for (base = 0; base < 3; ++base) printf("  %s: walks of the ranges left out (in order, backwards, at random): %lld/%lld, %lld/%lld, %lld/%lld\n",
		base == 0 ? "deviate " : base == 1 ? "deviate2" : "deviate3", c->saved[base][0], c->plain[base][0], c->saved[base][1], c->plain[base][1], c->saved[base][2], c->plain[base][2]);
	printf("  walks of the best lines left out: %lld/%lld, %lld/%lld, %lld/%lld; %.1f s\n", c->best_saved[0], c->best_plain[0], c->best_saved[1], c->best_plain[1], c->best_saved[2], c->best_plain[2], (real_clock() - t0) / 1000.0);
}

static const char *const precision_stages[] = {"1/1,2/2,3/3,4/4,5/5", "0/0,1/2,2/1,3/3", "2/4,1/1", "3/2,4/4,6/6", "0/1"};
#define PRECISION_STAGES ((int) (sizeof precision_stages / sizeof *precision_stages))

static bool precision_params(const int base, const char *stages, BaParams *prm, char *name, const int size)
{
	snprintf(name, (size_t) size, "base=%s stages=%s", base == BA_DEVIATE ? "deviate" : base == BA_DEVIATE2 ? "deviate2" : "deviate3", stages);
	return ba_params_read(name, prm);
}

/**
 * @brief The stages taken over in random books (see precision_compare; the positions in the order they were added,
 * by plies): the same as walked alone; the walks left out: none for the moves of loss 0 from a complete position
 * (deviate: same value), fewer than without taking over in all.
 */
static int precision(void)
{
	static Book book;
	PrecisionCount c;
	int t, base, si, i, n_solved = 0;
	const long long t0 = real_clock();

	memset(&c, 0, sizeof c);
	for (t = 0; t < 200; ++t) {
		tb_random_book(&book);
		for (i = 0; i < tb_n; ++i) if (position_is_solved(book_probe(&book, tb_board + i))) ++n_solved;
		for (base = BA_DEVIATE; base <= BA_DEVIATE3; ++base) for (si = 0; si < PRECISION_STAGES; ++si) {
			BaParams prm;
			char name[160], s[128];
			if (!precision_params(base, precision_stages[si], &prm, s, (int) sizeof s)) { printf("FAIL precision: %s\n", s); return 2; }
			snprintf(name, sizeof name, "book %d %s", t, s);
			precision_compare(&book, tb_board, tb_n, &prm, name, &c);
		}
		book_free(&book);
	}
	ba_index_free();
	printf("precision: 200 random books (%d solved positions)\n", n_solved);
	precision_print(&c, t0);
	if (c.down == 0 || c.saved[0][0] <= 0 || c.saved[1][0] <= 0 || c.saved[2][0] <= 0 || c.saved[1][1] <= 0 || c.saved[2][1] <= 0 || c.best_saved[1] <= 0) {
		printf("FAIL precision: no walk left out\n");
		return 2;
	}
	return c.wrong || c.down_walked ? 2 : 0;
}

/** @brief The stages taken over in a book file (see precision_compare; the positions in the order of the book). */
static int precision_book(const char *file, const char *stages)
{
	static Book book;
	PrecisionCount c;
	Board *boards;
	int base, n = 0, b, j;
	const long long t0 = real_clock();

	book_init(&book);
	if (!book_load(&book, file)) { printf("FAIL precision-book: cannot load %s\n", file); return 2; }
	boards = (Board*) malloc((size_t) book.n_nodes * sizeof *boards);
	for (b = 0; b < book.n; ++b) for (j = 0; j < book.array[b].n; ++j) boards[n++] = book.array[b].positions[j].board;
	ba_index_free();
	ba_index_get(&book);
	memset(&c, 0, sizeof c);
	for (base = BA_DEVIATE; base <= BA_DEVIATE3; ++base) {
		BaParams prm;
		char s[128];
		if (!precision_params(base, stages, &prm, s, (int) sizeof s)) { printf("FAIL precision-book: %s\n", s); return 2; }
		precision_compare(&book, boards, n, &prm, s, &c);
	}
	ba_index_free();
	printf("precision-book %s: %d positions, stages=%s\n", file, n, stages);
	precision_print(&c, t0);
	free(boards);
	book_free(&book);
	return c.wrong || c.down_walked ? 2 : 0;
}

/* ------------------------------------------------------------------ results taken over to the next positions ------------------------------ */

/** A position of a test and its side to move. */
typedef struct TestPos {
	Board board;
	int player;
} TestPos;

/** @brief Forget the kept results (as when the book changes); the numbering of the positions is kept. */
static void inherit_forget(void)
{
	free(ba_cache); ba_cache = NULL;
	free(ba_reached); ba_reached = NULL;
}

/** @brief The positions of a random book (see tb_build), the side to move by the number of discs. */
static TestPos* inherit_tb_positions(void)
{
	TestPos *pos = (TestPos*) malloc((size_t) tb_n * sizeof *pos);
	int i;
	for (i = 0; i < tb_n; ++i) {
		pos[i].board = tb_board[i];
		pos[i].player = (board_count_empties(tb_board + i) & 1) ? WHITE : BLACK;
	}
	return pos;
}

/** @brief The positions of the games of a file (one game a line: moves as "f5", "pa" for a pass), in the order of the games. */
static TestPos* inherit_game_positions(const char *file, int *n)
{
	FILE *f = fopen(file, "r");
	char line[1024];
	TestPos *pos = NULL;
	int size = 0;

	*n = 0;
	if (f == NULL) return NULL;
	while (fgets(line, sizeof line, f)) {
		Board board;
		int player = BLACK;
		const char *s = line;
		board_init(&board);
		for (;;) {
			int x;
			if (*n == size) { size = size * 2 + 64; pos = (TestPos*) realloc(pos, (size_t) size * sizeof *pos); }
			pos[*n].board = board;
			pos[*n].player = player;
			++*n;
			while (*s == ' ' || *s == '\t') ++s;
			x = string_to_coordinate(s);
			if (x == NOMOVE) break;
			if (x == PASS ? get_moves(board.player, board.opponent) != 0 : ((get_moves(board.player, board.opponent) >> x) & 1) == 0) break;
			{ Board next; board_next(&board, x, &next); board = next; }
			player = !player;
			s += 2;
		}
	}
	fclose(f);
	return pos;
}

/** Counts of the comparisons of the positions reached. */
typedef struct ReachCount {
	long long calls, walks, wrong;
	long long after, after_walked;  /**< book levels at a position counted as the position after a move */
} ReachCount;

/** @brief The positions reached from a position, kept (ba_reach_kept), compared with ba_reach from the position alone. @return the walks. */
static long long reach_check(BaWork *w, Position *p, const unsigned int idx, const char *name, ReachCount *c)
{
	long long hist[61], ref_hist[61], n, ref, walks = ba_test_reach_walks;
	n = ba_reach_kept(w, p, idx, hist);
	walks = ba_test_reach_walks - walks;
	ref = ba_reach(w, p, idx, ref_hist);
	++c->calls;
	c->walks += walks;
	if (n != ref || memcmp(hist, ref_hist, sizeof hist) != 0) {
		if (++c->wrong <= 10) printf("FAIL reach %s: %lld positions, alone %lld\n", name, n, ref);
	}
	return walks;
}

/**
 * @brief The positions reached, as the user interface asks for them at each position (book moveinfo parts=count for
 * the positions after the moves, then book levels), kept, compared with the positions reached from each position
 * alone. The positions are taken in the order given, backwards and at random.
 */
static void reach_compare(Book *book, const TestPos *pos, const int n_pos, const char *name, ReachCount *c)
{
	char *counted = (char*) malloc((size_t) ba.index.n);
	int *order = (int*) malloc((size_t) n_pos * sizeof *order);
	BaParams prm;
	BaJob job;
	BaWork w;
	int i, o;

	ba_params_default(&prm);
	test_book_work(&w, &job, book, &prm);
	for (o = 0; o < 3; ++o) {
		test_order(order, n_pos, o);
		inherit_forget();
		memset(counted, 0, (size_t) ba.index.n);
		for (i = 0; i < n_pos; ++i) {
			BaMove move[MAX_MOVE + 1];
			Position *cur;
			unsigned int cur_idx;
			int n, k;
			memset(move, 0, sizeof move);
			job.board = pos[order[i]].board;
			job.player = pos[order[i]].player;
			n = ba_moves(&job, &prm, &w, move, &cur, &cur_idx);
			for (k = 0; k < n; ++k) if (move[k].p) {
				reach_check(&w, move[k].p, move[k].idx, name, c);
				counted[move[k].idx] = 1;
			}
			if (cur) {
				long long kept_n, kept_hist[61];
				const bool after = counted[cur_idx] != 0 && ba_reached_get(cur_idx, &kept_n, kept_hist); // (not when the table was full and started again)
				if (reach_check(&w, cur, cur_idx, name, c) && after && ++c->after_walked <= 10) printf("FAIL reach %s: book levels walked a position counted\n", name);
				if (after) ++c->after;
			}
		}
	}
	ba_work_free(&w);
	free(counted); free(order);
}

static int reach_result(const ReachCount *c, const char *name, const long long t0)
{
	printf("%s: %lld counts, %lld wrong, %lld walked; book levels at a position counted: %lld, %lld walked; %.1f s\n",
		name, c->calls, c->wrong, c->walks, c->after, c->after_walked, (real_clock() - t0) / 1000.0);
	if (c->after == 0 || c->walks >= c->calls) { printf("FAIL %s: no walk left out\n", name); return 2; }
	return c->wrong || c->after_walked ? 2 : 0;
}

/** @brief The positions reached, kept, in random books (see reach_compare). */
static int reach(void)
{
	static Book book;
	ReachCount c;
	int t;
	const long long t0 = real_clock();

	memset(&c, 0, sizeof c);
	for (t = 0; t < 200; ++t) {
		TestPos *pos;
		char name[32];
		tb_random_book(&book);
		pos = inherit_tb_positions();
		snprintf(name, sizeof name, "book %d", t);
		reach_compare(&book, pos, tb_n, name, &c);
		free(pos);
		book_free(&book);
	}
	ba_index_free();
	return reach_result(&c, "reach (200 random books)", t0);
}

/** @brief Load a book file and the positions of the games of a file. */
static bool inherit_load(Book *book, const char *file, const char *games, TestPos **pos, int *n)
{
	book_init(book);
	if (!book_load(book, file)) { printf("FAIL: cannot load %s\n", file); return false; }
	*pos = inherit_game_positions(games, n);
	if (*pos == NULL) { printf("FAIL: cannot read %s\n", games); return false; }
	ba_index_free();
	ba_index_get(book);
	return true;
}

/** @brief Free what inherit_load loaded. */
static void inherit_unload(Book *book, TestPos *pos)
{
	ba_index_free();
	free(pos);
	book_free(book);
}

/** @brief The positions reached, kept, along the games of a file in a book file (see reach_compare). */
static int reach_book(const char *file, const char *games)
{
	static Book book;
	ReachCount c;
	TestPos *pos;
	int n, r;
	char name[64];
	const long long t0 = real_clock();

	if (!inherit_load(&book, file, games, &pos, &n)) return 2;
	memset(&c, 0, sizeof c);
	reach_compare(&book, pos, n, file, &c);
	snprintf(name, sizeof name, "reach-book (%d positions)", n);
	r = reach_result(&c, name, t0);
	inherit_unload(&book, pos);
	return r;
}

/** Counts of the comparisons of book missing. */
typedef struct MissingCount {
	long long asked, known, empty, wrong;
} MissingCount;

/**
 * @brief The walks of book missing known to find nothing by the kept stage of the position after a move
 * (ba_missing_complete), after the stages of the moves (ba_move_stage, as book moveinfo does), compared with the walks.
 * The losses asked: each range, its total loss - 1 and + 1, and 2/4.
 */
static void missing_compare(Book *book, const TestPos *pos, const int n_pos, const BaParams *prm, const char *name, MissingCount *c)
{
	const unsigned long long cond = ba_cond_stage(prm);
	int *order = (int*) malloc((size_t) n_pos * sizeof *order);
	BaJob job;
	BaWork w;
	int i, o;

	test_book_work(&w, &job, book, prm);
	for (o = 0; o < 3; ++o) {
		test_order(order, n_pos, o);
		inherit_forget();
		for (i = 0; i < n_pos; ++i) {
			BaMove move[MAX_MOVE + 1];
			BaStageInfo cur_info;
			Position *cur;
			unsigned int cur_idx;
			bool known;
			int n, k, q;
			memset(move, 0, sizeof move);
			job.board = pos[order[i]].board;
			job.player = pos[order[i]].player;
			n = ba_moves(&job, prm, &w, move, &cur, &cur_idx);
			known = cur && ba_cache_get_stage(cur_idx, cond, &cur_info);
			for (k = 0; k < n; ++k) {
				BaMove *m = move + k;
				if (m->p == NULL || m->same >= 0) continue;
				ba_move_stage(&w, cur, &cur_info, known, m);
				for (q = 0; q <= 3 * prm->n_stage; ++q) {
					const int x = q == 3 * prm->n_stage ? 2 : prm->stage_x[q / 3];
					const int y = q == 3 * prm->n_stage ? 4 : prm->stage_y[q / 3] + q % 3 - 1;
					bool complete;
					long long walked;
					if (y < 0) continue;
					complete = ba_missing_complete(prm, m->idx, x, y);
					walked = ba_expansions(&w, m->p, m->idx, prm->base, x, y, false);
					++c->asked;
					if (walked == 0) ++c->empty;
					if (complete) {
						++c->known;
						if (walked != 0 && ++c->wrong <= 10) printf("FAIL missing %s order %d: %d/%d known complete, %lld leaves\n", name, o, x, y, walked);
					}
				}
			}
		}
	}
	ba_work_free(&w);
	free(order);
}

static int missing_result(const MissingCount *c, const char *name, const long long t0)
{
	printf("%s: %lld walks asked, %lld find nothing, %lld known to find nothing, %lld wrong; %.1f s\n", name, c->asked, c->empty, c->known, c->wrong, (real_clock() - t0) / 1000.0);
	if (c->known == 0) { printf("FAIL %s: no walk left out\n", name); return 2; }
	return c->wrong ? 2 : 0;
}

/** @brief book missing known to find nothing, in random books (see missing_compare). */
static int missing(void)
{
	static Book book;
	MissingCount c;
	int t, base, si;
	const long long t0 = real_clock();

	memset(&c, 0, sizeof c);
	for (t = 0; t < 100; ++t) {
		TestPos *pos;
		tb_random_book(&book);
		pos = inherit_tb_positions();
		for (base = BA_DEVIATE; base <= BA_DEVIATE3; ++base) for (si = 0; si < PRECISION_STAGES; ++si) {
			BaParams prm;
			char name[160], s[128];
			if (!precision_params(base, precision_stages[si], &prm, s, (int) sizeof s)) { printf("FAIL missing: %s\n", s); return 2; }
			snprintf(name, sizeof name, "book %d %s", t, s);
			missing_compare(&book, pos, tb_n, &prm, name, &c);
		}
		free(pos);
		book_free(&book);
	}
	ba_index_free();
	return missing_result(&c, "missing (100 random books)", t0);
}

/** @brief book missing known to find nothing, along the games of a file in a book file (see missing_compare). */
static int missing_book(const char *file, const char *games)
{
	static Book book;
	MissingCount c;
	TestPos *pos;
	int n, r, base, si;
	char name[256];
	const long long t0 = real_clock();

	if (!inherit_load(&book, file, games, &pos, &n)) return 2;
	memset(&c, 0, sizeof c);
	for (base = BA_DEVIATE; base <= BA_DEVIATE3; ++base) for (si = 0; si < 3; si += 2) {
		BaParams prm;
		char s[128];
		if (!precision_params(base, precision_stages[si], &prm, s, (int) sizeof s)) { printf("FAIL missing-book: %s\n", s); return 2; }
		missing_compare(&book, pos, n, &prm, s, &c);
	}
	snprintf(name, sizeof name, "missing-book %s (%d positions)", file, n);
	r = missing_result(&c, name, t0);
	inherit_unload(&book, pos);
	return r;
}

/** The umigame numbers of the position after a move. */
typedef struct UmiResult {
	long long umi[2];
	int state;
	bool exact, known;
} UmiResult;

/** Ways to take the umigame numbers over: the values kept from the lines of a move, up to some moves after it (ba_umi_keep_depth; 2 in the engine). */
static const struct { int depth; const char *name; } umi_plans[] = {
	{0, "none"}, {1, "A1"}, {2, "A2"}, {3, "A3"}
};
#define UMI_PLANS ((int) (sizeof umi_plans / sizeof *umi_plans))

/** Counts of the comparisons of the umigame numbers, by way and order. */
typedef struct UmiCount {
	long long moves, wrong, better;   /**< better: exact taken over, inexact (or too many positions) alone */
	long long builds[UMI_PLANS][3], nodes[UMI_PLANS][3], keeps[UMI_PLANS][3];
} UmiCount;

/** @brief The umigame numbers of the position after a move, counted from it alone (as ba_move_umi without the kept numbers). */
static void umi_alone(BaWork *w, const int player, const BaMove *m, UmiResult *r)
{
	const int b = ba_umi_build(w, m->p, m->idx, !player, player);
	r->known = true;
	r->exact = true;
	r->umi[BLACK] = r->umi[WHITE] = 0;
	r->state = b == 1 ? 1 : b < 0 ? 2 : 0;
	if (b == 0) {
		r->umi[BLACK] = ba_umi_count(w, BLACK);
		r->exact = !w->inexact;
		ba_umi_clear(w);
		r->umi[WHITE] = r->umi[BLACK] >= 0 ? ba_umi_count(w, WHITE) : -1;
		r->exact = r->exact && !w->inexact;
		ba_umi_clear(w);
		if (r->umi[BLACK] < 0 || r->umi[WHITE] < 0) r->state = 2;
	}
	ba_umi_free(w);
}

/**
 * @brief The umigame numbers of the moves of the positions of a book, as book moveinfo gives them (ba_move_umi), compared with the numbers counted from each position after a move alone, for each way to take them
 * over; the positions in the order given (the next positions after), backwards and at random.
 */
static void umi_compare(Book *book, const TestPos *pos, const int n_pos, const BaParams *prm, const char *name, UmiCount *c)
{
	UmiResult *ref = (UmiResult*) calloc((size_t) ba.index.n * 2, sizeof *ref);
	int *order = (int*) malloc((size_t) n_pos * sizeof *order);
	const unsigned long long rng = rng_state;
	BaJob job;
	BaWork w, w_ref;
	int i, o, plan;

	test_book_work(&w, &job, book, prm);
	ba_work_init(&w_ref, &job, prm);
	for (plan = 0; plan < UMI_PLANS; ++plan) for (o = 0; o < 3; ++o) {
		ba_umi_keep_depth = umi_plans[plan].depth;
		rng_state = rng; // (the same random order for each way)
		test_order(order, n_pos, o);
		inherit_forget();
		for (i = 0; i < n_pos; ++i) {
			BaMove move[MAX_MOVE + 1];
			Position *cur;
			unsigned int cur_idx;
			const long long builds = ba_test_umi_builds, nodes = ba_test_umi_nodes, keeps = ba_test_umi_keeps;
			int n, k;
			memset(move, 0, sizeof move);
			job.board = pos[order[i]].board;
			job.player = pos[order[i]].player;
			n = ba_moves(&job, prm, &w, move, &cur, &cur_idx);
			for (k = 0; k < n; ++k) {
				BaMove *m = move + k;
				UmiResult *r;
				if (m->p == NULL || m->same >= 0) continue;
				ba_move_umi(&w, job.player, m);
				r = ref + 2 * (size_t) m->idx + !job.player;
				if (!r->known) {
					const long long b0 = ba_test_umi_builds, n0 = ba_test_umi_nodes, k0 = ba_test_umi_keeps;
					umi_alone(&w_ref, job.player, m, r);
					ba_test_umi_builds = b0; ba_test_umi_nodes = n0; ba_test_umi_keeps = k0;
				}
				++c->moves;
				if (m->umi_state == r->state && (r->state != 0 || (m->umi[0] == r->umi[0] && m->umi[1] == r->umi[1] && m->umi_exact == r->exact))) continue;
				if ((r->state == 2 || (r->state == 0 && !r->exact)) && m->umi_state == 0 && m->umi_exact
				 && (r->state == 2 || (m->umi[0] <= r->umi[0] && m->umi[1] <= r->umi[1]))) { ++c->better; continue; }
				if (++c->wrong <= 10) printf("FAIL umi %s %s order %d: %lld:%lld state %d%s, alone %lld:%lld state %d%s\n", name, umi_plans[plan].name, o,
					m->umi[0], m->umi[1], m->umi_state, m->umi_exact ? "" : " inexact", r->umi[0], r->umi[1], r->state, r->exact ? "" : " inexact");
			}
			c->builds[plan][o] += ba_test_umi_builds - builds;
			c->nodes[plan][o] += ba_test_umi_nodes - nodes;
			c->keeps[plan][o] += ba_test_umi_keeps - keeps;
		}
	}
	rng_state = rng;
	ba_umi_keep_depth = 2;
	ba_work_free(&w);
	ba_work_free(&w_ref);
	free(ref); free(order);
}

static int umi_result(const UmiCount *c, const char *name, const long long t0)
{
	int plan, bad = c->wrong != 0;
	printf("%s: %lld moves, %lld wrong, %lld better (exact taken over, inexact alone)\n", name, c->moves, c->wrong, c->better);
	for (plan = 0; plan < UMI_PLANS; ++plan) {
		printf("  %-4s lines found (in order, backwards, at random): %lld, %lld, %lld; their positions: %lld, %lld, %lld; values kept: %lld, %lld, %lld\n", umi_plans[plan].name,
			c->builds[plan][0], c->builds[plan][1], c->builds[plan][2], c->nodes[plan][0], c->nodes[plan][1], c->nodes[plan][2], c->keeps[plan][0], c->keeps[plan][1], c->keeps[plan][2]);
	}
	if (c->builds[2][0] >= c->builds[0][0]) { printf("FAIL %s: no lines left out (A2, in order)\n", name); bad = 1; }
	printf("  %.1f s\n", (real_clock() - t0) / 1000.0);
	return bad ? 2 : 0;
}

static const char *const umi_modes[] = {"umi=best", "umi=loss x=2 y=4", "umi=range lo=-4 hi=4", "umi=range lo=0 hi=64", "umi=best depth=8", "umi=loss x=1 y=3 depth=9"};
#define UMI_MODES ((int) (sizeof umi_modes / sizeof *umi_modes))

/** @brief The umigame numbers taken over in random books (see umi_compare), for each condition. */
static int umi_inherit(void)
{
	static Book book;
	UmiCount c[UMI_MODES];
	int t, mode, r = 0;
	const long long t0 = real_clock();

	memset(c, 0, sizeof c);
	for (t = 0; t < 100; ++t) {
		TestPos *pos;
		tb_random_book(&book);
		pos = inherit_tb_positions();
		for (mode = 0; mode < UMI_MODES; ++mode) {
			BaParams prm;
			char name[160];
			ba_params_default(&prm);
			if (!ba_params_read(umi_modes[mode], &prm)) { printf("FAIL umi-inherit: %s\n", umi_modes[mode]); return 2; }
			snprintf(name, sizeof name, "book %d %s", t, umi_modes[mode]);
			umi_compare(&book, pos, tb_n, &prm, name, c + mode);
		}
		free(pos);
		book_free(&book);
	}
	ba_index_free();
	for (mode = 0; mode < UMI_MODES; ++mode) {
		char name[160];
		snprintf(name, sizeof name, "umi-inherit (100 random books) %s", umi_modes[mode]);
		r |= umi_result(c + mode, name, t0);
	}
	return r;
}

/** @brief The umigame numbers taken over along the games of a file in a book file (see umi_compare). */
static int umi_book(const char *file, const char *games, const char *mode)
{
	static Book book;
	UmiCount c;
	BaParams prm;
	TestPos *pos;
	int n, r;
	char name[256];
	const long long t0 = real_clock();

	ba_params_default(&prm);
	if (!ba_params_read(mode, &prm)) { printf("FAIL umi-book: %s\n", mode); return 2; }
	if (!inherit_load(&book, file, games, &pos, &n)) return 2;
	memset(&c, 0, sizeof c);
	umi_compare(&book, pos, n, &prm, file, &c);
	snprintf(name, sizeof name, "umi-book %s (%d positions) %s", file, n, mode);
	r = umi_result(&c, name, t0);
	inherit_unload(&book, pos);
	return r;
}

/* ------------------------------------------------------------------ book commands typed without book ------------------------------------- */

/** @brief The hint of the warning when a book command is typed without book (edax.c, book_command_hint). */
static int bookhint(void)
{
	static const struct { const char *cmd, *param, *hint; } cases[] = {
		{"leaf", "expand 30 30", " (book command: type \"book leaf expand 30 30\" or \"b leaf expand 30 30\")"},
		{"leaf", "recalculate 30 30", " (book command: type \"book leaf recalculate 30 30\" or \"b leaf recalculate 30 30\")"},
		{"fill2", "2", " (book command: type \"book fill2 2\" or \"b fill2 2\")"},
		{"deviate3", "5 5", " (book command: type \"book deviate3 5 5\" or \"b deviate3 5 5\")"},
		{"moveinfo", "id=1", " (book command: type \"book moveinfo id=1\" or \"b moveinfo id=1\")"},
		{"store", "", " (book command: type \"book store\" or \"b store\")"},
		{"reduce", "2 4", " (book command: type \"book reduce 2 4\" or \"b reduce 2 4\")"},
		// top-level commands and options of the same name, base commands, others: no hint
		{"new", "x", ""}, {"load", "a.dat", ""}, {"open", "a.dat", ""}, {"save", "a.dat", ""}, {"play", "f5", ""},
		{"a", "", ""}, {"analyze", "", ""}, {"info", "", ""}, {"verbose", "", ""}, {"depth", "", ""},
		{"check", "", ""}, {"correct", "", ""}, {"problem", "", ""}, {"echo", "M1", ""}, {"f5", "", ""},
		{"leafexpand", "30 30", ""}, {"book", "leaf expand 30 30", ""},
	};
	int i, n_bad = 0;
	for (i = 0; i < (int) (sizeof cases / sizeof *cases); ++i) {
		char *hint = book_command_hint(cases[i].cmd, cases[i].param);
		if (strcmp(hint ? hint : "", cases[i].hint) != 0) {
			++n_bad;
			printf("FAIL bookhint \"%s %s\": \"%s\", wanted \"%s\"\n", cases[i].cmd, cases[i].param, hint ? hint : "", cases[i].hint);
		}
		free(hint);
	}
	printf("bookhint: %d cases, %d wrong\n", i, n_bad);
	return n_bad ? 2 : 0;
}

/* ------------------------------------------------------------------ analyses without memory ------------------------------------------------- */

/**
 * @brief book moveinfo when an allocation of a walk fails (ba_test_alloc_fail): the analysis ends with an error (as
 * ba_run tells it: @error memory=1), not with an @end that leaves out the lines of moves (up to v4.5.5-nikque.26: the
 * stage of ba_move_stage, the positions reached of ba_reach_kept).
 */
static int moveinfo_memory(void)
{
	static Book book;
	static const struct { int parts; const char *name; } part[] = {{BA_PART_STAGE, "parts=stage"}, {BA_PART_COUNT, "parts=count"}};
	int t, k, n_tried = 0, n_bad = 0;

	for (t = 0; t < 20; ++t) {
		tb_random_book(&book);
		for (k = 0; k < 2; ++k) {
			BaParams prm;
			BaJob job;
			int r0, r;
			bool failed;
			ba_params_default(&prm);
			prm.parts = part[k].parts;
			memset(&job, 0, sizeof job);
			job.book = &book;
			job.board = tb_board[0];
			job.player = BLACK;
			job.id = 1;
			inherit_forget();
			r0 = ba_moveinfo(&job, &prm); // (with memory)
			inherit_forget();
			ba_test_alloc_fail = 0; // the first allocation of a walk fails
			r = ba_moveinfo(&job, &prm);
			failed = ba_test_alloc_fail < 0;
			ba_test_alloc_fail = -1;
			if (r0 != 0) { ++n_bad; printf("FAIL moveinfo-memory book %d %s: %d with memory\n", t, part[k].name, r0); }
			if (!failed) continue; // (no walk: no move of the position is in the book)
			++n_tried;
			if (r >= 0) { ++n_bad; printf("FAIL moveinfo-memory book %d %s: an allocation failed, the analysis ended without an error\n", t, part[k].name); }
		}
		book_free(&book);
	}
	inherit_forget();
	ba_index_free();
	printf("moveinfo-memory: %d analyses with an allocation that failed, %d wrong\n", n_tried, n_bad);
	if (n_tried == 0) { printf("FAIL moveinfo-memory: no allocation failed\n"); return 2; }
	return n_bad ? 2 : 0;
}

/**
 * @brief ba_push refuses an item whose parent does not fit in BaItem.parent (an int), and the item 2^31 of a queue, the
 * parent of the next ones (up to v4.5.5-nikque.26 checked only when the queue grew: a parent above INT_MAX became
 * negative). The queue of 2^31 items is not made: its count is set, and nothing is written before the check.
 */
static int push_parent(void)
{
	BaQueue q = {NULL, 0, 0}, big;
	bool r;
	int n_bad = 0;

	if (!ba_push(&q, NULL, 0, -1, NOMOVE, 0, 0) || q.size < 3) { printf("FAIL push-parent: the first item\n"); return 2; }
	r = ba_push(&q, NULL, 1, INT_MAX, NOMOVE, 1, 0);
	printf("push-parent: an item of the parent INT_MAX %s\n", r ? "pushed" : "refused");
	if (!r || q.item[1].parent != INT_MAX) ++n_bad;
	r = ba_push(&q, NULL, 2, (long long) INT_MAX + 1, NOMOVE, 1, 0); // (the queue does not grow)
	printf("push-parent: an item of the parent INT_MAX + 1 %s%s\n", r ? "pushed, parent " : "refused", r ? (q.item[2].parent < 0 ? "negative" : "kept") : "");
	if (r) ++n_bad;
	free(q.item);
	if (n_bad) return 2; // (before the next check: without it, the item would be written out of the queue)
	big.item = NULL;
	big.n = (long long) INT_MAX + 1;
	big.size = big.n + 1;
	r = ba_push(&big, NULL, 0, 0, NOMOVE, 1, 0);
	printf("push-parent: the item INT_MAX + 1 of a queue %s\n", r ? "pushed" : "refused");
	return r ? 2 : 0;
}

/**
 * @brief The positions counted by book reducecount, kept for book reduce (ba_counted), give their memory to a walk of
 * another analysis that needs it (ba_dense_reserve), and are forgotten (up to v4.5.5-nikque.26 they kept it until the
 * book changed, and the walk took its marks in a hash table).
 */
static int reducecount_memory(void)
{
	static Book book;
	const bool reuse = options.book_reduce_reuse;
	int t, n_tried = 0, n_bad = 0;

	options.book_reduce_reuse = true;
	for (t = 0; t < 20 && n_tried < 5; ++t) {
		BaParams prm;
		BaJob job;
		unsigned long long held, want;
		bool ok;
		tb_build(&book, 9 + (int) rnd(2), 3); // (a book of thousands of positions, all counted: marks of one byte a position)
		ba_index_free();
		ba_index_get(&book);
		ba_params_default(&prm);
		prm.x = 129;
		prm.y = 254;
		memset(&job, 0, sizeof job);
		job.book = &book;
		job.board = tb_board[0];
		job.player = BLACK;
		job.id = 1;
		job.kind = BA_REDUCECOUNT;
		if (ba_reducecount(&job, &prm) == 0 && ba_counted.valid) {
			held = (ba_counted.kept.dense ? ba_counted.kept.n_index : 0) + (ba_counted.val.dense ? ba_counted.val.n_index : 0);
			if (held > 0) { // (marks in hash tables hold no memory reserved for the dense marks)
				++n_tried;
				want = 6 * ba.index.n + (1u << 20) - (unsigned long long) ba.dense_bytes + 1; // (more than is left while the count is kept)
				ok = ba_dense_reserve(want);
				printf("reducecount-memory book %d: the count holds %llu bytes; %llu bytes for a walk %s, count %s\n", t, held, want,
					ok ? "given" : "refused", ba_counted.valid ? "kept" : "forgotten");
				if (!ok || ba_counted.valid || (unsigned long long) ba.dense_bytes != want) ++n_bad;
				if (ok) ba_dense_release(want);
			}
		}
		ba_counted_free();
		if (ba.dense_bytes != 0) { ++n_bad; printf("FAIL reducecount-memory book %d: %lld bytes left reserved\n", t, ba.dense_bytes); ba.dense_bytes = 0; }
		book_free(&book);
	}
	options.book_reduce_reuse = reuse;
	inherit_forget();
	ba_index_free();
	printf("reducecount-memory: %d counts that held dense marks, %d wrong\n", n_tried, n_bad);
	if (n_tried == 0) { printf("FAIL reducecount-memory: no count held dense marks\n"); return 2; }
	return n_bad ? 2 : 0;
}

/** @brief The tables of the search, and one thread (the tests on books). */
static void test_search_init(void)
{
	bit_init(); edge_stability_init(); search_global_init();
	options.n_task = 1;
}

int main(int argc, char **argv)
{
	setbuf(stdout, NULL);
	if (argc < 2) return 1;
	if (strcmp(argv[1], "umigame") == 0) return umigame();
	if (strcmp(argv[1], "umigame-large") == 0) return umigame_large();
	if (strcmp(argv[1], "umigame-order") == 0) return umigame_order();
	if (strcmp(argv[1], "bookhint") == 0) return bookhint();
	if (strcmp(argv[1], "precision") == 0) { test_search_init(); return precision(); }
	if (strcmp(argv[1], "precision-book") == 0 && argc >= 3) { test_search_init(); return precision_book(argv[2], argc >= 4 ? argv[3] : precision_stages[0]); }
	if (strcmp(argv[1], "reach") == 0) { test_search_init(); return reach(); }
	if (strcmp(argv[1], "reach-book") == 0 && argc >= 4) { test_search_init(); return reach_book(argv[2], argv[3]); }
	if (strcmp(argv[1], "umi-inherit") == 0) { test_search_init(); return umi_inherit(); }
	if (strcmp(argv[1], "umi-book") == 0 && argc >= 4) { test_search_init(); return umi_book(argv[2], argv[3], argc >= 5 ? argv[4] : "umi=best"); }
	if (strcmp(argv[1], "missing") == 0) { test_search_init(); return missing(); }
	if (strcmp(argv[1], "missing-book") == 0 && argc >= 4) { test_search_init(); return missing_book(argv[2], argv[3]); }
	if (strcmp(argv[1], "moveinfo-memory") == 0) { test_search_init(); return moveinfo_memory(); }
	if (strcmp(argv[1], "push-parent") == 0) return push_parent();
	if (strcmp(argv[1], "reducecount-memory") == 0) { test_search_init(); return reducecount_memory(); }
	return 1;
}
