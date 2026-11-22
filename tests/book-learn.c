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

typedef struct PlayToArg {
	Board start;
	int n_empties;
} PlayToArg;

static void ref_play_to(Book *b, const PlayToArg *arg)
{
	const int limit = MAX(b->options.n_empties, arg->n_empties);
	unsigned long long *keys = (unsigned long long*) malloc((b->n_nodes + 1) * sizeof *keys);
	PositionArray *a;
	Position *p;
	long long n = 0, i, j;

	book_clean(b);
	ref_reach(b, &arg->start, limit);
	foreach_position(p, a, b) if (position_is_done(p, b) && linkless(b, p, limit) && p->leaf.move != NOMOVE) keys[n++] = position_key(b, p);
	while (n) {
		Board *child = (Board*) malloc((size_t) n * sizeof *child);
		long long m = 0;

		for (i = 0; i < n; ++i) {
			Board next = position_leaf_board(book_position(b, keys[i]));

			board_unique(&next, child + i);
		}
		if (!ref_round(b, keys, n, NULL)) { free(child); break; }
		for (i = 0; i < n; ++i) {
			const Position *q = book_probe(b, child + i);
			unsigned long long key;

			if (q == NULL || q->n_link || q->leaf.move == NOMOVE) continue;
			if (board_count_empties(child + i) < arg->n_empties || board_is_game_over(child + i)) continue;
			key = position_key(b, q);
			for (j = m; j > 0 && keys[j - 1] > key; --j) ;
			if (j > 0 && keys[j - 1] == key) continue;
			memmove(keys + j + 1, keys + j, (size_t) (m - j) * sizeof *keys);
			keys[j] = key;
			++m;
		}
		free(child);
		n = m;
	}
	free(keys);
	book_negamax_from(b, &arg->start);
}

/* ---- book exhaust: the boards by plain recursion, the searches and the values by their definitions ---- */

#define REF_SET_SIZE (1u << 20)
static Board ref_set[REF_SET_SIZE];
static bool ref_set_used[REF_SET_SIZE];
static unsigned int ref_set_n;

static bool ref_set_add(const Board *unique)
{
	unsigned int i = (unsigned int) ((unique->player * 0x9E3779B97F4A7C15ULL ^ unique->opponent * 0xC2B2AE3D27D4EB4FULL) >> 40) & (REF_SET_SIZE - 1);

	while (ref_set_used[i]) {
		if (board_equal(ref_set + i, unique)) return false;
		i = (i + 1) & (REF_SET_SIZE - 1);
	}
	ref_set_used[i] = true;
	ref_set[i] = *unique;
	++ref_set_n;
	return true;
}

static void ref_exhaust_boards(const Board *board, const int limit)
{
	Board unique, next;
	unsigned long long moves;

	if (ref_set_n >= REF_SET_SIZE / 2) return;
	board_unique(board, &unique);
	if (!ref_set_add(&unique)) return;
	if (board_count_empties(board) < limit) return;
	moves = board_get_moves(board);
	if (moves) {
		for (; moves; moves &= moves - 1) {
			board_next(board, first_bit(moves), &next);
			ref_exhaust_boards(&next, limit);
		}
	} else if (can_move(board->opponent, board->player)) {
		next.player = board->opponent;
		next.opponent = board->player;
		ref_exhaust_boards(&next, limit);
	}
}

/* ---- the cases ---- */

typedef void (*Command)(Book*, const void*);

static void run_play(Book *b, const void *arg) { (void) arg; book_play(b); }
static void run_play_to(Book *b, const void *arg) { const PlayToArg *a = (const PlayToArg*) arg; book_play_to(b, &a->start, a->n_empties); }
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

static bool copy_file(const char *from, const char *to)
{
	FILE *in = fopen(from, "rb"), *out;
	int c;

	if (in == NULL) return false;
	if ((out = fopen(to, "wb")) == NULL) { fclose(in); return false; }
	while ((c = fgetc(in)) != EOF) fputc(c, out);
	fclose(in);
	return fclose(out) == 0;
}

/* what is left on the disk when a run is stopped just after a timed save */
static int n_snapshot = 0;
static void take_snapshot(const char *file)
{
	char name[128];

	if (n_snapshot >= 400) return;
	sprintf(name, "snap-%d.play", n_snapshot);
	if (!copy_file(file, name)) return;
	sprintf(name, "snap-%d.play-to", n_snapshot);
	if (!copy_file("learn-test.dat.play-to", name)) return;
	++n_snapshot;
}

