/* This file is derived from source code for the Nachos
   instructional operating system.  The Nachos copyright notice
   is reproduced in full below. */

/* Copyright (c) 1992-1996 The Regents of the University of California.
   All rights reserved.

   Permission to use, copy, modify, and distribute this software
   and its documentation for any purpose, without fee, and
   without written agreement is hereby granted, provided that the
   above copyright notice and the following two paragraphs appear
   in all copies of this software.

   IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO
   ANY PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR
   CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE
   AND ITS DOCUMENTATION, EVEN IF THE UNIVERSITY OF CALIFORNIA
   HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY
   WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
   PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS"
   BASIS, AND THE UNIVERSITY OF CALIFORNIA HAS NO OBLIGATION TO
   PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
   MODIFICATIONS.
   */

#include "threads/synch.h"
#include <stdio.h>
#include <string.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

static bool cmp_sema_priority (const struct list_elem *a,
		const struct list_elem *b,
		void *aux UNUSED);
static void lock_donors_donate_priority_chain (struct thread *cur,
		struct lock* lock);
static void lock_donors_remove (struct lock* lock);

/* Initializes semaphore SEMA to VALUE.  A semaphore is a
   nonnegative integer along with two atomic operators for
   manipulating it:

   - down or "P": wait for the value to become positive, then
   decrement it.

   - up or "V": increment the value (and wake up one waiting
   thread, if any). */
/* 세마포어 SEMA를 VALUE로 초기화한다. 세마포어는 음수가 아닌 정수와 이를
   조작하는 두 atomic operator로 이루어진다.

   - down 또는 "P": 값이 양수가 될 때까지 기다린 뒤 값을 감소시킨다.

   - up 또는 "V": 값을 증가시키고, 기다리는 스레드가 있으면 하나를 깨운다. */
void
sema_init (struct semaphore *sema, unsigned value) {
	ASSERT (sema != NULL);

	sema->value = value;
	list_init (&sema->waiters);
}

/* Down or "P" operation on a semaphore.  Waits for SEMA's value
   to become positive and then atomically decrements it.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but if it sleeps then the next scheduled
   thread will probably turn interrupts back on. This is
   sema_down function. */
/* 세마포어에 대한 down 또는 "P" operation. SEMA의 값이 양수가 될 때까지 기다린
   뒤 atomic하게 감소시킨다.

   이 함수는 sleep할 수 있으므로 interrupt handler 안에서 호출하면 안 된다.
   인터럽트가 비활성화된 상태에서 호출할 수는 있지만, sleep하게 되면 다음에
   스케줄되는 스레드가 인터럽트를 다시 켤 가능성이 높다. 이것은 sema_down
   함수이다. */
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

/* Down or "P" operation on a semaphore, but only if the
   semaphore is not already 0.  Returns true if the semaphore is
   decremented, false otherwise.

   This function may be called from an interrupt handler. */
/* 세마포어에 대한 down 또는 "P" operation이지만, 세마포어가 이미 0이 아닐 때만
   수행한다. 세마포어가 감소되면 true, 아니면 false를 리턴한다.

   이 함수는 interrupt handler에서 호출할 수 있다. */
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

/* Up or "V" operation on a semaphore.  Increments SEMA's value
   and wakes up one thread of those waiting for SEMA, if any.

   This function may be called from an interrupt handler. */
/* 세마포어에 대한 up 또는 "V" operation. SEMA의 값을 증가시키고, SEMA를 기다리는
   스레드가 있으면 그중 하나를 깨운다.

   이 함수는 interrupt handler에서 호출할 수 있다. */
void
sema_up (struct semaphore *sema) {
	enum intr_level old_level;

	ASSERT (sema != NULL);

	old_level = intr_disable ();
	if (!list_empty (&sema->waiters)) {
		if (!thread_mlfqs) {
			// Priority Donation 때문에 정렬 필요, list_pop_front를 해야하니까
			list_sort (&sema->waiters, cmp_priority_more, NULL);
		}
		thread_unblock (list_entry (list_pop_front (&sema->waiters),
					struct thread, elem));
	}
	sema->value++;
	thread_yield_if_needed ();
	intr_set_level (old_level);
}

static void sema_test_helper (void *sema_);

/* Self-test for semaphores that makes control "ping-pong"
   between a pair of threads.  Insert calls to printf() to see
   what's going on. */
/* 두 스레드 사이에서 제어가 "ping-pong"되게 하는 세마포어 self-test.
   진행 상황을 보려면 printf() 호출을 넣어 본다. */
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

