#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "threads/mmu.h"
#include "lib/kernel/stdio.h"
#include "filesys/filesys.h"


#define NO_RETURN_VAL (-1)

struct syscall_entry {
	/* system call number */
	uint64_t syscall_num;

	/* return value (optional, default: NO_RETURN_VAL) */
	/* 반환값이 필요한 경우 handle_{syscall_name} 함수에서
	   설정함 */
	uint64_t return_value;

	/* arguments */
	/* Linux x86-64 system call ABI에선 인자를 6개로 제한함 */
	uint64_t args[6];
};

void syscall_entry (void);
void syscall_handler (struct intr_frame *);
static void init_syscall_entry (struct intr_frame *, struct syscall_entry *);
static void dispatch_syscall (struct intr_frame *, struct syscall_entry *);
static void handle_halt (struct intr_frame *, struct syscall_entry *);
static void handle_exit (struct intr_frame *, struct syscall_entry *);
static void handle_fork (struct intr_frame *, struct syscall_entry *);
static void handle_exec (struct intr_frame *, struct syscall_entry *);
static void handle_wait (struct intr_frame *, struct syscall_entry *);
static void handle_create (struct intr_frame *, struct syscall_entry *);
static void handle_remove (struct intr_frame *, struct syscall_entry *);
static void handle_open (struct intr_frame *, struct syscall_entry *);
static void handle_filesize (struct intr_frame *, struct syscall_entry *);
static void handle_read (struct intr_frame *, struct syscall_entry *);
static void handle_write (struct intr_frame *, struct syscall_entry *);
static void handle_seek (struct intr_frame *, struct syscall_entry *);
static void handle_tell (struct intr_frame *, struct syscall_entry *);
static void handle_close (struct intr_frame *, struct syscall_entry *);
static void *get_next_page_if_valid (void *);
static bool is_valid_user_buffer (void *, size_t);
static bool is_valid_user_string (char *);

/* 시스템 콜.
 *
 * 이전에는 시스템 콜 서비스가 인터럽트 핸들러를 통해 처리되었다
 * (예: Linux의 int 0x80). 하지만 x86-64에서는 제조사가 시스템 콜을
 * 요청하는 효율적인 경로인 `syscall` 명령어를 제공한다.
 *
 * syscall 명령어는 모델별 레지스터(MSR)에서 값을 읽어 동작한다.
 * 자세한 내용은 매뉴얼을 참고하라. */

#define MSR_STAR 0xc0000081         /* 세그먼트 셀렉터 MSR. */
#define MSR_LSTAR 0xc0000082        /* 롱 모드 SYSCALL 대상. */
#define MSR_SYSCALL_MASK 0xc0000084 /* eflags용 마스크. */

