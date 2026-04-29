/* 이 파일은 Nachos의 소스 코드에서 파생되었습니다.
   교육용 운영체제. Nachos 저작권 고지
   아래에 전체 내용이 재현되어 있습니다. */

/* 저작권 (c) 1992-1996 캘리포니아 대학교 이사회.
   모든 권리 보유.

   이 소프트웨어를 사용, 복사, 수정 및 배포할 수 있는 권한
   어떤 목적으로든 수수료 없이 해당 문서를
   서면 동의 없이 이에 따라 승인됩니다.
   위의 저작권 표시와 다음 두 단락이 나타납니다.
   이 소프트웨어의 모든 복사본에 포함됩니다.

   어떠한 경우에도 캘리포니아 대학교는 이 소프트웨어와 문서의 사용으로
   인해 발생하는 직접, 간접, 특수, 부수 또는 결과적 손해에 대해
   책임지지 않습니다. 캘리포니아 대학교가 그러한 손해 가능성을
   통지받았더라도 마찬가지입니다.

   캘리포니아 대학교는 상품성 및 특정 목적 적합성에 대한 묵시적 보증을
   포함하되 이에 한정되지 않는 모든 보증을 명시적으로 부인합니다.
   이 소프트웨어는 "있는 그대로" 제공되며, 캘리포니아 대학교는 유지보수,
   지원, 업데이트, 개선 또는 수정 제공 의무를 지지 않습니다.
   */

#include "threads/synch.h"
#include <stdio.h>
#include <string.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

static bool cmp_sema_priority (const struct list_elem *a,
		const struct list_elem *b,
		void *aux UNUSED);

/* 세마포어 SEMA을 VALUE로 초기화합니다. 세마포어는
   음이 아닌 정수와 두 개의 원자 연산자
   조작하기:

   - down 또는 "P": 값이 양수가 될 때까지 기다린 다음 감소시킨다.

   - up 또는 "V": 값을 증가시키고, 대기 중인 스레드가 있으면 깨운다. */
void
sema_init (struct semaphore *sema, unsigned value) {
	ASSERT (sema != NULL);

	sema->value = value;
	list_init (&sema->waiters);
}

/* 세마포어에 대한 Down 또는 "P" 작업입니다. SEMA의 값을 기다립니다.
   양수로 변한 다음 원자적으로 감소시킵니다.

   이 함수는 잠자기 상태일 수 있으므로
   인터럽트 핸들러. 이 함수는 다음과 같이 호출될 수 있습니다.
   인터럽트는 비활성화되지만 잠자기 상태이면 다음 예정된 인터럽트
   스레드는 아마도 인터럽트를 다시 켤 것입니다. 이것은
   sema_down 함수. */
void
sema_down (struct semaphore *sema) {
	enum intr_level old_level;

	ASSERT (sema != NULL);
	ASSERT (!intr_context ());

	old_level = intr_disable ();
	while (sema->value == 0) {
		list_insert_ordered (&sema->waiters, &thread_current ()->elem,
				cmp_priority_more, NULL);
		thread_block ();
	}
	sema->value--;
	intr_set_level (old_level);
}

/* 세마포어에 대한 Down 또는 "P" 연산.
   세마포어가 아직 0이 아닙니다. 세마포어가 0인 경우 true를 반환합니다.
   감소하고, 그렇지 않으면 거짓입니다.

   이 함수는 인터럽트 핸들러에서 호출될 수 있습니다. */
bool
sema_try_down (struct semaphore *sema) {
	enum intr_level old_level;
	bool success;

	ASSERT (sema != NULL);

	old_level = intr_disable ();
	if (sema->value > 0)
	{
		sema->value--;
		success = true;
	}
	else
		success = false;
	intr_set_level (old_level);

	return success;
}

/* 세마포어에 대한 Up 또는 "V" 작업입니다. SEMA의 값을 증가시킵니다.
   SEMA 을 기다리는 스레드 중 하나가 있으면 깨웁니다.

   이 함수는 인터럽트 핸들러에서 호출될 수 있습니다. */
void
sema_up (struct semaphore *sema) {
	enum intr_level old_level;

	ASSERT (sema != NULL);

	old_level = intr_disable ();
	if (!list_empty (&sema->waiters)) {
		list_sort (&sema->waiters, cmp_priority_more, NULL); // Priority Donation 때문
		thread_unblock (list_entry (list_pop_front (&sema->waiters),
					struct thread, elem));
	}
	sema->value++;
	thread_yield_if_needed ();
	intr_set_level (old_level);
}

static void sema_test_helper (void *sema_);

/* 제어를 "탁구"하게 만드는 세마포어에 대한 자체 테스트
   한 쌍의 스레드 사이. 확인하려면 printf()에 대한 호출을 삽입하세요.
   무슨 일이야. */
