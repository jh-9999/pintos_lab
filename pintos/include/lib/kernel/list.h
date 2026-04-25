#ifndef __LIB_KERNEL_LIST_H
#define __LIB_KERNEL_LIST_H

/* 이중 연결 리스트.
 *
 * 이중 연결 리스트의 구현에는 다음이 필요하지 않습니다.
 * 동적으로 할당된 메모리를 사용합니다. 대신 각 구조는
 * 잠재적으로 리스트 요소가 될 각 구조체는 `struct list_elem'
 * 멤버를 포함해야 합니다. 모든 리스트 함수는 이 `struct list_elem'에
 * 대해 동작합니다. list_entry 매크로는 `struct list_elem'에서 이를
 * 포함하는 구조체 객체로 다시 변환할 수 있게 해 줍니다.

 * 예를 들어 `struct foo'의 리스트가 필요하다고 가정합니다.
 * `struct foo'는 다음처럼 `struct list_elem' 멤버를 포함해야 합니다.

 * struct foo {
 *   struct list_elem elem;
 *   int bar;
 *   ...other members...
 * };

 * 그런 다음 `struct foo' 목록을 선언하고 초기화할 수 있습니다.
 * 이렇게:

 * struct list foo_list;

 * list_init(&foo_list);

 * 반복은 다음을 수행해야 하는 일반적인 상황입니다.
 * struct list_elem에서 다시 둘러싸는 것으로 변환
 * 구조. foo_list를 사용한 예는 다음과 같습니다.

 * struct list_elem *e;

 * for (e = list_begin (&foo_list); e != list_end (&foo_list);
 * e = list_next (e)) {
 *   struct foo *f = list_entry (e, struct foo, elem);
 *   ...f로 뭔가를 해보세요...
 * }

 * 목록 전체에서 실제 목록 사용 예를 찾을 수 있습니다.
 * 원천; 예를 들어, malloc.c, palloc.c 및 thread.c가 있습니다.
 * 스레드 디렉토리는 모두 목록을 사용합니다.

 * 이 목록의 인터페이스는 list<> 템플릿에서 영감을 받았습니다.
 * C++ STL 에서. list<>에 익숙하다면 다음을 수행해야 합니다.
 * 이것을 사용하기 쉽다는 것을 알았습니다. 그러나 강조해야 할 점은
 * 이 목록은 유형 검사를 *아무* 수행하지 않으며 다른 많은 작업을 수행할 수 없습니다.
 * 정확성 검사. 망치면 물릴 것입니다.

 * 목록 용어집:

 * - "front": 목록의 첫 번째 요소입니다. 정의되지 않음
 * 빈 목록. list_front()에 의해 반환되었습니다.

 * - "뒤로": 목록의 마지막 요소입니다. 빈 공간에 정의되지 않음
 * 목록. list_back()에 의해 반환되었습니다.

 * - "tail": 비유적으로 마지막 요소 바로 뒤의 요소
 * 목록의 요소입니다. 빈 목록에서도 잘 정의됩니다.
 * list_end()에 의해 반환되었습니다. 최종 파수꾼으로 사용됨
 * 앞에서 뒤로 반복.

 * - "시작": 비어 있지 않은 목록에서 맨 앞. 비어있는
 * 목록, 꼬리. list_begin()에 의해 반환되었습니다. 다음과 같이 사용됨
 * 앞에서 뒤로 반복의 시작점입니다.

 * - "head": 비유적으로 첫 번째 바로 앞의 요소
 * 목록의 요소입니다. 빈 목록에서도 잘 정의됩니다.
 * list_rend()에 의해 반환되었습니다. 최종 파수꾼으로 사용됨
 * 뒤에서 앞으로 반복.

 * - "역방향 시작": 비어 있지 않은 목록에서 맨 뒤입니다. 에
 * 빈 목록, 머리. list_rbegin()에 의해 반환되었습니다. 다음과 같이 사용됨
 * 뒤에서 앞으로 반복하는 시작점입니다.
 *
 * - "내부 요소": 머리 또는 머리가 아닌 요소
 * tail, 즉 실제 목록 요소입니다. 빈 목록은
 * 내부 요소가 없습니다.*/

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* 목록 요소. */
struct list_elem {
	struct list_elem *prev;     /* 이전 목록 요소입니다. */
	struct list_elem *next;     /* 다음 목록 요소입니다. */
};

/* 목록. */
struct list {
	struct list_elem head;      /* 목록 머리. */
	struct list_elem tail;      /* 꼬리를 나열하십시오. */
};

/* 목록 요소 LIST_ELEM에 대한 포인터를 포인터로 변환합니다.
   LIST_ELEM이 내부에 포함된 구조입니다. 공급하다
   외부 구조의 이름 STRUCT 및 멤버 이름 MEMBER
   목록 요소의 상단의 큰댓글을 참고하세요
   예시 파일입니다. */
#define list_entry(LIST_ELEM, STRUCT, MEMBER)           \
	((STRUCT *) ((uint8_t *) &(LIST_ELEM)->next     \
		- offsetof (STRUCT, MEMBER.next)))

void list_init (struct list *);

/* 순회를 나열합니다. */
struct list_elem *list_begin (struct list *);
struct list_elem *list_next (struct list_elem *);
struct list_elem *list_end (struct list *);

struct list_elem *list_rbegin (struct list *);
struct list_elem *list_prev (struct list_elem *);
struct list_elem *list_rend (struct list *);

struct list_elem *list_head (struct list *);
struct list_elem *list_tail (struct list *);

/* 목록 삽입. */
void list_insert (struct list_elem *, struct list_elem *);
void list_splice (struct list_elem *before,
		struct list_elem *first, struct list_elem *last);
void list_push_front (struct list *, struct list_elem *);
void list_push_back (struct list *, struct list_elem *);

/* 목록 제거. */
struct list_elem *list_remove (struct list_elem *);
struct list_elem *list_pop_front (struct list *);
struct list_elem *list_pop_back (struct list *);

/* 요소를 나열합니다. */
struct list_elem *list_front (struct list *);
struct list_elem *list_back (struct list *);

/* 속성을 나열합니다. */
size_t list_size (struct list *);
bool list_empty (struct list *);

/* 여러 가지 잡다한. */
void list_reverse (struct list *);

/* 주어진 두 목록 요소 A와 B의 값을 비교합니다.
   보조 데이터 AUX. A가 B보다 작으면 true를 반환합니다.
   A가 B보다 크거나 같으면 false입니다. */
typedef bool list_less_func (const struct list_elem *a,
                             const struct list_elem *b,
                             void *aux);

/* 순서가 지정된 요소가 있는 목록에 대한 작업입니다. */
void list_sort (struct list *,
                list_less_func *, void *aux);
void list_insert_ordered (struct list *, struct list_elem *,
                          list_less_func *, void *aux);
void list_unique (struct list *, struct list *duplicates,
                  list_less_func *, void *aux);

/* 최대 및 최소 */
struct list_elem *list_max (struct list *, list_less_func *, void *aux);
struct list_elem *list_min (struct list *, list_less_func *, void *aux);

#endif /* lib/kernel/list.h */
