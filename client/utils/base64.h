/* base64, T13.670-T13.788 $DVS:time$ */

#ifndef XDAG_BASE64_H
#define XDAG_BASE64_H


extern int encode_base64(unsigned char *t, const unsigned char *f, int dlen);

extern int decode_base64(unsigned char *t, const unsigned char *f, int n);

#endif