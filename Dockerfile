# Xdag C Node Dockerfile Version 1.0

FROM ubuntu:18.04

ENV DEBIAN_FRONTEND=noninteractive
WORKDIR /usr/local/

RUN apt-get update && apt-get install --yes git cmake automake autoconf g++ librocksdb-dev libjemalloc-dev libsnappy-dev zlib1g-dev libbz2-dev liblz4-dev libzstd-dev libgflags-dev libgtest-dev libssl-dev libsecp256k1-dev libgoogle-perftools-dev