#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include <stdbool.h>
#include "threads/interrupt.h"
#ifdef VM
#include "vm/vm.h"
#endif


/* 스레드 수명 주기의 상태입니다. */
enum thread_status {
	THREAD_RUNNING,     /* 현재 실행 중인 스레드. */
	THREAD_READY,       /* 실행 중이 아니지만 실행할 준비가 되었습니다. */
	THREAD_BLOCKED,     /* 어떤 이벤트를 기다리는 중입니다. */
	THREAD_DYING        /* 곧 제거될 예정입니다. */
};

/* 스레드 식별자 타입.
   원하는 타입으로 다시 정의할 수 있습니다. */
typedef int tid_t;
#define TID_ERROR ((tid_t) -1)          /* tid_t의 오류 값. */

/* 스레드 우선순위. */
#define PRI_MIN 0                       /* 가장 낮은 우선순위. */
#define PRI_DEFAULT 31                  /* 기본 우선순위. */
#define PRI_MAX 63                      /* 가장 높은 우선순위. */

/* 커널 스레드 또는 사용자 프로세스.
 *
 * 각 스레드 구조는 자체 4kB 페이지에 저장됩니다. 그만큼
 * 스레드 구조 자체는 페이지 맨 아래에 위치합니다.
 * (오프셋 0에서). 페이지의 나머지 부분은 다음을 위해 예약되어 있습니다.
 * 스레드의 커널 스택은 스레드의 맨 위에서 아래로 성장합니다.
 * 페이지(오프셋 4kB). 다음은 예시입니다.
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
 * 이것의 결과는 두 가지입니다:
 *
 *    1. 첫째, `struct thread'가 너무 커지는 것을 허용해서는 안 됩니다.
 *       큰. 그렇게 되면 공간이 부족해
 *       커널 스택. 우리의 기본 `구조 스레드'는 단지
 *       크기는 몇 바이트입니다. 아마도 1 미만으로 유지되어야 할 것입니다.
 *       kB.
 *
 *    2. 둘째, 커널 스택이 너무 커지면 안 됩니다.
 *       크기가 큰. 스택이 오버플로되면 스레드가 손상됩니다.
 *       상태. 따라서 커널 함수는 큰 할당을 해서는 안 됩니다.
 *       비정적 지역 변수로 구조체나 배열을 사용합니다. 사용
 *       malloc() 또는 palloc_get_page()을 사용한 동적 할당
 *       대신에.
 *
 * 이러한 문제 중 하나의 첫 번째 증상은 아마도 다음과 같습니다.
 * thread_current() 의 어설션 실패, 이를 확인합니다.
 * 실행 중인 스레드의 `struct thread' 안 `magic' 멤버가
 * THREAD_MAGIC 으로 설정합니다. 스택 오버플로는 일반적으로 이를 변경합니다.
 * 값, 어설션을 트리거합니다. */
/* `elem' 멤버는 두 가지 목적을 가지고 있습니다. 의 요소가 될 수 있습니다.
 * 실행 큐(thread.c) 또는
 * 세마포어 대기 목록(synch.c). 이 두 가지 방법으로 사용할 수 있습니다
 * 단지 그것들은 상호 배타적이기 때문입니다.
 * 준비 상태는 실행 대기열에 있는 반면,
 * 차단된 상태는 세마포어 대기 목록에 있습니다. */
struct thread {
	/* thread.c가 소유합니다. */
	tid_t tid;                          /* 스레드 식별자. */
	enum thread_status status;          /* 스레드 상태. */
	char name[16];                      /* 이름(디버깅용). */
	int priority;                       /* 우선순위. */
	int64_t wakeup_ticks;               /* 깨워야 할 타이머 tick. */

	/* thread.c와 synch.c 간에 공유됩니다. */
	struct list_elem elem;              /* 목록 요소. */

	/* priority는 Donation으로 변하기 때문에, 순수하게 해당 스레드의 priority를 저장하는 역할 */
	int base_priority;

	/* 특정 락에 대기하고 있는 경우, 그 락을 바라본다. 그 외에는 NULL. */
	struct lock *wait_on_lock;

	/* d_elem 요소를 가짐.
	   Donors(후원자)의 목록, Multiple Donation 표현. 정렬 순서를 보장하지 않음 */
	struct list donations;

	/* donations로 관리되는 리스트, elem과는 별개로 관리되어야 하므로 필요함 */
	struct list_elem d_elem;

#ifdef USERPROG
	/* userprog/process.c가 소유합니다. */
	uint64_t *pml4;                     /* 페이지 맵 레벨 4 */
#endif
#ifdef VM
	/* 스레드가 소유한 전체 가상 메모리에 대한 테이블입니다. */
	struct supplemental_page_table spt;
#endif

	/* thread.c가 소유합니다. */
	struct intr_frame tf;               /* 전환 정보. */
	unsigned magic;                     /* 스택 오버플로를 감지합니다. */
};

/* false(기본값)이면 라운드 로빈 스케줄러를 사용합니다.
   true이면 다중 레벨 피드백 큐 스케줄러를 사용합니다.
   커널 명령줄 옵션 "-o mlfqs"로 제어됩니다. */
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
bool cmp_donors_priority_more (const struct list_elem *a,
		const struct list_elem *b, void *aux UNUSED);
void refresh_priority_in_donors (void);

#endif /* threads/thread.h */
