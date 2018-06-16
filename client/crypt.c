/* cryptography (ECDSA), T13.654-T13.847 $DVS:time$ */

#include <string.h>
#include <openssl/crypto.h>
#include <openssl/bio.h>
#include <openssl/bn.h>
#include <openssl/evp.h>
#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/ecdsa.h>
#include "crypt.h"
#include "transport.h"
#include "utils/log.h"
#include "system.h"

#include "../secp256k1/include/secp256k1.h"

secp256k1_context *ctx_noopenssl;

static EC_GROUP *group;

extern unsigned int xOPENSSL_ia32cap_P[4];
extern int xOPENSSL_ia32_cpuid(unsigned int *);

// initialization of the encryption system
int xdag_crypt_init(int withrandom)
{
	if (withrandom) {
		uint64_t buf[64];
		xOPENSSL_ia32_cpuid(xOPENSSL_ia32cap_P);
		xdag_generate_random_array(buf, sizeof(buf));
		xdag_debug("Seed  : [%s]", xdag_log_array(buf, sizeof(buf)));
		RAND_seed(buf, sizeof(buf));
	}

	ctx_noopenssl = secp256k1_context_create(SECP256K1_CONTEXT_VERIFY);

	group = EC_GROUP_new_by_curve_name(NID_secp256k1);
	if (!group) return -1;
	
	return 0;
}

/* creates a new pair of private and public keys
 * the private key is saved to the 'privkey' array, the public key to the 'pubkey' array,
 * the parity of the public key is saved to the variable 'pubkey_bit'
 * returns a pointer to its internal representation
 */
void *xdag_create_key(xdag_hash_t privkey, xdag_hash_t pubkey, uint8_t *pubkey_bit)
{
	uint8_t buf[sizeof(xdag_hash_t) + 1];
	EC_KEY *eckey = 0;
	const BIGNUM *priv = 0;
	const EC_POINT *pub = 0;
	BN_CTX *ctx = 0;
	int res = -1;

	if (!group) {
		goto fail;
	}

	eckey = EC_KEY_new();
	
	if (!eckey) {
		goto fail;
	}

	if (!EC_KEY_set_group(eckey, group)) {
		goto fail;
	}

	if (!EC_KEY_generate_key(eckey)) {
		goto fail;
	}

	priv = EC_KEY_get0_private_key(eckey);
	if (!priv) {
		goto fail;
	}

	if (BN_bn2bin(priv, (uint8_t*)privkey) != sizeof(xdag_hash_t)) {
		goto fail;
	}

	pub = EC_KEY_get0_public_key(eckey);
	if (!pub) {
		goto fail;
	}

	ctx = BN_CTX_new();
	if (!ctx) {
		goto fail;
	}

	BN_CTX_start(ctx);
	if (EC_POINT_point2oct(group, pub, POINT_CONVERSION_COMPRESSED, buf, sizeof(xdag_hash_t) + 1, ctx) != sizeof(xdag_hash_t) + 1) {
		goto fail;
	}

	memcpy(pubkey, buf + 1, sizeof(xdag_hash_t));
	*pubkey_bit = *buf & 1;
	res = 0;

 fail:
	if (ctx) {
		BN_CTX_free(ctx);
	}

	if (res && eckey) {
		EC_KEY_free(eckey);
	}
	
	return res ? 0 : eckey;
}

