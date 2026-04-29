#include "threads/thread.h"
#include <debug.h>
#include <stddef.h>
#include <random.h>
#include <stdio.h>
#include <string.h>
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/intr-stubs.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "intrinsic.h"
#ifdef USERPROG
#include "userprog/process.h"
#endif

/* struct thread의 `magic' 멤버에 넣는 임의의 값입니다.
   스택 오버플로를 감지하는 데 사용됩니다. 상단의 큰댓글을 참고하세요
   자세한 내용은 thread.h를 참조하세요. */
#define THREAD_MAGIC 0xcd6abf4b

/* 기본 스레드의 임의 값
   이 값을 수정하지 마십시오. */
#define THREAD_BASIC 0xd42df210

/* THREAD_READY 상태인 프로세스, 즉 프로세스 목록
   실행할 준비가 되었지만 실제로 실행되지는 않습니다. */
static struct list ready_list;

/* 특정 타이머 tick까지 잠든 스레드 목록. */
static struct list sleep_list;

/* 유휴 스레드. */
static struct thread *idle_thread;

/* 초기 스레드, init.c를 실행하는 스레드: main(). */
static struct thread *initial_thread;

/* allocate_tid()에서 사용되는 잠금입니다. */
static struct lock tid_lock;

/* 스레드 소멸 요청 */
static struct list destruction_req;

/* 통계. */
static long long idle_ticks;    /* idle에서 보낸 타이머 틱 수. */
static long long kernel_ticks;  /* 커널 스레드에서 보낸 타이머 틱 수. */
static long long user_ticks;    /* 사용자 프로그램에서 보낸 타이머 틱 수. */

/* 스케줄링. */
#define TIME_SLICE 4            /* 각 스레드에 부여할 타이머 틱 수. */
static unsigned thread_ticks;   /* 마지막 양보 이후의 타이머 틱 수. */

/* false(기본값)인 경우 라운드 로빈 스케줄러를 사용합니다.
   true인 경우 다중 레벨 피드백 큐 스케줄러를 사용하십시오.
   커널 명령줄 옵션 "-o mlfqs"로 제어됩니다. */
bool thread_mlfqs;

static void kernel_thread (thread_func *, void *aux);

static void idle (void *aux UNUSED);
static struct thread *next_thread_to_run (void);
static void init_thread (struct thread *, const char *name, int priority);
static void do_schedule (int status);
static void schedule (void);
static tid_t allocate_tid (void);
static bool cmp_wakeup_ticks_less (const struct list_elem *a,
		const struct list_elem *b, void *aux UNUSED);

/* T가 유효한 스레드를 가리키는 것으로 나타나면 true를 반환합니다. */
#define is_thread(t) ((t) != NULL && (t)->magic == THREAD_MAGIC)

/* 실행 중인 스레드를 반환합니다.
 * CPU의 스택 포인터 `rsp'를 읽고 반올림합니다.
 * 페이지 시작 부분까지. `struct thread'는
 * 항상 페이지의 시작 부분에 있고 스택 포인터는
 * 중간 어딘가에 현재 스레드가 위치합니다. */
#define running_thread() ((struct thread *) (pg_round_down (rrsp ())))


// thread_start에 대한 전역 설명자 테이블입니다.
// gdt는 thread_init 이후에 설정되므로
// 먼저 임시 GDT를 설정하세요.
static uint64_t gdt[3] = { 0, 0x00af9a000000ffff, 0x00cf92000000ffff };

/* 코드를 변환하여 스레딩 시스템을 초기화합니다.
   현재 스레드로 실행 중입니다. 이것은 작동하지 않습니다
   일반적이며 이 경우에만 가능합니다. 왜냐하면 loader.S
   스택의 맨 아래를 페이지 경계에 두도록 주의했습니다.

   또한 실행 큐와 tid 잠금을 초기화합니다.

   이 함수를 호출한 후에는 반드시 페이지를 초기화하세요.
   스레드를 생성하기 전에 할당자
   thread_create().

   이 함수가 나올 때까지 thread_current()을 호출하는 것은 안전하지 않습니다.
   끝납니다. */
