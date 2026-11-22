/*
 * Tests of the commands that learn by rounds (book play, book play <n>, book extend, book exhaust).
 *
 * A plain reference (one thread, one position after the other, every child looked up in the list by brute force)
 * is compared with the commands: positions, links, leaves, values, and the number of searches. Both are built
 * with BOOK_TEST_ISOLATE (every search starts with empty hash tables), and run with book-reproducible on.
 *
 * Build: as tests/regression; run from a folder for temporary files, with EDAX_EVAL_FILE set:
 *   book-learn <case>
 */
#define BOOK_TEST_ISOLATE
#define BOOK_TEST_NODES
#define BOOK_TEST_COUNTS
#define main edax_original_main
#ifdef __cplusplus
#include "../src/all.cpp"
#else
#include "../src/all.c"
#endif
#undef main

static int n_check = 0, n_fail = 0;
#define CHECK(cond, ...) do { ++n_check; if (!(cond)) { ++n_fail; printf("FAIL %d: ", __LINE__); printf(__VA_ARGS__); putchar('\n'); } } while (0)

static unsigned rng = 0x651197;
static unsigned rr(void) { rng ^= rng << 13; rng ^= rng >> 17; return rng ^= rng << 5; }
static int threads = 4;
static Search main_search;

static void learn_engine_init(void)
{
	const char *eval = getenv("EDAX_EVAL_FILE");

	options.n_task = threads;
	options.hash_table_size = 10; // the searches of the expansions and of the plans then have the same size
	options.hash_table_auto = false;
	options.verbosity = 0;
	options.cpu_affinity = false;
	options.book_reproducible = true;
	options.book_expand_tasks = 0;
	options.book_save_interval = 0;
	options.level = 2;
	options.book_file = (char*) "learn-test.dat";
	book_verbose = false;
	eval_open(eval ? eval : "data/eval.dat");
	search_global_init();
	search_init(&main_search);
}

static void set_threads(const int n)
{
	book_store_release();
	search_pool_release(&expand_pool);
	options.n_task = n;
}

static Board random_board(const int n_empties)
{
	Board b;

	board_init(&b);
	while (board_count_empties(&b) > n_empties) {
		unsigned long long m = board_get_moves(&b);
		int k;

		if (!m) {
			board_pass(&b);
			if (!board_get_moves(&b)) { board_init(&b); continue; }
			continue;
		}
		for (k = rr() % bit_count(m); k--; ) m &= m - 1;
		board_next(&b, first_bit(m), &b);
	}
	return b;
}

/* ---- books ---- */

static void book_start(Book *b, const int level, const int n_empties)
{
	memset(b, 0, sizeof *b);
	b->search = &main_search;
	book_new(b, level, n_empties);
	book_clean(b);
}

static void book_bare(Book *b, const int level, const int n_empties)
{
	memset(b, 0, sizeof *b);
	b->search = &main_search;
	book_init(b);
	b->options.level = level;
	b->options.n_empties = n_empties;
	book_clean(b);
}

static void book_copy(Book *dest, const Book *src)
{
	PositionArray *a;
	Position *p, q;

	book_bare(dest, src->options.level, src->options.n_empties);
	dest->options.midgame_error = src->options.midgame_error;
	dest->options.endcut_error = src->options.endcut_error;
	foreach_position(p, a, src) {
		position_copy(&q, p);
		if (book_add(dest, &q) <= 0) position_free(&q);
	}
}

static bool link_in(const Position *q, const Link *l, const bool scores)
{
	const Link *m;

	foreach_link(m, q) if (m->move == l->move && (!scores || m->score == l->score)) return true;
	return false;
}

