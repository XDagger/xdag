## The Dagger crypto currency: white paper v0.3

##### April 4, 2018

**Abstract:** Dagger Community introduces a new crypto currency based on an directed acyclic graph (DAG) instead of blockchain, and unlike other DAG-oriented crypto currency, XDAG allows mining. The goal of this project is to create a decentralized payment system that allows processing thousands of transactions per second.

**PLEASE NOTE: CRYPTOGRAPHIC TOKENS REFERRED TO IN THIS WHITE PAPER REFER TO CRYPTOGRAPHIC TOKENS ON A LAUNCHED DAG. THEY DO NOT REFER TO THE ERC-20 COMPATIBLE TOKENS BEING DISTRIBUTED ON THE ETHEREUM BLOCKCHAIN IN CONNECTION WITH THE XDAG TOKEN DISTRIBUTION.**

Copyright © 2018 Dagger Community Contributors

Without permission, anyone may use, reproduce or distribute any material in this white paper for non-commercial and educational use (i.e., other than for a fee or for commercial purposes) provided that the original source and the applicable copyright notice are cited.

**DISCLAIMER:** This White Paper v0.3 is for information purposes only. Dagger Community does not guarantee the accuracy of or the conclusions reached in this white paper, and this white paper is provided “as is”. Dagger Community does not make and expressly disclaims all representations and warranties, express, implied, statutory or otherwise, whatsoever, including, but not limited to: (i) warranties of merchantability, fitness for a particular purpose, suitability, usage, title or noninfringement; (ii) that the contents of this white paper are free from error; and (iii) that such contents will not infringe third-party rights. Dagger Community shall have no liability for damages of any kind arising out of the use, reference to, or reliance on this white paper or any of the content contained herein, even if advised of the possibility of such damages. In no event will Dagger Community be liable to any person or entity for any damages, losses, liabilities, costs or expenses of any kind, whether direct or indirect, consequential, compensatory, incidental, actual, exemplary, punitive or special for the use of, reference to, or reliance on this white paper or any of the content contained herein, including, without limitation, any loss of business, revenues, profits, data, use, goodwill or other intangible losses.


## Introduction
Dagger (token XDAG) is a new crypto currency, which is not based on the blockchain, but on an directed acyclic graph (DAG) instead and, unlike other DAG-oriented coins, allows mining.


## Ideas
Each block contains exactly one transaction. At the same time, the block is an address. Among all transactions, the main chain is allocated - it is a chain with the maximum difficulty. In the main chain, new coins are created about once a minute.

Every block in DAG has up to 15 links to another blocks (inputs and outputs). Block B is referenced by another block A if we can reach B from A by following the links. Chain is a sequence of blocks each of which is referenced by the previous block. Chain is called distinct if every its block belongs to separate 64-seconds interval. Difficulty_of_block is 1/hash where hash is sha256(sha256(block)) regarded as little-endian number. Difficulty_of_chain is sum of difficulties of blocks. Main_chain is the distinct chain with maximum difficulty. Blocks in main chain are called main_blocks.

Daggers are mined in every main block. For first 4 years 1024 XDAG are mined in each main block. For second 4 years - 512 XDAG, and so on. So, maximum XDAG supply is approximately power(2,32). Each dagger is equal to power(2,32) cheatoshino. Transaction is valid if it is referenced by a main block. Valid transactions are strictly ordered depending on main chain and links order. Double spending is prohibited because only first concurrent transaction (by this order) is applied.


## Security
The ECDSA algorithm with a 256-bit private key is used for a signature that confirms the rights of the wallet owner for money in the given address. All messages are transmitted between hosts in an encrypted form using the author's semi-symmetric encryption algorithm. The session key to it is transmitted using the 8192-bit key RSA algorithm.


## Roadmap
Main network was launched **January 5, 2018. ICO is not planned. There is no pre-mine**. Everyone can participate in mining on equal terms. 

CPU/GPU can be used for mining at the present, and existing asics are not suitable for mining.

## Team
The original developer of the dagger is an anonymous author under the pseudonym Daniel Cheatoshin (cheatoshin@mail.com).

Currently this project is maintained by Dagger Community Dev team.
