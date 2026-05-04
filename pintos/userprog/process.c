#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/mmu.h"
#include "threads/vaddr.h"
#include "intrinsic.h"
#ifdef VM
#include "vm/vm.h"
#endif

static void process_cleanup (void);
static bool load (const char *file_name, struct intr_frame *if_);
static void initd (void *f_name);
static void __do_fork (void *);

#define MAX_ARGC 128

/* initd와 다른 프로세스를 위한 일반 프로세스 초기화 함수. */
static void
process_init (void) {
	struct thread *current = thread_current ();
}

/* FILE_NAME에서 로드되는 첫 사용자 영역 프로그램인 "initd"를 시작한다.
 * 새 스레드는 process_create_initd()가 반환되기 전에 스케줄될 수 있고,
 * 심지어 종료될 수도 있다. initd의 스레드 id를 반환하며, 스레드를 만들 수
 * 없으면 TID_ERROR를 반환한다. 이 함수는 반드시 한 번만 호출되어야 한다. */
tid_t
process_create_initd (const char *file_name) {
	char *fn_copy;
	char *prog_copy;
	char *prog_name;
	char *save_ptr;
	tid_t tid;

	/* FILE_NAME의 복사본을 만든다.
	 * 그렇지 않으면 호출자와 load() 사이에 경쟁 상태가 생긴다. */
	fn_copy = palloc_get_page (0);
	if (fn_copy == NULL)
		return TID_ERROR;
	strlcpy (fn_copy, file_name, PGSIZE);

	prog_copy = palloc_get_page (0);
	if (prog_copy == NULL) {
		palloc_free_page (fn_copy);
		return TID_ERROR;
	}
	strlcpy (prog_copy, file_name, PGSIZE);
	prog_name = strtok_r (prog_copy, " ", &save_ptr);

	/* FILE_NAME을 실행할 새 스레드를 만든다. */
	tid = thread_create (file_name, PRI_DEFAULT, initd, fn_copy);
	palloc_free_page (prog_copy);
	if (tid == TID_ERROR)
		palloc_free_page (fn_copy);
	return tid;
}

/* 첫 사용자 프로세스를 시작하는 스레드 함수. */
static void
initd (void *f_name) {
#ifdef VM
	supplemental_page_table_init (&thread_current ()->spt);
#endif

	process_init ();

	if (process_exec (f_name) < 0)
		PANIC("Fail to launch initd\n");
	NOT_REACHED ();
}

/* 현재 프로세스를 `name`으로 복제한다. 새 프로세스의 스레드 id를 반환하며,
 * 스레드를 만들 수 없으면 TID_ERROR를 반환한다. */
tid_t
process_fork (const char *name, struct intr_frame *if_ UNUSED) {
	/* 현재 스레드를 새 스레드로 복제한다. */
	return thread_create (name,
			PRI_DEFAULT, __do_fork, thread_current ());
}

#ifndef VM
/* 이 함수를 pml4_for_each에 전달하여 부모의 주소 공간을 복제한다.
 * 이 코드는 프로젝트 2에서만 사용한다. */
static bool
duplicate_pte (uint64_t *pte, void *va, void *aux) {
	struct thread *current = thread_current ();
	struct thread *parent = (struct thread *) aux;
	void *parent_page;
	void *newpage;
	bool writable;

	/* 1. TODO: parent_page가 커널 페이지라면 즉시 반환한다. */

	/* 2. 부모의 페이지 맵 레벨 4에서 VA를 해석한다. */
	parent_page = pml4_get_page (parent->pml4, va);

	/* 3. TODO: 자식을 위한 새 PAL_USER 페이지를 할당하고 결과를
	 *    TODO: NEWPAGE에 설정한다. */

	/* 4. TODO: 부모의 페이지를 새 페이지로 복제하고, 부모 페이지가 쓰기
	 *    TODO: 가능한지 확인한다(결과에 따라 WRITABLE을 설정한다). */

	/* 5. 새 페이지를 자식의 페이지 테이블에 VA 주소와 WRITABLE 권한으로
	 *    추가한다. */
	if (!pml4_set_page (current->pml4, va, newpage, writable)) {
		/* 6. TODO: 페이지 삽입에 실패하면 오류 처리를 수행한다. */
	}
	return true;
}
#endif