// returns the internal representation of the key and the public key by the known private key
void *xdag_private_to_key(const xdag_hash_t privkey, xdag_hash_t pubkey, uint8_t *pubkey_bit)
{
	uint8_t buf[sizeof(xdag_hash_t) + 1];
	EC_KEY *eckey = 0;
	BIGNUM *priv = 0;
	EC_POINT *pub = 0;
	BN_CTX *ctx = 0;
	int res = -1;

	if (!group) {
		goto fail;
	}

	eckey = EC_KEY_new();
	if (!eckey) {
		goto fail;
	}

	if (!EC_KEY_set_group(eckey, group)) {
		goto fail;
	}

	priv = BN_new();
	if (!priv) {
		goto fail;
	}

	//	BN_init(priv);
	BN_bin2bn((uint8_t*)privkey, sizeof(xdag_hash_t), priv);
	EC_KEY_set_private_key(eckey, priv);
	
	ctx = BN_CTX_new();
	if (!ctx) {
		goto fail;
	}

	BN_CTX_start(ctx);

	pub = EC_POINT_new(group);
	if (!pub) {
		goto fail;
	}

	EC_POINT_mul(group, pub, priv, NULL, NULL, ctx);
	EC_KEY_set_public_key(eckey, pub);
	if (EC_POINT_point2oct(group, pub, POINT_CONVERSION_COMPRESSED, buf, sizeof(xdag_hash_t) + 1, ctx) != sizeof(xdag_hash_t) + 1) {
		goto fail;
	}

	memcpy(pubkey, buf + 1, sizeof(xdag_hash_t));
	*pubkey_bit = *buf & 1;
	res = 0;

 fail:
	if (ctx) {
		BN_CTX_free(ctx);
	}

	if (priv) {
		BN_free(priv);
	}

	if (pub) {
		EC_POINT_free(pub);
	}

	if (res && eckey) {
		EC_KEY_free(eckey);
	}

	return res ? 0 : eckey;
}

// Returns the internal representation of the key by the known public key
void *xdag_public_to_key(const xdag_hash_t pubkey, uint8_t pubkey_bit)
{
	EC_KEY *eckey = 0;
	BIGNUM *pub = 0;
	EC_POINT *p = 0;
	BN_CTX *ctx = 0;
	int res = -1;

	if (!group) {
		goto fail;
	}

	eckey = EC_KEY_new();
	if (!eckey) {
		goto fail;
	}

	if (!EC_KEY_set_group(eckey, group)) {
		goto fail;
	}

	pub = BN_new();
	if (!pub) {
		goto fail;
	}

	//	BN_init(pub);
	BN_bin2bn((uint8_t*)pubkey, sizeof(xdag_hash_t), pub);
	p = EC_POINT_new(group);
	if (!p) {
		goto fail;
	}

	ctx = BN_CTX_new();
	if (!ctx) {
		goto fail;
	}

	BN_CTX_start(ctx);
	if (!EC_POINT_set_compressed_coordinates_GFp(group, p, pub, pubkey_bit, ctx)) {
		goto fail;
	}

	EC_KEY_set_public_key(eckey, p);
	res = 0;

 fail:
	if (ctx) {
		BN_CTX_free(ctx);
	}

	if (pub) {
		BN_free(pub);
	}

	if (p) {
		EC_POINT_free(p);
	}

	if (res && eckey) {
		EC_KEY_free(eckey);
	}

	return res ? 0 : eckey;
}

// removes the internal key representation
void xdag_free_key(void *key)
{
	EC_KEY_free((EC_KEY*)key);
}

// sign the hash and put the result in sign_r and sign_s
int xdag_sign(const void *key, const xdag_hash_t hash, xdag_hash_t sign_r, xdag_hash_t sign_s)
{
	uint8_t buf[72], *p;
	unsigned sig_len, s;

	if (!ECDSA_sign(0, (const uint8_t*)hash, sizeof(xdag_hash_t), buf, &sig_len, (EC_KEY*)key)) {
		return -1;
	}

	p = buf + 3, s = *p++;

	if (s >= sizeof(xdag_hash_t)) {
		memcpy(sign_r, p + s - sizeof(xdag_hash_t), sizeof(xdag_hash_t));
	} else  {
		memset(sign_r, 0, sizeof(xdag_hash_t));
		memcpy((uint8_t*)sign_r + sizeof(xdag_hash_t) - s, p, s);
	}

	p += s + 1, s = *p++;

	if (s >= sizeof(xdag_hash_t)) {
		memcpy(sign_s, p + s - sizeof(xdag_hash_t), sizeof(xdag_hash_t));
	} else  {
		memset(sign_s, 0, sizeof(xdag_hash_t));
		memcpy((uint8_t*)sign_s + sizeof(xdag_hash_t) - s, p, s);
	}

	xdag_debug("Sign  : hash=[%s] sign=[%s] r=[%s], s=[%s]", xdag_log_hash(hash),
		xdag_log_array(buf, sig_len), xdag_log_hash(sign_r), xdag_log_hash(sign_s));
	
	return 0;
}