void
thread_init (void) {
	ASSERT (intr_get_level () == INTR_OFF);

	/* 커널의 임시 gdt를 다시 로드합니다.
	 * 이 gdt에는 사용자 컨텍스트가 포함되지 않습니다.
	 * 커널은 gdt_init ()에서 사용자 컨텍스트로 gdt를 다시 빌드합니다. */
	struct desc_ptr gdt_ds = {
		.size = sizeof (gdt) - 1,
		.address = (uint64_t) gdt
	};
	lgdt (&gdt_ds);

/* 전역 스레드 컨텍스트를 초기화한다. */
	lock_init (&tid_lock);
	list_init (&ready_list);
	list_init (&sleep_list);
	list_init (&destruction_req);

	/* 실행 중인 스레드에 대한 스레드 구조를 설정합니다. */
	initial_thread = running_thread ();
	init_thread (initial_thread, "main", PRI_DEFAULT);
	initial_thread->status = THREAD_RUNNING;
	initial_thread->tid = allocate_tid ();
}

/* 인터럽트를 활성화하여 선점형 스레드 스케줄링을 시작합니다.
   또한 유휴 스레드를 생성합니다. */
void
thread_start (void) {
	/* 유휴 스레드를 만듭니다. */
	struct semaphore idle_started;
	sema_init (&idle_started, 0);
	thread_create ("idle", PRI_MIN, idle, &idle_started);

	/* 선점형 스레드 스케줄링을 시작합니다. */
	intr_enable ();

	/* 유휴 스레드가 유휴 스레드를 초기화할 때까지 기다립니다. */
	sema_down (&idle_started);
}

/* 각 타이머 틱마다 타이머 인터럽트 핸들러에 의해 호출됩니다.
   따라서 이 함수는 외부 인터럽트 컨텍스트에서 실행됩니다. */
void
thread_tick (void) {
	struct thread *t = thread_current ();

	/* 통계를 업데이트하세요. */
	if (t == idle_thread)
		idle_ticks++;
#ifdef USERPROG
	else if (t->pml4 != NULL)
		user_ticks++;
#endif
	else
		kernel_ticks++;

	/* 선점을 시행합니다. */
	if (++thread_ticks >= TIME_SLICE)
		intr_yield_on_return ();
}

/* 스레드 통계를 인쇄합니다. */
void
thread_print_stats (void) {
	printf ("Thread: %lld idle ticks, %lld kernel ticks, %lld user ticks\n",
			idle_ticks, kernel_ticks, user_ticks);
}

/* 주어진 초기값을 사용하여 NAME이라는 새 커널 스레드를 생성합니다.
   AUX를 인수로 전달하여 FUNCTION을 실행하는 PRIORITY,
   준비 대기열에 추가합니다. 스레드 식별자를 반환합니다.
   새 스레드의 경우, 생성이 실패하면 TID_ERROR입니다.

   thread_start()이 호출된 경우 새 스레드는 다음과 같을 수 있습니다.
   thread_create()이 반환되기 전에 예약되었습니다. 빠져나갈 수도 있다
   thread_create()이 반환되기 전에. 그에 비해 원작은
   스레드는 새 스레드가 생성되기 전까지 얼마 동안 실행될 수 있습니다.
   예정. 세마포어 또는 다른 형태의 사용
   주문을 보장해야 하는 경우 동기화.

   제공된 코드는 새 스레드의 '우선순위' 멤버를 다음으로 설정합니다.
   PRIORITY 이지만 실제 우선순위 스케줄링은 구현되지 않습니다.
   우선순위 스케줄링은 문제 1-3의 목표입니다. */
