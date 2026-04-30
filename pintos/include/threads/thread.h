#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include <stdbool.h>
#include "threads/interrupt.h"
#include "threads/fixed-point.h"
#ifdef VM
#include "vm/vm.h"
#endif


/* States in a thread's life cycle. */
/* 스레드 생명 주기의 상태들. */
enum thread_status {
	THREAD_RUNNING,     /* Running thread. */
	/* 실행 중인 스레드. */
	THREAD_READY,       /* Not running but ready to run. */
	/* 실행 중은 아니지만 실행 준비가 된 스레드. */
	THREAD_BLOCKED,     /* Waiting for an event to trigger. */
	/* 이벤트 발생을 기다리는 스레드. */
	THREAD_DYING        /* About to be destroyed. */
	/* 곧 파괴될 스레드. */
};

/* Thread identifier type.
   You can redefine this to whatever type you like. */
/* 스레드 identifier 타입.
   원하는 타입으로 다시 정의할 수 있다. */
typedef int tid_t;
#define TID_ERROR ((tid_t) -1)          /* Error value for tid_t. */
/* tid_t의 에러 값. */

/* Thread priorities. */
/* 스레드 priority. */
#define PRI_MIN 0                       /* Lowest priority. */
/* 가장 낮은 priority. */
#define PRI_DEFAULT 31                  /* Default priority. */
/* 기본 priority. */
#define PRI_MAX 63                      /* Highest priority. */
/* 가장 높은 priority. */

/* A kernel thread or user process.
 *
 * Each thread structure is stored in its own 4 kB page.  The
 * thread structure itself sits at the very bottom of the page
 * (at offset 0).  The rest of the page is reserved for the
 * thread's kernel stack, which grows downward from the top of
 * the page (at offset 4 kB).  Here's an illustration:
 *
 *      4 kB +---------------------------------+
 *           |          kernel stack           |
 *           |                |                |
 *           |                |                |
 *           |                V                |
 *           |         grows downward          |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           +---------------------------------+
 *           |              magic              |
 *           |            intr_frame           |
 *           |                :                |
 *           |                :                |
 *           |               name              |
 *           |              status             |
 *      0 kB +---------------------------------+
 *
 * The upshot of this is twofold:
 *
 *    1. First, `struct thread' must not be allowed to grow too
 *       big.  If it does, then there will not be enough room for
 *       the kernel stack.  Our base `struct thread' is only a
 *       few bytes in size.  It probably should stay well under 1
 *       kB.
 *
 *    2. Second, kernel stacks must not be allowed to grow too
 *       large.  If a stack overflows, it will corrupt the thread
 *       state.  Thus, kernel functions should not allocate large
 *       structures or arrays as non-static local variables.  Use
 *       dynamic allocation with malloc() or palloc_get_page()
 *       instead.
 *
 * The first symptom of either of these problems will probably be
 * an assertion failure in thread_current(), which checks that
 * the `magic' member of the running thread's `struct thread' is
 * set to THREAD_MAGIC.  Stack overflow will normally change this
 * value, triggering the assertion. */
/* The `elem' member has a dual purpose.  It can be an element in
 * the run queue (thread.c), or it can be an element in a
 * semaphore wait list (synch.c).  It can be used these two ways
 * only because they are mutually exclusive: only a thread in the
 * ready state is on the run queue, whereas only a thread in the
 * blocked state is on a semaphore wait list. */
struct thread {
	/* Owned by thread.c. */
	/* thread.c가 소유한다. */
	tid_t tid;                          /* Thread identifier. */
	/* 스레드 identifier. */
	enum thread_status status;          /* Thread state. */
	/* 스레드 상태. */
	char name[16];                      /* Name (for debugging purposes). */
	/* 이름(디버깅 용도). */
	int priority;                       /* Priority. */
	/* priority 값. */

	// wakeup_tick은 sleep 상황에서만 의미 가짐.
	int64_t wakeup_ticks;                /* thread가 wakeup 해야 할 tick. */

	/* Shared between thread.c and synch.c. */
	/* thread.c와 synch.c가 공유한다. */
	struct list_elem elem;              /* List element. */
	/* 리스트 element. */

	/* priority는 Donation으로 변하기 때문에, 순수하게 해당 스레드의 priority를 저장하는 역할 */
	int base_priority;

	/* 특정 락에 대기하고 있는 경우, 그 락을 바라본다. 그 외에는 NULL. */
	struct lock *wait_on_lock;

	/* d_elem 요소를 가짐.
	   Donors(후원자)의 목록, Multiple Donation 표현. 정렬 순서를 보장하지 않음 */
	struct list donations;

	/* donations로 관리되는 리스트, elem과는 별개로 관리되어야 하므로 필요함 */
	struct list_elem d_elem;

	/* 4.4BSD Scheduler를 위한 필드 */
	int nice;
	fp32_t recent_cpu;
	struct list_elem q_elem; // 스케쥴링 큐

#ifdef USERPROG
	/* Owned by userprog/process.c. */
	/* userprog/process.c가 소유한다. */
	uint64_t *pml4;                     /* Page map level 4 */
	/* Page map level 4이다. */
#endif
#ifdef VM
	/* Table for whole virtual memory owned by thread. */
	/* 스레드가 소유한 전체 가상 메모리용 테이블. */
	struct supplemental_page_table spt;
#endif

	/* Owned by thread.c. */
	/* thread.c가 소유한다. */
	struct intr_frame tf;               /* Information for switching */
	/* switching에 필요한 정보. */
	unsigned magic;                     /* Detects stack overflow. */
	/* 스택 오버플로우를 감지한다. */
};

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
/* false(기본값)이면 round-robin 스케줄러를 사용한다.
   true이면 multi-level feedback queue 스케줄러를 사용한다.
   커널 커맨드라인 옵션 "-o mlfqs"로 제어한다. */
extern bool thread_mlfqs;

void thread_init (void);
void thread_start (void);

void thread_tick (void);
void thread_print_stats (void);

typedef void thread_func (void *aux);
tid_t thread_create (const char *name, int priority, thread_func *, void *);

void thread_block (void);
void thread_unblock (struct thread *);

struct thread *thread_current (void);
tid_t thread_tid (void);
const char *thread_name (void);

void thread_exit (void) NO_RETURN;
void thread_yield (void);
void thread_yield_if_needed (void);
void thread_sleep (int64_t wakeup_tick);
void threads_wakeup (int64_t ticks);

int thread_get_priority (void);
void thread_set_priority (int);

int thread_get_nice (void);
void thread_set_nice (int);
int thread_get_recent_cpu (void);
int thread_get_load_avg (void);

void do_iret (struct intr_frame *tf);

bool cmp_priority_more (const struct list_elem *a,
		const struct list_elem *b, void *aux UNUSED);

// Priority Donation
bool cmp_donors_priority_more (const struct list_elem *a,
		const struct list_elem *b, void *aux UNUSED);
void thread_donors_recalc_priorities (void);

// mlfqs
void thread_mlfqs_recalc_priorities (void);
void thread_mlfqs_incr_recent_cpu (void);
void thread_mlfqs_recalc_shcd_queue (void);

#endif /* threads/thread.h */
