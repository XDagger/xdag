## Build on Linux

Build on Docker:

Prerequisites:
- docker
- [Dockerfile](https://github.com/Holt666/xdag/blob/feature/apollo-rocksdb/Dockerfile)
```
root@xxx:/usr/local# docker build -t xdag_c_node:v0.4 .
root@xxx:/usr/local# docker images
REPOSITORY               TAG                 IMAGE ID            CREATED             SIZE
xdag_c_node              v0.4                005fdb01c9d1        18 minutes ago      491MB
ubuntu                   18.04               56def654ec22        2 weeks ago         63.2MB
docker/getting-started   latest              1f32459ef038        2 months ago        26.8MB
root@xxx:/usr/local# docker run -it xdag_c_node:v0.4 /bin/bash
root@xxx:/usr/local# git clone https://github.com/Holt666/xdag.git
root@xxx:/usr/local# cd xdag
root@xxx:/usr/local/xdag# git checkout -b feature/apollo-rocksdb origin/feature/apollo-rocksdb
root@xxx:/usr/local/xdag# mkdir build && cd build
root@xxx:/usr/local/xdag/build# cmake ..
root@xxx:/usr/local/xdag/build# make
```

Build on Linux:

Prerequisites:
- gcc-7+
- g++
- glibc
- cmake
- automake
- autoconf
- libsnappy
- librocksdb
- libjemalloc
- libzlib1g
- libbz2
- liblz4
- libbz2
- libgflags
- libgtest
- libssl
- libsecp256k1
- libgoogle-perftools

Steps to update and install libs
```
apt-get update && apt-get install --yes g++ cmake automake autoconf libsnappy-dev librocksdb-dev libjemalloc-dev zlib1g-dev libbz2-dev liblz4-dev libzstd-dev libgflags-dev libgtest-dev libssl-dev libsecp256k1-dev libgoogle-perftools-dev

Compile Xdag Code
```
mkdir build & cd build
cmake ..
make -j 4

OPTIONS:
```
-DBUILD_TEST=ON  #build test cases
-DRUN_TEST=ON    #run test cases after build
```