tid_t
thread_create (const char *name, int priority,
		thread_func *function, void *aux) {
	struct thread *t;
	tid_t tid;

	ASSERT (function != NULL);

	/* 스레드를 할당합니다. */
	t = palloc_get_page (PAL_ZERO);
	if (t == NULL)
		return TID_ERROR;

	/* 스레드를 초기화합니다. */
	init_thread (t, name, priority);
	tid = t->tid = allocate_tid ();

	/* 예정된 경우 kernel_thread를 호출합니다.
	 * 참고) rdi는 첫 번째 인수이고, rsi는 두 번째 인수입니다. */
	t->tf.rip = (uintptr_t) kernel_thread;
	t->tf.R.rdi = (uint64_t) function;
	t->tf.R.rsi = (uint64_t) aux;
	t->tf.ds = SEL_KDSEG;
	t->tf.es = SEL_KDSEG;
	t->tf.ss = SEL_KDSEG;
	t->tf.cs = SEL_KCSEG;
	t->tf.eflags = FLAG_IF;

	/* 실행 대기열에 추가합니다. */
	thread_unblock (t);
	thread_yield_if_needed ();

	return tid;
}

/* 현재 스레드를 절전 모드로 전환합니다. 예정되어 있지 않습니다
   thread_unblock()에 의해 깨어날 때까지 다시.

   이 함수는 인터럽트가 꺼진 상태에서 호출되어야 합니다. 그것
   일반적으로 동기화 중 하나를 사용하는 것이 더 좋습니다.
   sync.h의 프리미티브 */
void
thread_block (void) {
	ASSERT (!intr_context ());
	ASSERT (intr_get_level () == INTR_OFF);
	thread_current ()->status = THREAD_BLOCKED;
	schedule ();
}

/* 차단된 스레드 T를 실행 준비 상태로 전환합니다.
   T가 차단되지 않은 경우 이는 오류입니다. (thread_yield()을(를) 사용하여
   실행 중인 스레드를 준비합니다.)

   이 함수는 실행 중인 스레드를 선점하지 않습니다. 이것은 할 수 있다
   중요: 호출자가 인터럽트 자체를 비활성화한 경우
   스레드를 원자적으로 차단 해제할 수 있다고 기대할 수 있으며
   다른 데이터를 업데이트하세요. */
void
thread_unblock (struct thread *t) {
	enum intr_level old_level;

	ASSERT (is_thread (t));

	old_level = intr_disable ();
	ASSERT (t->status == THREAD_BLOCKED);
	list_insert_ordered (&ready_list, &t->elem, cmp_priority_more, NULL);
	t->status = THREAD_READY;
	intr_set_level (old_level);
}

/* 실행 중인 스레드의 이름을 반환합니다. */
const char *
thread_name (void) {
	return thread_current ()->name;
}

/* 실행 중인 스레드를 반환합니다.
   이것은 running_thread()에 몇 가지 온전성 검사를 더한 것입니다.
   자세한 내용은 thread.h 상단의 큰 주석을 참조하세요. */
struct thread *
thread_current (void) {
	struct thread *t = running_thread ();

	/* T가 실제로 스레드인지 확인하세요.
	   이러한 어설션 중 하나가 실행되면 스레드가 발생할 수 있습니다.
	   스택이 오버플로되었습니다. 각 스레드의 크기는 4kB 미만입니다.
	   스택이 많으므로 몇 개의 큰 자동 배열 또는 중간 정도
	   재귀로 인해 스택 오버플로가 발생할 수 있습니다. */
	ASSERT (is_thread (t));
	ASSERT (t->status == THREAD_RUNNING);

	return t;
}

/* 실행 중인 스레드의 tid를 반환합니다. */
tid_t
thread_tid (void) {
	return thread_current ()->tid;
}

/* 현재 스레드의 일정을 취소하고 삭제합니다. 절대
   호출자에게 반환됩니다. */
void
thread_exit (void) {
	ASSERT (!intr_context ());

#ifdef USERPROG
	process_exit ();
#endif

	/* 상태를 죽어가는 것으로 설정하고 다른 프로세스를 예약하면 됩니다.
	   schedule_tail()을 호출하는 동안 우리는 파괴될 것입니다. */
	intr_disable ();
	do_schedule (THREAD_DYING);
	NOT_REACHED ();
}

/* CPU을 생성합니다. 현재 스레드는 휴면 상태가 아니며
   스케줄러의 변덕에 따라 즉시 다시 예약될 수 있습니다. */
