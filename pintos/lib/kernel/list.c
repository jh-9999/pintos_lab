#include "list.h"
#include "../debug.h"

/* 이중 연결 목록에는 "head"라는 두 개의 헤더 요소가 있습니다.
   첫 번째 요소 바로 앞과 바로 뒤의 "꼬리"
   마지막 요소. 앞 헤더의 `prev' 링크는 null입니다.
   뒷 헤더의 '다음' 링크입니다. 다른 두 링크
   목록의 내부 요소를 통해 서로를 가리킵니다.

   빈 목록은 다음과 같습니다.

   +------+     +------+
   <---| 머리 |<--->| 꼬리 |--->
   +------+     +------+

   두 개의 요소가 포함된 목록은 다음과 같습니다.

   +------+     +-------+     +-------+     +------+
   <---| 머리 |<--->| 1 |<--->| 2 |<--->| 꼬리 |<--->
   +------+     +-------+     +-------+     +------+

   이 배열의 대칭으로 인해 많은 특수 요소가 제거됩니다.
   목록 처리의 경우. 예를 들어 다음을 살펴보세요.
   list_remove(): 두 개의 포인터 할당만 필요하며
   조건부. 코드보다 훨씬 간단해요
   헤더 요소가 없습니다.

   (각 헤더 요소의 포인터 중 하나만 사용되기 때문에,
   실제로 이를 단일 헤더 요소로 결합할 수 있습니다.
   이 단순함을 희생하지 않고. 그러나 두 개의 별도 사용
   요소를 사용하면 일부 요소를 약간 확인할 수 있습니다.
   이는 가치 있는 작업입니다.) */

static bool is_sorted (struct list_elem *a, struct list_elem *b,
		list_less_func *less, void *aux) UNUSED;

/* ELEM이 헤드이면 true를 반환하고, 그렇지 않으면 false를 반환합니다. */
static inline bool
is_head (struct list_elem *elem) {
	return elem != NULL && elem->prev == NULL && elem->next != NULL;
}

/* ELEM이 내부 요소인 경우 true를 반환합니다.
   그렇지 않으면 거짓입니다. */
static inline bool
is_interior (struct list_elem *elem) {
	return elem != NULL && elem->prev != NULL && elem->next != NULL;
}

/* ELEM이 꼬리이면 true를 반환하고, 그렇지 않으면 false를 반환합니다. */
static inline bool
is_tail (struct list_elem *elem) {
	return elem != NULL && elem->prev != NULL && elem->next == NULL;
}

/* LIST을 빈 목록으로 초기화합니다. */
void
list_init (struct list *list) {
	ASSERT (list != NULL);
	list->head.prev = NULL;
	list->head.next = &list->tail;
	list->tail.prev = &list->head;
	list->tail.next = NULL;
}

/* LIST 의 시작 부분을 반환합니다.  */
struct list_elem *
list_begin (struct list *list) {
	ASSERT (list != NULL);
	return list->head.next;
}

/* 목록에서 ELEM 뒤에 있는 요소를 반환합니다. ELEM이(가)
   목록의 마지막 요소는 목록 꼬리를 반환합니다. 결과는
   ELEM 자체가 목록 꼬리인 경우 정의되지 않습니다. */
struct list_elem *
list_next (struct list_elem *elem) {
	ASSERT (is_head (elem) || is_interior (elem));
	return elem->next;
}

/* LIST의 꼬리를 반환합니다.

   list_end()은(는) 목록을 반복하는 데 자주 사용됩니다.
   앞에서 뒤로. list.h 상단의 큰 주석을 참조하세요.
   예. */
struct list_elem *
list_end (struct list *list) {
	ASSERT (list != NULL);
	return &list->tail;
}

/* 반복을 위해 LIST 의 역방향 시작을 반환합니다.
   LIST 역순으로 뒤에서 앞으로. */
struct list_elem *
list_rbegin (struct list *list) {
	ASSERT (list != NULL);
	return list->tail.prev;
}

/* 목록에서 ELEM 앞에 있는 요소를 반환합니다. ELEM이(가)
   목록의 첫 번째 요소는 목록 헤드를 반환합니다. 결과는
   ELEM 자체가 목록 헤드인 경우 정의되지 않습니다. */
struct list_elem *
list_prev (struct list_elem *elem) {
	ASSERT (is_interior (elem) || is_tail (elem));
	return elem->prev;
}