static int book_differences(const Book *x, const Book *y, const char *what)
{
	PositionArray *a;
	Position *p;
	int n = 0;

	if (x->n_nodes != y->n_nodes) { printf("  %s: %u positions against %u\n", what, x->n_nodes, y->n_nodes); ++n; }
	foreach_position(p, a, x) {
		const Position *q = book_probe(y, &p->board);
		const Link *l;
		const char *why = NULL;

		if (q == NULL) why = "position";
		else if (p->n_link != q->n_link) why = "number of links";
		else if (p->leaf.move != q->leaf.move || p->leaf.score != q->leaf.score) why = "leaf";
		else if (p->score.value != q->score.value || p->score.lower != q->score.lower || p->score.upper != q->score.upper) why = "values";
		else if (p->level != q->level) why = "level";
		else foreach_link(l, p) if (!link_in(q, l, true)) why = "link";
		if (why && n++ < 5) printf("  %s: %s differs at %016llx %016llx\n", what, why, p->board.player, p->board.opponent);
	}
	return n;
}

/* no position without a score, no link to a position out of the book, no leaf that is a link */
static int book_unsound(const Book *b)
{
	PositionArray *a;
	Position *p;
	int n = 0;

	foreach_position(p, a, b) {
		const Link *l;

		if (!position_check(p, false)) ++n;
		foreach_link(l, p) {
			Board next;

			if (l->move == PASS) { next.player = p->board.opponent; next.opponent = p->board.player; }
			else board_next(&p->board, l->move, &next);
			if (book_probe(b, &next) == NULL) ++n;
			if (l->move == p->leaf.move) ++n;
		}
	}
	return n;
}

static bool linkless(const Book *b, const Position *p, const int limit)
{
	return p->n_link == 0 && board_count_empties(&p->board) >= limit && !board_is_game_over(&p->board);
}

/* ---- the reference ---- */

/* one round, the plain way. @return positions and links added */
static long long ref_round(Book *b, const unsigned long long *keys, const long long n, long long *n_shared)
{
	Board *child = (Board*) malloc((size_t) (n + 1) * sizeof *child);
	bool *expand = (bool*) malloc((size_t) (n + 1) * sizeof *expand);
	long long i, j;

	book_clean(b);
	for (i = 0; i < n; ++i) {
		const Position *p = book_position(b, keys[i]);
		Board next;

		expand[i] = false;
		if (p->leaf.move == NOMOVE) continue;
		next = position_leaf_board(p);
		board_unique(&next, child + i);
		expand[i] = book_probe(b, child + i) == NULL;
		for (j = 0; j < i && expand[i]; ++j) {
			if (book_position(b, keys[j])->leaf.move != NOMOVE && board_equal(child + j, child + i)) {
				expand[i] = false;
				if (n_shared) ++*n_shared;
			}
		}
	}
	for (i = 0; i < n; ++i) if (expand[i]) position_expand(book_position(b, keys[i]), b);
	for (i = 0; i < n; ++i) {
		Position *p = book_position(b, keys[i]);
		const int x = p->leaf.move;

		if (expand[i] || x == NOMOVE) continue;
		position_link(p, b);
		if (p->leaf.move == NOMOVE) position_search(p, b);
	}
	free(child); free(expand);
	return b->stats.n_nodes + b->stats.n_links;
}

static void ref_play(Book *b, long long *n_shared)
{
	for (;;) {
		unsigned long long *keys = (unsigned long long*) malloc((b->n_nodes + 1) * sizeof *keys);
		PositionArray *a;
		Position *p;
		long long n = 0, diffs;

		foreach_position(p, a, b) if (linkless(b, p, b->options.n_empties)) keys[n++] = position_key(b, p);
		diffs = ref_round(b, keys, n, n_shared);
		free(keys);
		if (!diffs) break;
	}
	book_negamax(b);
}

/* mark (done) what the links reach from a board, by recursion */
static void ref_reach(Book *b, const Board *board, const int limit)
{
	Position *p = book_probe(b, board);
	const Link *l;

	if (p == NULL || position_is_done(p, b) || board_count_empties(board) < limit || board_is_game_over(board)) return;
	position_set_done(p, b);
	foreach_link(l, p) {
		Board next;

		if (l->move == PASS) { next.player = p->board.opponent; next.opponent = p->board.player; }
		else board_next(&p->board, l->move, &next);
		ref_reach(b, &next, limit);
	}
}