/* Thread function used by sema_self_test(). */
/* sema_self_test()에서 사용하는 스레드 함수. */
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

/* Initializes LOCK.  A lock can be held by at most a single
   thread at any given time.  Our locks are not "recursive", that
   is, it is an error for the thread currently holding a lock to
   try to acquire that lock.

   A lock is a specialization of a semaphore with an initial
   value of 1.  The difference between a lock and such a
   semaphore is twofold.  First, a semaphore can have a value
   greater than 1, but a lock can only be owned by a single
   thread at a time.  Second, a semaphore does not have an owner,
   meaning that one thread can "down" the semaphore and then
   another one "up" it, but with a lock the same thread must both
   acquire and release it.  When these restrictions prove
   onerous, it's a good sign that a semaphore should be used,
   instead of a lock. */
/* LOCK을 초기화한다. lock은 어떤 시점에도 최대 하나의 스레드만 보유할 수 있다.
   여기의 lock은 "recursive"가 아니다. 즉, 현재 lock을 들고 있는 스레드가 같은
   lock을 다시 acquire하려고 하면 에러이다.

   lock은 초기값이 1인 세마포어의 특수한 형태이다. lock과 그런 세마포어의 차이는
   두 가지이다. 첫째, 세마포어는 1보다 큰 값을 가질 수 있지만 lock은 한 번에 한
   스레드만 소유할 수 있다. 둘째, 세마포어에는 owner가 없다. 한 스레드가
   세마포어를 "down"하고 다른 스레드가 "up"할 수 있지만, lock은 같은 스레드가
   acquire와 release를 모두 해야 한다. 이러한 제약이 부담스럽다면 lock 대신
   세마포어를 써야 한다는 신호이다. */
void
lock_init (struct lock *lock) {
	ASSERT (lock != NULL);

	lock->holder = NULL;
	sema_init (&lock->semaphore, 1);
}

/* Acquires LOCK, sleeping until it becomes available if
   necessary.  The lock must not already be held by the current
   thread.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
/* LOCK을 acquire한다. 필요하면 사용 가능해질 때까지 sleep한다. lock은 현재
   스레드가 이미 들고 있으면 안 된다.

   이 함수는 sleep할 수 있으므로 interrupt handler 안에서 호출하면 안 된다.
   인터럽트가 비활성화된 상태에서 호출할 수는 있지만, sleep해야 하면 인터럽트가
   다시 켜진다. */
void
lock_acquire (struct lock *lock) {
	ASSERT (lock != NULL);
	ASSERT (!intr_context ());
	ASSERT (!lock_held_by_current_thread (lock));

	struct thread* cur = thread_current ();
	if (!thread_mlfqs) {
		lock_donors_donate_priority_chain (cur, lock);
	}

	sema_down (&lock->semaphore);
	cur->wait_on_lock = NULL;
	lock->holder = cur;
}

/* Tries to acquires LOCK and returns true if successful or false
   on failure.  The lock must not already be held by the current
   thread.

   This function will not sleep, so it may be called within an
   interrupt handler. */
/* LOCK acquire를 시도하고 성공하면 true, 실패하면 false를 리턴한다. lock은 현재
   스레드가 이미 들고 있으면 안 된다.

   이 함수는 sleep하지 않으므로 interrupt handler 안에서 호출할 수 있다. */
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

/* Releases LOCK, which must be owned by the current thread.
   This is lock_release function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to release a lock within an interrupt
   handler. */
/* 현재 스레드가 소유해야 하는 LOCK을 release한다.
   이것은 lock_release 함수이다.

   interrupt handler는 lock을 acquire할 수 없으므로, interrupt handler 안에서
   lock을 release하려고 하는 것은 의미가 없다. */
void
lock_release (struct lock *lock) {
	ASSERT (lock != NULL);
	ASSERT (lock_held_by_current_thread (lock));

	if (!thread_mlfqs) {
		lock_donors_remove (lock);
		thread_donors_recalc_priorities ();
	}

	lock->holder = NULL;
	sema_up (&lock->semaphore);
}

/* Returns true if the current thread holds LOCK, false
   otherwise.  (Note that testing whether some other thread holds
   a lock would be racy.) */
/* 현재 스레드가 LOCK을 들고 있으면 true, 아니면 false를 리턴한다.
   다른 스레드가 lock을 들고 있는지 검사하는 것은 racy하다는 점에 주의한다. */
bool
lock_held_by_current_thread (const struct lock *lock) {
	ASSERT (lock != NULL);

	return lock->holder == thread_current ();
}