static uint8_t *add_number_to_sign(uint8_t *sign, const xdag_hash_t num)
{
        uint8_t *n = (uint8_t*)num;
        int i, len, leadzero;

        for (i = 0; i < sizeof(xdag_hash_t) && !n[i]; ++i);

        leadzero = (i < sizeof(xdag_hash_t) && n[i] & 0x80);
        len = (sizeof(xdag_hash_t) - i) + leadzero;
        *sign++ = 0x02;
        *sign++ = len;

        if (leadzero) { 
                *sign++ = 0;
        }

        while (i < sizeof(xdag_hash_t)) {
                *sign++ = n[i++];
        }

        return sign;
}


static uint8_t *add_number_to_sign_noopenssl(uint8_t *sign, const xdag_hash_t num)
{
	uint8_t *n = (uint8_t*)num;
	int i, len, leadzero;

	for (i = 0; i < sizeof(xdag_hash_t) && !n[i]; ++i);

	leadzero = (i < sizeof(xdag_hash_t) && n[i] & 0x80);
	len = (sizeof(xdag_hash_t) - i) + leadzero;
	*sign++ = 0x02;
	if(len)
		*sign++ = len;
	else{
		*sign++ = 1;
		*sign++ = 0;
		return sign;
	}
	
	if (leadzero) {
		*sign++ = 0;
	}

	while (i < sizeof(xdag_hash_t)) {
		*sign++ = n[i++];
	}
	
	return sign;
}

// verify that the signature (sign_r, sign_s) corresponds to a hash 'hash', a version for its own key
// returns 0 on success
int xdag_verify_signature(const void *key, const xdag_hash_t hash, const xdag_hash_t sign_r, const xdag_hash_t sign_s)
{
	uint8_t buf[72], *ptr;
	int res;

	ptr = add_number_to_sign(buf + 2, sign_r);
	ptr = add_number_to_sign(ptr, sign_s);
	buf[0] = 0x30;
	buf[1] = ptr - buf - 2;
	res = ECDSA_verify(0, (const uint8_t*)hash, sizeof(xdag_hash_t), buf, ptr - buf, (EC_KEY*)key);

	xdag_debug("Verify: res=%2d key=%lx hash=[%s] sign=[%s] r=[%s], s=[%s]", res, (long)key, xdag_log_hash(hash),
		xdag_log_array(buf, ptr - buf), xdag_log_hash(sign_r), xdag_log_hash(sign_s));
	
	return res != 1;
}