/* LIST의 머리를 반환합니다.

   list_rend()은(는) 목록을 반복하는 데 자주 사용됩니다.
   역순으로 뒤에서 앞으로. 일반적인 사용법은 다음과 같습니다.
   list.h 상단의 예를 따르세요.

   for (e = list_rbegin (&foo_list); e != list_rend (&foo_list);
   e = list_prev (e))
   {
   struct foo *f = list_entry (e, struct foo, elem);
   ...f로 뭔가를 해보세요...
   }
   */
struct list_elem *
list_rend (struct list *list) {
	ASSERT (list != NULL);
	return &list->head;
}

/* Return의 LIST 의 머리입니다.

   list_head()은 대체 스타일의 반복에 사용될 수 있습니다.
   목록을 통해, 예:

   e = list_head (&list);
   while ((e = list_next (e)) != list_end (&list))
   {
   ...
   }
   */
struct list_elem *
list_head (struct list *list) {
	ASSERT (list != NULL);
	return &list->head;
}

/* LIST 의 꼬리를 반환합니다. */
struct list_elem *
list_tail (struct list *list) {
	ASSERT (list != NULL);
	return &list->tail;
}

/* BEFORE 바로 앞에 ELEM을 삽입합니다.
   내부 요소 또는 꼬리. 후자의 경우는 다음과 같습니다.
   list_push_back(). */
void
list_insert (struct list_elem *before, struct list_elem *elem) {
	ASSERT (is_interior (before) || is_tail (before));
	ASSERT (elem != NULL);

	elem->prev = before->prev;
	elem->next = before;
	before->prev->next = elem;
	before->prev = elem;
}

/* FIRST~LAST(제외) 요소를 해당 요소에서 제거합니다.
   현재 목록을 찾은 다음 BEFORE 바로 앞에 삽입합니다.
   내부 요소이거나 꼬리일 수 있습니다. */
void
list_splice (struct list_elem *before,
		struct list_elem *first, struct list_elem *last) {
	ASSERT (is_interior (before) || is_tail (before));
	if (first == last)
		return;
	last = list_prev (last);

	ASSERT (is_interior (first));
	ASSERT (is_interior (last));

	/* 현재 목록에서 FIRST... LAST을(를) 완전히 제거합니다. */
	first->prev->next = last->next;
	last->next->prev = first->prev;

	/* FIRST... LAST을 새 목록에 연결합니다. */
	first->prev = before->prev;
	last->next = before;
	before->prev->next = first;
	before->prev = last;
}

/* LIST의 시작 부분에 ELEM을 삽입하여
   LIST 앞. */
void
list_push_front (struct list *list, struct list_elem *elem) {
	list_insert (list_begin (list), elem);
}

/* LIST 끝에 ELEM을 삽입하여
   LIST으로 돌아갑니다. */
void
list_push_back (struct list *list, struct list_elem *elem) {
	list_insert (list_end (list), elem);
}

/* 목록에서 ELEM을 제거하고 해당 요소를 반환합니다.
   그것을 따랐다. ELEM이 목록에 없으면 정의되지 않은 동작입니다.

   ELEM을(를) 목록의 요소로 처리하는 것은 안전하지 않습니다.
   그것을 제거합니다. 특히 list_next() 또는 list_prev()을 사용하면
   제거 후 ELEM에서 정의되지 않은 동작이 발생합니다. 이는 다음을 의미합니다.
   목록의 요소를 제거하는 순진한 루프는 실패합니다.

 ** DON'T DO THIS **
 for (e = list_begin (&list); e != list_end (&list); e = list_next (e))
 {
 ...do something with e...
 list_remove (e);
 }
 ** DON'T DO THIS **

 다음은 요소를 반복하고 제거하는 올바른 방법 중 하나입니다.
목록:

for (e = list_begin (&list); e != list_end (&list); e = list_remove (e))
{
...do something with e...
}

목록의 요소를 free()해야 한다면 다음과 같이 해야 합니다.
더 보수적입니다. 다음은 효과적인 대체 전략입니다.
그 경우에도:

while (!list_empty (&list))
{
struct list_elem *e = list_pop_front (&list);
...do something with e...
}
*/
struct list_elem *
list_remove (struct list_elem *elem) {
	ASSERT (is_interior (elem));
	elem->prev->next = elem->next;
	elem->next->prev = elem->prev;
	return elem->next;
}

/* LIST에서 앞의 요소를 제거하고 반환합니다.
   제거하기 전에 LIST이 비어 있으면 정의되지 않은 동작입니다. */
struct list_elem *
list_pop_front (struct list *list) {
	struct list_elem *front = list_front (list);
	list_remove (front);
	return front;
}