void
thread_yield (void) {
	struct thread *curr = thread_current ();
	enum intr_level old_level;

	ASSERT (!intr_context ());

	old_level = intr_disable ();
	if (curr != idle_thread)
		list_insert_ordered (&ready_list, &curr->elem, cmp_priority_more, NULL);
	do_schedule (THREAD_READY);
	intr_set_level (old_level);
}

/* 만약 ready_list 중 현재 스레드보다 우선순위가 높은게 있으면 preemption(yield 호출) 한다. */
void
thread_yield_if_needed (void) {
	if (list_empty (&ready_list))
		return;

	list_sort (&ready_list, cmp_priority_more, NULL); // Priority Donation 때문
	struct thread *peek_t =
		list_entry (list_front (&ready_list), struct thread, elem);
	bool need_preemption = peek_t->priority > thread_current ()->priority;

	if (need_preemption) {
		if (intr_context ())
			intr_yield_on_return ();
		else
			thread_yield ();
	}
}

/* WAKEUP_TICK까지 현재 스레드를 재우고 스케줄 대상에서 제외한다. */
void
thread_sleep (int64_t wakeup_tick) {
	struct thread *curr = thread_current ();
	enum intr_level old_level;

	ASSERT (!intr_context ());

	old_level = intr_disable ();
	if (curr != idle_thread) {
		curr->wakeup_ticks = wakeup_tick;
		list_insert_ordered (&sleep_list, &curr->elem, cmp_wakeup_ticks_less, NULL);
		thread_block ();
	}
	intr_set_level (old_level);
}

/* 깨어날 tick에 도달한 모든 sleeping thread를 ready 상태로 옮긴다. */
void
threads_wakeup (int64_t ticks) {
	struct list_elem *e;

	ASSERT (intr_context ());
	ASSERT (intr_get_level () == INTR_OFF);

	e = list_begin (&sleep_list);
	while (e != list_end (&sleep_list)) {
		struct thread *t = list_entry (e, struct thread, elem);

		if (t->wakeup_ticks <= ticks) {
			e = list_remove (e);
			thread_unblock (t);
		} else {
			break;
		}
	}
	thread_yield_if_needed ();
}

/* 현재 스레드의 우선순위를 NEW_PRIORITY 으로 설정합니다. */
void
thread_set_priority (int new_priority) {
	thread_current ()->base_priority = new_priority;
	refresh_priority_in_donors ();
	thread_yield_if_needed ();
}

/* 현재 스레드의 우선순위를 반환합니다. */
int
thread_get_priority (void) {
	return thread_current ()->priority;
}

/* 현재 스레드의 nice 값을 NICE 으로 설정합니다. */
void
thread_set_nice (int nice UNUSED) {
	/* TODO: 구현 내용은 여기에 있습니다. */
}

/* 현재 스레드의 nice 값을 반환합니다. */
int
thread_get_nice (void) {
	/* TODO: 구현 내용은 여기에 있습니다. */
	return 0;
}

/* 시스템 로드 평균의 100배를 반환합니다. */
int
thread_get_load_avg (void) {
	/* TODO: 구현 내용은 여기에 있습니다. */
	return 0;
}

/* 현재 스레드의 Recent_cpu 값의 100배를 반환합니다. */
int
thread_get_recent_cpu (void) {
	/* TODO: 구현 내용은 여기에 있습니다. */
	return 0;
}

/* 유휴 스레드. 실행할 준비가 된 다른 스레드가 없을 때 실행됩니다.

   유휴 스레드는 처음에 다음과 같이 준비 목록에 추가됩니다.
   thread_start(). 처음에는 한 번만 예약되며,
   유휴_스레드를 초기화하는 지점에서 전달된 세마포어가 "업"됩니다.
   계속하려면 thread_start()을 활성화하고 즉시
   블록. 그 이후에는 유휴 스레드가 더 이상 나타나지 않습니다.
   준비 목록. next_thread_to_run()에 의해 다음과 같이 반환됩니다.
   준비 목록이 비어 있는 특별한 경우입니다. */