/* One semaphore in a list. */
/* 리스트 안의 세마포어 하나. */
struct semaphore_elem {
	struct list_elem elem;              /* List element. */
	/* 리스트 element. */
	struct semaphore semaphore;         /* This semaphore. */
	/* 이 세마포어. */

	struct thread *thread;
};

/* Initializes condition variable COND.  A condition variable
   allows one piece of code to signal a condition and cooperating
   code to receive the signal and act upon it. */
/* condition variable COND를 초기화한다. condition variable은 한 코드 조각이
   condition을 signal하고, 협력하는 코드가 signal을 받아 동작할 수 있게 한다. */
void
cond_init (struct condition *cond) {
	ASSERT (cond != NULL);

	list_init (&cond->waiters);
}

/* Atomically releases LOCK and waits for COND to be signaled by
   some other piece of code.  After COND is signaled, LOCK is
   reacquired before returning.  LOCK must be held before calling
   this function.

   The monitor implemented by this function is "Mesa" style, not
   "Hoare" style, that is, sending and receiving a signal are not
   an atomic operation.  Thus, typically the caller must recheck
   the condition after the wait completes and, if necessary, wait
   again.

   A given condition variable is associated with only a single
   lock, but one lock may be associated with any number of
   condition variables.  That is, there is a one-to-many mapping
   from locks to condition variables.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
/* LOCK을 atomic하게 release하고 다른 코드가 COND를 signal할 때까지 기다린다.
   COND가 signal되면 리턴하기 전에 LOCK을 다시 acquire한다. 이 함수를 호출하기
   전에 LOCK을 들고 있어야 한다.

   이 함수가 구현하는 monitor는 "Hoare" style이 아니라 "Mesa" style이다. 즉,
   signal을 보내는 것과 받는 것이 atomic operation이 아니다. 따라서 일반적으로
   호출자는 wait가 끝난 뒤 condition을 다시 검사하고, 필요하면 다시 wait해야
   한다.

   주어진 condition variable은 하나의 lock에만 연결되지만, 하나의 lock은 여러
   condition variable에 연결될 수 있다. 즉, lock에서 condition variable로
   one-to-many 맵핑이 있다.

   이 함수는 sleep할 수 있으므로 interrupt handler 안에서 호출하면 안 된다.
   인터럽트가 비활성화된 상태에서 호출할 수는 있지만, sleep해야 하면 인터럽트가
   다시 켜진다. */
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

/* If any threads are waiting on COND (protected by LOCK), then
   this function signals one of them to wake up from its wait.
   LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
/* LOCK으로 보호되는 COND를 기다리는 스레드가 있다면, 이 함수는 그중 하나에
   signal을 보내 wait에서 깨운다. 이 함수를 호출하기 전에 LOCK을 들고 있어야
   한다.

   interrupt handler는 lock을 acquire할 수 없으므로, interrupt handler 안에서
   condition variable에 signal하려고 하는 것은 의미가 없다. */
void
cond_signal (struct condition *cond, struct lock *lock UNUSED) {
	ASSERT (cond != NULL);
	ASSERT (lock != NULL);
	ASSERT (!intr_context ());
	ASSERT (lock_held_by_current_thread (lock));

	if (!list_empty (&cond->waiters)) {
		if (!thread_mlfqs) {
			// Priority Donation 때문에 정렬 필요, list_pop_front를 해야하니까
			list_sort (&cond->waiters, cmp_sema_priority, NULL);
		}
		sema_up (&list_entry (list_pop_front (&cond->waiters),
					struct semaphore_elem, elem)->semaphore);
	}
}
/* Wakes up all threads, if any, waiting on COND (protected by
   LOCK).  LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
/* LOCK으로 보호되는 COND를 기다리는 모든 스레드를 깨운다. 이 함수를 호출하기
   전에 LOCK을 들고 있어야 한다.

   interrupt handler는 lock을 acquire할 수 없으므로, interrupt handler 안에서
   condition variable에 signal하려고 하는 것은 의미가 없다. */
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

static void
lock_donors_donate_priority_chain (struct thread *cur, struct lock* lock) {
	struct thread *holder = lock->holder;
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
}

static void
lock_donors_remove (struct lock* lock) {
	ASSERT (lock != NULL);
	ASSERT (lock_held_by_current_thread (lock));

	struct thread *holder = lock->holder; // holder는 curr로 ASSERT 됨

	struct list_elem *e;
	e = list_begin (&holder->donations);
	while (e != list_end (&holder->donations)) {
		struct thread *t = list_entry (e, struct thread, d_elem);

		if (t->wait_on_lock == lock) {
			e = list_remove (e);
		} else {
			e = list_next (e);
		}
	}
}