/* 부모의 실행 컨텍스트를 복사하는 스레드 함수.
 * 힌트) parent->tf는 프로세스의 사용자 영역 컨텍스트를 보관하지 않는다.
 *       즉, process_fork의 두 번째 인자를 이 함수에 전달해야 한다. */
static void
__do_fork (void *aux) {
	struct intr_frame if_;
	struct thread *parent = (struct thread *) aux;
	struct thread *current = thread_current ();
	/* TODO: 어떤 방식으로든 parent_if를 전달한다. (즉, process_fork()의 if_) */
	struct intr_frame *parent_if;
	bool succ = true;

	/* 1. CPU 컨텍스트를 로컬 스택으로 읽는다. */
	memcpy (&if_, parent_if, sizeof (struct intr_frame));

	/* 2. 페이지 테이블을 복제한다. */
	current->pml4 = pml4_create();
	if (current->pml4 == NULL)
		goto error;

	process_activate (current);
#ifdef VM
	supplemental_page_table_init (&current->spt);
	if (!supplemental_page_table_copy (&current->spt, &parent->spt))
		goto error;
#else
	if (!pml4_for_each (parent->pml4, duplicate_pte, parent))
		goto error;
#endif

	/* TODO: 구현 내용을 여기에 작성한다.
	 * TODO: 힌트) 파일 객체를 복제하려면 include/filesys/file.h의
	 * TODO:       `file_duplicate`를 사용한다. 이 함수가 부모의 리소스를
	 * TODO:       성공적으로 복제하기 전까지 부모는 fork()에서 반환하면
	 * TODO:       안 된다는 점에 유의하라. */

	process_init ();

	/* 마지막으로 새로 생성한 프로세스로 전환한다. */
	if (succ)
		do_iret (&if_);
error:
	thread_exit ();
}

static int
parse_command_line (char *cmdline, char **argv) {
	char delimiter[] = " \t\n";
	char *token;
	char *save_ptr;
	int count = 0;

	token = strtok_r (cmdline, delimiter, &save_ptr);
	if (token == NULL)
		return -1;

	while (token != NULL && count < MAX_ARGC - 1) {
		argv[count] = token;
		count++;
		token = strtok_r (NULL, delimiter, &save_ptr);
	}
	argv[count] = NULL;
	return count;
}

static void
setup_argument_stack (char **argv, int argc, struct intr_frame *if_) {
	int count = argc - 1;
	char *arg_addr[MAX_ARGC];

	while (count >= 0) {
		char *item = argv[count];
		size_t len = strlen (item) + 1;
		if_->rsp -= len;

		memcpy ((void *) if_->rsp, item, len);
		arg_addr[count] = (char *) if_->rsp;
		count--;
	}
	arg_addr[argc] = NULL;

	while (if_->rsp % sizeof (uint64_t) != 0) {
		if_->rsp--;
		*(uint8_t *) if_->rsp = 0;
	}

	count = argc;
	while (count >= 0) {
		if_->rsp -= sizeof (uint64_t);
		memcpy ((void *) if_->rsp, &arg_addr[count], sizeof (arg_addr[count]));
		count--;
	}

	char *start_pt = (char *) if_->rsp;
	if_->R.rdi = (uint64_t) argc;
	if_->R.rsi = (uint64_t) start_pt;

	while (if_->rsp % sizeof (uint64_t) != 0) {
		if_->rsp--;
		*(uint8_t *) if_->rsp = 0;
	}

	uint64_t zero = 0;
	if_->rsp -= sizeof (uint64_t);
	memcpy ((void *) if_->rsp, &zero, sizeof (zero));
}

/* 현재 실행 컨텍스트를 f_name으로 전환한다.
 * 실패하면 -1을 반환한다. */
int
process_exec (void *f_name) {
	ASSERT (f_name != NULL);

	char *file_name = f_name;
	bool success;

	/* thread 구조체 안의 intr_frame은 사용할 수 없다.
	 * 현재 스레드가 다시 스케줄될 때 실행 정보를 그 멤버에 저장하기 때문이다. */
	struct intr_frame _if;
	memset (&_if, 0, sizeof (_if));
	_if.ds = _if.es = _if.ss = SEL_UDSEG;
	_if.cs = SEL_UCSEG;
	_if.eflags = FLAG_IF | FLAG_MBS;

	/* 먼저 현재 컨텍스트를 제거한다. */
	process_cleanup ();

	char *argv[MAX_ARGC];
	int argc = parse_command_line (file_name, argv);
	if (argc == -1) {
		palloc_free_page (file_name);
		return -1;
	}

	/* 그런 다음 바이너리를 로드한다. */
	success = load (argv[0], &_if);

	/* load에 실패했으면 종료한다. */
	if (!success) {
		palloc_free_page (file_name);
		return -1;
	}

	setup_argument_stack (argv, argc, &_if);

	palloc_free_page (file_name);
	/* 전환된 프로세스를 시작한다. */
	do_iret (&_if);
	NOT_REACHED ();
}