static void
idle (void *idle_started_ UNUSED) {
	struct semaphore *idle_started = idle_started_;

	idle_thread = thread_current ();
	sema_up (idle_started);

	for (;;) {
		/* 다른 사람이 뛰게 해주세요. */
		intr_disable ();
		thread_block ();

		/* 인터럽트를 다시 활성화하고 다음 인터럽트를 기다립니다.

		   `sti' 명령어는 다음이 나올 때까지 인터럽트를 비활성화합니다.
		   다음 명령이 완료되었으므로 이 두 명령은
		   명령어는 원자적으로 실행됩니다. 이 원자성은
		   important; otherwise, an interrupt could be handled
		   인터럽트를 다시 활성화하고 다음을 기다리는 사이
		   하나가 발생하면 1시계 틱만큼 낭비됩니다.
		   시간.

		   [IA32-v2a] " HLT ", [IA32-v2b] " STI " 및 [IA32-v3a]를 참조하세요.
		   7.11.1 " HLT 명령어". */
		asm volatile ("sti; hlt" : : : "memory");
	}
}

/* 커널 스레드의 기초로 사용되는 함수입니다. */
static void
kernel_thread (thread_func *function, void *aux) {
	ASSERT (function != NULL);

	intr_enable ();       /* 스케줄러는 인터럽트가 꺼진 상태로 실행됩니다. */
	function (aux);       /* 스레드 함수를 실행합니다. */
	thread_exit ();       /* function()이 반환되면 스레드를 종료합니다. */
}


/* T의 기본 초기화를 차단된 스레드로 수행합니다.
   NAME. */
static void
init_thread (struct thread *t, const char *name, int priority) {
	ASSERT (t != NULL);
	ASSERT (PRI_MIN <= priority && priority <= PRI_MAX);
	ASSERT (name != NULL);

	memset (t, 0, sizeof *t);
	t->status = THREAD_BLOCKED;
	strlcpy (t->name, name, sizeof t->name);
	t->tf.rsp = (uint64_t) t + PGSIZE - sizeof (void *);
	t->priority = priority;
	t->base_priority = priority;
	t->magic = THREAD_MAGIC;
	t->wait_on_lock = NULL;
	list_init (&t->donations);
}

/* 예약할 다음 스레드를 선택하고 반환합니다. 해야 한다
   실행 큐가 그렇지 않은 경우 실행 큐에서 스레드를 반환합니다.
   비어 있는. (실행 중인 스레드가 계속 실행될 수 있으면
   실행 큐에 있습니다.) 실행 큐가 비어 있으면 반환
   idle_thread. */
static struct thread *
next_thread_to_run (void) {
	if (list_empty (&ready_list))
		return idle_thread;
	list_sort (&ready_list, cmp_priority_more, NULL); // Priority Donation 때문에 정렬 필요
	return list_entry (list_pop_front (&ready_list), struct thread, elem);
}

/* iretq를 사용하여 스레드 시작 */
void
do_iret (struct intr_frame *tf) {
	__asm __volatile(
			"movq %0, %%rsp\n"
			"movq 0(%%rsp),%%r15\n"
			"movq 8(%%rsp),%%r14\n"
			"movq 16(%%rsp),%%r13\n"
			"movq 24(%%rsp),%%r12\n"
			"movq 32(%%rsp),%%r11\n"
			"movq 40(%%rsp),%%r10\n"
			"movq 48(%%rsp),%%r9\n"
			"movq 56(%%rsp),%%r8\n"
			"movq 64(%%rsp),%%rsi\n"
			"movq 72(%%rsp),%%rdi\n"
			"movq 80(%%rsp),%%rbp\n"
			"movq 88(%%rsp),%%rdx\n"
			"movq 96(%%rsp),%%rcx\n"
			"movq 104(%%rsp),%%rbx\n"
			"movq 112(%%rsp),%%rax\n"
			"addq $120,%%rsp\n"
			"movw 8(%%rsp),%%ds\n"
			"movw (%%rsp),%%es\n"
			"addq $32, %%rsp\n"
			"iretq"
			: : "g" ((uint64_t) tf) : "memory");
}