// returns 0 on success
int xdag_verify_signature_noopenssl(const void *key, const xdag_hash_t hash, const xdag_hash_t sign_r, const xdag_hash_t sign_s)
{

	uint8_t buf_pubkey[sizeof(xdag_hash_t) + 1];
	//xdag_hash_t pubkey={0};
//	secp256k1_context *ctx_noopenssl = secp256k1_context_create(SECP256K1_CONTEXT_VERIFY);
	secp256k1_pubkey pubkey_noopenssl;
	size_t pubkeylen =sizeof(xdag_hash_t)+1;
	secp256k1_ecdsa_signature sig_noopenssl;
        secp256k1_ecdsa_signature sig_noopenssl_normalized;



/*
	if((pubkey=malloc(sizeof(xdag_hash_t) + 1))){
                printf("malloc noopensll failed");
                fflush(stdout);i
	
	}
*/	
/*

	if (!group) {
		printf("group is not defined?");
                fflush(stdout);
	}
*/
/*
 	
	const EC_POINT *ec_point = NULL;
	if(!(ec_point = EC_KEY_get0_public_key(key))){
		printf("cannot create EC_POINT");
		fflush(stdout);
	}
*/
/*
        BN_CTX *ctx = 0;
	if (!(ctx = BN_CTX_new())) {
		printf("cannot create BN_CTX");
                fflush(stdout);

	}

	BN_CTX_start(ctx);
*/
/*	
        if(EC_POINT_point2oct(group, ec_point, POINT_CONVERSION_COMPRESSED, buf, sizeof(xdag_hash_t)+1, ctx)!=sizeof(xdag_hash_t) + 1){
         //       printf("cannot create oct");
                fflush(stdout);
        }
*/




        //   printf("public key right \n %02hhx%016llx%016llx%016llx%016llx \n", buf[0] );
	//	fflush(stdout);

	//memcpy(pubkey, buf , sizeof(xdag_hash_t)+1);
	// *pubkey_bit = *buf & 1;
int odd=0;
if((uintptr_t)key & 1){
	odd=1;
//	printf("ODD");
//	fflush(stdout);
}
//else{
//printf("EVEN");
//fflush(stdout);
//}

buf_pubkey[0]=2+odd;
memcpy(&(buf_pubkey[1]),(xdag_hash_t*)((uintptr_t)key & ~1l), sizeof(xdag_hash_t));



int i=0;
	if((i=secp256k1_ec_pubkey_parse(ctx_noopenssl, &pubkey_noopenssl,buf_pubkey, pubkeylen))!=1){
		printf("cannot parse key, %i",i );
		fflush(stdout);
	}

        uint8_t sign_buf[72], *ptr;
        //int res;

        ptr = add_number_to_sign_noopenssl(sign_buf + 2, sign_r);
        ptr = add_number_to_sign_noopenssl(ptr, sign_s);
        sign_buf[0] = 0x30;
        sign_buf[1] = ptr - sign_buf - 2;


//printf("noopenssl sign_r \n %08llx%016llx%016llx%016llx \n", &sign_r );
//printf("noopenssl sign_s \n %08llx%016llx%016llx%016llx \n", &sign_s );
//printf("noopenssl signature \n %08llx%016llx%016llx%016llx%016llx%016llx%016llx%016llx \n", *sign_buf );
//printf("noopenssl signature \n %02hhx \n", *sign_buf );
//printf("sig_r\n %02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx   \n", &sign_r );
//printf("sig_r\n %s   \n", xdag_log_array(&sign_r,sizeof(xdag_hash_t)) );
//printf("sig_s\n %02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx   \n", &sign_s );
//printf("sig_r\n %s   \n", xdag_log_array(&sign_s,sizeof(xdag_hash_t)) );

//printf("sign_buf\n %02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx   \n", &sign_buf );

//printf("sign_buf\n %s   \n", xdag_log_array(&sign_buf,72) );


//fflush(stdout);


/*


ECDSA_SIG* ec_sig = ECDSA_SIG_new(); // TODO FREE

if (NULL == BN_bin2bn((const unsigned char *)sign_r, 32, (ec_sig->r))) {
 // dumpOpenSslErrors();
                printf("failed to load sign_r" );
                fflush(stdout);

 }
//DBG("post r  :%s\n", BN_bn2hex(ec_sig->r));

if (NULL == BN_bin2bn((const unsigned char *)sign_s, 32, (ec_sig->s))) {
 // dumpOpenSslErrors();    
            printf("failed to load sign_s" );
                fflush(stdout);

 }
//DBG("post s  :%s\n", BN_bn2hex(ec_sig->s));

int sig_size = i2d_ECDSA_SIG(ec_sig, NULL);
if (sig_size > 255) {
//  DBG("signature is too large wants %d\n", sig_size);
            printf("signature too much big" );
                fflush(stdout);

 }

unsigned char rsig_bytes[256]={0};
unsigned char ** sig_bytes = &rsig_bytes;
//memset(sig_bytes, 6, 256);

if(!(sig_size = i2d_ECDSA_SIG(ec_sig, (sig_bytes)))){
            printf("signature NOT completed" );
                fflush(stdout);
}

//printf("signature good\n %02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx   \n", &sig_noopenssl );

//printf("sig_noopenssl\n %s   \n", xdag_log_array(*sig_bytes,256) );
//printf("size %i \n", ptr - sign_buf);
//fflush(stdout);

//printf("pubkey 32 \n %s   \n", xdag_log_array(((uintptr_t)key & ~1l),32) );
//printf("pubkey 33\n %s   \n", xdag_log_array(buf_pubkey,33) );
//fflush(stdout);

*/
        if(secp256k1_ecdsa_signature_parse_der(ctx_noopenssl, &sig_noopenssl,sign_buf/* *sig_bytes*/, /*sig_size*/  ptr - sign_buf)!=1){
               printf("cannot parse signature \n" );

               fflush(stdout);
//return 1;

 //       }
/*
printf("sig_noopenssl\n %s   \n", xdag_log_array(*sig_bytes,256) );
printf("size %i \n", sig_size);
*/


printf("sig_noopenssl\n %s   \n", xdag_log_array(sign_buf,72) );
printf("size %ld \n", ptr - sign_buf);
printf("sig_r \n %s   \n", xdag_log_array(sign_r,32) );
printf("sig_s \n %s   \n", xdag_log_array(sign_s,32) );


               fflush(stdout);
return 1;
        }



     /*   if(*/secp256k1_ecdsa_signature_normalize(ctx_noopenssl, &sig_noopenssl_normalized,&sig_noopenssl);//!=1){
/*
                printf("already normalized" );
                fflush(stdout);
        }
	else{
		printf("NORMALIZED");
		fflush(stdout);
	}

*/


        if(secp256k1_ecdsa_verify(ctx_noopenssl, &sig_noopenssl_normalized,(unsigned char*) hash, &pubkey_noopenssl)!=1){
         //       printf("cannot verify" );
         //       fflush(stdout);
		return 1;
        }
/*
else{
printf("VERIFIED");
fflush(stdout);
}

*/
return 0;
//DBG("New size %d\n", sig_size);
//DBG("post i2d:%s\n", BN_bn2hex(ec_sig->s));

//hexDump("Sig ", (const byte*)sig_bytes, sig_size);


//DBG("post i2d:%s\n", BN_bn2hex(ec_sig->s));


//	free(pubkey);
      //  if (ctx) {
       //         BN_CTX_free(ctx);
      // }
//	EC_POINT_free(ec_point); 

/*

        uint8_t buf[72], *ptr;
        int res;

        ptr = add_number_to_sign(buf + 2, sign_r);
        ptr = add_number_to_sign(ptr, sign_s);
        buf[0] = 0x30;
        buf[1] = ptr - buf - 2;
        res = ECDSA_verify(0, (const uint8_t*)hash, sizeof(xdag_hash_t), buf, ptr - buf, (EC_KEY*)key);

        xdag_debug("Verify: res=%2d key=%lx hash=[%s] sign=[%s] r=[%s], s=[%s]", res, (long)key, xdag_log_hash(hash),
                xdag_log_array(buf, ptr - buf), xdag_log_hash(sign_r), xdag_log_hash(sign_s));

        return res != 1;

  fail:
        if (ctx) {
                BN_CTX_free(ctx);
        }

        if (res && eckey) {
                EC_KEY_free(eckey);
        }

        return res ? 0 : eckey;
i
*/
//return 1;
}
