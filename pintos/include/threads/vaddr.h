#ifndef THREADS_VADDR_H
#define THREADS_VADDR_H

#include <debug.h>
#include <stdint.h>
#include <stdbool.h>

#include "threads/loader.h"

/* 가상 주소 작업을 위한 함수 및 매크로입니다.
 *
 * x86용 함수 및 매크로는 pte.h를 참조하세요.
 * 하드웨어 페이지 테이블. */

#define BITMASK(SHIFT, CNT) (((1ul << (CNT)) - 1) << (SHIFT))

/* 페이지 오프셋(비트 0:12). */
#define PGSHIFT 0                          /* 첫 번째 오프셋 비트의 인덱스입니다. */
#define PGBITS  12                         /* 오프셋 비트 수입니다. */
#define PGSIZE  (1 << PGBITS)              /* 페이지의 바이트입니다. */
#define PGMASK  BITMASK(PGSHIFT, PGBITS)   /* 페이지 오프셋 비트(0:12). */

/* 페이지 내 오프셋. */
#define pg_ofs(va) ((uint64_t) (va) & PGMASK)

#define pg_no(va) ((uint64_t) (va) >> PGBITS)

/* page-aligned가 아니면 다음 페이지 경계로 올림합니다. */
#define pg_round_up(va) ((void *) (((uint64_t) (va) + PGSIZE - 1) & ~PGMASK))

/* 이전 페이지 경계로 내림합니다. */
#define pg_round_down(va) (void *) ((uint64_t) (va) & ~PGMASK)

/* 항상 다음 페이지 경계로 이동합니다. */
#define pg_next(va) ((void *) (((uint64_t) pg_round_down(va)) + PGSIZE))

/* 커널 가상 주소 시작 */
#define KERN_BASE LOADER_KERN_BASE

/* 사용자 스택 시작 */
#define USER_STACK 0x47480000

/* VADDR이 사용자 가상 주소인 경우 true를 반환합니다. */
#define is_user_vaddr(vaddr) (!is_kernel_vaddr((vaddr)))

/* VADDR이 커널 가상 주소인 경우 true를 반환합니다. */
#define is_kernel_vaddr(vaddr) ((uint64_t)(vaddr) >= KERN_BASE)

// FIXME: 검사 추가
/* 물리적 주소가 PADDR인 커널 가상 주소를 반환합니다.
 *  매핑됩니다. */
#define ptov(paddr) ((void *) (((uint64_t) paddr) + KERN_BASE))

/* 커널 가상 주소 VADDR이 있는 물리적 주소를 반환합니다.
 * 매핑됩니다. */
#define vtop(vaddr) \
({ \
	ASSERT(is_kernel_vaddr(vaddr)); \
	((uint64_t) (vaddr) - (uint64_t) KERN_BASE);\
})

#endif /* threads/vaddr.h */
