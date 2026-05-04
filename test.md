# Pintos Project 2 D Role Test Guide

D 역할(file syscall / fd table)을 진행하면서 테스트를 바로 복붙해서 실행할 수 있게 정리한 문서다.

## 0. 테스트 전 준비

처음 테스트를 시작할 때는 컨테이너에서 아래 순서로 준비한다.

```bash
cd /workspaces/pintos_lab/pintos/userprog
export PATH=/workspaces/pintos_lab/pintos/utils:$PATH
make
cd build
which pintos
```

`which pintos` 결과가 아래처럼 나와야 한다.

```text
/workspaces/pintos_lab/pintos/utils/pintos
```

만약 `pintos: not found`가 나오면 아래를 다시 실행한다.

```bash
export PATH=/workspaces/pintos_lab/pintos/utils:$PATH
```

## 1. 같은 테스트를 다시 돌릴 때

같은 테스트를 다시 돌릴 때 이전 `.output`, `.errors`, `.result`가 남아 있으면 헷갈릴 수 있다. 제일 단순하게는 clean 후 다시 빌드한다.

```bash
cd /workspaces/pintos_lab/pintos/userprog
make clean
make
cd build
export PATH=/workspaces/pintos_lab/pintos/utils:$PATH
```

그 다음 원하는 테스트를 다시 실행한다.

```bash
make tests/userprog/create-normal.result
```

결과 파일만 다시 확인하고 싶으면 아래처럼 본다.

```bash
cat tests/userprog/create-normal.result
```

실패 원인을 더 보고 싶으면 아래 파일도 확인한다.

```bash
cat tests/userprog/create-normal.output
cat tests/userprog/create-normal.errors
```

## 2. 준비 확인 테스트

D 역할 테스트 전에 user program 실행, stdout 출력, exit 메시지가 되는지 확인한다.

| 테스트 | 실행 명령어 | 요구사항 |
|---|---|---|
| `args-none` | `make tests/userprog/args-none.result` | argument passing 기본. `argc=1`, `argv[0]` 출력, exit 메시지 필요 |
| `args-single` | `make tests/userprog/args-single.result` | 인자 1개가 `argv[1]`로 들어가야 함 |
| `args-multiple` | `make tests/userprog/args-multiple.result` | 여러 인자가 순서대로 stack에 들어가야 함 |
| `args-many` | `make tests/userprog/args-many.result` | 많은 인자도 빠짐없이 stack에 들어가야 함 |
| `args-dbl-space` | `make tests/userprog/args-dbl-space.result` | 공백이 여러 개 있어도 인자를 올바르게 나눠야 함 |
| `exit` | `make tests/userprog/exit.result` | `프로그램명: exit(status)` 형식 출력 필요 |

## 3. Create 테스트

`create(file, initial_size)`는 fd table 없이 시작할 수 있다. 파일 이름 문자열과 초기 크기를 받아 `filesys_create`에 연결한다.

| 테스트 | 실행 명령어 | 요구사항 |
|---|---|---|
| `create-normal` | `make tests/userprog/create-normal.result` | 평범한 파일 이름 `quux.dat` 생성 성공 |
| `create-empty` | `make tests/userprog/create-empty.result` | 빈 문자열 `""` 생성 실패 또는 `exit(-1)` |
| `create-long` | `make tests/userprog/create-long.result` | 너무 긴 파일 이름 생성 실패 |
| `create-exists` | `make tests/userprog/create-exists.result` | 이미 존재하는 파일 이름으로 다시 생성하면 실패 |
| `create-null` | `make tests/userprog/create-null.result` | 파일 이름 포인터가 `NULL`이면 `exit(-1)` 필요 |
| `create-bad-ptr` | `make tests/userprog/create-bad-ptr.result` | 잘못된 유저 주소를 파일 이름으로 주면 `exit(-1)` 필요 |
| `create-bound` | `make tests/userprog/create-bound.result` | 파일 이름 문자열이 페이지 경계에 걸쳐 있어도 유효하면 성공 |

먼저 노릴 테스트는 `create-normal`, `create-empty`, `create-long`, `create-exists`다. `create-null`, `create-bad-ptr`, `create-bound`는 pointer validation이 필요하다.

## 4. Remove 테스트

userprog 단독 remove 테스트는 적고, `filesys/base`에서 함께 확인된다.