void
syscall_init (void) {
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48  |
			((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t) syscall_entry);

	/* syscall_entry가 사용자 영역 스택을 커널 모드 스택으로 교체하기 전까지
	 * 인터럽트 서비스 루틴이 어떤 인터럽트도 처리하면 안 된다.
	 * 따라서 FLAG_FL을 마스킹한다. */
	write_msr(MSR_SYSCALL_MASK,
			FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
}

/* 주 시스템 콜 인터페이스. */
void
syscall_handler (struct intr_frame *f) {
	struct syscall_entry entry;

	init_syscall_entry (f, &entry);
	dispatch_syscall (f, &entry);

	if (entry.return_value != NO_RETURN_VAL) {
		f->R.rax = entry.return_value;
	}
}

static void
init_syscall_entry (struct intr_frame *f, struct syscall_entry *entry) {
	entry->syscall_num = f->R.rax;
	entry->return_value = NO_RETURN_VAL;
	entry->args[0] = f->R.rdi;
	entry->args[1] = f->R.rsi;
	entry->args[2] = f->R.rdx;
	entry->args[3] = f->R.r10;
	entry->args[4] = f->R.r8;
	entry->args[5] = f->R.r9;
}

static void
dispatch_syscall (struct intr_frame *f, struct syscall_entry *entry) {
	switch (entry->syscall_num) {
		case SYS_HALT:
			handle_halt (f, entry);
			break;
		case SYS_EXIT:
			handle_exit (f, entry);
			break;
		case SYS_FORK:
			handle_fork (f, entry);
			break;
		case SYS_EXEC:
			handle_exec (f, entry);
			break;
		case SYS_WAIT:
			handle_wait (f, entry);
			break;
		case SYS_CREATE:
			handle_create (f, entry);
			break;
		case SYS_REMOVE:
			handle_remove (f, entry);
			break;
		case SYS_OPEN:
			handle_open (f, entry);
			break;
		case SYS_FILESIZE:
			handle_filesize (f, entry);
			break;
		case SYS_READ:
			handle_read (f, entry);
			break;
		case SYS_WRITE:
			handle_write (f, entry);
			break;
		case SYS_SEEK:
			handle_seek (f, entry);
			break;
		case SYS_TELL:
			handle_tell (f, entry);
			break;
		case SYS_CLOSE:
			handle_close (f, entry);
			break;
		default:
			ASSERT (false); /* 현재 처리할 수 없는 syscall */
	}
}

/* TODO: 구현하면 UNUSED, ASSERT 빼기 */
static void
handle_halt (struct intr_frame *f UNUSED, struct syscall_entry *entry UNUSED) {
	barrier ();
	ASSERT (false); /* 현재 처리할 수 없는 syscall */
}

static void
handle_exit (struct intr_frame *f UNUSED, struct syscall_entry *entry) {
	int status = entry->args[0];

	printf ("%s: exit(%d)\n", thread_current ()->name, status);
	thread_exit ();
}

/* TODO: 구현하면 UNUSED, ASSERT 빼기 */
static void
handle_fork (struct intr_frame *f UNUSED, struct syscall_entry *entry UNUSED) {
	barrier ();
	ASSERT (false); /* 현재 처리할 수 없는 syscall */
}

/* TODO: 구현하면 UNUSED, ASSERT 빼기 */
static void
handle_exec (struct intr_frame *f UNUSED, struct syscall_entry *entry UNUSED) {
	barrier ();
	ASSERT (false); /* 현재 처리할 수 없는 syscall */
}

/* TODO: 구현하면 UNUSED, ASSERT 빼기 */
static void
handle_wait (struct intr_frame *f UNUSED, struct syscall_entry *entry UNUSED) {
	barrier ();
	ASSERT (false); /* 현재 처리할 수 없는 syscall */
}

/* TODO: 구현하면 UNUSED, ASSERT 빼기 */
static void
handle_create (struct intr_frame *f UNUSED,
		struct syscall_entry *entry) {
	const char *file = (const char *) entry->args[0];
	off_t initial_size = entry->args[1];

	if (file == NULL) {
		entry->return_value = false;
		return;
	}

	entry->return_value = filesys_create (file, initial_size);
}

/* TODO: 구현하면 UNUSED, ASSERT 빼기 */
static void
handle_remove (struct intr_frame *f UNUSED,
		struct syscall_entry *entry UNUSED) {
	barrier ();
	ASSERT (false); /* 현재 처리할 수 없는 syscall */
}

/* TODO: 구현하면 UNUSED, ASSERT 빼기 */
static void
handle_open (struct intr_frame *f UNUSED, struct syscall_entry *entry UNUSED) {
	barrier ();
	ASSERT (false); /* 현재 처리할 수 없는 syscall */
}

/* TODO: 구현하면 UNUSED, ASSERT 빼기 */
static void
handle_filesize (struct intr_frame *f UNUSED,
		struct syscall_entry *entry UNUSED) {
	barrier ();
	ASSERT (false); /* 현재 처리할 수 없는 syscall */
}

/* TODO: 구현하면 UNUSED, ASSERT 빼기 */
static void
handle_read (struct intr_frame *f UNUSED, struct syscall_entry *entry UNUSED) {
	barrier ();
	ASSERT (false); /* 현재 처리할 수 없는 syscall */
}

/* TODO: 구현하면 UNUSED, ASSERT 빼기 */
static void
handle_write (struct intr_frame *f UNUSED, struct syscall_entry *entry) {
	int fd = entry->args[0];
	const void *buffer = (const void *) entry->args[1];
	size_t size = entry->args[2];

	if (fd == 1) {
		putbuf (buffer, size);
		entry->return_value = size;
		return;
	}

	entry->return_value = -1;
}


/* TODO: 구현하면 UNUSED, ASSERT 빼기 */
static void
handle_seek (struct intr_frame *f UNUSED, struct syscall_entry *entry UNUSED) {
	barrier ();
	ASSERT (false); /* 현재 처리할 수 없는 syscall */
}

/* TODO: 구현하면 UNUSED, ASSERT 빼기 */
static void
handle_tell (struct intr_frame *f UNUSED, struct syscall_entry *entry UNUSED) {
	barrier ();
	ASSERT (false); /* 현재 처리할 수 없는 syscall */
}

/* TODO: 구현하면 UNUSED, ASSERT 빼기 */
static void
handle_close (struct intr_frame *f UNUSED, struct syscall_entry *entry UNUSED) {
	barrier ();
	ASSERT (false); /* 현재 처리할 수 없는 syscall */
}

static bool
is_valid_user_buffer (void *buf, size_t size) {
	void *p = buf;
	/* 산술 연산 시에는 uintptr_t 변환이 안전해보임. */
	void *buf_end = (void *) ((uintptr_t) p + size);

	if (size <= 0) {
		return get_next_page_if_valid (p) != NULL;
	}

	while (p < buf_end) {
		/* 유효하면 다음 페이지, 아니면 NULL 반환. */
		p = get_next_page_if_valid (p);
		if (p == NULL) {
			return false;
		}
	}
	return true;
}

static bool
is_valid_user_string (char *str) {
	char *p = str;

	while (true) {
		/* 유효하면 다음 페이지, 아니면 NULL 반환. */
		char *next_p = get_next_page_if_valid (p);
		char *page_end = pg_round_up (p);
		if (next_p == NULL) {
			return false;
		}

		/* 현재 페이지 내부를 순회하며 문자열의 끝이 있는지 검사. */
		while (p < page_end) {
			if (*p == '\0') {
				return true;
			}
			p++;
		}

		/* 다음 페이지 검사 진행. */
		p = next_p;
	}
}

/* 해당 ptr의 페이지가 유효한지 확인하고,
   유효 시 다음 페이지 주소를 반환, 그렇지 않으면 NULL을 반환한다.

   구현의 편의를 위해 분리한 함수라서, is_valid_user_* 에서만 사용 추천
   리턴하는 다음 페이지 주소가 유효하지 않을 수 있음을 주의하기
   */
static void *
get_next_page_if_valid (void *ptr) {
	if (ptr == NULL) {
		return NULL;
	}

	/* 커널 영역 위인가? */
	if (!is_user_vaddr (ptr)) {
		return NULL;
	}
	/* thread가 가지는 유저 가상 주소(pml4 필드)가 unmapped 상태인가? */
	if (pml4_get_page (thread_current ()->pml4, ptr) == NULL) {
		return NULL;
	}
	return pg_round_up (ptr);
}