static bool ref_extendable(const Position *p)
{
	const Link *l;

	if (p->leaf.move == NOMOVE) return false;
	foreach_link(l, p) if (l->score > p->leaf.score) return false;
	return true;
}

static void ref_extend(Book *b, const Board *start)
{
	for (;;) {
		unsigned long long *keys;
		PositionArray *a;
		Position *p;
		long long n = 0, diffs;

		book_negamax_from(b, start);
		book_clean(b);
		ref_reach(b, start, b->options.n_empties);
		keys = (unsigned long long*) malloc((b->n_nodes + 1) * sizeof *keys);
		foreach_position(p, a, b) if (position_is_done(p, b) && ref_extendable(p)) keys[n++] = position_key(b, p);
		diffs = ref_round(b, keys, n, NULL);
		free(keys);
		if (!diffs) break;
	}
	book_negamax_from(b, start);
}

/* ---- the cases ---- */

typedef void (*Command)(Book*, const void*);

static void run_play(Book *b, const void *arg) { (void) arg; book_play(b); }
static void run_extend(Book *b, const void *arg) { book_extend(b, (const Board*) arg); }

/* the board after the first moves */
static Board start_board(const int plies)
{
	Board b;
	int i;

	board_init(&b);
	for (i = 0; i < plies; ++i) board_next(&b, first_bit(board_get_moves(&b)), &b);
	return b;
}

/*
 * The same command with 4, 2 and 1 threads against the reference: the same book, as many searches.
 * @param same_searches The reference does not search more nor less.
 */
static void against_reference(const Book *seed, Book *ref, const long long ref_searches, Command run, const void *arg, const char *what)
{
	static const int n_tasks[] = {4, 2, 1};
	int k;

	for (k = 0; k < 3; ++k) {
		Book b;

		book_copy(&b, seed);
		set_threads(n_tasks[k]);
		book_test_searches = 0;
		book_test_serial = book_test_parallel = 0;
		book_test_expand_redo = 0;
		run(&b, arg);
		if (n_tasks[k] > 1) CHECK(book_test_serial == 0, "%s, %d threads: %lld rounds expanded one after the other", what, n_tasks[k], book_test_serial);
		if (n_tasks[k] > 1) CHECK(book_test_parallel > 0, "%s, %d threads: nothing was expanded at the same time", what, n_tasks[k]);
		/* (book-reproducible searches an expansion again, 2 searches, when an earlier one of the list changed its links) */
		CHECK(book_test_searches >= ref_searches && book_test_searches <= ref_searches + 2 * book_test_expand_redo, "%s, %d threads: %lld searches, the reference %lld, %lld searched again", what, n_tasks[k], book_test_searches, ref_searches, book_test_expand_redo);
		if (n_tasks[k] == 1) CHECK(book_test_expand_redo == 0, "%s, 1 thread: searched again", what);
		if (book_test_expand_redo) printf("%s, %d threads: %lld searches more than the reference, %lld expansions searched again", what, n_tasks[k], book_test_searches - ref_searches, book_test_expand_redo);
		if (book_test_expand_redo) putchar(10);
		CHECK(book_differences(&b, ref, what) == 0, "%s, %d threads: not the book of the reference", what, n_tasks[k]);
		CHECK(book_unsound(&b) == 0, "%s, %d threads: the book is not sound", what, n_tasks[k]);
		CHECK(!b.failed, "%s, %d threads: failed", what, n_tasks[k]);
		book_free(&b);
	}
}

static void seed_book(Book *b, const int level, const int n_empties, const int deeper)
{
	Board start;

	board_init(&start);
	set_threads(1);
	book_start(b, level, n_empties);
	book_deviate(b, &start, 4, 8);
	b->options.n_empties = deeper;
	book_negamax(b);
}

