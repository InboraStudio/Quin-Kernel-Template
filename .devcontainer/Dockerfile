FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    clang \
    lld \
    llvm \
    clang-format \
    clang-tidy \
    xorriso \
    qemu-system-x86 \
    ovmf \
    gdb \
    make \
    curl \
    git \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*