/* 스레드 TID가 종료될 때까지 기다린 뒤 종료 상태를 반환한다. 커널에 의해
 * 종료되었다면(즉, 예외 때문에 종료되었다면) -1을 반환한다. TID가
 * 유효하지 않거나, 호출 프로세스의 자식이 아니거나, 주어진 TID에 대해
 * process_wait()가 이미 성공적으로 호출된 적이 있다면 기다리지 않고 즉시
 * -1을 반환한다.
 *
 * 이 함수는 문제 2-2에서 구현된다. 지금은 아무 일도 하지 않는다. */
int
process_wait (tid_t child_tid UNUSED) {
	/* XXX: 힌트) process_wait(initd)에서 Pintos가 종료된다. process_wait를
	 * XXX:       구현하기 전에는 여기에 무한 루프를 추가하는 것을 권장한다. */
	while (1) {
	}
	return -1;
}

/* 프로세스를 종료한다. 이 함수는 thread_exit()에서 호출된다. */
void
process_exit (void) {
	struct thread *curr = thread_current ();
	/* TODO: 구현 내용을 여기에 작성한다.
	 * TODO: 프로세스 종료 메시지를 구현한다
	 * TODO: (project2/process_termination.html 참고).
	 * TODO: 여기에서 프로세스 리소스 정리를 구현하는 것을 권장한다. */

	process_cleanup ();
}

/* 현재 프로세스의 리소스를 해제한다. */
static void
process_cleanup (void) {
	struct thread *curr = thread_current ();

#ifdef VM
	supplemental_page_table_kill (&curr->spt);
#endif

	uint64_t *pml4;
	/* 현재 프로세스의 페이지 디렉터리를 파괴하고 커널 전용 페이지
	 * 디렉터리로 다시 전환한다. */
	pml4 = curr->pml4;
	if (pml4 != NULL) {
		/* 여기서는 순서가 매우 중요하다. 페이지 디렉터리를 전환하기 전에
		 * cur->pagedir를 NULL로 설정해야 타이머 인터럽트가 프로세스 페이지
		 * 디렉터리로 다시 전환하지 못한다. 또한 프로세스의 페이지 디렉터리를
		 * 파괴하기 전에 기본 페이지 디렉터리를 활성화해야 한다. 그렇지 않으면
		 * 이미 해제되고 지워진 페이지 디렉터리가 활성 상태가 될 수 있다. */
		curr->pml4 = NULL;
		pml4_activate (NULL);
		pml4_destroy (pml4);
	}
}

/* 다음 스레드에서 사용자 코드를 실행할 수 있도록 CPU를 설정한다.
 * 이 함수는 문맥 전환 때마다 호출된다. */
void
process_activate (struct thread *next) {
	/* 스레드의 페이지 테이블을 활성화한다. */
	pml4_activate (next->pml4);

	/* 인터럽트 처리에 사용할 스레드의 커널 스택을 설정한다. */
	tss_update (next);
}

/* 우리는 ELF 바이너리를 로드한다. 다음 정의들은 [ELF1]의 ELF 명세에서
 * 거의 그대로 가져온 것이다. */

/* ELF 타입. [ELF1] 1-2를 참고하라. */
#define EI_NIDENT 16

#define PT_NULL    0            /* 무시. */
#define PT_LOAD    1            /* 로드 가능한 세그먼트. */
#define PT_DYNAMIC 2            /* 동적 링킹 정보. */
#define PT_INTERP  3            /* 동적 로더 이름. */
#define PT_NOTE    4            /* 보조 정보. */
#define PT_SHLIB   5            /* 예약됨. */
#define PT_PHDR    6            /* 프로그램 헤더 테이블. */
#define PT_STACK   0x6474e551   /* 스택 세그먼트. */

#define PF_X 1          /* 실행 가능. */
#define PF_W 2          /* 쓰기 가능. */
#define PF_R 4          /* 읽기 가능. */