| 테스트 | 실행 명령어 | 요구사항 |
|---|---|---|
| `syn-remove` | `make tests/filesys/base/syn-remove.result` | 열린 파일을 remove한 뒤에도 기존 fd로 read/write 가능해야 함 |

`syn-remove`는 create, open, remove, write, seek, read, close가 모두 어느 정도 되어야 의미 있게 볼 수 있다.

## 5. Open 테스트

`open(file)`부터 fd table이 필요하다. 성공하면 fd `2 이상`을 반환해야 한다.

| 테스트 | 실행 명령어 | 요구사항 |
|---|---|---|
| `open-normal` | `make tests/userprog/open-normal.result` | 존재하는 `sample.txt`를 열고 fd `2 이상` 반환 |
| `open-missing` | `make tests/userprog/open-missing.result` | 없는 파일을 열면 `-1` 반환 |
| `open-twice` | `make tests/userprog/open-twice.result` | 같은 파일을 두 번 열면 서로 다른 fd 반환 |
| `open-empty` | `make tests/userprog/open-empty.result` | 빈 문자열 파일 이름은 `-1` 반환 |
| `open-null` | `make tests/userprog/open-null.result` | 파일 이름 포인터가 `NULL`이면 `exit(-1)` 필요 |
| `open-bad-ptr` | `make tests/userprog/open-bad-ptr.result` | 잘못된 파일 이름 주소면 `exit(-1)` 필요 |
| `open-boundary` | `make tests/userprog/open-boundary.result` | 페이지 경계에 걸친 유효한 파일 이름을 열 수 있어야 함 |

## 6. Close 테스트

`close(fd)`는 fd table에서 fd를 찾고, 연결된 `struct file *`을 닫고, fd table 칸을 비운다.

| 테스트 | 실행 명령어 | 요구사항 |
|---|---|---|
| `close-normal` | `make tests/userprog/close-normal.result` | 열린 파일 fd를 정상 close |
| `close-twice` | `make tests/userprog/close-twice.result` | 같은 fd를 두 번 close하면 조용히 실패하거나 `exit(-1)` |
| `close-bad-fd` | `make tests/userprog/close-bad-fd.result` | 존재하지 않는 fd close는 조용히 실패하거나 `exit(-1)` |

## 7. Read 테스트

`read(fd, buffer, size)`는 fd 0이면 stdin, fd 1이면 실패, fd 2 이상이면 fd table lookup 후 `file_read`로 연결한다.

| 테스트 | 실행 명령어 | 요구사항 |
|---|---|---|
| `read-normal` | `make tests/userprog/read-normal.result` | `sample.txt` 내용을 정확히 읽고 `check_file` 통과 |
| `read-zero` | `make tests/userprog/read-zero.result` | size 0이면 0 반환, buffer 수정 없음 |
| `read-stdout` | `make tests/userprog/read-stdout.result` | fd 1(stdout)은 읽기 대상이 아니므로 실패 또는 `exit(-1)` |
| `read-bad-fd` | `make tests/userprog/read-bad-fd.result` | 없는 fd, 음수 fd, 매우 큰 fd 처리 |
| `read-bad-ptr` | `make tests/userprog/read-bad-ptr.result` | buffer가 잘못된 유저 주소면 `exit(-1)` 필요 |
| `read-boundary` | `make tests/userprog/read-boundary.result` | 페이지 경계에 걸친 buffer에도 정상 read |

## 8. Write 테스트

`write(fd, buffer, size)`는 fd 1이면 stdout 출력, fd 0이면 실패, fd 2 이상이면 fd table lookup 후 `file_write`로 연결한다.

| 테스트 | 실행 명령어 | 요구사항 |
|---|---|---|
| `write-normal` | `make tests/userprog/write-normal.result` | create/open 후 파일에 sample 내용을 write, 쓴 byte 수 반환 |
| `write-zero` | `make tests/userprog/write-zero.result` | size 0이면 0 반환 |
| `write-stdin` | `make tests/userprog/write-stdin.result` | fd 0(stdin)은 쓰기 대상이 아니므로 실패 또는 `exit(-1)` |
| `write-bad-fd` | `make tests/userprog/write-bad-fd.result` | 없는 fd, 음수 fd, 매우 큰 fd 처리 |
| `write-bad-ptr` | `make tests/userprog/write-bad-ptr.result` | buffer가 잘못된 유저 주소면 `exit(-1)` 필요 |
| `write-boundary` | `make tests/userprog/write-boundary.result` | 페이지 경계에 걸친 buffer에도 정상 write |

