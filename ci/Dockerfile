FROM ubuntu:18.04

RUN apt-get update && \
    apt-get install -y curl wget zip unzip build-essential pkg-config cmake zlib1g-dev python python3 && \
    apt-get clean

RUN cd /tmp && \
    wget -O install.sh https://github.com/bazelbuild/bazel/releases/download/0.25.2/bazel-0.25.2-installer-linux-x86_64.sh && \
    chmod +x install.sh && \
    ./install.sh && \
    rm install.sh

RUN useradd build && \
    mkdir -p /home/build && \
    chown build:build /home/build
USER build
WORKDIR /build