/* 실행 파일 헤더. [ELF1] 1-4부터 1-8까지를 참고하라.
 * ELF 바이너리의 맨 앞에 위치한다. */
struct ELF64_hdr {
	unsigned char e_ident[EI_NIDENT];
	uint16_t e_type;
	uint16_t e_machine;
	uint32_t e_version;
	uint64_t e_entry;
	uint64_t e_phoff;
	uint64_t e_shoff;
	uint32_t e_flags;
	uint16_t e_ehsize;
	uint16_t e_phentsize;
	uint16_t e_phnum;
	uint16_t e_shentsize;
	uint16_t e_shnum;
	uint16_t e_shstrndx;
};

struct ELF64_PHDR {
	uint32_t p_type;
	uint32_t p_flags;
	uint64_t p_offset;
	uint64_t p_vaddr;
	uint64_t p_paddr;
	uint64_t p_filesz;
	uint64_t p_memsz;
	uint64_t p_align;
};

/* 약어. */
#define ELF ELF64_hdr
#define Phdr ELF64_PHDR

static bool setup_stack (struct intr_frame *if_);
static bool validate_segment (const struct Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
		uint32_t read_bytes, uint32_t zero_bytes,
		bool writable);

/* FILE_NAME의 ELF 실행 파일을 현재 스레드로 로드한다.
 * 실행 파일의 진입점을 *RIP에 저장하고 초기 스택 포인터를 *RSP에 저장한다.
 * 성공하면 true, 그렇지 않으면 false를 반환한다. */
static bool
load (const char *file_name, struct intr_frame *if_) {
	struct thread *t = thread_current ();
	struct ELF ehdr;
	struct file *file = NULL;
	off_t file_ofs;
	bool success = false;
	int i;

	/* 페이지 디렉터리를 할당하고 활성화한다. */
	t->pml4 = pml4_create ();
	if (t->pml4 == NULL)
		goto done;
	process_activate (thread_current ());

	/* 실행 파일을 연다. */
	file = filesys_open (file_name);
	if (file == NULL) {
		printf ("load: %s: open failed\n", file_name);
		goto done;
	}

	/* 실행 파일 헤더를 읽고 검증한다. */
	if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
			|| memcmp (ehdr.e_ident, "\177ELF\2\1\1", 7)
			|| ehdr.e_type != 2
			|| ehdr.e_machine != 0x3E // amd64
			|| ehdr.e_version != 1
			|| ehdr.e_phentsize != sizeof (struct Phdr)
			|| ehdr.e_phnum > 1024) {
		printf ("load: %s: error loading executable\n", file_name);
		goto done;
	}

	/* 프로그램 헤더를 읽는다. */
	file_ofs = ehdr.e_phoff;
	for (i = 0; i < ehdr.e_phnum; i++) {
		struct Phdr phdr;

		if (file_ofs < 0 || file_ofs > file_length (file))
			goto done;
		file_seek (file, file_ofs);

		if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
			goto done;
		file_ofs += sizeof phdr;
		switch (phdr.p_type) {
			case PT_NULL:
			case PT_NOTE:
			case PT_PHDR:
			case PT_STACK:
			default:
				/* 이 세그먼트는 무시한다. */
				break;
			case PT_DYNAMIC:
			case PT_INTERP:
			case PT_SHLIB:
				goto done;
			case PT_LOAD:
				if (validate_segment (&phdr, file)) {
					bool writable = (phdr.p_flags & PF_W) != 0;
					uint64_t file_page = phdr.p_offset & ~PGMASK;
					uint64_t mem_page = phdr.p_vaddr & ~PGMASK;
					uint64_t page_offset = phdr.p_vaddr & PGMASK;
					uint32_t read_bytes, zero_bytes;
					if (phdr.p_filesz > 0) {
						/* 일반 세그먼트.
						 * 앞부분은 디스크에서 읽고 나머지는 0으로 채운다. */
						read_bytes = page_offset + phdr.p_filesz;
						zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
								- read_bytes);
					} else {
						/* 전체가 0이다.
						 * 디스크에서 아무것도 읽지 않는다. */
						read_bytes = 0;
						zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
					}
					if (!load_segment (file, file_page, (void *) mem_page,
								read_bytes, zero_bytes, writable))
						goto done;
				}
				else
					goto done;
				break;
		}
	}

	/* 스택을 설정한다. */
	if (!setup_stack (if_))
		goto done;

	/* 시작 주소. */
	if_->rip = ehdr.e_entry;

	/* TODO: 구현 내용을 여기에 작성한다.
	 * TODO: 인자 전달을 구현한다(project2/argument_passing.html 참고). */

	success = true;

done:
	/* load 성공 여부와 관계없이 여기로 도착한다. */
	file_close (file);
	return success;
}


/* PHDR이 FILE 안의 유효하고 로드 가능한 세그먼트를 설명하는지 확인한다.
 * 그렇다면 true, 아니면 false를 반환한다. */
static bool
validate_segment (const struct Phdr *phdr, struct file *file) {
	/* p_offset과 p_vaddr은 같은 페이지 오프셋을 가져야 한다. */
	if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK))
		return false;

	/* p_offset은 FILE 내부를 가리켜야 한다. */
	if (phdr->p_offset > (uint64_t) file_length (file))
		return false;

	/* p_memsz는 적어도 p_filesz만큼 커야 한다. */
	if (phdr->p_memsz < phdr->p_filesz)
		return false;

	/* 세그먼트는 비어 있으면 안 된다. */
	if (phdr->p_memsz == 0)
		return false;

	/* 가상 메모리 영역의 시작과 끝은 모두 사용자 주소 공간 범위 안에
	   있어야 한다. */
	if (!is_user_vaddr ((void *) phdr->p_vaddr))
		return false;
	if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
		return false;

	/* 이 영역은 커널 가상 주소 공간을 가로질러 주소가 되감길 수 없다. */
	if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
		return false;

	/* 페이지 0 매핑을 금지한다.
	   페이지 0을 매핑하는 것 자체도 좋지 않지만, 이를 허용하면 시스템 콜에
	   null 포인터를 넘긴 사용자 코드가 memcpy() 등의 null 포인터 assertion을
	   통해 커널 패닉을 일으킬 가능성이 크다. */
	if (phdr->p_vaddr < PGSIZE)
		return false;

	/* 문제없다. */
	return true;
}

#ifndef VM
/* 이 블록의 코드는 프로젝트 2 동안에만 사용된다.
 * 프로젝트 2 전체에서 이 함수를 구현하고 싶다면 #ifndef 매크로 밖에
 * 구현하라. */

/* load() 보조 함수. */
static bool install_page (void *upage, void *kpage, bool writable);

/* FILE의 OFS 오프셋에서 시작하는 세그먼트를 UPAGE 주소에 로드한다.
 * 총 READ_BYTES + ZERO_BYTES 바이트의 가상 메모리를 다음과 같이
 * 초기화한다.
 *
 * - UPAGE의 READ_BYTES 바이트는 FILE의 OFS 오프셋부터 읽어야 한다.
 *
 * - UPAGE + READ_BYTES의 ZERO_BYTES 바이트는 0으로 채워야 한다.
 *
 * 이 함수가 초기화한 페이지들은 WRITABLE이 true이면 사용자 프로세스가
 * 쓸 수 있어야 하고, 그렇지 않으면 읽기 전용이어야 한다.
 *
 * 성공하면 true를 반환하고, 메모리 할당 오류나 디스크 읽기 오류가
 * 발생하면 false를 반환한다. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
		uint32_t read_bytes, uint32_t zero_bytes, bool writable) {
	ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
	ASSERT (pg_ofs (upage) == 0);
	ASSERT (ofs % PGSIZE == 0);

	file_seek (file, ofs);
	while (read_bytes > 0 || zero_bytes > 0) {
		/* 이 페이지를 어떻게 채울지 계산한다.
		 * FILE에서 PAGE_READ_BYTES 바이트를 읽고 마지막 PAGE_ZERO_BYTES
		 * 바이트를 0으로 채운다. */
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		/* 메모리 페이지를 얻는다. */
		uint8_t *kpage = palloc_get_page (PAL_USER);
		if (kpage == NULL)
			return false;

		/* 이 페이지를 로드한다. */
		if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes) {
			palloc_free_page (kpage);
			return false;
		}
		memset (kpage + page_read_bytes, 0, page_zero_bytes);

		/* 페이지를 프로세스의 주소 공간에 추가한다. */
		if (!install_page (upage, kpage, writable)) {
			printf("fail\n");
			palloc_free_page (kpage);
			return false;
		}

		/* 다음으로 진행한다. */
		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;
		upage += PGSIZE;
	}
	return true;
}

