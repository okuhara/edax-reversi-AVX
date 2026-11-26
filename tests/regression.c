#define main edax_original_main
#include "../src/all.c"
#undef main
#include "stabwindow-reference.h"
static unsigned rng=0x651197;
static unsigned rr(void){rng^=rng<<13;rng^=rng>>17;return rng^=rng<<5;}
static unsigned long long ref_flip(Board b,int x){
 if((b.player|b.opponent)&(1ULL<<x))return 0;
 unsigned long long ans=0;int r=x/8,c=x%8;
 for(int dr=-1;dr<=1;++dr)for(int dc=-1;dc<=1;++dc)if(dr||dc){int y=r+dr,z=c+dc;unsigned long long run=0;
 while(y>=0&&y<8&&z>=0&&z<8){unsigned long long q=1ULL<<(y*8+z);if(b.opponent&q)run|=q;else{if(b.player&q)ans|=run;break;}y+=dr;z+=dc;}}
 return ans;
}
static unsigned long long ref_moves(Board b)
{
 unsigned long long empty = ~(b.player | b.opponent), moves = 0;
 while (empty) {
  const int x = first_bit(empty);
  empty &= empty - 1;
  if (ref_flip(b, x)) moves |= 1ULL << x;
 }
 return moves;
}
static Board ref_next(Board b,int x){unsigned long long f=ref_flip(b,x);Board q={b.opponent^f,b.player^f^(1ULL<<x)};return q;}
/* Test-only transposition cache. A hit requires both complete bitboards. */
enum { REF_CACHE_BITS = 18 };
enum RefKind { REF_EMPTY, REF_EXACT, REF_LOWER, REF_UPPER };
typedef struct RefEntry {
 Board board;
 int score;
 enum RefKind kind;
} RefEntry;
static RefEntry *ref_cache;

static int ref_solve(Board board, int alpha, int beta)
{
 const int initial_alpha = alpha;
 RefEntry *entry = NULL;
 int best = -65;
 if (ref_cache) {
  const unsigned long long key = (board.player * 0x9e3779b97f4a7c15ULL)
                               ^ (board.opponent * 0xbf58476d1ce4e5b9ULL);
  entry = ref_cache + (key >> (64 - REF_CACHE_BITS));
  if (entry->board.player == board.player && entry->board.opponent == board.opponent
      && (entry->kind == REF_EXACT || (entry->kind == REF_LOWER && entry->score >= beta)
          || (entry->kind == REF_UPPER && entry->score <= alpha))) return entry->score;
 }

 unsigned long long moves = ref_moves(board);
 if (!moves) {
  Board passed = {board.opponent, board.player};
  if (ref_moves(passed)) best = -ref_solve(passed, -beta, -alpha);
  else {
   const int discs = bit_count(board.player) - bit_count(board.opponent);
   const int empty = 64 - bit_count(board.player | board.opponent);
   best = discs > 0 ? discs + empty : discs < 0 ? discs - empty : 0;
  }
 }
 while (moves) {
  const unsigned long long corners = moves & 0x8100000000000081ULL;
  const unsigned long long other = moves & ~0x42c300000000c342ULL;
  const int x = first_bit(corners ? corners : other ? other : moves);
  moves ^= 1ULL << x;
  const int score = -ref_solve(ref_next(board, x), -beta, -alpha);
  if (score > best) best = score;
  if (score > alpha) alpha = score;
  if (alpha >= beta) break;
 }
 if (entry) {
  entry->board = board;
  entry->score = best;
  entry->kind = best <= initial_alpha ? REF_UPPER : best >= beta ? REF_LOWER : REF_EXACT;
 }
 return best;
}
static const char *eval_path(void) { const char *p = getenv("EDAX_EVAL_FILE"); return p ? p : "data/eval.dat"; }
/* the engine of the cases: threads, a small hash table, no output, the evaluation of eval_path() */
static void init_engine(int tasks){options.n_task=tasks;options.hash_table_size=10;options.verbosity=0;options.cpu_affinity=false;eval_open(eval_path());search_global_init();}
static void init_search(Search *s,int tasks){init_engine(tasks);search_init(s);}
/* one of the moves m, at random (rr) */
static int random_move(unsigned long long m){int k=rr()%bit_count(m);while(k--)m&=m-1;return first_bit(m);}
/* a file of a case: the case fails (exit 2) when it cannot be opened (up to v4.5.5-nikque.26 the null FILE was used: a crash) */
#define OPEN(f,name,mode) do{if(((f)=fopen((name),(mode)))==NULL){printf("FAIL: cannot open %s\n",(name));return 2;}}while(0)
#define TMPFILE(f) do{if(((f)=tmpfile())==NULL){puts("FAIL: cannot create a temporary file");return 2;}}while(0)
/* the book of some cases, read from the root of the repository */
#define FIXTURE "tests/merge-complete-fixture.dat"
/* a copy of a file without its last byte; returns the size of the file, -1 if a file cannot be opened (said) */
static long copy_cut(const char *from,const char *to){FILE *in=fopen(from,"rb"),*out=in?fopen(to,"wb"):NULL;if(!in||!out){if(in)fclose(in);printf("FAIL: cannot copy %s to %s\n",from,to);return -1;}fseek(in,0,SEEK_END);long n=ftell(in);rewind(in);for(long i=0;i<n-1;++i)fputc(fgetc(in),out);fclose(in);fclose(out);return n;}
static bool file_exists(const char *path){FILE *f=fopen(path,"rb");if(!f)return false;fclose(f);return true;}
/* false (said) when the book of the cases is not in the current folder */
static bool have_fixture(void){if(file_exists(FIXTURE))return true;printf("FAIL: %s is not in the current folder (run the cases from the root of the repository)\n",FIXTURE);return false;}
/* what is written to stdout (fd 1) or stderr (fd 2) between capture_begin and capture_end, which gives it back (to be freed) */
#ifdef _WIN32
#include <io.h>
#define fd_dup _dup
#define fd_dup2 _dup2
#define fd_close _close
#define fd_of _fileno
#else
#include <unistd.h>
#define fd_dup dup
#define fd_dup2 dup2
#define fd_close close
#define fd_of fileno
#endif
static int capture_fd=-1,capture_saved=-1;
static bool capture_begin(int fd){FILE *f;fflush(stdout);fflush(stderr);if((f=fopen("audit-capture.txt","wb"))==NULL)return false;capture_saved=fd_dup(fd);fd_dup2(fd_of(f),fd);fclose(f);capture_fd=fd;return true;}
static char *capture_end(void){FILE *f;long n;char *s;fflush(stdout);fflush(stderr);fd_dup2(capture_saved,capture_fd);fd_close(capture_saved);if((f=fopen("audit-capture.txt","rb"))==NULL)return NULL;fseek(f,0,SEEK_END);n=ftell(f);rewind(f);s=malloc(n+1);n=(long)fread(s,1,n,f);s[n]=0;fclose(f);remove("audit-capture.txt");return s;}
/* a user interface of a protocol, without its input thread: the messages are added to its event */
static UI *ui_new(void){UI *ui=mm_malloc(sizeof *ui);memset(ui,0,sizeof *ui);event_init(&ui->event);return ui;}
static void ui_delete(UI *ui){event_free(&ui->event);mm_free(ui);}
static Board late_board(int empties){Board b;board_init(&b);while(board_count_empties(&b)>empties){unsigned long long m=ref_moves(b);if(!m){board_pass(&b);m=ref_moves(b);if(!m)break;}b=ref_next(b,random_move(m));}return b;}
static int test_solid_hash(void)
{
	/* These boards differ only in solid disc colors. Seven empties remain at
	 * the search entry, and the old normalized key matches for both boards. */
	const Board first = {0xe07f00466cf0988dULL, 0x1b007fb9130f6702ULL};
	const Board second = {0xe07f004e6cf39b8dULL, 0x1b007fb1130c6402ULL};
	const int alpha = 8;
	const int expected_first = ref_solve(first, SCORE_MIN, SCORE_MAX);
	const int expected_second = ref_solve(second, SCORE_MIN, SCORE_MAX);
	Search search;

	init_search(&search, 1);
	memset(search.thread_hash.hash, 0, search.thread_hash.n_hash * sizeof(Hash));
	search.stop = RUNNING;
	search_set_board(&search, &first, BLACK);
	const int first_bound = NWS_endgame_local(&search, alpha);

	/* Keep the same thread-local table when searching the second board. */
	search_set_board(&search, &second, BLACK);
	const int second_bound = NWS_endgame_local(&search, alpha);
	search_free(&search);

	printf("SOLID hash alpha=%d first exact=%d bound=%d second exact=%d bound=%d\n",
	       alpha, expected_first, first_bound, expected_second, second_bound);
	return first_bound < expected_first || first_bound > alpha ||
	       second_bound <= alpha || second_bound > expected_second ? 2 : 0;
}

#if COUNT_LAST_FLIP == COUNT_LAST_FLIP_BMI2
static int test_last_empty_diagonals(void)
{
	int checked = 0;
	for (int x = 0; x < BOARD_SIZE; ++x) {
		const unsigned long long occupied = ~(1ULL << x);
		unsigned long long diagonals = 0;
		for (int y = 0; y < BOARD_SIZE; ++y) {
			if (y != x && abs(y / 8 - x / 8) == abs(y % 8 - x % 8)) diagonals |= 1ULL << y;
		}
		unsigned long long player = diagonals;
		do {
			const Board board = {player, occupied ^ player};
			const Board swapped = {board.opponent, board.player};
			const int expected = ref_solve(board, SCORE_MIN, SCORE_MAX);
			if (last_flip(x, board.player) != 2 * bit_count(ref_flip(board, x)) ||
			    last_flip(x, board.opponent) != 2 * bit_count(ref_flip(swapped, x)) ||
			    solve_exact_1(board.player, x) != expected) {
				printf("FAIL last empty square=%d player=%016llx exact=%d\n", x, board.player, expected);
				return 2;
			}
			++checked;
			player = (player - 1) & diagonals;
		} while (player != diagonals);
	}
	printf("PASS last empty diagonals: %d positions\n", checked);
	return 0;
}
#endif

