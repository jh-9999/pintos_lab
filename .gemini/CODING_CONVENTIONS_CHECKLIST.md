# 코드 컨벤션 평가 기준

기준은 `pintos/tests`보다 `threads`, `userprog`, `filesys`, `vm`, `devices`,
`lib`, `include`의 커널/라이브러리 코드에 맞춰야 한다. 테스트 코드는 파일마다
스타일이 섞여 있으므로 주변 코드를 우선해야 한다.

## 공통 기준

1. `GEN-010`: 수정 코드는 수정 위치 주변의 기존 스타일을 우선해야 한다.
2. `GEN-020`: 커널/라이브러리 코드는 `pintos/tests`의 테스트 코드 스타일보다 `threads`, `userprog`, `filesys`, `vm`, `devices`, `lib`, `include`의 스타일을 우선해야 한다.
3. `GEN-030`: 변경 범위는 요청된 작업과 직접 관련된 코드로 제한해야 한다.
4. `GEN-040`: 대규모 포맷팅 변경은 기능 변경과 섞지 않아야 한다.

## 들여쓰기

1. `IND-010`: 들여쓰기는 space가 아니라 tab을 사용해야 한다.
2. `IND-020`: tab width는 8 기준으로 해석되어야 한다.
3. `IND-030`: block depth 1단계는 tab 1개로 들여써야 한다.
4. `IND-040`: 주석과 코드는 가능하면 80 columns 안쪽으로 작성해야 한다.

```c
if (cond)
	return;
```

## 함수 호출/선언

1. `CALL-010`: 함수 호출은 함수명과 `(` 사이에 공백을 둬야 한다.
2. `CALL-020`: 함수 선언은 함수명과 `(` 사이에 공백을 둬야 한다.

```c
void thread_init (void);
thread_current ();
list_push_back (&ready_list, &t->elem);
```

## 함수 정의

1. `DEF-010`: `.c` 파일의 함수 정의는 반환 타입을 함수명 위 줄에 둬야 한다.
2. `DEF-020`: `static` 함수 정의는 반환 타입을 함수명 위 줄에 둬야 한다.
3. `DEF-030`: 함수 정의의 여는 중괄호는 함수명 줄 끝에 둬야 한다.

```c
void
thread_init (void) {
	...
}

static size_t
block_size (void *block) {
	...
}
```

## 이름 규칙

1. `NAME-010`: 함수명은 `snake_case`를 사용해야 한다.
2. `NAME-020`: 변수명은 `snake_case`를 사용해야 한다.
3. `NAME-030`: 매크로 이름은 `UPPER_SNAKE_CASE`를 사용해야 한다.
4. `NAME-040`: enum 값은 `UPPER_SNAKE_CASE`를 사용해야 한다.
5. `NAME-050`: struct 타입은 `struct thread`, `struct lock`처럼 C 스타일 이름을 유지해야 한다.

```c
static struct list ready_list;
#define THREAD_MAGIC 0xcd6abf4b
```

## 포인터

1. `PTR-010`: 포인터 선언의 `*`는 타입보다 변수명 쪽에 붙여야 한다.

```c
struct thread *t;
char *name;
void *aux;
```

## if / while / for

1. `CTRL-010`: `if`, `while`, `for` 키워드는 `(` 앞에 공백을 둬야 한다.
2. `CTRL-020`: 한 줄 body는 중괄호 없이 한 단계 들여쓸 수 있다.
3. `CTRL-030`: 여러 문장 body는 중괄호를 사용해야 한다.
4. `CTRL-040`: 새 코드의 braced block은 가능하면 `if (...) {` 형태를 사용해야 한다.
5. `CTRL-050`: 기존 코드가 `if (...)\n{` 형태를 사용하면 수정 위치 주변 스타일을 우선해야 한다.

```c
if (first == last)
	return;

if (cond) {
	stmt1 ();
	stmt2 ();
}
```

## else

1. `ELSE-010`: 앞 `if`가 braced block이면 `else`는 `} else {` 형태를 사용해야 한다.
2. `ELSE-020`: 단문 `if`의 `else`는 수정 위치 주변 스타일을 따라야 한다.

```c
if (cond) {
	stmt1 ();
} else {
	stmt2 ();
}
```

## 주석

1. `CMT-010`: 공개 함수 설명은 `/* ... */` block comment를 사용해야 한다.
2. `CMT-020`: 구조체 설명은 `/* ... */` block comment를 사용해야 한다.
3. `CMT-030`: 긴 설명은 `/* ... */` block comment를 사용해야 한다.
4. `CMT-040`: 짧은 TODO나 임시 설명은 수정 위치 주변 코드의 주석 스타일을 따라야 한다.
5. `CMT-050`: 자명한 코드는 불필요한 주석을 추가하지 않아야 한다.

## 최종 검증

1. `VERIFY-010`: 새 코드는 기존 Pintos 스타일과 일관되어야 한다.
2. `VERIFY-020`: 포맷 변경 줄과 로직 변경 줄은 리뷰가 어렵게 섞이지 않아야 한다.
3. `VERIFY-030`: 관련 테스트 또는 최소 빌드 명령은 실행 가능한 상태로 유지되어야 한다.