/* T8: from every save of a run, a new run ends with the positions of the run that was not stopped */
static void test_resume(const Book *seed, const Book *ref, const PlayToArg *arg)
{
	Book b;
	int k, n_same = 0, n_run = 0;

	book_copy(&b, seed);
	set_threads(4);
	n_snapshot = 0;
	remove("learn-test.dat.play-to");
	book_test_save_ms = 0;
	book_test_saved = take_snapshot;
	book_play_to(&b, &arg->start, arg->n_empties);
	book_test_save_ms = -1;
	book_test_saved = NULL;
	CHECK(book_differences(&b, ref, "book play <n> with timed saves") == 0, "book play <n>: the timed saves changed the book");
	book_free(&b);
	CHECK(n_snapshot > 2, "book play <n>: %d snapshots", n_snapshot);
	{
		FILE *f = fopen("learn-test.dat.play-to", "r");

		CHECK(f == NULL, "book play <n>: the file of the ends is left");
		if (f) fclose(f);
	}

	for (k = 0; k < n_snapshot; k += MAX(1, n_snapshot / 12)) {
		char name[128];
		PositionArray *a;
		Position *p;
		const Book *bp = &b;
		int left = 0, missing = 0;

		sprintf(name, "snap-%d.play-to", k);
		if (!copy_file(name, "learn-test.dat.play-to")) continue;
		sprintf(name, "snap-%d.play", k);
		memset(&b, 0, sizeof b);
		b.search = &main_search;
		if (!book_load(&b, name)) { CHECK(false, "cannot load %s", name); continue; }
		book_play_to(&b, &arg->start, arg->n_empties);
		++n_run;
		foreach_position(p, a, bp) {
			const Position *r = book_probe(ref, &p->board);

			if (r == NULL) ++missing;
			else if (r->n_link > 0 && p->n_link == 0) ++left;
		}
		if (b.n_nodes == ref->n_nodes && missing == 0) ++n_same;
		CHECK(left == 0, "book play <n> from the save %d: %d lines were not played on", k, left);
		CHECK(b.n_nodes == ref->n_nodes && missing == 0, "book play <n> from the save %d: %u positions (%d others), the run that was not stopped %u", k, b.n_nodes, missing, ref->n_nodes);
		book_free(&b);
	}
	remove("learn-test.dat.play-to");
	printf("book play <n> stopped at %d saves: %d of the %d new runs end with the same positions\n", n_snapshot, n_same, n_run);
}

static void test_play_to(void)
{
	static const int shapes[][5] = {{2, 56, 53, 44, 0}, {2, 54, 54, 46, 1}, {1, 55, 52, 55, 0}};
	int k;

	for (k = 0; k < 3; ++k) {
		PlayToArg arg;
		Book seed, ref;
		const Book *rp = &ref;
		PositionArray *a;
		Position *p;
		long long searches;
		int deepest = 64;

		arg.start = start_board(shapes[k][4]);
		arg.n_empties = shapes[k][3];
		seed_book(&seed, shapes[k][0], shapes[k][1], shapes[k][2]);
		book_copy(&ref, &seed);
		book_test_searches = 0;
		ref_play_to(&ref, &arg);
		searches = book_test_searches;
		foreach_position(p, a, rp) if (book_probe(&seed, &p->board) == NULL) deepest = MIN(deepest, board_count_empties(&p->board));
		CHECK(ref.n_nodes > seed.n_nodes, "book play <n> %d: nothing was added", k);
		if (arg.n_empties < seed.options.n_empties) CHECK(deepest == arg.n_empties - 1, "book play %d: the last position has %d empty squares", arg.n_empties, deepest);
		else CHECK(deepest >= arg.n_empties - 1, "book play %d: a position has %d empty squares", arg.n_empties, deepest);
		printf("book play %d: %u -> %u positions, %lld searches, the last has %d empty squares\n", arg.n_empties, seed.n_nodes, ref.n_nodes, searches, deepest);
		remove("learn-test.dat.play-to");
		against_reference(&seed, &ref, searches, run_play_to, &arg, "book play <n>");
		if (k == 0) test_resume(&seed, &ref, &arg);
		book_free(&ref);
		book_free(&seed);
	}
}

