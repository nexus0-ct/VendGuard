FROM ubuntu:22.04@sha256:5ec03bb3441e8b0bf3b4f9cd4629a1ae763010dc3035bb8da3ae6cf026486401

ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    make \
    gcc-arm-none-eabi=15:10.3-2021.07-4 \
    binutils-arm-none-eabi=2.38-3ubuntu1+15build1 \
    libnewlib-arm-none-eabi=3.3.0-1.3 \
    gcc \
    libc6-dev \
    python3 \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace
