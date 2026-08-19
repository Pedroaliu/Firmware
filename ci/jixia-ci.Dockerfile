FROM ubuntu:24.04

LABEL org.opencontainers.image.source="https://github.com/Pedroaliu/Firmware"
LABEL org.opencontainers.image.description="Hermetic RV64/QEMU CI toolchain for Jixia Firmware"
LABEL org.opencontainers.image.licenses="Apache-2.0"

ARG DEBIAN_FRONTEND=noninteractive

SHELL ["/bin/bash", "-o", "pipefail", "-c"]

RUN set -eux; \
    timeout --signal=TERM --kill-after=10s 300s \
        apt-get \
            -o Acquire::Retries=5 \
            -o Acquire::http::Timeout=20 \
            -o Acquire::https::Timeout=20 \
            -o Acquire::ForceIPv4=true \
            -o Dpkg::Use-Pty=0 \
            update; \
    timeout --signal=TERM --kill-after=10s 600s \
        apt-get \
            -o Acquire::Retries=5 \
            -o Acquire::http::Timeout=20 \
            -o Acquire::https::Timeout=20 \
            -o Acquire::ForceIPv4=true \
            -o Dpkg::Use-Pty=0 \
            install -y --no-install-recommends \
                bash \
                binutils-riscv64-unknown-elf \
                ca-certificates \
                clang-format \
                cmake \
                coreutils \
                diffutils \
                file \
                findutils \
                gawk \
                gcc-riscv64-unknown-elf \
                git \
                grep \
                gzip \
                ninja-build \
                python3 \
                qemu-system-misc \
                sed \
                xz-utils; \
    rm -rf /var/lib/apt/lists/*

WORKDIR /workspace
