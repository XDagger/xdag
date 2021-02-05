# RandomX Algorithm in XDAG



## Requirements:

#### Install RandomX library

```bash
git clone https://github.com/tevador/RandomX.git
cd RandomX
mkdir build && cd build
cmake -DARCH=native ..
make
sudo make install
```

#### Enable huge pages

Temporary (until next reboot) reserve huge pages

```bash
sudo sysctl -w vm.nr_hugepages=2560
```

Permanent huge pages reservation

```bash
sudo bash -c "echo vm.nr_hugepages=2560 >> /etc/sysctl.conf"
```

- A pool using randomx of full mode needs 2560 huge pages

- A pool using randomx of light mode needs 256 huge pages

- A miner using randomx hash needs 1280 huge pages

When multiple nodes running on one machine, the huge pages number should be the accumulation of all nodes

## Launch parameters (test net):

#### pool
##### RandomX light mode:
```bash
./xdag -t -randomx l -disable-refresh -f pool.config
```
##### RandomX fast mode:
```bash
./xdag -t -randomx f -disable-refresh -f pool.config
```
The pool will switch to randomx algorithm, when fork time frame ( defined by constant in rx_hash.h) coming , automatically. 

light mode: slower hash speed and less memory usage (about 0.5GB)

fast mode: faster hash speed and more memory usage (more than 5GB)
#### miner

##### before RandomX fork:

```bash
./xdag -t -m <MINING_THREAD_NUMBER>  <POOL_ADDRESS>:<POOL_PORT> -a <WALLET_ADDRESS>
```

##### after RandomX fork:

```bash
./xdag -t -randomx f -m <MINING_THREAD_NUMBER> <POOL_ADDRESS>:<POOL_PORT> -a <WALLET_ADDRESS>
```

The miner can't change PoW algorithm automatically.   So miner must turn on switch '-randomx'  after algorithm fork manually.

##### Windows x64 CPU RandowX Miner 

```bash
DaggerMiner -cpu  -p <POOL_ADDRESS>:<POOL_PORT> -t <MINING_THREAD_NUMBER> -a <WALLET_ADDRESS>
```

repository https://github.com/swordlet/DaggerRandomxMiner/tree/RandomX

release https://github.com/swordlet/DaggerRandomxMiner/releases/tag/Pre_0.4.0

miner not support GPU yet.



## Algorithm Description:

#### algorithm seed and fork 

- selecting seed block that `(seedBlockHeight + 128) % 4096 == 0 `
- using sha256 hash of seed block as new randomx seed
- changing the seed  after time frame   `(Time frame of (seedBlockHeight + 128)) + 128  `
- pool algorithm fork after time frame   `(Time frame of RANDOMX_FORK_HEIGHT) + 128  `
- pre set next switch time and next seed when height % 4096 == 0

#### mining

- every 64 Bytes mining task contains: 32 Bytes sha256 hash of mining block without nonce( i.e., without last field) and 32 Bytes randomx seed 
- randomx miner using  32 Bytes sha256 hash of mining block concatenate 32 Bytes nonce as input, searching nonce for minimal  randomx hash 

- miner send mined nonce back  to pool,  pool verify nonce with randomx hash and calculate share payment by difficulty of the randomx hash

####  hash of block

- using block's randomx hash to calculate its difficulty when block time is end with 0xffff, otherwise using block's Dsha256 hash
- using block's Dsha256 hash  as its hash value in any other circumstance



## Implementations:

#### constants

in rx_hash.h

```c
#define SEEDHASH_EPOCH_BLOCKS   4096 // period of a randomx seed
#define SEEDHASH_EPOCH_LAG    128 // lag time frames for switch randomx seed

#define SEEDHASH_EPOCH_TESTNET_BLOCKS  64
#define SEEDHASH_EPOCH_TESTNET_LAG    32

// fork seed height, (time frame of RANDOMX_FORK_HEIGHT) + SEEDHASH_EPOCH_LAG = fork time frame
#define RANDOMX_FORK_HEIGHT           1339392 

// (time frame of RANDOMX_TESTNET_FORK_HEIGHT) + SEEDHASH_EPOCH_TESTNET_LAG = test net fork time frame
#define RANDOMX_TESTNET_FORK_HEIGHT   196288 // 196288 % 64 = 0
```