/* LIST에서 뒤쪽 요소를 제거하고 반환합니다.
   제거하기 전에 LIST이 비어 있으면 정의되지 않은 동작입니다. */
struct list_elem *
list_pop_back (struct list *list) {
	struct list_elem *back = list_back (list);
	list_remove (back);
	return back;
}

/* LIST 의 앞 요소를 반환합니다.
   LIST이 비어 있으면 정의되지 않은 동작입니다. */
struct list_elem *
list_front (struct list *list) {
	ASSERT (!list_empty (list));
	return list->head.next;
}

/* LIST 의 뒤쪽 요소를 반환합니다.
   LIST이 비어 있으면 정의되지 않은 동작입니다. */
struct list_elem *
list_back (struct list *list) {
	ASSERT (!list_empty (list));
	return list->tail.prev;
}

/* LIST의 요소 수를 반환합니다.
   요소 수에서 O(n)으로 실행됩니다. */
size_t
list_size (struct list *list) {
	struct list_elem *e;
	size_t cnt = 0;

	for (e = list_begin (list); e != list_end (list); e = list_next (e))
		cnt++;
	return cnt;
}

/* LIST이 비어 있으면 true를 반환하고, 그렇지 않으면 false를 반환합니다. */
bool
list_empty (struct list *list) {
	return list_begin (list) == list_end (list);
}

/* A와 B가 가리키는 `struct list_elem *'을 교환합니다. */
static void
swap (struct list_elem **a, struct list_elem **b) {
	struct list_elem *t = *a;
	*a = *b;
	*b = t;
}

/* LIST의 순서를 반대로 바꿉니다. */
void
list_reverse (struct list *list) {
	if (!list_empty (list)) {
		struct list_elem *e;

		for (e = list_begin (list); e != list_end (list); e = e->prev)
			swap (&e->prev, &e->next);
		swap (&list->head.next, &list->tail.prev);
		swap (&list->head.next->prev, &list->tail.prev->next);
	}
}

/* 목록 요소 A부터 B까지(제외)인 경우에만 true를 반환합니다.
   주어진 보조 데이터 AUX에 따라 LESS에 따라 순서가 지정됩니다. */
static bool
is_sorted (struct list_elem *a, struct list_elem *b,
		list_less_func *less, void *aux) {
	if (a != b)
		while ((a = list_next (a)) != b)
			if (less (a, list_prev (a), aux))
				return false;
	return true;
}

/* A에서 시작하고 B 다음이 아닌 목록에서 끝나는 실행을 찾습니다.
   LESS에 따라 감소하지 않는 순서의 요소
   주어진 보조 데이터 AUX. (배타적) 끝을 반환합니다.
   달리다.
   A부터 B까지(제외)는 비어 있지 않은 범위를 형성해야 합니다. */
static struct list_elem *
find_end_of_run (struct list_elem *a, struct list_elem *b,
		list_less_func *less, void *aux) {
	ASSERT (a != NULL);
	ASSERT (b != NULL);
	ASSERT (less != NULL);
	ASSERT (a != b);

	do {
		a = list_next (a);
	} while (a != b && !less (a, list_prev (a), aux));
	return a;
}

/* A0부터 A1B0까지(제외)를 A1B0부터 B1까지 병합합니다.
   (제외) B1으로 끝나는 결합된 범위를 형성합니다.
   (독점적인). 두 입력 범위 모두 비어 있지 않아야 하며 다음으로 정렬되어야 합니다.
   주어진 보조 데이터에 따라 LESS에 따라 감소하지 않는 순서
   AUX. 출력 범위도 같은 방식으로 정렬됩니다. */
static void
inplace_merge (struct list_elem *a0, struct list_elem *a1b0,
		struct list_elem *b1,
		list_less_func *less, void *aux) {
	ASSERT (a0 != NULL);
	ASSERT (a1b0 != NULL);
	ASSERT (b1 != NULL);
	ASSERT (less != NULL);
	ASSERT (is_sorted (a0, a1b0, less, aux));
	ASSERT (is_sorted (a1b0, b1, less, aux));

	while (a0 != a1b0 && a1b0 != b1)
		if (!less (a1b0, a0, aux))
			a0 = list_next (a0);
		else {
			a1b0 = list_next (a1b0);
			list_splice (a0, list_prev (a1b0), a1b0);
		}
}

