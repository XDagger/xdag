## Build on Docker:

Prerequisites:
- docker
- [Dockerfile](https://github.com/Holt666/xdag/blob/feature/apollo-rocksdb/Dockerfile)
```
root@xxx:/usr/local# git clone https://github.com/Holt666/xdag.git
root@xxx:/usr/local# cd xdag
root@xxx:/usr/local/xdag# git checkout -b feature/apollo-rocksdb origin/feature/apollo-rocksdb
root@xxx:/usr/local# docker build -t xdag_c_node:v0.4 .
root@xxx:/usr/local# docker images
REPOSITORY               TAG                 IMAGE ID            CREATED             SIZE
xdag_c_node              v0.4                5272776db435        About an hour ago   496MB
ubuntu                   20.04               9140108b62dc        2 weeks ago         72.9MB
docker/getting-started   latest              1f32459ef038        3 months ago        26.8MB
root@xxx:/usr/local# docker run -it xdag_c_node:v0.4 /bin/bash
root@xxx:/usr/local/xdag# ./start_pool.sh
xdag client/server, version 0.4.0.
Set password:
Re-type password:
Type random keys: 123456
Generating host keys... OK.
Transport module: dnet T11.231-T14.290.
Type command, help for example.
xdag> stats
Statistics for ours and maximum known parameters:
            hosts: 3 of 3
           blocks: 1 of 1
      main blocks: 0 of 0
     extra blocks: 0
    orphan blocks: 1
 wait sync blocks: 0
 chain difficulty: 0000000024ab18234 of 0000000024ab18234
      XDAG supply: 0.000000000 of 0.000000000
4 hr hashrate MHs: 0.00 of 0.00
xdag>
```

##Build on Linux:

Prerequisites:
- g++
- cmake
- automake
- autoconf
- libssl-dev
- libsecp256k1-dev
- librocksdb-dev
- ibjemalloc-dev
- libgtest-dev
- libgoogle-perftools-dev

Steps to update and install libs
```
root@xxx:/usr/local# apt-get update && apt-get install --yes g++ cmake automake autoconf libssl-dev libsecp256k1-dev librocksdb-dev libjemalloc-dev libgtest-dev libgoogle-perftools-dev
root@xxx:/usr/local# mkdir build & cd build
root@xxx:/usr/local# cmake ..
root@xxx:/usr/local# make -j 4
```

OPTIONS:
```
-DBUILD_TEST=ON  #build test cases
-DRUN_TEST=ON    #run test cases after build
```