static void test_play(void)
{
	static const int shapes[][3] = {{2, 55, 52}, {4, 56, 50}, {1, 54, 51}};
	int k;

	for (k = 0; k < 3; ++k) {
		Book seed, ref;
		PositionArray *a;
		Position *p;
		long long n_shared = 0, searches;
		int left = 0;
		const Book *rp;

		seed_book(&seed, shapes[k][0], shapes[k][1], shapes[k][2]);
		book_copy(&ref, &seed);
		book_test_searches = 0;
		ref_play(&ref, &n_shared);
		searches = book_test_searches;
		CHECK(ref.n_nodes > seed.n_nodes, "book play %d: nothing was added", k);
		rp = &ref;
		foreach_position(p, a, rp) if (linkless(rp, p, rp->options.n_empties) && p->leaf.move != NOMOVE) ++left;
		CHECK(left == 0, "book play %d: %d positions left without a link", k, left);
		printf("book play %d: %u -> %u positions, %lld searches, %lld shared children\n", k, seed.n_nodes, ref.n_nodes, searches, n_shared);
		against_reference(&seed, &ref, searches, run_play, NULL, "book play");
		{
			Book b;

			/* one look at the whole book, one negamax, one save */
			book_copy(&b, &seed);
			set_threads(4);
			book_test_scans = book_test_negamaxes = book_test_saves = 0;
			book_play(&b);
			CHECK(book_test_scans == 1, "book play: %lld scans of the book", book_test_scans);
			CHECK(book_test_negamaxes == 1, "book play: %lld negamax", book_test_negamaxes);
			CHECK(book_test_saves == 1, "book play: %lld saves", book_test_saves);
			book_free(&b);

			/* a save at every chance, inside the rounds and between them: the same book */
			book_copy(&b, &seed);
			book_test_saves = 0;
			book_test_save_ms = 0;
			book_play(&b);
			book_test_save_ms = -1;
			CHECK(book_test_saves > 1, "book play: no timed save");
			CHECK(book_differences(&b, &ref, "book play with timed saves") == 0, "book play: the timed saves changed the book");
			book_free(&b);
		}
		book_free(&ref);
		book_free(&seed);
	}
}

/* two positions (not symmetrical to each other) whose moves x and y lead to the same position */
static bool shared_child(const int n_empties, Board *p1, int *x1, Board *p2, int *x2, Board *child)
{
	int tries;

	for (tries = 0; tries < 2000; ++tries) {
		static Board parent[20000], next[20000];
		static int move[20000];
		const Board root = random_board(n_empties + 2);
		unsigned long long m0 = board_get_moves(&root), m1, m2;
		int a, b, c, i, j, n = 0;

		while (m0) {
			Board u, v;

			a = first_bit(m0); m0 &= m0 - 1;
			board_next(&root, a, &u);
			for (m1 = board_get_moves(&u); m1; m1 &= m1 - 1) {
				b = first_bit(m1);
				board_next(&u, b, &v);
				board_unique(&v, &v);
				for (m2 = board_get_moves(&v); m2 && n < 20000; m2 &= m2 - 1) {
					c = first_bit(m2);
					parent[n] = v; move[n] = c;
					board_next(&v, c, next + n);
					board_unique(next + n, next + n);
					++n;
				}
			}
		}
		for (i = 0; i < n; ++i) for (j = i + 1; j < n; ++j) {
			if (board_equal(next + i, next + j) && !board_equal(parent + i, parent + j)) {
				*p1 = parent[i]; *x1 = move[i];
				*p2 = parent[j]; *x2 = move[j];
				*child = next[i];
				return true;
			}
		}
	}
	return false;
}

static void add_position(Book *b, const Board *unique, const int move)
{
	Position p;

	position_init(&p);
	p.board = *unique;
	p.level = b->options.level;
	p.leaf.move = (unsigned char) move;
	p.leaf.score = 0;
	p.score.value = 0;
	book_add(b, &p);
}