/* T2: a line that reaches a position of the book goes on if it has no link, and ends if it has one */
static void test_reached(void)
{
	int linked, tries;

	for (linked = 0; linked < 2; ++linked) {
		Board head, child, grand, next;
		unsigned long long m;
		Book b;
		Position p, *q;
		int x = 0;

		for (tries = 0; tries < 1000; ++tries) {
			next = random_board(20);
			board_unique(&next, &head);
			if ((m = board_get_moves(&head)) == 0) continue;
			x = first_bit(m);
			board_next(&head, x, &next);
			board_unique(&next, &child);
			if (bit_count(board_get_moves(&child)) < 2) continue;
			board_next(&child, first_bit(board_get_moves(&child)), &next);
			board_unique(&next, &grand);
			if (board_get_moves(&grand)) break;
		}
		book_bare(&b, 2, 20);
		if (linked) {
			position_init(&p); p.board = grand; p.level = 2;
			position_search(&p, &b);
			book_add(&b, &p);
		}
		position_init(&p); p.board = child; p.level = 2;
		position_link(&p, &b);
		position_search(&p, &b);
		book_add(&b, &p);
		add_position(&b, &head, x);
		{
			const unsigned int before = b.n_nodes;
			const int n_link = book_probe(&b, &child)->n_link;
			const int leaf = book_probe(&b, &child)->leaf.move;

			book_play_to(&b, &head, 18);
			q = book_probe(&b, &child);
			CHECK(book_probe(&b, &head)->n_link > 0, "reached (linked=%d): the head is not linked", linked);
			if (linked) {
				CHECK(b.n_nodes == before && q->n_link == n_link && q->leaf.move == leaf, "reached: a position with a link was played on");
			} else {
				CHECK(b.n_nodes > before && q->n_link > 0, "reached: a position without link was not played on");
			}
		}
		book_free(&b);
	}
}

/* number of children of a board, and how many are in the set (a pass is a child) */
static int ref_children(const Board *board, int *n_in, const Book *seed)
{
	unsigned long long moves = board_get_moves(board);
	Board child[64], unique;
	int n = 0, k;

	if (moves) {
		for (; moves; moves &= moves - 1) board_next(board, first_bit(moves), child + n++);
	} else if (can_move(board->opponent, board->player)) {
		child[0].player = board->opponent;
		child[0].opponent = board->player;
		n = 1;
	}
	*n_in = 0;
	for (k = 0; k < n; ++k) {
		unsigned int i;
		bool in = false;

		board_unique(child + k, &unique);
		i = (unsigned int) ((unique.player * 0x9E3779B97F4A7C15ULL ^ unique.opponent * 0xC2B2AE3D27D4EB4FULL) >> 40) & (REF_SET_SIZE - 1);
		while (ref_set_used[i]) {
			if (board_equal(ref_set + i, &unique)) { in = true; break; }
			i = (i + 1) & (REF_SET_SIZE - 1);
		}
		if (in || book_probe(seed, &unique)) ++*n_in;
	}
	return n;
}

/*
 * One exhaust against the definitions: the positions are those of the recursion and those of the seed, every
 * position above the bottom has every move as a link, the searches are those of the new positions that still
 * have a move out of the book, the values are the best of the links and the leaf; and the same book with 4, 2
 * and 1 threads.
 */