/* 새 스레드의 페이지를 활성화하여 스레드 전환
   테이블을 삭제하고, 이전 스레드가 죽어가고 있으면 이를 삭제합니다.

   이 함수를 호출하면 방금 스레드에서 전환했습니다.
   PREV, 새 스레드가 이미 실행 중이고 인터럽트가 발생했습니다.
   여전히 비활성화되어 있습니다.

   스레드 스위치가 활성화될 때까지 printf()을 호출하는 것은 안전하지 않습니다.
   완벽한. 실제로 이는 printf() s가 다음과 같아야 함을 의미합니다.
   함수 끝에 추가되었습니다. */
static void
thread_launch (struct thread *th) {
	uint64_t tf_cur = (uint64_t) &running_thread ()->tf;
	uint64_t tf = (uint64_t) &th->tf;
	ASSERT (intr_get_level () == INTR_OFF);

	/* 주요 스위칭 로직.
	 * 먼저 전체 실행 컨텍스트를 intr_frame으로 복원합니다.
	 * 그런 다음 do_iret를 호출하여 다음 스레드로 전환합니다.
	 * 여기서는 SHOULD NOT 스택을 사용합니다.
	 * 전환이 완료될 때까지. */
	__asm __volatile (
			/* 사용될 레지스터를 저장합니다. */
			"push %%rax\n"
			"push %%rbx\n"
			"push %%rcx\n"
			/* 입력을 한 번 가져옵니다. */
			"movq %0, %%rax\n"
			"movq %1, %%rcx\n"
			"movq %%r15, 0(%%rax)\n"
			"movq %%r14, 8(%%rax)\n"
			"movq %%r13, 16(%%rax)\n"
			"movq %%r12, 24(%%rax)\n"
			"movq %%r11, 32(%%rax)\n"
			"movq %%r10, 40(%%rax)\n"
			"movq %%r9, 48(%%rax)\n"
			"movq %%r8, 56(%%rax)\n"
			"movq %%rsi, 64(%%rax)\n"
			"movq %%rdi, 72(%%rax)\n"
			"movq %%rbp, 80(%%rax)\n"
			"movq %%rdx, 88(%%rax)\n"
			"pop %%rbx\n"              // rcx를 저장했습니다.
			"movq %%rbx, 96(%%rax)\n"
			"pop %%rbx\n"              // 저장된 rbx
			"movq %%rbx, 104(%%rax)\n"
			"pop %%rbx\n"              // 저장된 랙스
			"movq %%rbx, 112(%%rax)\n"
			"addq $120, %%rax\n"
			"movw %%es, (%%rax)\n"
			"movw %%ds, 8(%%rax)\n"
			"addq $32, %%rax\n"
			"call __next\n"         // 현재 찢어짐을 읽으십시오.
			"__next:\n"
			"pop %%rbx\n"
			"addq $(out_iret -  __next), %%rbx\n"
			"movq %%rbx, 0(%%rax)\n" // 찢다
			"movw %%cs, 8(%%rax)\n"  // CS
			"pushfq\n"
			"popq %%rbx\n"
			"mov %%rbx, 16(%%rax)\n" // 깃발
			"mov %%rsp, 24(%%rax)\n" // RSP
			"movw %%ss, 32(%%rax)\n"
			"mov %%rcx, %%rdi\n"
			"call do_iret\n"
			"out_iret:\n"
			: : "g"(tf_cur), "g" (tf) : "memory"
			);
}

/* 새로운 프로세스를 예약합니다. 진입 시 인터럽트는 꺼져 있어야 합니다.
 * 이 함수는 현재 스레드의 상태를 상태로 수정한 다음
 * 실행할 다른 스레드를 찾아 해당 스레드로 전환합니다.
 * schedule()에서 printf()을 호출하는 것은 안전하지 않습니다. */
