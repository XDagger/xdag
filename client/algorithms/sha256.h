/*********************************************************************
* Filename:   sha256.h
* Author:     Brad Conte (brad AT bradconte.com)
* Copyright:
* Disclaimer: This code is presented "as is" without any guarantees.
* Details:    Defines the API for the corresponding SHA1 implementation.
*********************************************************************/

#ifndef SHA256_H
#define SHA256_H

/*************************** HEADER FILES ***************************/
#include <stddef.h>
#include <stdint.h>
#include <openssl/sha.h>

/****************************** MACROS ******************************/
#define SHA256_BLOCK_SIZE 32            // SHA256 outputs a 32 byte digest

/**************************** DATA TYPES ****************************/

typedef SHA256_CTX SHA256REF_CTX;
#define state h
#define bitlen Nl
#define bitlenH Nh
#define datalen num

/*********************** FUNCTION DECLARATIONS **********************/
void sha256_init(SHA256REF_CTX *ctx);
void sha256_update(SHA256REF_CTX *ctx, const uint8_t *data, size_t len);
void sha256_final(SHA256REF_CTX *ctx, uint8_t *hash);

#endif   // SHA256_H
