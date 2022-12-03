/*******************************************************************************
*   Ledger Blue
*   (c) 2016 Ledger
*
*  Licensed under the Apache License, Version 2.0 (the "License");
*  you may not use this file except in compliance with the License.
*  You may obtain a copy of the License at
*
*      http://www.apache.org/licenses/LICENSE-2.0
*
*  Unless required by applicable law or agreed to in writing, software
*  distributed under the License is distributed on an "AS IS" BASIS,
*  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*  See the License for the specific language governing permissions and
*  limitations under the License.
********************************************************************************/

// Adapted from https://github.com/calccrypto/uint256_t

#include <stdint.h>
#include <stdbool.h>

typedef struct uint128_t { uint64_t elements[2]; } uint128_t;

typedef struct uint256_t { uint128_t elements[2]; } uint256_t;

#define UPPER_P(x) x->elements[0]
#define LOWER_P(x) x->elements[1]
#define UPPER(x) x.elements[0]
#define LOWER(x) x.elements[1]
#ifdef __cplusplus
extern "C" {
#endif
extern void readu128BE(uint8_t *buffer, uint128_t *target);
extern void readu256BE(uint8_t *buffer, uint256_t *target);
extern bool zero128(uint128_t *number);
extern bool zero256(uint256_t *number);
extern void copy128(uint128_t *target, uint128_t *number);
extern void copy256(uint256_t *target, uint256_t *number);
extern void clear128(uint128_t *target);
extern void clear256(uint256_t *target);
extern void shiftl128(uint128_t *number, uint32_t value, uint128_t *target);
extern void shiftr128(uint128_t *number, uint32_t value, uint128_t *target);
extern void shiftl256(uint256_t *number, uint32_t value, uint256_t *target);
extern void shiftr256(uint256_t *number, uint32_t value, uint256_t *target);
extern uint32_t bits128(uint128_t *number);
extern uint32_t bits256(uint256_t *number);
extern bool equal128(uint128_t *number1, uint128_t *number2);
extern bool equal256(uint256_t *number1, uint256_t *number2);
extern bool gt128(uint128_t *number1, uint128_t *number2);
extern bool gt256(uint256_t *number1, uint256_t *number2);
extern bool gte128(uint128_t *number1, uint128_t *number2);
extern bool gte256(uint256_t *number1, uint256_t *number2);
extern void add128(uint128_t *number1, uint128_t *number2, uint128_t *target);
extern void add256(uint256_t *number1, uint256_t *number2, uint256_t *target);
extern void minus128(uint128_t *number1, uint128_t *number2, uint128_t *target);
extern void minus256(uint256_t *number1, uint256_t *number2, uint256_t *target);
extern void or128(uint128_t *number1, uint128_t *number2, uint128_t *target);
extern void or256(uint256_t *number1, uint256_t *number2, uint256_t *target);
extern void mul128(uint128_t *number1, uint128_t *number2, uint128_t *target);
extern void mul256(uint256_t *number1, uint256_t *number2, uint256_t *target);
extern void divmod128(uint128_t *l, uint128_t *r, uint128_t *div, uint128_t *mod);
extern void divmod256(uint256_t *l, uint256_t *r, uint256_t *div, uint256_t *mod);
extern bool tostring128(uint128_t *number, uint32_t base, char *out,
                 uint32_t outLength);
extern bool tostring256(uint256_t *number, uint32_t base, char *out,
                 uint32_t outLength);
#ifdef __cplusplus
};
#endif