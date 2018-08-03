/* dnet: communication with tap interface; T13.137-T13.138; $DVS:time$ */

#if defined(_WIN32) || defined(_WIN64)

int dnet_tap_open(int tap_number) {
	return -1;
}

#elif defined(__MACOS__) || defined(__APPLE__)

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include "dnet_tap.h"
#include <net/if.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <net/if_utun.h>
#include <sys/kdebug.h>

int dnet_tap_open(int tap_number) {
    return -1;
}

#else

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include "dnet_tap.h"

#ifndef __LDuS__

#include <errno.h>
#include <sys/ioctl.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <linux/kd.h>

int dnet_tap_open(int tap_number) {
	struct ifreq ifr;
	int fd, err;

   /* open the clone device */
	if ((fd = open("/dev/net/tun", O_RDWR)) < 0) {
		return -(errno * 10 + 1);
	}

   /* preparation of the struct ifr, of type "struct ifreq" */
	memset(&ifr, 0, sizeof(ifr));
	ifr.ifr_flags = IFF_TAP;	/* IFF_TUN or IFF_TAP, plus maybe IFF_NO_PI */
	sprintf(ifr.ifr_name, "tap%u", tap_number);

   /* try to create the device */
	if (ioctl(fd, TUNSETIFF, (void *)&ifr) < 0) {
		err = -(errno * 10 + 2);
		close(fd);
		return err;
	}

   /* this is the special file descriptor that the caller will use to talk with the virtual interface */
	return fd;
}

#else

#include <ldus/system/kernel.h>
#include <ldus/system/stdlib.h>
#include <ldus/system/network.h>

#define TAP_BUF_SIZE	0x600
#define TAP_NBUFS		0x2000

//#define RECEIVE_DUMP
//#define TRANSMIT_DUMP

struct tap_data {
	struct ldus_socket *sock;
	uint32_t nrbuf, ntbuf;
	int fd, tapN;
};

static ssize_t tap_read(struct ldus_special_file *f, void *buf, size_t size) {
	struct tap_data *t = (struct tap_data *)f->data;
	struct ldus_sbuffer *tb;
	volatile uint64_t *ptraffic = &t->sock->t.stat->traffic;
	uint8_t *ubuf = (uint8_t *)buf;
	uint32_t ntbuf;
	uint64_t traffic;
	ssize_t res;
begin:
	while ((off_t)((traffic = *ptraffic) - t->sock->t.last_traffic) < TAP_BUF_SIZE) {
		ldus_wait_variable_update(0, ptraffic, traffic);
	}
	ntbuf = t->ntbuf++ & (TAP_NBUFS - 1);
	tb = ldus_get_sbuffer(t->sock, t, ntbuf);
	res = tb->len;
	if (res >= 14 && res + 4 <= size) {
		ubuf[0] = ubuf[1] = 0;
		ubuf[2] = tb->mess[12];
		ubuf[3] = tb->mess[13];
		memcpy(buf + 4, tb->mess, res);
		res += 4;
	} else res = 0;

#ifdef TRANSMIT_DUMP
	{
		int i;
		printf("tap%d %03X out ", t->tapN, res);
		for (i = 0; i < 66; ++i) {
			if (i == 4 || i == 10 || i == 16 || i == 18 || i == 20 || i >= 22 && i % 4 == 2)
				printf(":");
			printf("%02X", ubuf[i]);
		}
		printf("\n");
	}
#endif

	tb->len = 0;
	t->sock->t.last_traffic += TAP_BUF_SIZE;
	if (!res) goto begin;
	return res;
}

static ssize_t tap_write(struct ldus_special_file *f, const void *buf, size_t size) {
	struct tap_data *t = (struct tap_data *)f->data;
	struct ldus_sbuffer *rb;
	volatile uint64_t *prblen;
	uint64_t rblen;
	uint32_t nrbuf;
	if (size < 4 || size > TAP_BUF_SIZE + 4 - offsetof(struct ldus_sbuffer, mess)) return size;
	nrbuf = t->nrbuf++ & (TAP_NBUFS - 1);
	rb = ldus_get_sbuffer(t->sock, r, nrbuf);
	prblen = (volatile uint64_t *)&rb->len;
	while ((uint16_t)(rblen = *prblen)) {
		ldus_wait_variable_update(0, prblen, rblen);
	}
	memcpy(rb->mess, buf + 4, size - 4);

#ifdef RECEIVE_DUMP
	{
		int i;
		uint8_t *ubuf = (uint8_t *)buf;
		printf("tap%d %03X in  ", t->tapN, size);
		for (i = 0; i < 66; ++i) {
			if (i == 4 || i == 10 || i == 16 || i == 18 || i == 20 || i >= 22 && i % 4 == 2)
				printf(":");
			printf("%02X", ubuf[i]);
		}
		printf("\n");
	}
#endif

	*prblen = size;
	t->sock->r.stat->traffic += TAP_BUF_SIZE;
	return size;
}

static int tap_close(struct ldus_special_file *f) {
	struct tap_data *t = (struct tap_data *)f->data;
	close(t->fd);
	free(t);
	free(f);
	return 0;
}

static const struct ldus_special_file_func tap_func = {
	.read	= tap_read,
	.write	= tap_write,
	.close	= tap_close,
};

int dnet_tap_open(int tap_number) {
	struct ldus_sockaddr_ether se;
	struct ldus_socket *sock;
	struct ldus_special_file *f = 0;
	struct tap_data *t = 0;
	int fd = -1, err;

   /* создаём интерфейс tapN */
	fd = socket(PF_ETHER, SOCK_RAW, 0);
	if (fd < 0) { err = 1; goto fail; }
	sock = ldus_get_socket(fd);
	if (!sock) { err = 2; goto fail; }
	err = ldus_set_socket_create_options(fd, LDUS_SOCKET_CREATE, 0640, 0660, TAP_BUF_SIZE, TAP_BUF_SIZE, TAP_NBUFS, TAP_NBUFS);
	if (err) { err = 3; goto fail; }
	se.se_family = AF_ETHER;
	sprintf(se.se_name, "tap%u", tap_number);
	se.se_mac_addr.b[0] = 13;
	se.se_mac_addr.b[1] = 137;
	se.se_mac_addr.b[2] = tap_number >> 24 & 0xff;
	se.se_mac_addr.b[3] = tap_number >> 16 & 0xff;
	se.se_mac_addr.b[4] = tap_number >>  8 & 0xff;
	se.se_mac_addr.b[5] = tap_number       & 0xff;
	err = bind(fd, (struct sockaddr *)&se, sizeof(se));
	if (err) { err = 4; goto fail; }

   /* создаём специальный файл, связанный с этим интерфейсом */
	f = calloc(1, sizeof(*f));
	t = calloc(1, sizeof(*t));
	if (!f || !t) { err = 5; goto fail; }
	t->sock = sock;
	t->fd = fd;
	t->tapN = tap_number;
	f->func = &tap_func;
	f->data = t;
	fd = ldus_open_special_file(f, O_RDWR);
	if (fd < 0) { err = 6; goto fail; }

	return fd;

fail:
	if (t) {
		close(t->fd);
		free(t);
	}
	if (f) {
		free(f);
	}
	if (fd >= 0) close(fd);
	ldus_errno = ldus_errno << 4 | err;
	return -1;
}

#endif

#endif
