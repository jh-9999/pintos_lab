# Pintos CLion 개발 가이드

이 저장소는 `.devcontainer` 기반 사용만 지원합니다. 문서는 CLion + Dev Container 기준으로만 설명합니다.

## 준비물

- Docker Desktop
- CLion
- JetBrains Dev Containers 사용 가능 환경

## 시작하기

프로젝트를 받은 뒤 CLion에서 저장소 루트를 엽니다.

```bash
git clone --depth=1 https://github.com/krafton-jungle/pintos_22.04_lab_docker.git
cd pintos_22.04_lab_docker
```

CLion에서는 JetBrains Dev Containers로 `.devcontainer/clion/devcontainer.json`을 선택해 컨테이너를 생성합니다.

- 처음 빌드에는 시간이 걸릴 수 있습니다.
- 컨테이너 내부 작업 디렉터리는 `/workspaces/pintos_22.04_lab_docker`입니다.
- 컨테이너 터미널은 `pintos/activate`를 자동으로 읽도록 설정되어 있습니다.

`pintos` 명령이 잡히지 않으면 한 번만 아래를 실행하면 됩니다.

```bash
source /workspaces/pintos_22.04_lab_docker/pintos/activate
```

## 기본 작업 위치

주차별 기본 디렉터리는 아래와 같습니다.

```bash
cd /workspaces/pintos_22.04_lab_docker/pintos/threads   # Project 1
# cd /workspaces/pintos_22.04_lab_docker/pintos/userprog  # Project 2
# cd /workspaces/pintos_22.04_lab_docker/pintos/vm        # Project 3
```

## 빌드와 테스트

가장 기본적인 작업 순서는 아래와 같습니다.

```bash
cd /workspaces/pintos_22.04_lab_docker/pintos/threads
make
make check
```

특정 테스트만 실행하려면 `build` 디렉터리에서 실행합니다.

```bash
cd /workspaces/pintos_22.04_lab_docker/pintos/threads/build
make tests/threads/alarm-zero.result
```

자주 쓰는 예시는 아래와 같습니다.

```bash
cd /workspaces/pintos_22.04_lab_docker/pintos/threads
make

cd /workspaces/pintos_22.04_lab_docker/pintos/threads/build
pintos -- run alarm-multiple
```

## CLion 디버거 사용법

가장 단순한 방식은 터미널에서 QEMU를 `gdb stub` 모드로 띄운 뒤, CLion이 원격 디버거로 attach 하는 방식입니다.

### 1. 커널 빌드

```bash
cd /workspaces/pintos_22.04_lab_docker/pintos/threads
make
```

### 2. QEMU를 디버그 대기 상태로 실행

```bash
cd /workspaces/pintos_22.04_lab_docker/pintos/threads/build
pintos --gdb -- run alarm-multiple
```

이 명령은 QEMU를 실행한 뒤 디버거 연결을 기다립니다. 터미널이 멈춘 것처럼 보여도 정상입니다.

### 3. CLion에서 Remote Debug 설정 선택

이 저장소에는 CLion Remote Debug 설정이 `.idea/runConfigurations/Pintos_Remote_Debug.xml`로 포함되어 있습니다. CLion 상단 실행 설정에서 `Pintos Remote Debug`를 선택하면 됩니다.

설정값은 아래와 같습니다.

- `Debugger`: `GDB`
- `Target`: `tcp:127.0.0.1:1234`
- `Symbol file`: `/workspaces/pintos_22.04_lab_docker/pintos/threads/build/kernel.o`

중요:

- 심볼 파일은 `kernel.bin`이 아니라 `kernel.o`를 사용해야 합니다.
- `pintos --gdb`는 QEMU를 `1234` 포트에서 대기시키므로 CLion도 같은 포트로 붙어야 합니다.

### 4. 브레이크포인트 후 디버그 시작

소스에 브레이크포인트를 건 뒤 CLion에서 `Pintos Remote Debug` 설정을 실행합니다.

처음에는 아래 위치가 확인하기 쉽습니다.

- `threads/init.c`
- `threads/thread.c`
- `devices/timer.c`

CLion이 attach 되면 `Resume`으로 커널 실행을 이어갈 수 있습니다.

## 디버깅 예시 명령

```bash
cd /workspaces/pintos_22.04_lab_docker/pintos/threads
make

cd /workspaces/pintos_22.04_lab_docker/pintos/threads/build
pintos --gdb -- run alarm-zero
```

이후 CLion `Pintos Remote Debug`에서 `kernel.o`에 attach 하면 됩니다.

## 문제 해결

`pintos` 명령이 없을 때:

```bash
source /workspaces/pintos_22.04_lab_docker/pintos/activate
```

CLion이 attach 하지 못할 때:

- `pintos --gdb -- ...`가 먼저 실행되어 있어야 합니다.
- CLion 실행 설정이 `Pintos Remote Debug`인지 확인합니다.
- `Pintos Remote Debug`의 `Target`이 `tcp:127.0.0.1:1234`인지 확인합니다.
- `Pintos Remote Debug`의 `Symbol file`이 현재 빌드한 `kernel.o`인지 확인합니다.

브레이크포인트가 맞지 않을 때:

- `make`를 다시 실행합니다.
- `threads/build/kernel.o` 경로가 맞는지 확인합니다.
