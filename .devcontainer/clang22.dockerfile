FROM debian:13

ARG USERNAME=developer
ARG USER_UID=1000
ARG USER_GID=1000
ARG LLVM_VERSION=22

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
       ca-certificates \
       wget \
       gnupg \
       sudo \
       git \
       cmake \
       g++ \
       build-essential \
       pkg-config \
       gdb \
       ninja-build \
       libicu76 \
       libicu-dev \
       bubblewrap \
       linux-perf \
       binutils \
       elfutils \
       procps \
    && mkdir -p /etc/apt/keyrings \
    && wget -qO- https://apt.llvm.org/llvm-snapshot.gpg.key \
       | gpg --dearmor > /etc/apt/keyrings/llvm-snapshot.gpg \
    && echo "deb [signed-by=/etc/apt/keyrings/llvm-snapshot.gpg] http://apt.llvm.org/trixie/ llvm-toolchain-trixie-${LLVM_VERSION} main" \
       > /etc/apt/sources.list.d/llvm.list \
    && apt-get update \
    && apt-get install -y --no-install-recommends \
       clang-${LLVM_VERSION} \
       clangd-${LLVM_VERSION} \
       clang-tidy-${LLVM_VERSION} \
       clang-format-${LLVM_VERSION} \
       lld-${LLVM_VERSION} \
       lldb-${LLVM_VERSION} \
       libc++-${LLVM_VERSION}-dev \
       libc++abi-${LLVM_VERSION}-dev \
       libomp-${LLVM_VERSION}-dev \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

RUN groupadd --gid ${USER_GID} ${USERNAME} \
    && useradd --uid ${USER_UID} --gid ${USER_GID} -m ${USERNAME} \
    && echo "${USERNAME} ALL=(root) NOPASSWD:ALL" > /etc/sudoers.d/${USERNAME} \
    && chmod 0440 /etc/sudoers.d/${USERNAME}