static void exhaust_case(const Book *seed, const Board *start, const char *what)
{
	static const int n_tasks[] = {4, 2, 1};
	const int limit = seed->options.n_empties;
	long long n_new = 0, n_searches = 0;
	Book first;
	unsigned int i;
	int k;

	memset(ref_set_used, 0, sizeof ref_set_used);
	ref_set_n = 0;
	ref_exhaust_boards(start, limit);
	CHECK(ref_set_n < REF_SET_SIZE / 2, "%s: too large for the test", what);
	for (i = 0; i < REF_SET_SIZE; ++i) {
		int n_in, n;

		if (!ref_set_used[i] || book_probe(seed, ref_set + i)) continue;
		++n_new;
		n = ref_children(ref_set + i, &n_in, seed);
		if (board_get_moves(ref_set + i) ? n_in < n : n_in == 0) ++n_searches;
	}

	for (k = 0; k < 3; ++k) {
		Book b;
		PositionArray *a;
		Position *p;
		const Book *bp = &b;
		int n_bad_link = 0, n_bad_value = 0, n_absent = 0;

		book_copy(&b, seed);
		set_threads(n_tasks[k]);
		book_test_searches = book_test_negamaxes = book_test_saves = 0;
		book_test_wide_walks = book_test_wide_threaded = 0;
		book_exhaust(&b, start);

		CHECK(b.n_nodes == seed->n_nodes + n_new, "%s, %d threads: %u positions, %lld expected", what, n_tasks[k], b.n_nodes, seed->n_nodes + n_new);
		for (i = 0; i < REF_SET_SIZE; ++i) if (ref_set_used[i] && !book_probe(&b, ref_set + i)) ++n_absent;
		CHECK(n_absent == 0, "%s, %d threads: %d boards are absent", what, n_tasks[k], n_absent);

		// the searches: a position of the seed is searched again only if its leaf became a link and a move is left
		{
			long long n_again = 0;

			foreach_position(p, a, seed) {
				const Position *q = book_probe(&b, &p->board);

				if (p->leaf.move != NOMOVE && q && link_in(q, &p->leaf, false) && q->leaf.move != NOMOVE) ++n_again;
			}
			CHECK(book_test_searches == n_searches + n_again, "%s, %d threads: %lld searches, %lld + %lld expected", what, n_tasks[k], book_test_searches, n_searches, n_again);
		}
		CHECK(book_test_negamaxes == 1, "%s: %lld negamax", what, book_test_negamaxes);
		CHECK(book_test_saves == (n_new ? 1 : book_test_saves), "%s: %lld saves", what, book_test_saves);
		if (n_tasks[k] > 1) CHECK(book_test_wide_threaded == book_test_wide_walks, "%s: a wide level was walked by one thread", what);
		if (k == 0) printf("%s: %lld new positions, %lld searches, %lld levels of 4096 boards or more\n", what, n_new, book_test_searches, book_test_wide_walks);

		foreach_position(p, a, bp) {
			const unsigned long long moves = board_get_moves(&p->board);
			const Link *l;
			int best = p->leaf.score;

			unsigned int j = (unsigned int) ((p->board.player * 0x9E3779B97F4A7C15ULL ^ p->board.opponent * 0xC2B2AE3D27D4EB4FULL) >> 40) & (REF_SET_SIZE - 1);

			while (ref_set_used[j] && !board_equal(ref_set + j, &p->board)) j = (j + 1) & (REF_SET_SIZE - 1);
			if (!ref_set_used[j]) continue;
			if (board_count_empties(&p->board) >= limit) {
				const int need = moves ? bit_count(moves) : (can_move(p->board.opponent, p->board.player) ? 1 : 0);

				if (p->n_link != need) ++n_bad_link;
				if (need && p->leaf.move != NOMOVE) ++n_bad_link;
			}
			foreach_link(l, p) {
				Board next;
				const Position *c;

				if (l->move == PASS) { next.player = p->board.opponent; next.opponent = p->board.player; }
				else board_next(&p->board, l->move, &next);
				c = book_probe(bp, &next);
				if (c == NULL || l->score != -c->score.value) { ++n_bad_value; continue; }
				best = MAX(best, l->score);
			}
			if (p->score.value != best) ++n_bad_value;
		}
		CHECK(n_bad_link == 0, "%s, %d threads: %d positions without all their links", what, n_tasks[k], n_bad_link);
		CHECK(n_bad_value == 0, "%s, %d threads: %d wrong values", what, n_tasks[k], n_bad_value);
		CHECK(book_unsound(&b) == 0, "%s, %d threads: not sound", what, n_tasks[k]);
		if (k == 0) {
			book_copy(&first, &b);
		} else {
			CHECK(book_differences(&b, &first, what) == 0, "%s: another book with %d threads", what, n_tasks[k]);
		}
		book_free(&b);
	}
	book_free(&first);
}

/* the book of a board alone: searched */
static void book_of_board(Book *b, const Board *board, const int level, const int n_empties)
{
	Position p;

	book_bare(b, level, n_empties);
	position_init(&p);
	board_unique(board, &p.board);
	p.level = b->options.level;
	position_search(&p, b);
	book_add(b, &p);
	book_clean(b);
}