## 9. Filesize, Seek, Tell 관련 테스트

`filesize`, `seek`, `tell`은 userprog에 직접 이름이 붙은 테스트가 적지만, `tests/lib.c`와 filesys/base 테스트에서 사용된다.

| 테스트 | 실행 명령어 | 요구사항 |
|---|---|---|
| `read-normal` | `make tests/userprog/read-normal.result` | `check_file` 내부에서 `filesize`와 `read` 사용 |
| `sm-random` | `make tests/filesys/base/sm-random.result` | `seek` 후 random offset에 write/read |
| `sm-seq-block` | `make tests/filesys/base/sm-seq-block.result` | 순차 write/read와 close 확인 |
| `sm-seq-random` | `make tests/filesys/base/sm-seq-random.result` | 다양한 block size로 순차 write/read |
| `lg-random` | `make tests/filesys/base/lg-random.result` | 큰 파일 random seek/write/read |
| `lg-seq-block` | `make tests/filesys/base/lg-seq-block.result` | 큰 파일 순차 write/read |
| `lg-seq-random` | `make tests/filesys/base/lg-seq-random.result` | 큰 파일 다양한 block size write/read |

## 10. Fork, Exec, fd table 연결 테스트

이 묶음은 D 혼자만으로 통과하기 어렵다. fork/exec/wait 구현과 fd table 복제/유지 정책이 필요하다.

| 테스트 | 실행 명령어 | 요구사항 |
|---|---|---|
| `fork-read` | `make tests/userprog/fork-read.result` | fork 후 부모/자식이 fd로 read 가능, fd 상태 독립성 필요 |
| `fork-close` | `make tests/userprog/fork-close.result` | 자식 close가 부모 fd를 망가뜨리면 안 됨 |
| `exec-read` | `make tests/userprog/exec-read.result` | fork 후 exec한 프로그램에서도 기존 fd 사용 가능해야 함 |
| `multi-child-fd` | `make tests/userprog/multi-child-fd.result` | 여러 자식의 fd table 독립성, fork/exec/wait 연동 필요 |

## 11. Rox 테스트

실행 중인 executable 파일에는 write가 막혀야 한다. D의 write와 연결되지만, 핵심은 process load/exit에서 실행 파일을 기억하고 write deny를 거는 것이다.

| 테스트 | 실행 명령어 | 요구사항 |
|---|---|---|
| `rox-simple` | `make tests/userprog/rox-simple.result` | 실행 중인 자기 executable에 write하면 0 반환 |
| `rox-child` | `make tests/userprog/rox-child.result` | 자식 executable에도 실행 중 write deny 유지 |
| `rox-multichild` | `make tests/userprog/rox-multichild.result` | 여러 자식 exec 상황에서도 executable write deny 유지 |

## 12. D 역할 추천 진행 순서

아래 순서대로 테스트를 늘려간다.

1. `args-none`, `args-single`, `exit`
2. `create-normal`, `create-empty`, `create-long`, `create-exists`
3. `open-normal`, `open-missing`, `open-twice`
4. `close-normal`, `close-twice`, `close-bad-fd`
5. `read-normal`, `read-zero`
6. `write-normal`, `write-zero`
7. `read-stdout`, `write-stdin`, `read-bad-fd`, `write-bad-fd`
8. pointer validation 후 `*-bad-ptr`, `*-boundary`
9. fork/exec/wait 연동 후 `fork-read`, `fork-close`, `exec-read`, `multi-child-fd`
10. process executable deny write 후 `rox-*`

## 13. 묶어서 돌릴 때

create 기본 묶음:

```bash
make tests/userprog/create-normal.result
make tests/userprog/create-empty.result
make tests/userprog/create-long.result
make tests/userprog/create-exists.result
```

open/close 기본 묶음:

```bash
make tests/userprog/open-normal.result
make tests/userprog/open-missing.result
make tests/userprog/open-twice.result
make tests/userprog/close-normal.result
```

read/write 기본 묶음:

```bash
make tests/userprog/read-normal.result
make tests/userprog/read-zero.result
make tests/userprog/write-normal.result
make tests/userprog/write-zero.result
```
