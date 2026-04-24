# CLion용 Dev Container도 기존 Pintos 환경과 같은 이미지를 사용하도록
# 루트 .devcontainer/Dockerfile을 거의 그대로 복제합니다.
FROM --platform=linux/amd64 ubuntu:22.04

ENV TZ=Asia/Seoul LANG=ko_KR.UTF-8 LANGUAGE=ko_KR.UTF-8

# Build / debug / qemu 실행에 필요한 기본 도구만 기존과 동일하게 설치합니다.
RUN export DEBIAN_FRONTEND=noninteractive && \
    apt-get update \
    # && echo "tzdata tzdata/Areas select Asia" | debconf-set-selections \
    # && echo "tzdata tzdata/Zones/Asia select Seoul" | debconf-set-selections \
    && apt-get install -y \
    locales tzdata \
    build-essential \
    gcc \
    gdb \
    vim \
    git \
    sudo \
    qemu-system-x86 \
    python3 \
    && locale-gen ko_KR.UTF-8 && update-locale LANG=ko_KR.UTF-8
# && apt-get clean \
# && rm -rf /var/lib/apt/lists/*

RUN useradd -m -s /bin/bash jungle && usermod -aG sudo jungle
RUN echo "jungle ALL=(ALL) NOPASSWD:ALL" > /etc/sudoers.d/jungle

USER jungle
WORKDIR /home/jungle

# CLion backend가 여는 터미널에서도 기존 VSCode Dev Container와 동일하게
# Pintos activate 스크립트가 자동 적용되도록 같은 경로를 유지합니다.
RUN echo "source /workspaces/pintos_22.04_lab_docker/pintos/activate" >> /home/jungle/.bashrc

# Clone the repository
# RUN git clone https://github.com/casys-kaist/pintos-kaist /home/jungle/pintos

# Set default shell for the `jungle` user