#if defined(hasSSE2) || defined(__ARM_NEON)
static int test_eval_vectors(void)
{
	int checked = 0;
	for (int i = 0; i < 256; ++i) {
		const Board board = late_board(i % 61);
		const unsigned long long empty = ~(board.player | board.opponent);
		Eval eval = {0};
		eval.n_empties = bit_count(empty);
		const unsigned long long side = (eval.n_empties & 1) ? board.opponent : board.player;
		eval_set(&eval, &board);
		for (int lane = 0; lane < 48; ++lane) {
			unsigned int expected = EVAL_FEATURE_all_opponent.us[lane];
			for (int x = 0; x < BOARD_SIZE; ++x) {
				const unsigned long long square = 1ULL << x;
				if (side & square) expected -= EVAL_FEATURE[x].us[lane];
				if (empty & square) expected += EVAL_FEATURE[x].us[lane];
			}
			if (eval.feature.us[lane] != (unsigned short) expected) {
				printf("FAIL initial evaluation features at position %d lane %d\n", i, lane);
				return 2;
			}
		}
		unsigned long long moves = ref_moves(board);
		int x;
		foreach_bit (x, moves) {
			const unsigned long long flipped = ref_flip(board, x);
			const Board child = ref_next(board, x);
			Eval inplace = eval, separate = {0}, expected = {0};
			expected.n_empties = eval.n_empties - 1;
			eval_set(&expected, &child);
			eval_update(x, flipped, &inplace);
			eval_update_leaf(x, flipped, &separate, &eval);
			if (memcmp(inplace.feature.us, expected.feature.us, sizeof expected.feature.us) ||
			    memcmp(separate.feature.us, expected.feature.us, sizeof expected.feature.us)) {
				printf("FAIL incremental evaluation features at position %d move %d\n", i, x);
				return 2;
			}
			++checked;
		}
	}
	printf("PASS evaluation vectors: 256 setups, %d moves\n", checked);
	return 0;
}
#endif

#ifdef __AVX2__
static int test_stability_pair(void)
{
	for (int i = 0; i < 256; ++i) {
		const Board board = late_board(i % 61);
		int player, opponent;
		get_stability_pair(&board, &player, &opponent);
		if (player != get_stability(board.player, board.opponent) ||
		    opponent != get_stability(board.opponent, board.player)) {
			printf("FAIL paired stability at position %d\n", i);
			return 2;
		}
	}
	puts("PASS paired stability: 256 positions");
	return 0;
}

#endif

static void core(int tasks){Search s;init_search(&s,tasks);unsigned long long checks=0;int exact=0,passes=0;for(int g=0;g<300;++g){Board b;board_init(&b);for(int ply=0;ply<120;++ply){unsigned long long m=ref_moves(b);if(m!=board_get_moves(&b)){puts("FAIL moves");exit(2);}for(int x=0;x<64;++x)if(!((b.player|b.opponent)&(1ULL<<x))){Move mv;unsigned long long f=board_get_move_flip(&b,x,&mv);++checks;if(f!=ref_flip(b,x)){puts("FAIL flips");exit(2);}}
 Board unique;board_unique(&b,&unique);for(int sym=0;sym<8;++sym){Board transformed,u;board_symetry(&b,sym,&transformed);board_unique(&transformed,&u);if(!board_equal(&unique,&u)){puts("FAIL symmetry");exit(2);}}
 if(!m){board_pass(&b);++passes;if(!ref_moves(b))break;continue;}int x=random_move(m);Board next=ref_next(b,x),actual;board_next(&b,x,&actual);if(!board_equal(&next,&actual)){puts("FAIL next");exit(2);}Eval e={0},fresh={0};e.n_empties=board_count_empties(&b);eval_set(&e,&b);eval_update(x,ref_flip(b,x),&e);fresh.n_empties=board_count_empties(&next);eval_set(&fresh,&next);if(memcmp(&e.feature,&fresh.feature,sizeof e.feature)){puts("FAIL incremental eval");exit(2);}b=next;}}
 for(int i=0;i<96;++i){Board b=late_board(1+i%12);int want=ref_solve(b,-65,65);search_cleanup(&s);search_set_board(&s,&b,BLACK);search_set_level(&s,60,board_count_empties(&b));search_run(&s);if(s.result->score!=want){printf("FAIL exact tasks=%d empty=%d expected=%d actual=%d\n",tasks,board_count_empties(&b),want,s.result->score);exit(2);}++exact;}
 printf("PASS core tasks=%d games=300 flip_checks=%llu symmetries=all8 exact_positions=%d empties=1..12 pass_events=%d\n",tasks,checks,exact,passes);search_free(&s);}
