## Build on Linux

Build on Linux:

Prerequisites:
- gcc-7+
- glibc-2.25
- cmake
- automake
- autoconf
- libsnappy
- zlib1g
- liblz4
- libzstd
- libgflags
- libgtest
- jemalloc
- rocksdb

Steps to update your gcc and glibc
```
sudo add-apt-repository ppa:ubuntu-toolchain-r/test
sudo apt-get update 
sudo apt-get install gcc-7

sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-7 100
sudo update-alternatives --config gcc

```

Steps to build on Debian/Ubuntu based distributions with a x86_64 machine:
```
sudo apt install cmake automake autoconf gcc libsnappy-dev zlib1g-dev libbz2-dev liblz4-dev libzstd-dev libgflags-dev libgtest-dev

```
Steps to get depend source and compile
```
wget https://github.com/jemalloc/jemalloc/archive/5.2.1.tar.gz
mv 5.2.1.tar.gz jemalloc-5.2.1.tar.gz
tar -zvxf jemalloc-5.2.1.tar.gz
cd jemalloc-5.2.1
./autogen.sh
make -j 4
sudo make install

wget https://github.com/gperftools/gperftools/releases/download/gperftools-2.7/gperftools-2.7.tar.gz
tar -zvxf gperftools-2.7.tar.gz
cd gperftools-2.7
./autogen.sh
make -j 4
sudo make install

wget https://github.com/facebook/rocksdb/archive/v6.4.6.tar.gz
tar -zvxf v6.4.6.tar.gz
cd v6.4.6
mkdir build & cd build
cmake .. -DWITH_JEMALLOC=ON -DWITH_SNAPPY=ON -DWITH_LZ4=ON -DWITH_ZLIB=ON -DWITH_ZSTD=ON
make -j 4
sudo make install

```
Compile Xdag Code
```
mkdir build & cd build
cmake ..
make -j 4

OPTIONS:
-DBUILD_TEST=ON  #build test cases
-DRUN_TEST=ON    #run test cases after build

```