/* a board of n empty squares whose player has no move, the opponent has */
static bool pass_board(const int n_empties, Board *board)
{
	int i;

	for (i = 0; i < 2000000; ++i) {
		const Board b = random_board(n_empties + 1);
		unsigned long long m;

		for (m = board_get_moves(&b); m; m &= m - 1) {
			Board next;

			board_next(&b, first_bit(m), &next);
			if (!board_get_moves(&next) && can_move(next.opponent, next.player)) {
				*board = next;
				return true;
			}
		}
	}
	return false;
}

static void test_exhaust(void)
{
	Board start;
	Book seed;

	board_init(&start);
	set_threads(1);
	book_start(&seed, 1, 54);
	exhaust_case(&seed, &start, "book exhaust of a new book");
	book_free(&seed);

	seed_book(&seed, 2, 56, 54);
	exhaust_case(&seed, &start, "book exhaust of a book");
	book_free(&seed);

	seed_book(&seed, 2, 54, 53);
	start = start_board(1);
	exhaust_case(&seed, &start, "book exhaust from a move, wide");
	book_free(&seed);

	// T5: the passes, the end of games, the positions without leaf
	if (pass_board(8, &start)) {
		book_of_board(&seed, &start, 2, 8);
		exhaust_case(&seed, &start, "book exhaust from a pass, at the depth");
		book_free(&seed);
		book_of_board(&seed, &start, 2, 1);
		exhaust_case(&seed, &start, "book exhaust from a pass, to the end of the games");
		{
			Position *p = book_probe(&seed, &start);

			p->leaf = BAD_LINK; // (Edax 4.7 left such a position as it was)
			exhaust_case(&seed, &start, "book exhaust from a pass without leaf");
		}
		book_free(&seed);
	} else {
		CHECK(false, "no board with a pass");
	}
	start = random_board(8);
	book_of_board(&seed, &start, 2, 3);
	{
		Position *p = book_probe(&seed, &start);

		p->leaf = BAD_LINK;
		exhaust_case(&seed, &start, "book exhaust from a position without leaf");
	}
	book_free(&seed);
}

/* pgn: the games with a pass, of black and of white, then a game after each */
static void test_pgn(void)
{
	Base want, got;
	int tries, n[2] = {0, 0}, i;

	base_init(&want);
	for (tries = 0; tries < 100000 && (n[0] < 20 || n[1] < 20); ++tries) {
		Board b;
		Game g;
		int ply = 0, side = 0, passed = -1;

		game_init(&g);
		board_init(&g.initial_board);
		b = g.initial_board;
		while (ply < 60) {
			unsigned long long m = board_get_moves(&b);
			int k, x;

			if (!m) {
				if (!can_move(b.opponent, b.player)) break;
				if (passed < 0) passed = side;
				board_pass(&b);
				side ^= 1;
				m = board_get_moves(&b);
			}
			for (k = rr() % bit_count(m); k--; ) m &= m - 1;
			x = first_bit(m);
			g.move[ply++] = (char) x;
			board_next(&b, x, &b);
			side ^= 1;
		}
		if (passed < 0 || n[passed] >= 20) continue;
		++n[passed];
		base_append(&want, &g);
	}
	CHECK(n[0] == 20 && n[1] == 20, "pgn: no games with a pass of each side");
	remove("pass.pgn");
	CHECK(base_save(&want, "pass.pgn"), "pgn: not written");
	base_init(&got);
	base_load(&got, "pass.pgn");
	CHECK(got.n_games == want.n_games, "pgn: %d games of %d", got.n_games, want.n_games);
	for (i = 0; i < got.n_games && i < want.n_games; ++i) {
		if (memcmp(got.game[i].move, want.game[i].move, sizeof got.game[i].move)) {
			CHECK(false, "pgn: the game %d differs", i);
			break;
		}
	}
	base_free(&got);
	base_free(&want);
}

/* ---- book extract all ---- */

static long file_bytes(const char *name, unsigned char **bytes)
{
	FILE *f = fopen(name, "rb");
	long n;

	*bytes = NULL;
	if (f == NULL) return -1;
	fseek(f, 0, SEEK_END);
	n = ftell(f);
	rewind(f);
	*bytes = (unsigned char*) malloc(n + 1);
	if (fread(*bytes, 1, n, f) != (size_t) n) n = -1;
	fclose(f);
	return n;
}