/* LIST을(를) 사용하여 주어진 보조 데이터 AUX에 따라 LESS에 따라 정렬합니다.
   O(n lg n) 시간에 실행되는 자연적인 반복 병합 정렬
   LIST 의 요소 수에 O(1) 공백이 있습니다. */
void
list_sort (struct list *list, list_less_func *less, void *aux) {
	size_t output_run_cnt;        /* 현재 패스의 실행 출력 수입니다. */

	ASSERT (list != NULL);
	ASSERT (less != NULL);

	/* 목록을 반복적으로 전달하여 인접한 실행을 병합합니다.
	   단 하나의 실행만 남을 때까지 감소하지 않는 요소. */
	do {
		struct list_elem *a0;     /* 첫 실행 시작. */
		struct list_elem *a1b0;   /* 첫 번째 실행이 끝나고 두 번째 실행이 시작됩니다. */
		struct list_elem *b1;     /* 두 번째 실행을 종료합니다. */

		output_run_cnt = 0;
		for (a0 = list_begin (list); a0 != list_end (list); a0 = b1) {
			/* 각 반복은 하나의 출력 실행을 생성합니다. */
			output_run_cnt++;

			/* 감소하지 않는 요소로 구성된 인접한 두 개의 연속을 찾습니다.
			   A0... A1B0 및 A1B0... B1. */
			a1b0 = find_end_of_run (a0, list_end (list), less, aux);
			if (a1b0 == list_end (list))
				break;
			b1 = find_end_of_run (a1b0, list_end (list), less, aux);

			/* 실행을 병합합니다. */
			inplace_merge (a0, a1b0, b1, less, aux);
		}
	}
	while (output_run_cnt > 1);

	ASSERT (is_sorted (list_begin (list), list_end (list), less, aux));
}

/* LIST의 적절한 위치에 ELEM을 삽입합니다.
   주어진 보조 데이터 AUX에 따라 LESS에 따라 정렬됩니다.
   LIST 의 요소 수에서 O(n) 평균 사례로 실행됩니다. */
void
list_insert_ordered (struct list *list, struct list_elem *elem,
		list_less_func *less, void *aux) {
	struct list_elem *e;

	ASSERT (list != NULL);
	ASSERT (elem != NULL);
	ASSERT (less != NULL);

	for (e = list_begin (list); e != list_end (list); e = list_next (e))
		if (less (elem, e, aux))
			break;
	return list_insert (e, elem);
}

/* LIST을 반복하고 각 항목의 첫 번째 항목을 제외한 모든 항목을 제거합니다.
   LESS에 따라 동일한 인접 요소 집합
   주어진 보조 데이터 AUX. DUPLICATES이 null이 아닌 경우
   LIST의 요소는 DUPLICATES에 추가됩니다. */
void
list_unique (struct list *list, struct list *duplicates,
		list_less_func *less, void *aux) {
	struct list_elem *elem, *next;

	ASSERT (list != NULL);
	ASSERT (less != NULL);
	if (list_empty (list))
		return;

	elem = list_begin (list);
	while ((next = list_next (elem)) != list_end (list))
		if (!less (elem, next, aux) && !less (next, elem, aux)) {
			list_remove (next);
			if (duplicates != NULL)
				list_push_back (duplicates, next);
		} else
			elem = next;
}

/* LIST에서 가장 큰 값을 갖는 요소를 반환합니다.
   주어진 보조 데이터 AUX 에 LESS. 하나 이상인 경우
   최대값은 목록의 앞부분에 나타나는 것을 반환합니다. 만약에
   목록이 비어 있으면 꼬리를 반환합니다. */
struct list_elem *
list_max (struct list *list, list_less_func *less, void *aux) {
	struct list_elem *max = list_begin (list);
	if (max != list_end (list)) {
		struct list_elem *e;

		for (e = list_next (max); e != list_end (list); e = list_next (e))
			if (less (max, e, aux))
				max = e;
	}
	return max;
}

/* LIST에 있는 요소 중 가장 작은 값을 반환합니다.
   주어진 보조 데이터 AUX 에 LESS. 하나 이상인 경우
   최소, 목록의 앞부분에 나타나는 항목을 반환합니다. 만약에
   목록이 비어 있으면 꼬리를 반환합니다. */
struct list_elem *
list_min (struct list *list, list_less_func *less, void *aux) {
	struct list_elem *min = list_begin (list);
	if (min != list_end (list)) {
		struct list_elem *e;

		for (e = list_next (min); e != list_end (list); e = list_next (e))
			if (less (e, min, aux))
				min = e;
	}
	return min;
}
