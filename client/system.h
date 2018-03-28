#ifndef _SYSTEM_H
#define _SYSTEM_H

#if defined(_WIN32) || defined(_WIN64)
#include <Windows.h>
#define inline              __inline
#include "../dus/programs/dfstools/source/include/dfsrsa.h"

#define strtok_r            strtok_s
#define localtime_r(a, b)   localtime_s(b, a)
#ifdef _WIN32
#define sleep(x)            Sleep((x) * 1000)
#else
#define sleep(x)            _sleep(x)
#endif
#define pthread_self_ptr()  pthread_self().p

typedef struct {
	dfsrsa_t num[4];
} xdag_diff_t;

#define xdag_diff_max      { -1, -1, -1, -1 }
#define xdag_diff_gt(l, r) (dfsrsa_cmp((l).num, (r).num, 4) > 0)
#define xdag_diff_args(d)  (unsigned long long)(*(uint64_t*)&d.num[2]), (unsigned long long)(*(uint64_t*)&d.num[0])
#define xdag_diff_shr32(p) ((p)->num[0] = (p)->num[1], (p)->num[1] = (p)->num[2], (p)->num[2] = (p)->num[3], (p)->num[3] = 0)
static inline xdag_diff_t xdag_diff_add(xdag_diff_t p, xdag_diff_t q)
{
	xdag_diff_t r;

	dfsrsa_add(r.num, p.num, q.num, 4);
	return r;
}
static inline xdag_diff_t xdag_diff_div(xdag_diff_t p, xdag_diff_t q)
{
	xdag_diff_t r;

	dfsrsa_divmod(p.num, 4, q.num, 4, r.num);
	return r;
}
#define xdag_diff_to64(d)       (*(uint64_t*)&d.num[0])
#define strdup(x)               _strdup(x)
#define ioctl                   ioctlsocket
#define fcntl(a, b, c)          0
#define close                   closesocket
#define write(a, b, c)          send(a, b, c, 0)
#define read(a, b, c)           recv(a, b, c, 0)
#define sysconf(x)              (512)

#define xOPENSSL_ia32_cpuid     OPENSSL_ia32_cpuid
#define xOPENSSL_ia32cap_P      OPENSSL_ia32cap_P
#define xsha256_multi_block     sha256_multi_block

#else

#define pthread_self_ptr()      pthread_self()
typedef unsigned __int128 xdag_diff_t;
#define xdag_diff_max      (-(xdag_diff_t)1)
#define xdag_diff_gt(l, r) ((l) > (r))
#define xdag_diff_shr32(p) (*(p) >>= 32)
#define xdag_diff_args(d)  (unsigned long long)((d) >> 64), (unsigned long long)(d)
#define xdag_diff_add(p, q) ((p) + (q))
#define xdag_diff_div(p, q) ((p) / (q))
#define xdag_diff_to64(d)  ((uint64_t)(d))
#define INVALID_SOCKET          -1

#endif

#endif
