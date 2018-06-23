/*********************************************************************
* Filename:   base64.c
* Author:     peter Liu
* Copyright:
* Disclaimer: This code is presented "as is" without any guarantees.
* Details:

*********************************************************************/



/*************************** HEADER FILES ***************************/


#include "base64.h"

/****************************** MACROS ******************************/


#define B64_WS              0xE0
#define B64_ERROR           0xFF
#define B64_NOT_BASE64(a)   (((a)|0x13) == 0xF3)

#define conv_bin2ascii(a)   (data_bin2ascii[(a)&0x3f])
#define conv_ascii2bin(a)   (data_ascii2bin[(a)&0x7f])

static const unsigned char data_bin2ascii[65] =
"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static const unsigned char data_ascii2bin[128] =
{ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xE0, 0xF0, 0xFF, 0xFF,
        0xF1, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xE0, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x3E, 0xFF, 0xF2, 0xFF, 0x3F, 0x34,
        0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0xFF, 0xFF, 0xFF,
        0x00, 0xFF, 0xFF, 0xFF, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13,
        0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25,
        0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31,
        0x32, 0x33, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, };


/* Base64 Encode */
int encode_base64(unsigned char *t, const unsigned char *f, int dlen)
{
    int i, ret = 0;
    unsigned long l;

    for (i = dlen; i > 0; i -= 3)
    {
        if (i >= 3)
        {
            l = (((unsigned long) f[0]) << 16L) | (((unsigned long) f[1]) << 8L) | f[2];
            *(t++) = conv_bin2ascii(l>>18L);
            *(t++) = conv_bin2ascii(l>>12L);
            *(t++) = conv_bin2ascii(l>> 6L);
            *(t++) = conv_bin2ascii(l );
        }
        else
        {
            l = ((unsigned long) f[0]) << 16L;
            if (i == 2)
                l |= ((unsigned long) f[1] << 8L);

            *(t++) = conv_bin2ascii(l>>18L);
            *(t++) = conv_bin2ascii(l>>12L);
            *(t++) = (i == 1) ? '=' : conv_bin2ascii(l>> 6L);
            *(t++) = '=';
        }
        ret += 4;
        f += 3;
    }

    *t = '\0';
    return (ret);
}

/* Base64 Decode */
int decode_base64(unsigned char *t, const unsigned char *f, int n)
{
    int i, ret = 0, a, b, c, d;
    unsigned long l;

    /* trim white space from the start of the line. */
    while ((conv_ascii2bin(*f) == B64_WS) && (n > 0))
    {
        f++;
        n--;
    }

    /* strip off stuff at the end of the line
     * ascii2bin values B64_WS, B64_EOLN, B64_EOLN and B64_EOF */
    while ((n > 3) && (B64_NOT_BASE64(conv_ascii2bin(f[n-1]))))
        n--;

    if (n % 4 != 0)
        return (-1);

    for (i = 0; i < n; i += 4)
    {
        a = conv_ascii2bin(*(f++));
        b = conv_ascii2bin(*(f++));
        c = conv_ascii2bin(*(f++));
        d = conv_ascii2bin(*(f++));
        if ((a & 0x80) || (b & 0x80) || (c & 0x80) || (d & 0x80))
            return (-1);
        l = ((((unsigned long) a) << 18L) | (((unsigned long) b) << 12L)
                | (((unsigned long) c) << 6L) | (((unsigned long) d)));
        *(t++) = (unsigned char) (l >> 16L) & 0xff;
        *(t++) = (unsigned char) (l >> 8L) & 0xff;
        *(t++) = (unsigned char) (l) & 0xff;
        ret += 3;
    }
    return (ret);
}