static void
do_schedule (int status) {
	ASSERT (intr_get_level () == INTR_OFF);
	ASSERT (thread_current ()->status == THREAD_RUNNING);
	while (!list_empty (&destruction_req)) {
		struct thread *victim =
			list_entry (list_pop_front (&destruction_req), struct thread, elem);
		palloc_free_page (victim);
	}
	thread_current ()->status = status;
	schedule ();
}

static void
schedule (void) {
	struct thread *curr = running_thread ();
	struct thread *next = next_thread_to_run ();

	ASSERT (intr_get_level () == INTR_OFF);
	ASSERT (curr->status != THREAD_RUNNING);
	ASSERT (is_thread (next));
	/* 우리를 달리고 있는 것으로 표시하세요. */
	next->status = THREAD_RUNNING;

	/* 새로운 시간 분할을 시작합니다. */
	thread_ticks = 0;

#ifdef USERPROG
	/* 새 주소 공간을 활성화합니다. */
	process_activate (next);
#endif

	if (curr != next) {
		/* 전환한 스레드가 죽어가고 있으면 해당 스레드를 파괴하세요.
		   실. thread_exit()이(가) 발생하지 않도록 이 작업이 늦게 발생해야 합니다.
		   깔개를 꺼내십시오.
		   페이지가 다음과 같기 때문에 여기에 페이지 무료 요청을 대기열에 추가합니다.
		   현재 스택에서 사용됩니다.
		   실제 파괴 로직은 시작 부분에서 호출됩니다.
		   schedule(). */
		if (curr && curr->status == THREAD_DYING && curr != initial_thread) {
			ASSERT (curr != next);
			list_push_back (&destruction_req, &curr->elem);
		}

		/* 스레드를 전환하기 전에 먼저 정보를 저장합니다.
		 * 현재 실행 중입니다. */
		thread_launch (next);
	}
}

/* 새 스레드에 사용할 tid를 반환합니다. */
static tid_t
allocate_tid (void) {
	static tid_t next_tid = 1;
	tid_t tid;

	lock_acquire (&tid_lock);
	tid = next_tid++;
	lock_release (&tid_lock);

	return tid;
}

static bool
cmp_wakeup_ticks_less (const struct list_elem *a, const struct list_elem *b,
		void *aux UNUSED) {
	const struct thread *ta = list_entry (a, struct thread, elem);
	const struct thread *tb = list_entry (b, struct thread, elem);

	if (ta->wakeup_ticks == tb->wakeup_ticks)
		return ta->priority > tb->priority;

	return ta->wakeup_ticks < tb->wakeup_ticks;
}

/* 헷갈릴 수 있는데, 큰 게 앞에 위치하도록 조건을 명세의 반대로 한다. */
bool
cmp_priority_more (const struct list_elem *a, const struct list_elem *b,
		void *aux UNUSED) {
	const struct thread *ta = list_entry (a, struct thread, elem);
	const struct thread *tb = list_entry (b, struct thread, elem);

	/* 같은 priority는 false를 반환해서 기존 항목 뒤에 간다. */
	return ta->priority > tb->priority;
}

/* cmp_priority 와 동일하게 큰게 앞에(작은걸로 판단) 오게 처리. */
bool
cmp_donors_priority_more (const struct list_elem *a, const struct list_elem *b,
		void *aux UNUSED) {
	const struct thread *ta = list_entry (a, struct thread, d_elem);
	const struct thread *tb = list_entry (b, struct thread, d_elem);

	return ta->priority > tb->priority;
}

/* priority를 donations를 순회하면서 올바르게 보정함.
   donations에 변화가 생기거나,
   lock chain?의 root의 우선순위 변경이 발생했을 때 항상 실행되어야 함. */
void refresh_priority_in_donors (void) {
	struct thread *cur = thread_current ();

	cur->priority = cur->base_priority;

	if (!list_empty (&cur->donations)) {
		// _more 비교 함수라 min이 가장 큰 값을 반환함.
		int max_priority = list_entry (list_min (&cur->donations,
				cmp_donors_priority_more, NULL), struct thread, d_elem)->priority;

		if (cur->priority < max_priority) {
			cur->priority = max_priority;
		}
	}
}
