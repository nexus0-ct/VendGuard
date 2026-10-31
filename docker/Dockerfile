FROM ubuntu:24.04@sha256:534baea6a22c03a63003dbc8dbe78fe34bc0d7e595d9a9dc9834884ff530eb55

ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    make \
    gcc-arm-none-eabi=15:13.2.rel1-2 \
    binutils-arm-none-eabi=2.42-1ubuntu1+23 \
    libnewlib-arm-none-eabi=4.4.0.20231231-2 \
    gcc \
    libc6-dev \
    python3 \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace
