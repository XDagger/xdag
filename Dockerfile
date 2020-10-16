# Xdag C Node Dockerfile Version 1.0

FROM ubuntu:20.04
COPY . /usr/src/xdag
ENV DEBIAN_FRONTEND=noninteractive
WORKDIR /usr/local/xdag
RUN apt-get update && \
    apt-get install --yes g++ cmake automake autoconf libssl-dev libsecp256k1-dev librocksdb-dev libjemalloc-dev libgtest-dev libgoogle-perftools-dev && \
    mkdir -p /usr/src/xdag/build && \
    cd /usr/src/xdag/build && \
    cmake .. && \
    make &&\
    mkdir -p /usr/local/xdag &&\
    cp /usr/src/xdag/build/xdag /usr/local/xdag &&\
    cp /usr/src/xdag/client/netdb-testnet.txt /usr/local/xdag/ &&\
    cp /usr/src/xdag/client/netdb-white-testnet.txt /usr/local/xdag/ &&\
    cp /usr/src/xdag/client/example.pool.config /usr/local/xdag/pool.config
RUN echo '#!/bin/sh' >> /usr/local/xdag/start_pool.sh &&\
    echo './xdag -t -tag docker_xdag -f pool.config -disable-refresh' >> /usr/local/xdag/start_pool.sh &&\
    chmod +x /usr/local/xdag/start_pool.sh