void
sema_self_test (void) {
	struct semaphore sema[2];
	int i;

	printf ("Testing semaphores...");
	sema_init (&sema[0], 0);
	sema_init (&sema[1], 0);
	thread_create ("sema-test", PRI_DEFAULT, sema_test_helper, &sema);
	for (i = 0; i < 10; i++)
	{
		sema_up (&sema[0]);
		sema_down (&sema[1]);
	}
	printf ("done.\n");
}

/* sema_self_test() 에서 사용하는 스레드 함수입니다. */
static void
sema_test_helper (void *sema_) {
	struct semaphore *sema = sema_;
	int i;

	for (i = 0; i < 10; i++)
	{
		sema_down (&sema[0]);
		sema_up (&sema[1]);
	}
}

/* LOCK을 초기화합니다. 잠금은 최대 한 명이 보유할 수 있습니다.
   스레드를 언제든지 사용할 수 있습니다. 우리의 잠금은 "재귀적"이 아닙니다.
   즉, 현재 잠금을 보유하고 있는 스레드에 대한 오류입니다.
   그 자물쇠를 얻으려고 노력하십시오.

   잠금은 초기 문자가 있는 세마포어의 특수화입니다.
   값은 1입니다. 잠금과 이러한 잠금의 차이점은
   세마포어는 두 가지입니다. 첫째, 세마포어는 값을 가질 수 있습니다
   1보다 크지만 잠금은 단일 소유자만 소유할 수 있습니다.
   한 번에 스레드. 둘째, 세마포어에는 소유자가 없습니다.
   이는 하나의 스레드가 세마포어를 "다운"할 수 있음을 의미합니다.
   다른 스레드는 "위로" 이동하지만 잠금을 사용하면 동일한 스레드가 둘 다 있어야 합니다.
   획득하고 출시하세요. 이러한 제한이 입증되면
   번거롭지만 세마포어를 사용해야 한다는 좋은 신호입니다.
   자물쇠 대신. */
void
lock_init (struct lock *lock) {
	ASSERT (lock != NULL);

	lock->holder = NULL;
	sema_init (&lock->semaphore, 1);
}

/* LOCK을 획득하고, 다음과 같은 경우 사용할 수 있을 때까지 잠자기합니다.
   필요한. 현재 잠금이 이미 보유되어 있지 않아야 합니다.
   실.

   이 함수는 잠자기 상태일 수 있으므로
   인터럽트 핸들러. 이 함수는 다음과 같이 호출될 수 있습니다.
   인터럽트는 비활성화되지만 다음과 같은 경우 인터럽트가 다시 켜집니다.
   우리는 자야 해. */
void
lock_acquire (struct lock *lock) {
	ASSERT (lock != NULL);
	ASSERT (!intr_context ());
	ASSERT (!lock_held_by_current_thread (lock));

	struct thread *holder = lock->holder;
	struct thread *cur = thread_current ();
	if (holder != NULL) {
		cur->wait_on_lock = lock;
		list_push_back (&holder->donations, &cur->d_elem);

		// lock holder 체인의 끝(편의상 root)까지 순회하며 이동
		struct thread *t = holder;
		while (t != NULL) {
			if (t->priority < cur->priority) { // 내 우선순위보다 작으면 갱신
				t->priority = cur->priority;
			}
			if (t->wait_on_lock == NULL) {
				break;
			}
			t = t->wait_on_lock->holder;
		}
	}

	sema_down (&lock->semaphore);
	cur->wait_on_lock = NULL;
	lock->holder = cur;
}

/* LOCK 획득을 시도하고 성공하거나 거짓인 경우 true를 반환합니다.
   실패시. 현재 잠금이 이미 보유되어 있지 않아야 합니다.
   실.

   이 함수는 잠들지 않으므로 다음 시간 내에 호출될 수 있습니다.
   인터럽트 핸들러. */
bool
lock_try_acquire (struct lock *lock) {
	bool success;

	ASSERT (lock != NULL);
	ASSERT (!lock_held_by_current_thread (lock));

	success = sema_try_down (&lock->semaphore);
	if (success)
		lock->holder = thread_current ();
	return success;
}

/* 현재 스레드가 소유해야 하는 LOCK 을 해제합니다.
   lock_release 함수입니다.

   인터럽트 핸들러는 잠금을 획득할 수 없으므로 잠금을 획득하지 않습니다.
   인터럽트 내에서 잠금을 해제하려고 시도하는 것이 합리적입니다.
   매니저. */
void
lock_release (struct lock *lock) {
	ASSERT (lock != NULL);
	ASSERT (lock_held_by_current_thread (lock));

	struct thread *cur = thread_current ();

	struct list_elem *e;
	e = list_begin (&cur->donations);
	while (e != list_end (&cur->donations)) {
		struct thread *t = list_entry (e, struct thread, d_elem);

		if (t->wait_on_lock == lock) {
			e = list_remove (e);
		} else {
			e = list_next (e);
		}
	}

	refresh_priority_in_donors ();

	lock->holder = NULL;
	sema_up (&lock->semaphore);
}