/* T3 and T4 */
static void test_shared(void)
{
	Board p1, p2, child;
	int x1, x2, exists;

	if (!shared_child(22, &p1, &x1, &p2, &x2, &child)) { CHECK(false, "no shared child found"); return; }
	for (exists = 0; exists < 2; ++exists) {
		Book b;
		const Position *c, *q1, *q2;

		book_bare(&b, 2, 22); // (the children are out of the depth: the rounds end there)
		add_position(&b, &p1, x1);
		add_position(&b, &p2, x2);
		if (exists) {
			Position p;

			position_init(&p);
			p.board = child;
			p.level = 2;
			position_search(&p, &b);
			book_add(&b, &p);
		}
		book_test_searches = 0;
		book_play(&b);
		c = book_probe(&b, &child); q1 = book_probe(&b, &p1); q2 = book_probe(&b, &p2);
		CHECK(b.n_nodes == 3, "shared child (exists=%d): %u positions", exists, b.n_nodes);
		CHECK(c && q1 && q2 && q1->n_link >= 1 && q2->n_link >= 1, "shared child (exists=%d): a parent is not linked", exists);
		CHECK(book_test_searches <= (exists ? 2 : 3), "shared child (exists=%d): %lld searches", exists, book_test_searches);
		CHECK(book_unsound(&b) == 0, "shared child (exists=%d): not sound", exists);
		book_free(&b);
	}
}

static void test_extend(void)
{
	static const int shapes[][4] = {{2, 55, 53, 0}, {3, 56, 52, 0}, {2, 55, 51, 1}};
	int k;

	for (k = 0; k < 3; ++k) {
		const Board start = start_board(shapes[k][3]);
		Book seed, ref;
		const Book *rp = &ref;
		PositionArray *a;
		Position *p;
		long long searches;
		int left = 0;

		seed_book(&seed, shapes[k][0], shapes[k][1], shapes[k][2]);
		book_copy(&ref, &seed);
		book_test_searches = 0;
		ref_extend(&ref, &start);
		searches = book_test_searches;
		CHECK(ref.n_nodes > seed.n_nodes, "book extend %d: nothing was added", k);
		book_clean(&ref);
		ref_reach(&ref, &start, ref.options.n_empties);
		foreach_position(p, a, rp) if (position_is_done(p, rp) && ref_extendable(p)) ++left;
		CHECK(left == 0, "book extend %d: %d leaves left", k, left);
		printf("book extend %d: %u -> %u positions, %lld searches\n", k, seed.n_nodes, ref.n_nodes, searches);
		against_reference(&seed, &ref, searches, run_extend, &start, "book extend");
		book_free(&ref);
		book_free(&seed);
	}
}

/* T4: a link that leads out of the book is skipped */
static void test_dangling(void)
{
	Board start;
	Book b;
	Position *root;

	board_init(&start);
	seed_book(&b, 2, 56, 54);
	root = book_probe(&b, &start);
	if (root && root->n_link) {
		PositionArray *a;
		Board next, unique;
		int i;

		board_next(&root->board, position_links(root)[0].move, &next);
		board_unique(&next, &unique);
		a = book_array(&b, &unique);
		for (i = 0; i < a->n; ++i) {
			if (board_equal(&a->positions[i].board, &unique)) {
				book_remove_at(&b, a, i);
				break;
			}
		}
		book_extend(&b, &start);
		CHECK(!b.failed, "dangling link: failed");
		CHECK(book_probe(&b, &start) != NULL, "dangling link: the root is gone");
	} else {
		CHECK(false, "dangling link: no root");
	}
	book_free(&b);
}

int main(int argc, char **argv)
{
	const char *t = argc > 1 ? argv[1] : "all";
	const bool all = strcmp(t, "all") == 0;

	setbuf(stdout, NULL);
	bit_init();
	edge_stability_init();
	learn_engine_init();
	if (all || !strcmp(t, "play")) test_play();
	if (all || !strcmp(t, "shared")) test_shared();
	if (all || !strcmp(t, "extend")) test_extend();
	if (all || !strcmp(t, "dangling")) test_dangling();
	printf("book-learn: %d checks, %d failures\n", n_check, n_fail);
	return (n_check && !n_fail) ? 0 : 1;
}
