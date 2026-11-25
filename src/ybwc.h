/**
 * @file ybwc.h
 *
 * @brief Parallel search header.
 *
 * @date 1998 - 2017
 * @author Richard Delorme
 * @version 4.4
 *
 */

#ifndef EDAX_YBWC_H
#define EDAX_YBWC_H

#include "util.h"
#include "const.h"
#include "settings.h"

#include <stdbool.h>

struct Search;
struct Move;
struct MoveList;
struct Task;

/*
 * Shared fields of the nodes and of the tasks read without their lock: a test made again under the lock,
 * the alpha bound of a node read by its slaves, or a flag that a thread waits for. They are written under the
 * lock. ybwc_load()/ybwc_store() are relaxed atomics: the plain loads and stores of the processor, with no
 * fence. The end of a task (task->run) is written with a release and waited for with an acquire, so that
 * what the task did before is seen after the wait: plain loads and stores on x86, ldar/stlr on arm64.
 */
#if defined(__GNUC__) || defined(__clang__)
	#define	ybwc_load(p)	__atomic_load_n((p), __ATOMIC_RELAXED)
	#define	ybwc_store(p, v)	__atomic_store_n((p), (v), __ATOMIC_RELAXED)
	#define	ybwc_load_acquire(p)	__atomic_load_n((p), __ATOMIC_ACQUIRE)
	#define	ybwc_store_release(p, v)	__atomic_store_n((p), (v), __ATOMIC_RELEASE)
#elif defined(_M_IX86) || defined(_M_X64)
	// (MSVC: the fields are volatile; on x86 a volatile load is an acquire and a volatile store a release)
	#define	ybwc_load(p)	(*(p))
	#define	ybwc_store(p, v)	(*(p) = (v))
	#define	ybwc_load_acquire(p)	(*(p))
	#define	ybwc_store_release(p, v)	(*(p) = (v))
#else
	#define	ybwc_load(p)	(*(p))
	#define	ybwc_store(p, v)	(*(p) = (v))
	static inline bool ybwc_load_acquire_bool(const volatile bool *p) { const bool v = *p; MemoryBarrier(); return v; }
	#define	ybwc_load_acquire(p)	ybwc_load_acquire_bool(p)
	#define	ybwc_store_release(p, v)	do { MemoryBarrier(); *(p) = (v); } while (0)
#endif

/**
 * A Task is a parallel search thread.
 */
typedef struct Task {
	volatile bool loop;          /**< loop flag */
	volatile bool run;           /**< run flag */
	volatile bool is_helping;    /**< is helping */
	struct Search *search;       /**< search data */
	struct Node *node;           /**< node splitted */
	struct Move *move;           /**< move to search */
	volatile int lazy_depth;     /**< lazy helper (search-b): depth searched at the root (0: a split) */
	volatile int lazy_sel;       /**< lazy helper: first selectivity of a helper of the iterative selectivity (-1: deepening) */
	Thread thread;               /**< thread */
	unsigned long long n_calls;  /**< call counter */
	unsigned long long n_nodes;  /**< nodes counter */
	Lock lock;                   /**< lock */
	Condition cond;              /**< condition */
	struct TaskStack *container; /**< link to its container */
} Task;

/**
 * A Node is a position in the search tree, containing information shared with
 * parallel threads.
 */
typedef struct Node {
	volatile int bestmove;       /**< bestmove */
	volatile int bestscore;      /**< bestscore */
	volatile int alpha;          /**< alpha lower bound */
	int beta;                    /**< beta upper bound (is constant after initialisation) */
	bool pv_node;                /**< pv_node */
	volatile int n_slave;	     /**< number of slaves splitted flag */
	volatile bool stop_point;    /**< stop point flag */
	volatile bool is_waiting;	 /**< waiting flag */
	int depth;                   /**< depth */
	int height;                  /**< height */
	struct Search *search;       /**< master search structure */
	struct Search *slave[SPLIT_MAX_SLAVES]; /**< slave search structure */
	struct Node *parent;         /**< master node */
	struct Move *move;           /**< move to search */
	volatile int n_moves_done;   /**< search done */
	volatile int n_moves_todo;   /**< search todo */
	volatile bool is_helping;	 /**< waiting flag */
	Task help;                   /**< helper task */
	Lock lock;                   /**< mutex */
	Condition cond;              /**< condition variable */
	struct Move *first;          /**< first move of the list (search-b: node_move_rank) */
} Node;

/* node function declaration */
void node_init(Node*, struct Search*, const int, const int, const int, const int, Node*);
void node_free(Node*);
bool node_split(Node*, struct Move*);
void node_stop_slaves(Node*);
void node_wait_slaves(Node*);
void node_update(Node*, struct Move*);
int node_move_rank(const Node*, const struct Move*);
struct Move * node_first_move(Node*, struct MoveList*);
struct Move * node_next_move(Node*);

/* task function declaration */
void* task_loop(void*);
void* task_help(void*);
void task_init(Task*);
void task_free(Task*);
void task_update(Task*);
void task_search(Task *task);

/** @struct TaskStack
 *
 * A FILO of tasks
 */
typedef struct TaskStack {
	SpinLock spin;               /**< mutex */
	Task *task;                  /**< set of tasks */
	Task **stack;                /**< stack of tasks */
	int n;                       /**< maximal number of idle tasks */
	int n_idle;                  /**< number of idle tasks */
} TaskStack;

/* task stack function declaration */
void task_stack_init(TaskStack*, const int);
void task_stack_free(TaskStack*);
void task_stack_resize(TaskStack*, const int);
void task_stack_stop(TaskStack*, const Stop);
Task* task_stack_get_idle_task(TaskStack*);
void task_stack_put_idle_task(TaskStack*, Task*);
void task_stack_clear(TaskStack*);
unsigned long long task_stack_count_nodes(TaskStack*);

#endif