/* 현재 스레드에 LOCK이 있으면 true를 반환하고, false를 반환합니다.
   그렇지 않으면. (다른 스레드가 보유하고 있는지 테스트하는 것에 유의하세요.
   자물쇠는 선정적일 것입니다.) */
bool
lock_held_by_current_thread (const struct lock *lock) {
	ASSERT (lock != NULL);

	return lock->holder == thread_current ();
}

/* 목록에 하나의 세마포어가 있습니다. */
struct semaphore_elem {
	struct list_elem elem;              /* 목록 요소. */
	struct semaphore semaphore;         /* 이 세마포어. */

	struct thread *thread;
};

/* 조건 변수 COND 을 초기화합니다. 조건 변수
   하나의 코드가 상태를 알리고 협력하도록 허용합니다.
   신호를 수신하고 이에 따라 조치를 취하는 코드입니다. */
void
cond_init (struct condition *cond) {
	ASSERT (cond != NULL);

	list_init (&cond->waiters);
}

/* LOCK을 원자적으로 해제하고 COND이 신호를 받을 때까지 기다립니다.
   다른 코드 조각. COND이 신호를 받은 후 LOCK은(는)
   돌아오기 전에 다시 획득했습니다. 호출하기 전에 LOCK을(를) 누르고 있어야 합니다.
   이 기능.

   이 기능으로 구현된 모니터는 "Mesa" 스타일이 아닌 "Mesa" 스타일입니다.
   "Hoare" 스타일, 즉 신호를 보내고 받는 것은
   원자 연산. 따라서 일반적으로 호출자는 다시 확인해야 합니다.
   대기가 완료된 후의 조건 및 필요한 경우 대기
   다시.

   주어진 조건 변수는 단일 조건과만 연관됩니다.
   그러나 하나의 잠금은 여러 개의 잠금과 연관될 수 있습니다.
   조건변수. 즉, 일대다 매핑이 ​​있습니다.
   잠금에서 조건 변수까지.

   이 함수는 잠자기 상태일 수 있으므로
   인터럽트 핸들러. 이 함수는 다음과 같이 호출될 수 있습니다.
   인터럽트는 비활성화되지만 다음과 같은 경우 인터럽트가 다시 켜집니다.
   우리는 자야 해. */
void
cond_wait (struct condition *cond, struct lock *lock) {
	struct semaphore_elem waiter;

	ASSERT (cond != NULL);
	ASSERT (lock != NULL);
	ASSERT (!intr_context ());
	ASSERT (lock_held_by_current_thread (lock));

	sema_init (&waiter.semaphore, 0);
	waiter.thread = thread_current ();
	list_insert_ordered (&cond->waiters, &waiter.elem, cmp_sema_priority, NULL);
	lock_release (lock);
	sema_down (&waiter.semaphore);
	lock_acquire (lock);
}

/* COND(LOCK 로 보호됨)에서 대기 중인 스레드가 있으면
   이 함수는 그 중 하나에게 대기 상태에서 깨어나도록 신호를 보냅니다.
   이 함수를 호출하기 전에 LOCK을 보유해야 합니다.

   인터럽트 핸들러는 잠금을 획득할 수 없으므로 잠금을 획득하지 않습니다.
   내에서 조건 변수에 신호를 보내는 것이 합리적입니다.
   인터럽트 핸들러. */
void
cond_signal (struct condition *cond, struct lock *lock UNUSED) {
	ASSERT (cond != NULL);
	ASSERT (lock != NULL);
	ASSERT (!intr_context ());
	ASSERT (lock_held_by_current_thread (lock));

	if (!list_empty (&cond->waiters)) {
		list_sort (&cond->waiters, cmp_sema_priority, NULL); // Priority Donation 때문
		sema_up (&list_entry (list_pop_front (&cond->waiters),
					struct semaphore_elem, elem)->semaphore);
	}
}

/* COND을(를 통해 보호되는) 대기 중인 모든 스레드를 깨웁니다.
   LOCK). 이 함수를 호출하기 전에 LOCK을 보유해야 합니다.

   인터럽트 핸들러는 잠금을 획득할 수 없으므로 잠금을 획득하지 않습니다.
   내에서 조건 변수에 신호를 보내는 것이 합리적입니다.
   인터럽트 핸들러. */
void
cond_broadcast (struct condition *cond, struct lock *lock) {
	ASSERT (cond != NULL);
	ASSERT (lock != NULL);

	while (!list_empty (&cond->waiters))
		cond_signal (cond, lock);
}


static bool
cmp_sema_priority (const struct list_elem *a, const struct list_elem *b,
		void *aux UNUSED) {
	struct semaphore_elem *sa = list_entry (a, struct semaphore_elem, elem);
	struct semaphore_elem *sb = list_entry (b, struct semaphore_elem, elem);

	return sa->thread->priority > sb->thread->priority;
}