/* USER_STACK에 0으로 채운 페이지를 매핑하여 최소 스택을 만든다. */
static bool
setup_stack (struct intr_frame *if_) {
	uint8_t *kpage;
	bool success = false;

	kpage = palloc_get_page (PAL_USER | PAL_ZERO);
	if (kpage != NULL) {
		success = install_page (((uint8_t *) USER_STACK) - PGSIZE, kpage, true);
		if (success)
			if_->rsp = USER_STACK;
		else
			palloc_free_page (kpage);
	}
	return success;
}

/* 사용자 가상 주소 UPAGE에서 커널 가상 주소 KPAGE로의 매핑을 페이지
 * 테이블에 추가한다.
 * WRITABLE이 true이면 사용자 프로세스가 페이지를 수정할 수 있고,
 * 그렇지 않으면 읽기 전용이다.
 * UPAGE는 아직 매핑되어 있으면 안 된다.
 * KPAGE는 palloc_get_page()로 사용자 풀에서 얻은 페이지여야 할 것이다.
 * 성공하면 true를 반환하고, UPAGE가 이미 매핑되어 있거나 메모리 할당에
 * 실패하면 false를 반환한다. */
static bool
install_page (void *upage, void *kpage, bool writable) {
	struct thread *t = thread_current ();

	/* 해당 가상 주소에 이미 페이지가 없는지 확인한 뒤, 그곳에 페이지를
	 * 매핑한다. */
	return (pml4_get_page (t->pml4, upage) == NULL
			&& pml4_set_page (t->pml4, upage, kpage, writable));
}
#else
/* 여기부터의 코드는 프로젝트 3 이후에 사용된다.
 * 프로젝트 2에서만 이 함수를 구현하려면 위쪽 블록에 구현하라. */

static bool
lazy_load_segment (struct page *page, void *aux) {
	/* TODO: 파일에서 세그먼트를 로드한다. */
	/* TODO: 이 함수는 VA 주소에서 첫 페이지 폴트가 발생했을 때 호출된다. */
	/* TODO: 이 함수를 호출할 때 VA를 사용할 수 있다. */
}

/* FILE의 OFS 오프셋에서 시작하는 세그먼트를 UPAGE 주소에 로드한다.
 * 총 READ_BYTES + ZERO_BYTES 바이트의 가상 메모리를 다음과 같이
 * 초기화한다.
 *
 * - UPAGE의 READ_BYTES 바이트는 FILE의 OFS 오프셋부터 읽어야 한다.
 *
 * - UPAGE + READ_BYTES의 ZERO_BYTES 바이트는 0으로 채워야 한다.
 *
 * 이 함수가 초기화한 페이지들은 WRITABLE이 true이면 사용자 프로세스가
 * 쓸 수 있어야 하고, 그렇지 않으면 읽기 전용이어야 한다.
 *
 * 성공하면 true를 반환하고, 메모리 할당 오류나 디스크 읽기 오류가
 * 발생하면 false를 반환한다. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
		uint32_t read_bytes, uint32_t zero_bytes, bool writable) {
	ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
	ASSERT (pg_ofs (upage) == 0);
	ASSERT (ofs % PGSIZE == 0);

	while (read_bytes > 0 || zero_bytes > 0) {
		/* 이 페이지를 어떻게 채울지 계산한다.
		 * FILE에서 PAGE_READ_BYTES 바이트를 읽고 마지막 PAGE_ZERO_BYTES
		 * 바이트를 0으로 채운다. */
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		/* TODO: lazy_load_segment에 정보를 전달하도록 aux를 설정한다. */
		void *aux = NULL;
		if (!vm_alloc_page_with_initializer (VM_ANON, upage,
					writable, lazy_load_segment, aux))
			return false;

		/* 다음으로 진행한다. */
		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;
		upage += PGSIZE;
	}
	return true;
}

/* USER_STACK에 스택 PAGE를 만든다. 성공하면 true를 반환한다. */
static bool
setup_stack (struct intr_frame *if_) {
	bool success = false;
	void *stack_bottom = (void *) (((uint8_t *) USER_STACK) - PGSIZE);

	/* TODO: stack_bottom에 스택을 매핑하고 즉시 페이지를 claim한다.
	 * TODO: 성공하면 그에 맞게 rsp를 설정한다.
	 * TODO: 해당 페이지가 스택임을 표시해야 한다. */
	/* TODO: 구현 내용을 여기에 작성한다. */

	return success;
}
#endif /* VM */