static bool same_bytes(const char *x, const char *y)
{
	unsigned char *a, *b;
	const long n = file_bytes(x, &a), m = file_bytes(y, &b);
	const bool same = n >= 0 && n == m && memcmp(a, b, n) == 0;

	free(a); free(b);
	return same;
}

/* the lines, the plain way */
static void ref_lines(Book *b, const Board *board, Line *pv, Base *lines)
{
	Position *p = book_probe(b, board);
	MoveList list;
	Move *m;

	list.n_moves = 0;
	if (p && !position_is_done(p, b)) {
		position_set_done(p, b);
		position_get_moves(p, board, &list);
		if (list.n_moves) {
			foreach_move(m, list) {
				Board next = *board;

				board_update(&next, m);
				line_push(pv, m->x);
				ref_lines(b, &next, pv, lines);
				line_pop(pv);
			}
			return;
		}
	}
	{
		Board init;
		Game game;

		board_init(&init);
		line_to_game(&init, pv, &game);
		base_append(lines, &game);
	}
}

static void extract_case(Book *b, const Board *end, const char *what)
{
	static const char *files[] = {"lines.txt", "lines.ggf", "lines.sgf", "lines.pgn", "lines.edx", "lines.wtb"};
	Base expected, got;
	Board after_f5, init;
	Line pv;
	int i, k;
	bool has_end = (end == NULL);

	board_init(&init);
	board_next(&init, F5, &after_f5);
	base_init(&expected);
	line_init(&pv, BLACK);
	line_push(&pv, F5);
	book_clean(b);
	ref_lines(b, &after_f5, &pv, &expected);
	CHECK(expected.n_games > 0, "%s: no line", what);
	remove("WTHOR.JOU");
	for (k = 0; k < 6; ++k) {
		remove(files[k]);
		CHECK(book_extract_all(b, files[k]), "%s: %s not written", what, files[k]);
		base_init(&got);
		CHECK(base_load(&got, files[k]), "%s: %s not read", what, files[k]);
		CHECK(got.n_games == expected.n_games, "%s: %s has %d games, %d expected", what, files[k], got.n_games, expected.n_games);
		for (i = 0; i < got.n_games && i < expected.n_games; ++i) {
			if (memcmp(got.game[i].move, expected.game[i].move, sizeof got.game[i].move)) {
				CHECK(false, "%s: the game %d of %s differs", what, i, files[k]);
				break;
			}
		}
		if (k == 0 && end) {
			for (i = 0; i < got.n_games; ++i) {
				Board board = init, u;
				int j;

				for (j = 0; j < 60 && got.game[i].move[j] != NOMOVE && game_update_board(&board, got.game[i].move[j]); ++j) ;
				board_unique(&board, &u);
				if (board_equal(&u, end)) has_end = true;
				board_pass(&board);
				board_unique(&board, &u);
				if (board_equal(&u, end)) has_end = true;
			}
			CHECK(has_end, "%s: the line to the end of the game is missing", what);
		}
		base_free(&got);
		// the games of the file are kept
		CHECK(book_extract_all(b, files[k]), "%s: %s not added to", what, files[k]);
		base_init(&got);
		base_load(&got, files[k]);
		CHECK(got.n_games == 2 * expected.n_games, "%s: %s has %d games after a second extract", what, files[k], got.n_games);
		base_free(&got);
	}
	printf("%s: %d lines\n", what, expected.n_games);
	base_free(&expected);
}

