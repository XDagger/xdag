[![FOSSA Status](https://app.fossa.com/api/projects/git%2Bgithub.com%2FXDagger%2Fxdag.svg?type=shield)](https://app.fossa.com/projects/git%2Bgithub.com%2FXDagger%2Fxdag?ref=badge_shield)

The Dagger (XDAG) cryptocurrency
================================
- Community site: https://xdag.io  
- The Main net was launched January 5, 2018 at 22:45 GMT  

Principles:
----------

- Directed acyclic graph (DAG), not blockchain  
- Block = transaction = address  
- Original idea and implementation  
- Mineable, no premine, no ICO  
- Mining new money every 64 seconds  


HOW-TO:  
----------

- [How to get a wallet](https://github.com/XDagger/xdag/wiki/Get-a-wallet)  
- [How to getting started](https://github.com/XDagger/xdag/wiki/Getting-started)  
- [How to find pools](https://github.com/XDagger/xdag/wiki/Mineable-Pool-List)  
- [How to contribute](https://github.com/XDagger/xdag/blob/master/Contributing.md)  

Docs:  
----------
- [Wiki](https://github.com/XDagger/xdag/wiki)
- [Whitepaper](https://github.com/XDagger/xdag/blob/master/WhitePaper.md)  [中文版](https://github.com/XDagger/xdag/blob/master/WhitePaper%20zh-cn.md)  
- [Protocol](https://github.com/XDagger/xdag/blob/master/Protocol.md)  [中文版](https://github.com/XDagger/xdag/blob/master/Protocol-cn.md)  
- [License](https://github.com/XDagger/xdag/blob/master/LICENSE)  

Main chain idea:
---------------

Every block in DAG has up to 15 links to another blocks (inputs and outputs).
Block B is _referenced_ by another block A if we can reach B from A by following the links.
_Chain_ is a sequence of blocks each of which is referenced by the previous block.
Chain is called _distinct_ if every its block belongs to separate 64-seconds interval.
_Difficulty_of_block_ is 1/hash where _hash_ is sha256(sha256(block)) regarded as little-endian number.
_Difficulty_of_chain_ is sum of difficulties of blocks.
_Main_chain_ is the distinct chain with maximum difficulty.
Blocks in main chain are called _main_blocks_.

Daggers are mined in every main block. There are three periods of mining:

| Stage 1 | Stage 2 | Stage 3 |
| -- | -- | -- |
| Block #1 to #1,017,322 | Block #1,017,323 to #2,097,151 | Each 2,097,152 blocks |
| 1024 XDAG each block | 128 XDAG each block | 64 * (1/2)^n XDAG each block |

The maximum XDAG supply is approximately 1.446294144 billion.

Each dagger is equal to power(2,32) cheatoshino.
Transaction is _valid_ if it is referenced by a main block.
Valid transactions are strictly ordered depending on main chain and links order.
Double spending is prohibited because only first concurrent transaction (by this order) is applied.


Structure of block:
------------------

_The on-disk format will change in the future. Consider this the network protocol._
Each block has a fixed size of 512 bytes.
Block consists of 16 fields each of which has length 32 bytes.
Field 0 is header, it consists of 4 quadwords:
- transport-layer header
- types of all 16 fields, 4 bits for one type
- timestamp of the block, in seconds from Unix era * 1024
- block fee in cheatoshi

Types of fields:

0. nonce
1. header
2. transaction input: 24 lower bytes of block hash and 8 bytes of input amount
3. transaction output, structure is the same as input
4. half of block signature; ECDSA number r or s; digest for signature is hash of (block concate public key)
5. half of output signature; only owner of this key can use this block as input
6. public key (x) with even y
7. public key with odd y
8. header of testnet
9. ... 15. are reserved for future usage.  


Transport layer:
---------------

The dnet network is used as transport layer.
_A new transport layer will come in the future._


Maintainers:
---------------
[Evgeniy](https://github.com/jonano614) ( XDAG: gKNRtSL1pUaTpzMuPMznKw49ILtP6qX3, BTC: 1Jonano4esJzZvqNtUY6NwfPme3EMpVs7n )  
[Frozen](https://github.com/xrdavies) ( XDAG: ~~+L5dzSh1QZv1We3wi8Of31M8eHwQJq4K~~ ) 


Code Contributors:
---------------

<a href = "https://github.com/XDagger/xdag/graphs/contributors">
  <img src = "https://contrib.rocks/image?repo=XDagger/xdag"/>
</a>


* [trueserve](https://github.com/trueserve)
* [sgaragagghu](https://github.com/sgaragagghu)
* [Sofarlemineur](https://github.com/Sofarlemineur)
* [Chen Zhen](https://github.com/czsilence)
* [kbs1](https://github.com/kbs1)
* [amwsffgu](https://github.com/amwsffgu)
* [s2n-gribbly](https://github.com/s2n-Gribbly)
* [reymondtu](https://github.com/reymondtu)
* [sugaryen](https://github.com/sugaryen)
* [mojs78](https://github.com/mojs78)
* [rubencm](https://github.com/rubencm)
* [SantoMacias](https://github.com/SantoMacias)
* [cnukaus](https://github.com/cnukaus)
* [mathsw](https://github.com/mathsw)
* [ruben](https://github.com/xdgruben)
* [Dengfeng Liu](https://github.com/liudf0716)
* [BinaryOverflow](https://github.com/BinaryOverflow)
* [Caleb Marshall](https://github.com/cmarshall108)
* [NKU-Nikoni](https://github.com/NKU-Nikoni)
* [Elder Ryan](https://github.com/RyanKung)
* [hughchiu](https://github.com/hillhero789)
* [Mingege](https://github.com/Mingege-cc)
* [p2peer](https://github.com/p2peer)
* [ZR](https://github.com/zergl)

[Full list of contributors](https://github.com/XDagger/xdag/blob/master/CONTRIBUTORS.md) including all contributors for XDAG sub-projects.


## License
[![FOSSA Status](https://app.fossa.com/api/projects/git%2Bgithub.com%2FXDagger%2Fxdag.svg?type=large)](https://app.fossa.com/projects/git%2Bgithub.com%2FXDagger%2Fxdag?ref=badge_large)