int main(int argc,char **argv){if(argc<2)return 1;setbuf(stdout,NULL);bit_init();edge_stability_init();const char *t=argv[1];
 if(!strcmp(t,"bool")){bool b=false;options_read("book-usage","abcdef");printf("value=%d\n",b);}
 else if(!strcmp(t,"short")){Base b;base_init(&b);char *p=malloc(2);p[0]='x';p[1]=0;base_load(&b,p);free(p);base_free(&b);}
 else if(!strcmp(t,"ggf")){Game g;FILE *f;TMPFILE(f);fputs("(;GM[Othello]PB[",f);for(int i=0;i<1000;++i)fputc('a',f);fputs("];)",f);rewind(f);game_import_ggf(&g,f);fclose(f);}
 else if(!strcmp(t,"edx")){Game g;Base b,c;remove("audit-roundtrip.edx");game_init(&g);g.move[0]=26;g.date.month=10;base_init(&b);base_init(&c);base_append(&b,&g);base_save(&b,"audit-roundtrip.edx");base_load(&c,"audit-roundtrip.edx");printf("EDX roundtrip expected_games=1 actual=%d expected_firstmove=26 actual=%d\n",c.n_games,c.n_games?c.game[0].move[0]:-1);if(c.n_games!=1||c.game[0].move[0]!=26)return 2;base_free(&b);base_free(&c);}
 else if(!strcmp(t,"savefail")){Book b={0};book_init(&b);int ok=book_save(&b,"audit-directory-does-not-exist/book.dat");book_free(&b);printf("BOOK save to missing directory accepted=%d\n",ok);if(ok)return 2;}
 else if(!strcmp(t,"deviatesave")){
  const char *path="audit-deviate-save.dat";Book b={0};book_init(&b);int rounds=0;
  remove(path);options_read("book-deviate-save-rounds","0");
  book_deviate_save_progress(&b,path,1,&rounds);book_deviate_save_progress(&b,path,1,&rounds);
  if(file_exists(path))return 2;
  book_deviate_save_progress(&b,path,0,&rounds);
  if(!file_exists(path))return 2;remove(path);
  options_read("book-deviate-save-rounds","10");rounds=0;
  for(int i=0;i<9;++i)book_deviate_save_progress(&b,path,1,&rounds);
  if(file_exists(path))return 2;
  book_deviate_save_progress(&b,path,1,&rounds);
  if(!file_exists(path))return 2;remove(path);
  book_deviate_save_progress(&b,path,1,&rounds);book_deviate_save_progress(&b,path,0,&rounds);
  if(!file_exists(path))return 2;remove(path);
  options_read("book-deviate-save-rounds","1");rounds=0;
  book_deviate_save_progress(&b,path,1,&rounds);
  if(!file_exists(path))return 2;remove(path);
  book_free(&b);puts("PASS deviate save cadence: final-only, every 10 rounds, final remainder, and every round");
 }
 else if(!strcmp(t,"reproducible")){
  /* book-seed: the random numbers of a book start from the seed; auto and a wrong value leave it to the clock */
  Book r={0};Random want;options_read("book-seed","12345");book_init(&r);random_seed(&want,12345);int ok=options.book_seed_set&&r.random.x==want.x;book_free(&r);
  options_read("book-seed","auto");ok=ok&&!options.book_seed_set;options_read("book-seed","-1");ok=ok&&!options.book_seed_set;options_read("book-seed","1x");ok=ok&&!options.book_seed_set;
  printf("REPRODUCIBLE seed option ok=%d\n",ok);if(!ok)return 2;
  /* the links of a new position are checked against the book before its expansion is applied */
  Search s;init_search(&s,4);Book b={0};b.search=&s;book_init(&b);Board start;board_init(&start);
  Position c,d;position_init(&c);c.board=start;position_link(&c,&b);int fresh=position_links_current(&c,&b);
  position_init(&d);board_next(&start,F5,&d.board);d.score.value=3;position_unique(&d);if(book_add(&b,&d)<=0)return 2;
  int stale_new=!position_links_current(&c,&b);position_link(&c,&b);int relinked=position_links_current(&c,&b)&&c.n_link==4; /* (the 4 first moves give the same position) */
  for(int i=0;i<c.n_link;++i)relinked=relinked&&position_links(&c)[i].score==-3;
  Board after;board_next(&start,F5,&after);book_probe(&b,&after)->score.value=5;int stale_score=!position_links_current(&c,&b);position_free(&c);book_free(&b);
  printf("REPRODUCIBLE links check: fresh=%d new_position=%d relinked=%d new_score=%d\n",fresh,stale_new,relinked,stale_score);if(!fresh||!stale_new||!relinked||!stale_score)return 2;
  /* the same book with 4 threads (expansions applied in the order of the list) and with 1 (one after the other) */
  options_read("book-reproducible","on");options.book_file="audit-repro.dat";
  const char *out[3]={"audit-repro-4a.dat","audit-repro-4b.dat","audit-repro-1.dat"};const int tasks[3]={4,4,1};unsigned char *bytes[3];long size[3];
  /* (a shallow book: positions down to 52 empties, 8 moves; with 36 empties the book grew to hundreds of MB) */
  for(int k=0;k<3;++k){options.n_task=tasks[k];book_new(&b,2,52);book_deviate(&b,&start,2,4);if(!book_save(&b,out[k]))return 2;printf("REPRODUCIBLE n_task=%d positions=%d\n",tasks[k],b.n_nodes);book_free(&b);
   FILE *f=fopen(out[k],"rb");if(!f)return 2;fseek(f,0,SEEK_END);size[k]=ftell(f);rewind(f);bytes[k]=malloc(size[k]);if(fread(bytes[k],1,size[k],f)!=(size_t)size[k])return 2;fclose(f);remove(out[k]);}
  remove("audit-repro.dat.dev");
  for(int k=1;k<3;++k){int same=size[k]==size[0]&&!memcmp(bytes[k],bytes[0],10)&&!memcmp(bytes[k]+18,bytes[0]+18,size[0]-18);
   printf("REPRODUCIBLE %s against %s: same except the date=%d (%ld, %ld bytes)\n",out[k],out[0],same,size[k],size[0]);if(!same)return 2;}
  printf("REPRODUCIBLE expansions searched again (their links changed): %lld\n",book_test_expand_redo);
  for(int k=0;k<3;++k)free(bytes[k]);options_read("book-reproducible","off");search_free(&s);
  puts("PASS reproducible: book-seed, links check, the same book with 4 threads (twice) and 1 thread");
 }
 else if(!strcmp(t,"expandredo")){
  /* book-reproducible: the expansion 1 is searched before the expansion 0, whose new position is a move of the new position of 1:
     when 0 is applied, 1 has old links, is searched again, then applied; the book is the one of position_expand (0, then 1) */
  Search s;init_search(&s,1);options_read("book-reproducible","on");
  Board x0=late_board(40),x,c0;unsigned long long m0=board_get_moves(&x0);if(!m0)return 2;int a=first_bit(m0);board_next(&x0,a,&x);
  unsigned long long m1=board_get_moves(&x);if(!m1)return 2;int m=first_bit(m1);board_next(&x,m,&c0);
  Book b[2]={{0}};
  for(int k=0;k<2;++k){b[k].search=&s;book_init(b+k);b[k].options.level=2;b[k].options.n_empties=20;
   Position p;position_init(&p);p.board=x;p.level=2;p.leaf.move=(unsigned char)m;p.leaf.score=0;p.score.value=0;position_unique(&p);if(book_add(b+k,&p)<=0)return 2;
   position_init(&p);p.board=x0;p.level=2;p.leaf.move=(unsigned char)a;p.leaf.score=0;p.score.value=0;position_unique(&p);if(book_add(b+k,&p)<=0)return 2;}
  /* the reference: one after the other */
  position_expand(book_probe(b+1,&x),b+1);position_expand(book_probe(b+1,&x0),b+1);
  /* the same expansions, applied in their order after being searched in the other order */
  ExpandShared sh;memset(&sh,0,sizeof sh);sh.book=b;sh.action="Expand test";sh.tmp_file="audit-expandredo.tmp";sh.t=real_clock();sh.n_slot=4;sh.slot=calloc(4,sizeof *sh.slot);for(int i=0;i<4;++i)sh.slot[i].index=-1;
  lock_init(&sh);condition_init(&sh);
  b[0].todo_list.item=malloc(2*sizeof *b[0].todo_list.item);b[0].todo_list.n=b[0].todo_list.size=2;b[0].todo_list.valid=true;
  b[0].todo_list.item[0]=position_key(b,book_probe(b,&x));b[0].todo_list.item[1]=position_key(b,book_probe(b,&x0));
  Search *search=book_search(b);ExpandSlot *e0=sh.slot,*e1=sh.slot+1;
  for(int i=0;i<2;++i){ExpandSlot *e=sh.slot+i;e->index=i;e->skip=false;e->ready=false;if(!book_expand_take(&sh,book_position(b,b[0].todo_list.item[i]),&e->parent,&e->child))return 2;}sh.next=2;
  int links_before=e1->child.n_link;
  e1->r=book_expand_search(search,&e1->parent,&e1->child,&e1->leaf);e1->ready=true;Link stale_leaf=e1->leaf,stale_parent=e1->parent.leaf;book_expand_apply_ready(&sh);int waited=sh.applied==0&&sh.redo==NULL;
  e0->r=book_expand_search(search,&e0->parent,&e0->child,&e0->leaf);e0->ready=true;book_expand_apply_ready(&sh);int redo=sh.applied==1&&sh.redo==e1&&e1->child.n_link==links_before+1;
  sh.redo=NULL;e1->r=book_expand_search(search,&e1->parent,&e1->child,&e1->leaf);e1->ready=true;
  printf("EXPANDREDO expansion 1 with the old links: link %d:%d, next leaf %d:%d; searched again: link %d:%d, next leaf %d:%d\n",stale_leaf.move,stale_leaf.score,stale_parent.move,stale_parent.score,e1->leaf.move,e1->leaf.score,e1->parent.leaf.move,e1->parent.leaf.score);
  book_expand_apply_ready(&sh);int applied=sh.applied==2;
  printf("EXPANDREDO waited_for_0=%d searched_again=%d (links %d -> %d) applied=%d positions=%d/%d\n",waited,redo,links_before,links_before+1,applied,b[0].n_nodes,b[1].n_nodes);
  int same=b[0].n_nodes==b[1].n_nodes;const Board *boards[3]={&x0,&x,&c0};
  for(int i=0;i<3;++i){Position *p=book_probe(b,boards[i]),*q=book_probe(b+1,boards[i]);if(!p||!q){same=0;continue;}
   int eq=p->leaf.move==q->leaf.move&&p->leaf.score==q->leaf.score&&p->score.value==q->score.value&&p->n_link==q->n_link;
   for(int j=0;eq&&j<p->n_link;++j){int f=0;for(int k=0;k<q->n_link;++k)f|=position_links(p)[j].move==position_links(q)[k].move&&position_links(p)[j].score==position_links(q)[k].score;eq=f;}
   printf("EXPANDREDO position %d: leaf %d:%d / %d:%d, score %d / %d, links %d / %d, same=%d\n",i,p->leaf.move,p->leaf.score,q->leaf.move,q->leaf.score,p->score.value,q->score.value,p->n_link,q->n_link,eq);same=same&&eq;}
  free(sh.slot);condition_free(&sh);lock_free(&sh);book_free(b);book_free(b+1);options_read("book-reproducible","off");search_free(&s);remove("audit-expandredo.tmp");
  if(!waited||!redo||!applied||!same)return 2;
  puts("PASS expandredo: an expansion whose links changed is searched again, and the book is the one of position_expand");
 }
 else if(!strcmp(t,"analyze")){Search s;init_search(&s,1);Game g;game_init(&g);g.initial_board=late_board(4);printf("ANALYZE custom initial empty=%d moves=0\n",board_count_empties(&g.initial_board));fflush(stdout);game_analyze(&g,&s,4,false);search_free(&s);}
 else if(!strcmp(t,"enhance")){Search s;init_search(&s,1);Book b={0};b.search=&s;book_init(&b);b.options.level=0;b.options.n_empties=58;options.book_file="audit-enhance.dat";Board start;board_init(&start);book_add_board(&b,&start);book_enhance(&b,&start,64,64);printf("ENHANCE nodes_after=%d\n",b.n_nodes);book_clean(&b);position_negamax(book_probe(&b,&start),&b);book_clean(&b);position_enhance(book_probe(&b,&start),&b);int pending=0;Position *p;PositionArray *a;Book *bp=&b;foreach_position(p,a,bp)pending+=position_is_todo(p,bp);printf("ENHANCE pending_after_finished=%d\n",pending);book_free(&b);search_free(&s);}
 else if(!strcmp(t,"core")){core(argc>2?atoi(argv[2]):1);}
 else if(!strcmp(t,"taskzero")){ /* the first task of the stack, after the number of tasks changed: bound to its search, its counts from 0 */
  Search s;init_search(&s,2);int bad=0;for(int n=1;n<=4;++n){search_set_task_number(&s,n);Task *t0=s.tasks->task;
   if(s.task!=t0||t0->search!=&s||t0->n_calls!=0||t0->n_nodes!=0||t0->loop)++bad;
   Board b=late_board(10);search_cleanup(&s);search_set_board(&s,&b,BLACK);search_set_level(&s,60,board_count_empties(&b));search_run(&s);
   for(int i=s.tasks->n;i<MAX_THREADS;++i)if(statistics.n_task_nodes[i]||statistics.n_task[i])++bad;}
  search_free(&s);printf("TASK ZERO bad=%d\n",bad);if(bad)return 2;}
 else if(!strcmp(t,"stabwindow")){ /* the window of the options alpha and beta against the scores the stable discs allow [lo, hi] (as search_run computes them): above, touching, overlapping, below; the result keeps the contract of the window */
  Search s;init_search(&s,argc>2?atoi(argv[2]):1);int n_case[4]={0},checked=0;
  const bool verify_reference = argc > 3 && strcmp(argv[3], "--verify-reference") == 0;
  if (verify_reference) {
   ref_cache = (RefEntry*) calloc(1U << REF_CACHE_BITS, sizeof *ref_cache);
   if (!ref_cache) {search_free(&s); fputs("FAIL: cannot allocate reference search cache\n", stderr); return 2;}
  }
  const int n_positions = sizeof stabwindow_reference / sizeof stabwindow_reference[0];
  for(int i=0;i<n_positions;++i){
   Board b=late_board(6+i%9);
   int n=board_count_empties(&b),want=stabwindow_reference[i].score;
   if (b.player != stabwindow_reference[i].player || b.opponent != stabwindow_reference[i].opponent
       || (verify_reference && want != ref_solve(b,-65,65))) {
    fprintf(stderr, "FAIL: reference position %d changed\n", i + 1);
    search_free(&s);free(ref_cache);return 2;
   }
   int hi=MIN(SCORE_MAX,SCORE_MAX-2*get_stability(b.opponent,b.player)+2),lo=MAX(SCORE_MIN,2*get_stability(b.player,b.opponent)-SCORE_MAX-2);
   int win[6][3]={{hi+2,hi+6,0},{hi,hi+2,1},{hi-2,hi+2,2},{lo-2,lo+2,2},{lo-2,lo,1},{lo-6,lo-2,3}}; /* kind: 0 above, 1 touching, 2 overlapping, 3 below */
   for(int k=0;k<6;++k){int a=win[k][0],z=win[k][1];if(a<SCORE_MIN||z>SCORE_MAX||a>=z)continue;
    options.alpha=a;options.beta=z;search_cleanup(&s);search_set_board(&s,&b,BLACK);search_set_level(&s,60,n);search_run(&s);int r=s.result->score;
    bool ok=want<=a?(r<=a&&r>=want):want>=z?(r>=z&&r<=want):r==want;
    if(!ok){printf("FAIL stability window [%d, %d] stable scores [%d, %d] empty=%d expected=%d actual=%d\n",a,z,lo,hi,n,want,r);search_free(&s);free(ref_cache);return 2;}
    ++n_case[win[k][2]];++checked;}}
  options.alpha=SCORE_MIN;options.beta=SCORE_MAX;search_free(&s);free(ref_cache);ref_cache = NULL;
  printf("PASS stability window checks=%d above=%d touching=%d overlapping=%d below=%d\n",checked,n_case[0],n_case[1],n_case[2],n_case[3]);if(!n_case[0]||!n_case[1]||!n_case[2]||!n_case[3])return 2;}
 else if(!strcmp(t,"solid-hash")){return test_solid_hash();}
#if COUNT_LAST_FLIP == COUNT_LAST_FLIP_BMI2
 else if(!strcmp(t,"last-empty-diagonals")){return test_last_empty_diagonals();}
#endif
#if defined(hasSSE2) || defined(__ARM_NEON)
 else if(!strcmp(t,"eval-vectors")){return test_eval_vectors();}
#endif
#ifdef __AVX2__
 else if(!strcmp(t,"stability-pair")){return test_stability_pair();}
#endif
 else if(!strcmp(t,"nboard")){Result result={0};result.n_nodes=1234;result.time=12345;nboard_observer(&result);}
 else if(!strcmp(t,"gtp")||!strcmp(t,"gtptime")||!strcmp(t,"gtpreg")||!strcmp(t,"xboard")){
 if(!have_fixture())return 2;options.book_file=FIXTURE;init_engine(1);UI *ui=ui_new();
 if(!strcmp(t,"xboard")){ui->type=UI_XBOARD;ui_init_xboard(ui);event_add_message(&ui->event,string_duplicate("level 0 5 1"));event_add_message(&ui->event,string_duplicate("quit"));ui_loop_xboard(ui);ui_free_xboard(ui);}
 else {ui->type=UI_GTP;ui_init_gtp(ui);if(!strcmp(t,"gtp")){event_add_message(&ui->event,string_duplicate("42 play white d3"));event_add_message(&ui->event,string_duplicate("7 name"));event_add_message(&ui->event,string_duplicate("name"));}
 else if(!strcmp(t,"gtpreg")){options.level=0;event_add_message(&ui->event,string_duplicate("reg_genmove black"));} else{ui->play->force.real[0].opponent=0x12345678;printf("GTP before forced_board=%llx\n",ui->play->force.real[0].opponent);event_add_message(&ui->event,string_duplicate("time_left banana 99 0"));}
 event_add_message(&ui->event,string_duplicate("quit"));ui_loop_gtp(ui);if(!strcmp(t,"gtptime"))printf("GTP after forced_board=%llx\n",ui->play->force.real[0].opponent);ui_free_gtp(ui);}
 ui_delete(ui);
 }
 else if(!strcmp(t,"formats")){Game src,dst;Random random;random_seed(&random,1234);game_rand(&src,20,&random);src.date.year=2026;src.date.month=9;src.date.day=27;src.date.hour=1;src.date.minute=2;src.date.second=3;
 void (*exports[4])(const Game*,FILE*)={game_export_text,game_export_ggf,game_export_sgf,game_export_pgn};void (*imports[4])(Game*,FILE*)={game_import_text,game_import_ggf,game_import_sgf,game_import_pgn};const char *names[]={"txt","ggf","sgf","pgn"};
 for(int j=0;j<4;++j){FILE *f;TMPFILE(f);exports[j](&src,f);rewind(f);imports[j](&dst,f);printf("FORMAT %s board_equal=%d moves_equal=%d date=%d/%d/%d %d:%d:%d\n",names[j],board_equal(&src.initial_board,&dst.initial_board),memcmp(src.move,dst.move,60)==0,dst.date.year,dst.date.month,dst.date.day,dst.date.hour,dst.date.minute,dst.date.second);fclose(f);}
 game_init(&src);src.initial_board=late_board(4);FILE *f;TMPFILE(f);game_export_pgn(&src,f);rewind(f);game_import_pgn(&dst,f);printf("PGN_CUSTOM expected_empty=%d actual_empty=%d board_equal=%d\n",board_count_empties(&src.initial_board),board_count_empties(&dst.initial_board),board_equal(&src.initial_board,&dst.initial_board));fclose(f);
 }
 else if(!strcmp(t,"pgnlong")){Game g;FILE *f;TMPFILE(f);fputs("[Event \"audit\"]\n",f);for(int i=0;i<300;++i)fprintf(f,"%d. d3 c3 ",i+1);rewind(f);game_import_pgn(&g,f);fclose(f);}
 else if(!strcmp(t,"pgnfen")){Game g;Board want=late_board(12);char fen[128];FILE *f;TMPFILE(f);fprintf(f,"[FEN \"%s\"]\n*\n",board_to_FEN(&want,BLACK,fen));rewind(f);game_import_pgn(&g,f);printf("PGN FEN expected_empty=%d actual_empty=%d\n",board_count_empties(&want),board_count_empties(&g.initial_board));fclose(f);}
 else if(!strcmp(t,"unsupported")){FILE *f;OPEN(f,"audit-sentinel.xyz","wb");fputs("KEEP THIS FILE",f);fclose(f);Play *p=mm_malloc(sizeof *p);memset(p,0,sizeof *p);board_init(&p->initial_board);p->initial_player=BLACK;play_save(p,"audit-sentinel.xyz");OPEN(f,"audit-sentinel.xyz","rb");fseek(f,0,SEEK_END);printf("UNSUPPORTED SAVE bytes_before=14 bytes_after=%ld\n",ftell(f));fclose(f);mm_free(p);}
 else if(!strcmp(t,"bookio")){Book b={0},c={0};FILE *f;if(!have_fixture())return 2;OPEN(f,"audit-book-saved.dat","wb");fputs("KEEP",f);fclose(f);if(!book_load(&b,FIXTURE)||b.n_nodes!=2)return 2;if(!book_save(&b,"audit-book-saved.dat"))return 2;if(!book_load(&c,"audit-book-saved.dat")||c.n_nodes!=2)return 2;printf("BOOK save and reload nodes=%d\n",c.n_nodes);book_free(&b);book_free(&c);}
 else if(!strcmp(t,"truncated")){if(copy_cut(FIXTURE,"audit-truncated.dat")<0)return 2;Book b={0};int ok=book_load(&b,"audit-truncated.dat");printf("TRUNCATED accepted=%d nodes=%d original_expected=2\n",ok,b.n_nodes);book_free(&b);if(ok)return 2;}
 else if(!strcmp(t,"corruptstartup")){long n=copy_cut(FIXTURE,"audit-truncated.dat");if(n<0)return 2;options.book_file="audit-truncated.dat";init_engine(1);UI *ui=ui_new();ui->type=UI_GTP;ui_init_gtp(ui);int safe=!ui->book.need_saving;ui_free_gtp(ui);ui_delete(ui);FILE *in;OPEN(in,"audit-truncated.dat","rb");fseek(in,0,SEEK_END);long after=ftell(in);fclose(in);printf("CORRUPT STARTUP autosave_disabled=%d original_bytes=%ld after_bytes=%ld\n",safe,n-1,after);if(!safe||after!=n-1)return 2;}
 else if(!strcmp(t,"ggsclock")){GGSClock c;char input[WORD_SIZE+1];memset(input,'1',WORD_SIZE);input[WORD_SIZE]=0;ggs_parse_clock(&c,input);}
 else if(!strcmp(t,"cassio")){init_engine(1);Engine *e=calloc(1,sizeof *e);e->search=engine_create_search();for(int i=0;i<24;++i){Board b=late_board(3+i%7);char text[80];int want=ref_solve(b,-65,65);for(int repeat=0;repeat<2;++repeat){int got=engine_endgame_search(e,board_to_string(&b,BLACK,text),-64,64,100);if(got!=want){printf("FAIL Cassio expected=%d actual=%d\n",want,got);return 2;}}}engine_free(e->search);free(e);puts("PASS Cassio endgame API 24 positions x 2 repeated searches");}
 else if(!strcmp(t,"events")){Event ev;event_init(&ev);int expected=0;for(int i=0;i<10000;++i){char b[32];sprintf(b,"%d",i);event_add_message(&ev,string_duplicate(b));if(i%3==0){char *s=event_peek_message(&ev);if(!s||atoi(s)!=expected++)return 2;free(s);}}char *s;while((s=event_peek_message(&ev))){if(atoi(s)!=expected++)return 2;free(s);}printf("PASS event FIFO %d messages with wrap and growth\n",expected);event_free(&ev);}
 else if(!strcmp(t,"missinglink")){if(!have_fixture())return 2;Search s;init_search(&s,1);Book b={0};if(!book_load(&b,FIXTURE)||b.n_nodes!=2)return 2;b.search=&s;Position *root=book_root(&b),*child=NULL,*p;PositionArray *a;Book *bp=&b;foreach_position(p,a,bp)if(p!=root)child=p;if(!root||!child||root->n_link==0)return 2;{PositionArray *ca=b.array+(board_get_hash_code(&child->board)&(b.n-1));book_remove_at(&b,ca,(int)(child-ca->positions));}book_clean(&b);position_negamax(book_root(&b),&b);book_fix(&b);root=book_root(&b);printf("MISSING LINK nodes=%d root_links_after_fix=%d\n",b.n_nodes,root->n_link);if(b.n_nodes!=1||root->n_link!=0)return 2;book_free(&b);search_free(&s);}
 else if(!strcmp(t,"mergefile")){if(copy_cut(FIXTURE,"audit-merge-truncated.dat")<0)return 2;Book d={0};book_init(&d);int ok=book_merge_file(&d,"audit-merge-truncated.dat");printf("MERGE truncated_source accepted=%d destination_nodes=%d\n",ok,d.n_nodes);if(ok||d.n_nodes!=0)return 2;ok=book_merge_file(&d,FIXTURE);printf("MERGE complete_source accepted=%d destination_nodes=%d\n",ok,d.n_nodes);if(!ok||d.n_nodes!=2)return 2;book_free(&d);}
 else if(!strcmp(t,"linkstore")){Position p;Link l;int i,ok;position_init(&p);for(i=0;i<7;++i){l.move=(unsigned char)i;l.score=(signed char)i;position_add_link(&p,&l);}ok=p.n_link==7&&position_links(&p)[6].move==6;{Link keep[3]={{1,1},{3,3},{5,5}};position_set_links(&p,keep,3);}ok=ok&&p.n_link==3&&position_links(&p)[2].move==5;for(i=10;i<13;++i){l.move=(unsigned char)i;l.score=0;position_add_link(&p,&l);}ok=ok&&p.n_link==6&&position_links(&p)[0].move==1&&position_links(&p)[5].move==12;position_free(&p);printf("LINK storage inline/heap ok=%d\n",ok);if(!ok)return 2;}
 else if(!strcmp(t,"mergetwice")){ /* a second merge must not take the positions of the first one for duplicated positions; a merge that fails must only remove what it added */
  Book s={0},d={0};Position p;Board b;FILE *f;long n;unsigned char *data;unsigned int count=2;int ok;
  book_init(&s);board_init(&b);position_init(&p);board_unique(&b,&p.board);p.level=1;if(book_add(&s,&p)<=0)return 2;
  b=late_board(40);position_init(&p);board_unique(&b,&p.board);p.level=1;if(book_add(&s,&p)<=0)return 2; /* a position out of reach from the root */
  if(!book_save(&s,"audit-merge-a.dat"))return 2;book_free(&s);
  book_init(&s);b=late_board(38);position_init(&p);board_unique(&b,&p.board);p.level=1;if(book_add(&s,&p)<=0)return 2;if(!book_save(&s,"audit-merge-one.dat"))return 2;book_free(&s);
  OPEN(f,"audit-merge-one.dat","rb");fseek(f,0,SEEK_END);n=ftell(f);rewind(f);data=malloc(n);if(fread(data,1,n,f)!=(size_t)n)return 2;fclose(f);
  memcpy(data+38,&count,4);OPEN(f,"audit-merge-dup.dat","wb");fwrite(data,1,n,f);fwrite(data+42,1,n-42,f);fclose(f);free(data); /* the same position twice */
  book_init(&d);
  ok=book_merge_file(&d,"audit-merge-a.dat");printf("MERGE first accepted=%d nodes=%u\n",ok,d.n_nodes);if(!ok||d.n_nodes!=2)return 2;
  ok=book_merge_file(&d,"audit-merge-a.dat");printf("MERGE same book again accepted=%d nodes=%u\n",ok,d.n_nodes);if(!ok||d.n_nodes!=2)return 2;
  ok=book_merge_file(&d,"audit-merge-dup.dat");printf("MERGE book with a duplicated position accepted=%d nodes=%u\n",ok,d.n_nodes);if(ok||d.n_nodes!=2)return 2;
  book_free(&d);remove("audit-merge-a.dat");remove("audit-merge-one.dat");remove("audit-merge-dup.dat");}
 else if(!strcmp(t,"negamaxloop")){ /* damaged book: a link that leads back to its own position must not make the negamax with threads endless */
  Book b={0};Position p;Board board;Link l={0,PASS};
  book_init(&b);board_init(&board);position_init(&p);board_unique(&board,&p.board);p.level=1;position_add_link(&p,&l);if(book_add(&b,&p)<=0)return 2;
  options.n_task=4;book_negamax(&b);printf("NEGAMAX with a link that leads back: done, nodes=%u\n",b.n_nodes);book_free(&b);}
 else if(!strcmp(t,"learnline")){ /* the first moves of a game to learn are copied without their spaces: a long line is not cut between two moves */
  char line[400],copy[LEARN_MOVES_SIZE];int i,ok;
  strcpy(line,"F5d6");for(i=4;i<253;++i)line[i]=' ';strcpy(line+253,"c3g5");
  learn_moves_copy(copy,line);ok=!strcmp(copy,"f5d6c3g5");printf("LEARN line of %d bytes copied as \"%s\" ok=%d\n",(int)strlen(line),copy,ok);if(!ok)return 2;}
 else if(!strcmp(t,"hashlock")){ /* v4.5.5-nikque.16: the hash codes of the same entries take the same lock (the lock was hash_code & lock_mask) */
  HashTable h={0};int bad=0;hash_init(&h,1024);
  for(unsigned long long k=0;k<100000;++k){unsigned long long c=k*0x9E3779B97F4A7C15ULL;for(int d=1;d<HASH_N_WAY;++d)if(hash_lock(&h,c)!=hash_lock(&h,c^(unsigned long long)d))++bad;}
  printf("HASH lock of the same entries: n_lock=%d hash_mask=%llu different=%d\n",h.n_lock,h.hash_mask,bad);hash_cleanup(&h);free(h.memory);if(bad)return 2;}
 else if(!strcmp(t,"lastgame")){ /* v4.5.5-nikque.16: the last game of a file without an end of line, and of a pgn file, is loaded */
  const char *ext[]={".txt",".pgn",".ggf",".sgf",".edx"};const char *line[]={"f5d6c3d3c4f4f6f3e6e7d7g6g5f7e8","f5f6e6f4e3c5c4","f5d6c5f4e3"};int ok=1;
  for(int e=0;e<5;++e){char name[32];Base b;Game g;FILE *f;long n;char *buf;sprintf(name,"audit-lastgame%s",ext[e]);remove(name);base_init(&b);
   for(int i=0;i<3;++i){game_init(&g);text_to_game(line[i],&g);base_append(&b,&g);}base_save(&b,name);base_free(&b);
   OPEN(f,name,"rb");fseek(f,0,SEEK_END);n=ftell(f);rewind(f);buf=malloc(n);if(fread(buf,1,n,f)!=(size_t)n)n=0;fclose(f);if(e<4)while(n>0&&(buf[n-1]=='\n'||buf[n-1]=='\r'))--n;
   OPEN(f,name,"wb");fwrite(buf,1,n,f);fclose(f);free(buf);base_init(&b);int loaded=base_load(&b,name);printf("BASE %s without the final end of line: loaded=%d games=%d (expected 3)\n",ext[e],loaded,b.n_games);
   if(!loaded||b.n_games!=3||b.game[2].move[4]!=g.move[4]){ok=0;}base_free(&b);remove(name);}
  if(!ok)return 2;}
 else if(!strcmp(t,"basereplace")){ /* v4.5.5-nikque.16: base correct/complete save to another file first, then replace the file */
  Base b;Game g;FILE *f;int ok;remove("audit-replace.txt");remove("audit-replace.txt.saving.txt");OPEN(f,"audit-replace.txt","w");fputs("f5d6\nf5f6\nf5d6c3",f);fclose(f);
  base_init(&b);ok=base_load(&b,"audit-replace.txt")&&b.n_games==3;game_init(&g);text_to_game("f5f4",&g);b.game[0]=g;ok=ok&&base_replace(&b,"audit-replace.txt");base_free(&b);
  base_init(&b);ok=ok&&base_load(&b,"audit-replace.txt")&&b.n_games==3&&b.game[0].move[1]==g.move[1];base_free(&b);if(file_exists("audit-replace.txt.saving.txt"))ok=0;
  printf("BASE replace: 3 games, first one changed, no file left aside ok=%d\n",ok);remove("audit-replace.txt");if(!ok)return 2;}
 else if(!strcmp(t,"keepunread")){ /* v4.5.5-nikque.16: a book file that could not be loaded is kept as .damaged before a book saved to another file replaces it */
  Search s;init_search(&s,1);Book b={0};FILE *f;int ok,kept;remove("audit-unread.dat");remove("audit-unread.dat.damaged");remove("audit-unread.dat.saving");
  OPEN(f,"audit-unread.dat","wb");for(int i=0;i<300;++i)fputc(i*7,f);fclose(f);b.search=&s;ok=!book_load(&b,"audit-unread.dat");book_free(&b);
  book_init(&b);b.search=&s;book_new(&b,2,36);ok=ok&&book_save(&b,"audit-unread.dat.saving");ok=ok&&book_keep_unread_file("audit-unread.dat");
  f=fopen("audit-unread.dat.damaged","rb");kept=f!=NULL;if(f){fseek(f,0,SEEK_END);kept=ftell(f)==300;fclose(f);}ok=ok&&kept&&rename("audit-unread.dat.saving","audit-unread.dat")==0;
  ok=ok&&book_keep_unread_file("audit-unread.dat");if(file_exists("audit-unread.dat.damaged.1"))ok=0; /* (once kept, the file is no longer the one that could not be loaded) */
  printf("BOOK unread file kept as .damaged (300 bytes)=%d ok=%d\n",kept,ok);book_free(&b);remove("audit-unread.dat");remove("audit-unread.dat.damaged");search_free(&s);if(!ok)return 2;}
 else if(!strcmp(t,"statsrange")){ /* v4.5.5-nikque.16: book stats counts apart the scores out of -64..64 (a book file holds up to +/-127) */
  Search s;init_search(&s,1);Book b={0};Board init;Position *p;b.search=&s;book_new(&b,2,36);board_init(&init);p=book_probe(&b,&init);if(!p)return 2;p->score.value=127;
  book_stats(&b);p->score.value=-127;book_stats(&b);printf("BOOK stats with scores +/-127 done\n");book_free(&b);search_free(&s);}
 else if(!strcmp(t,"gamescore")){ /* v4.5.5-nikque.16: the score of a game of an odd number of moves had the wrong sign */
  int bad[2]={0,0},total[2]={0,0};
  for(int k=0;k<4000;++k){Game g;Board b;int n=0,black=1;game_init(&g);board_init(&b);
   for(;;){unsigned long long m=board_get_moves(&b);if(!m){board_pass(&b);black^=1;m=board_get_moves(&b);if(!m)break;}int x=random_move(m);g.move[n++]=x;board_next(&b,x,&b);black^=1;}
   int p=bit_count(b.player),o=bit_count(b.opponent),e=64-p-o,d=p-o;if(d>0)d+=e;else if(d<0)d-=e;++total[n&1];if(game_score(&g)!=(black?d:-d))++bad[n&1];}
  printf("GAME score: even moves %d games %d wrong, odd moves %d games %d wrong\n",total[0],bad[0],total[1],bad[1]);if(bad[0]||bad[1]||!total[1])return 2;}
 else if(!strcmp(t,"badinput")){ /* v4.5.5-nikque.16: FEN rows, edx games, boards of fewer than 4 discs, book depth */
  Board b;Game g;FILE *f;int ok=1;
  if(board_from_FEN(&b,"9p b")!=EMPTY||board_from_FEN(&b,"8/8/8/8/8/8/8/8/8/p b")!=EMPTY||board_from_FEN(&b,"8/8/8/3Pp3/3pP3/8/8 b")!=EMPTY)ok=0;
  if(board_from_FEN(&b,"8/8/8/3Pp3/3pP3/8/8/8 b")!=BLACK||board_count_empties(&b)!=60)ok=0;
  b.player=1;b.opponent=2;if(board_is_usable(&b))ok=0;board_init(&b);if(!board_is_usable(&b))ok=0;
  game_init(&g);g.move[0]=100;OPEN(f,"audit-bad.edx","wb");fwrite(&g,sizeof g,1,f);fclose(f);OPEN(f,"audit-bad.edx","rb");if(game_read_checked(&g,f))ok=0;fclose(f);
  game_init(&g);g.player=7;OPEN(f,"audit-bad.edx","wb");fwrite(&g,sizeof g,1,f);fclose(f);OPEN(f,"audit-bad.edx","rb");if(game_read_checked(&g,f))ok=0;fclose(f);
  {Base base;base_init(&base);if(base_load(&base,"audit-bad.edx"))ok=0;base_free(&base);}remove("audit-bad.edx");
  {Line l;line_init(&l,BLACK);for(int i=0;i<GAME_SIZE;++i)line_push(&l,PASS);if(line_push(&l,PASS)||l.n_moves!=GAME_SIZE)ok=0;}
  printf("BAD input refused ok=%d\n",ok);if(!ok)return 2;}
 else if(!strcmp(t,"wthortrunc")){ /* v4.5.5-nikque.16: a wthor file that cannot be read completely is not loaded, and no game is added to it */
  Base b;Game g;FILE *f;long n;char buf[1024];int ok;remove("audit-trunc.wtb");base_init(&b);for(int i=0;i<3;++i){game_init(&g);text_to_game(i?"f5f6e6f4":"f5d6c3",&g);base_append(&b,&g);}
  ok=base_save(&b,"audit-trunc.wtb");base_free(&b);{Base c;base_init(&c);ok=ok&&base_load(&c,"audit-trunc.wtb")&&c.n_games==3;base_free(&c);}
  OPEN(f,"audit-trunc.wtb","rb");n=fread(buf,1,sizeof buf,f);fclose(f);OPEN(f,"audit-trunc.wtb","wb");fwrite(buf,1,n-30,f);fclose(f);{Base c;base_init(&c);ok=ok&&!base_load(&c,"audit-trunc.wtb");base_free(&c);} /* (a truncated wthor file is not loaded) */
  base_init(&b);game_init(&g);text_to_game("f5f4",&g);base_append(&b,&g);ok=ok&&!base_save(&b,"audit-trunc.wtb");base_free(&b);OPEN(f,"audit-trunc.wtb","rb");fseek(f,0,SEEK_END);ok=ok&&ftell(f)==n-30;fclose(f);
  printf("WTHOR truncated file kept ok=%d\n",ok);remove("audit-trunc.wtb");remove("WTHOR.JOU");if(!ok)return 2;}
 else if(!strcmp(t,"cassiolist")){ /* v4.5.5-nikque.16: the list of the positions of Cassio keeps every position */
  init_engine(1);
  Engine *e=calloc(1,sizeof *e);Board b[3];int ok;e->search=engine_create_search();e->last_position.n=0;
  for(int i=0;i<3;++i){b[i]=late_board(20+i);is_position_new(e,b+i);}
  ok=e->last_position.n==3&&board_equal(e->last_position.board,b+2)&&board_equal(e->last_position.board+1,b+1)&&board_equal(e->last_position.board+2,b);
  ok=ok&&!is_position_new(e,b);printf("CASSIO position list of 3 ok=%d\n",ok);engine_free(e->search);free(e);if(!ok)return 2;}
 else if(!strcmp(t,"parsers")){ /* v4.5.5-nikque.16: D:HH:MM:SS times, "P@" alone, option files that read themselves, mode */
  int ok=string_to_time("0:1:0:0")==3600000&&string_to_time("1:0:0:0")==86400000&&string_to_time("1:00:00")==3600000&&string_to_time("1:30")==90000&&string_to_time("2:03.5")==123500;
  /* an empty field is 0 (up to v4.5.5-nikque.26 the field before it: "5:" was 5 minutes 5 seconds) */
  printf("PARSERS times with an empty field: 5: %lld ms, 1:: %lld ms, 2:3: %lld ms\n",string_to_time("5:"),string_to_time("1::"),string_to_time("2:3:"));
  ok=ok&&string_to_time("5:")==300000&&string_to_time("1::")==3600000&&string_to_time("2:3:")==7380000;
  char *p=malloc(3);strcpy(p,"P@");ok=ok&&string_to_coordinate(p)==NOMOVE;free(p);p=malloc(2);strcpy(p,"@");ok=ok&&string_to_coordinate(p)==NOMOVE;free(p);
  ok=ok&&string_to_coordinate("P@d3")==D3&&string_to_coordinate("@@@@")==PASS;
  FILE *f;OPEN(f,"audit-loop.ini","w");fputs("option-file audit-loop.ini\nmode 9\n",f);fclose(f);options_parse("audit-loop.ini");options_bound();ok=ok&&options.mode==3;remove("audit-loop.ini");
  printf("PARSERS times, coordinates, option-file loop, mode ok=%d\n",ok);if(!ok)return 2;}
 else if(!strcmp(t,"tools")){ /* v4.5.5-nikque.16: opening names, shapes after a pass, the moves of an OBF line */
  int ok=1;const char *l;ShapeHash sh;BoardCache bc;Board b;unsigned seed=3;int found=0;OBF obf;FILE *f;
  l=opening_get_line("tigre centrale");ok=ok&&l&&!strcmp(l,"f5d6c3d3c4f4c5");l=opening_get_line("tigre");ok=ok&&l&&!strcmp(l,"f5d6c3d3c4");
  while(!found){board_init(&b);for(;;){unsigned long long m=board_get_moves(&b);if(!m){Board p=b;board_pass(&p);if(board_get_moves(&p)&&board_count_empties(&b)>3)found=1;break;}int k=(int)((seed=seed*1103515245+12345)>>16);k%=bit_count(m);while(k--)m&=m-1;board_next(&b,first_bit(m),&b);}}
  {Board p=b;board_pass(&p);shapehash_init(&sh,12);boardcache_init(&bc,12);ok=ok&&count_shape(&sh,&bc,&b,1)==(unsigned long long)bit_count(board_get_moves(&p));shapehash_delete(&sh);boardcache_delete(&bc);}
  OPEN(f,"audit-moves.obf","w");fputs("---------------------------OX------XO--------------------------- X",f);for(int i=0;i<40;++i)fputs(";f5:+1",f);fputs(";\n",f);fclose(f);
  OPEN(f,"audit-moves.obf","r");memset(&obf,0,sizeof obf);obf_read(&obf,f);fclose(f);ok=ok&&obf.n_moves==MAX_MOVE;obf_free(&obf);remove("audit-moves.obf");
  printf("TOOLS opening names, shapes after a pass, OBF moves ok=%d\n",ok);if(!ok)return 2;}
 else if(!strcmp(t,"unique")){ /* v4.5.5-nikque.16: base unique sorts the games (the first ones kept, in order); games from other boards differ */
  Base b;Game g,c;int ok;base_init(&b);
  for(int i=0;i<300;++i){game_init(&g);text_to_game(i%3?"f5d6c3":"f5f6e6f4",&g);sprintf(g.name[0],"p%d",i%50);base_append(&b,&g);}
  c=g;c.initial_board.player^=1ULL<<A1;base_append(&b,&c);base_unique(&b);
  ok=b.n_games==101&&!strcmp(b.game[0].name[0],"p0")&&b.game[0].move[1]==F6&&!strcmp(b.game[1].name[0],"p1")&&b.game[1].move[1]==D6&&!board_equal(&b.game[100].initial_board,&b.game[99].initial_board);
  printf("BASE unique: %d games kept (101 expected) ok=%d\n",b.n_games,ok);base_free(&b);if(!ok)return 2;}
 else if(!strcmp(t,"importscore")){ /* a line of a text book whose leaf score is out of -64..64 holds no position (up to v4.5.5-nikque.26 300 was read as 44) */
  Board b[3];char s[80],m[4];FILE *f;Book k={0};const int score[3]={6,300,-65};int ok;board_init(b);board_next(b,F5,b+1);board_next(b+1,(int)first_bit(board_get_moves(b+1)),b+2);
  OPEN(f,"audit-import.txt","w");for(int i=0;i<3;++i){move_to_string((int)first_bit(board_get_moves(b+i)),BLACK,m);fprintf(f,"%s,10,%s,%d\n",board_to_string(b+i,BLACK,s),m,score[i]);}fclose(f);
  ok=book_import(&k,"audit-import.txt");Position *p=ok?book_probe(&k,b):NULL;
  printf("IMPORT leaf scores 6, 300, -65: imported=%d positions=%u (1 expected), leaf score of the first %d\n",ok,ok?k.n_nodes:0,p?p->leaf.score:-999);
  for(int i=1;ok&&i<3;++i){Position *q=book_probe(&k,b+i);if(q)printf("IMPORT line %d imported with the leaf score %d\n",i+1,q->leaf.score);}
  ok=ok&&k.n_nodes==1&&p&&p->leaf.score==6;if(k.array)book_free(&k);remove("audit-import.txt");if(!ok)return 2;}
 else if(!strcmp(t,"mergeorient")){ /* book merge of a position written in another orientation (a book file of another tool): the merge links it
   (up to v4.5.5-nikque.26 it was put in the bucket of its board as written, linked to nothing, then moved by book fix) */
  Search s;init_search(&s,1);Book src={0},d={0};Position p;Board b,x,u;int m=NOMOVE,ok;const int first[4]={F5,D3,C4,E6};board_init(&b);
  for(int i=0;i<4&&m==NOMOVE;++i){board_next(&b,first[i],&x);board_unique(&x,&u);if(!board_equal(&x,&u))m=first[i];}
  if(m==NOMOVE)return 2;
  book_init(&src);position_init(&p);p.board=x;p.level=1;p.leaf.move=(unsigned char)first_bit(board_get_moves(&x));p.leaf.score=0;p.score.value=0;if(book_add(&src,&p)<=0||!book_save(&src,"audit-merge-orient.dat"))return 2;book_free(&src);
  d.search=&s;book_new(&d,1,49);ok=book_merge_file(&d,"audit-merge-orient.dat");
  if(ok){book_link_parallel(&d);book_fix(&d);book_negamax(&d);} /* (as play_book_merge) */
  Position *r=book_probe(&d,&b),*c=book_probe(&d,&x);
  printf("MERGE a position in another orientation: merged=%d positions=%u, links of the root %d, position found=%d\n",ok,d.n_nodes,r?r->n_link:-1,c!=NULL);
  ok=ok&&d.n_nodes==2&&r&&r->n_link>0&&c;
  /* the same position twice, in two orientations: a duplicated position, as the same board twice (the merge is refused) */
  book_init(&src);position_init(&p);p.board=x;p.level=1;if(book_add(&src,&p)<=0)return 2;position_init(&p);p.board=u;p.level=1;if(book_add(&src,&p)<=0||!book_save(&src,"audit-merge-orient.dat"))return 2;book_free(&src);
  book_free(&d);d.search=&s;book_new(&d,1,49);int twice=book_merge_file(&d,"audit-merge-orient.dat");
  printf("MERGE the same position in two orientations to a new book: merged=%d positions=%u\n",twice,d.n_nodes);ok=ok&&!twice&&d.n_nodes==1;
  book_free(&d);search_free(&s);remove("audit-merge-orient.dat");if(!ok)return 2;}
 else if(!strcmp(t,"deviate254")){ /* book deviate2/3 export with a cumulative loss above 254: reduced with the warning of book deviate2/3, in book_deviate_export
   (up to v4.5.5-nikque.26 edax.c and libedax.c each had their own: libedax said "cumulative loss = 300 is out of range") */
  if(!have_fixture())return 2;Search s;init_search(&s,1);Book k={0};Board b;char *e;long long n;int ok;
  if(!book_load(&k,FIXTURE))return 2;k.search=&s;board_init(&b);
  if(!capture_begin(2))return 2;n=book_deviate_export(&k,&b,NULL,3,2,300,"audit-dev254.txt",BOOK_EXPORT_ALL);e=capture_end();
  ok=e&&strstr(e,"WARNING: total loss 300 reduced to 254")!=NULL;
  printf("DEVIATE export with the cumulative loss 300: %lld lines, the warning \"total loss 300 reduced to 254\"=%d\n",n,ok);
  free(e);remove("audit-dev254.txt");book_free(&k);search_free(&s);if(!ok)return 2;}
 else if(!strcmp(t,"deviateexport")){ /* the words after the numbers of book deviate, deviate2, deviate3: a file name between quotes may hold spaces; a name too long,
   a quote not closed and a word other than export are errors, and the book is not expanded (up to v4.5.5-nikque.26 the name was cut, and another word skipped) */
  if(!have_fixture())return 2;options.book_file=FIXTURE;init_engine(1);UI *ui=ui_new();ui->type=UI_EDAX;ui_init_edax(ui);Play *play=ui->play;
  static char long_name[FILENAME_MAX+64];strcpy(long_name,"export ");memset(long_name+7,'a',FILENAME_MAX+5);long_name[7+FILENAME_MAX+5]=0;
  static const struct{const char *param;bool handled;const char *file,*said;}c[]={
   {"",false,NULL,NULL},{"   ",false,NULL,NULL},{"export audit-dev-a.txt ends",true,"audit-dev-a.txt","lines written"},
   {"export \"audit dev b.txt\" all",true,"audit dev b.txt","lines written"},{"EXPORT \"audit dev c.txt\"",true,"audit dev c.txt","lines written"},
   {"exprot audit-dev-d.txt",true,NULL,"unknown word exprot"},{"7",true,NULL,"unknown word 7"},{"export \"audit dev e.txt",true,NULL,"quote"},
   {"export",true,NULL,"no file"},{"export \"\" all",true,NULL,"no file"},{long_name,true,NULL,"too long"}};
  int ok=1;
  for(int i=0;i<(int)(sizeof c/sizeof *c);++i){char *said;bool handled;if(c[i].file)remove(c[i].file);
   if(!capture_begin(1))return 2;handled=deviate_export(play,c[i].param,3,2,4);said=capture_end();
   const bool written=c[i].file?file_exists(c[i].file):true,told=!c[i].said||(said&&strstr(said,c[i].said));
   printf("DEVIATE \"%.40s\": handled=%d (%d expected), file written=%d, told \"%s\"=%d\n",c[i].param,handled,c[i].handled,written,c[i].said?c[i].said:"",told);
   ok=ok&&handled==c[i].handled&&written&&told;free(said);if(c[i].file)remove(c[i].file);}
  ok=ok&&!file_exists("audit-dev-d.txt")&&!file_exists("audit dev e.txt")&&!file_exists("\"audit");
  ui_free_edax(ui);ui_delete(ui);if(!ok)return 2;}
 else if(!strcmp(t,"configpath")){ /* config.ini next to the program; with a name of its folder too long, none (up to v4.5.5-nikque.26 the one of the current folder, with the warning of a program started without its folder) */
  static char exe[FILENAME_MAX+64];char c[FILENAME_MAX+1];int ok;
  get_config_file_path("C:\\x\\edax.exe",c);ok=!strcmp(c,"C:\\x\\config.ini");get_config_file_path("bin/edax",c);ok=ok&&!strcmp(c,"bin/config.ini");get_config_file_path("edax",c);ok=ok&&!strcmp(c,"config.ini");
  printf("CONFIG next to the program ok=%d\n",ok);
  memset(exe,'a',FILENAME_MAX);exe[FILENAME_MAX-6]='\\';strcpy(exe+FILENAME_MAX-5,"edax");strcpy(c,"x");get_config_file_path(exe,c);
  printf("CONFIG of a program in a folder of %d characters: \"%s\" (none expected)\n",FILENAME_MAX-6,c);ok=ok&&c[0]==0;if(!ok)return 2;}
 else if(!strcmp(t,"histogram")){ /* histogram_print (weval) with rows and columns of 0 and 1 position: no division by 0 (up to v4.5.5-nikque.26 nan and inf were printed) */
  static unsigned long long h[129][65];char *out;int ok;histogram_init(h);
  if(!capture_begin(1))return 2;histogram_print(h);h[64][32]=1;h[70][40]=1;h[70][41]=1;histogram_print(h);out=capture_end();if(!out)return 2;
  for(char *c=out;*c;++c)*c=(char)tolower((unsigned char)*c);
  ok=!strstr(out,"nan")&&!strstr(out,"inf")&&!strstr(out,"ind");printf("HISTOGRAM of 0, then 3 positions: %d characters, without nan nor inf=%d\n",(int)strlen(out),ok);free(out);
  /* histogram_stats of no position: no division by 0 (up to v4.5.5-nikque.26 nan was printed) */
  histogram_init(h);if(!capture_begin(1))return 2;histogram_stats(h);out=capture_end();if(!out)return 2;
  for(char *c=out;*c;++c)*c=(char)tolower((unsigned char)*c);
  {const int ok0=!strstr(out,"nan")&&!strstr(out,"inf")&&!strstr(out,"ind");printf("HISTOGRAM stats of 0 position: without nan nor inf=%d\n",ok0);ok=ok&&ok0;}free(out);
  /* histogram_stats of 1 position, then of 3 positions of the same evaluation: no variance, no division by 0 */
  histogram_init(h);h[64][32]=1;if(!capture_begin(1))return 2;histogram_stats(h);h[70][40]=2;h[70][41]=1;h[64][32]=0;histogram_stats(h);out=capture_end();if(!out)return 2;
  for(char *c=out;*c;++c)*c=(char)tolower((unsigned char)*c);
  {const int ok1=!strstr(out,"nan")&&!strstr(out,"inf")&&!strstr(out,"ind")&&strstr(out,"undefined");printf("HISTOGRAM stats of 1 position, of 1 evaluation: without nan nor inf=%d\n",ok1);ok=ok&&ok1;}free(out);
  /* every row and column of many positions: the text, as before (its hash) */
  for(int i=0;i<129;++i)for(int j=0;j<65;++j)h[i][j]=1+rr()%5;
  if(!capture_begin(1))return 2;histogram_print(h);out=capture_end();if(!out)return 2;{unsigned long long x=1469598103934665603ULL;for(char *c=out;*c;++c)x=(x^(unsigned char)*c)*1099511628211ULL;printf("HISTOGRAM of random counts: %d characters, hash %016llx\n",(int)strlen(out),x);}
  free(out);
  if(!capture_begin(1))return 2;histogram_stats(h);out=capture_end();if(!out)return 2;{unsigned long long x=1469598103934665603ULL;for(char *c=out;*c;++c)x=(x^(unsigned char)*c)*1099511628211ULL;printf("HISTOGRAM stats of random counts: %d characters, hash %016llx\n",(int)strlen(out),x);}
  free(out);if(!ok)return 2;}
 else if(!strcmp(t,"gameio")){ /* game files: a ggf or sgf game cut by the end of the file, dates and numbers, sgf of a game from a position, the hash of a corrected
   game, the nboard game command, the names of the files of a wthor base, the year and depth of a wthor file, OBF boards and output, opening names */
  int ok=1,r;Base b;FILE *f;Game g,h;char s[256];long n0,n1;
  static const char *ggf[2]={"(;GM[othello]PB[a]PW[b]B[f5]W[d6];)\n(;GM[othello]PB[c]PW[d]B[f5]W[f6];)\n","(;GM[othello]PB[e]B[f5]W["};
  static const char *sgf[2]={"(;FF[4]GM[2]SZ[8]PB[a]PW[b];B[f5];W[d6])\n(;FF[4]GM[2]SZ[8]PB[c]PW[d];B[f5];W[f6])\n","(;FF[4]GM[2]SZ[8];B[f5];W["};
  for(int k=0;k<2;++k){const char *name=k?"audit-cut.sgf":"audit-cut.ggf";const char **x=k?sgf:ggf;
   OPEN(f,name,"wb");fputs(x[0],f);fclose(f);base_init(&b);r=base_load(&b,name);printf("GAMEIO %s of 2 complete games: loaded=%d games=%d\n",name,r,b.n_games);ok=ok&&r&&b.n_games==2;base_free(&b);
   OPEN(f,name,"wb");fputs(x[0],f);fputs(x[1],f);n0=ftell(f);fclose(f);base_init(&b);r=base_load(&b,name);if(r)base_replace(&b,name); /* (as base correct) */
   base_free(&b);OPEN(f,name,"rb");fseek(f,0,SEEK_END);n1=ftell(f);fclose(f);
   printf("GAMEIO %s with a game cut by its end: loaded=%d (0 expected), size %ld -> %ld\n",name,r,n0,n1);ok=ok&&!r&&n0==n1;remove(name);}
  {static const struct{const char *dt;int y,mo,d,h;}c[]={{"DT[2003.11.21_17:32:51.met]",2003,11,21,17},{"DT[2003.11]",0,0,0,-1},{"DT[2003.13.21_17:32:51]",0,0,0,-1},{"DT[1069432371]",0,0,0,-1}};
   for(int i=0;i<4;++i){sprintf(s,"(;GM[othello]%sB[f5];)\n",c[i].dt);OPEN(f,"audit-date.ggf","wb");fputs(s,f);fclose(f);OPEN(f,"audit-date.ggf","r");game_import_ggf(&g,f);fclose(f);
    printf("GAMEIO GGF %s: %d.%d.%d %d h\n",c[i].dt,g.date.year,g.date.month,g.date.day,g.date.hour);ok=ok&&g.date.year==c[i].y&&g.date.month==c[i].mo&&g.date.day==c[i].d&&g.date.hour==c[i].h&&g.move[0]==F5;}
   remove("audit-date.ggf");}
  OPEN(f,"audit-num.pgn","wb");fputs("[Date \"2003.01.05\"]\n[Time \"10:20:30\"]\n\n1. f5 d6 99999999999999999999. c3 1-99999999999999999999\n\n[Date \"99999.01.01\"]\n\n1. f5 *\n",f);fclose(f);
  OPEN(f,"audit-num.pgn","r");game_import_pgn(&g,f);game_import_pgn(&h,f);fclose(f);remove("audit-num.pgn");
  printf("GAMEIO PGN date %d.%d.%d %d:%d:%d, moves %d %d %d; date 99999.01.01 read as year %d (0 expected)\n",g.date.year,g.date.month,g.date.day,g.date.hour,g.date.minute,g.date.second,g.move[0],g.move[1],g.move[2],h.date.year);
  ok=ok&&g.date.year==2003&&g.date.month==1&&g.date.day==5&&g.date.hour==10&&g.date.minute==20&&g.date.second==30&&g.move[0]==F5&&g.move[1]==D6&&g.move[2]==C3&&h.date.year==0;
  {Line line;Board pos,e1,e2;game_init(&g);text_to_game("f5d6c3",&g);game_get_board(&g,3,&pos);line_init(&line,WHITE);line_push(&line,D3);line_push(&line,C4);line_to_game(&pos,&line,&h);strcpy(h.name[0],"x");
   game_init(&g);text_to_game("f5d6c3d3c4",&g);OPEN(f,"audit-pos.sgf","wb");game_export_sgf(&h,f);game_export_sgf(&g,f);fclose(f);base_init(&b);r=base_load(&b,"audit-pos.sgf");remove("audit-pos.sgf");
   const int same=r&&b.n_games==2&&board_equal(&b.game[0].initial_board,&pos)&&b.game[0].player==WHITE&&b.game[0].move[0]==D3&&b.game[0].move[1]==C4&&b.game[0].move[2]==NOMOVE
    &&game_get_board(b.game,2,&e1)&&game_get_board(&h,2,&e2)&&board_equal(&e1,&e2)&&board_equal(&b.game[1].initial_board,&g.initial_board)&&b.game[1].player==BLACK&&!memcmp(b.game[1].move,g.move,60);
   printf("GAMEIO SGF of a game from a position (white to move) and of a game from the start, saved and read: the same=%d\n",same);ok=ok&&same;base_free(&b);
   /* a game whose end is replaced (base correct): the hash of its new moves, the hash of the same game read from a text */
   game_init(&g);text_to_game("f5d6c3d3c4f4",&g);line_init(&line,WHITE);line_push(&line,B3);line_push(&line,C5);game_append_line(&g,&line,5);game_init(&h);text_to_game("f5d6c3d3c4b3c5",&h);
   base_init(&b);base_append(&b,&g);base_append(&b,&h);base_unique(&b);
   printf("GAMEIO hash of a game whose end is replaced %08llx, of the same game read %08llx, games after base unique %d (1 expected)\n",g.hash,h.hash,b.n_games);ok=ok&&g.hash==h.hash&&b.n_games==1;base_free(&b);}
  {static const struct{const char *s;int r;}c[]={{"(;GM[Othello]PB[a]PW[b]B[F5]W[D6];)",D6},{"(;GM[Othello]PB[a]B[F5]",-1},{"nothing",-1},{"(;GM[Othello]BO[8 x]B[F5];)",-1},{"(;GM[Othello]B[F5]W[Z9];)",-1},{"(;GM[Othello]B[F5]W[D6]XYZW[1];)",-1}};
   for(int i=0;i<(int)(sizeof c/sizeof *c);++i){r=parse_ggf(&g,c[i].s);printf("GAMEIO nboard game \"%s\": %d (%d expected)\n",c[i].s,r,c[i].r);ok=ok&&r==c[i].r;}}
  {static char longname[FILENAME_MAX*2+16];WthorBase w;memset(longname,'a',sizeof longname-1);longname[FILENAME_MAX+8]='/';strcpy(longname+FILENAME_MAX+9,"x.wtb");
   r=wthor_load(&w,longname);wthor_free(&w);const int saved=wthor_save(&w,longname);printf("GAMEIO wthor base in a folder of %d characters: loaded=%d saved=%d (0 expected)\n",FILENAME_MAX+8,r,saved);ok=ok&&!r&&!saved;}
  {WthorHeader hd;WthorGame wg,w0;WthorBase w;
#ifdef _WIN32
   CreateDirectoryA("audit-wthor",NULL);
#else
   mkdir("audit-wthor",0777);
#endif
   wthor_header_set(&hd,1,0,1999);hd.depth=22;OPEN(f,"audit-wthor/a.wtb","wb");wthor_header_write(&hd,f);game_init(&g);text_to_game("f5d6c3d3c4",&g);game_to_wthor(&g,&wg);wg.black=wg.white=0;fwrite(&wg,sizeof wg,1,f);fclose(f);
   base_init(&b);game_init(&g);text_to_game("f5f6",&g);base_append(&b,&g);r=base_save(&b,"audit-wthor/a.wtb");base_free(&b);
   OPEN(f,"audit-wthor/a.wtb","rb");wthor_header_read(&hd,f);n0=(long)fread(&w0,sizeof w0,1,f);fclose(f);
   printf("GAMEIO a game added to a wthor file of 1999, depth 22: saved=%d games=%d year %d depth %d, first game kept=%d\n",r,hd.n_games,hd.game_year,hd.depth,n0==1&&!memcmp(&w0,&wg,sizeof wg));
   ok=ok&&r&&hd.n_games==2&&hd.game_year==1999&&hd.depth==22&&n0==1&&!memcmp(&w0,&wg,sizeof wg);
   r=wthor_load(&w,"audit-wthor/a.wtb")&&wthor_save(&w,"audit-wthor/a.wtb");wthor_free(&w);OPEN(f,"audit-wthor/a.wtb","rb");wthor_header_read(&hd,f);fclose(f);
   printf("GAMEIO wthor file loaded and saved: %d, year %d depth %d\n",r,hd.game_year,hd.depth);ok=ok&&r&&hd.game_year==1999&&hd.depth==22;
   remove("audit-wthor/a.wtb");remove("audit-wthor/WTHOR.JOU");remove("audit-wthor/WTHOR.TRN");
#ifdef _WIN32
   RemoveDirectoryA("audit-wthor");
#else
   rmdir("audit-wthor");
#endif
  }
  {OBF obf;char *tmp;FILE *in,*out;OPEN(f,"audit-obf.obf","wb");fputs("--------------------------XO------------------------------------- X;\n---------------------------OX------XO--------------------------- X;\n",f);fclose(f);
   OPEN(in,"audit-obf.obf","r");memset(&obf,0,sizeof obf);r=obf_read(&obf,in);obf_free(&obf);const int r2=obf_read(&obf,in);obf_free(&obf);fclose(in);
   printf("GAMEIO OBF board of 2 discs: %d (skipped %d expected), of 4 discs: %d\n",r,OBF_PARSE_SKIP,r2);ok=ok&&r==OBF_PARSE_SKIP&&r2==OBF_PARSE_OK;
   /* an output is only replaced once the input is read to its end */
   OPEN(f,"audit-out.obf","wb");fputs("old\n",f);fclose(f);OPEN(in,"audit-obf.obf","r");out=obf_output_open("audit-out.obf",&tmp);if(!out)return 2;fputs("new\n",out);
   r=obf_output_close(out,"audit-out.obf",tmp,in);OPEN(f,"audit-out.obf","rb");n0=(long)fread(s,1,sizeof s-1,f);s[n0]=0;fclose(f);printf("GAMEIO OBF output of an input not read to its end: replaced=%d, \"%.3s\"\n",r,s);ok=ok&&!r&&!strcmp(s,"old\n");
   OPEN(in,"audit-obf.obf","r");while(fgets(s,sizeof s,in));out=obf_output_open("audit-out.obf",&tmp);if(!out)return 2;fputs("new\n",out);r=obf_output_close(out,"audit-out.obf",tmp,in);
   OPEN(f,"audit-out.obf","rb");n0=(long)fread(s,1,sizeof s-1,f);s[n0]=0;fclose(f);printf("GAMEIO OBF output of an input read to its end: replaced=%d, \"%.3s\"\n",r,s);ok=ok&&r&&!strncmp(s,"new",3); /* (written as text) */
   remove("audit-out.obf");remove("audit-obf.obf");}
  {const char *l1=opening_get_line("heath"),*l2=opening_get_line("heath #2"),*l3=opening_get_line("ganglion #2"),*l4=opening_get_line("ganglion #3"),*l5=opening_get_line("tigre");Board x;Move m;const char *e;int bad=0,same=0;
   printf("GAMEIO opening heath: %s, heath #2: %s, ganglion #2: %s, ganglion #3: %s\n",l1?l1:"-",l2?l2:"-",l3?l3:"-",l4?l4:"-");
   ok=ok&&l1&&!strcmp(l1,"f5f6e6f4g5")&&l2&&!strcmp(l2,"f5f6e6d6e7")&&l3&&!strcmp(l3,"f5d6c4g5")&&!l4&&l5&&!strcmp(l5,"f5d6c3d3c4");
   board_init(&x);e="f5f6e6d6e7f4";while(*e){const char *n=parse_move(e,&x,&m);if(n==e)break;board_update(&x,&m);e=n;}e=opening_get_english_name(&x);
   printf("GAMEIO english name of f5f6e6d6e7f4: %s\n",e?e:"-");ok=ok&&e&&!strcmp(e,"heath-chimney, \"mass-turning\"");
   for(const OpeningName *o=OPENING_NAME;o->name;++o){board_init(&x);e=o->line;while(*e){const char *n=parse_move(e,&x,&m);if(n==e||m.x==PASS)break;board_update(&x,&m);e=n;}if(*e)++bad;}
   for(const PositionName *p=POSITION_NAME;p->name;++p)for(const PositionName *q=POSITION_NAME;q<p;++q)same+=board_equal(&p->board,&q->board);
   for(const PositionName *p=NOM_POSITION;p->name;++p)for(const PositionName *q=NOM_POSITION;q<p;++q)same+=board_equal(&p->board,&q->board);
   printf("GAMEIO opening lines that are not legal: %d, boards of two names: %d (0 expected)\n",bad,same);ok=ok&&!bad&&!same;}
  if(!ok)return 2;}
 /* GTP: time_settings in ms without overflow, a time out of range keeps the clock; reg_genmove keeps the game (also a forced pass) */
 else if(!strcmp(t,"gtpclock")||!strcmp(t,"gtpregpass")){
  if(!have_fixture())return 2;options.book_file=(char*)FIXTURE;init_engine(1);options.level=1;UI *ui=ui_new();ui->type=UI_GTP;ui_init_gtp(ui);int ok=1;Play *p=ui->play;
  if(!strcmp(t,"gtpclock")){
   event_add_message(&ui->event,string_duplicate("time_settings 3000000 0 0"));event_add_message(&ui->event,string_duplicate("quit"));ui_loop_gtp(ui);long long t1=options.time;ok&=(t1==3000000000LL);
   event_add_message(&ui->event,string_duplicate("time_settings 3000000000 0 0"));event_add_message(&ui->event,string_duplicate("time_settings -5 0 0"));event_add_message(&ui->event,string_duplicate("quit"));ui_loop_gtp(ui);ok&=(options.time==3000000000LL);
   printf("GTP CLOCK 3000000 s=%lld ms, after out of range=%lld ms\n",t1,options.time);
  } else {
   char s[80];memset(s,'-',64);memcpy(s,"OXXX",4);strcpy(s+64," X");play_set_board(p,s); /* black to move, no black move, white has e1 */
   Board b0=p->board;int p0=p->player,i0=p->i_game,n0=p->n_game;long long l0=p->time[BLACK].left,l1=p->time[WHITE].left;
   event_add_message(&ui->event,string_duplicate("reg_genmove white"));event_add_message(&ui->event,string_duplicate("quit"));ui_loop_gtp(ui);
   int same=board_equal(&b0,&p->board)&&p0==p->player&&i0==p->i_game&&n0==p->n_game&&l0==p->time[BLACK].left&&l1==p->time[WHITE].left;ok&=same;
   event_add_message(&ui->event,string_duplicate("genmove white"));event_add_message(&ui->event,string_duplicate("quit"));ui_loop_gtp(ui);
   ok&=(p->i_game==i0+2);printf("GTP REG_GENMOVE game kept=%d, genmove then: %d moves (pass and e1)\n",same,p->i_game-i0);
  }
  ui_free_gtp(ui);ui_delete(ui);if(!ok)return 2;}
 /* GGS: a board update with a bad player on turn keeps the game; a good one updates it */
 else if(!strcmp(t,"ggsturn")){
  init_engine(1);options.can_ponder=false;UI *ui=ui_new();play_init(ui->play,&ui->book);play_init(ui->play+1,&ui->book);play_new(ui->play);play_new(ui->play+1);
  GGSClient *c=(GGSClient*)calloc(1,sizeof *c);if(!c)return 2;ui->ggs=c;c->me="edax";c->board->id=(char*)"1.2";c->board->player[0].name=(char*)"edax";c->board->player[1].name=(char*)"opp";
  c->board->move=37;memset(c->board->board,'-',64);memcpy(c->board->board+27,"O*",2);memcpy(c->board->board+35,"***",3);c->board->board[64]=0; /* after f5 */
  c->board->turn='?';ui_ggs_update(ui);int bad=(ui->play->player==BLACK&&ui->play->i_game==0);
  c->board->turn='O';ui_ggs_update(ui);Board e;char s2[80];sprintf(s2,"%s O",c->board->board);board_set(&e,s2);int good=(ui->play->player==WHITE&&ui->play->i_game==1&&board_equal(&e,&ui->play->board));
  printf("GGS TURN bad turn: game kept=%d; good turn: game updated=%d\n",bad,good);
  play_free(ui->play);play_free(ui->play+1);free(c);ui_delete(ui);if(!bad||!good)return 2;}
 else return 1;
 return 0;
}