static void test_extract(void)
{
	Book b;
	int tries, n_played;
	bool found = false;

	// T6: a whole game in the book, with a pass: the line to its end
	for (tries = 0; tries < 100000 && !found; ++tries) {
		Board board, end;
		Game game;
		int n_pass = 0;

		game_init(&game);
		board_init(&game.initial_board);
		board = game.initial_board;
		for (n_played = 0; n_played < 60; ++n_played) {
			unsigned long long m = board_get_moves(&board);
			int x, k;

			if (!m) {
				if (!can_move(board.opponent, board.player)) break;
				board_pass(&board);
				++n_pass;
				m = board_get_moves(&board);
			}
			if (n_played == 0) {
				x = F5;
			} else {
				for (k = rr() % bit_count(m); k--; ) m &= m - 1;
				x = first_bit(m);
			}
			game.move[n_played] = (char) x;
			board_next(&board, x, &board);
		}
		if (n_pass == 0 || !board_is_game_over(&board)) continue;
		found = true;
		book_bare(&b, 1, 0);
		book_add_game(&b, &game);
		book_negamax(&b);
		board_unique(&board, &end);
		if (book_probe(&b, &end) == NULL) {
			board_pass(&board);
			board_unique(&board, &end);
		}
		CHECK(book_probe(&b, &end) != NULL, "extract: the end of the game is not in the book");
		extract_case(&b, &end, "book extract all of a whole game with a pass");
		book_free(&b);
	}
	CHECK(found, "extract: no game with a pass");

	seed_book(&b, 2, 55, 55);
	extract_case(&b, NULL, "book extract all of a book with transpositions");

	// a file that cannot be read to its end is left as it was
	{
		unsigned char *bytes;
		const long n = file_bytes("lines.wtb", &bytes);
		FILE *f = fopen("cut.wtb", "wb");

		fwrite(bytes, 1, n - 5, f);
		fclose(f);
		free(bytes);
		copy_file("cut.wtb", "cut.copy");
		CHECK(!book_extract_all(&b, "cut.wtb"), "extract: added to a cut file");
		CHECK(same_bytes("cut.wtb", "cut.copy"), "extract: a cut file was changed");
		f = fopen("cut.wtb.writing.wtb", "rb");
		CHECK(f == NULL, "extract: the file being written is left");
		if (f) fclose(f);
	}
	CHECK(!book_extract_all(&b, "no-such-folder/lines.txt"), "extract: wrote to a folder that is not there");
	CHECK(!book_extract_all(&b, ""), "extract: wrote to no name");
	CHECK(!book_extract_all(&b, "lines.xyz"), "extract: wrote an unknown format");
	book_free(&b);

	// no position after f5: no game, the file is left
	{
		Board start;

		board_init(&start);
		book_of_board(&b, &start, 2, 55);
		copy_file("lines.txt", "lines.copy");
		CHECK(!book_extract_all(&b, "lines.txt"), "extract: lines of a book without f5");
		CHECK(same_bytes("lines.txt", "lines.copy"), "extract: the file was changed for a book without f5");
		book_free(&b);
	}
}

/* ---- the base commands ---- */

static void write_text(const char *name, const char *text)
{
	FILE *f = fopen(name, "wb");

	fputs(text, f);
	fclose(f);
}

static int games_of(const char *name)
{
	Base b;
	int n;

	base_init(&b);
	base_load(&b, name);
	n = b.n_games;
	base_free(&b);
	return n;
}

/* T7 and T8 */
static void test_base(void)
{
	Base b;
	Game g;
	unsigned char *bytes;
	long n;
	FILE *f;

	write_text("in.txt", "f5d6c3d3c4\nf5f6e6f4\nf5d6c3d3c4\n");
	remove("out.txt");
	CHECK(base_add_file("in.txt", "out.txt", false) && games_of("out.txt") == 3, "base convert: not 3 games");
	CHECK(base_add_file("in.txt", "out.txt", false) && games_of("out.txt") == 6, "base append: not 6 games");
	remove("uniq.txt");
	CHECK(base_add_file("in.txt", "uniq.txt", true) && games_of("uniq.txt") == 2, "base unique: not 2 games");

	// the same file, whatever its name
	copy_file("in.txt", "in.copy");
	CHECK(!base_add_file("in.txt", "in.txt", false), "base convert to the input: accepted");
	CHECK(!base_add_file("in.txt", "./in.txt", false), "base convert to ./ the input: accepted");
#ifdef _WIN32
	CHECK(!base_add_file("in.txt", "IN.TXT", true), "base unique to the input in capitals: accepted");
	CHECK(!base_add_file("in.txt", ".\\in.txt", false), "base convert to .\\ the input: accepted");
#endif
	CHECK(same_bytes("in.txt", "in.copy"), "the input was changed");
	CHECK(!base_add_file("in.txt", "", false), "base convert without an output: accepted");

	// an input that cannot be read, to its end or at all
	base_init(&b);
	game_init(&g);
	text_to_game("f5d6c3", &g);
	strcpy(g.name[BLACK], "A");
	strcpy(g.name[WHITE], "Bb");
	base_append(&b, &g);
	base_append(&b, &g);
	remove("two.edx");
	CHECK(base_save(&b, "two.edx"), "base_save edx");
	n = file_bytes("two.edx", &bytes);
	f = fopen("cut.edx", "wb");
	fwrite(bytes, 1, n - 3, f);
	fclose(f);
	free(bytes);
	copy_file("out.txt", "out.copy");
	CHECK(!base_add_file("cut.edx", "out.txt", false), "base convert of a cut file: accepted");
	CHECK(!base_add_file("none.txt", "out.txt", false), "base convert of no file: accepted");
	CHECK(same_bytes("out.txt", "out.copy"), "the output was changed by an input that cannot be read");

	// an output that cannot be read to its end
	copy_file("cut.edx", "cut.keep");
	CHECK(!base_add_file("in.txt", "cut.edx", false), "base convert to a cut file: accepted");
	CHECK(same_bytes("cut.edx", "cut.keep"), "a cut output was changed");

	// T8: the same input gives the same WTHOR.JOU (memory is not written to it)
	{
		static const char *names[] = {"A", "Bb", "Ogawa", "x"};
		unsigned char *one, *two;
		int k;
		long n1, n2;

		for (k = 0; k < 2; ++k) {
			void *dirt[64];
			int i;

			for (i = 0; i < 64; ++i) {
				dirt[i] = malloc(20 + i % 5 * 20);
				memset(dirt[i], k ? 0xAA : 0x55, 20 + i % 5 * 20);
			}
			for (i = 0; i < 64; ++i) free(dirt[i]);
			for (i = 0; i < b.n_games; ++i) {
				strcpy(b.game[i].name[BLACK], names[2 * i]);
				strcpy(b.game[i].name[WHITE], names[2 * i + 1]);
			}
			remove("WTHOR.JOU");
			remove("names.wtb");
			CHECK(base_save(&b, "names.wtb"), "base_save wtb");
			copy_file("WTHOR.JOU", k ? "jou.2" : "jou.1");
		}
		n1 = file_bytes("jou.1", &one);
		n2 = file_bytes("jou.2", &two);
		CHECK(n1 == 16 + 5 * 20 && n1 == n2 && !memcmp(one + 16, two + 16, n1 - 16), "WTHOR.JOU differs for the same input");
		if (n1 == 16 + 5 * 20) {
			int i, j, n_dirty = 0;

			for (i = 0; i < 5; ++i) {
				const unsigned char *field = one + 16 + 20 * i;

				for (j = (int) strlen((const char*) field); j < 20; ++j) n_dirty += field[j] != 0;
			}
			CHECK(n_dirty == 0, "WTHOR.JOU: %d bytes of memory after the names", n_dirty);
		}
		free(one); free(two);
	}

	// wthor: the games of the file are kept as they are (players, score)
	{
		WthorBase before, after;

		wthor_load(&before, "names.wtb");
		if (before.n_games == 2) {
			before.game[0].score = 41;
			before.game[0].theoric_score = 39;
			before.game[0].tournament = 7;
			wthor_save(&before, "names.wtb");
			CHECK(base_add_file("in.txt", "names.wtb", false), "base append to wtb");
			wthor_load(&after, "names.wtb");
			CHECK(after.n_games == 5 && !memcmp(after.game, before.game, 2 * sizeof *after.game), "the games of the wtb file were changed");
			wthor_free(&after);
		} else {
			CHECK(false, "names.wtb: %d games", before.n_games);
		}
		wthor_free(&before);
	}
	base_free(&b);
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
	if (all || !strcmp(t, "playto")) test_play_to();
	if (all || !strcmp(t, "reached")) test_reached();
	if (all || !strcmp(t, "exhaust")) test_exhaust();
	if (all || !strcmp(t, "extract")) test_extract();
	if (all || !strcmp(t, "base")) test_base();
	if (all || !strcmp(t, "pgn")) test_pgn();
	if (all || !strcmp(t, "dangling")) test_dangling();
	printf("book-learn: %d checks, %d failures\n", n_check, n_fail);
	return (n_check && !n_fail) ? 0 : 1;
}
